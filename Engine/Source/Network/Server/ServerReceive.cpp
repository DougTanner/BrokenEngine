#include "Pch.h"

#if defined(BT_SERVER)

#include "Network/Server/Server.h"
#include "Network/Server/ServerSessionRuntime.h"
#include "Network/NetworkCursor.h"

#include "Game.h"

namespace engine
{

constexpr std::chrono::seconds kDesynchronizationDiagnosticCooldown = 2s;

// Full-state adoption freezes the client's floor at the adopted tick; repeated flushes tolerate ACK loss.
// Client::TrackReceivedTick disconnects when received ticks exceed that floor by more than kiNetworkBufferSize.
// Twice that window covers full-state delivery and the ACK trip while bounding relaxation of the monotonic floor guard.
constexpr int64_t kiPendingFullStateWindowTicks = 2 * kiNetworkBufferSize;

void Server::ClientAcknowledgementStream(std::span<const uint8_t> packetData, int64_t iClientId)
{
	ClientConnection* pClient = FindHandshakenClient(iClientId);
	if (pClient == nullptr)
	{
		return;
	}

	NetworkMessages::AckStreamEntry entries[NetworkManager::kiMaximumEnetCoordinateSlots] {};
	NetworkMessages::ClientAckStreamMessage message
	{
		.pEntries = entries,
		.iEntryCapacity = NetworkManager::kiMaximumEnetCoordinateSlots,
	};
	NetworkMessages::Read(packetData, message);

	// A layout failure already threw above and is recorded by the dispatch catch; this cross-check only
	// catches a readable packet whose declared slot count disagrees with its byte length, so no packet is
	// counted twice.
	if (static_cast<int64_t>(packetData.size()) != NetworkMessages::ClientAckStreamMessage::GetSize(message.uiSlotCount))
	{
		RecordContractViolation(iClientId, ContractViolationKind::kCorrupt, "ackstream size", packetData[0], static_cast<int64_t>(packetData.size()));
		return;
	}

	int64_t iFloorAdvanceCount = 0;
	for (int64_t i = 0; i < message.uiSlotCount; ++i)
	{
		const NetworkMessages::AckStreamEntry& rEntry = message.pEntries[i];
		uint8_t uiSlotIndex = rEntry.uiSlotIndex;
		uint16_t uiSlotEpoch = rEntry.uiEpoch;
		int64_t iSlotAcknowledgementFloor = rEntry.iAckFloor;
		uint64_t uiSlotBitfieldLow = rEntry.uiReceivedBitfieldLow;
		uint64_t uiSlotBitfieldHigh = rEntry.uiReceivedBitfieldHigh;

		if (!(uiSlotIndex < std::ssize(pClient->slots) && (pClient->slots.at(uiSlotIndex).subscription.flags & SubscriptionFlags::kActive)))
		{
			continue;
		}

		ClientConnection::SlotState& rSlot = pClient->slots.at(uiSlotIndex);

		// Adopting a full state re-baselines the client's floor to that full state's tick, so the floor legitimately
		// moves backward when the client had already received later ticks. Admit exactly that regression, never below
		// the tick whose authoritative state the client now holds. The acks the client sent before it adopted still
		// carry higher floors and satisfy the strict guard, so they cannot consume a backward target. A held restart
		// target is consumed when an accepted ACK crosses it; a backward target is consumed by the bounded regression.
		bool bFullStateRebaseline = rSlot.iPendingFullStateTick >= 0 && miLatestBufferedTick - rSlot.iPendingFullStateTick <= kiPendingFullStateWindowTicks && iSlotAcknowledgementFloor < rSlot.ack.iAcknowledgmentFloor && iSlotAcknowledgementFloor >= rSlot.iPendingFullStateTick;

		if (uiSlotEpoch == rSlot.ack.uiEpoch && (iSlotAcknowledgementFloor >= rSlot.ack.iAcknowledgmentFloor || bFullStateRebaseline))
		{
			// Clamp to server's latest sent tick to prevent future ACK floors
			iSlotAcknowledgementFloor = std::min(iSlotAcknowledgementFloor, miLatestBufferedTick);
			AckState& rAcknowledgementState = rSlot.ack;
			bool bFullStateAcknowledgement = bFullStateRebaseline || (rSlot.bHoldUpdatesUntilFullStateAck && rSlot.iPendingFullStateTick >= 0 && rAcknowledgementState.iAcknowledgmentFloor < rSlot.iPendingFullStateTick && iSlotAcknowledgementFloor >= rSlot.iPendingFullStateTick);
			if (bFullStateRebaseline)
			{
				LOG(kNetwork, kDebug, "Server::ClientAckStream Full-state floor re-baseline Client: {} Slot: {} Floor: {} -> {} FullStateTick: {}", iClientId, uiSlotIndex, rAcknowledgementState.iAcknowledgmentFloor, iSlotAcknowledgementFloor, rSlot.iPendingFullStateTick);
			}
			if (iSlotAcknowledgementFloor == rAcknowledgementState.iAcknowledgmentFloor)
			{
				rAcknowledgementState.uiReceivedBitfieldLow |= uiSlotBitfieldLow;
				rAcknowledgementState.uiReceivedBitfieldHigh |= uiSlotBitfieldHigh;
			}
			else
			{
				++iFloorAdvanceCount;
				rAcknowledgementState.iAcknowledgmentFloor = iSlotAcknowledgementFloor;
				rAcknowledgementState.uiReceivedBitfieldLow = uiSlotBitfieldLow;
				rAcknowledgementState.uiReceivedBitfieldHigh = uiSlotBitfieldHigh;
			}
			if (bFullStateAcknowledgement)
			{
				rSlot.iPendingFullStateTick = -1;
				rSlot.bHoldUpdatesUntilFullStateAck = false;
			}
		}
		else if (uiSlotEpoch != rSlot.ack.uiEpoch)
		{
			LOG(kNetwork, kVerbose, "Server::ClientAckStream EpochMismatch Client: {} Slot: {} ClientEpoch: {} ServerEpoch: {}", iClientId, uiSlotIndex, uiSlotEpoch, rSlot.ack.uiEpoch);
		}
	}

	if (iFloorAdvanceCount > 0)
	{
		if (pClient->bFloorStalled)
		{
			if (pClient->iPeakConsecutiveStallAcks >= kiFloorStallLogThreshold)
			{
				LOG(kNetwork, kVerbose, "Server::ClientAckStream FloorStallResolved Client: {} PeakStalledAcks: {} Slots: {}", iClientId, pClient->iPeakConsecutiveStallAcks, message.uiSlotCount);
			}
			pClient->bFloorStalled = false;
			pClient->iPeakConsecutiveStallAcks = 0;
		}
		pClient->iConsecutiveZeroAdvanceAcks = 0;
	}
	else if (message.uiSlotCount > 0)
	{
		++pClient->iConsecutiveZeroAdvanceAcks;
		pClient->iPeakConsecutiveStallAcks = std::max(pClient->iPeakConsecutiveStallAcks, pClient->iConsecutiveZeroAdvanceAcks);
		if (pClient->iConsecutiveZeroAdvanceAcks >= 3)
		{
			pClient->bFloorStalled = true;
		}
	}

	// Pipeline RTT: store client timestamp for echo in SendUpdate (monotonically increasing to guard against out-of-order packets)
	int64_t iClientTimestampNanoseconds = message.iTimestampNanoseconds;
	if (iClientTimestampNanoseconds > pClient->iClientTimestampNanoseconds)
	{
		pClient->iClientTimestampNanoseconds = iClientTimestampNanoseconds;
	}
}

void Server::ClientDesynchronizationReport(std::span<const uint8_t> packetData, int64_t iClientId)
{
	ClientConnection* pClient = FindHandshakenClient(iClientId);
	if (pClient == nullptr)
	{
		return;
	}

	NetworkMessages::ClientDesyncReportMessage message {};
	NetworkMessages::Read(packetData, message);
	// Before the cooldown, so a corrupt report does not consume it.
	if (message.iTick < 0)
	{
		NetworkMessages::ThrowCorruptStream("Server::ClientDesyncReport");
	}

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	if (now < pClient->desynchronizationReportDeadline)
	{
		return;
	}
	pClient->desynchronizationReportDeadline = now + kDesynchronizationDiagnosticCooldown;

	int64_t iTick = message.iTick;
	GridCoord coordinate = message.coord;
	uint64_t uiExpectedCrc = message.uiExpectedCrc;
	uint64_t uiActualCrc = message.uiActualCrc;

	char pcExpected[20] {};
	char pcActual[20] {};
	LOG(kNetwork, kError, "Server::ClientDesyncReport Frame: {} Grid: ({},{}) Expected: {} Actual: {}", iTick, coordinate.iX, coordinate.iY, common::ToHex(std::span(pcExpected), uiExpectedCrc), common::ToHex(std::span(pcActual), uiActualCrc));
}

void Server::ClientDebugFrameRequest(std::span<const uint8_t> packetData, ENetPeer* pPeer, int64_t iClientId)
{
	ClientConnection* pClient = FindHandshakenClient(iClientId);
	if (pClient == nullptr)
	{
		return;
	}

	NetworkMessages::ClientDebugFrameRequestMessage message {};
	NetworkMessages::Read(packetData, message);
	// Before the cooldown, so a corrupt request does not consume it.
	if (message.iTick < 0)
	{
		NetworkMessages::ThrowCorruptStream("Server::ClientDebugFrameRequest");
	}

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	if (now < pClient->debugFrameRequestDeadline)
	{
		return;
	}
	pClient->debugFrameRequestDeadline = now + kDesynchronizationDiagnosticCooldown;

	if constexpr (game::NetworkSessionContract::kbDebugFrames)
	{
		int64_t iTick = message.iTick;
		GridCoord coordinate = message.coord;

		LOG(kNetwork, kError, "Server::ClientDebugFrameRequest Frame: {} Grid: ({},{})", iTick, coordinate.iX, coordinate.iY);
		ScopedLogIndent scopedLogIndent;

		const BufferedFullFrame* pBuffered = nullptr;
		for (const BufferedFullFrame& rBuffered : mBufferedFullFrames)
		{
			if (rBuffered.iTick == iTick)
			{
				pBuffered = &rBuffered;
				break;
			}
		}

		if (pBuffered == nullptr)
		{
			LOG(kNetwork, kError, "Server::ClientDebugFrameRequest Frame {} not found in buffer", iTick);
			return;
		}

		auto it = pBuffered->serializedFrames.find(coordinate);
		if (it == pBuffered->serializedFrames.end())
		{
			LOG(kNetwork, kError, "Server::ClientDebugFrameRequest Frame: {} Coord: ({},{}) not found", iTick, coordinate.iX, coordinate.iY);
			return;
		}

		const std::string& rFrameData = it->second;

		// Heap: compression buffer may grow when LZ4 expansion bound exceeds current capacity
		ScopedSuppressAllocationTracking suppress;

		// LZ4 compress (reuse persistent compression buffer)
		int iCompressedSize = CompressToBuffer(std::span<const char>(rFrameData.data(), rFrameData.size()));

		common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
		common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();

		NetworkMessages::ServerDebugFrameMessage response
		{
			.iTick = iTick,
			.coord = coordinate,
			.iUncompressedSize = static_cast<int32_t>(rFrameData.size()),
			.compressedPayload = {.puiData = mCompressionBuffer.data(), .iSize = static_cast<int32_t>(iCompressedSize)},
		};
		NetworkMessages::Write(rWorkbuffer, response);

		NetworkManager::SendPacket(pPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
	}
}

void Server::ClientHello(std::span<const uint8_t> packetData, ENetPeer* pPeer, int64_t iClientId)
{
	NetworkMessages::ClientHelloMessage message {};
	NetworkMessages::Read(packetData, message);

	uint32_t uiClientProtocolVersion = message.uiProtocolVersion;
	if (uiClientProtocolVersion != kuiProtocolVersion)
	{
		char pcMessage[256] {};
		std::snprintf(pcMessage, sizeof(pcMessage), "Protocol version mismatch: server is %u, client is %u", kuiProtocolVersion, uiClientProtocolVersion);
		LOG(kNetwork, kWarning, "Server::ClientHello Rejecting Client: {} Reason: {}", iClientId, pcMessage);

		RejectHello(pPeer, iClientId, pcMessage);
		return;
	}

	int64_t iClientFrameVersion = message.iFrameVersion;
	if (iClientFrameVersion != game::NetworkSessionContract::GetFrameVersion())
	{
		char pcMessage[256] {};
		std::snprintf(pcMessage, sizeof(pcMessage), "Frame version mismatch: server is %lld, client is %lld", game::NetworkSessionContract::GetFrameVersion(), iClientFrameVersion);
		LOG(kNetwork, kWarning, "Server::ClientHello Rejecting Client: {} Reason: {}", iClientId, pcMessage);

		RejectHello(pPeer, iClientId, pcMessage);
		return;
	}

	common::crc_t uiClientPackIntegrityToken = message.uiPackIntegrityToken;
	common::crc_t uiServerPackIntegrityToken = gpFileManager->GetPackIntegrityToken();
	if (uiClientPackIntegrityToken != uiServerPackIntegrityToken)
	{
		char pcMessage[256] {};
		std::snprintf(pcMessage, sizeof(pcMessage), "Pack integrity mismatch: server token is %llu, client token is %llu. Regenerate generated game data and retry.", static_cast<unsigned long long>(uiServerPackIntegrityToken), static_cast<unsigned long long>(uiClientPackIntegrityToken));
		LOG(kNetwork, kWarning, "Server::ClientHello Rejecting Client: {} Reason: {}", iClientId, pcMessage);

		RejectHello(pPeer, iClientId, pcMessage);
		return;
	}

	char pcClientConfiguration[64] = {};
	size_t uiCopyLength = std::min(message.buildConfiguration.size(), sizeof(pcClientConfiguration) - 1);
	std::memcpy(pcClientConfiguration, message.buildConfiguration.data(), uiCopyLength);

	if (std::strcmp(pcClientConfiguration, kpcBuildConfigName) != 0)
	{
		LOG(kNetwork, kWarning, "Server::ClientHello Client {} build config mismatch: server is {}, client is {}", iClientId, kpcBuildConfigName, pcClientConfiguration);
	}

	ClientGuid clientGuid = message.bHasGuid ? message.guid : ClientGuid {};

	ClientConnection* pClient = FindClient(iClientId);
	if (pClient == nullptr)
	{
		LOG(kNetwork, kWarning, "Server::ClientHello Rejecting Client: {} Reason: no connection state", iClientId);
		return;
	}

	// Idempotent replay: an already-handshaken client re-sends the accept using its STORED GUID.
	// Do not overwrite established identity (fleet ownership is GUID-keyed) or mint a fresh GUID.
	if (pClient->bHandshakeComplete)
	{
		LOG(kNetwork, kInfo, "Server::ClientHello Replay Client: {} GUID: {} {}", iClientId, pClient->clientGuid.uiHigh, pClient->clientGuid.uiLow);
		SendConnectionResponse(pPeer, true, nullptr, &pClient->clientGuid);
		SendTimespeedUpdate(pPeer, game::gpGame->mTimeStep.miTimeMultiply, game::gpGame->mTimeStep.miTimeDivide);
		return;
	}

	// One live handshaken peer per GUID: fleet ownership is GUID-keyed, so a second peer must not take over the first's identity.
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
			LOG(kNetwork, kWarning, "Server::ClientHello Rejecting Client: {} Reason: {} Existing Client: {}", iClientId, pcMessage, it->iClientId);

			SendConnectionResponse(pPeer, false, pcMessage, nullptr);
			RemoveClient(iClientId);
			enet_peer_disconnect_later(pPeer, 0);
			return;
		}
	}

	if ((clientGuid.uiHigh == 0 && clientGuid.uiLow == 0))
	{
		::UUID uuid;
		[[maybe_unused]] RPC_STATUS iRpcStatus = UuidCreate(&uuid); // RPC_S_UUID_LOCAL_ONLY still yields a usable UUID
		std::memcpy(&clientGuid.uiHigh, &uuid, 8);
		std::memcpy(&clientGuid.uiLow, reinterpret_cast<const uint8_t*>(&uuid) + 8, 8);
	}

	pClient->bHandshakeComplete = true;
	pClient->clientGuid = clientGuid;

	LOG(kNetwork, kInfo, "Server::ClientHello Accepted Client: {} Config: {} GUID: {} {}", iClientId, pcClientConfiguration, clientGuid.uiHigh, clientGuid.uiLow);
	SendConnectionResponse(pPeer, true, nullptr, &clientGuid);

	SendTimespeedUpdate(pPeer, game::gpGame->mTimeStep.miTimeMultiply, game::gpGame->mTimeStep.miTimeDivide);
}

void Server::RejectHello(ENetPeer* pPeer, int64_t iClientId, const char* pcMessage)
{
	SendConnectionResponse(pPeer, false, pcMessage, nullptr);

	// An accepted client's removal must reach the game layer: the later DISCONNECT event finds no record and publishes nothing.
	if (const ClientConnection* pClient = FindHandshakenClient(iClientId); pClient != nullptr)
	{
		ScopedSuppressAllocationTracking suppress;
		mPendingDisconnects.push_back({.iClientId = iClientId, .clientGuid = pClient->clientGuid});
	}

	RemoveClient(iClientId);
	enet_peer_disconnect_later(pPeer, 0);
}

void Server::ClientSubscribe(std::span<const uint8_t> packetData, int64_t iClientId)
{
	NetworkMessages::ClientSubscribeMessage message {};
	NetworkMessages::Read(packetData, message);

	GridCoord coordinate = message.coord;

	ClientConnection* pClient = FindHandshakenClient(iClientId);
	if (pClient == nullptr)
	{
		return;
	}
	if (message.uiLoadGeneration != muiLoadGeneration)
	{
		LOG(kNetwork, kWarning, "Server::ClientSubscribe LoadGenerationMismatch Client: {} Received: {} Current: {}", iClientId, message.uiLoadGeneration, muiLoadGeneration);
		return;
	}

	// Answer every subscribe exactly once: the client matches each answer to its oldest outstanding request for the coord.
	int64_t iExistingSlot = pClient->FindSlotForCoordinate(coordinate);
	if (iExistingSlot >= 0)
	{
		LOG(kNetwork, kVerbose, "Server::ClientSubscribe AlreadySubscribed Client: {} Coord: ({},{})", iClientId, coordinate.iX, coordinate.iY);
		SendSubscribeAccept(*pClient, iExistingSlot, coordinate);
		return;
	}

	// The origin is always simulated and permits the initial fleet spawn.
	bool bAdjacent = (coordinate == kOriginCoordinate);
	for (const GridCoord& rOwnedCoordinate : pClient->authorizedCoordinates)
	{
		// 64-bit: the client-supplied coord is hostile input, so the difference can overflow int32 and std::abs(INT32_MIN) is undefined.
		int64_t iDeltaX = std::abs(static_cast<int64_t>(coordinate.iX) - static_cast<int64_t>(rOwnedCoordinate.iX));
		int64_t iDeltaY = std::abs(static_cast<int64_t>(coordinate.iY) - static_cast<int64_t>(rOwnedCoordinate.iY));
		if (iDeltaX <= 1 && iDeltaY <= 1)
		{
			bAdjacent = true;
			break;
		}
	}
	if (!bAdjacent)
	{
		LOG(kNetwork, kWarning, "Server::ClientSubscribe Rejected (not adjacent) Client: {} Coord: ({},{})", iClientId, coordinate.iX, coordinate.iY);
		SendSubscribeAccept(*pClient, kuiSubscribeRejectSlot, coordinate);
		return;
	}

	int64_t iSlot = pClient->AllocateSlot(game::NetworkSessionContract::kiCoordSlots);
	if (iSlot < 0)
	{
		LOG(kNetwork, kWarning, "Server::ClientSubscribe No free slot Client: {} Coord: ({},{})", iClientId, coordinate.iX, coordinate.iY);
		SendSubscribeAccept(*pClient, kuiSubscribeRejectSlot, coordinate);
		return;
	}

	pClient->slots.at(iSlot).subscription.coordinate = coordinate;
	pClient->slots.at(iSlot).subscription.flags.Set(SubscriptionFlags::kActive);
	++pClient->slots.at(iSlot).ack.uiEpoch;

	LOG(kNetwork, kDebug, "Server::ClientSubscribe Client: {} Coord: ({},{}) Slot: {}", iClientId, coordinate.iX, coordinate.iY, iSlot);

	SendSubscribeAccept(*pClient, iSlot, coordinate);

	ScopedSuppressAllocationTracking suppress;
	// Replace an existing pending entry for this client+slot (a subscribe->unsubscribe->subscribe
	// cycle reuses the slot with a new coord) rather than appending a duplicate.
	auto it = std::ranges::find_if(mPendingNewSubscriptions, [iClientId, iSlot](const PendingNewSubscription& rPending)
	{
		return rPending.iClientId == iClientId && rPending.iSlot == iSlot;
	});
	if (it != mPendingNewSubscriptions.end())
	{
		it->coordinate = coordinate;
	}
	else
	{
		// Heap: pending subscription entry
		mPendingNewSubscriptions.push_back({.iClientId = iClientId, .iSlot = iSlot, .coordinate = coordinate});
	}
}

void Server::ClientUnsubscribe(std::span<const uint8_t> packetData, int64_t iClientId)
{
	NetworkMessages::ClientUnsubscribeMessage message {};
	NetworkMessages::Read(packetData, message);

	uint8_t uiSlotIndex = message.uiSlotIndex;
	uint16_t uiEpoch = message.uiEpoch;

	ClientConnection* pClient = FindClient(iClientId);
	if (pClient == nullptr)
	{
		return;
	}

	if (uiSlotIndex < std::ssize(pClient->slots) && (pClient->slots.at(uiSlotIndex).subscription.flags & SubscriptionFlags::kActive)
	 && pClient->slots.at(uiSlotIndex).ack.uiEpoch == uiEpoch)
	{
		GridCoord coordinate = pClient->slots.at(uiSlotIndex).subscription.coordinate;
		pClient->FreeSlot(uiSlotIndex);

		LOG(kNetwork, kDebug, "Server::ClientUnsubscribe Client: {} Slot: {} Coord: ({},{})", iClientId, uiSlotIndex, coordinate.iX, coordinate.iY);
	}

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	NetworkMessages::ServerUnsubscribeAckMessage response {.uiSlotIndex = uiSlotIndex};
	NetworkMessages::Write(rWorkbuffer, response);
	NetworkManager::SendPacket(pClient->pPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void Server::ClientResynchronizationRequest(std::span<const uint8_t> packetData, int64_t iClientId)
{
	NetworkMessages::ClientResyncRequestMessage message {};
	NetworkMessages::Read(packetData, message);

	ClientConnection* pClient = FindHandshakenClient(iClientId);
	if (pClient == nullptr)
	{
		return;
	}

	LOG(kNetwork, kWarning, "Server::ClientResyncRequest Client: {}", iClientId);

	if (std::ranges::contains(mPendingResynchronizationClientIds, iClientId))
	{
		return;
	}

	// Heap: pending resync client-id vector grows on request
	ScopedSuppressAllocationTracking suppress;
	mPendingResynchronizationClientIds.push_back(iClientId);
}

} // namespace engine

#endif // BT_SERVER
