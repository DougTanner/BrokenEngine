#include "Pch.h"

#include "Network/Client/ReconcileReplay.h"

#if defined(BT_CLIENT)

#include "Frame/FrameBase.h"

#include "Frame/Frame.h"
#include "Frame/FrameInput.h"
#include "Frame/StatusChange.h"
#include "SpawnTransfer.h"

namespace engine
{

void ReconcileRollbackCoord(CoordWork& rWork, int64_t iRollbackOffset)
{
	engine::Cell& rCell = *rWork.pCell;
	CoordScratch& rScratch = rWork.scratch;

	ASSERT(iRollbackOffset >= 0);
	int64_t iRollbackPhysical = SnapshotIndex(rCell.iSnapshotHead, iRollbackOffset);
	rScratch.replayStack.clear();
	rScratch.replayStack.push_back(rCell.snapshots[iRollbackPhysical].get());
	rScratch.iReplayStackCount = 1;
	rScratch.iReplayWriteHead = SnapshotIndex(rCell.iSnapshotHead, iRollbackOffset + 1);
	rScratch.iReplayWriteCount = 0;
	rScratch.iLastValidatedIndex = -1;
}

int64_t ReconcileFindReplayRangeCoord(const CoordWork& rWork, int64_t iReplayStart)
{
	const engine::Cell& rCell = *rWork.pCell;

	int64_t iMaximumConsecutive = iReplayStart - 1;
	for (int64_t i = iReplayStart; ; ++i)
	{
		if (!rCell.serverUpdates.contains(i))
		{
			break;
		}
		iMaximumConsecutive = i;
	}
	return iMaximumConsecutive;
}

static void LogTransferSummary(const CoordWork& rWork, int64_t iTick, int64_t iTransferPlayerCount, int64_t iTransferBlasterCount, int64_t iTransferSpaceshipCount, int64_t iTransferMissileCount, std::span<const engine::GlobalId> transferPlayerIds)
{
	// NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores) — read only by the kVerbose LOGs below, which compile out at default log levels
	int64_t iTransferTotal = iTransferPlayerCount + iTransferBlasterCount + iTransferSpaceshipCount + iTransferMissileCount;
	if (iTransferPlayerCount > 0)
	{
		char acPlayerIds[192] {};
		int64_t iPosition = 0;
		for (int64_t i = 0; i < std::ssize(transferPlayerIds); ++i)
		{
			static constexpr int64_t kiReserve = 24;
			if (iPosition + kiReserve > std::ssize(acPlayerIds))
			{
				break;
			}
			if (i > 0)
			{
				acPlayerIds[iPosition++] = ',';
				acPlayerIds[iPosition++] = ' ';
			}
			int64_t iWritten = std::snprintf(acPlayerIds + iPosition, static_cast<size_t>(std::ssize(acPlayerIds) - iPosition), "%lld", transferPlayerIds[i].iValue);
			if (iWritten <= 0)
			{
				break;
			}
			iPosition += iWritten;
		}
		LOG(kNetwork, kVerbose, "ReconcileRunTickCoord SpawnTransfers Coord: ({},{}) ForTick: {} TransferCount: {} PlayerCount: {} BlasterCount: {} SpaceshipCount: {} MissileCount: {} PlayerIds: [{}]", rWork.coord.iX, rWork.coord.iY, iTick, iTransferTotal, iTransferPlayerCount, iTransferBlasterCount, iTransferSpaceshipCount, iTransferMissileCount, acPlayerIds);
	}
	else
	{
		LOG(kNetwork, kVerbose, "ReconcileRunTickCoord SpawnTransfers Coord: ({},{}) ForTick: {} TransferCount: {} PlayerCount: {} BlasterCount: {} SpaceshipCount: {} MissileCount: {}", rWork.coord.iX, rWork.coord.iY, iTick, iTransferTotal, iTransferPlayerCount, iTransferBlasterCount, iTransferSpaceshipCount, iTransferMissileCount);
	}
}

static bool ReconcileRunTickCoord(CoordWork& rWork, int64_t iTick, float fTime, game::FrameInput& rFrameInput, bool bIsReplay)
{
	engine::Cell& rCell = *rWork.pCell;
	CoordScratch& rScratch = rWork.scratch;

	// Invariant: any tick whose CRC matched the server must never be re-simulated.
	if (iTick <= rCell.iHighWaterValidatedTick)
	{
		DEBUG_BREAK();
	}

	// The slot before the write head holds the ring base the output layout keeps (rollback base,
	// injected full state, or fast-path tail), so writes must never wrap onto it.
	if (rScratch.iReplayWriteCount >= engine::kiNetworkBufferSize - 1)
	{
		LOG(kNetwork, kVerbose, "ReconcileRunTickCoord Ring buffer full WriteCount: {} ForTick: {}", rScratch.iReplayWriteCount, iTick);
		return false;
	}

	int64_t iNextSlot = SnapshotIndex(rScratch.iReplayWriteHead, rScratch.iReplayWriteCount);
	if (rCell.snapshots[iNextSlot] == nullptr)
	{
		rCell.snapshots[iNextSlot] = std::make_unique<game::Frame>();
	}

	game::Frame* pCurrent = rScratch.replayStack.at(rScratch.iReplayStackCount - 1);
	game::Frame* pNext = rCell.snapshots[iNextSlot].get();

	pNext->interpolate.frameFlags.Set(engine::FrameFlags::kRecalculated, iTick <= rScratch.iPreReconcileTailTick);

	engine::ActiveFrameReference activeFrameReference
	{
		.pNext = pNext,
		.pCurrent = pCurrent,
		.pFrameInput = &rFrameInput,
		.pStaticData = &rCell.staticData,
	};
	engine::RunFrameTick(activeFrameReference, iTick, fTime);

	// Apply transfer StatusChanges (runs after Destroy/Spawn to match server ordering)
	bool bHadTransfers = false;
	int64_t iTransferPlayerCount = 0;
	int64_t iTransferBlasterCount = 0;
	int64_t iTransferSpaceshipCount = 0;
	int64_t iTransferMissileCount = 0;
	engine::GlobalId transferPlayerIds[8] {};
	for (const game::StatusChange& rStatusChange : rFrameInput.statusChanges)
	{
		if (game::IsTransferType(rStatusChange.eType))
		{
			const game::TransferData& rData = std::get<game::TransferData>(rStatusChange.data);
			game::SpawnTransfer(*pNext, rStatusChange.eType, rData, pNext->postRender.playerAlignment);
			bHadTransfers = true;

			switch (rStatusChange.eType)
			{
				case game::StatusChangeType::kTransferPlayer:
					if (iTransferPlayerCount < 8)
					{
						transferPlayerIds[iTransferPlayerCount] = rData.globalPlayerId;
					}
					++iTransferPlayerCount;
					break;
				case game::StatusChangeType::kTransferBlaster:
					++iTransferBlasterCount;
					break;
				case game::StatusChangeType::kTransferSpaceship:
					++iTransferSpaceshipCount;
					break;
				case game::StatusChangeType::kTransferMissile:
					++iTransferMissileCount;
					break;
				default:
					break;
			}
		}
	}
	if (bHadTransfers && !bIsReplay && iTick > rWork.pCell->iLastSpawnTransferLogTick)
	{
		rWork.pCell->iLastSpawnTransferLogTick = iTick;
		LogTransferSummary(rWork, iTick, iTransferPlayerCount, iTransferBlasterCount, iTransferSpaceshipCount, iTransferMissileCount, std::span<const engine::GlobalId>(transferPlayerIds, static_cast<size_t>(std::min<int64_t>(iTransferPlayerCount, 8))));
	}
	std::erase_if(rFrameInput.statusChanges, [](const game::StatusChange& rStatusChange)
	{
		return game::IsTransferType(rStatusChange.eType);
	});

	if (bHadTransfers)
	{
		pNext->postRender.uiSharedCrc = pNext->Crc();
	}

	rScratch.replayStack.push_back(pNext);
	++rScratch.iReplayStackCount;
	++rScratch.iReplayWriteCount;

	return true;
}

static bool ReconcileValidateCrcCoord(CoordWork& rWork, int64_t iTick, const engine::Cell::CoordServerUpdate& rUpdate, const game::FrameInput& rFrameInput)
{
	engine::Cell& rCell = *rWork.pCell;
	CoordScratch& rScratch = rWork.scratch;

	game::Frame& rCurrentFrame = *rScratch.replayStack.at(rScratch.iReplayStackCount - 1);
	rCurrentFrame.interpolate.frameFlags.Set(engine::FrameFlags::kRecalculated, false);
	common::crc_t uiClientCrc = rCurrentFrame.postRender.uiSharedCrc;

	if (uiClientCrc != rUpdate.uiSharedCrc)
	{
		if (!(rScratch.flags & ReconcileScratchFlags::kSuppressRepeatLogs))
		{
			char acSharedCrc[20] {}, acClientCrc[20] {};
			common::ToHex(std::span<char, 20>(acSharedCrc), rUpdate.uiSharedCrc);
			common::ToHex(std::span<char, 20>(acClientCrc), uiClientCrc);
			LOG(kNetwork, kDebug, "ReconcileValidateCrcCoord Replay CRC mismatch; reconciliation outcome pending Coord: ({},{}) ForTick: {} ServerCrc: {} ClientCrc: {} ServerStatusChanges: {} ClientStatusChanges: {}", rWork.coord.iX, rWork.coord.iY, iTick, acSharedCrc, acClientCrc, std::ssize(rUpdate.statusChanges), std::ssize(rFrameInput.statusChanges));
		}

		rScratch.iDesyncTick = iTick;
		rScratch.desyncExpectedCrc = rUpdate.uiSharedCrc;
		rScratch.desyncActualCrc = uiClientCrc;
		if constexpr (kbDesynchronizationDebugFrames)
		{
			rScratch.pDesyncClientFrame = std::make_unique<game::Frame>();
			TransferViaStream(rCurrentFrame, *rScratch.pDesyncClientFrame);
		}
		return false;
	}

	rScratch.iLastValidatedIndex = rScratch.iReplayStackCount - 1;
	rScratch.iNewConfirmedTick = iTick;
	rCell.iHighWaterValidatedTick = std::max(rCell.iHighWaterValidatedTick, iTick);

	return true;
}

void ReconcileReplayCoord(CoordWork& rWork, int64_t iReplayStart, int64_t iMaximumConsecutive, float& rfTime)
{
	engine::Cell& rCell = *rWork.pCell;
	CoordScratch& rScratch = rWork.scratch;

	for (int64_t i = iReplayStart; i <= iMaximumConsecutive; ++i)
	{
		auto it = rCell.serverUpdates.find(i);
		if (it == rCell.serverUpdates.end())
		{
			break;
		}

		rfTime += engine::kfDeltaTime;

		if (i <= rScratch.iPreReconcileTailTick)
		{
			rScratch.flags.Set(ReconcileScratchFlags::kResimulationOccurred);
		}

		game::FrameInput frameInput;
		frameInput.statusChanges = it->second.statusChanges;

		if (!ReconcileRunTickCoord(rWork, i, rfTime, frameInput, true))
		{
			break;
		}

		// The replay range is target-capped, so a matching pending full state is necessarily due.
		if (rCell.pendingFullState.has_value() && rCell.pendingFullState->iTick == i)
		{
			ReconcileInjectPendingFullState(rWork);
			rfTime = rScratch.replayStack.at(0)->interpolate.fCurrentTime;
			LOG(kNetwork, kVerbose, "ReconcileReplayCoord Injected pending full state Coord: ({},{}) ForTick: {}", rWork.coord.iX, rWork.coord.iY, i);
		}

		if (!ReconcileValidateCrcCoord(rWork, i, it->second, frameInput))
		{
			return;
		}

		bool bHadStatusChanges = !it->second.statusChanges.empty();

		rCell.serverUpdates.erase(it);

		if (bHadStatusChanges)
		{
			++rScratch.profiling.iStatusChangeReplayTicks;
		}
		else
		{
			++rScratch.profiling.iKnockOnReplayTicks;
		}
	}

	// Record the replay output layout with the confirmed frame at its head. Index zero belongs to an
	// injected full state, whose slot ComputeOutputLayout selects instead.
	if (rScratch.iLastValidatedIndex > 0)
	{
		rScratch.outputLayout.iHead = SnapshotIndex(rScratch.iReplayWriteHead, rScratch.iLastValidatedIndex - 1);
		rScratch.outputLayout.iConfirmedInner = 0;
	}
}

// Forward ticks incorporate available server StatusChanges but leave their updates buffered.
// Validation and advancement of iConfirmedTick belong to the next reconciliation's CRC fast path.
static bool ReconcileForwardStepCoord(CoordWork& rWork, int64_t iTick, float& rfTime)
{
	engine::Cell& rCell = *rWork.pCell;
	CoordScratch& rScratch = rWork.scratch;

	rfTime += engine::kfDeltaTime;

	game::FrameInput frameInput;
	auto it = rCell.serverUpdates.find(iTick);
	if (it != rCell.serverUpdates.end())
	{
		frameInput.statusChanges = it->second.statusChanges;
	}

	if (!ReconcileRunTickCoord(rWork, iTick, rfTime, frameInput, false))
	{
		return false;
	}

	++rScratch.profiling.iAssumedFrameTicks;
	return true;
}

void ReconcileCatchUpCoord(CoordWork& rWork, int64_t iTargetTick, float& rfTime)
{
	engine::Cell& rCell = *rWork.pCell;
	CoordScratch& rScratch = rWork.scratch;

	int64_t iStartWriteCount = rScratch.iReplayWriteCount;
	int64_t iCurrentTick = rScratch.replayStack.at(rScratch.iReplayStackCount - 1)->interpolate.iTick;

	int64_t iBudget = engine::kiNetworkBufferSize - 1 - rScratch.iReplayWriteCount;
	int64_t iCappedTarget = std::min(iTargetTick, iCurrentTick + iBudget);

	while (iCurrentTick < iCappedTarget)
	{
		++iCurrentTick;
		if (!ReconcileForwardStepCoord(rWork, iCurrentTick, rfTime))
		{
			break;
		}
	}

	for (int64_t i = iStartWriteCount; i < rScratch.iReplayWriteCount; ++i)
	{
		int64_t iSlot = SnapshotIndex(rScratch.iReplayWriteHead, i);
		rCell.snapshots[iSlot]->interpolate.frameFlags.Set(engine::FrameFlags::kRecalculated, false);
	}
}

// After the CRC fast path commits, the ring contains the confirmed frame and speculative tail.
// Catch-up extends that tail toward iTargetTick, incorporating buffered server updates when available.
void ReconcileFastPathCatchUp(CoordWork& rWork, int64_t iTargetTick)
{
	engine::Cell& rCell = *rWork.pCell;
	CoordScratch& rScratch = rWork.scratch;

	if (rCell.iSnapshotCount == 0)
	{
		return;
	}

	int64_t iTailOffset = rCell.iSnapshotCount - 1;
	int64_t iTailPhysical = SnapshotIndex(rCell.iSnapshotHead, iTailOffset);
	game::Frame* pTail = rCell.snapshots[iTailPhysical].get();
	if (pTail == nullptr)
	{
		return;
	}

	if (pTail->interpolate.iTick >= iTargetTick)
	{
		return;
	}

	rScratch.replayStack.clear();
	rScratch.replayStack.push_back(pTail);
	rScratch.iReplayStackCount = 1;
	rScratch.iReplayWriteHead = SnapshotIndex(iTailPhysical, 1);
	rScratch.iReplayWriteCount = 0;

	float fTime = pTail->interpolate.fCurrentTime;
	int64_t iStartCount = rCell.iSnapshotCount;
	int64_t iBudget = engine::kiNetworkBufferSize - iStartCount;
	int64_t iCurrentTick = pTail->interpolate.iTick;
	int64_t iCappedTarget = std::min(iTargetTick, iCurrentTick + iBudget);

	while (iCurrentTick < iCappedTarget)
	{
		++iCurrentTick;
		if (!ReconcileForwardStepCoord(rWork, iCurrentTick, fTime))
		{
			break;
		}
	}

	for (int64_t i = 0; i < rScratch.iReplayWriteCount; ++i)
	{
		int64_t iSlot = SnapshotIndex(rScratch.iReplayWriteHead, i);
		rCell.snapshots[iSlot]->interpolate.frameFlags.Set(engine::FrameFlags::kRecalculated, false);
	}

	rCell.iSnapshotCount = std::min(iStartCount + rScratch.iReplayWriteCount, static_cast<int64_t>(engine::kiNetworkBufferSize));
}

} // namespace engine

#endif // BT_CLIENT
