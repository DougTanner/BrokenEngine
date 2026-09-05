#include "GameBase.h"

#include "Game.h"
#if defined(BT_CLIENT)
#include "Network/Client/ClientSession.h"
#endif
#if defined(BT_SERVER)
#include "File/Replay.h"
#include "Network/Server/ServerSession.h"
#include "Network/Server/ServerTransferManager.h"
#endif
#include "Frame/FrameBase.h"
#include "Input/Input.h"
#include "Profile/ProfileManager.h"
#include "Ui/GraphicsSettingsWrappersBase.h"

namespace engine
{

GameBase::GameBase()
{
	game::FrameInterpolate::Register();

#if defined(BT_SERVER)
	mpReplay = std::make_unique<Replay>(*this);
#endif // BT_SERVER
}

GameBase::~GameBase()
{
#if defined(BT_CLIENT)
	if (mMinimizedThrottleTimer != nullptr)
	{
		CloseHandle(mMinimizedThrottleTimer);
	}
#endif // BT_CLIENT
}

#if defined(BT_CLIENT)
// Fixed policy order: BeginPoll -> quit -> modal gate -> cursor -> pause/back-out -> engine toggles -> game
// callback -> CompletePoll. The modal gate skips everything after it except CompletePoll, so a modal frame still
// resolves quit and still advances the previous snapshot that the next frame's edge detection needs.
void GameBase::ProcessInput(bool bLostFocus)
{
	MenuInput menuInput {};
	InputPoll inputPoll = gpInput->BeginPoll(bLostFocus, meUiState != UiState::kNone, menuInput);

	// Escape quits only from the basic main menu; with a settings sub-menu open it backs out to the menu below instead
	if (menuInput.flags & MenuInputFlags::kQuit || (menuInput.flags & MenuInputFlags::kPauseMenu && (mGameFlags & GameFlags::kMainMenu) && meUiState == UiState::kPause))
	{
		mGameFlags.Set(GameFlags::kQuit);
	}

	if (meUiState == UiState::kModal)
	{
		gpInput->CompletePoll();
		return;
	}

	if (menuInput.bGamepad && mMenuFlags & MenuFlags::kMouseVisible)
	{
		ShowCursor(FALSE);
		mMenuFlags.Clear(MenuFlags::kMouseVisible);
	}
	else if (!menuInput.bGamepad && !(mMenuFlags & MenuFlags::kMouseVisible))
	{
		ShowCursor(TRUE);
		mMenuFlags.Set(MenuFlags::kMouseVisible);
	}

	if (menuInput.flags & MenuInputFlags::kPauseMenu) [[unlikely]]
	{
		if (meUiState == UiState::kNone || meUiState == UiState::kGameSettings || meUiState == UiState::kGraphicsSettings || meUiState == UiState::kSound)
		{
			meUiState = UiState::kPause;
		}
		else if (!(mGameFlags & GameFlags::kMainMenu))
		{
			meUiState = UiState::kNone;
		}
	}

	if (menuInput.flags & MenuInputFlags::kToggleFullscreen)
	{
		gFullscreen.Toggle();
	}

	if constexpr (kbProfiling)
	{
		if (menuInput.flags & MenuInputFlags::kToggleProfileText)
		{
			gpProfileManager->ToggleProfileText();
		}
	}

	if constexpr (kbDebugRender)
	{
		if (menuInput.flags & MenuInputFlags::kToggleDebugRender)
		{
			DebugRender::Toggle();
		}
	}

	if constexpr (kbDebugInput)
	{
		if (menuInput.flags & MenuInputFlags::kMenuTweaks)
		{
			mbShowImGui = !mbShowImGui;
		}

		if (menuInput.flags & MenuInputFlags::kMenuDebugTexture)
		{
			gDebugTexture.Toggle();
		}
		if (menuInput.flags & MenuInputFlags::kDebugTextureNext)
		{
			float fNext = gDebugTextureIndex.Get() + 1.0f;
			if (fNext >= static_cast<float>(gpTextureManager->mRenderTargetTextures.miDebugTextureCount))
			{
				fNext = 0.0f;
			}
			gDebugTextureIndex.Set(fNext);
		}
		if (menuInput.flags & MenuInputFlags::kDebugTexturePrev)
		{
			float fPrev = gDebugTextureIndex.Get() - 1.0f;
			if (fPrev < 0.0f)
			{
				fPrev = static_cast<float>(gpTextureManager->mRenderTargetTextures.miDebugTextureCount - 1);
			}
			gDebugTextureIndex.Set(fPrev);
		}

		// Runs before the game callback's timespeed request packets, which is safe because mbTimeScaleChanged is
		// set only where an applied timespeed change lands, never by sending the request.
		if (mTimeStep.mbTimeScaleChanged)
		{
			mTimeStep.mbTimeScaleChanged = false;
			LOG(kNetwork, kWarning, "Timespeed changed Multiply: {} Divide: {}", mTimeStep.miTimeMultiply, mTimeStep.miTimeDivide);

			if (mTimeStep.miTimeMultiply == 1 && mTimeStep.miTimeDivide == 1)
			{
				gpImGuiManager->UpdateTextArea(kTextDebug, "");
			}
			else
			{
				common::ScopedWorkbufferArena scopedWorkbufferArena = common::gpThreadLocal->mWorkbuffer.Push();
				if (mTimeStep.miTimeMultiply > 1)
				{
					common::gpThreadLocal->mWorkbuffer.Append("Time ratio: ");
					common::gpThreadLocal->mWorkbuffer.Append(mTimeStep.miTimeMultiply);
					common::gpThreadLocal->mWorkbuffer.Append("x");
				}
				else
				{
					common::gpThreadLocal->mWorkbuffer.Append("Time ratio: 1/");
					common::gpThreadLocal->mWorkbuffer.Append(mTimeStep.miTimeDivide);
					common::gpThreadLocal->mWorkbuffer.Append("x");
				}
				gpImGuiManager->UpdateTextArea(kTextDebug, common::gpThreadLocal->mWorkbuffer.View());
			}
		}

		// Local pause state flips here so the callback's pause packet reports the state the client just entered.
		if (menuInput.flags & MenuInputFlags::kTogglePauseFrame)
		{
			mGameFlags.Toggle(GameFlags::kPaused);
			if (mGameFlags & GameFlags::kPaused)
			{
				gpImGuiManager->UpdateTextArea(kTextDebug, "PAUSED");
			}
			else
			{
				gpImGuiManager->UpdateTextArea(kTextDebug, "");
			}
		}
	}

	if constexpr (kbScreenshots)
	{
		// F9 toggles continuous dev capture, queuing a default one-shot request each frame when the request slot is
		// empty; an empty path uses the default %TEMP% location.
		static bool sbContinuousScreenshots = false;
		if (menuInput.flags & MenuInputFlags::kToggleScreenshots)
		{
			sbContinuousScreenshots = !sbContinuousScreenshots;
		}
		if (sbContinuousScreenshots && !gpGraphics->mScreenshotRequest)
		{
			// Fill only when empty so the per-frame F9 default request can't overwrite an agent-issued request's
			// parameters (path / maxWidth / bPublishResult) before the capture site consumes it.
			gpGraphics->mScreenshotRequest = ScreenshotRequest {};
		}
	}

	ProcessGameMenuInput(menuInput, inputPoll);

	gpInput->CompletePoll();
}
#endif // BT_CLIENT

#if defined(BT_CLIENT)
void GameBase::ClientUpdate()
{
	common::LogTickScope logTickScope(miTickCounter);

	{
		// Heap: transport receive buffers and game packet/frame adoption
		ScopedSuppressAllocationTracking suppress;
		NetworkTimeState networkTimeState {
			.bFastForward = mTimeStep.miTimeMultiply > 1,
			.iExpectedUpdateIntervalMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(mTimeStep.SimToWall(game::NetworkSessionContract::kTickDuration)).count(),
			.iExpectedUpdatesPerSecond = kiTickRate * mTimeStep.miTimeMultiply / mTimeStep.miTimeDivide,
		};
		game::gpClientSession->mpRuntime->PollAndDrain(networkTimeState);
	}

	if (game::gpClientSession->mpDesyncCore->IsStalled())
	{
		return;
	}

	int64_t iFullTicks = mTimeStep.TickRealtime();
	if (mGameFlags & GameFlags::kPaused) [[unlikely]]
	{
		mTimeStep.ClearAccumulator();
		iFullTicks = 0;
	}

	game::gpClientSession->UpdateDesiredCoords(game::SubscriptionChangeReason::kPollTick);
	game::gpClientSession->UpdateSubscriptions();
	PrepareActiveSet();

	// Hard ceiling: clock-servo target + kiSimCeilingSlackTicks. EvaluateClock steers the
	// sim toward the bare target, so this clamp engages only on genuine arrival stalls (loss bursts),
	// not per-packet jitter, while StatusChanges still normally arrive before their tick simulates.
	// Extreme "sim way behind target" is handled by the snap path in Reconcile.
	const engine::ClientSessionRuntime& rRuntime = *game::gpClientSession->mpRuntime;
	int64_t iCeiling = rRuntime.miLatestServerTick < 0 ? -1 : rRuntime.miLatestServerTick - rRuntime.miCurrentTargetBehind + engine::kiSimCeilingSlackTicks;
	int64_t iAbsorbedTicks = 0;
	if (iCeiling >= 0 && iFullTicks > 0)
	{
		int64_t iRoomToAdvance = std::max<int64_t>(0, iCeiling - miTickCounter);
		if (iFullTicks > iRoomToAdvance)
		{
			iAbsorbedTicks = iFullTicks - iRoomToAdvance;
			mTimeStep.AbsorbUnusedTicks(iAbsorbedTicks);
			iFullTicks = iRoomToAdvance;
		}
	}

	// When latestServerTick stalls (loss burst), the ceiling freezes the sim entirely, then releases
	// the backlog as a burst — log at the transition. Frequent stalls outside loss bursts indicate a
	// clock-servo / kiSimCeilingSlackTicks tuning problem (the servo should keep steady-state jitter
	// away from the ceiling).
	{
		static int64_t siCeilingStallFrames = 0;
		static int64_t siCeilingAbsorbedTicks = 0;
		if (iAbsorbedTicks > 0 && iFullTicks == 0)
		{
			++siCeilingStallFrames;
			siCeilingAbsorbedTicks += iAbsorbedTicks;
		}
		else if (iFullTicks > 0 && siCeilingStallFrames > 0)
		{
			siCeilingAbsorbedTicks += iAbsorbedTicks; // Release frame may itself be partially clamped
			LOG(kNetwork, kVerbose, "Sim ceiling stall ended StallFrames: {} AbsorbedTicks: {} BurstTicks: {} Ceiling: {} Tick: {}", siCeilingStallFrames, siCeilingAbsorbedTicks, iFullTicks, iCeiling, miTickCounter);
			siCeilingStallFrames = 0;
			siCeilingAbsorbedTicks = 0;
		}
	}

	// Set mfLastDeltaTime after the ceiling clamp so it records executed sim seconds (iFullTicks * kfDeltaTime), in
	// lockstep with miTickCounter and mfCurrentTime. PrepareActiveSet runs earlier and has no client-side reader of
	// this delta.
	mfLastDeltaTime = static_cast<float>(iFullTicks) * kfDeltaTime;

	miTickCounter += iFullTicks;
	mfCurrentTime += static_cast<float>(iFullTicks) * kfDeltaTime;

	gpProfileManager->CpuStart(game::kCpuTimerFrameUpdate);
	game::gpClientSession->Reconcile();
	gpProfileManager->CpuStop(game::kCpuTimerFrameUpdate);

	if constexpr (kbProfiling)
	{
		gpProfileManager->mFullUpdatesInTheLastSecond.Set(iFullTicks);
	}
}
#endif // BT_CLIENT

#if defined(BT_SERVER)
void GameBase::ServerUpdate()
{
	common::LogTickScope logTickScope(miTickCounter);

	NetworkTimeState networkTimeState {
		.bFastForward = mTimeStep.miTimeMultiply > 1,
		.iExpectedUpdateIntervalMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(mTimeStep.SimToWall(game::NetworkSessionContract::kTickDuration)).count(),
		.iExpectedUpdatesPerSecond = kiTickRate * mTimeStep.miTimeMultiply / mTimeStep.miTimeDivide,
	};
	game::gpServerSession->mpRuntime->Poll(networkTimeState);

	// Drain agent commands with the debug-control packets, before SaveLoadReplay/PrepareActiveSet so
	// flag-setting commands are consumed the same update and injected StatusChanges enter the broadcast snapshot.
	if (gpAgentCommandServer != nullptr) [[unlikely]]
	{
		gpAgentCommandServer->Drain();
	}

	gpReplay->SaveLoadReplay();

	game::gpServerSession->mpRuntime->WaitForTick(mTimeStep);

	int64_t iFullTicks = mTimeStep.TickRealtime();
	// Second poll of the update: drains commands that arrived during WaitForTick so they enter the imminent
	// tick rather than the next one. It runs before the pause/timespeed decision below, so a pause request
	// arriving in the boundary window (Debug builds only, kbDebugInput) prevents the imminent tick. A
	// save/load-replay request arriving here is serviced next update by design -- SaveLoadReplay already ran.
	game::gpServerSession->mpRuntime->PollTickBoundary(networkTimeState);
	bool bAcceptRawCpuTimers = false;
	if constexpr (kbProfiling)
	{
		bAcceptRawCpuTimers = iFullTicks == 1 &&
			mTimeStep.miTimeMultiply == 1 &&
			mTimeStep.miTimeDivide == 1 &&
			!(mGameFlags & GameFlags::kPaused) &&
			!gpReplay->IsRecording() &&
			!mbReplaying &&
			!(mGameFlags & GameFlags::kSaveReplay);
	}
	gpServer->BroadcastTimespeedIfChanged();
	if (mGameFlags & GameFlags::kPaused) [[unlikely]]
	{
		mTimeStep.ClearAccumulator();
		iFullTicks = 0;
	}
	else if (iFullTicks != 1) [[unlikely]]
	{
		LOG(kDefault, kWarning, "ServerUpdate FullTicks: {} (expected 1)", iFullTicks);
	}
	int64_t iUnusedTicks = 0;
	if (gpReplay->mReplayTransferCaptureInfo.iPauseAfterWriterInputCount != -1 && iFullTicks > 1)
	{
		iUnusedTicks = iFullTicks - 1;
		iFullTicks = 1;
	}
	mfLastDeltaTime = static_cast<float>(iFullTicks) * kfDeltaTime;

	PrepareActiveSet();

	const std::vector<GridCoord>& rActiveCoords = mActiveCoords;

	gpProfileManager->CpuStart(game::kCpuTimerFrameUpdate);
	int64_t iFinalizedTicks = 0;
	bool bRawCpuTimersNoDispatchLatched = false;
	for (int64_t i = 0; i < iFullTicks; ++i)
	{
		const int64_t iPreviousTickCounter = miTickCounter;
		const float fPreviousCurrentTime = mfCurrentTime;
		++miTickCounter;
		mfCurrentTime += kfDeltaTime;

		common::LogTickScope perTickScope(miTickCounter);

		game::gpServerSession->PrepareTick();

		if (mbReplaying) [[unlikely]]
		{
			// Pick up readers activated by the previous iteration's SyncReplayTick, which the pre-loop PrepareActiveSet
			// only sees once per update. Must run before this iteration's SyncReplayTick: the recording never simulated
			// a coord at its activation tick E, so it may not enter E's dispatch list — its first dispatch is E + 1.
			RefreshReplayActiveSet();
		}

		if (gpReplay->IsRecording() || mbReplaying || (mGameFlags & GameFlags::kSaveReplay)) [[unlikely]]
		{
			if (!gpReplay->SyncReplayTick())
			{
				if constexpr (kbProfiling)
				{
					gpProfileManager->LatchRawCpuTimers(false, miTickCounter);
					bRawCpuTimersNoDispatchLatched = true;
				}
				// The final replay reader retired before dispatch. Reload now so server display/network observers never
				// see an empty grid between updates, but start simulating the new loop on the next update.
				gpReplay->SaveLoadReplay();
				if (!mbReplaying)
				{
					// The reload failed after this iteration advanced the sim clock but before it finalized a frame.
					// Restore the exact pre-tick values so buffered-frame indexing remains contiguous.
					miTickCounter = iPreviousTickCounter;
					mfCurrentTime = fPreviousCurrentTime;
					iFullTicks = iFinalizedTicks;
					mfLastDeltaTime = static_cast<float>(iFinalizedTicks) * kfDeltaTime;
					// Rebuild only active frames; normal input/manager progression resumes on the next server update.
					game::gpGame->ComputeActiveSet();
					game::gpGame->EnsureNextFrames();
				}
				break;
			}
		}

		BuildAndDispatchFrameTicks(rActiveCoords);
#if defined(BT_SERVER)
		if constexpr (kbProfiling)
		{
			gpProfileManager->LatchRawCpuTimers(bAcceptRawCpuTimers, miTickCounter);
		}
#endif // BT_SERVER
		FinalizeFrameTick();
		++iFinalizedTicks;
		if (mGameFlags & GameFlags::kPaused) [[unlikely]]
		{
			mTimeStep.ClearAccumulator();
			iFullTicks = iFinalizedTicks;
			mfLastDeltaTime = static_cast<float>(iFinalizedTicks) * kfDeltaTime;
			break;
		}
	}
	if (iFullTicks == 0 && !bRawCpuTimersNoDispatchLatched)
	{
		if constexpr (kbProfiling)
		{
			gpProfileManager->LatchRawCpuTimers(false, miTickCounter);
		}
	}
	if (iUnusedTicks > 0 && !(mGameFlags & GameFlags::kPaused))
	{
		mTimeStep.AbsorbUnusedTicks(iUnusedTicks);
	}
	if (iFullTicks > 0)
	{
		game::gpServerSession->mpRuntime->CompleteUpdate(1, miTickCounter);
	}
	else
	{
		// Zero-tick update (paused, or an occasional clock/timescale remainder): service the persist-until-served
		// subscription/resync queues so a client can connect to a paused server and receive full state (the per-tick
		// CompleteTick consumers never run here).
		game::gpServerSession->mpRuntime->CompleteUpdate(0, miTickCounter);
	}
	gpProfileManager->CpuStop(game::kCpuTimerFrameUpdate, CpuStopFlags::kSmoothNow);

	if constexpr (kbProfiling)
	{
		gpProfileManager->mFullUpdatesInTheLastSecond.Set(iFullTicks);
	}

	game::gpGame->mGameSaveLoad.TickAutosave();
}
#endif // BT_SERVER

#if defined(BT_SERVER)
void GameBase::BuildAndDispatchFrameTicks(const std::vector<GridCoord>& rActiveCoords)
{
	const int64_t iActiveCount = static_cast<int64_t>(rActiveCoords.size());

	// Pre-resolve frame references to avoid repeated map lookups across all phases
	{
		// Heap: mActiveFrameRefs persists across ticks and may grow with the unbounded active-cell count;
		// retain its capacity while rebuilding the per-tick references.
		ScopedSuppressAllocationTracking suppress;
		mActiveFrameRefs.clear();
		mActiveFrameRefs.reserve(static_cast<size_t>(iActiveCount));
		for (int64_t j = 0; j < iActiveCount; ++j)
		{
			const GridCoord& rCoord = rActiveCoords[static_cast<size_t>(j)];
			CoordFrames& rFrames = mCoordFrames.at(rCoord);
			if (rFrames.pCurrent == nullptr || rFrames.pNext == nullptr)
			{
				LOG(kDefault, kWarning, "BuildDispatch NullFrame Coord: ({},{}) pCurrent: {} pNext: {}",
					rCoord.x, rCoord.y, rFrames.pCurrent != nullptr, rFrames.pNext != nullptr);
				continue;
			}
			mActiveFrameRefs.push_back({
				.pNext = &NextFrame(rCoord),
				.pCurrent = &CurrentFrame(rCoord),
				.pFrameInput = &mFrameInputs.at(rCoord),
				.pStaticData = &rFrames.staticData,
			});
		}
	}
	const std::vector<ActiveFrameRef>& rActiveFrameRefs = mActiveFrameRefs;

	gpProfileManager->CpuStart(game::kCpuTimerFrameInterpolate);
	gpProfileManager->CpuStart(game::kCpuTimerFramePostRender);

	const int64_t iFrameRefCount = static_cast<int64_t>(rActiveFrameRefs.size());
	auto processRange = [&](int64_t iBegin, int64_t iEnd)
	{
		for (int64_t j = iBegin; j < iEnd; ++j)
		{
			RunFrameTick(rActiveFrameRefs.at(static_cast<size_t>(j)), miTickCounter, mfCurrentTime);
		}
	};
	if constexpr (kbFrameDispatch)
	{
		common::gpMultithreading->Dispatch(iFrameRefCount, processRange);
	}
	else
	{
		processRange(0, iFrameRefCount);
	}

	gpProfileManager->CpuStop(game::kCpuTimerFramePostRender);
	gpProfileManager->CpuStop(game::kCpuTimerFrameInterpolate);
}

void GameBase::FinalizeFrameTick()
{
	// Transfer entities that crossed frame boundaries into destination frames
	if (!mbReplaying)
	{
		game::gpGame->HarvestTransfers();
	}
	else
	{
		game::gpServerSession->mpTransferManager->ApplyReplayTransfers();
	}

	SwapFrames();

	game::gpServerSession->mpRuntime->CompleteTick(miTickCounter);

	for (auto& [rCoord, rFrameInput] : mFrameInputs)
	{
		rFrameInput.statusChanges.clear();
	}
}
#endif // BT_SERVER

#if defined(BT_CLIENT)
game::Frame& GameBase::RenderFrame(GridCoord coord) const
{
	// Returns the frame kiRenderBehindTicks slots behind tail when possible so the one-tick render
	// window spans (source -> source+1) — a true interpolation between two committed ticks, with
	// the newer committed ticks up to tail held as starvation cushion. When the ring hasn't
	// populated that many slots yet (cold start or a replay/rollback that didn't apply retention),
	// fall back to the oldest available; callers force fDeltaTime = 0 for that coord so no
	// extrapolation occurs.
	const CoordFrames& rFrames = mCoordFrames.at(coord);
	ASSERT(rFrames.iSnapshotCount > 0);
	int64_t iDesiredLogical = rFrames.iSnapshotCount - 1 - kiRenderBehindTicks;
	int64_t iLogical = std::max<int64_t>(0, iDesiredLogical);
	int64_t iPhysical = SnapshotIndex(rFrames.iSnapshotHead, iLogical);
	ASSERT(rFrames.snapshots[iPhysical] != nullptr);
	return *rFrames.snapshots[iPhysical];
}

void GameBase::ResetRenderClock()
{
	mfRenderTime = 0.0;
	mbRenderClockSeeded = false;
	mRenderTimer.Reset();
}

// Render-side sim clock advance. Integrates mfRenderTime (sim seconds) and returns fDeltaTime in [0, kfDt], the
// sub-tick interpolation offset from the window start dT. dT is the source frame's fCurrentTime; bPaused freezes the
// clock; bHaveInterpolationWindow gates the seeded steady-state path; dSimDeltaSeconds is this frame's sim delta
// (wall x current time ratio). Clamped to [dT, dT + kfDt] so rendering only ever interpolates between two committed
// ticks, never extrapolates. Preserve exact float ops — client-render-clock invariants.
float GameBase::AdvanceRenderClock(double dT, bool bPaused, bool bHaveInterpolationWindow, double dSimDeltaSeconds)
{
	float fDeltaTime = 0.0f;
	if (bPaused)
	{
		// Freeze. Don't advance mfRenderTime; rendered scene stays static until unpause.
		fDeltaTime = static_cast<float>(std::clamp(mfRenderTime - dT, 0.0, static_cast<double>(kfDeltaTime)));
	}
	else if (!bHaveInterpolationWindow)
	{
		// Cold start / under-populated coord (fewer than kiRenderBehindTicks + 1 snapshots): no
		// interpolation window yet. Force fDt=0 so rendering stays pinned to the oldest available
		// frame — never extrapolating past committed ticks. Reset the seed flag so the next
		// steady-state entry re-seeds at midpoint.
		mfRenderTime = dT;
		mbRenderClockSeeded = false;
		fDeltaTime = 0.0f;
	}
	else
	{
		// One-shot seed at window midpoint so jitter has symmetric headroom before hitting
		// either clamp. After seeding, wall-rate integration preserves phase.
		if (!mbRenderClockSeeded)
		{
			mbRenderClockSeeded = true;
			mfRenderTime = dT + 0.5 * kfDeltaTime;
		}

		// Rebase only when mfRenderTime leaves [T - kfDeltaTime, T + 2*kfDeltaTime], so single-tick commits keep the
		// render phase continuous.
		if (mfRenderTime < dT - kfDeltaTime || mfRenderTime > dT + 2.0 * kfDeltaTime)
		{
			// A rebase is a discontinuous visual time jump — should be rare outside loss bursts
			LOG(kNetwork, kVerbose, "Render clock rebase JumpTicks: {} RenderTime: {} WindowStart: {}", common::Wb(static_cast<float>((dT - mfRenderTime) / kfDeltaTime), 2), common::Wb(static_cast<float>(mfRenderTime), 4), common::Wb(static_cast<float>(dT), 4));
			mfRenderTime = dT + 0.5 * kfDeltaTime;
		}

		mfRenderTime += dSimDeltaSeconds;
		double dUnclampedRenderTime = mfRenderTime;
		mfRenderTime = std::clamp(mfRenderTime, dT, dT + static_cast<double>(kfDeltaTime));
		fDeltaTime = static_cast<float>(mfRenderTime - dT);

		// Top-clamp = renderer starved of committed ticks (scene freezes); bottom-clamp = commits
		// outpaced the render clock (scene skips ahead). Both should be brief and rare outside
		// loss bursts. Streaks are logged at the transition; streaks losing < 1/4 tick are
		// dropped as noise.
		{
			static int64_t siStarvedFrames = 0;
			static double sdStarvedSeconds = 0.0;
			static int64_t siSkippedFrames = 0;
			static double sdSkippedSeconds = 0.0;
			if (dUnclampedRenderTime > mfRenderTime)
			{
				++siStarvedFrames;
				sdStarvedSeconds += dUnclampedRenderTime - mfRenderTime;
			}
			else if (siStarvedFrames > 0)
			{
				if (sdStarvedSeconds > 0.25 * kfDeltaTime)
				{
					LOG(kNetwork, kVerbose, "Render clock starved Frames: {} LostTicks: {}", siStarvedFrames, common::Wb(static_cast<float>(sdStarvedSeconds / kfDeltaTime), 2));
				}
				siStarvedFrames = 0;
				sdStarvedSeconds = 0.0;
			}
			if (dUnclampedRenderTime < mfRenderTime)
			{
				++siSkippedFrames;
				sdSkippedSeconds += mfRenderTime - dUnclampedRenderTime;
			}
			else if (siSkippedFrames > 0)
			{
				if (sdSkippedSeconds > 0.25 * kfDeltaTime)
				{
					LOG(kNetwork, kVerbose, "Render clock skipped Frames: {} SkippedTicks: {}", siSkippedFrames, common::Wb(static_cast<float>(sdSkippedSeconds / kfDeltaTime), 2));
				}
				siSkippedFrames = 0;
				sdSkippedSeconds = 0.0;
			}
		}
	}
	return fDeltaTime;
}

bool GameBase::IsCoordRenderable(GridCoord coord) const
{
	// A coord is renderable only when its snapshot ring is populated (iSnapshotCount > 0). This two-condition
	// check is the render-side invariant's definition — single-sourced here, used at every site below.
	auto it = mCoordFrames.find(coord);
	return it != mCoordFrames.end() && it->second.iSnapshotCount > 0;
}

void GameBase::SelectRenderCamera(const std::vector<GridCoord>& rActiveCoords, GridCoord& rCameraCoord, bool& rbHaveRenderableCamera) const
{
	// Camera coord fallback: use client coord if its ring is populated, else the first active coord with
	// a populated (iSnapshotCount > 0) ring. bHaveRenderableCamera == false is the failed-reconnect case
	// where every active coord (including mClientGridCoord) has an empty ring — no coord is renderable
	// this frame, so the camera-anchored render-clock / interpolate / RenderFrame work below is skipped.
	{
		if (IsCoordRenderable(rCameraCoord))
		{
			rbHaveRenderableCamera = true;
		}
		else
		{
			for (const GridCoord& rCoord : rActiveCoords)
			{
				if (IsCoordRenderable(rCoord))
				{
					rCameraCoord = rCoord;
					rbHaveRenderableCamera = true;
					break;
				}
			}
		}
	}
}

void GameBase::UpdateRenderInterpolation(const std::vector<GridCoord>& rActiveCoords, GridCoord cameraCoord, bool bHaveRenderableCamera)
{
	// Per-frame render interpolates and camera update before RenderGlobal so that
	// f4RenderVisibleArea and lighting area are computed from the current camera position
	if (!rActiveCoords.empty())
	{
		gpProfileManager->CpuStart(game::kCpuTimerFrameUpdate);
		gpProfileManager->CpuStart(game::kCpuTimerFrameInterpolate);

		// Render-side sim clock: advance mfRenderTime by sim delta (wall × current time ratio),
		// clamped to [T, T + kfDt]. Render integrates in the same units as T (sim seconds, not
		// wall seconds), so phase is preserved at any time ratio without an explicit servo.
		// Phase is set once via a one-shot seed at the window midpoint; after that, integration
		// preserves it automatically. On a single-tick commit, T advances +kfDt while mfRenderTime
		// stays continuous, so fDt drops by kfDt and the Update(N, kfDt) ≡ Update(N+1, 0)
		// invariant makes the handoff pixel-identical.
		double dSimDeltaSeconds = common::NanosecondsToFloatSeconds<double>(mTimeStep.WallToSim(mRenderTimer.GetDeltaNs(true)));
		mfLastRenderFrameSeconds = dSimDeltaSeconds;
		// Only advance the render clock and sample the source frame when a renderable camera coord exists.
		// On the failed-reconnect all-empty-rings frame (bHaveRenderableCamera == false) fDeltaTime stays 0
		// and none of mCoordFrames.at(cameraCoord) / RenderFrame(cameraCoord) / the clock math runs; the
		// prune and per-coord interpolate below still run so mRenderInterpolates ends renderable-only.
		float fDeltaTime = 0.0f;
		if (bHaveRenderableCamera)
		{
			const CoordFrames& rCameraFrames = mCoordFrames.at(cameraCoord);
			bool bHaveInterpolationWindow = (rCameraFrames.iSnapshotCount >= kiRenderBehindTicks + 1);
			// RenderFrame returns the frame kiRenderBehindTicks behind tail once populated, else the oldest
			// available. Either way, its fCurrentTime is the START of the current render window. Promoted to double so mfRenderTime - dT keeps
			// nanosecond precision even after hours of accumulated game time (float ULP at ~16384s is 2ms,
			// which would otherwise quantize the alpha and visibly stutter at high zoom).
			const game::Frame& rSourceFrame = RenderFrame(cameraCoord);
			double dT = rSourceFrame.interpolate.fCurrentTime;

			bool bPaused = mGameFlags & GameFlags::kPaused;

			fDeltaTime = AdvanceRenderClock(dT, bPaused, bHaveInterpolationWindow, dSimDeltaSeconds);
		}
		{
			ScopedSuppressAllocationTracking suppress;

			// Heap: std::erase_if may rehash, operator[] may insert — must stay wrapped for allocation-tracker compliance
			// Remove render interpolates for deactivated coords AND coords whose snapshot ring is empty, so
			// mRenderInterpolates holds entries for exactly the renderable (iSnapshotCount > 0) active coords.
			// This must run even when bHaveRenderableCamera is false so no stale empty-ring entry survives
			// into RenderFrameMain (whose RenderFrame(coord) asserts iSnapshotCount > 0).
			std::erase_if(mRenderInterpolates, [&](const std::pair<const GridCoord, game::FrameInterpolate>& rPair)
			{
				return !std::ranges::contains(rActiveCoords, rPair.first) || !IsCoordRenderable(rPair.first);
			});

			// AllocateAndCopy + Update each active frame's render interpolate (camera frame first).
			// Per-coord fDt override: a coord with fewer than kiRenderBehindTicks + 1 snapshots cannot
			// fill the render window, so force fDt=0 for that coord regardless of the camera-anchored
			// global fDeltaTime. This prevents a transient under-populated coord (post-fast-path
			// shrink) from rendering extrapolated state.
			auto interpolateFrame = [&](const GridCoord& rCoord)
			{
				const game::Frame& rFrame = RenderFrame(rCoord);
				CoordFrames& rSub = mCoordFrames.at(rCoord);
				if (rFrame.interpolate.iTick < rSub.iLastRenderedTick ||
					(rFrame.interpolate.iTick == rSub.iLastRenderedTick && rFrame.interpolate.fCurrentTime < rSub.fLastRenderedTime))
				{
					LOG(kNetwork, kError, "Render regressed to older frame Coord: ({},{}) Tick: {} LastTick: {} Time: {} LastTime: {}", rCoord.x, rCoord.y, rFrame.interpolate.iTick, rSub.iLastRenderedTick, common::Wb(rFrame.interpolate.fCurrentTime, 4), common::Wb(rSub.fLastRenderedTime, 4));
 					DEBUG_BREAK();
				}
				rSub.iLastRenderedTick = rFrame.interpolate.iTick;
				rSub.fLastRenderedTime = rFrame.interpolate.fCurrentTime;
				float fCoordDeltaTime = (rSub.iSnapshotCount >= kiRenderBehindTicks + 1) ? fDeltaTime : 0.0f;
				game::FrameInterpolate::AllocateAndCopy(mRenderInterpolates.try_emplace(rCoord).first->second, rFrame.interpolate);
				game::FrameInterpolate::Update(mRenderInterpolates.at(rCoord), rFrame, fCoordDeltaTime);
			};
			if (bHaveRenderableCamera)
			{
				interpolateFrame(cameraCoord);
			}
			for (const GridCoord& rCoord : rActiveCoords)
			{
				if (rCoord == cameraCoord)
				{
					continue;
				}
				// Skip empty-ring coords: RenderFrame(rCoord) inside interpolateFrame asserts iSnapshotCount > 0
				// (mirrors Islands::UpdateActiveIslands' empty-ring skip).
				if (!IsCoordRenderable(rCoord))
				{
					continue;
				}
				interpolateFrame(rCoord);
			}
		}
		gpProfileManager->CpuStop(game::kCpuTimerFrameInterpolate);
		gpProfileManager->CpuStop(game::kCpuTimerFrameUpdate);

		if constexpr (kbProfiling)
		{
			gpProfileManager->mInterpolateUpdatesInTheLastSecond.Set();
		}

		// Update camera before RenderGlobal. Skip when no coord is renderable (failed reconnect): the camera
		// holds its last state and mRenderInterpolates.at(cameraCoord) would throw on the absent entry.
		if (bHaveRenderableCamera)
		{
			gpCamera->Update(mRenderInterpolates.at(cameraCoord), static_cast<float>(mfLastRenderFrameSeconds));
		}

		// Decay visual error offset for smooth reconciliation corrections using the sim-scaled render delta
		// (wall delta multiplied by the active time ratio; equal to wall time at ratio 1.0), matching the
		// camera and player interpolation rather than a fixed 1/refreshRate.
		{
			float fDisplayDeltaTime = static_cast<float>(mfLastRenderFrameSeconds);
			float fDecay = std::exp(-game::Game::kfVisualErrorDecayRate * fDisplayDeltaTime);
			game::gpGame->mVecVisualErrorOffset = XMVectorScale(game::gpGame->mVecVisualErrorOffset, fDecay);
			if (XMVectorGetX(XMVector3Length(game::gpGame->mVecVisualErrorOffset)) < game::Game::kfVisualErrorMinDistance)
			{
				game::gpGame->mVecVisualErrorOffset = {};
			}
		}
	}
}

bool GameBase::HandleDeferredSwapchain()
{
	// Skip rendering/submission/present when recreation is deferred or meDestroyType >= kSwapchain: the retired or
	// torn-down swapchain has a stale framebuffer index and unsignaled acquire semaphore, risking a fence-wait timeout.
	// At frame head the destroy tier comes from a failed OUT_OF_DATE acquire/present tail; that tail already consumes
	// settings escalations. Retry Create each frame and acquire only once deferral clears, mirroring
	// RenderMainPresentAcquire. The last rendered tail already waited for present, and skipped frames enqueue none, so
	// no mPresent.Wait is needed here.
	if (gpGraphics->mbSwapchainRecreateDeferred || gpGraphics->meDestroyType >= DestroyType::kSwapchain) [[unlikely]]
	{
		gpGraphics->Create();
		// Acquire only if the recreate actually proceeded. A proceeded Create() clears the flag AND zeroes meDestroyType
		// (via Destroy()); every defer path (pre- or post-Destroy) instead sets the flag, so !mbSwapchainRecreateDeferred
		// is the correct proceeded test — a still-deferred post-Destroy state has a torn-down swapchain manager to acquire.
		if (!gpGraphics->mbSwapchainRecreateDeferred)
		{
			gpSwapchainManager->AcquireNextImage();
			mMinimizedThrottleLast.reset(); // Recreate resumed: drop the throttle timestamp so a fresh minimize starts clean.
		}
		else
		{
			// Still deferred (minimized / off-screen): this branch loops every frame with no vkQueuePresentKHR to
			// throttle it, so pace it to the sim tick's remaining time (kTickNs minus this iteration's elapsed
			// wall time) — the minimized loop holds ~32 Hz instead of busy-spinning a core and re-issuing a
			// vkGetPhysicalDeviceSurfaceCapabilitiesKHR per spin. Mirrors ServerSessionRuntime::WaitForTick's high-resolution
			// waitable timer minus its precision spin (nothing minimized needs sub-ms accuracy).
			if (mMinimizedThrottleTimer == nullptr)
			{
				mMinimizedThrottleTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
			}

			if (mMinimizedThrottleLast.has_value())
			{
				std::chrono::nanoseconds elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - *mMinimizedThrottleLast);
				std::chrono::nanoseconds remainingNs = kTickNs - elapsedNs;
				static constexpr std::chrono::nanoseconds kMinThrottleNs = 1'000'000ns; // ~1 ms floor: skip sub-ms / non-positive remainders.
				if (remainingNs >= kMinThrottleNs)
				{
					LARGE_INTEGER dueTime {.QuadPart = -(remainingNs.count() / 100),}; // Negative = relative, 100ns units.
					if (mMinimizedThrottleTimer != nullptr && SetWaitableTimerEx(mMinimizedThrottleTimer, &dueTime, 0, nullptr, nullptr, nullptr, 0) != 0)
					{
						WaitForSingleObject(mMinimizedThrottleTimer, INFINITE);
					}
					else
					{
						// Timer create or arm failed (OS API results are a trust boundary — waiting on an unarmed
						// auto-reset timer would block forever): fall back to Sleep at ambient granularity.
						Sleep(static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(remainingNs).count()));
					}
				}
			}

			// Timestamp AFTER the wait so the next iteration's remainder measures one full loop.
			mMinimizedThrottleLast = std::chrono::steady_clock::now();
		}
		return true;
	}
	return false;
}

void GameBase::Render()
{
	const std::vector<GridCoord>& rActiveCoords = mActiveCoords;

	// Interpolate elapsed time with the sub-step remainder for smooth rendering
	float fCurrentTime = mfCurrentTime + std::max(0.0f, common::NanosecondsToFloatSeconds<float>(mTimeStep.mTickRemainderNs));

	GridCoord cameraCoord = game::gpGame->mClientGridCoord;
	bool bHaveRenderableCamera = false;
	SelectRenderCamera(rActiveCoords, cameraCoord, bHaveRenderableCamera);
	UpdateRenderInterpolation(rActiveCoords, cameraCoord, bHaveRenderableCamera);

	if (HandleDeferredSwapchain())
	{
		return;
	}

	// Pack island instances after the current camera update and only once the acquired framebuffer index is valid.
	game::gpGame->UpdateActiveIslands();
	gpGraphics->RenderGlobal(fCurrentTime);

	// Capture command buffer index before async launch to avoid re-reading in async thread
	int64_t iCommandBuffer = gpSwapchainManager->miFramebufferIndex;
	gpGraphics->RenderMainPresentAcquire(iCommandBuffer, mRenderInterpolates, rActiveCoords, cameraCoord);

}
#endif // BT_CLIENT

#if defined(BT_SERVER)
void GameBase::RefreshReplayActiveSet()
{
	// During replay, all coords with live readers are active; SyncReplayTick retires ended readers before dispatch.
	// Heap: vector clear/push_back, unordered_map insertion + make_unique<Frame>
	ScopedSuppressAllocationTracking suppress;
	mActiveCoords.clear();
	for (const auto& [rCoord, rpReader] : gpReplay->mReplayReaders)
	{
		mActiveCoords.push_back(rCoord);
		CoordFrames& rSub = mCoordFrames.try_emplace(rCoord).first->second;
		if (rSub.pNext == nullptr)
		{
			rSub.pNext = std::make_unique<game::Frame>();
		}
	}
}
#endif // BT_SERVER

void GameBase::CreateFrameAtCoord(GridCoord coord)
{
	// Heap: unordered_map insertion + make_unique<Frame>. Frame persists across game lifetime
	ScopedSuppressAllocationTracking suppress;

	CoordFrames& rFrames = mCoordFrames.try_emplace(coord).first->second;
#if defined(BT_CLIENT)
	// Client uses snapshot ring as the source of truth — seed slot 0.
	rFrames.iSnapshotHead = 0;
	rFrames.iSnapshotCount = 1;
	rFrames.snapshots[0] = std::make_unique<game::Frame>();
	game::Frame& rFrame = *rFrames.snapshots[0];
#else
	rFrames.pCurrent = std::make_unique<game::Frame>();
	game::Frame& rFrame = *rFrames.pCurrent;
#endif
	rFrame.interpolate.iTick = miTickCounter;
	rFrame.interpolate.fCurrentTime = mfCurrentTime;
	rFrame.interpolate.gameFlags.Set(game::GameFlags::kGame);
	game::gpGame->InitFramePostRender(rFrame);

	// Populate static data for this coord
	FrameStaticData& rStaticData = rFrames.staticData;
	rStaticData.vecArea = ComputeCanonicalFrameArea(coord);
	rStaticData.coord = coord;
	GenerateIslandChain(coord, rStaticData.islands);
	// navData stays empty; RunFrameTick builds it lazily on the per-coord dispatch thread.
}

void GameBase::PrepareActiveSet()
{
#if defined(BT_SERVER)
	if (mbReplaying)
	{
		RefreshReplayActiveSet();
		// SyncReplayTick creates and overwrites each recorded FrameInput. Do not advance normal server managers here.
		return;
	}
#endif // BT_SERVER

	game::gpGame->ComputeActiveSet();
#if defined(BT_SERVER)
	game::gpGame->EnsureNextFrames();
#endif
	game::gpGame->BuildFrameInputs();
}

#if defined(BT_SERVER)
void GameBase::SwapFrames()
{
	for (auto& [rCoord, rFrames] : mCoordFrames)
	{
		std::swap(rFrames.pCurrent, rFrames.pNext);
	}

	// After swap, .next holds old current frames (stale data, reusable memory).
	// Ensure active entries exist for next iteration's AllocateAndCopy.
	if (!mbReplaying)
	{
		game::gpGame->EnsureNextFrames();
	}
	else if (mCoordFrames.contains(game::gpGame->mClientGridCoord)
		&& mCoordFrames.at(game::gpGame->mClientGridCoord).pNext == nullptr)
	{
		// Heap: make_unique<Frame> for replay target coordinate
		ScopedSuppressAllocationTracking suppress;
		mCoordFrames.at(game::gpGame->mClientGridCoord).pNext = std::make_unique<game::Frame>();
	}
}
#endif // BT_SERVER

} // namespace engine
