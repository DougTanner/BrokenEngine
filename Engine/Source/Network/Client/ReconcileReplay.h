#pragma once

#if defined(BT_CLIENT)

// The engine owns the client reconciliation replay chain: the CRC fast path, the past-frame ring and rollback
// orchestration, and the replay tick driver that applies transfers after each tick and recomputes the Frame CRC at the
// same comparison point the server uses. The game supplies the Frame/FrameInput/StatusChange payloads this machinery
// replays, the transfer classification and materialization it calls, and the client-state policy that consumes the
// per-coordinate scratch. Naming game types keeps this header out of the Engine.h aggregation; its consumers include
// it directly.

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
	kReSimOccurred      = 0x08,
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
	return {
		.iHead = SnapshotIndex(iHeadPhysical, iHeadAdvance),
		.iCount = iSnapshotCount - iHeadAdvance,
		.iConfirmedInner = iConfirmedIndex - iHeadAdvance,
	};
}

struct CoordScratch
{
	std::vector<game::Frame*> replayStack;
	int64_t iReplayStackCount = 0;
	int64_t iReplayWriteHead = 0;
	int64_t iReplayWriteCount = 0;
	int64_t iLastValidatedIndex = -1;
	// Physical slot of a full state injected during this reconcile, or -1. It is the authoritative
	// output-ring base until a later validated replay frame supersedes it.
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

	// Reset every field to its declared default while preserving replayStack's allocated capacity.
	void Reset()
	{
		replayStack.clear();
		iReplayStackCount = 0;
		iReplayWriteHead = 0;
		iReplayWriteCount = 0;
		iLastValidatedIndex = -1;
		iInjectedBaseSlot = -1;
		iNewConfirmedTick = -1;
		outputLayout = {};
		flags.ClearAll();
		iPreReconcileTailTick = -1;
		profiling = {};
		iDesyncTick = -1;
		desyncExpectedCrc = 0;
		desyncActualCrc = 0;
		pDesyncClientFrame.reset();
	}
};

struct CoordWork
{
	engine::GridCoord coord {};
	engine::CoordFrames* pFrames = nullptr;
	CoordScratch scratch;
};

struct CrcFastPathCoordResult
{
	bool bHandled = true;
	RingLayout preWritebackLayout;
	// Set only when bHandled == false. The lowest tick > post-walk iConfirmedTick whose ring
	// frame sharedCrc did not match its server update — used by ReconcileCoord to shrink the
	// full-replay rollback window. -1 when no mismatch is pending (e.g., fast path was blocked
	// by a missing snapshot rather than a CRC failure).
	int64_t iLowestUnresolvedMismatch = -1;
};

inline bool HasDuePendingFullState(const engine::CoordFrames& rFrames, int64_t iTargetTick)
{
	return rFrames.pendingFullState.has_value() && rFrames.pendingFullState->iTick <= iTargetTick;
}

CrcFastPathCoordResult CrcFastPathProcessCoord(CoordWork& rWork, int64_t iTargetTick);

void ReconcileCoord(CoordWork& rWork, const ReconcileInputs& rInputs);
void ReconcileInjectPendingFullState(CoordWork& rWork);

void ReconcileRollbackCoord(CoordWork& rWork, int64_t iRollbackOffset);
int64_t ReconcileFindReplayRangeCoord(CoordWork& rWork, int64_t iReplayStart);
void ReconcileReplayCoord(CoordWork& rWork, int64_t iReplayStart, int64_t iMaxConsecutive, float& rfTime);
void ReconcileCatchUpCoord(CoordWork& rWork, int64_t iTargetTick, float& rfTime);
void ReconcileFastPathCatchUp(CoordWork& rWork, int64_t iTargetTick);

struct ReconcileDispatchResult
{
	int64_t iActiveCount = 0;
	bool bAnyFullReplay = false;
	ReconcileProfiling profiling;
	// First coord in slot order whose reconcile ended in an unresolved CRC mismatch, or nullptr. It points
	// into the dispatcher's work list, so it stays valid only until the next Run() or Reset().
	CoordWork* pDesyncWork = nullptr;
};

struct ReconcileDesyncInfo
{
	bool bDesync = false;
	int64_t iDesyncTick = -1;
	GridCoord desyncCoord {};
	common::crc_t desyncExpectedCrc = 0;
	common::crc_t desyncActualCrc = 0;
	std::unique_ptr<game::Frame> pDesyncClientFrame;
};

// Owns the per-coord work list and runs one parallel reconcile pass over every eligible coord of a
// GameBase. The caller keeps the client-state, desync, and visual policy that consumes the result.
class ReconcileDispatcher
{
public:

	ReconcileDispatchResult Run(GameBase& rGameBase, const ReconcileInputs& rInputs);
	std::span<const CoordWork> ActiveWorks() const;
	void Reset();

private:

	std::vector<CoordWork> mWorks;
	int64_t miActiveCount = 0;
};

} // namespace engine

#endif // BT_CLIENT
