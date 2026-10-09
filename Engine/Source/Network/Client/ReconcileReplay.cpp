#include "Pch.h"

#include "Network/Client/ReconcileReplay.h"

#if defined(BT_CLIENT)

#include "Frame/Frame.h"
#include "Game.h"

namespace engine
{

void ReconcileInjectPendingFullState(CoordWork& rWork)
{
	engine::Cell& rCell = *rWork.pCell;
	CoordScratch& rScratch = rWork.scratch;

	if (!rCell.pendingFullState)
	{
		return;
	}
	engine::Cell::PendingFullState& rPending = *rCell.pendingFullState;
	ASSERT(rPending.pFrame->interpolate.iTick == rPending.iTick);
	int64_t iSlot = SnapshotIndex(rScratch.iReplayWriteHead, rScratch.iReplayWriteCount);
	rPending.pFrame->postRender.uiSharedCrc = rPending.pFrame->Crc();
	rCell.snapshots[iSlot] = std::move(rPending.pFrame);
	rScratch.replayStack.clear();
	rScratch.replayStack.push_back(rCell.snapshots[iSlot].get());
	rScratch.iReplayStackCount = 1;
	rScratch.iReplayWriteHead = SnapshotIndex(iSlot, 1);
	rScratch.iReplayWriteCount = 0;
	rScratch.iInjectedBaseSlot = iSlot;
	// Full state replaces the timeline; a prior higher high-water mark was against a discarded timeline.
	rCell.iHighWaterValidatedTick = rPending.iTick;
	rCell.iLastFullStateTick = rPending.iTick;
	rCell.pendingFullState.reset();
}

// Writeback stays in the dispatch worker because reconciliation already mutates rCell.
// Success commits the confirmed tick and final ring layout.
static void ApplyCoordWriteback(CoordWork& rWork)
{
	engine::Cell& rCell = *rWork.pCell;
	CoordScratch& rScratch = rWork.scratch;

	if (rScratch.iNewConfirmedTick >= 0)
	{
		rCell.iConfirmedTick = rScratch.iNewConfirmedTick;
		rCell.iSnapshotHead = rScratch.outputLayout.iHead;
		rCell.iConfirmedOffset = rScratch.outputLayout.iConfirmedInner;
		rCell.iSnapshotCount = rScratch.outputLayout.iCount;
		ASSERT(rCell.iSnapshotCount >= 0 && rCell.iSnapshotCount <= engine::kiNetworkBufferSize);
	}
}

static void AdoptUnreachablePendingFullState(CoordWork& rWork)
{
	engine::Cell& rCell = *rWork.pCell;
	CoordScratch& rScratch = rWork.scratch;
	engine::Cell::PendingFullState& rPending = *rCell.pendingFullState;

	ASSERT(rPending.pFrame != nullptr);
	ASSERT(rPending.pFrame->interpolate.iTick == rPending.iTick);

	int64_t iAdoptedTick = rPending.iTick;
	int64_t iAdoptedSlot = SnapshotIndex(rScratch.iReplayWriteHead, rScratch.iReplayWriteCount);
	rPending.pFrame->postRender.uiSharedCrc = rPending.pFrame->Crc();
	rCell.snapshots[iAdoptedSlot] = std::move(rPending.pFrame);

	rScratch.replayStack.clear();
	rScratch.replayStack.push_back(rCell.snapshots[iAdoptedSlot].get());
	rScratch.iReplayStackCount = 1;
	rScratch.iReplayWriteHead = SnapshotIndex(iAdoptedSlot, 1);
	rScratch.iReplayWriteCount = 0;
	rScratch.iLastValidatedIndex = -1;
	rScratch.iNewConfirmedTick = iAdoptedTick;
	rScratch.outputLayout =
	{
		.iHead = iAdoptedSlot,
		.iCount = 1,
		.iConfirmedInner = 0,
	};

	rCell.iHighWaterValidatedTick = iAdoptedTick;
	rCell.iLastFullStateTick = iAdoptedTick;
	rCell.pendingFullState.reset();
	std::erase_if(rCell.serverUpdates, [iAdoptedTick](const auto& rEntry) { return rEntry.first <= iAdoptedTick; });
	rCell.iLastReplayConfirmedTick = -1;
	rCell.iLastReplayServerUpdateCount = -1;

	LOG(kNetwork, kWarning, "ReconcileCoord Adopted authoritative full state past update gap Coord: ({},{}) AdoptedTick: {}", rWork.coord.iX, rWork.coord.iY, iAdoptedTick);
}

// Aggressive CRC walk: finds the highest matching ring frame across all server updates
// in range, advances iConfirmedTick/iConfirmedOffset to it, and reports the lowest
// unresolved mismatch (if any) past the new confirmed point. Returns true if the fast
// path fully resolved this coord (writeback + catch-up already done, caller should
// return); false if the caller should continue to full replay using rOutputResult.
static bool ApplyCrcFastPath(CoordWork& rWork, const ReconcileInputs& rInputs, CrcFastPathCoordResult& rOutputResult)
{
	engine::Cell& rCell = *rWork.pCell;
	CoordScratch& rScratch = rWork.scratch;

	rOutputResult = CrcFastPathProcessCoord(rWork, rInputs.iTargetTick);
	if (rOutputResult.bHandled)
	{
		++rScratch.profiling.iCrcFastPathEvents;
		rCell.iLastReplayConfirmedTick = -1;
		rCell.iLastReplayServerUpdateCount = -1;
		ApplyCoordWriteback(rWork);
		ReconcileFastPathCatchUp(rWork, rInputs.iTargetTick);
		return true;
	}

	// The walk may have set the output layout via CrcApplyMatchResult —
	// preserve those as the floor result. If full replay validates further or injects a full state,
	// ComputeOutputLayout overwrites them. Count must be recomputed from scratch
	// because walk's count included old speculative frames that replay will overwrite.
	rScratch.flags.Set(ReconcileScratchFlags::kCrcFastPath, false);
	rScratch.outputLayout.iCount = 0;
	return false;
}

// No server data at the first tick past confirmed — replay cannot start. Keep existing
// speculative ring (populated by prior catch-up) and wait for resend. Returns true if
// this short-circuit applied (caller should return from ReconcileCoord).
static bool EarlyReturnIfNoServerData(CoordWork& rWork, const ReconcileInputs& rInputs, const CrcFastPathCoordResult& rFastPathResult)
{
	engine::Cell& rCell = *rWork.pCell;
	CoordScratch& rScratch = rWork.scratch;

	if (rCell.serverUpdates.contains(rCell.iConfirmedTick + 1) || HasDuePendingFullState(rCell, rInputs.iTargetTick))
	{
		return false;
	}

	if (rScratch.iNewConfirmedTick >= 0)
	{
		rScratch.outputLayout = ComputeRetention(rFastPathResult.preWritebackLayout.iHead, rFastPathResult.preWritebackLayout.iCount, rFastPathResult.preWritebackLayout.iConfirmedInner);
		ApplyCoordWriteback(rWork);
	}
	ReconcileFastPathCatchUp(rWork, rInputs.iTargetTick);
	return true;
}

// Determine rollback base: prefer the shrunk target (one tick before the lowest unresolved
// mismatch), using the speculative ring frame at that logical offset as the starting state.
// Fall back to iConfirmedTick if the shrunk target is unavailable or the walk didn't find
// a mismatch (e.g., due pending full state or gap-only path). Only attempt shrunk rollback
// if the base frame was CRC-validated — speculative frames from catch-up are guaranteed
// wrong when a gap caused the mismatch.
static void DetermineRollbackBase(CoordWork& rWork, const ReconcileInputs& rInputs, const CrcFastPathCoordResult& rFastPathResult, int64_t& riRollbackTick, int64_t& riRollbackOffset, bool& rbShrunkRollback)
{
	engine::Cell& rCell = *rWork.pCell;
	CoordScratch& rScratch = rWork.scratch;

	riRollbackTick = rCell.iConfirmedTick;
	riRollbackOffset = rFastPathResult.preWritebackLayout.iConfirmedInner;
	rbShrunkRollback = false;

	if (rFastPathResult.iLowestUnresolvedMismatch > rCell.iConfirmedTick + 1 && !HasDuePendingFullState(rCell, rInputs.iTargetTick))
	{
		int64_t iShrunkTick = rFastPathResult.iLowestUnresolvedMismatch - 1;
		if (iShrunkTick <= rCell.iHighWaterValidatedTick)
		{
			int64_t iShrunkIndex = -1;
			for (int64_t i = 0; i < rCell.iSnapshotCount; ++i)
			{
				int64_t iPhysical = SnapshotIndex(rCell.iSnapshotHead, i);
				if (rCell.snapshots[iPhysical] != nullptr && rCell.snapshots[iPhysical]->interpolate.iTick == iShrunkTick)
				{
					iShrunkIndex = i;
					break;
				}
			}
			if (iShrunkIndex >= 0)
			{
				riRollbackTick = iShrunkTick;
				riRollbackOffset = iShrunkIndex;
				rbShrunkRollback = true;
				rScratch.flags.Set(ReconcileScratchFlags::kShrunkRollback);
			}
		}
	}
}

// Roll back to the selected base, inject a reachable pending full state, and replay the consecutive
// range. The fallback retries from the typed full-confirmed layout after a provisional shrunk-base
// desync; only its empty range resolves the coord here.
enum class ReplayMode
{
	kPrimary,
	kFallback,
};

static bool RunReplay(CoordWork& rWork, const ReconcileInputs& rInputs, const RingLayout& rPreWritebackLayout, int64_t& riRollbackTick, int64_t& riRollbackOffset, bool& rbShrunkRollback, ReplayMode eMode, float& rfTime, int64_t& riReplayStart)
{
	engine::Cell& rCell = *rWork.pCell;
	CoordScratch& rScratch = rWork.scratch;

	if (eMode == ReplayMode::kFallback)
	{
		if (!(rScratch.flags & ReconcileScratchFlags::kSuppressRepeatLogs))
		{
			LOG(kNetwork, kDebug, "ReconcileCoord Provisional shrunk-rollback mismatch; retrying from full rollback Coord: ({},{}) DesyncTick: {}", rWork.coord.iX, rWork.coord.iY, rScratch.iDesyncTick);
		}
		rScratch.iDesyncTick = -1;
		rScratch.desyncExpectedCrc = 0;
		rScratch.desyncActualCrc = 0;
		rScratch.pDesyncClientFrame.reset();
		rScratch.iLastValidatedIndex = -1;

		riRollbackTick = rCell.iConfirmedTick;
		riRollbackOffset = rPreWritebackLayout.iConfirmedInner;
		rbShrunkRollback = false;
		rScratch.flags.Set(ReconcileScratchFlags::kShrunkRollback, false);
	}

	ReconcileRollbackCoord(rWork, riRollbackOffset);
	rfTime = rScratch.replayStack.at(0)->interpolate.fCurrentTime;

	// Inject a due pending full state at the confirmed frame. Future states remain queued.
	if (HasDuePendingFullState(rCell, rInputs.iTargetTick) && rCell.pendingFullState->iTick == rCell.iConfirmedTick)
	{
		ReconcileInjectPendingFullState(rWork);
		rfTime = rScratch.replayStack.at(0)->interpolate.fCurrentTime;
		if (eMode == ReplayMode::kPrimary)
		{
			LOG(kNetwork, kVerbose, "ReconcileCoord Injected pending full state Coord: ({},{}) AtTick: {}", rWork.coord.iX, rWork.coord.iY, rCell.iConfirmedTick);
		}
	}
	else if (eMode == ReplayMode::kPrimary && HasDuePendingFullState(rCell, rInputs.iTargetTick) && rCell.pendingFullState->iTick < rCell.iConfirmedTick)
	{
		LOG(kNetwork, kVerbose, "ReconcileCoord Discarded stale pending full state Coord: ({},{}) FullStateTick: {} ConfirmedTick: {}", rWork.coord.iX, rWork.coord.iY, rCell.pendingFullState->iTick, rCell.iConfirmedTick);
		rCell.pendingFullState.reset();
	}

	riReplayStart = riRollbackTick + 1;
	int64_t iUncappedMaximumConsecutive = ReconcileFindReplayRangeCoord(rWork, riReplayStart);
	if (eMode == ReplayMode::kPrimary && HasDuePendingFullState(rCell, rInputs.iTargetTick) && rCell.pendingFullState->iTick > iUncappedMaximumConsecutive)
	{
		int64_t iAdoptedTick = rCell.pendingFullState->iTick;
		AdoptUnreachablePendingFullState(rWork);
		rfTime = rScratch.replayStack.at(0)->interpolate.fCurrentTime;
		riReplayStart = iAdoptedTick + 1;
		return false;
	}
	int64_t iMaximumConsecutive = std::min(iUncappedMaximumConsecutive, rInputs.iTargetTick);

	if (eMode == ReplayMode::kFallback && iMaximumConsecutive < riReplayStart && !HasDuePendingFullState(rCell, rInputs.iTargetTick))
	{
		if (rScratch.iNewConfirmedTick >= 0)
		{
			rScratch.outputLayout = ComputeRetention(rPreWritebackLayout.iHead, rPreWritebackLayout.iCount, rPreWritebackLayout.iConfirmedInner);
			ApplyCoordWriteback(rWork);
		}
		ReconcileFastPathCatchUp(rWork, rInputs.iTargetTick);
		return true;
	}

	ReconcileReplayCoord(rWork, riReplayStart, iMaximumConsecutive, rfTime);
	return false;
}

// Compute output layout: confirmed frame + remaining replay/catch-up frames, plus up to
// kiRenderBehindTicks frames below the confirmed frame after validation past the base. Four
// subcases fold together: validation past the base, validation at the base, walk advanced
// confirmed but replay didn't validate further, and catch-up after a gap.
static void ComputeOutputLayout(CoordWork& rWork, int64_t iRollbackOffset)
{
	engine::Cell& rCell = *rWork.pCell;
	CoordScratch& rScratch = rWork.scratch;

	if (rScratch.iLastValidatedIndex > 0)
	{
		// Replay-stack frame k sits k slots past its base: the rollback base in the pre-replay ring,
		// whose older slots stay valid history, or an injected full state, which has none below it.
		int64_t iBaseHead = rCell.iSnapshotHead;
		int64_t iBaseOffset = iRollbackOffset;
		if (rScratch.iInjectedBaseSlot >= 0)
		{
			iBaseHead = rScratch.iInjectedBaseSlot;
			iBaseOffset = 0;
		}
		rScratch.outputLayout = ComputeRetention(iBaseHead, iBaseOffset + 1 + rScratch.iReplayWriteCount, iBaseOffset + rScratch.iLastValidatedIndex);

		// Writes past the ring size overwrote the oldest pre-replay slots; drop them from the head.
		int64_t iExcess = rScratch.outputLayout.iCount - engine::kiNetworkBufferSize;
		if (iExcess > 0)
		{
			rScratch.outputLayout.iHead = SnapshotIndex(rScratch.outputLayout.iHead, iExcess);
			rScratch.outputLayout.iCount -= iExcess;
			rScratch.outputLayout.iConfirmedInner -= iExcess;
		}
	}
	else if (rScratch.iLastValidatedIndex == 0)
	{
		rScratch.outputLayout.iCount = rScratch.iReplayWriteCount + 1;
	}
	else if (rScratch.iNewConfirmedTick >= 0)
	{
		// Walk advanced iConfirmedTick but full replay didn't validate anything further.
		// Preserve walk's confirmed frame as the base and include new catch-up frames.
		rScratch.outputLayout.iCount = rScratch.iReplayWriteCount + 1;
	}
	else
	{
		// Full replay ran catch-up without validating (gap in serverUpdates past confirmed).
		// Preserve existing confirmed tick/offset as the base so catch-up frames are committed.
		rScratch.iNewConfirmedTick = rCell.iConfirmedTick;
		rScratch.outputLayout =
		{
			.iHead = SnapshotIndex(rCell.iSnapshotHead, iRollbackOffset),
			.iCount = rScratch.iReplayWriteCount + 1,
			.iConfirmedInner = 0,
		};
	}

	// A validated index of zero is the injected frame itself and a negative index validated nothing
	// after it, so the injected full state stays the ring head at confirmed offset zero; from index
	// one the retention above places head and confirmed offset, never below the injection.
	if (rScratch.iInjectedBaseSlot >= 0 && rScratch.iLastValidatedIndex <= 0)
	{
		rScratch.outputLayout.iHead = rScratch.iInjectedBaseSlot;
		rScratch.outputLayout.iConfirmedInner = 0;
	}

	rScratch.outputLayout.iCount = std::min(rScratch.outputLayout.iCount, static_cast<int64_t>(engine::kiNetworkBufferSize));
	ASSERT(rScratch.outputLayout.iCount >= 0 && rScratch.outputLayout.iCount <= engine::kiNetworkBufferSize);
}

void ReconcileCoord(CoordWork& rWork, const ReconcileInputs& rInputs)
{
	engine::Cell& rCell = *rWork.pCell;
	CoordScratch& rScratch = rWork.scratch;

	// Capture pre-reconcile ring tail tick so ReconcileReplayCoord can distinguish actual
	// re-simulation (replay of a tick that already existed) from first-time forward sim.
	if (rCell.iSnapshotCount > 0)
	{
		int64_t iTailPhysical = SnapshotIndex(rCell.iSnapshotHead, rCell.iSnapshotCount - 1);
		if (rCell.snapshots[iTailPhysical] != nullptr)
		{
			rScratch.iPreReconcileTailTick = rCell.snapshots[iTailPhysical]->interpolate.iTick;
		}
	}

	CrcFastPathCoordResult fastPathResult {};
	if (ApplyCrcFastPath(rWork, rInputs, fastPathResult))
	{
		return;
	}

	if (EarlyReturnIfNoServerData(rWork, rInputs, fastPathResult))
	{
		return;
	}

	rScratch.flags.Set(ReconcileScratchFlags::kReplayed);

	// Invariant: full replay must not repeat identical work. If iConfirmedTick and serverUpdates
	// are unchanged since the last full replay attempt, the result would be the same.
	int64_t iCurrentUpdateCount = static_cast<int64_t>(rCell.serverUpdates.size());
	if (rCell.iConfirmedTick == rCell.iLastReplayConfirmedTick && iCurrentUpdateCount == rCell.iLastReplayServerUpdateCount
	 && !HasDuePendingFullState(rCell, rInputs.iTargetTick))
	{
		DEBUG_BREAK();
	}
	rCell.iLastReplayConfirmedTick = rCell.iConfirmedTick;
	rCell.iLastReplayServerUpdateCount = iCurrentUpdateCount;

	int64_t iRollbackTick = 0;
	int64_t iRollbackOffset = 0;
	bool bShrunkRollback = false;
	DetermineRollbackBase(rWork, rInputs, fastPathResult, iRollbackTick, iRollbackOffset, bShrunkRollback);

	float fTime = 0.0f;
	int64_t iReplayStart = 0;
	RunReplay(rWork, rInputs, fastPathResult.preWritebackLayout, iRollbackTick, iRollbackOffset, bShrunkRollback, ReplayMode::kPrimary, fTime, iReplayStart);

	if (rScratch.iDesyncTick >= 0 && bShrunkRollback && rScratch.iDesyncTick == iReplayStart)
	{
		if (RunReplay(rWork, rInputs, fastPathResult.preWritebackLayout, iRollbackTick, iRollbackOffset, bShrunkRollback, ReplayMode::kFallback, fTime, iReplayStart))
		{
			return;
		}
	}

	if (rScratch.iDesyncTick >= 0)
	{
		return;
	}

	ReconcileCatchUpCoord(rWork, rInputs.iTargetTick, fTime);

	ComputeOutputLayout(rWork, iRollbackOffset);

	ASSERT(rScratch.replayStack.at(rScratch.iReplayStackCount - 1)->interpolate.iTick <= rInputs.iTargetTick);

	ApplyCoordWriteback(rWork);
}

ReconcileDispatchResult ReconcileDispatcher::Run(const ReconcileInputs& rInputs)
{
	// Heap: work list growth and per-coord scratch retention
	ScopedSuppressAllocationTracking suppress;

	// Reusing work slots preserves replayStack capacity across Run() calls and avoids per-frame allocations.
	int64_t iEligibleCount = 0;
	for (const auto& [rCoord, rCell] : game::gpGame->mCells)
	{
		if (rCell.iConfirmedTick >= 0)
		{
			++iEligibleCount;
		}
	}
	if (std::ssize(mWorks) < iEligibleCount)
	{
		mWorks.resize(static_cast<size_t>(iEligibleCount));
	}
	int64_t iSlot = 0;
	for (auto& [rCoord, rCell] : game::gpGame->mCells)
	{
		if (rCell.iConfirmedTick < 0)
		{
			continue;
		}
		CoordWork& rWork = mWorks.at(iSlot++);
		rWork.coord = rCoord;
		rWork.pCell = &rCell;
		rWork.scratch.Reset();
	}
	int64_t iActiveCount = iSlot;
	miActiveCount = iActiveCount;

	ReconcileDispatchResult result;
	result.iActiveCount = miActiveCount;

	if (iActiveCount == 0)
	{
		return result;
	}

	// Each worker touches only its own Cell entry.
	std::span<CoordWork> activeWorks(mWorks.data(), static_cast<size_t>(iActiveCount));
	int64_t iCount = iActiveCount;
	auto ProcessRange = [&](int64_t iBegin, int64_t iEnd)
	{
		// Heap: ReconcileCoord may grow per-coord scratch (frames, replay buffers) on dispatch
		ScopedSuppressAllocationTracking suppress;
		for (int64_t i = iBegin; i < iEnd; ++i)
		{
			ReconcileCoord(activeWorks[i], rInputs);
		}
	};
	common::gpMultithreading->Dispatch(iCount, ProcessRange);

	// Post-dispatch merge: profiling, desync (first-wins), bAnyFullReplay
	for (CoordWork& rWork : activeWorks)
	{
		const CoordScratch& rScratch = rWork.scratch;
		result.profiling.iCrcValidatedFrameTicks += rScratch.profiling.iCrcValidatedFrameTicks;
		result.profiling.iAssumedFrameTicks += rScratch.profiling.iAssumedFrameTicks;
		result.profiling.iCrcFastPathEvents += rScratch.profiling.iCrcFastPathEvents;
		result.profiling.iStatusChangeReplayTicks += rScratch.profiling.iStatusChangeReplayTicks;
		result.profiling.iKnockOnReplayTicks += rScratch.profiling.iKnockOnReplayTicks;

		if (rScratch.iDesyncTick >= 0 && result.pDesyncWork == nullptr)
		{
			result.pDesyncWork = &rWork;
		}

		using enum ReconcileScratchFlags;

		// Audio voice invalidation skip is gated on the client coord experiencing a full replay.
		if ((rScratch.flags & kReplayed) && rWork.coord == game::gpGame->mClientGridCoordinate)
		{
			result.bAnyFullReplay = true;
		}

		if (rScratch.iDesyncTick >= 0 || ((rScratch.flags & kReplayed) && (rScratch.flags & kResimulationOccurred)))
		{
			bool bLogThis = !(rScratch.flags & kSuppressRepeatLogs) || (rWork.pCell->iStuckFrameCount % engine::Cell::kiStuckLogInterval == 0);
			if (bLogThis)
			{
				if (rScratch.iDesyncTick >= 0)
				{
					LOG(kNetwork, kDebug, "Reconcile post-replay Rollback/replay exhausted with unresolved CRC mismatch Coord: ({},{}) NewConfirmedTick: {} Validated: {}/{} CrcFastPath: {} Replayed: {} ShrunkRollback: {} DesyncTick: {}", rWork.coord.iX, rWork.coord.iY, rScratch.iNewConfirmedTick, rScratch.iLastValidatedIndex + 1, rScratch.iReplayStackCount, static_cast<bool>(rScratch.flags & kCrcFastPath), static_cast<bool>(rScratch.flags & kReplayed), static_cast<bool>(rScratch.flags & kShrunkRollback), rScratch.iDesyncTick);
				}
				else
				{
					LOG(kNetwork, kDebug, "Reconcile post-replay Reconciliation resimulation completed without an unresolved CRC mismatch Coord: ({},{}) NewConfirmedTick: {} Validated: {}/{} CrcFastPath: {} Replayed: {} ShrunkRollback: {} DesyncTick: {}", rWork.coord.iX, rWork.coord.iY, rScratch.iNewConfirmedTick, rScratch.iLastValidatedIndex + 1, rScratch.iReplayStackCount, static_cast<bool>(rScratch.flags & kCrcFastPath), static_cast<bool>(rScratch.flags & kReplayed), static_cast<bool>(rScratch.flags & kShrunkRollback), rScratch.iDesyncTick);
				}
			}
		}
	}

	return result;
}

void ReconcileDispatcher::Reset()
{
	mWorks.clear();
	miActiveCount = 0;
}

} // namespace engine

#endif // BT_CLIENT
