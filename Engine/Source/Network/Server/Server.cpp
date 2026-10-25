#include "Pch.h"

#if defined(BT_SERVER)

#include "Network/Server/Server.h"

#include "File/PackChunks.h"
#include "Network/Server/ServerSessionRuntime.h"
#include "Network/NetworkCursor.h"
#include "CrashReport.h"

#include "Network/Server/ServerClientManager.h"
#include "Game.h"

namespace engine
{

// NetworkProtocol.h cannot include NetworkManager.h; tie the codec's maximum ack message to the real
// slot ceiling here so a transport slot change trips this assertion.
static_assert(NetworkMessages::ClientAckStreamMessage::kiMaxSlotCount == NetworkManager::kiMaximumEnetCoordinateSlots, "ack-stream codec cardinality must match the transport slot ceiling");
static_assert(kiMaximumAcknowledgmentStreamPacketSize == NetworkMessages::ClientAckStreamMessage::GetSize(NetworkManager::kiMaximumEnetCoordinateSlots), "kiMaxAckStreamPacketSize must match the ack-stream wire layout sized by kiMaxEnetCoordSlots");
static_assert(kiCoordinateSlots <= NetworkManager::kiMaximumEnetCoordinateSlots, "kiCoordinateSlots must fit the transport slot ceiling");

// Both the unscheduled poll gap that opens the budget stall grace and the grace length, each on top of the
// scheduled update interval. Every stall long enough to queue a budget-crossing ack backlog (~4.6 s at 1:1)
// exceeds it, and ordinary running never reaches it.
constexpr std::chrono::seconds kBudgetStallGrace = 1s;

constexpr std::chrono::seconds kDesynchronizationDiagnosticCooldown = 2s;

// Per-client chat token bucket: a burst of five messages, then one per second of steady_clock time.
constexpr float kfChatTokenBurst = 5.0f;
constexpr float kfChatTokensPerSecond = 1.0f;

Server::Server(int64_t iPort)
: common::Singleton<Server>(gpServer, this)
{
	ENetAddress address {};
	address.host = (gLaunchOptions.flags & LaunchOptionFlags::kLoopbackOnly) ? htonl(INADDR_LOOPBACK) : ENET_HOST_ANY;
	address.port = static_cast<uint16_t>(iPort);

	ScopedSuppressAllocationTracking suppress;
	// Heap: ENet allocates host data internally
	mpHost = enet_host_create(&address, 64, static_cast<size_t>(NetworkManager::kiChannelCount), 0, 0);
	if (mpHost == nullptr)
	{
		std::string message = std::format("Server::Server enet_host_create failed to bind UDP port {}", iPort);
		LOG(kNetwork, kError, "{}", message);
		throw std::runtime_error(message);
	}
	mpHost->checksum = enet_crc32;
	// 1MB send/receive buffers to handle bursty packet dispatches
	enet_socket_set_option(mpHost->socket, ENET_SOCKOPT_SNDBUF, 1'024 * 1'024);
	enet_socket_set_option(mpHost->socket, ENET_SOCKOPT_RCVBUF, 1'024 * 1'024);
}

Server::~Server()
{
	// Heap: ENet destroys host data internally
	ScopedSuppressAllocationTracking suppress;
	enet_host_destroy(mpHost);
}

void Server::AdvanceLoadGeneration()
{
	ASSERT(miLoadGeneration < UINT8_MAX);
	++miLoadGeneration;
}

void Server::Flush()
{
	enet_host_flush(mpHost);
}

void Server::Poll(const NetworkTimeState& rTimeState, ServerPollMode ePollMode)
{
	ASSERT(common::gpMultithreading->IsMainThread());

	// mPendingNewSubscriptions / mPendingResynchronizationClientIds are intentionally NOT cleared here. Their consumers
	// (ServerSessionRuntime::SendNewSubscriptionFullStates / ServerSessionRuntime::HandleResyncRequests) run post-tick,
	// so a per-poll clear would drop a subscribe/resync accepted between servicings. They persist until the runtime
	// consumers service and clear them. While paused or otherwise zero-tick (iFullTicks == 0),
	// ServerSessionRuntime::CompleteUpdate services them each update instead, so a client can join before a tick advances.
	mDeferredClientPackets.clear();

	// Reset the per-update (~ per-tick window) contract budgets before draining this update's packets. The
	// tick-boundary poll skips the reset so both polls of an update share one admission budget window.
	if (ePollMode == ServerPollMode::kUpdateStart)
	{
		for (ClientConnection& rClient : mClients)
		{
			rClient.iTickPacketCount = 0;
			rClient.iTickByteCount = 0;
			std::memset(rClient.puiTickTypeCounts, 0, sizeof(rClient.puiTickTypeCounts));
		}
	}

	// Budget stall grace: the acks a legitimate client queued while the server did not poll all drain into
	// one budget window, so after an unscheduled stall gate 2 drops over-budget packets without recording a
	// violation until the deadline. The scheduled interval is added to the threshold because the gap before
	// the tick-boundary poll contains WaitForTick's scheduled sleep, which slow motion lengthens, and to the
	// length because a burst drained here can first cross the budget at the tick-boundary poll one interval later.
	// The gap runs from the previous poll's end, so draining a flood cannot open a window.
	std::chrono::steady_clock::time_point pollStart = std::chrono::steady_clock::now();
	std::chrono::microseconds scheduledInterval(rTimeState.iExpectedUpdateIntervalMicroseconds);
	if (pollStart - mPreviousPollEnd > kBudgetStallGrace + scheduledInterval)
	{
		mBudgetGraceDeadline = pollStart + kBudgetStallGrace + scheduledInterval;
	}

	ENetEvent event {};
	while (enet_host_service(mpHost, &event, 0) > 0)
	{
		switch (event.type)
		{
			case ENET_EVENT_TYPE_CONNECT:
				Connect(event);
				break;
			case ENET_EVENT_TYPE_DISCONNECT:
				Disconnect(event);
				break;
			case ENET_EVENT_TYPE_RECEIVE:
				DispatchIncoming(event, rTimeState.bFastForward);
				break;
			case ENET_EVENT_TYPE_NONE:
				break;
		}
	}

	// Process delayed packets whose release time has passed (or flush all when bypassing simulation)
	if constexpr (keNetworkSimulation != engine::NetworkSimulationLevel::kDisabled)
	{
		NetworkSimulation::ProcessOrFlush(mDelayedPackets, rTimeState.bFastForward, [this](const DelayedPacket& rPacket)
		{
			Receive(rPacket.data, rPacket.pPeer);
		});
	}

	mPreviousPollEnd = std::chrono::steady_clock::now();
}

void Server::DispatchIncoming(ENetEvent& rEvent, bool bFastForward)
{
	if constexpr (keNetworkSimulation != engine::NetworkSimulationLevel::kDisabled)
	{
		static constexpr NetworkSimulationConfig kSimulationConfiguration = GetNetworkSimulationConfiguration(keNetworkSimulation);
		NetworkSimulation::DispatchOrEnqueue(mDelayedPackets, mNetworkSimulationState, kSimulationConfiguration, bFastForward, rEvent, [this](ENetEvent& rInner)
		{
			Receive(std::span<const uint8_t>(rInner.packet->data, rInner.packet->dataLength), rInner.peer);
		});
	}
	else
	{
		Receive(std::span<const uint8_t>(rEvent.packet->data, rEvent.packet->dataLength), rEvent.peer);
		enet_packet_destroy(rEvent.packet);
	}
}

void Server::Connect(const ENetEvent& rEvent)
{
	ScopedSuppressAllocationTracking suppress;

	ClientConnection connection {};
	connection.pPeer = rEvent.peer;
	connection.iClientId = miNextClientId++;
	connection.slots.resize(NetworkManager::kiMaximumEnetCoordinateSlots);

	rEvent.peer->data = reinterpret_cast<void*>(connection.iClientId);

	// Heap: client vector grows on connect
	mClients.push_back(std::move(connection));

	// Disable ENet peer throttle to prevent unreliable packet drops during client reconciliation stalls
	enet_peer_throttle_configure(rEvent.peer, UINT32_MAX, 0, 0);

	LOG(kNetwork, kInfo, "Server::Connect Client: {}", mClients.back().iClientId);
}

void Server::Disconnect(const ENetEvent& rEvent)
{
	int64_t iClientId = reinterpret_cast<int64_t>(rEvent.peer->data);

	RemoveClient(iClientId);

	if constexpr (keNetworkSimulation != engine::NetworkSimulationLevel::kDisabled)
	{
		std::erase_if(mDelayedPackets, [&rEvent](const DelayedPacket& rPacket)
		{
			return rPacket.pPeer == rEvent.peer;
		});
	}

	LOG(kNetwork, kInfo, "Server::Disconnect Client: {}", iClientId);
}

void Server::Receive(std::span<const uint8_t> packetData, ENetPeer* pPeer)
{
	if (packetData.size() < 1)
	{
		return;
	}

	int64_t iClientId = reinterpret_cast<int64_t>(pPeer->data);
	PacketType eType = static_cast<PacketType>(packetData[0]);

	// Gate 1: unknown client id, or a peer no longer connected whose record awaits RemoveDisconnectedClients -> silent
	// drop. ENet receives only from connected peers, so only network-simulation delayed packets are dropped here.
	ClientConnection* pClient = FindClient(iClientId);
	if (pClient == nullptr)
	{
		return;
	}

	if (pClient->pPeer->state != ENET_PEER_STATE_CONNECTED)
	{
		return;
	}

	// Both polls share one packet/byte budget for all types. Only the first crossing of each budget records
	// a rate violation; subsequent crossings drop silently. Sustained floods can add two violations per
	// update and disconnect in about eight updates (0.25 s at 32 Hz). Poll's unscheduled-stall grace drops
	// crossings without charging ACK backlogs. Handshaken subscribe/unsubscribe requests bypass this budget
	// because they are reliable with no retry, and a handshaken crash report because one report exceeds the
	// byte budget; gates 3–5 still enforce size and per-type caps. Pre-handshake requests remain budgeted
	// because gate 4 drops them before gate 5.
	bool bBudgetExempt = pClient->bHandshakeComplete && (eType == PacketType::kClientSubscribe || eType == PacketType::kClientUnsubscribe || eType == PacketType::kClientCrashReport);
	if (!bBudgetExempt)
	{
		bool bPacketWasUnderBudget = pClient->iTickPacketCount <= kiMaximumClientPacketsPerTick;
		++pClient->iTickPacketCount;
		if (pClient->iTickPacketCount > kiMaximumClientPacketsPerTick)
		{
			if (bPacketWasUnderBudget && std::chrono::steady_clock::now() >= mBudgetGraceDeadline)
			{
				RecordContractViolation(*pClient, ContractViolationKind::kRate, "tick budget", packetData[0], static_cast<int64_t>(packetData.size()));
			}
			return;
		}

		bool bByteWasUnderBudget = pClient->iTickByteCount <= kiMaximumClientInboundBytesPerTick;
		pClient->iTickByteCount += static_cast<int64_t>(packetData.size());
		if (pClient->iTickByteCount > kiMaximumClientInboundBytesPerTick)
		{
			if (bByteWasUnderBudget && std::chrono::steady_clock::now() >= mBudgetGraceDeadline)
			{
				RecordContractViolation(*pClient, ContractViolationKind::kRate, "tick budget", packetData[0], static_cast<int64_t>(packetData.size()));
			}
			return;
		}
	}

	// Gates 3–5 apply only to engine types. Game-range types use the default branch's handshake gate;
	// their contract checks run at admission in the deferred drain.
	if (static_cast<uint8_t>(eType) < static_cast<uint8_t>(PacketType::kGamePacketStart))
	{
		ClientPacketContract contract = GetClientPacketContract(eType);

		// Gate 3: contract lookup -- sentinel row (not client-sendable)
		// or size outside [min, max].
		if (contract.iMaximumSize == 0)
		{
			RecordContractViolation(*pClient, ContractViolationKind::kCorrupt, "not client-sendable", packetData[0], static_cast<int64_t>(packetData.size()));
			return;
		}
		if (static_cast<int64_t>(packetData.size()) < contract.iMinimumSize || static_cast<int64_t>(packetData.size()) > contract.iMaximumSize)
		{
			RecordContractViolation(*pClient, ContractViolationKind::kCorrupt, "size out of range", packetData[0], static_cast<int64_t>(packetData.size()));
			return;
		}

		// Gate 4: handshake gate -- silent drop, no violation (pre-Hello ack race: ack streams legitimately
		// arrive before the handshake completes).
		if (contract.bRequiresHandshake && !pClient->bHandshakeComplete)
		{
			LOG(kNetwork, kDebug, "Server::Receive pre-handshake drop Client: {} Type: {}", iClientId, packetData[0]);
			return;
		}

		// Gate 5: per-type per-tick cap -- drop; violation only if the contract counts over-cap.
		if (++pClient->puiTickTypeCounts[packetData[0]] > contract.iMaximumPerTick)
		{
			if (contract.bOverCapCountsViolation)
			{
				RecordContractViolation(*pClient, ContractViolationKind::kRate, "per-type cap", packetData[0], static_cast<int64_t>(packetData.size()));
			}
			return;
		}
	}

	try
	{
		switch (eType)
		{
			case PacketType::kClientAcknowledgmentStream:
				ClientAcknowledgementStream(packetData, *pClient);
				break;
			case PacketType::kClientDesynchronizationReport:
				ClientDesynchronizationReport(packetData, *pClient);
				break;
			case PacketType::kClientDebugFrameRequest:
				ClientDebugFrameRequest(packetData, *pClient);
				break;
			case PacketType::kClientHello:
				ClientHello(packetData, *pClient);
				break;
			case PacketType::kClientSubscribe:
				ClientSubscribe(packetData, *pClient);
				break;
			case PacketType::kClientUnsubscribe:
				ClientUnsubscribe(packetData, *pClient);
				break;
			case PacketType::kClientResynchronizationRequest:
				ClientResynchronizationRequest(packetData, *pClient);
				break;
			case PacketType::kClientCrashReport:
				ClientCrashReport(packetData, *pClient);
				break;
			case PacketType::kClientChat:
				ClientChat(packetData, *pClient);
				break;
			case PacketType::kClientPauseRequest:
			{
				NetworkMessages::ClientPauseRequestMessage message {};
				NetworkMessages::Read(packetData, message);
				if (message.uiPaused > 1)
				{
					NetworkMessages::ThrowCorruptStream("Server::ClientPauseRequest");
				}
				DeferClientPacket(packetData, iClientId);
				break;
			}
			case PacketType::kClientTimespeedRequest:
			{
				NetworkMessages::ClientTimespeedRequestMessage message {};
				NetworkMessages::Read(packetData, message);
				if (message.uiFaster > 1)
				{
					NetworkMessages::ThrowCorruptStream("Server::ClientTimespeedRequest");
				}
				DeferClientPacket(packetData, iClientId);
				break;
			}
			case PacketType::kClientSaveRequest:
			case PacketType::kClientLoadRequest:
			case PacketType::kClientResetRequest:
			case PacketType::kClientReplayRecordRequest:
			case PacketType::kClientReplayPlaybackRequest:
				DeferClientPacket(packetData, iClientId);
				break;
			default:
				// Only game-range types reach the default -- engine sentinel types are caught at gate 3.
				// Game-range handshake gate stays here (silent drop pre-handshake).
				if (pClient->bHandshakeComplete)
				{
					DeferClientPacket(packetData, iClientId);
				}
				break;
		}
	}
	catch (const std::exception& rException)
	{
		// Packet readers throw std::ios_base::failure for corrupt input; this catch also handles
		// standard-library failures. Readers record no violations. Drop the packet and charge one
		// corrupt-data violation; the peer disconnects at the corrupt-data limit.
		LOG(kNetwork, kDebug, "Server::Receive dropped corrupt packet (type {}) Client: {}: {}", static_cast<uint8_t>(eType), iClientId, rException.what());
		RecordContractViolation(*pClient, ContractViolationKind::kCorrupt, "corrupt payload", packetData[0], static_cast<int64_t>(packetData.size()));
	}
}

void Server::DeferClientPacket(std::span<const uint8_t> packetData, int64_t iClientId)
{
	// Heap: deferred client packet buffer grows on game and debug-control packets
	ScopedSuppressAllocationTracking suppress;
	mDeferredClientPackets.push_back({.iClientId = iClientId, .iPacketType = packetData[0], .payload = std::vector<uint8_t>(packetData.begin() + 1, packetData.end())});
}

void Server::RemoveClient(int64_t iClientId)
{
	for (int64_t i = 0; i < std::ssize(mClients); ++i)
	{
		if (mClients.at(i).iClientId == iClientId)
		{
			game::gpServerSession->mpClientManager->OnClientDisconnected(iClientId, mClients.at(i).clientGuid);
			std::erase_if(mDeferredClientPackets, [iClientId](const DeferredClientPacket& rPacket)
			{
				return rPacket.iClientId == iClientId;
			});
			if (i != std::ssize(mClients) - 1)
			{
				mClients.at(i) = std::move(mClients.back());
			}
			mClients.pop_back();
			return;
		}
	}
}

ClientConnection* Server::FindClient(int64_t iClientId)
{
	for (ClientConnection& rClient : mClients)
	{
		if (rClient.iClientId == iClientId)
		{
			return &rClient;
		}
	}
	return nullptr;
}

const ClientConnection* Server::FindClient(int64_t iClientId) const
{
	for (const ClientConnection& rClient : mClients)
	{
		if (rClient.iClientId == iClientId)
		{
			return &rClient;
		}
	}
	return nullptr;
}

void Server::RecordContractViolation(ClientConnection& rClient, ContractViolationKind eKind, std::string_view reason, int64_t iPacketType, int64_t iSize)
{
	ASSERT(rClient.pPeer->state == ENET_PEER_STATE_CONNECTED);

	bool bCorrupt = eKind == ContractViolationKind::kCorrupt;
	if (!bCorrupt)
	{
		// Lazy decay: forgive one rate violation per whole interval elapsed since the decay start. A count the
		// elapsed intervals cover (including the first strike, from the epoch default) restarts the decay now.
		std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		int64_t iDecayed = (now - rClient.rateViolationDecayStart) / kRateViolationDecayInterval;
		if (iDecayed >= rClient.iRateViolations)
		{
			rClient.iRateViolations = 0;
			rClient.rateViolationDecayStart = now;
		}
		else
		{
			rClient.iRateViolations -= iDecayed;
			rClient.rateViolationDecayStart += iDecayed * kRateViolationDecayInterval;
		}
	}

	int64_t& riViolations = bCorrupt ? rClient.iCorruptViolations : rClient.iRateViolations;
	int64_t iDisconnectCount = bCorrupt ? kiCorruptViolationDisconnectCount : kiRateViolationDisconnectCount;
	const char* pcKind = bCorrupt ? "corrupt" : "rate";
	++riViolations;

	// Log only when a kind's count reaches 1 and at disconnect -- at most one First line per kind (per rate decay
	// back to zero) plus one Disconnecting line per hostile client, no per-packet spam, no cooldown state.
	if (riViolations == 1)
	{
		LOG(kNetwork, kWarning, "Server::RecordContractViolation First Client: {} Kind: {} Reason: {} Type: {} Size: {}", rClient.iClientId, pcKind, reason, iPacketType, iSize);
	}

	if (riViolations >= iDisconnectCount)
	{
		LOG(kNetwork, kWarning, "Server::RecordContractViolation Disconnecting Client: {} Kind: {} Violations: {} Reason: {} Type: {} Size: {}", rClient.iClientId, pcKind, riViolations, reason, iPacketType, iSize);

		enet_peer_disconnect(rClient.pPeer, 0);
	}
}

void Server::RemoveDisconnectedClients()
{
	// Back to front: RemoveClient moves the last record into the removed slot, and that record was already visited.
	for (int64_t i = std::ssize(mClients) - 1; i >= 0; --i)
	{
		if (mClients.at(i).pPeer->state != ENET_PEER_STATE_CONNECTED)
		{
			RemoveClient(mClients.at(i).iClientId);
		}
	}
}

bool Server::AdmitGamePacket(ClientConnection& rClient, const DeferredClientPacket& rPacket, const ClientPacketContract& rContract)
{
	int64_t iFullSize = std::ssize(rPacket.payload) + 1; // + type byte (already stripped from payload)
	if (rContract.iMaximumSize == 0)
	{
		// Sentinel: not client-sendable (server->client or unknown).
		RecordContractViolation(rClient, ContractViolationKind::kCorrupt, "game type not client-sendable", rPacket.iPacketType, iFullSize);
		return false;
	}
	if (iFullSize < rContract.iMinimumSize || iFullSize > rContract.iMaximumSize)
	{
		RecordContractViolation(rClient, ContractViolationKind::kCorrupt, "game packet size out of range", rPacket.iPacketType, iFullSize);
		return false;
	}
	// Per-type per-tick cap. puiTickTypeCounts is reset once per update by the engine (both of the update's polls
	// share the window); engine and game types occupy disjoint type-byte ranges, so sharing one array across
	// both dispatch points is coherent within that window.
	if (++rClient.puiTickTypeCounts[rPacket.iPacketType] > rContract.iMaximumPerTick)
	{
		if (rContract.bOverCapCountsViolation)
		{
			RecordContractViolation(rClient, ContractViolationKind::kRate, "game packet per-tick cap exceeded", rPacket.iPacketType, iFullSize);
		}
		return false; // drop
	}

	return true;
}

void Server::ClientAcknowledgementStream(std::span<const uint8_t> packetData, ClientConnection& rClient)
{
	NetworkMessages::AckStreamEntry pEntries[NetworkManager::kiMaximumEnetCoordinateSlots] {};
	NetworkMessages::ClientAckStreamMessage message
	{
		.pEntries = pEntries,
		.iEntryCapacity = NetworkManager::kiMaximumEnetCoordinateSlots,
	};
	NetworkMessages::Read(packetData, message);

	// A layout failure already threw above and is recorded by the dispatch catch; this cross-check only
	// catches a readable packet whose declared slot count disagrees with its byte length, so no packet is
	// counted twice.
	if (static_cast<int64_t>(packetData.size()) != NetworkMessages::ClientAckStreamMessage::GetSize(message.uiSlotCount))
	{
		RecordContractViolation(rClient, ContractViolationKind::kCorrupt, "ackstream size", packetData[0], static_cast<int64_t>(packetData.size()));
		return;
	}

	mBufferedFrames.ApplyAckStream(rClient, message, rClient.iClientId);
}

void Server::ClientDesynchronizationReport(std::span<const uint8_t> packetData, ClientConnection& rClient)
{
	NetworkMessages::ClientDesyncReportMessage message {};
	NetworkMessages::Read(packetData, message);
	// Before the cooldown, so a corrupt report does not consume it.
	if (message.iTick < 0)
	{
		NetworkMessages::ThrowCorruptStream("Server::ClientDesyncReport");
	}

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	if (now < rClient.desynchronizationReportDeadline)
	{
		return;
	}
	rClient.desynchronizationReportDeadline = now + kDesynchronizationDiagnosticCooldown;

	int64_t iTick = message.iTick;
	GridCoord coordinate = message.coord;
	uint64_t uiExpectedCrc = message.uiExpectedCrc;
	uint64_t uiActualCrc = message.uiActualCrc;

	char pcExpected[20] {};
	char pcActual[20] {};
	LOG(kNetwork, kError, "Server::ClientDesyncReport Frame: {} Grid: ({},{}) Expected: {} Actual: {}", iTick, coordinate.iQ, coordinate.iR, common::ToHex(std::span(pcExpected), uiExpectedCrc), common::ToHex(std::span(pcActual), uiActualCrc));
}

void Server::ClientDebugFrameRequest(std::span<const uint8_t> packetData, ClientConnection& rClient)
{
	NetworkMessages::ClientDebugFrameRequestMessage message {};
	NetworkMessages::Read(packetData, message);
	// Before the cooldown, so a corrupt request does not consume it.
	if (message.iTick < 0)
	{
		NetworkMessages::ThrowCorruptStream("Server::ClientDebugFrameRequest");
	}

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	if (now < rClient.debugFrameRequestDeadline)
	{
		return;
	}
	rClient.debugFrameRequestDeadline = now + kDesynchronizationDiagnosticCooldown;

	mBufferedFrames.SendDebugFrame(rClient.pPeer, message.iTick, message.coord);
}

void Server::ClientChat(std::span<const uint8_t> packetData, ClientConnection& rClient)
{
	NetworkMessages::ClientChatMessage message {};
	NetworkMessages::Read(packetData, message);
	// Before the token bucket, so corrupt text does not spend a token. Bytes after an embedded NUL are never relayed,
	// because only the validated text is re-encoded below.
	if (!IsValidChatText(message.text))
	{
		NetworkMessages::ThrowCorruptStream("Server::ClientChat");
	}

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	float fElapsedSeconds = std::chrono::duration<float>(now - rClient.chatTokenRefillTime).count();
	rClient.fChatTokens = std::min(rClient.fChatTokens + fElapsedSeconds * kfChatTokensPerSecond, kfChatTokenBurst);
	rClient.chatTokenRefillTime = now;
	// A rate limit is not corruption: the message drops with no violation.
	if (rClient.fChatTokens < 1.0f)
	{
		return;
	}
	rClient.fChatTokens -= 1.0f;

	// The text view points into packetData, which stays valid for the whole Receive call.
	for (const ClientConnection& rRecipient : mClients)
	{
		if (!rRecipient.bHandshakeComplete)
		{
			continue;
		}

		common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
		common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
		NetworkMessages::ServerChatMessage relay {.iSenderId = rClient.iClientId, .text = message.text};
		NetworkMessages::Write(rWorkbuffer, relay);
		NetworkManager::SendPacket(rRecipient.pPeer, NetworkManager::kiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
	}
}

void Server::ClientHello(std::span<const uint8_t> packetData, ClientConnection& rClient)
{
	NetworkMessages::ClientHelloMessage message {};
	NetworkMessages::Read(packetData, message);

	int64_t iClientProtocolVersion = message.uiProtocolVersion;
	if (iClientProtocolVersion != kiProtocolVersion)
	{
		char pcMessage[256] {};
		std::snprintf(pcMessage, sizeof(pcMessage), "Protocol version mismatch: server is %u, client is %u", static_cast<unsigned int>(kiProtocolVersion), static_cast<uint32_t>(iClientProtocolVersion));
		LOG(kNetwork, kWarning, "Server::ClientHello Rejecting Client: {} Reason: {}", rClient.iClientId, pcMessage);

		RejectHello(rClient, pcMessage);
		return;
	}

	int64_t iClientFrameVersion = message.iFrameVersion;
	if (iClientFrameVersion != game::Frame::kiVersion)
	{
		char pcMessage[256] {};
		std::snprintf(pcMessage, sizeof(pcMessage), "Frame version mismatch: server is %lld, client is %lld", game::Frame::kiVersion, iClientFrameVersion);
		LOG(kNetwork, kWarning, "Server::ClientHello Rejecting Client: {} Reason: {}", rClient.iClientId, pcMessage);

		RejectHello(rClient, pcMessage);
		return;
	}

	common::crc_t uiClientPackIntegrityToken = message.uiPackIntegrityToken;
	common::crc_t uiServerPackIntegrityToken = gpFileManager->mpPackChunks->mPackIntegrityToken;
	if (uiClientPackIntegrityToken != uiServerPackIntegrityToken)
	{
		char pcMessage[256] {};
		std::snprintf(pcMessage, sizeof(pcMessage), "Pack integrity mismatch: server token is %llu, client token is %llu. Regenerate generated game data and retry.", static_cast<unsigned long long>(uiServerPackIntegrityToken), static_cast<unsigned long long>(uiClientPackIntegrityToken));
		LOG(kNetwork, kWarning, "Server::ClientHello Rejecting Client: {} Reason: {}", rClient.iClientId, pcMessage);

		RejectHello(rClient, pcMessage);
		return;
	}

	char pcClientConfiguration[64] = {};
	int64_t iCopyLength = std::min(std::ssize(message.buildConfiguration), std::ssize(pcClientConfiguration) - 1);
	std::memcpy(pcClientConfiguration, message.buildConfiguration.data(), static_cast<size_t>(iCopyLength));

	if (std::strcmp(pcClientConfiguration, kpcBuildConfigurationName) != 0)
	{
		LOG(kNetwork, kWarning, "Server::ClientHello Client {} build config mismatch: server is {}, client is {}", rClient.iClientId, kpcBuildConfigurationName, pcClientConfiguration);
	}

	ClientGuid clientGuid = message.bHasGuid ? message.guid : ClientGuid {};

	// Idempotent replay: an already-handshaken client re-sends the accept using its STORED GUID.
	// Do not overwrite established identity (game entity ownership is GUID-keyed) or mint a fresh GUID.
	if (rClient.bHandshakeComplete)
	{
		LOG(kNetwork, kInfo, "Server::ClientHello Replay Client: {} GUID: {} {}", rClient.iClientId, rClient.clientGuid.uiHigh, rClient.clientGuid.uiLow);
		SendConnectionResponse(rClient.pPeer, true, nullptr, &rClient.clientGuid);
		SendTimespeedUpdate(rClient.pPeer, game::gpGame->mTimeStep.miTimeMultiply, game::gpGame->mTimeStep.miTimeDivide);
		return;
	}

	// One live handshaken peer per GUID: game entity ownership is GUID-keyed, so a second peer must not take over the first's identity.
	// This peer's own record is not yet handshaken (the replay branch returned otherwise), so it cannot match itself.
	if ((clientGuid.uiHigh != 0 || clientGuid.uiLow != 0))
	{
		auto it = std::ranges::find_if(mClients, [&clientGuid](const ClientConnection& rOther)
		{
			return rOther.bHandshakeComplete && rOther.clientGuid == clientGuid;
		});
		if (it != mClients.end())
		{
			const char* pcMessage = "Duplicate client GUID: another connected client already uses this identity";
			LOG(kNetwork, kWarning, "Server::ClientHello Rejecting Client: {} Reason: {} Existing Client: {}", rClient.iClientId, pcMessage, it->iClientId);

			RejectHello(rClient, pcMessage);
			return;
		}
	}

	if ((clientGuid.uiHigh == 0 && clientGuid.uiLow == 0))
	{
		::UUID uuid;
		[[maybe_unused]] int64_t iRpcStatus = UuidCreate(&uuid); // RPC_S_UUID_LOCAL_ONLY still yields a usable UUID
		std::memcpy(&clientGuid.uiHigh, &uuid, 8);
		std::memcpy(&clientGuid.uiLow, reinterpret_cast<const uint8_t*>(&uuid) + 8, 8);
	}

	rClient.bHandshakeComplete = true;
	rClient.clientGuid = clientGuid;

	LOG(kNetwork, kInfo, "Server::ClientHello Accepted Client: {} Config: {} GUID: {} {}", rClient.iClientId, pcClientConfiguration, clientGuid.uiHigh, clientGuid.uiLow);
	SendConnectionResponse(rClient.pPeer, true, nullptr, &clientGuid);

	SendTimespeedUpdate(rClient.pPeer, game::gpGame->mTimeStep.miTimeMultiply, game::gpGame->mTimeStep.miTimeDivide);
}

void Server::RejectHello(const ClientConnection& rClient, const char* pcMessage)
{
	SendConnectionResponse(rClient.pPeer, false, pcMessage, nullptr);

	enet_peer_disconnect_later(rClient.pPeer, 0);
	RemoveClient(rClient.iClientId);
}

void Server::ClientSubscribe(std::span<const uint8_t> packetData, ClientConnection& rClient)
{
	NetworkMessages::ClientSubscribeMessage message {};
	NetworkMessages::Read(packetData, message);

	GridCoord coordinate = message.coord;

	if (message.uiLoadGeneration != miLoadGeneration)
	{
		LOG(kNetwork, kWarning, "Server::ClientSubscribe LoadGenerationMismatch Client: {} Received: {} Current: {}", rClient.iClientId, message.uiLoadGeneration, miLoadGeneration);
		return;
	}

	// Answer every subscribe exactly once: the client matches each answer to its oldest outstanding request for the coord.
	int64_t iExistingSlot = rClient.FindSlotForCoordinate(coordinate);
	if (iExistingSlot >= 0)
	{
		LOG(kNetwork, kVerbose, "Server::ClientSubscribe AlreadySubscribed Client: {} Coord: ({},{})", rClient.iClientId, coordinate.iQ, coordinate.iR);
		SendSubscribeAccept(rClient, iExistingSlot, coordinate);
		return;
	}

	// The origin is always simulated and permits the initial game spawn.
	bool bAdjacent = (coordinate == kOriginCoordinate);
	bAdjacent |= std::ranges::any_of(rClient.authorizedCoordinates, [&coordinate](const GridCoord& rOwnedCoordinate)
	{
		// The client-supplied coord is hostile input, so its difference from an owned coord can overflow int32;
		// GridDistance takes both differences and their sum in int64.
		return GridDistance(rOwnedCoordinate, coordinate) <= 1;
	});
	if (!bAdjacent)
	{
		LOG(kNetwork, kWarning, "Server::ClientSubscribe Rejected (not adjacent) Client: {} Coord: ({},{})", rClient.iClientId, coordinate.iQ, coordinate.iR);
		SendSubscribeAccept(rClient, kiSubscribeRejectSlot, coordinate);
		return;
	}

	int64_t iSlot = rClient.AllocateSlot();
	if (iSlot < 0)
	{
		LOG(kNetwork, kWarning, "Server::ClientSubscribe No free slot Client: {} Coord: ({},{})", rClient.iClientId, coordinate.iQ, coordinate.iR);
		SendSubscribeAccept(rClient, kiSubscribeRejectSlot, coordinate);
		return;
	}

	rClient.slots.at(iSlot).subscription.coordinate = coordinate;
	rClient.slots.at(iSlot).subscription.flags.Set(SubscriptionFlags::kActive);
	++rClient.slots.at(iSlot).ack.uiEpoch;

	LOG(kNetwork, kDebug, "Server::ClientSubscribe Client: {} Coord: ({},{}) Slot: {}", rClient.iClientId, coordinate.iQ, coordinate.iR, iSlot);

	SendSubscribeAccept(rClient, iSlot, coordinate);

	ScopedSuppressAllocationTracking suppress;
	// Replace an existing pending entry for this client+slot (a subscribe->unsubscribe->subscribe
	// cycle reuses the slot with a new coord) rather than appending a duplicate. The replacement starts over from
	// static data: the reused slot has a new epoch, so the client drops static data sent under the old one.
	auto it = std::ranges::find_if(mPendingNewSubscriptions, [&rClient, iSlot](const PendingNewSubscription& rPending)
	{
		return rPending.iClientId == rClient.iClientId && rPending.iSlot == iSlot;
	});
	if (it != mPendingNewSubscriptions.end())
	{
		*it = {.iClientId = rClient.iClientId, .iSlot = iSlot, .coordinate = coordinate};
	}
	else
	{
		// Heap: pending subscription entry
		mPendingNewSubscriptions.push_back({.iClientId = rClient.iClientId, .iSlot = iSlot, .coordinate = coordinate});
	}
}

void Server::ClientUnsubscribe(std::span<const uint8_t> packetData, ClientConnection& rClient)
{
	NetworkMessages::ClientUnsubscribeMessage message {};
	NetworkMessages::Read(packetData, message);

	int64_t iSlotIndex = message.uiSlotIndex;
	int64_t iEpoch = message.uiEpoch;

	if (iSlotIndex < std::ssize(rClient.slots)
	 && (rClient.slots.at(static_cast<size_t>(iSlotIndex)).subscription.flags & SubscriptionFlags::kActive)
	 && rClient.slots.at(static_cast<size_t>(iSlotIndex)).ack.uiEpoch == iEpoch)
	{
		GridCoord coordinate = rClient.slots.at(static_cast<size_t>(iSlotIndex)).subscription.coordinate;
		rClient.FreeSlot(iSlotIndex);

		LOG(kNetwork, kDebug, "Server::ClientUnsubscribe Client: {} Slot: {} Coord: ({},{})", rClient.iClientId, iSlotIndex, coordinate.iQ, coordinate.iR);
	}

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	NetworkMessages::ServerUnsubscribeAckMessage response {.uiSlotIndex = static_cast<uint8_t>(iSlotIndex)};
	NetworkMessages::Write(rWorkbuffer, response);
	NetworkManager::SendPacket(rClient.pPeer, NetworkManager::kiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void Server::ClientResynchronizationRequest(std::span<const uint8_t> packetData, const ClientConnection& rClient)
{
	NetworkMessages::ClientResyncRequestMessage message {};
	NetworkMessages::Read(packetData, message);

	LOG(kNetwork, kWarning, "Server::ClientResyncRequest Client: {}", rClient.iClientId);

	if (std::ranges::contains(mPendingResynchronizationClientIds, rClient.iClientId))
	{
		return;
	}

	// Heap: pending resync client-id vector grows on request
	ScopedSuppressAllocationTracking suppress;
	mPendingResynchronizationClientIds.push_back(rClient.iClientId);
}

void Server::ClientCrashReport(std::span<const uint8_t> packetData, ClientConnection& rClient)
{
	NetworkMessages::ClientCrashReportMessage message {};
	NetworkMessages::Read(packetData, message);
	// Read bounds the declared size by the bytes present but does not require it to reach the packet end.
	if (message.report.iSize != std::ssize(packetData) - NetworkMessages::ClientCrashReportMessage::kiFixedSize)
	{
		NetworkMessages::ThrowCorruptStream("Server::ClientCrashReport");
	}
	if (!ParseCrashReportHeader(std::string_view(reinterpret_cast<const char*>(message.report.puiData), static_cast<size_t>(message.report.iSize))))
	{
		NetworkMessages::ThrowCorruptStream("Server::ClientCrashReport");
	}

	if (rClient.stateFlags & ClientConnection::StateFlags::kCrashReportReceived)
	{
		RecordContractViolation(rClient, ContractViolationKind::kRate, "repeat crash report", packetData[0], std::ssize(packetData));
		return;
	}
	rClient.stateFlags.Set(ClientConnection::StateFlags::kCrashReportReceived);

	std::chrono::sys_seconds now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
	std::chrono::sys_days today = std::chrono::floor<std::chrono::days>(now);
	if (today != mStoredCrashReportDay)
	{
		mStoredCrashReportDay = today;
		miStoredCrashReportCount = 0;
	}
	if (miStoredCrashReportCount >= kiMaximumStoredCrashReports)
	{
		LOG(kNetwork, kWarning, "Server::ClientCrashReport Not stored, daily cap reached Client: {}", rClient.iClientId);
		return;
	}

	ScopedSuppressAllocationTracking suppress;
	// Heap: timestamp formatting, the file path, the file stream, and the posting task allocate
	char pcFilename[96] {};
	std::format_to_n(pcFilename, std::ssize(pcFilename) - 1, "CrashReport-{:%Y%m%dT%H%M%SZ}-{}-{:016X}{:016X}.txt", now, miStoredCrashReportCount, rClient.clientGuid.uiHigh, rClient.clientGuid.uiLow);

	bool bWritten = gpFileManager->WriteFileAtomically({FileFlags::kAppDataDirectory, FileFlags::kWrite}, std::filesystem::path(pcFilename), [&message](std::fstream& rStream)
	{
		rStream.write(reinterpret_cast<const char*>(message.report.puiData), message.report.iSize);
	});
	if (!bWritten)
	{
		LOG(kNetwork, kWarning, "Server::ClientCrashReport Write failed Client: {} File: {}", rClient.iClientId, pcFilename);
		return;
	}

	++miStoredCrashReportCount;
	LOG(kNetwork, kInfo, "Server::ClientCrashReport Stored Client: {} File: {}", rClient.iClientId, pcFilename);
	gpCrashReportPoster->PostAsync(gpFileManager->GetFilePath({FileFlags::kAppDataDirectory}, pcFilename));
}

void Server::SendConnectionResponse(ENetPeer* pPeer, bool bAccepted, const char* pcMessage, const ClientGuid* pGloballyUniqueIdentifier)
{
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();

	NetworkMessages::ServerConnectionResponseMessage message
	{
		.uiLoadGeneration = static_cast<uint8_t>(miLoadGeneration),
		.uiAccepted = static_cast<uint8_t>(bAccepted ? 1 : 0),
		.uiDebugInput = static_cast<uint8_t>(kbDebugInput ? 1 : 0),
		.uiReplaying = static_cast<uint8_t>(game::gpGame->mbReplaying ? 1 : 0),
		.guid = (pGloballyUniqueIdentifier != nullptr) ? *pGloballyUniqueIdentifier : ClientGuid {},
		.bHasGuid = bAccepted && pGloballyUniqueIdentifier != nullptr,
		.rejectionMessage = (!bAccepted && pcMessage != nullptr) ? pcMessage : "",
	};
	NetworkMessages::Write(rWorkbuffer, message);

	NetworkManager::SendPacket(pPeer, NetworkManager::kiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void Server::SendSubscribeAccept(const ClientConnection& rClient, int64_t iSlot, GridCoord coordinate)
{
	int64_t iEpoch = (iSlot < std::ssize(rClient.slots)) ? rClient.slots.at(iSlot).ack.uiEpoch : 0;
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	NetworkMessages::ServerSubscribeAcceptMessage message
	{
		.uiLoadGeneration = static_cast<uint8_t>(miLoadGeneration),
		.uiSlotIndex = static_cast<uint8_t>(iSlot),
		.uiEpoch = static_cast<uint16_t>(iEpoch),
		.coord = coordinate,
	};
	NetworkMessages::Write(rWorkbuffer, message);
	NetworkManager::SendPacket(rClient.pPeer, NetworkManager::kiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void Server::BroadcastLoadNotification()
{
	LOG(kDefault, kDebug, "Server::BroadcastLoadNotification");

	for (const ClientConnection& rClient : mClients)
	{
		if (!rClient.bHandshakeComplete)
		{
			continue;
		}

		common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
		common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
		NetworkMessages::ServerLoadNotificationMessage message
		{
			.uiLoadGeneration = static_cast<uint8_t>(miLoadGeneration),
			.uiReplaying = static_cast<uint8_t>(game::gpGame->mbReplaying ? 1 : 0),
		};
		NetworkMessages::Write(rWorkbuffer, message);
		NetworkManager::SendPacket(rClient.pPeer, NetworkManager::kiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
	}
}

void Server::SendTimespeedUpdate(ENetPeer* pPeer, int64_t iMultiply, int64_t iDivide)
{
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	NetworkMessages::ServerTimespeedUpdateMessage message {.iMultiply = iMultiply, .iDivide = iDivide};
	NetworkMessages::Write(rWorkbuffer, message);
	NetworkManager::SendPacket(pPeer, NetworkManager::kiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void Server::BroadcastTimespeedIfChanged()
{
	if (!game::gpGame->mTimeStep.mbTimeScaleChanged) [[likely]]
	{
		return;
	}
	game::gpGame->mTimeStep.mbTimeScaleChanged = false;

	int64_t iMultiply = game::gpGame->mTimeStep.miTimeMultiply;
	int64_t iDivide = game::gpGame->mTimeStep.miTimeDivide;
	LOG(kNetwork, kDebug, "Server::BroadcastTimespeedIfChanged Multiply: {} Divide: {}", iMultiply, iDivide);

	for (const ClientConnection& rClient : mClients)
	{
		if (!rClient.bHandshakeComplete)
		{
			continue;
		}

		SendTimespeedUpdate(rClient.pPeer, iMultiply, iDivide);
	}
}

} // namespace engine

#endif // BT_SERVER
