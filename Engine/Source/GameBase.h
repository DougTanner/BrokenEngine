#pragma once

#include "Frame/CellStaticData.h"

// game::FrameInput is the mapped type of mFrameInputs below, so the complete definition is required here.
#include "Frame/FrameInput.h"

#if defined(BT_CLIENT)
#include "Frame/Frame.h"
#include "Ui/ChatLog.h"
#endif
#if defined(BT_SERVER)
#include "File/ServerSaveLoad.h"
#endif

namespace game
{

struct Frame;
struct FrameInput;
struct StatusChange;

} // namespace game

namespace engine
{

#if defined(BT_SERVER)
// File/Replay.h names game types, so it stays out of Engine.h and out of this header; GameBase.cpp includes it.
class Replay;
struct ActiveFrameReference;
#endif // BT_SERVER

enum class MenuFlags : uint64_t
{
	kMouseVisible = 0x01,
};
using MenuFlags_t = common::Flags<MenuFlags>;

enum class UiState
{
	kNone,

	kGameSettings,
	kGraphicsSettings,
	kModal,
	kPause,
	kSound,
};

enum class GameFlags : uint64_t
{
	kQuit                 = 0x01,
	kSaveReplay           = 0x02,
	kLoadReplay           = 0x04,
	kMainMenu             = 0x08,
	kPaused               = 0x20,
};
using GameFlags_t = common::Flags<GameFlags>;

#if defined(BT_CLIENT)
constexpr int64_t SnapshotIndex(int64_t iHead, int64_t iLogical)
{
	return (iHead + iLogical) % kiNetworkBufferSize;
}
#endif // BT_CLIENT

struct Cell
{
	CellStaticData staticData;

#if defined(BT_SERVER)
	std::unique_ptr<game::Frame> pCurrent;
	std::unique_ptr<game::Frame> pNext;
#endif

#if defined(BT_CLIENT)
	std::unique_ptr<game::Frame> ppSnapshots[kiNetworkBufferSize] {};
	int64_t iSnapshotHead = 0;       // physical index of oldest entry
	int64_t iSnapshotCount = 0;      // number of valid entries in ring

	// iConfirmedTick is the validated simulation tick; iConfirmedOffset is its logical offset from the ring head (-1 = none).
	int64_t iConfirmedTick = -1;
	int64_t iConfirmedOffset = -1;

	// Monotonic high-water mark: highest tick whose CRC has matched the server. Re-simulating
	// any tick <= this value is an invariant violation and trips DEBUG_BREAK in ReconcileRunTickCoord.
	int64_t iHighWaterValidatedTick = -1;

	// Monotonic guard for the rendered frame: the renderer must never regress to an older tick
	// for the same coord. Trips DEBUG_BREAK in UpdateRenderInterpolation().
	int64_t iLastRenderedTick = -1;

	struct CoordServerUpdate
	{
		common::crc_t uiSharedCrc = 0;
		std::vector<game::StatusChange> statusChanges;
	};
	std::unordered_map<int64_t, CoordServerUpdate> serverUpdates;

	struct PendingFullState
	{
		int64_t iTick = -1;
		std::unique_ptr<game::Frame> pFrame;
	};
	std::optional<PendingFullState> optionalPendingFullState;
	int64_t iLastFullStateTick = -1;

	// Log deduplication: suppress repeated mismatch logging when reconcile is stuck on same desync
	int64_t iLastLoggedConfirmedTick = -1;
	int64_t iLastLoggedFirstMismatch = -1;
	int64_t iStuckFrameCount = 0;
	static constexpr int64_t kiStuckLogInterval = 64;

	// Per-coord log cooldowns: prevent redundant detail logging across multiple Run() calls
	// within the same render frame (multiple ClientUpdates can fire at the same or adjacent ticks)
	int64_t iLastMismatchDetailLogTick = -1;
	int64_t iLastSpawnTransferLogTick = -1;
	static constexpr int64_t kiMismatchDetailLogCooldown = 32;

	// Repeated-work guard: detect if full replay is entered with identical state as last attempt.
	// Set when entering full replay; checked on next entry to trip DEBUG_BREAK.
	int64_t iLastReplayConfirmedTick = -1;
	int64_t iLastReplayServerUpdateCount = -1;

	const game::Frame* TailFrame() const
	{
		if (iSnapshotCount == 0)
		{
			return nullptr;
		}
		return ppSnapshots[SnapshotIndex(iSnapshotHead, iSnapshotCount - 1)].get();
	}

	void ResetClientState()
	{
		iConfirmedTick = -1;
		iConfirmedOffset = -1;
		iHighWaterValidatedTick = -1;
		iLastRenderedTick = -1;
		iSnapshotHead = 0;
		iSnapshotCount = 0;
		serverUpdates.clear();
		optionalPendingFullState.reset();
		iLastFullStateTick = -1;
		iLastLoggedConfirmedTick = -1;
		iLastLoggedFirstMismatch = -1;
		iStuckFrameCount = 0;
		iLastMismatchDetailLogTick = -1;
		iLastSpawnTransferLogTick = -1;
		iLastReplayConfirmedTick = -1;
		iLastReplayServerUpdateCount = -1;
	}
#endif // BT_CLIENT
};

#if defined(BT_CLIENT)
// Number of committed ticks the renderer stays behind the ring tail. Render interpolates across
// the one-tick window starting kiRenderBehindTicks behind tail (e.g. 2 = [tail-2, tail-1], holding
// tail as an extra committed tick of starvation cushion), so every rendered position lies between
// two already-simulated ticks and velocity changes never require extrapolation. Raise this to
// absorb more jitter at the cost of visual latency (31.25 ms per tick at 32 Hz). Drives the
// reconcile retention floor (ComputeRetention, applied by the CRC fast path and full replay) and
// the source index in GameBase::RenderFrame — change here and the reconcile/render pair move together.
inline constexpr int64_t kiRenderBehindTicks = 2;

// Standard-menu contract. The engine owns the six standard menu screens (Ui/Screens); this is the
// narrow surface they cannot read for themselves: which optional entries the game offers, the live
// client/discovery state behind the Main Menu, and the game-owned side effects.
enum class StandardMenuFeature : uint8_t
{
	kLocalServer  = 0x01,
	kRemoteServer = 0x02,
};
using StandardMenuFeature_t = common::Flags<StandardMenuFeature>;

struct StandardMenuModel
{
	// Must refer to storage that outlives the render pass; a workbuffer allocation would dangle.
	const char* pcTitle = nullptr;
	StandardMenuFeature_t features;
	StandardMenuState_t state;
};

enum class StandardMenuAction
{
	kStartDiscovery,
	kConnectToDiscoveredServer,
	kChangeFrameToMainMenu,
};
#endif // BT_CLIENT

class GameBase
{
public:

	GameBase();
	~GameBase();

#if defined(BT_CLIENT)
	bool ShouldUseCrosshair() const;

	void ProcessInput(bool bLostFocus);
	void ClientUpdate();
	void Render();
	float AdvanceRenderClock(double fWindowStartTime, bool bPaused, bool bHaveInterpolationWindow, double fSimulationDeltaSeconds);
	game::Frame& RenderFrame(GridCoord coordinate) const;
	void ResetRenderClock();
#endif // BT_CLIENT
#if defined(BT_SERVER)
	void ServerUpdate();
	void EnsureNextFrames();
#endif // BT_SERVER

	void ResetSession();
	void CreateNewFrame();
	void ChangeFrameToMainMenu();

	static void BuildMenuIslandPlacement(int64_t iIndex, std::vector<IslandPlacement>& rOutput);
	// Debug-only main-menu island browser index into gpIslandTerrain->mIslandCrcsByArea (largest footprint first);
	// advanced by 'E'.
	int64_t miMenuIslandIndex = 0;

	GameFlags_t mGameFlags;
	UiState meUiState = UiState::kPause;
	char mpcModalMessage[256] = {};
	bool mbShowImGui = false;
#if defined(BT_CLIENT)
	// Stored here rather than in ChatScreen so an ImGuiManager recreation keeps the history and an open input box.
	ChatLog mChatLog;
#endif // BT_CLIENT

	TimeStep mTimeStep;
	// Sim-clock delta covered by the most recent ServerUpdate / ClientUpdate iteration.
	// Equals iFullTicks * kfDeltaTime — zero during pause, scales with mTimeStep multiplier.
	// For per-tick systems (those running inside the for-iFullTicks loop), keep using kfDeltaTime.
	// For systems outside the per-tick loop that run once per advancing Update and represent
	// sim-time progression (e.g. game timers advanced once per update), use this to respect
	// time-scaling without each one re-deriving the delta.
	float mfLastDeltaTime = 0.0f;
#if defined(BT_CLIENT)
	// Sim-scaled seconds elapsed since the previous render frame: wall delta multiplied by the active time
	// ratio (equal to wall time at ratio 1.0). Cached here so render-rate consumers share the render clock's
	// delta; camera blend / shake decay / mfTime and visual-error-offset decay use the same timing units.
	double mfLastRenderFrameSeconds = 0.0;
#endif // BT_CLIENT
	std::unordered_map<GridCoord, Cell> mCells;
	// Awake cells for this update: the coords simulated, published, and rendered. Populated by the session runtimes
	// (ClientSessionRuntime::ComputeActiveSet on the client, ServerSessionRuntime::ComputeActiveSet on the server); the
	// append order is observable and reaches dispatch, broadcast, transfer, and render. Membership does not imply a
	// client is watching a cell, so liveness checks must not be derived from it.
	std::vector<GridCoord> mActiveCoordinates;
	// The client session runtime caches the visible-neighbor ring keyed off this coord, so every write goes through
	// SetClientGridCoordinate, which invalidates that cache.
	GridCoord mClientGridCoordinate {};
	void SetClientGridCoordinate(GridCoord coordinate);

	std::unordered_map<GridCoord, game::FrameInput> mFrameInputs;

	// Creates the cell's frame storage and static data on demand; the game supplies the frame's contents.
	void CreateCellAtCoordinate(GridCoord coordinate);

#if defined(BT_SERVER)
	// True while replay playback owns the simulation, i.e. a reader is live or staged for a later tick.
	// engine::Replay owns the replay streams and republishes this whenever its reader sets change.
	bool mbReplaying = false;

	// Owns gpReplay for this game's lifetime; every user reaches it through that global.
	std::unique_ptr<Replay> mpReplay;

	ServerSaveLoad mServerSaveLoad;

	// Reusable server-side dispatch references; capacity persists across ticks as the active-cell count changes.
	std::vector<ActiveFrameReference> mActiveFrameReferences;
#endif // BT_SERVER

#if defined(BT_CLIENT)
	// Per-coord render interpolation. Render() and the Main.cpp boot path own its lifecycle and forward
	// it by const ref into Graphics::RenderMainPresentAcquire (Graphics never touches it). Kept here so
	// storage and lifecycle share one object; the Frame/Frame.h include above supplies the complete type.
	std::unordered_map<GridCoord, game::FrameInterpolate> mRenderInterpolates;

	// Frame-head skip check shared by Render() and the Main.cpp boot render loop; true means skip this frame.
	bool HandleDeferredSwapchain();

private:

	void UpdateActiveIslands();
	bool IsCoordinateRenderable(GridCoord coordinate) const;
	void SelectRenderCamera(const std::vector<GridCoord>& rActiveCoordinates, GridCoord& rCameraCoordinate, bool& rbHaveRenderableCamera) const;
	void UpdateRenderInterpolation(const std::vector<GridCoord>& rActiveCoordinates, GridCoord cameraCoordinate, bool bHaveRenderableCamera);
#endif // BT_CLIENT

protected:

	void PrepareActiveSet();
#if defined(BT_SERVER)
	void RefreshReplayActiveSet();
	void SwapFrames();
	void BuildAndDispatchFrameTicks(const std::vector<GridCoord>& rActiveCoordinates);
	void FinalizeFrameTick();
#endif // BT_SERVER

public:
	int64_t miTickCounter = 0;

	uint16_t muiNextFrameId = 0;

	int64_t miNextGlobalId = 1;
protected:

	MenuFlags_t mMenuFlags {MenuFlags::kMouseVisible};

#if defined(BT_CLIENT)
	// Render clock advances in simulation seconds, with wall time scaled by the current time ratio, and is clamped to the one-tick window starting at RenderFrame's source time. A midpoint seed sets phase; rebasing occurs only outside [source - kfDeltaTime, source + 2 * kfDeltaTime]. Single-tick commits preserve continuity through the Update(N, kfDeltaTime) == Update(N+1, 0) interpolation invariant.
	double mfRenderTime = 0.0;
	bool mbRenderClockSeeded = false;
	common::Timer mRenderTimer;

	// Minimized-render-loop throttle (HandleDeferredSwapchain()'s swapchain-recreate-deferred skip branch). While the
	// recreate stays deferred the branch early-returns past vkQueuePresentKHR — the client loop's only
	// vsync throttle — so a high-resolution waitable timer paces the skip loop to one sim tick's cadence.
	// Lazily created on first use, closed in ~GameBase. mOptionalMinimizedThrottleLast is the previous skip-branch
	// timestamp (empty = first iteration after normal rendering resumed → wait skipped).
	HANDLE mHandleMinimizedThrottleTimer = nullptr;
	std::optional<std::chrono::steady_clock::time_point> mOptionalMinimizedThrottleLast;
#endif // BT_CLIENT

private:

	game::Frame& CreateCellFrame(Cell& rCell, GridCoord coordinate);
};

} // namespace engine
