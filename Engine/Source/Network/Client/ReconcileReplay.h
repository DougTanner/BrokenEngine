#pragma once

#if defined(BT_CLIENT)

// The engine owns the client reconciliation replay chain: the CRC fast path, the past-frame ring and rollback
// orchestration, and the replay tick driver that applies transfers after each tick and recomputes the Frame CRC at the
// same comparison point the server uses. The game supplies the Frame/FrameInput/StatusChange payloads this machinery
// replays, the transfer classification and materialization it calls, and the client-state policy that consumes the
// per-coordinate scratch. This header stays out of the Engine.h aggregation because it needs the complete engine::Cell
// and SnapshotIndex from GameBase.h, which each game Pch.h includes after Engine.h; its consumers include it directly.

namespace game
{

struct Frame;
struct FrameInput;
struct StatusChange;

} // namespace game

namespace engine
{

struct ReconcileProfiling
{
	int64_t iExecutedCoordTicks = 0;
	int64_t iRecalculatedCoordTicks = 0;
	int64_t iTickExecutionNanoseconds = 0;
	FrameTickPhaseTimings phaseTimings;
	int64_t iCrcValidatedFrameTicks = 0;
	int64_t iAssumedFrameTicks = 0;
	int64_t iCrcFastPathEvents = 0;
	int64_t iStatusChangeReplayTicks = 0;
	int64_t iKnockOnReplayTicks = 0;
};

struct ReconcileInputs
{
	int64_t iTargetTick = 0;
};

enum class ReconcileScratchFlags : uint8_t
{
	kCrcFastPath        = 0x01,
	kReplayed           = 0x02,
	kShrunkRollback     = 0x04,
	kResimulationOccurred      = 0x08,
	kSuppressRepeatLogs = 0x10,
};

struct RingLayout
{
	int64_t iHead = -1;
	int64_t iCount = 0;
	int64_t iConfirmedInner = 0;
};

constexpr RingLayout ComputeRetention(int64_t iHeadPhysical, int64_t iSnapshotCount, int64_t iConfirmedIndex)
{
	int64_t iHeadAdvance = std::max<int64_t>(0, iConfirmedIndex - engine::kiRenderBehindTicks);
	return
	{
		.iHead = SnapshotIndex(iHeadPhysical, iHeadAdvance),
		.iCount = iSnapshotCount - iHeadAdvance,
		.iConfirmedInner = iConfirmedIndex - iHeadAdvance,
	};
}

struct CoordScratch
{
	game::Frame* ppReplayStack[engine::kiNetworkBufferSize] {};
	int64_t iReplayStackCount = 0;
	int64_t iReplayWriteHead = 0;
	int64_t iLastValidatedIndex = -1;
	// Physical slot of a full state injected during this reconcile, or -1. ComputeOutputLayout bases
	// the output ring on it: it is the head and confirmed frame when validation reaches no later
	// replayed frame, and retention never goes below it when one is validated.
	int64_t iInjectedBaseSlot = -1;
	int64_t iNewConfirmedTick = -1;
	RingLayout outputLayout;
	common::Flags<ReconcileScratchFlags> flags;
	int64_t iPreReconcileTailTick = -1;
	ReconcileProfiling profiling;

	int64_t iDesyncTick = -1;
	common::crc_t desyncExpectedCrc = 0;
	common::crc_t desyncActualCrc = 0;
	std::unique_ptr<game::Frame> pDesyncClientFrame;

	// Restart the replay stack at a base frame; replay writes begin in the slot after iBaseSlot.
	void ResetReplayBase(game::Frame* pBaseFrame, int64_t iBaseSlot)
	{
		ppReplayStack[0] = pBaseFrame;
		iReplayStackCount = 1;
		iReplayWriteHead = SnapshotIndex(iBaseSlot, 1);
	}
};

struct CoordWork
{
	engine::GridCoord coord {};
	engine::Cell* pCell = nullptr;
	CoordScratch scratch;
};

struct CrcFastPathCoordResult
{
	bool bHandled = true;
	RingLayout preWritebackLayout;
	// Set only when bHandled == false. The lowest tick > post-walk iConfirmedTick whose ring
	// frame uiSharedCrc did not match its server update — used by ReconcileCoord to shrink the
	// full-replay rollback window. -1 when no mismatch is pending (e.g., fast path was blocked
	// by a missing snapshot rather than a CRC failure).
	int64_t iLowestUnresolvedMismatch = -1;
};

inline engine::Cell::PendingFullState* FindDuePendingFullState(engine::Cell& rCell, int64_t iTargetTick)
{
	if (!rCell.optionalPendingFullState || rCell.optionalPendingFullState->iTick > iTargetTick)
	{
		return nullptr;
	}
	return &*rCell.optionalPendingFullState;
}

int64_t FindSnapshotIndex(const engine::Cell& rCell, int64_t iTick);
CrcFastPathCoordResult CrcFastPathProcessCoord(CoordWork& rWork, int64_t iTargetTick);

void ReconcileInjectPendingFullState(CoordWork& rWork, engine::Cell::PendingFullState& rPending);

void ReconcileRollbackCoord(CoordWork& rWork, int64_t iRollbackOffset);
int64_t ReconcileFindReplayRangeCoord(const CoordWork& rWork, int64_t iReplayStart);
void ReconcileReplayCoord(CoordWork& rWork, int64_t iReplayStart, int64_t iMaximumConsecutive);
void ReconcileCatchUpCoord(CoordWork& rWork, int64_t iTargetTick);
void ReconcileFastPathCatchUp(CoordWork& rWork, int64_t iTargetTick);

struct ReconcileDesyncInfo
{
	bool bDesync = false;
	int64_t iDesyncTick = -1;
	GridCoord desyncCoord {};
	common::crc_t desyncExpectedCrc = 0;
	common::crc_t desyncActualCrc = 0;
	std::unique_ptr<game::Frame> pDesyncClientFrame;
};

struct ReconcileDispatchResult
{
	std::span<const CoordWork> activeWorks;
	bool bAnyFullReplay = false;
	ReconcileProfiling profiling;
	// The first coord in work-list order whose reconcile ended in an unresolved CRC mismatch; bDesync is false when none
	// did. The work list follows cell-map iteration order, which is unspecified.
	ReconcileDesyncInfo desync;
};

class ReconcileDispatcher
{
public:

	ReconcileDispatchResult Run(const ReconcileInputs& rInputs);
	void Reset();

private:

	std::vector<CoordWork> mWorks;
};

} // namespace engine

#endif // BT_CLIENT
