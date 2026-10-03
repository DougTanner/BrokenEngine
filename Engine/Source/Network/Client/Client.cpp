#include "Pch.h"

#include "Network/Client/Client.h"

#if defined(BT_CLIENT)

#include "Agent/Commands/ClientNetworkFixtures.h"

#include "Game.h"

namespace engine
{

constexpr std::chrono::seconds kSubscriptionTransitionTimeout = 5s;

void SubscribeRequests::Add(GridCoord coordinate)
{
	// Heap: subscribe request list grows on subscribe (SynchronizeSubscriptions suppresses tracking)
	mRecords.push_back({.coordinate = coordinate, .startTime = std::chrono::steady_clock::now()});
}

void SubscribeRequests::Cancel(GridCoord coordinate)
{
	for (SubscribeRequest& rRecord : mRecords)
	{
		if (rRecord.coordinate == coordinate)
		{
			rRecord.flags.Set(SubscribeRequestFlags::kCancelled);
		}
	}
	LOG(kNetwork, kVerbose, "SubscribeRequests::Cancel Coord: ({},{}) Records: {}", coordinate.iX, coordinate.iY, std::ssize(mRecords));
}

void SubscribeRequests::WarnTimedOut(std::chrono::steady_clock::time_point now)
{
	for (SubscribeRequest& rRecord : mRecords)
	{
		if (rRecord.flags & SubscribeRequestFlags::kCancelled)
		{
			continue;
		}

		if (rRecord.flags & SubscribeRequestFlags::kTimeoutWarned)
		{
			continue;
		}

		if (now - rRecord.startTime < kSubscriptionTransitionTimeout)
		{
			continue;
		}

		rRecord.flags.Set(SubscribeRequestFlags::kTimeoutWarned);
		LOG(kNetwork, kWarning, "SubscribeRequests::WarnTimedOut unanswered subscribe Coord: ({},{})", rRecord.coordinate.iX, rRecord.coordinate.iY);
	}
}

bool SubscribeRequests::TakeAnswer(GridCoord coordinate)
{
	auto it = std::ranges::find(mRecords, coordinate, &SubscribeRequest::coordinate);
	if (it == mRecords.end())
	{
		return false;
	}
	bool bLive = !(it->flags & SubscribeRequestFlags::kCancelled);
	mRecords.erase(it);
	return bLive;
}

bool SubscribeRequests::IsLive(GridCoord coordinate) const
{
	return std::ranges::any_of(mRecords, [coordinate](const SubscribeRequest& rRecord)
	{
		return rRecord.coordinate == coordinate && !(rRecord.flags & SubscribeRequestFlags::kCancelled);
	});
}

Client::Client(const char* pcServerAddress, uint16_t uiPort, int64_t iCoordinateSlotCount, const ClientGuid& rGuid, GuidAssignedCallback pGuidAssignedCallback)
{
	ASSERT(gpClient == nullptr);

	gpClient = this;

	mClientGuid = rGuid;
	mpGuidAssignedCallback = pGuidAssignedCallback;

	ScopedSuppressAllocationTracking suppress;

	mReceivedCoordinateUpdates.resize(iCoordinateSlotCount);
	mCoordinateSlots.resize(iCoordinateSlotCount);
	mStatusChangeScratch.resize(kiMaximumStatusChangesPerCell);
	ENetAddress localAddress {};
	localAddress.host = htonl(INADDR_LOOPBACK);
	// Heap: ENet allocates host data internally
	mpHost = enet_host_create((gLaunchOptions.flags & LaunchOptionFlags::kLoopbackOnly) ? &localAddress : nullptr, 1, NetworkManager::kuiChannelCount, 0, 0);
	if (mpHost == nullptr)
	{
		LOG(kNetwork, kWarning, "Client::Client enet_host_create failed");
		return;
	}
	mpHost->checksum = enet_crc32;
	// 1MB send/receive buffers to handle bursty packet traffic
	enet_socket_set_option(mpHost->socket, ENET_SOCKOPT_SNDBUF, 1'024 * 1'024);
	enet_socket_set_option(mpHost->socket, ENET_SOCKOPT_RCVBUF, 1'024 * 1'024);

	ENetAddress address {};
	enet_address_set_host(&address, pcServerAddress);
	address.port = uiPort;

	// Heap: ENet allocates peer data internally
	mpServerPeer = enet_host_connect(mpHost, &address, NetworkManager::kuiChannelCount, 0);
}

Client::~Client()
{
	if (mpServerPeer != nullptr && (mStateFlags & ClientStateFlags::kConnected))
	{
		enet_peer_disconnect(mpServerPeer, 0);

		// Allow time for disconnect to be sent
		ENetEvent event {};
		// Heap: ENet service polls and may allocate event packets during disconnect drain
		ScopedSuppressAllocationTracking suppress;
		while (enet_host_service(mpHost, &event, 100) > 0)
		{
			if (event.type == ENET_EVENT_TYPE_DISCONNECT)
			{
				break;
			}
			if (event.type == ENET_EVENT_TYPE_RECEIVE)
			{
				enet_packet_destroy(event.packet);
			}
		}
	}

	if (mpHost != nullptr)
	{
		// Heap: ENet destroys host data internally
		ScopedSuppressAllocationTracking suppress;
		enet_host_destroy(mpHost);
	}

	if (gpClient == this)
	{
		gpClient = nullptr;
	}
}

void Client::FreeSlot(int64_t iSlot)
{
	ClientCoordSlot& rSlot = mCoordinateSlots.at(iSlot);
	// Reset slot state but retain the epoch of the subscription being cleared, so a packet still in
	// flight from that subscription can be recognized as stale before the slot is admitted again
	uint16_t uiEpoch = rSlot.acknowledgementState.uiEpoch;
	rSlot = {};
	rSlot.acknowledgementState.uiEpoch = uiEpoch;
}

void Client::ResetAllSlots()
{
	ClientNetworkFixtures::Reset(*this);
	for (int64_t i = 0; i < std::ssize(mCoordinateSlots); ++i)
	{
		FreeSlot(i);
	}
	mSubscribeRequests.mRecords.clear();
}

void Client::RecoverTimedOutSubscriptions()
{
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	mSubscribeRequests.WarnTimedOut(now);
	// kWaitingFullState has no timeout: the server keeps an accepted subscription queued until the coordinate has a frame with
	// navigation data built, which during replay waits for the coordinate's recorded activation
	for (int64_t i = 0; i < std::ssize(mCoordinateSlots); ++i)
	{
		const ClientCoordSlot& rSlot = mCoordinateSlots.at(i);
		if (rSlot.eState == CoordSubscriptionState::kUnsubscribing && now - rSlot.transitionStartTime >= kSubscriptionTransitionTimeout)
		{
			FreeSlot(i);
		}
	}
}

void Client::Poll(const NetworkTimeState& rTimeState)
{
	ASSERT(common::gpMultithreading->IsMainThread());
	mTimeState = rTimeState;

	if (mpHost == nullptr)
	{
		return;
	}

	for (std::vector<ReceivedCoordUpdate>& rSlotUpdates : mReceivedCoordinateUpdates)
	{
		rSlotUpdates.clear();
	}
	mReceivedFullStates.clear();
	mReceivedStaticData.clear();
	mReceivedGamePackets.clear();

	ENetEvent event {};
	while (enet_host_service(mpHost, &event, 0) > 0)
	{
		switch (event.type)
		{
			case ENET_EVENT_TYPE_CONNECT:
			{
				LOG(kNetwork, kInfo, "Client::Poll ENET_EVENT_TYPE_CONNECT");
				mStateFlags.Set(ClientStateFlags::kConnected);
				// Disable ENet peer throttle to prevent unreliable packet drops during reconciliation stalls
				enet_peer_throttle_configure(mpServerPeer, UINT32_MAX, 0, 0);
				SendHello();
				break;
			}
			case ENET_EVENT_TYPE_DISCONNECT:
				if constexpr (keNetworkSimulation != engine::NetworkSimulationLevel::kDisabled)
				{
					// ENet can acknowledge a reliable rejection before the simulated receive delay releases it.
					// Deliver that rejection before the disconnect tears down the session and its delay queue.
					auto it = std::ranges::find_if(mDelayedPackets, [](const DelayedPacket& rPacket)
					{
						return std::ssize(rPacket.data) >= NetworkMessages::ServerConnectionResponseMessage::kiMinSize
						    && static_cast<PacketType>(rPacket.data.at(0)) == PacketType::kServerConnectionResponse
						    && rPacket.data.at(2) == 0;
					});
					if (it != mDelayedPackets.end())
					{
						Receive(it->data);
						mDelayedPackets.erase(it);
					}
				}
				mStateFlags.Set(ClientStateFlags::kConnected, false);
				mStateFlags.Set(ClientStateFlags::kDisconnectedEvent);
				mpServerPeer = nullptr;
				LOG(kNetwork, kInfo, "ENET_EVENT_TYPE_DISCONNECT");
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
			Receive(rPacket.data);
		});
	}

	uint32_t uiReceivedData = mpHost->totalReceivedData;
	uint32_t uiSentData = mpHost->totalSentData;
	mBytesInPerSecond.Set(static_cast<int64_t>(uiReceivedData - muiPreviousReceivedData));
	mBytesOutPerSecond.Set(static_cast<int64_t>(uiSentData - muiPreviousSentData));
	muiPreviousReceivedData = uiReceivedData;
	muiPreviousSentData = uiSentData;
}

void Client::DispatchIncoming(ENetEvent& rEvent, bool bFastForward)
{
	if constexpr (keNetworkSimulation != engine::NetworkSimulationLevel::kDisabled)
	{
		static constexpr NetworkSimulationConfig kSimulationConfiguration = GetNetworkSimulationConfiguration(keNetworkSimulation);
		NetworkSimulation::DispatchOrEnqueue(mDelayedPackets, mNetworkSimulationState, kSimulationConfiguration, bFastForward, rEvent, [this](ENetEvent& rInner)
		{
			Receive(rInner);
		});
	}
	else
	{
		Receive(rEvent);
		enet_packet_destroy(rEvent.packet);
	}
}

void Client::Receive(ENetEvent& rEvent)
{
	Receive(std::span<const uint8_t>(rEvent.packet->data, rEvent.packet->dataLength));
}

void Client::Receive(std::span<const uint8_t> packetData)
{
	if (packetData.size() < 1)
	{
		return;
	}

	PacketType eType = static_cast<PacketType>(packetData[0]);

	try
	{
		switch (eType)
		{
			case PacketType::kServerCoordinateFullState:
				ServerCoordinateFullState(packetData);
				break;
			case PacketType::kServerCoordinateStaticData:
				ServerCoordinateStaticData(packetData);
				break;
			case PacketType::kServerCoordinateUpdate:
				ServerCoordinateUpdateOrResend(packetData, true);
				break;
			case PacketType::kServerCoordinateResend:
				ServerCoordinateUpdateOrResend(packetData, false);
				break;
			case PacketType::kServerDebugFrame:
				ServerDebugFrame(packetData);
				break;
			case PacketType::kServerConnectionResponse:
				ServerConnectionResponse(packetData);
				break;
			case PacketType::kServerSubscribeAccept:
				ServerSubscribeAccept(packetData);
				break;
			case PacketType::kServerUnsubscribeAck:
				ServerUnsubscribeAcknowledgement(packetData);
				break;
			case PacketType::kServerLoadNotification:
				ServerLoadNotification(packetData);
				break;
			case PacketType::kServerTimespeedUpdate:
				ServerTimespeedUpdate(packetData);
				break;
			default:
				if (static_cast<uint8_t>(eType) >= static_cast<uint8_t>(PacketType::kGamePacketStart))
				{
					ScopedSuppressAllocationTracking suppress;
					// Heap: raw game packet buffer grows on game-specific packets
					mReceivedGamePackets.emplace_back(packetData[0], std::vector<uint8_t>(packetData.begin() + 1, packetData.end()));
				}
				break;
		}
	}
	// The client trusts its server, so a shared reader throwing std::ios_base::failure here means server bytes the
	// client cannot decode, and it cannot keep playing against them — assert so the crash report names the reader.
	// The std::exception catch below is log-and-continue, so an ordinary local failure (bad_alloc, .at(), file I/O
	// beneath a handler) is never blamed on the peer.
	catch (const std::ios_base::failure& rException)
	{
		LOG(kNetwork, kError, "Client::Receive dropped corrupt packet (type {}): {}", static_cast<uint8_t>(eType), rException.what());
		ASSERT(false);
	}
	catch (const std::exception& rException)
	{
		LOG(kNetwork, kWarning, "Client::Receive dropped packet after local failure (type {}): {}", static_cast<uint8_t>(eType), rException.what());
	}
}

void Client::TrackReceivedTick(int64_t iSlot, int64_t iTick)
{
	if (mStateFlags & ClientStateFlags::kDesynchronizationDebugMode)
	{
		return;
	}

	AckState& rAcknowledgementState = mCoordinateSlots.at(iSlot).acknowledgementState;

	// First frame received, initialize the ACK floor
	if (rAcknowledgementState.iAcknowledgmentFloor < 0)
	{
		rAcknowledgementState.iAcknowledgmentFloor = iTick;
		mFramesReceived.Set(1);
		return;
	}

	// Already acknowledged
	if (iTick <= rAcknowledgementState.iAcknowledgmentFloor)
	{
		return;
	}

	int64_t iBitIndex = iTick - rAcknowledgementState.iAcknowledgmentFloor - 1;
	if (iBitIndex >= kiNetworkBufferSize)
	{
		LOG(kNetwork, kWarning, "Client::TrackReceivedTick Too many missing frames, disconnecting Slot: {} Gap: {}", iSlot, iBitIndex + 1);
		mStateFlags.Set(ClientStateFlags::kDisconnectedEvent);
		return;
	}

	// Mark this frame as received and advance the floor past any contiguous run
	if (iBitIndex < 64)
	{
		rAcknowledgementState.uiReceivedBitfieldLow |= (1ULL << iBitIndex);
	}
	else
	{
		rAcknowledgementState.uiReceivedBitfieldHigh |= (1ULL << (iBitIndex - 64));
	}
	mFramesReceived.Set(1);

	int64_t iPreviousFloor = rAcknowledgementState.iAcknowledgmentFloor;
	while ((rAcknowledgementState.uiReceivedBitfieldLow & 1ULL) != 0u)
	{
		++rAcknowledgementState.iAcknowledgmentFloor;
		rAcknowledgementState.uiReceivedBitfieldLow >>= 1;
		if ((rAcknowledgementState.uiReceivedBitfieldHigh & 1ULL) != 0u)
		{
			rAcknowledgementState.uiReceivedBitfieldLow |= (1ULL << 63);
		}
		rAcknowledgementState.uiReceivedBitfieldHigh >>= 1;
	}
	if (rAcknowledgementState.iAcknowledgmentFloor != iPreviousFloor && rAcknowledgementState.iAcknowledgmentFloor - iPreviousFloor > 50)
	{
		LOG(kNetwork, kVerbose, "Client::TrackReceivedTick FloorAdvance Slot: {} Floor: {} -> {} Delta: {}", iSlot, iPreviousFloor, rAcknowledgementState.iAcknowledgmentFloor, rAcknowledgementState.iAcknowledgmentFloor - iPreviousFloor);
	}
}

void Client::Flush()
{
	if (mpHost == nullptr)
	{
		return;
	}
	enet_host_flush(mpHost);
}

void Client::Disconnect()
{
	if (mpServerPeer != nullptr && (mStateFlags & ClientStateFlags::kConnected))
	{
		// Heap: ENet may queue a peer disconnect packet
		ScopedSuppressAllocationTracking suppress;
		enet_peer_disconnect(mpServerPeer, 0);
		mStateFlags.Set(ClientStateFlags::kConnected, false);
	}
}

} // namespace engine

#endif // BT_CLIENT
