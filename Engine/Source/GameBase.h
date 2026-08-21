#pragma once

// game::FrameInput is the mapped type of mFrameInputs below, so the complete definition is required here.
#include "Frame/FrameInput.h"
#include "Frame/FrameStaticData.h"

#if defined(BT_CLIENT)
#include "Frame/Frame.h"
#include "Graphics/Camera.h"
#endif

namespace game
{

struct Frame;
struct FrameInput;
struct StatusChange;

}

namespace engine
{

#if defined(BT_CLIENT)
// Complete definitions come from Input/Input.h — included directly by GameBase.cpp and aggregated by Engine.h —
// so this public header stays independent of PCH include order.
struct MenuInput;
class InputPoll;
#endif // BT_CLIENT

#if defined(BT_SERVER)
// File/Replay.h names game types, so it stays out of Engine.h and out of this header; GameBase.cpp includes it.
class Replay;
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

	kTweaks,
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

struct CoordFrames
{
	FrameStaticData staticData;

#if defined(BT_SERVER)
	std::unique_ptr<game::Frame> pCurrent;
	std::unique_ptr<game::Frame> pNext;
#endif

#if defined(BT_CLIENT)
	std::unique_ptr<game::Frame> snapshots[kiNetworkBufferSize] {};
	int64_t iSnapshotHead = 0;       // physical index of oldest entry
	int64_t iSnapshotCount = 0;      // number of valid entries in ring

	// Confirmed tick = logical offset from head (-1 = none)
	int64_t iConfirmedTick = -1;
	int64_t iConfirmedOffset = -1;

	// Monotonic high-water mark: highest tick whose CRC has matched the server. Re-simulating
	// any tick <= this value is an invariant violation and trips DEBUG_BREAK in ReconcileRunTickCoord.
	int64_t iHighWaterValidatedTick = -1;

	// Monotonic guard for the rendered frame: the renderer must never regress to an older tick
	// or an earlier interpolated time for the same coord. Trips DEBUG_BREAK in Render().
	int64_t iLastRenderedTick = -1;
	float fLastRenderedTime = 0.0f;

	struct CoordServerUpdate
	{
		common::crc_t sharedCrc = 0;
		std::vector<game::StatusChange> statusChanges;
	};
	std::map<int64_t, CoordServerUpdate> serverUpdates;

	struct PendingFullState
	{
		int64_t iTick = -1;
		std::unique_ptr<game::Frame> pFrame;
	};
	std::optional<PendingFullState> pendingFullState;
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

	void ResetClientState()
	{
		iConfirmedTick = -1;
		iConfirmedOffset = -1;
		iHighWaterValidatedTick = -1;
		iLastRenderedTick = -1;
		fLastRenderedTime = 0.0f;
		iSnapshotHead = 0;
		iSnapshotCount = 0;
		serverUpdates.clear();
		pendingFullState.reset();
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
constexpr int64_t SnapshotIndex(int64_t iHead, int64_t iLogical)
{
	return (iHead + iLogical) % kiNetworkBufferSize;
}

// Number of committed ticks the renderer stays behind the ring tail. Render interpolates across
// the one-tick window starting kiRenderBehindTicks behind tail (e.g. 2 = [tail-2, tail-1], holding
// tail as an extra committed tick of starvation cushion), so every rendered position lies between
// two already-simulated ticks and velocity changes never require extrapolation. Raise this to
// absorb more jitter at the cost of visual latency (31.25 ms per tick at 32 Hz). Drives the
// retention floor in ReconcileReplayCrc's fast-path and the source index in
// GameBase::RenderFrame — change here and the reconcile/render pair move together.
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

enum class StandardMenuState : uint8_t
{
	kClientPresent           = 0x01,
	kDiscoveryScannerPresent = 0x02,
	kServerDiscovered        = 0x04,
	kConnectionAccepted      = 0x08,
	kAutoConnect             = 0x10,
};
using StandardMenuState_t = common::Flags<StandardMenuState>;

struct StandardMenuModel
{
	// Must refer to storage that outlives the render pass; a workbuffer allocation would dangle.
	const char* pcTitle;
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
	virtual ~GameBase();

#if defined(BT_CLIENT)
	virtual bool ShouldTrapCursor() = 0;
	virtual bool ShouldUseCrosshair() = 0;
	virtual bool ShouldShowInGameUi() = 0;
	virtual void ProcessGameMenuInput(const MenuInput& rMenuInput, const InputPoll& rInputPoll) = 0;

	// The standard menu screens read this every render pass, and again after any action that can
	// change it, because an action's effect must be visible to later decisions in the same pass.
	virtual StandardMenuModel GetStandardMenuModel() const = 0;
	virtual void ApplyStandardMenuAction(StandardMenuAction eAction) = 0;

	void ProcessInput(bool bLostFocus);
	void ClientUpdate();
	void Render();
	float AdvanceRenderClock(double dT, bool bPaused, bool bHaveInterpolationWindow, double dSimDeltaSeconds);
	game::Frame& RenderFrame(GridCoord coord) const;
	void ResetRenderClock();
#endif // BT_CLIENT
#if defined(BT_SERVER)
	void ServerUpdate();
#endif // BT_SERVER

	uint16_t GenerateFrameId() { return muiNextFrameId++; }
	int64_t TickCounter() const { return miTickCounter; }
	float CurrentTime() const { return mfCurrentTime; }
	void SetTickCounter(int64_t iTickCounter) { ASSERT(iTickCounter >= 0); miTickCounter = iTickCounter; }
	void SetCurrentTime(float fCurrentTime) { mfCurrentTime = fCurrentTime; }
	uint16_t NextFrameId() const { return muiNextFrameId; }
	void SetNextFrameId(uint16_t uiNextFrameId) { muiNextFrameId = uiNextFrameId; }
	int64_t GenerateGlobalId() { return miNextGlobalId++; }
	int64_t NextGlobalId() const { return miNextGlobalId; }
	void SetNextGlobalId(int64_t iNextGlobalId) { miNextGlobalId = iNextGlobalId; }

	bool InMainMenu() const { return mGameFlags & GameFlags::kMainMenu; }

#if defined(BT_SERVER)
	game::Frame& CurrentFrame(GridCoord coord) const
	{
		return *mCoordFrames.at(coord).pCurrent;
	}

	game::Frame& NextFrame(GridCoord coord)
	{
		return *mCoordFrames.at(coord).pNext;
	}
#endif // BT_SERVER

	GameFlags_t mGameFlags;
	UiState meUiState = UiState::kPause;
	char mModalMessage[256] = {};
	bool mbShowImGui = false;

	TimeStep mTimeStep;
	// Sim-clock delta covered by the most recent ServerUpdate / ClientUpdate iteration.
	// Equals iFullTicks * kfDeltaTime — zero during pause, scales with mTimeStep multiplier.
	// For per-tick systems (those running inside the for-iFullTicks loop), keep using kfDeltaTime.
	// For systems outside the per-tick loop that run once per advancing Update and represent
	// sim-time progression (e.g. ServerFleetManager::TickFleetTimers), use this to respect
	// time-scaling without each one re-deriving the delta.
	float mfLastDeltaTime = 0.0f;
#if defined(BT_CLIENT)
	// Sim-scaled seconds elapsed since the previous render frame: wall delta multiplied by the active time
	// ratio (equal to wall time at ratio 1.0). Cached here so render-rate consumers share the render clock's
	// delta; camera blend / shake decay / mfTime and visual-error-offset decay use the same timing units.
	double mfLastRenderFrameSeconds = 0.0;
#endif // BT_CLIENT
	std::unordered_map<GridCoord, CoordFrames> mCoordFrames;
	// Awake cells for this update: the coords simulated, published, and rendered. Populated by game policy
	// (Game::ComputeActiveSet on the client, ServerSessionRuntime::ComputeActiveSet on the server); the append
	// order is observable and reaches dispatch, broadcast, transfer, and render. Membership does not imply a
	// client is watching a cell, so liveness checks must not be derived from it.
	std::vector<GridCoord> mActiveCoords;
	// game::Game caches the visible-neighbor ring keyed off this coord, so every write goes through
	// Game::SetClientGridCoord, which invalidates that cache.
	GridCoord mClientGridCoord {};

	std::unordered_map<GridCoord, game::FrameInput> mFrameInputs;

	// Creates the cell's frame storage and static data on demand; the game supplies the frame's contents.
	void CreateFrameAtCoord(GridCoord coord);

#if defined(BT_SERVER)
	// True while replay playback owns the simulation, i.e. a reader is live or staged for a later tick.
	// engine::Replay owns the replay streams and republishes this whenever its reader sets change.
	bool mbReplaying = false;

	// Owns gpReplay for this game's lifetime; every user reaches it through that global.
	std::unique_ptr<Replay> mpReplay;
#endif // BT_SERVER

#if defined(BT_CLIENT)
	// Per-coord render interpolation. Render() and the Main.cpp boot path own its lifecycle and forward
	// it by const ref into Graphics::RenderMainPresentAcquire (Graphics never touches it). Kept here so
	// storage and lifecycle share one object; the Frame/Frame.h include above supplies the complete type.
	std::unordered_map<GridCoord, game::FrameInterpolate> mRenderInterpolates;

private:

	bool IsCoordRenderable(GridCoord coord) const;
	void SelectRenderCamera(const std::vector<GridCoord>& rActiveCoords, GridCoord& rCameraCoord, bool& rbHaveRenderableCamera) const;
	void UpdateRenderInterpolation(const std::vector<GridCoord>& rActiveCoords, GridCoord cameraCoord, bool bHaveRenderableCamera);
	bool HandleDeferredSwapchain();
#endif // BT_CLIENT

protected:

	void PrepareActiveSet();
#if defined(BT_SERVER)
	void RefreshReplayActiveSet();
	void SwapFrames();
	void BuildAndDispatchFrameTicks(const std::vector<GridCoord>& rActiveCoords);
	void FinalizeFrameTick();
#endif // BT_SERVER

	int64_t miTickCounter = 0;
	float mfCurrentTime = 0.0f;

	uint16_t muiNextFrameId = 0;
	int64_t miNextGlobalId = 1;

	MenuFlags_t mMenuFlags {MenuFlags::kMouseVisible};

#if defined(BT_CLIENT)
	// Render-side sim clock. Advances at sim rate (wall × current time ratio), clamped to
	// [T_prevTail, T_prevTail+kfDeltaTime] so fDeltaTime in [0, kfDeltaTime] maps to a true
	// interpolation window (prevTail -> tail) — never extrapolation past tail velocity. Sim
	// commits and render integration share the same time-ratio-scaled clock, so no separate
	// servo is needed; mfRenderTime simply integrates sim seconds and the clamps absorb
	// sub-tick jitter. Single-tick commits don't rebase: T advances +kfDt while mfRenderTime
	// stays continuous, so fDt drops by kfDt and the Update(N, kfDt) ≡ Update(N+1, 0)
	// invariant makes the handoff pixel-identical.
	double mfRenderTime = 0.0;
	bool mbRenderClockSeeded = false;
	common::Timer mRenderTimer;

	// Minimized-render-loop throttle (Render()'s swapchain-recreate-deferred skip branch). While the
	// recreate stays deferred the branch early-returns past vkQueuePresentKHR — the client loop's only
	// vsync throttle — so a high-resolution waitable timer paces the skip loop to one sim tick's cadence.
	// Lazily created on first use, closed in ~GameBase. mMinimizedThrottleLast is the previous skip-branch
	// timestamp (empty = first iteration after normal rendering resumed → wait skipped).
	HANDLE mMinimizedThrottleTimer = nullptr;
	std::optional<std::chrono::steady_clock::time_point> mMinimizedThrottleLast;
#endif // BT_CLIENT
};

} // namespace engine
