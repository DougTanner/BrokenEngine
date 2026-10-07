#include "Pch.h"

#include "Network/Client/ReconcileReplay.h"

#if defined(BT_CLIENT)

#include "Frame/Frame.h"
#include "Frame/StatusChange.h"

namespace engine
{

static int64_t FindSnapshotIndex(const std::unique_ptr<game::Frame> (&rSnapshots)[engine::kiNetworkBufferSize], int64_t iHead, int64_t iCount, int64_t iTick)
{
	for (int64_t i = 0; i < iCount; ++i)
	{
		int64_t iPhysical = SnapshotIndex(iHead, i);
		if (rSnapshots[iPhysical] != nullptr && rSnapshots[iPhysical]->interpolate.iTick == iTick)
		{
			return i;
		}
	}
	return -1;
}

struct CrcValidateResult
{
	// True when no unresolved mismatches remain past iHighestMatch.
	bool bMatch = true;
	// Highest tick whose ring-frame uiSharedCrc matched its server update. -1 if no match.
	int64_t iHighestMatch = -1;
	int64_t iHighestMatchIndex = -1;
	// Lowest tick > iConfirmedTick whose ring-frame uiSharedCrc did NOT match. -1 if none.
	// Mismatches at ticks < iHighestMatch are bypassed (they'll be dropped anyway).
	int64_t iLowestUnresolvedMismatch = -1;
};

static CrcValidateResult CrcValidateLoop(const CoordWork& rWork, int64_t iTargetTick, bool bSuppressRepeatLogs)
{
	engine::Cell& rCell = *rWork.pCell;

	CrcValidateResult result;
	int64_t iLowestUnresolved = std::numeric_limits<int64_t>::max();
	int64_t iMismatchCount = 0;
	int64_t iAdditionalMismatchesWithStatusChanges = 0;
	int64_t iSecondMismatchTick = -1;
	int64_t iLastMismatchTick = -1;

	// Walk in tick-ascending order. iHighestMatch grows monotonically, so whenever it advances
	// any prior-tracked mismatch becomes bypassed (older than the new confirmed tick) and we
	// can reset the unresolved tracker.
	common::ScopedWorkbufferArena sortedTicks = common::gpThreadLocal->mWorkbuffer.Push();
	for (const auto& [iTick, rUpdate] : rCell.serverUpdates)
	{
		if (iTick >= rCell.iConfirmedTick + 1 && iTick <= iTargetTick)
		{
			sortedTicks.mBuffer.PushBack(iTick);
		}
	}
	std::span<int64_t> ticks = sortedTicks.mBuffer.Span<int64_t>();
	std::ranges::sort(ticks);
	for (int64_t iTick : ticks)
	{
		auto it = rCell.serverUpdates.find(iTick);
		int64_t iIndex = FindSnapshotIndex(rCell.snapshots, rCell.iSnapshotHead, rCell.iSnapshotCount, iTick);
		if (iIndex < 0)
		{
			continue;
		}
		int64_t iPhysical = SnapshotIndex(rCell.iSnapshotHead, iIndex);
		const game::Frame& rClientFrame = *rCell.snapshots[iPhysical];

		if (rClientFrame.postRender.uiSharedCrc == it->second.uiSharedCrc)
		{
			result.iHighestMatch = iTick;
			result.iHighestMatchIndex = iIndex;
			if (iLowestUnresolved != std::numeric_limits<int64_t>::max())
			{
				LOG(kNetwork, kDebug, "CrcValidateLoop Speculative CRC mismatch superseded by later CRC match Coord: ({},{}) MismatchTick: {} MatchTick: {}", rWork.coord.iX, rWork.coord.iY, iLowestUnresolved, iTick);
				iLowestUnresolved = std::numeric_limits<int64_t>::max();
			}
		}
		else
		{
			if (!bSuppressRepeatLogs && iMismatchCount == 0)
			{
				char acSharedCrc[20] {}, acClientCrc[20] {};
				common::ToHex(std::span<char, 20>(acSharedCrc), it->second.uiSharedCrc);
				common::ToHex(std::span<char, 20>(acClientCrc), rClientFrame.postRender.uiSharedCrc);

				common::ScopedWorkbufferArena builder = common::gpThreadLocal->mWorkbuffer.Push();
				if (!it->second.statusChanges.empty())
				{
					int64_t iCounts[static_cast<int64_t>(game::StatusChangeType::kCount)] {};
					for (const game::StatusChange& rStatusChange : it->second.statusChanges)
					{
						++iCounts[static_cast<int64_t>(rStatusChange.eType)];
					}
					bool bFirst = true;
					for (int64_t i = 0; i < static_cast<int64_t>(game::StatusChangeType::kCount); ++i)
					{
						if (iCounts[i] > 0)
						{
							if (!bFirst)
							{
								builder.mBuffer.Append(", ");
							}
							builder.mBuffer.Append(game::StatusChangeTypeName(static_cast<game::StatusChangeType>(i)));
							if (iCounts[i] > 1)
							{
								builder.mBuffer.Append("x");
								builder.mBuffer.Append(iCounts[i]);
							}
							bFirst = false;
						}
					}
				}

				LOG(kNetwork, kDebug, "CrcValidateLoop Speculative CRC mismatch; reconciliation pending Coord: ({},{}) ForTick: {} ServerCrc: {} ClientCrc: {} StatusChanges: {} [{}] TicksSinceFullState: {}", rWork.coord.iX, rWork.coord.iY, iTick, acSharedCrc, acClientCrc, std::ssize(it->second.statusChanges), builder.mBuffer.View(), (rCell.iLastFullStateTick >= 0) ? (iTick - rCell.iLastFullStateTick) : -1i64);
			}
			++iMismatchCount;
			if (iMismatchCount > 1)
			{
				if (iSecondMismatchTick < 0)
				{
					iSecondMismatchTick = iTick;
				}
				if (!it->second.statusChanges.empty())
				{
					++iAdditionalMismatchesWithStatusChanges;
				}
			}
			// NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores) — read only by the kVerbose summary LOG below, which compiles out at default log levels
			iLastMismatchTick = iTick;
			if (iLowestUnresolved == std::numeric_limits<int64_t>::max())
			{
				iLowestUnresolved = iTick;
			}
		}
	}

	if (!bSuppressRepeatLogs && iMismatchCount > 1)
	{
		LOG(kNetwork, kVerbose, "CrcValidateLoop {} additional mismatches Coord: ({},{}) Ticks: {}-{} ({} with StatusChanges)", iMismatchCount - 1, rWork.coord.iX, rWork.coord.iY, iSecondMismatchTick, iLastMismatchTick, iAdditionalMismatchesWithStatusChanges);
	}

	if (iLowestUnresolved != std::numeric_limits<int64_t>::max())
	{
		result.iLowestUnresolvedMismatch = iLowestUnresolved;
		result.bMatch = false;
	}

	return result;
}

static void CrcApplyMatchResult(CoordWork& rWork, int64_t iHighestMatch, int64_t iHighestMatchIndex)
{
	engine::Cell& rCell = *rWork.pCell;
	CoordScratch& rScratch = rWork.scratch;

	rScratch.flags.Set(ReconcileScratchFlags::kCrcFastPath);
	rScratch.iNewConfirmedTick = iHighestMatch;
	rScratch.profiling.iCrcValidatedFrameTicks += iHighestMatch - rCell.iConfirmedTick;

	// Retain kiRenderBehindTicks frames before the confirmed match so the renderer always has
	// a prev-tail (or N prev-tails) available for interpolation.
	rScratch.outputLayout = ComputeRetention(rCell.iSnapshotHead, rCell.iSnapshotCount, iHighestMatchIndex);

	// Drop validated entries — fast-path advances iConfirmedTick in place, so anything
	// at or below it is now consumed and would otherwise accumulate in serverUpdates.
	std::erase_if(rCell.serverUpdates, [iHighestMatch](const auto& rEntry) { return rEntry.first <= iHighestMatch; });
}

CrcFastPathCoordResult CrcFastPathProcessCoord(CoordWork& rWork, int64_t iTargetTick)
{
	engine::Cell& rCell = *rWork.pCell;

	CrcFastPathCoordResult result
	{
		.preWritebackLayout =
		{
			.iHead = rCell.iSnapshotHead,
			.iCount = rCell.iSnapshotCount,
			.iConfirmedInner = rCell.iConfirmedOffset,
		},
	};

	if (rCell.serverUpdates.empty() && !HasDuePendingFullState(rCell, iTargetTick))
	{
		return result;
	}

	if (HasDuePendingFullState(rCell, iTargetTick))
	{
		result.bHandled = false;
		LOG(kNetwork, kVerbose, "CrcFastPathProcessCoord Due pending full state forces reconcile Coord: ({},{})", rWork.coord.iX, rWork.coord.iY);
		return result;
	}

	// Compute log suppression: if confirmed tick and first mismatch tick are unchanged from
	// last frame, this is a repeat stuck state — suppress per-tick mismatch detail logging.
	bool bSameState = (rCell.iConfirmedTick == rCell.iLastLoggedConfirmedTick);
	if (bSameState && !rCell.serverUpdates.empty())
	{
		auto it = rCell.serverUpdates.end();
		for (auto candidateIt = rCell.serverUpdates.begin(); candidateIt != rCell.serverUpdates.end(); ++candidateIt)
		{
			if (candidateIt->first > rCell.iConfirmedTick && (it == rCell.serverUpdates.end() || candidateIt->first < it->first))
			{
				it = candidateIt;
			}
		}
		bSameState = (it != rCell.serverUpdates.end() && it->first == rCell.iLastLoggedFirstMismatch);
	}

	// Cooldown: suppress detail logging when mismatch was recently logged (covers multiple
	// Run() calls at the same or adjacent ticks within a single render frame)
	bool bCooldownActive = (rCell.iLastMismatchDetailLogTick >= 0
	                     && iTargetTick - rCell.iLastMismatchDetailLogTick < engine::Cell::kiMismatchDetailLogCooldown);

	CrcValidateResult validateResult = CrcValidateLoop(rWork, iTargetTick, bSameState || bCooldownActive);

	if (bSameState)
	{
		++rCell.iStuckFrameCount;
		rWork.scratch.flags.Set(ReconcileScratchFlags::kSuppressRepeatLogs);
		if ((rCell.iStuckFrameCount % engine::Cell::kiStuckLogInterval) == 0)
		{
			LOG(kNetwork, kVerbose, "CrcValidateLoop still stuck Coord: ({},{}) ConfirmedTick: {} FirstMismatch: {} StuckFrames: {}", rWork.coord.iX, rWork.coord.iY, rCell.iConfirmedTick, rCell.iLastLoggedFirstMismatch, rCell.iStuckFrameCount);
		}
	}
	else if (!validateResult.bMatch)
	{
		rCell.iLastLoggedConfirmedTick = rCell.iConfirmedTick;
		rCell.iLastLoggedFirstMismatch = validateResult.iLowestUnresolvedMismatch;
		rCell.iStuckFrameCount = 0;
		if (!bCooldownActive)
		{
			rCell.iLastMismatchDetailLogTick = iTargetTick;
		}
		else
		{
			rWork.scratch.flags.Set(ReconcileScratchFlags::kSuppressRepeatLogs);
		}
	}

	// Gap at confirmed+1 with no matches/mismatches: first server update is non-consecutive
	// and nothing was validatable. Nothing for the fast path or full replay to do this cycle.
	if (validateResult.iHighestMatch == -1 && validateResult.bMatch && !rCell.serverUpdates.empty() && std::ranges::min_element(rCell.serverUpdates, {}, [](const auto& rEntry) { return rEntry.first; })->first != rCell.iConfirmedTick + 1)
	{
		return result;
	}

	if (validateResult.iHighestMatch == -1 && validateResult.bMatch && rCell.iConfirmedTick >= iTargetTick)
	{
		return result;
	}

	if (validateResult.iHighestMatch >= 0)
	{
		CrcApplyMatchResult(rWork, validateResult.iHighestMatch, validateResult.iHighestMatchIndex);
		result.preWritebackLayout.iConfirmedInner = validateResult.iHighestMatchIndex;
		rCell.iHighWaterValidatedTick = std::max(rCell.iHighWaterValidatedTick, validateResult.iHighestMatch);

		// Advance iConfirmedTick/iConfirmedOffset so any subsequent rollback starts at the new
		// confirmed point. Required by the Part 1 invariant (no re-simulation of validated ticks).
		rCell.iConfirmedTick = validateResult.iHighestMatch;
		rCell.iConfirmedOffset = validateResult.iHighestMatchIndex;

		if (!validateResult.bMatch)
		{
			result.bHandled = false;
			result.iLowestUnresolvedMismatch = validateResult.iLowestUnresolvedMismatch;
			if (!(rWork.scratch.flags & ReconcileScratchFlags::kSuppressRepeatLogs))
			{
				LOG(kNetwork, kDebug, "CrcFastPathProcessCoord Later CRC match left an unresolved speculative mismatch; rollback/replay required Coord: ({},{}) HighestMatch: {} LowestMismatch: {}", rWork.coord.iX, rWork.coord.iY, validateResult.iHighestMatch, validateResult.iLowestUnresolvedMismatch);
			}
		}
	}
	else if (!validateResult.bMatch)
	{
		result.bHandled = false;
		result.iLowestUnresolvedMismatch = validateResult.iLowestUnresolvedMismatch;
		if (!(rWork.scratch.flags & ReconcileScratchFlags::kSuppressRepeatLogs))
		{
			LOG(kNetwork, kDebug, "CrcFastPathProcessCoord No snapshot CRC match; rollback/replay required Coord: ({},{}) FirstMismatchTick: {}", rWork.coord.iX, rWork.coord.iY, validateResult.iLowestUnresolvedMismatch);
		}
	}
	else if (rCell.iConfirmedTick + 1 < iTargetTick)
	{
		// No matches, no mismatches, but target is past confirmed — gaps in serverUpdates.
		// Fall through to full replay to extend the ring (or run catch-up).
		result.bHandled = false;
	}

	return result;
}

} // namespace engine

#endif // BT_CLIENT
