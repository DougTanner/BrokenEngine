#include "Pch.h"

#if defined(BT_SERVER)

#include "Network/Server/ServerBufferedFrames.h"

#include "Frame/FrameStaticData.h"
#include "Network/Server/Server.h"
#include "Network/NetworkCursor.h"

#include "Game.h"

namespace engine
{

// Full-state adoption freezes the client's floor at the adopted tick; repeated flushes tolerate ACK loss.
// Client::TrackReceivedTick disconnects when received ticks exceed that floor by more than kiNetworkBufferSize.
// Twice that window covers full-state delivery and the ACK trip while bounding relaxation of the monotonic floor guard.
constexpr int64_t kiPendingFullStateWindowTicks = 2 * kiNetworkBufferSize;

ServerBufferedFrames::ServerBufferedFrames(Server& rServer)
:	mrServer(rServer)
{
	ScopedSuppressAllocationTracking suppress;
	// Heap: one-time compression scratch buffer, sized so any valid capped StatusChange batch always fits
	// (CompressToBuffer grows it further on demand for full debug frames)
	mCompressionBuffer.resize(kiMaxCompressedStatusChangeBatchBytes);
}

void ServerBufferedFrames::BufferFrame(int64_t iTick, std::span<const std::pair<GridCoord, GridUpdateData>> gridUpdates)
{
	miLatestBufferedTick = iTick;
	ScopedSuppressAllocationTracking suppress;

	std::unordered_set<GridCoord> activeCoordinates;
	activeCoordinates.reserve(gridUpdates.size());

	for (const std::pair<GridCoord, GridUpdateData>& rGridUpdate : gridUpdates)
	{
		const auto& [rCoordinate, rUpdateData] = rGridUpdate;
		activeCoordinates.insert(rCoordinate);

		// Heap: per-coord ring buffer grows until steady state
		PerCoordBufferedFrame buffered {};
		buffered.iTick = iTick;
		buffered.uiSharedCrc = rUpdateData.uiSharedCrc;

		if (!rUpdateData.statusChanges.empty())
		{
			int64_t iStatusChangeCount = std::ssize(rUpdateData.statusChanges);
			if (iStatusChangeCount > kiMaximumStatusChangesPerCell)
			{
				// Should never happen: the sim must not exceed the protocol's per-cell cap (also the client decode
				// scratch size and the compression-scratch sizing basis). Alert in debug, then drop the payload rather
				// than overflow the scratch. The frame is still buffered (ring contiguity) with its uiSharedCrc, so the
				// client CRC-mismatches and resyncs instead of applying a truncated batch.
				DEBUG_BREAK();
				LOG(kNetwork, kError, "Server::BufferFrame status change count {} exceeds cap {}, dropping payload Coord: ({},{}) Frame: {}", iStatusChangeCount, kiMaximumStatusChangesPerCell, rCoordinate.iX, rCoordinate.iY, iTick);
			}
			else
			{
				int64_t iCompressedSize = game::NetworkSessionContract::CompressStatusChanges(std::span<const game::StatusChange>(rUpdateData.statusChanges.data(), static_cast<size_t>(iStatusChangeCount)), std::span<uint8_t>(mCompressionBuffer));
				if (iCompressedSize > 0)
				{
					buffered.compressedData.assign(mCompressionBuffer.begin(), mCompressionBuffer.begin() + iCompressedSize);
				}
				else
				{
					// Compression failed despite the sized scratch — drop the payload (logged kError by the codec)
					// rather than buffer an empty prefix, which the client's status-change decoder rejects as a corrupt
					// payload — a fatal response to the server's own encoding failure.
					LOG(kNetwork, kError, "Server::BufferFrame compression failed, dropping payload Coord: ({},{}) Frame: {} Count: {}", rCoordinate.iX, rCoordinate.iY, iTick, iStatusChangeCount);
				}
			}
		}

		std::deque<PerCoordBufferedFrame>& rCoordinateBuffer = mPerCoordinateBufferedFrames.try_emplace(rCoordinate).first->second;
		rCoordinateBuffer.push_back(std::move(buffered));
		while (static_cast<int64_t>(rCoordinateBuffer.size()) > kiMaximumBufferedFrames)
		{
			rCoordinateBuffer.pop_front();
		}
	}

	// Prune ring buffers for coords no longer in the active set
	std::erase_if(mPerCoordinateBufferedFrames, [&activeCoordinates](const std::pair<const GridCoord, std::deque<PerCoordBufferedFrame>>& rEntry)
	{
		return !activeCoordinates.contains(rEntry.first);
	});
}

void ServerBufferedFrames::BufferFullFrame(int64_t iTick, std::span<const std::pair<GridCoord, const game::Frame*>> frames)
{
	ScopedSuppressAllocationTracking suppress;

	// Evict oldest entries first, recycling their per-coord string storage into the pool so the build
	// loop below serializes into reused capacity (no large allocation once at steady state).
	// Heap: ring/pool grow until steady state
	while (static_cast<int64_t>(mBufferedFullFrames.size()) >= kiMaximumBufferedFrames)
	{
		for (auto& [rCoordinate, rSerialized] : mBufferedFullFrames.front().serializedFrames)
		{
			mFullFramePool.push_back(std::move(rSerialized));
		}
		mBufferedFullFrames.pop_front();
	}

	BufferedFullFrame buffered {};
	buffered.iTick = iTick;

	for (const std::pair<GridCoord, const game::Frame*>& rFrame : frames)
	{
		std::string serialized;
		if (!mFullFramePool.empty())
		{
			serialized = std::move(mFullFramePool.back());
			mFullFramePool.pop_back();
		}
		serialized.clear();

		mFrameStreamBuffer.mpTarget = &serialized;
		game::NetworkSessionContract::WriteFrame(mFrameStream, *rFrame.second);

		buffered.serializedFrames.insert_or_assign(rFrame.first, std::move(serialized));
	}

	mBufferedFullFrames.push_back(std::move(buffered));
}

void ServerBufferedFrames::ClearBufferedFrames()
{
	mPerCoordinateBufferedFrames.clear();
	mBufferedFullFrames.clear();
	miLatestBufferedTick = -1;
}

const PerCoordBufferedFrame* ServerBufferedFrames::FindBufferedFrame(GridCoord coordinate, int64_t iTick) const
{
	auto it = mPerCoordinateBufferedFrames.find(coordinate);
	if (it == mPerCoordinateBufferedFrames.end())
	{
		return nullptr;
	}
	const std::deque<PerCoordBufferedFrame>& rCoordinateBuffer = it->second;
	if (rCoordinateBuffer.empty())
	{
		return nullptr;
	}
	int64_t iIndex = iTick - rCoordinateBuffer.front().iTick;
	if (iIndex < 0 || iIndex >= std::ssize(rCoordinateBuffer))
	{
		return nullptr;
	}
	return &rCoordinateBuffer.at(static_cast<size_t>(iIndex));
}

int64_t ServerBufferedFrames::CompressToBuffer(std::span<const char> data)
{
	int64_t iSize = std::ssize(data);
	int64_t iMaxCompressed = LZ4_compressBound(static_cast<int>(iSize));
	if (std::ssize(mCompressionBuffer) < iMaxCompressed)
	{
		mCompressionBuffer.resize(static_cast<size_t>(iMaxCompressed));
	}
	return LZ4_compress_default(data.data(), reinterpret_cast<char*>(mCompressionBuffer.data()), static_cast<int>(iSize), static_cast<int>(iMaxCompressed));
}

void ServerBufferedFrames::SendCoordinateFullState(int64_t iClientIdentifier, int64_t iSlot, int64_t iTick, GridCoord coordinate, const game::Frame* pFrame)
{
	ClientConnection* pClient = mrServer.FindClient(iClientIdentifier);
	if (pClient == nullptr)
	{
		return;
	}

	// Adopting this full state makes iTick the client's ACK floor, which moves that floor backward whenever the
	// client had already received later ticks. Record the tick so ServerBufferedFrames::ApplyAckStream admits that one
	// regression; without it the monotonic floor guard rejects every later ack for this slot, the ticks the
	// client discarded on adoption are never resent, and the client self-disconnects on its frozen floor.
	// Pre-adoption ACKs keep raising the server's floor until the reliable full state arrives.
	ClientConnection::SlotState& rSlot = pClient->slots.at(iSlot);
	LOG(kNetwork, kDebug, "Server::SendCoordFullState Client: {} Frame: {} Slot: {} Coord: ({},{}) AckFloor: {}", iClientIdentifier, iTick, iSlot, coordinate.iX, coordinate.iY, rSlot.ack.iAcknowledgmentFloor);
	rSlot.iPendingFullStateTick = iTick;

	// Serialize frame into the reusable scratch (server main thread only - single-writer contract)
	ScopedSuppressAllocationTracking suppress;
	// Heap: scratch grows until steady state
	mSendScratch.clear();
	mFrameStreamBuffer.mpTarget = &mSendScratch;
	game::NetworkSessionContract::WriteFrame(mFrameStream, *pFrame);

	// LZ4 compress
	int64_t iCompressedSize = CompressToBuffer(std::span<const char>(mSendScratch.data(), mSendScratch.size()));

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();

	NetworkMessages::ServerCoordFullStateMessage message
	{
		.uiLoadGeneration = static_cast<uint8_t>(mrServer.miLoadGeneration),
		.uiSlotIndex = static_cast<uint8_t>(iSlot),
		.uiEpoch = rSlot.ack.uiEpoch,
		.iTick = iTick,
		.coord = coordinate,
		.iUncompressedSize = static_cast<int32_t>(mSendScratch.size()),
		.compressedPayload = {.puiData = mCompressionBuffer.data(), .iSize = static_cast<int32_t>(iCompressedSize)},
	};
	NetworkMessages::Write(rWorkbuffer, message);

	NetworkManager::SendPacket(pClient->pPeer, NetworkManager::CoordinateSlotReliable(iSlot), rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void ServerBufferedFrames::SendCoordinateStaticData(int64_t iClientIdentifier, int64_t iSlot, GridCoord coordinate, const FrameStaticData& rStaticData)
{
	ClientConnection* pClient = mrServer.FindClient(iClientIdentifier);
	if (pClient == nullptr)
	{
		return;
	}

	LOG(kNetwork, kDebug, "Server::SendCoordStaticData Client: {} Slot: {} Coord: ({},{})", iClientIdentifier, iSlot, coordinate.iX, coordinate.iY);

	// Serialize static data into the reusable scratch (server main thread only - single-writer contract)
	ScopedSuppressAllocationTracking suppress;
	// Heap: scratch grows until steady state
	mSendScratch.clear();
	mFrameStreamBuffer.mpTarget = &mSendScratch;
	rStaticData.Write(mFrameStream, /*bIncludeNavigationData=*/true);

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();

	NetworkMessages::ServerCoordStaticDataMessage message
	{
		.uiLoadGeneration = static_cast<uint8_t>(mrServer.miLoadGeneration),
		.uiSlotIndex = static_cast<uint8_t>(iSlot),
		.uiEpoch = pClient->slots.at(iSlot).ack.uiEpoch,
		.coord = coordinate,
		.staticData = {.puiData = reinterpret_cast<const uint8_t*>(mSendScratch.data()), .iSize = static_cast<int32_t>(mSendScratch.size())},
	};
	NetworkMessages::Write(rWorkbuffer, message);

	NetworkManager::SendPacket(pClient->pPeer, NetworkManager::CoordinateSlotReliable(iSlot), rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void ServerBufferedFrames::WriteBufferedFramePacket(common::Workbuffer& rWorkbuffer, PacketType eType, int64_t iSlot, int64_t iEpoch, const PerCoordBufferedFrame& rBuffered, int64_t iTimestampNanoseconds)
{
	NetworkMessages::CoordUpdateFields fields
	{
		.uiLoadGeneration = static_cast<uint8_t>(mrServer.miLoadGeneration),
		.uiSlotIndex = static_cast<uint8_t>(iSlot),
		.uiEpoch = static_cast<uint16_t>(iEpoch),
		.iTick = rBuffered.iTick,
		.iEchoedTimestampNanoseconds = iTimestampNanoseconds,
		.uiSharedCrc = rBuffered.uiSharedCrc,
		.compressedPayload = {.puiData = rBuffered.compressedData.data(), .iSize = static_cast<int32_t>(std::ssize(rBuffered.compressedData))},
	};
	if (eType == PacketType::kServerCoordinateUpdate)
	{
		NetworkMessages::ServerCoordUpdateMessage message {fields};
		NetworkMessages::Write(rWorkbuffer, message);
	}
	else if (eType == PacketType::kServerCoordinateResend)
	{
		NetworkMessages::ServerCoordResendMessage message {fields};
		NetworkMessages::Write(rWorkbuffer, message);
	}
}

void ServerBufferedFrames::SendUpdate(ClientConnection& rClient, int64_t iTick)
{
	// Heap: workbuffer Push (per-slot scope) and ENet packet creation in SendPacket
	ScopedSuppressAllocationTracking suppress;

	for (int64_t i = 0; i < std::ssize(rClient.slots); ++i)
	{
		ClientConnection::SlotState& rSlot = rClient.slots.at(i);
		if (!(rSlot.subscription.flags & SubscriptionFlags::kActive))
		{
			continue;
		}

		GridCoord coordinate = rSlot.subscription.coordinate;

		const PerCoordBufferedFrame* pBuffered = FindBufferedFrame(coordinate, iTick);
		if (pBuffered == nullptr)
		{
			continue;
		}

		bool bRestartedStream = mPerCoordinateBufferedFrames.at(coordinate).front().iTick == iTick && rSlot.ack.iAcknowledgmentFloor >= 0
		                     && iTick > rSlot.ack.iAcknowledgmentFloor + 1;
		if (bRestartedStream)
		{
			rSlot.bHoldUpdatesUntilFullStateAck = true;
			if (rSlot.iPendingFullStateTick != iTick)
			{
				SendCoordinateFullState(rClient.iClientId, i, iTick, coordinate, &(*game::gpGame->mCoordinateFrames.at(coordinate).pCurrent));
			}
		}
		if (rSlot.bHoldUpdatesUntilFullStateAck)
		{
			continue;
		}

		common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
		common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();

		WriteBufferedFramePacket(rWorkbuffer, PacketType::kServerCoordinateUpdate, i, rSlot.ack.uiEpoch, *pBuffered, rClient.iClientTimestampNanoseconds);

		NetworkManager::SendPacket(rClient.pPeer, NetworkManager::CoordinateSlotUnreliable(i), rWorkbuffer, 0);
		if (!(rSlot.subscription.flags & SubscriptionFlags::kFirstUpdateLogged))
		{
			LOG(kNetwork, kVerbose, "Server::SendUpdate Client: {} Slot: {} Coord: ({},{}) Tick: {}", rClient.iClientId, i, coordinate.iX, coordinate.iY, iTick);
			rSlot.subscription.flags.Set(SubscriptionFlags::kFirstUpdateLogged);
		}
	}
}

void ServerBufferedFrames::SendResends(ClientConnection& rClient, int64_t iTick)
{
	// Heap: workbuffer Push and ENet packet creation per resend frame
	ScopedSuppressAllocationTracking suppress;

	// Iterate per-slot: each active subscription has its own ACK state and coord ring buffer
	for (int64_t i = 0; i < std::ssize(rClient.slots); ++i)
	{
		ClientConnection::SlotState& rSlot = rClient.slots.at(i);
		GridCoord coordinate = rSlot.subscription.coordinate;

		if (!(rSlot.subscription.flags & SubscriptionFlags::kActive))
		{
			UpdateResendLogState(rClient, i, 0, coordinate);
			continue;
		}
		if (rSlot.bHoldUpdatesUntilFullStateAck)
		{
			UpdateResendLogState(rClient, i, 0, coordinate);
			continue;
		}

		const AckState& rAcknowledgmentState = rSlot.ack;
		if (rAcknowledgmentState.iAcknowledgmentFloor < 0)
		{
			UpdateResendLogState(rClient, i, 0, coordinate);
			continue;
		}

		int64_t iAcknowledgmentGap = iTick - rAcknowledgmentState.iAcknowledgmentFloor;
		if (iAcknowledgmentGap > 64)
		{
			LOG(kNetwork, kVerbose, "Server::SendResends Client: {} Slot: {} Coord: ({},{}) AckFloor: {} CurrentTick: {} Gap: {}", rClient.iClientId, i, coordinate.iX, coordinate.iY, rAcknowledgmentState.iAcknowledgmentFloor, iTick, iAcknowledgmentGap);
		}

		if (rAcknowledgmentState.uiReceivedBitfieldLow == 0 && rAcknowledgmentState.uiReceivedBitfieldHigh == 0)
		{
			UpdateResendLogState(rClient, i, 0, coordinate);
			continue;
		}
		int64_t iScanLimit = (rAcknowledgmentState.uiReceivedBitfieldHigh != 0)
			? std::max(static_cast<int64_t>(std::bit_width(rAcknowledgmentState.uiReceivedBitfieldLow)), 64 + static_cast<int64_t>(std::bit_width(rAcknowledgmentState.uiReceivedBitfieldHigh)))
			: static_cast<int64_t>(std::bit_width(rAcknowledgmentState.uiReceivedBitfieldLow));

		int64_t iSlotResendCount = 0;
		for (int64_t j = 0; j < iScanLimit && iSlotResendCount < kiMaximumResendFrames; ++j)
		{
			bool bReceived = (j < 64) ? (rAcknowledgmentState.uiReceivedBitfieldLow & (1ui64 << j)) != 0 : (rAcknowledgmentState.uiReceivedBitfieldHigh & (1ui64 << (j - 64))) != 0;
			if (bReceived)
			{
				continue;
			}

			int64_t iMissingFrame = rAcknowledgmentState.iAcknowledgmentFloor + 1 + j;
			if (iMissingFrame >= iTick)
			{
				break;
			}

			const PerCoordBufferedFrame* pBuffered = FindBufferedFrame(coordinate, iMissingFrame);
			if (pBuffered == nullptr)
			{
				LOG(kNetwork, kVerbose, "Server::SendResends Evicted frame Client: {} Slot: {} Coord: ({},{}) MissingTick: {} LatestBuffered: {}", rClient.iClientId, i, coordinate.iX, coordinate.iY, iMissingFrame, miLatestBufferedTick);
				continue;
			}

			common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
			common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();

			WriteBufferedFramePacket(rWorkbuffer, PacketType::kServerCoordinateResend, i, rSlot.ack.uiEpoch, *pBuffered, rClient.iClientTimestampNanoseconds);

			NetworkManager::SendPacket(rClient.pPeer, NetworkManager::CoordinateSlotUnreliable(i), rWorkbuffer, 0);

			++iSlotResendCount;
		}

		UpdateResendLogState(rClient, i, iSlotResendCount, coordinate);
	}
}

void ServerBufferedFrames::UpdateResendLogState(ClientConnection& rClient, int64_t iSlot, int64_t iSlotResendCount, GridCoord coordinate)
{
	static constexpr int64_t kiResendLogCooldownTicks = 64;

	bool bWasResending = rClient.slots.at(iSlot).iPreviousResendCount > 0;
	bool bIsResending = iSlotResendCount > 0;

	if (rClient.slots.at(iSlot).iResendLogCooldown > 0)
	{
		--rClient.slots.at(iSlot).iResendLogCooldown;
	}

	if (bWasResending != bIsResending && rClient.slots.at(iSlot).iResendLogCooldown <= 0)
	{
		LOG(kNetwork, kVerbose, "Server::SendResends Client: {} Slot: {} Coord: ({},{}) Count: {}", rClient.iClientId, iSlot, coordinate.iX, coordinate.iY, iSlotResendCount);
		rClient.slots.at(iSlot).iResendLogCooldown = kiResendLogCooldownTicks;
	}
	rClient.slots.at(iSlot).iPreviousResendCount = iSlotResendCount;
}

void ServerBufferedFrames::ApplyAckStream(ClientConnection& rClient, const NetworkMessages::ClientAckStreamMessage& rMessage, int64_t iClientId)
{
	int64_t iFloorAdvanceCount = 0;
	for (int64_t i = 0; i < rMessage.uiSlotCount; ++i)
	{
		const NetworkMessages::AckStreamEntry& rEntry = rMessage.pEntries[i];
		int64_t iSlotIndex = rEntry.uiSlotIndex;
		int64_t iSlotEpoch = rEntry.uiEpoch;
		int64_t iSlotAcknowledgementFloor = rEntry.iAckFloor;
		uint64_t uiSlotBitfieldLow = rEntry.uiReceivedBitfieldLow;
		uint64_t uiSlotBitfieldHigh = rEntry.uiReceivedBitfieldHigh;

		if (!(iSlotIndex < std::ssize(rClient.slots) && (rClient.slots.at(static_cast<size_t>(iSlotIndex)).subscription.flags & SubscriptionFlags::kActive)))
		{
			continue;
		}

		ClientConnection::SlotState& rSlot = rClient.slots.at(static_cast<size_t>(iSlotIndex));

		// Adopting a full state re-baselines the client's floor to that full state's tick, so the floor legitimately
		// moves backward when the client had already received later ticks. Admit exactly that regression, never below
		// the tick whose authoritative state the client now holds. The acks the client sent before it adopted still
		// carry higher floors and satisfy the strict guard, so they cannot consume a backward target. A held restart
		// target is consumed when an accepted ACK crosses it; a backward target is consumed by the bounded regression.
		bool bFullStateRebaseline = rSlot.iPendingFullStateTick >= 0 && miLatestBufferedTick - rSlot.iPendingFullStateTick <= kiPendingFullStateWindowTicks && iSlotAcknowledgementFloor < rSlot.ack.iAcknowledgmentFloor && iSlotAcknowledgementFloor >= rSlot.iPendingFullStateTick;

		if (iSlotEpoch == rSlot.ack.uiEpoch && (iSlotAcknowledgementFloor >= rSlot.ack.iAcknowledgmentFloor || bFullStateRebaseline))
		{
			// Clamp to server's latest sent tick to prevent future ACK floors
			iSlotAcknowledgementFloor = std::min(iSlotAcknowledgementFloor, miLatestBufferedTick);
			AckState& rAcknowledgementState = rSlot.ack;
			bool bFullStateAcknowledgement = bFullStateRebaseline || (rSlot.bHoldUpdatesUntilFullStateAck && rSlot.iPendingFullStateTick >= 0 && rAcknowledgementState.iAcknowledgmentFloor < rSlot.iPendingFullStateTick && iSlotAcknowledgementFloor >= rSlot.iPendingFullStateTick);
			if (bFullStateRebaseline)
			{
				LOG(kNetwork, kDebug, "Server::ClientAckStream Full-state floor re-baseline Client: {} Slot: {} Floor: {} -> {} FullStateTick: {}", iClientId, iSlotIndex, rAcknowledgementState.iAcknowledgmentFloor, iSlotAcknowledgementFloor, rSlot.iPendingFullStateTick);
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
		else if (iSlotEpoch != rSlot.ack.uiEpoch)
		{
			LOG(kNetwork, kVerbose, "Server::ClientAckStream EpochMismatch Client: {} Slot: {} ClientEpoch: {} ServerEpoch: {}", iClientId, iSlotIndex, iSlotEpoch, rSlot.ack.uiEpoch);
		}
	}

	if (iFloorAdvanceCount > 0)
	{
		if (rClient.bFloorStalled)
		{
			if (rClient.iPeakConsecutiveStallAcks >= kiFloorStallLogThreshold)
			{
				LOG(kNetwork, kVerbose, "Server::ClientAckStream FloorStallResolved Client: {} PeakStalledAcks: {} Slots: {}", iClientId, rClient.iPeakConsecutiveStallAcks, rMessage.uiSlotCount);
			}
			rClient.bFloorStalled = false;
			rClient.iPeakConsecutiveStallAcks = 0;
		}
		rClient.iConsecutiveZeroAdvanceAcks = 0;
	}
	else if (rMessage.uiSlotCount > 0)
	{
		++rClient.iConsecutiveZeroAdvanceAcks;
		rClient.iPeakConsecutiveStallAcks = std::max(rClient.iPeakConsecutiveStallAcks, rClient.iConsecutiveZeroAdvanceAcks);
		if (rClient.iConsecutiveZeroAdvanceAcks >= 3)
		{
			rClient.bFloorStalled = true;
		}
	}

	// Pipeline RTT: store client timestamp for echo in SendUpdate (monotonically increasing to guard against out-of-order packets)
	int64_t iClientTimestampNanoseconds = rMessage.iTimestampNanoseconds;
	if (iClientTimestampNanoseconds > rClient.iClientTimestampNanoseconds)
	{
		rClient.iClientTimestampNanoseconds = iClientTimestampNanoseconds;
	}
}

void ServerBufferedFrames::SendDebugFrame(ENetPeer* pPeer, int64_t iTick, GridCoord coordinate)
{
	if constexpr (game::NetworkSessionContract::kbDebugFrames)
	{
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
		int64_t iCompressedSize = CompressToBuffer(std::span<const char>(rFrameData.data(), rFrameData.size()));

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

		NetworkManager::SendPacket(pPeer, NetworkManager::kiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
	}
}

} // namespace engine

#endif // BT_SERVER
