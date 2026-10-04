#include "Pch.h"

#include "Network/Client/Client.h"

#if defined(BT_CLIENT)

#include "Agent/Commands/ClientNetworkFixtures.h"
#include "File/PackChunks.h"
#include "Frame/FrameStaticData.h"
#include "Network/NetworkCursor.h"

#include "Game.h"

namespace engine
{

Client::Client(const char* pcServerAddress, uint16_t uiPort, int64_t iCoordinateSlotCount, const ClientGuid& rGuid, GuidAssignedCallback pGuidAssignedCallback)
{
	ASSERT(gpClient == nullptr);

	gpClient = this;

	mClientGuid = rGuid;
	mpGuidAssignedCallback = pGuidAssignedCallback;

	ScopedSuppressAllocationTracking suppress;

	mReceivedCoordinateUpdates.resize(iCoordinateSlotCount);
	mSubscriptions.mCoordinateSlots.resize(iCoordinateSlotCount);
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

void Client::ResetAllSlots()
{
	ClientNetworkFixtures::Reset(*this);
	mSubscriptions.Reset();
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

	AckState& rAcknowledgementState = mSubscriptions.mCoordinateSlots.at(iSlot).acknowledgementState;

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
		rAcknowledgementState.uiReceivedBitfieldLow |= (1ui64 << iBitIndex);
	}
	else
	{
		rAcknowledgementState.uiReceivedBitfieldHigh |= (1ui64 << (iBitIndex - 64));
	}
	mFramesReceived.Set(1);

	int64_t iPreviousFloor = rAcknowledgementState.iAcknowledgmentFloor;
	while ((rAcknowledgementState.uiReceivedBitfieldLow & 1ui64) != 0ui32)
	{
		++rAcknowledgementState.iAcknowledgmentFloor;
		rAcknowledgementState.uiReceivedBitfieldLow >>= 1;
		if ((rAcknowledgementState.uiReceivedBitfieldHigh & 1ui64) != 0ui32)
		{
			rAcknowledgementState.uiReceivedBitfieldLow |= (1ui64 << 63);
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

static std::unique_ptr<game::Frame> DecompressAndReadFrame(int32_t iUncompressedSize, const NetworkMessages::PacketPayload& rCompressedPayload)
{
	std::string decompressed(iUncompressedSize, '\0');
	LZ4_decompress_safe(reinterpret_cast<const char*>(rCompressedPayload.puiData), decompressed.data(), rCompressedPayload.iSize, iUncompressedSize);

	std::istringstream frameStream(std::move(decompressed), std::ios::binary);
	std::unique_ptr<game::Frame> pFrame = std::make_unique<game::Frame>();
	game::NetworkSessionContract::ReadFrame(frameStream, *pFrame);
	return pFrame;
}

std::optional<uint8_t> Client::DrainLoadNotification()
{
	if (!(mStateFlags & ClientStateFlags::kLoadNotificationReceived))
	{
		return std::nullopt;
	}
	mStateFlags.Set(ClientStateFlags::kLoadNotificationReceived, false);
	std::optional<uint8_t> uiLoadGeneration = muiPendingLoadGeneration;
	muiPendingLoadGeneration.reset();
	return uiLoadGeneration;
}

void Client::ServerCoordinateFullState(std::span<const uint8_t> packetData)
{
	NetworkMessages::ServerCoordFullStateMessage message {};
	NetworkMessages::Read(packetData, message);
	if (message.uiLoadGeneration != muiCommittedLoadGeneration)
	{
		LOG(kNetwork, kWarning, "Client::ServerCoordFullState Dropped mismatched load generation Packet: {} Current: {}", message.uiLoadGeneration, muiCommittedLoadGeneration);
		return;
	}

	uint8_t uiSlotIndex = message.uiSlotIndex;
	uint16_t uiEpoch = message.uiEpoch;
	int64_t iTick = message.iTick;
	GridCoord coord = message.coord;

	LOG(kNetwork, kVerbose, "Client::ServerCoordFullState Frame: {} Slot: {} Coord: ({},{})", iTick, uiSlotIndex, coord.iX, coord.iY);
	ScopedLogIndent scopedLogIndent;

	ScopedSuppressAllocationTracking suppress;

	ClientCoordSlot& rSlot = mSubscriptions.mCoordinateSlots.at(uiSlotIndex);
	ClientSubscriptions::FullStateFlags_t actions = mSubscriptions.ClassifyFullState(uiSlotIndex, uiEpoch, coord);

	if (actions & ClientSubscriptions::FullStateFlags::kRejectAsGhost)
	{
		common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
		common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
		NetworkMessages::ClientUnsubscribeMessage unsubscribe {.uiSlotIndex = uiSlotIndex, .uiEpoch = uiEpoch};
		NetworkMessages::Write(rWorkbuffer, unsubscribe);
		NetworkManager::SendPacket(mpServerPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
		LOG(kNetwork, kVerbose, "Client::ServerCoordFullState coord mismatch, sent unsubscribe for ghost Slot: {} Coord: ({},{}) SlotCoord: ({},{})", uiSlotIndex, coord.iX, coord.iY, rSlot.coordinate.iX, rSlot.coordinate.iY);
		// Retire the ghost's epoch on a slot with no server-assigned epoch so its late accept is dropped
		bool bHasNoAssignedEpoch = rSlot.eState == CoordSubscriptionState::kUnsubscribed;
		if (bHasNoAssignedEpoch && static_cast<int16_t>(uiEpoch - rSlot.acknowledgementState.uiEpoch) > 0)
		{
			rSlot.acknowledgementState.uiEpoch = uiEpoch;
		}
		return;
	}

	if (!(actions & ClientSubscriptions::FullStateFlags::kCommit))
	{
		return;
	}

	std::unique_ptr<game::Frame> pFrame = DecompressAndReadFrame(message.iUncompressedSize, message.compressedPayload);

	if (actions & ClientSubscriptions::FullStateFlags::kAdoptCoordinate)
	{
		rSlot.coordinate = coord;
	}

	ReceivedCoordFullState fullState {};
	fullState.iTick = iTick;
	fullState.coordinate = coord;
	fullState.pFrame = std::move(pFrame);

	// Heap: received full states vector grows on new cell data
	mReceivedFullStates.push_back(std::move(fullState));

	rSlot.acknowledgementState.iAcknowledgmentFloor = iTick;
	rSlot.acknowledgementState.uiReceivedBitfieldLow = 0;
	rSlot.acknowledgementState.uiReceivedBitfieldHigh = 0;
	rSlot.acknowledgementState.uiEpoch = uiEpoch;
	rSlot.eState = CoordSubscriptionState::kActive;
	rSlot.transitionStartTime = {};
}

void Client::ServerCoordinateStaticData(std::span<const uint8_t> packetData)
{
	NetworkMessages::ServerCoordStaticDataMessage message {};
	NetworkMessages::Read(packetData, message);
	if (message.uiLoadGeneration != muiCommittedLoadGeneration)
	{
		LOG(kNetwork, kWarning, "Client::ServerCoordStaticData Dropped mismatched load generation Packet: {} Current: {}", message.uiLoadGeneration, muiCommittedLoadGeneration);
		return;
	}

	uint8_t uiSlotIndex = message.uiSlotIndex;
	uint16_t uiEpoch = message.uiEpoch;
	GridCoord coord = message.coord;

	const ClientCoordSlot& rSlot = mSubscriptions.mCoordinateSlots.at(uiSlotIndex);
	// Before the accept (this lane can overtake it and the unsubscribe ACK), a kUnsubscribed or reallocated kUnsubscribing
	// slot admits only a coord with a live subscribe request
	bool bAwaitingAccept = (rSlot.eState == CoordSubscriptionState::kUnsubscribed || rSlot.eState == CoordSubscriptionState::kUnsubscribing) && mSubscriptions.mSubscribeRequests.IsLive(coord);
	if (rSlot.eState != CoordSubscriptionState::kWaitingFullState && !bAwaitingAccept)
	{
		return;
	}

	// Stale static data from a previous subscription to a different coord (recycled slot) — silently drop (the full-state path owns ghost unsubscribe)
	if (rSlot.eState == CoordSubscriptionState::kWaitingFullState && rSlot.coordinate != coord)
	{
		return;
	}
	// Epoch guard: SubscribeAccept set the epoch
	if (rSlot.eState == CoordSubscriptionState::kWaitingFullState && uiEpoch != rSlot.acknowledgementState.uiEpoch)
	{
		return;
	}
	// The other admitted states hold no epoch for this subscription, so they are guarded by the retained epoch instead
	if (rSlot.eState != CoordSubscriptionState::kWaitingFullState && mSubscriptions.IsStaleRetainedEpoch(uiSlotIndex, uiEpoch, coord))
	{
		return;
	}

	ScopedSuppressAllocationTracking suppress;

	std::string staticBytes(reinterpret_cast<const char*>(message.staticData.puiData), message.staticData.iSize);
	std::istringstream staticStream(std::move(staticBytes), std::ios::binary);

	ReceivedStaticData received {};
	received.coordinate = coord;
	received.staticData.Read(staticStream, /*bIncludeNavigationData=*/true);

	// Heap: received static data vector grows on new subscription
	mReceivedStaticData.push_back(std::move(received));
}

void Client::ServerCoordinateUpdateOrResend(std::span<const uint8_t> packetData, bool bProcessRoundTripTime)
{
	auto Receive = [this, bProcessRoundTripTime, packetData](const NetworkMessages::CoordUpdateFields& rMessage)
	{
		if (rMessage.uiLoadGeneration != muiCommittedLoadGeneration)
		{
			LOG(kNetwork, kWarning, "Client::ServerCoordUpdateOrResend Dropped mismatched load generation Packet: {} Current: {}", rMessage.uiLoadGeneration, muiCommittedLoadGeneration);
			return;
		}
		// Pipeline RTT: read echoed client timestamp (monotonic guard prevents duplicate processing during multi-frame ticks)
		if (bProcessRoundTripTime && rMessage.iEchoedTimestampNanoseconds > 0 && rMessage.iEchoedTimestampNanoseconds > miLastEchoedTimestampNanoseconds)
		{
			miLastEchoedTimestampNanoseconds = rMessage.iEchoedTimestampNanoseconds;
			int64_t iNowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
			int64_t iRttUs = (iNowNs - rMessage.iEchoedTimestampNanoseconds) / 1'000;
			if (iRttUs >= 0)
			{
				mSmoothedPipelineRoundTripTimeMicroseconds = iRttUs;
				mSmoothedPipelineRoundTripTimeMicroseconds.Update();

				std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
				if (mStateFlags & ClientStateFlags::kHasLastUpdateArrival)
				{
					if (mStateFlags & ClientStateFlags::kSkipNextJitterInterval)
					{
						mStateFlags.Set(ClientStateFlags::kSkipNextJitterInterval, false);
					}
					else
					{
						int64_t iIntervalUs = std::chrono::duration_cast<std::chrono::microseconds>(now - mLastUpdateArrival).count();
						// Server broadcast cadence is wall-scaled by the debug timescale; expect the scaled wall interval, not the fixed sim tick period
						int64_t iExpectedMicroseconds = mTimeState.iExpectedUpdateIntervalMicroseconds;
						int64_t iDeviation = std::abs(iIntervalUs - iExpectedMicroseconds);
						mSmoothedJitterMicroseconds = iDeviation;
						mSmoothedJitterMicroseconds.Update();
					}
				}
				mLastUpdateArrival = now;
				mStateFlags.Set(ClientStateFlags::kHasLastUpdateArrival);
			}
		}

		ClientSubscriptions::CoordUpdateFlags_t actions = mSubscriptions.ClassifyCoordinateUpdate(rMessage.uiSlotIndex, rMessage.uiEpoch);
		if (!(actions & ClientSubscriptions::CoordUpdateFlags::kCommit))
		{
			return;
		}

		ScopedSuppressAllocationTracking suppress;

		ReceivedCoordUpdate update {};
		update.iTick = rMessage.iTick;
		update.uiSharedCrc = static_cast<common::crc_t>(rMessage.uiSharedCrc);

		if (rMessage.compressedPayload.iSize > 0)
		{
			int64_t iCount = game::NetworkSessionContract::DecompressStatusChanges(std::span<const uint8_t>(rMessage.compressedPayload.puiData, static_cast<size_t>(rMessage.compressedPayload.iSize)), mStatusChangeScratch.data());
			// Heap: exact-size copy out of the reused 1024-cap decode scratch, so the buffered update carries no capacity slack
			update.statusChanges.assign(mStatusChangeScratch.begin(), mStatusChangeScratch.begin() + iCount);
		}

		// Heap: received updates vector grows each tick
		mReceivedCoordinateUpdates.at(rMessage.uiSlotIndex).push_back(std::move(update));
		if (actions & ClientSubscriptions::CoordUpdateFlags::kTrackTick)
		{
			TrackReceivedTick(rMessage.uiSlotIndex, rMessage.iTick);
		}

		if (bProcessRoundTripTime)
		{
			ClientNetworkFixtures::CaptureStaleUpdate(*this, packetData, rMessage.uiSlotIndex, rMessage.uiEpoch, rMessage.iTick);
		}
	};

	if (bProcessRoundTripTime)
	{
		NetworkMessages::ServerCoordUpdateMessage message {};
		NetworkMessages::Read(packetData, message);
		Receive(message);
	}
	else
	{
		NetworkMessages::ServerCoordResendMessage message {};
		NetworkMessages::Read(packetData, message);
		Receive(message);
	}
}

void Client::ServerDebugFrame(std::span<const uint8_t> packetData)
{
	NetworkMessages::ServerDebugFrameMessage message {};
	NetworkMessages::Read(packetData, message);

	int64_t iTick = message.iTick;
	GridCoord coord = message.coord;
	LOG(kNetwork, kError, "Client::ServerDebugFrame Frame: {} Grid: ({},{})", iTick, coord.iX, coord.iY);
	ScopedLogIndent scopedLogIndent;

	// Heap: LZ4 decompresses debug frame; Frame allocated on heap
	ScopedSuppressAllocationTracking suppress;

	std::unique_ptr<game::Frame> pFrame = DecompressAndReadFrame(message.iUncompressedSize, message.compressedPayload);

	mpReceivedDebugFrame = std::make_unique<ReceivedDebugFrame>();
	mpReceivedDebugFrame->iTick = iTick;
	mpReceivedDebugFrame->coordinate = coord;
	mpReceivedDebugFrame->pFrame = std::move(pFrame);
}

void Client::ServerConnectionResponse(std::span<const uint8_t> packetData)
{
	NetworkMessages::ServerConnectionResponseMessage message {};
	NetworkMessages::Read(packetData, message);

	bool bAccepted = message.uiAccepted != 0;

	if (bAccepted)
	{
		if (mStateFlags & ClientStateFlags::kConnectionAccepted)
		{
			return;
		}
		muiCommittedLoadGeneration = message.uiLoadGeneration;
		mStateFlags.Set(ClientStateFlags::kConnectionAccepted);
		if (message.uiDebugInput == 1)
		{
			mStateFlags.Set(ClientStateFlags::kServerDebugInput);
		}

		if (message.bHasGuid)
		{
			mClientGuid = message.guid;
			LOG(kNetwork, kInfo, "Client GUID assigned: {} {}", mClientGuid.uiHigh, mClientGuid.uiLow);

			// Persist via the session-provided callback (keeps GUID disk I/O out of the transport layer)
			if (mpGuidAssignedCallback != nullptr)
			{
				mpGuidAssignedCallback(mClientGuid);
			}
		}

		// Seed smoothed pipeline RTT from game-layer handshake measurement so the value flows through the network sim
		int64_t iNowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		int64_t iRttUs = (miHelloSendTimeNanoseconds > 0) ? (iNowNs - miHelloSendTimeNanoseconds) / 1'000 : 0;
		if (iRttUs > 0 && iRttUs < 60'000'000)
		{
			mSmoothedPipelineRoundTripTimeMicroseconds.Seed(iRttUs);
		}
		LOG(kNetwork, kDebug, "Client::ServerConnectionResponse Handshake RTT: {} us", iRttUs);
	}
	else
	{
		size_t iCopyLength = std::min(message.rejectionMessage.size(), sizeof(mpcRejectionReason) - 1);
		std::memcpy(mpcRejectionReason, message.rejectionMessage.data(), iCopyLength);
		mpcRejectionReason[iCopyLength] = '\0';
		LOG(kNetwork, kWarning, "Client::ServerConnectionResponse Rejected: {}", mpcRejectionReason);
	}
}

void Client::ServerSubscribeAccept(std::span<const uint8_t> packetData)
{
	NetworkMessages::ServerSubscribeAcceptMessage message {};
	NetworkMessages::Read(packetData, message);
	if (message.uiLoadGeneration != muiCommittedLoadGeneration)
	{
		LOG(kNetwork, kWarning, "Client::ServerSubscribeAccept Dropped mismatched load generation Packet: {} Current: {}", message.uiLoadGeneration, muiCommittedLoadGeneration);
		return;
	}

	uint8_t uiSlotIndex = message.uiSlotIndex;
	uint16_t uiEpoch = message.uiEpoch;
	GridCoord coord = message.coord;

	if (uiSlotIndex == kuiSubscribeRejectSlot)
	{
		// Server rejected subscription (not adjacent / no free slot)
		mSubscriptions.mSubscribeRequests.TakeAnswer(coord);
		LOG(kNetwork, kWarning, "Client::ServerSubscribeAccept Rejected Coord: ({},{})", coord.iX, coord.iY);
		return;
	}

	// Defensive (trust boundary: network input): a non-sentinel slot the client cannot host would
	// throw at mCoordinateSlots.at() below. Unsubscribe so a server-side slot cannot leak, then drop.
	if (uiSlotIndex >= std::ssize(mSubscriptions.mCoordinateSlots))
	{
		LOG(kNetwork, kWarning, "Client::ServerSubscribeAccept Out-of-range, unsubscribing Slot: {} Coord: ({},{})", uiSlotIndex, coord.iX, coord.iY);
		common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
		common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
		NetworkMessages::ClientUnsubscribeMessage unsubscribe {.uiSlotIndex = uiSlotIndex, .uiEpoch = uiEpoch};
		NetworkMessages::Write(rWorkbuffer, unsubscribe);
		if (!ClientNetworkFixtures::ObserveSubscribeAcceptCleanup(*this, unsubscribe.uiSlotIndex, std::ssize(rWorkbuffer.View())))
		{
			NetworkManager::SendPacket(mpServerPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
		}
		return;
	}

	// This accept answers the coord's oldest request; a cancelled or missing one makes a heal or commit unsubscribe
	bool bLive = mSubscriptions.mSubscribeRequests.TakeAnswer(coord);
	ClientCoordSlot& rSlot = mSubscriptions.mCoordinateSlots.at(uiSlotIndex);
	ClientSubscriptions::SubscribeAcceptFlags_t actions = mSubscriptions.ClassifySubscribeAccept(uiSlotIndex, uiEpoch, coord);

	if (actions & ClientSubscriptions::SubscribeAcceptFlags::kRejectGhost)
	{
		LOG(kNetwork, kVerbose, "Client::ServerSubscribeAccept Ignoring Slot: {} Coord: ({},{}) SlotCoord: ({},{}) State: {}", uiSlotIndex, coord.iX, coord.iY, rSlot.coordinate.iX, rSlot.coordinate.iY, static_cast<int>(rSlot.eState));
		common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
		common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
		NetworkMessages::ClientUnsubscribeMessage unsubscribe {.uiSlotIndex = uiSlotIndex, .uiEpoch = uiEpoch};
		NetworkMessages::Write(rWorkbuffer, unsubscribe);
		NetworkManager::SendPacket(mpServerPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
		LOG(kNetwork, kVerbose, "Client::ServerSubscribeAccept sent unsubscribe for ghost Slot: {} Coord: ({},{})", uiSlotIndex, coord.iX, coord.iY);
		return;
	}

	if (actions & ClientSubscriptions::SubscribeAcceptFlags::kHealEpoch)
	{
		rSlot.acknowledgementState.uiEpoch = uiEpoch;
		if (!bLive)
		{
			LOG(kNetwork, kVerbose, "Client::ServerSubscribeAccept Healed then cancelled Slot: {} Coord: ({},{})", uiSlotIndex, coord.iX, coord.iY);
			SendUnsubscribe(uiSlotIndex);
		}
		else
		{
			LOG(kNetwork, kVerbose, "Client::ServerSubscribeAccept Healed active slot Slot: {} Coord: ({},{}) Epoch: {}", uiSlotIndex, coord.iX, coord.iY, uiEpoch);
		}
		return;
	}

	if (actions & ClientSubscriptions::SubscribeAcceptFlags::kCommitInitialization)
	{
		rSlot.coordinate = coord;
		rSlot.eState = CoordSubscriptionState::kWaitingFullState;
		rSlot.transitionStartTime = std::chrono::steady_clock::now();
		rSlot.acknowledgementState.iAcknowledgmentFloor = -1;
		rSlot.acknowledgementState.uiReceivedBitfieldLow = 0;
		rSlot.acknowledgementState.uiReceivedBitfieldHigh = 0;
		rSlot.acknowledgementState.uiEpoch = uiEpoch;

		if (!bLive)
		{
			LOG(kNetwork, kDebug, "Client::ServerSubscribeAccept Cancelled Slot: {} Coord: ({},{})", uiSlotIndex, coord.iX, coord.iY);
			SendUnsubscribe(uiSlotIndex);
		}
	}
}

void Client::ServerUnsubscribeAcknowledgement(std::span<const uint8_t> packetData)
{
	NetworkMessages::ServerUnsubscribeAckMessage message {};
	NetworkMessages::Read(packetData, message);

	uint8_t uiSlotIndex = message.uiSlotIndex;

	ClientCoordSlot& rSlot = mSubscriptions.mCoordinateSlots.at(uiSlotIndex);
	if (rSlot.eState != CoordSubscriptionState::kUnsubscribing)
	{
		return;
	}

	mSubscriptions.FreeSlot(uiSlotIndex);
	ClientNetworkFixtures::ObserveUnsubscribeAck(*this, uiSlotIndex);
}

void Client::ServerLoadNotification(std::span<const uint8_t> packetData)
{
	NetworkMessages::ServerLoadNotificationMessage message {};
	NetworkMessages::Read(packetData, message);
	if (message.uiLoadGeneration <= muiCommittedLoadGeneration)
	{
		return;
	}

	if (!muiPendingLoadGeneration.has_value() || message.uiLoadGeneration > *muiPendingLoadGeneration)
	{
		muiPendingLoadGeneration = message.uiLoadGeneration;
	}

	mReceivedGamePackets.clear();
	mStateFlags.Set(ClientStateFlags::kLoadNotificationReceived);
}

void Client::ServerTimespeedUpdate(std::span<const uint8_t> packetData)
{
	// The legitimate server sends its connection response before any timespeed update, so anything
	// arriving earlier is unsolicited and must not reach the time step
	if (!(mStateFlags & ClientStateFlags::kConnectionAccepted))
	{
		return;
	}

	NetworkMessages::ServerTimespeedUpdateMessage message {};
	NetworkMessages::Read(packetData, message);

	LOG(kNetwork, kDebug, "Client::ServerTimespeedUpdate Multiply: {} Divide: {}", message.iMultiply, message.iDivide);
	game::gpGame->mTimeStep.SetTimeScale(message.iMultiply, message.iDivide);
}

bool Client::SendAcknowledgement()
{
	if (!(mStateFlags & ClientStateFlags::kConnected) || mpServerPeer == nullptr)
	{
		return false;
	}

	// ACKs are sent at most once per simulation-tick interval (1/kiTickRate seconds), independently of render rate.
	// They continue while simulation is paused or stalled so server resends and RTT remain active.
	static constexpr std::chrono::nanoseconds kAckInterval(1'000'000'000 / kiTickRate);
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	if (now - mLastAcknowledgementSendTime < kAckInterval)
	{
		return false;
	}
	mLastAcknowledgementSendTime = now;

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();

	// Per-slot ACK state for proactive re-sends
	NetworkMessages::AckStreamEntry pEntries[NetworkManager::kiMaximumEnetCoordinateSlots] {};
	uint8_t uiAckSlotCount = 0;
	for (int64_t i = 0; i < std::ssize(mSubscriptions.mCoordinateSlots); ++i)
	{
		if (mSubscriptions.mCoordinateSlots.at(i).eState == CoordSubscriptionState::kActive)
		{
			pEntries[uiAckSlotCount] =
			{
				.uiSlotIndex = static_cast<uint8_t>(i),
				.uiEpoch = mSubscriptions.mCoordinateSlots.at(i).acknowledgementState.uiEpoch,
				.iAckFloor = mSubscriptions.mCoordinateSlots.at(i).acknowledgementState.iAcknowledgmentFloor,
				.uiReceivedBitfieldLow = mSubscriptions.mCoordinateSlots.at(i).acknowledgementState.uiReceivedBitfieldLow,
				.uiReceivedBitfieldHigh = mSubscriptions.mCoordinateSlots.at(i).acknowledgementState.uiReceivedBitfieldHigh,
			};
			++uiAckSlotCount;
		}
	}

	// Pipeline RTT: embed client timestamp for server to echo back
	int64_t iTimestampNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	NetworkMessages::ClientAckStreamMessage message
	{
		.uiSlotCount = uiAckSlotCount,
		.pEntries = pEntries,
		.iEntryCapacity = NetworkManager::kiMaximumEnetCoordinateSlots,
		.iTimestampNanoseconds = iTimestampNanoseconds,
	};
	NetworkMessages::Write(rWorkbuffer, message);

	NetworkManager::SendPacket(mpServerPeer, NetworkManager::kuiChannelUnreliable, rWorkbuffer, 0);
	return true;
}

void Client::SendDesynchronizationReport(int64_t iTick, GridCoord coordinate, common::crc_t uiExpectedCrc, common::crc_t uiActualCrc)
{
	if (!(mStateFlags & ClientStateFlags::kConnected) || mpServerPeer == nullptr)
	{
		return;
	}

	char pcExpected[20] {};
	char pcActual[20] {};
	LOG(kNetwork, kError, "Client::SendDesyncReport Frame: {} Grid: ({},{}) Expected: {} Actual: {}", iTick, coordinate.iX, coordinate.iY, common::ToHex(std::span(pcExpected), uiExpectedCrc), common::ToHex(std::span(pcActual), uiActualCrc));

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	NetworkMessages::ClientDesyncReportMessage message
	{
		.iTick = iTick,
		.coord = coordinate,
		.uiExpectedCrc = static_cast<uint64_t>(uiExpectedCrc),
		.uiActualCrc = static_cast<uint64_t>(uiActualCrc),
	};
	NetworkMessages::Write(rWorkbuffer, message);
	NetworkManager::SendPacket(mpServerPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void Client::SendDebugFrameRequest(int64_t iTick, GridCoord coordinate)
{
	if (!(mStateFlags & ClientStateFlags::kConnected) || mpServerPeer == nullptr)
	{
		return;
	}

	LOG(kNetwork, kError, "Client::SendDebugFrameRequest Frame: {} Grid: ({},{})", iTick, coordinate.iX, coordinate.iY);

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	NetworkMessages::ClientDebugFrameRequestMessage message {.iTick = iTick, .coord = coordinate};
	NetworkMessages::Write(rWorkbuffer, message);
	NetworkManager::SendPacket(mpServerPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

bool Client::SendSubscribe(GridCoord coordinate)
{
	if (!(mStateFlags & ClientStateFlags::kConnected))
	{
		return false;
	}
	if (!(mStateFlags & ClientStateFlags::kConnectionAccepted))
	{
		return false;
	}
	if (mpServerPeer == nullptr)
	{
		return false;
	}

	// The server picks the slot; the client needs a free slot for each unanswered subscribe request, cancelled ones
	// included, so the unanswered count stays within the slot count and under the server's per-update subscribe cap
	int64_t iFreeSlots = std::ranges::count_if(mSubscriptions.mCoordinateSlots, [](const ClientCoordSlot& rSlot)
	{
		return rSlot.eState == CoordSubscriptionState::kUnsubscribed || rSlot.eState == CoordSubscriptionState::kUnsubscribing;
	});
	if (iFreeSlots <= std::ssize(mSubscriptions.mSubscribeRequests.mRecords))
	{
		return false;
	}
	mSubscriptions.mSubscribeRequests.Add(coordinate);

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	NetworkMessages::ClientSubscribeMessage message {.uiLoadGeneration = muiCommittedLoadGeneration, .coord = coordinate};
	NetworkMessages::Write(rWorkbuffer, message);
	NetworkManager::SendPacket(mpServerPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);

	return true;
}

void Client::SendUnsubscribe(int64_t iSlot)
{
	if (!(mStateFlags & ClientStateFlags::kConnected) || mpServerPeer == nullptr)
	{
		return;
	}

	ClientCoordSlot& rSlot = mSubscriptions.mCoordinateSlots.at(iSlot);
	rSlot.eState = CoordSubscriptionState::kUnsubscribing;
	rSlot.transitionStartTime = std::chrono::steady_clock::now();
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	NetworkMessages::ClientUnsubscribeMessage message
	{
		.uiSlotIndex = static_cast<uint8_t>(iSlot),
		.uiEpoch = rSlot.acknowledgementState.uiEpoch,
	};
	NetworkMessages::Write(rWorkbuffer, message);
	NetworkManager::SendPacket(mpServerPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void Client::SendResynchronizationRequest()
{
	if (!(mStateFlags & ClientStateFlags::kConnected) || mpServerPeer == nullptr)
	{
		return;
	}

	LOG(kNetwork, kError, "Client::SendResyncRequest");

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	NetworkMessages::ClientResyncRequestMessage message {};
	NetworkMessages::Write(rWorkbuffer, message);
	NetworkManager::SendPacket(mpServerPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void Client::SendHello()
{
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();

	NetworkMessages::ClientHelloMessage message
	{
		.uiProtocolVersion = kuiProtocolVersion,
		.iFrameVersion = game::NetworkSessionContract::Frame::kiVersion,
		.uiPackIntegrityToken = gpFileManager->mpPackChunks->mPackIntegrityToken,
		.buildConfiguration = kpcBuildConfigurationName,
		.guid = mClientGuid,
		.bHasGuid = true,
	};
	NetworkMessages::Write(rWorkbuffer, message);

	miHelloSendTimeNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

	NetworkManager::SendPacket(mpServerPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

} // namespace engine

#endif // BT_CLIENT
