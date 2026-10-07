#include "GameBase.h"

#include "Game.h"
#if defined(BT_CLIENT)
#include "Network/Client/ClientSession.h"
#endif
#if defined(BT_SERVER)
#include "Agent/Commands/ReplayFixtures.h"
#include "File/Replay.h"
#include "Network/Server/ServerSession.h"
#include "Network/Server/ServerTransferManager.h"
#endif
#include "Frame/FrameBase.h"
#include "Input/Input.h"
#include "Ui/GraphicsSettingsWrappersBase.h"

#include "Profile/ProfileManager.h"

namespace engine
{

GameBase::GameBase()
{
	game::FrameInterpolate::Register();

#if defined(BT_SERVER)
	mpReplay = std::make_unique<Replay>();
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
// callback -> previous-snapshot assignment. The modal gate skips everything after it except that assignment, so a modal frame still
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
		gpInput->mPreviousRawInput = gpRawInputManager->mRawInput;
		return;
	}

	if (menuInput.bGamepad && mMenuFlags & MenuFlags::kMouseVisible)
	{
		ShowCursor(FALSE);
		mMenuFlags.Set(MenuFlags::kMouseVisible, false);
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
			DebugRender::msbEnabled = !DebugRender::msbEnabled;
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
			float fNext = gDebugTextureIndex.mfCurrent + 1.0f;
			if (fNext >= static_cast<float>(gpTextureManager->mRenderTargetTextures.miDebugTextureCount))
			{
				fNext = 0.0f;
			}
			gDebugTextureIndex.Set(fNext);
		}
		if (menuInput.flags & MenuInputFlags::kDebugTexturePrevious)
		{
			float fPrevious = gDebugTextureIndex.mfCurrent - 1.0f;
			if (fPrevious < 0.0f)
			{
				fPrevious = static_cast<float>(gpTextureManager->mRenderTargetTextures.miDebugTextureCount - 1);
			}
			gDebugTextureIndex.Set(fPrevious);
		}

		// Only an applied timespeed change sets mbTimeScaleChanged; sending a request in the later game callback does not.
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

	// Runs after every edge consumer, including the game callback and the modal path that skips it; moving this
	// earlier silently breaks edge detection for whatever still has to run.
	gpInput->mPreviousRawInput = gpRawInputManager->mRawInput;
}
#endif // BT_CLIENT

#if defined(BT_CLIENT)
void GameBase::ClientUpdate()
{
	common::LogTickScope logTickScope(miTickCounter);

	{
		// Heap: transport receive buffers and game packet/frame adoption
		ScopedSuppressAllocationTracking suppress;
		NetworkTimeState networkTimeState
		{
			.bFastForward = mTimeStep.miTimeMultiply > 1,
			.iExpectedUpdateIntervalMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(mTimeStep.SimulationToWall(game::NetworkSessionContract::kTickDuration)).count(),
			.iExpectedUpdatesPerSecond = kiTickRate * mTimeStep.miTimeMultiply / mTimeStep.miTimeDivide,
		};
		game::gpClientSession->mpRuntime->PollAndDrain(networkTimeState);
	}

	if (game::gpClientSession->mpDesynchronizationCore->IsStalled())
	{
		return;
	}

	int64_t iFullTicks = mTimeStep.TickRealtime();
	if (mGameFlags & GameFlags::kPaused) [[unlikely]]
	{
		mTimeStep.mTickRemainderNanoseconds = 0ns;
		iFullTicks = 0;
	}

	game::gpClientSession->UpdateDesiredCoordinates(game::SubscriptionChangeReason::kPollTick);
	game::gpClientSession->mpRuntime->SynchronizeSubscriptions();
	PrepareActiveSet();

	// Hard ceiling: clock-servo target + kiSimulationCeilingSlackTicks. EvaluateClock steers the
	// sim toward the bare target, so this clamp engages only on genuine arrival stalls (loss bursts),
	// not per-packet jitter, while StatusChanges still normally arrive before their tick simulates.
	// Extreme "sim way behind target" is handled by the snap path in Reconcile.
	const engine::ClientSessionRuntime& rRuntime = *game::gpClientSession->mpRuntime;
	int64_t iCeiling = rRuntime.miLatestServerTick < 0 ? -1 : rRuntime.miLatestServerTick - rRuntime.miCurrentTargetBehind + engine::kiSimulationCeilingSlackTicks;
	int64_t iAbsorbedTicks = 0;
	if (iCeiling >= 0 && iFullTicks > 0)
	{
		int64_t iRoomToAdvance = std::max<int64_t>(0, iCeiling - miTickCounter);
		if (iFullTicks > iRoomToAdvance)
		{
			iAbsorbedTicks = iFullTicks - iRoomToAdvance;
			// The client simulation ceiling returns unused tick time to the accumulator; TickRealtime's kiMaximumAccumulatorTicks cap bounds growth during long stalls.
			mTimeStep.mTickRemainderNanoseconds += iAbsorbedTicks * kTickNanoseconds;
			iFullTicks = iRoomToAdvance;
		}
	}

	// When latestServerTick stalls (loss burst), the ceiling freezes the sim entirely, then releases
	// the backlog as a burst — log at the transition. Frequent stalls outside loss bursts indicate a
	// clock-servo / kiSimulationCeilingSlackTicks tuning problem (the servo should keep steady-state jitter
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

	NetworkTimeState networkTimeState
	{
		.bFastForward = mTimeStep.miTimeMultiply > 1,
		.iExpectedUpdateIntervalMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(mTimeStep.SimulationToWall(game::NetworkSessionContract::kTickDuration)).count(),
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
		bAcceptRawCpuTimers = iFullTicks == 1 && mTimeStep.miTimeMultiply == 1 && mTimeStep.miTimeDivide == 1
		                   && !(mGameFlags & GameFlags::kPaused) && gpReplay->mReplayWriters.empty() && !mbReplaying
		                   && !(mGameFlags & GameFlags::kSaveReplay);
	}
	gpServer->BroadcastTimespeedIfChanged();
	if (mGameFlags & GameFlags::kPaused) [[unlikely]]
	{
		mTimeStep.mTickRemainderNanoseconds = 0ns;
		iFullTicks = 0;
	}
	else if (iFullTicks != 1) [[unlikely]]
	{
		LOG(kDefault, kWarning, "ServerUpdate FullTicks: {} (expected 1)", iFullTicks);
	}
	int64_t iUnusedTicks = 0;
	if (ReplayFixtures::IsWriterPauseArmed(*gpReplay) && iFullTicks > 1)
	{
		iUnusedTicks = iFullTicks - 1;
		iFullTicks = 1;
	}
	mfLastDeltaTime = static_cast<float>(iFullTicks) * kfDeltaTime;

	PrepareActiveSet();

	const std::vector<GridCoord>& rActiveCoordinates = mActiveCoordinates;

	gpProfileManager->CpuStart(game::kCpuTimerFrameUpdate);
	int64_t iFinalizedTicks = 0;
	bool bRawCpuTimersNoDispatchLatched = false;
	for (int64_t i = 0; i < iFullTicks; ++i)
	{
		int64_t iPreviousTickCounter = miTickCounter;
		float fPreviousCurrentTime = mfCurrentTime;
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

		if ((!gpReplay->mReplayWriters.empty()) || mbReplaying || (mGameFlags & GameFlags::kSaveReplay)) [[unlikely]]
		{
			if (gpReplay->SyncReplayTick() == Replay::ReplayTickDecision::kStopBeforeDispatch)
			{
				if constexpr (kbProfiling)
				{
					gpProfileManager->LatchRawCpuTimers(false, miTickCounter);
					bRawCpuTimersNoDispatchLatched = true;
				}
				// Loop completion reloads before observers see an empty grid. Replay abort leaves no load request,
				// so SaveLoadReplay is a no-op and the normal active state is restored below.
				gpReplay->SaveLoadReplay();
				if (!mbReplaying)
				{
					// Replay stopped after this iteration advanced the sim clock but before it finalized a frame.
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

		BuildAndDispatchFrameTicks(rActiveCoordinates);
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
			mTimeStep.mTickRemainderNanoseconds = 0ns;
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
		mTimeStep.mTickRemainderNanoseconds += iUnusedTicks * kTickNanoseconds;
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
void GameBase::BuildAndDispatchFrameTicks(const std::vector<GridCoord>& rActiveCoordinates)
{
	int64_t iActiveCount = std::ssize(rActiveCoordinates);

	// Pre-resolve frame references to avoid repeated map lookups across all phases
	{
		// Heap: mActiveFrameReferences persists across ticks and may grow with the unbounded active-cell count;
		// retain its capacity while rebuilding the per-tick references.
		ScopedSuppressAllocationTracking suppress;
		mActiveFrameReferences.clear();
		mActiveFrameReferences.reserve(static_cast<size_t>(iActiveCount));
		for (const GridCoord& rCoordinate : rActiveCoordinates)
		{
			Cell& rCell = mCells.at(rCoordinate);
			if (rCell.pCurrent == nullptr || rCell.pNext == nullptr)
			{
				LOG(kDefault, kWarning, "BuildDispatch NullFrame Coord: ({},{}) pCurrent: {} pNext: {}", rCoordinate.iX, rCoordinate.iY, rCell.pCurrent != nullptr, rCell.pNext != nullptr);
				continue;
			}
			mActiveFrameReferences.push_back(
			{
				.pNext = rCell.pNext.get(),
				.pCurrent = rCell.pCurrent.get(),
				.pFrameInput = &mFrameInputs.at(rCoordinate),
				.pStaticData = &rCell.staticData,
			});
		}
	}
	const std::vector<ActiveFrameReference>& rActiveFrameReferences = mActiveFrameReferences;

	gpProfileManager->CpuStart(game::kCpuTimerFrameInterpolate);
	gpProfileManager->CpuStart(game::kCpuTimerFramePostRender);

	int64_t iFrameReferenceCount = std::ssize(rActiveFrameReferences);
	auto ProcessRange = [&](int64_t iBegin, int64_t iEnd)
	{
		for (int64_t j = iBegin; j < iEnd; ++j)
		{
			RunFrameTick(rActiveFrameReferences.at(static_cast<size_t>(j)), miTickCounter, mfCurrentTime);
		}
	};
	if constexpr (kbFrameDispatch)
	{
		common::gpMultithreading->Dispatch(iFrameReferenceCount, ProcessRange);
	}
	else
	{
		ProcessRange(0, iFrameReferenceCount);
	}

	gpProfileManager->CpuStop(game::kCpuTimerFramePostRender);
	gpProfileManager->CpuStop(game::kCpuTimerFrameInterpolate);
}

void GameBase::FinalizeFrameTick()
{
	// Transfer entities that crossed cell boundaries into destination cells
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

	for (auto& [rCoordinate, rFrameInput] : mFrameInputs)
	{
		rFrameInput.statusChanges.clear();
	}
}
#endif // BT_SERVER

#if defined(BT_CLIENT)
game::Frame& GameBase::RenderFrame(GridCoord coordinate) const
{
	// Returns the frame kiRenderBehindTicks slots behind tail when possible so the one-tick render
	// window spans (source -> source+1) — a true interpolation between two committed ticks, with
	// the newer committed ticks up to tail held as starvation cushion. When the ring hasn't
	// populated that many slots yet (cold start or a replay/rollback that didn't apply retention),
	// fall back to the oldest available; callers force fDeltaTime = 0 for that coord so no
	// extrapolation occurs.
	const Cell& rCell = mCells.at(coordinate);
	ASSERT(rCell.iSnapshotCount > 0);
	int64_t iDesiredLogical = rCell.iSnapshotCount - 1 - kiRenderBehindTicks;
	int64_t iLogical = std::max<int64_t>(0, iDesiredLogical);
	int64_t iPhysical = SnapshotIndex(rCell.iSnapshotHead, iLogical);
	ASSERT(rCell.snapshots[iPhysical] != nullptr);
	return *rCell.snapshots[iPhysical];
}

void GameBase::ResetRenderClock()
{
	mfRenderTime = 0.0;
	mbRenderClockSeeded = false;
	mRenderTimer.Reset();
}

// Render-side sim clock advance. Integrates mfRenderTime (sim seconds) and returns fDeltaTime in [0, kfDt], the
// sub-tick interpolation offset from the window start fWindowStartTime. fWindowStartTime is the source frame's fCurrentTime; bPaused freezes the
// clock; bHaveInterpolationWindow gates the seeded steady-state path; fSimulationDeltaSeconds is this frame's sim delta
// (wall x current time ratio). Clamped to [fWindowStartTime, fWindowStartTime + kfDt] so rendering only ever interpolates between two committed
// ticks, never extrapolates. Preserve exact float ops — client-render-clock invariants.
float GameBase::AdvanceRenderClock(double fWindowStartTime, bool bPaused, bool bHaveInterpolationWindow, double fSimulationDeltaSeconds)
{
	float fDeltaTime = 0.0f;
	if (bPaused)
	{
		// Freeze. Don't advance mfRenderTime; rendered scene stays static until unpause.
		fDeltaTime = static_cast<float>(std::clamp(mfRenderTime - fWindowStartTime, 0.0, static_cast<double>(kfDeltaTime)));
	}
	else if (!bHaveInterpolationWindow)
	{
		// Cold start / under-populated coord (fewer than kiRenderBehindTicks + 1 snapshots): no
		// interpolation window yet. Force fDt=0 so rendering stays pinned to the oldest available
		// frame — never extrapolating past committed ticks. Reset the seed flag so the next
		// steady-state entry re-seeds at midpoint.
		mfRenderTime = fWindowStartTime;
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
			mfRenderTime = fWindowStartTime + 0.5 * kfDeltaTime;
		}

		// Rebase only when mfRenderTime leaves [T - kfDeltaTime, T + 2*kfDeltaTime], so single-tick commits keep the
		// render phase continuous.
		if (mfRenderTime < fWindowStartTime - kfDeltaTime || mfRenderTime > fWindowStartTime + 2.0 * kfDeltaTime)
		{
			// A rebase is a discontinuous visual time jump — should be rare outside loss bursts
			LOG(kNetwork, kVerbose, "Render clock rebase JumpTicks: {} RenderTime: {} WindowStart: {}", common::Wb(static_cast<float>((fWindowStartTime - mfRenderTime) / kfDeltaTime), 2), common::Wb(static_cast<float>(mfRenderTime), 4), common::Wb(static_cast<float>(fWindowStartTime), 4));
			mfRenderTime = fWindowStartTime + 0.5 * kfDeltaTime;
		}

		mfRenderTime += fSimulationDeltaSeconds;
		double fUnclampedRenderTime = mfRenderTime;
		mfRenderTime = std::clamp(mfRenderTime, fWindowStartTime, fWindowStartTime + static_cast<double>(kfDeltaTime));
		fDeltaTime = static_cast<float>(mfRenderTime - fWindowStartTime);

		// Top-clamp = renderer starved of committed ticks (scene freezes); bottom-clamp = commits
		// outpaced the render clock (scene skips ahead). Both should be brief and rare outside
		// loss bursts. Streaks are logged at the transition; streaks losing < 1/4 tick are
		// dropped as noise.
		{
			static int64_t siStarvedFrames = 0;
			static double sfStarvedSeconds = 0.0;
			static int64_t siSkippedFrames = 0;
			static double sfSkippedSeconds = 0.0;
			if (fUnclampedRenderTime > mfRenderTime)
			{
				++siStarvedFrames;
				sfStarvedSeconds += fUnclampedRenderTime - mfRenderTime;
			}
			else if (siStarvedFrames > 0)
			{
				if (sfStarvedSeconds > 0.25 * kfDeltaTime)
				{
					LOG(kNetwork, kVerbose, "Render clock starved Frames: {} LostTicks: {}", siStarvedFrames, common::Wb(static_cast<float>(sfStarvedSeconds / kfDeltaTime), 2));
				}
				siStarvedFrames = 0;
				sfStarvedSeconds = 0.0;
			}
			if (fUnclampedRenderTime < mfRenderTime)
			{
				++siSkippedFrames;
				sfSkippedSeconds += mfRenderTime - fUnclampedRenderTime;
			}
			else if (siSkippedFrames > 0)
			{
				if (sfSkippedSeconds > 0.25 * kfDeltaTime)
				{
					LOG(kNetwork, kVerbose, "Render clock skipped Frames: {} SkippedTicks: {}", siSkippedFrames, common::Wb(static_cast<float>(sfSkippedSeconds / kfDeltaTime), 2));
				}
				siSkippedFrames = 0;
				sfSkippedSeconds = 0.0;
			}
		}
	}
	return fDeltaTime;
}

bool GameBase::IsCoordinateRenderable(GridCoord coordinate) const
{
	// A coordinate is renderable only when it has frame storage and a populated snapshot ring.
	auto it = mCells.find(coordinate);
	return it != mCells.end() && it->second.iSnapshotCount > 0;
}

void GameBase::SelectRenderCamera(const std::vector<GridCoord>& rActiveCoordinates, GridCoord& rCameraCoordinate, bool& rbHaveRenderableCamera) const
{
	// Camera coord fallback: use client coord if its ring is populated, else the first active coord with
	// a populated (iSnapshotCount > 0) ring. bHaveRenderableCamera == false is the failed-reconnect case
	// where every active coord (including mClientGridCoordinate) has an empty ring — no coord is renderable
	// this frame, so the camera-anchored render-clock / interpolate / RenderFrame work below is skipped.
	{
		if (IsCoordinateRenderable(rCameraCoordinate))
		{
			rbHaveRenderableCamera = true;
		}
		else
		{
			for (const GridCoord& rCoordinate : rActiveCoordinates)
			{
				if (IsCoordinateRenderable(rCoordinate))
				{
					rCameraCoordinate = rCoordinate;
					rbHaveRenderableCamera = true;
					break;
				}
			}
		}
	}
}

void GameBase::UpdateRenderInterpolation(const std::vector<GridCoord>& rActiveCoordinates, GridCoord cameraCoordinate, bool bHaveRenderableCamera)
{
	// Per-frame render interpolates and camera update before RenderGlobal so that
	// mf4RenderVisibleArea and lighting area are computed from the current camera position
	if (rActiveCoordinates.empty())
	{
		return;
	}

	gpProfileManager->CpuStart(game::kCpuTimerFrameUpdate);
	gpProfileManager->CpuStart(game::kCpuTimerFrameInterpolate);

	// Render-side sim clock: advance mfRenderTime by sim delta (wall × current time ratio),
	// clamped to [T, T + kfDt]. Render integrates in the same units as T (sim seconds, not
	// wall seconds), so phase is preserved at any time ratio without an explicit servo.
	// Phase is set once via a one-shot seed at the window midpoint; after that, integration
	// preserves it automatically. On a single-tick commit, T advances +kfDt while mfRenderTime
	// stays continuous, so fDt drops by kfDt and the Update(N, kfDt) ≡ Update(N+1, 0)
	// invariant makes the handoff pixel-identical.
	double fSimulationDeltaSeconds = common::NanosecondsToFloatSeconds<double>(mTimeStep.WallToSimulation(mRenderTimer.GetDeltaNs(true)));
	mfLastRenderFrameSeconds = fSimulationDeltaSeconds;
	// Only advance the render clock and sample the source frame when a renderable camera coord exists.
	// On the failed-reconnect all-empty-rings frame (bHaveRenderableCamera == false) fDeltaTime stays 0
	// and none of mCells.at(cameraCoordinate) / RenderFrame(cameraCoordinate) / the clock math runs; the
	// prune and per-coord interpolate below still run so mRenderInterpolates ends renderable-only.
	float fDeltaTime = 0.0f;
	if (bHaveRenderableCamera)
	{
		const Cell& rCameraCell = mCells.at(cameraCoordinate);
		bool bHaveInterpolationWindow = (rCameraCell.iSnapshotCount >= kiRenderBehindTicks + 1);
		// RenderFrame returns the frame kiRenderBehindTicks behind tail once populated, else the oldest
		// available. Either way, its fCurrentTime is the START of the current render window. Promoted to double so mfRenderTime - fWindowStartTime keeps
		// nanosecond precision even after hours of accumulated game time (float ULP at ~16384s is 2ms,
		// which would otherwise quantize the alpha and visibly stutter at high zoom).
		const game::Frame& rSourceFrame = RenderFrame(cameraCoordinate);
		double fWindowStartTime = rSourceFrame.interpolate.fCurrentTime;

		bool bPaused = mGameFlags & GameFlags::kPaused;

		fDeltaTime = AdvanceRenderClock(fWindowStartTime, bPaused, bHaveInterpolationWindow, fSimulationDeltaSeconds);
	}
	{
		ScopedSuppressAllocationTracking suppress;

		// Heap: try_emplace can allocate frame-interpolation entries, so allocation tracking stays suppressed in this scope.
		// Remove render interpolates for deactivated coords AND coords whose snapshot ring is empty, so
		// mRenderInterpolates holds entries for exactly the renderable (iSnapshotCount > 0) active coords.
		// This must run even when bHaveRenderableCamera is false so no stale empty-ring entry survives
		// into RenderFrameMain (whose RenderFrame(coordinate) asserts iSnapshotCount > 0).
		std::erase_if(mRenderInterpolates, [&](const std::pair<const GridCoord, game::FrameInterpolate>& rPair)
		{
			return !std::ranges::contains(rActiveCoordinates, rPair.first) || !IsCoordinateRenderable(rPair.first);
		});

		// AllocateAndCopy + Update each active frame's render interpolate (camera frame first).
		// Per-coord fDt override: a coord with fewer than kiRenderBehindTicks + 1 snapshots cannot
		// fill the render window, so force fDt=0 for that coord regardless of the camera-anchored
		// global fDeltaTime. This prevents a transient under-populated coord (post-fast-path
		// shrink) from rendering extrapolated state.
		auto InterpolateFrame = [&](const GridCoord& rCoordinate)
		{
			const game::Frame& rFrame = RenderFrame(rCoordinate);
			Cell& rCell = mCells.at(rCoordinate);
			if (rFrame.interpolate.iTick < rCell.iLastRenderedTick
			 || (rFrame.interpolate.iTick == rCell.iLastRenderedTick && rFrame.interpolate.fCurrentTime < rCell.fLastRenderedTime))
			{
				LOG(kNetwork, kError, "Render regressed to older frame Coord: ({},{}) Tick: {} LastTick: {} Time: {} LastTime: {}", rCoordinate.iX, rCoordinate.iY, rFrame.interpolate.iTick, rCell.iLastRenderedTick, common::Wb(rFrame.interpolate.fCurrentTime, 4), common::Wb(rCell.fLastRenderedTime, 4));
				DEBUG_BREAK();
			}
			rCell.iLastRenderedTick = rFrame.interpolate.iTick;
			rCell.fLastRenderedTime = rFrame.interpolate.fCurrentTime;
			float fCoordinateDeltaTime = (rCell.iSnapshotCount >= kiRenderBehindTicks + 1) ? fDeltaTime : 0.0f;
			game::FrameInterpolate::AllocateAndCopy(mRenderInterpolates.try_emplace(rCoordinate).first->second, rFrame.interpolate);
			game::FrameInterpolate::Update(mRenderInterpolates.at(rCoordinate), rFrame, fCoordinateDeltaTime);
			// Label the copy with the cell its positions are local to and that cell's offset from the camera cell
			// this same render entry point just resolved. Every renderer converts with this value at its one
			// position-to-GPU point; the position columns are never rewritten.
			mRenderInterpolates.at(rCoordinate).renderBasis = MakeRenderBasis(rCoordinate, cameraCoordinate);
		};
		if (bHaveRenderableCamera)
		{
			InterpolateFrame(cameraCoordinate);
		}
		for (const GridCoord& rCoordinate : rActiveCoordinates)
		{
			if (rCoordinate == cameraCoordinate)
			{
				continue;
			}
			// Skip empty-ring coords: RenderFrame(rCoordinate) inside InterpolateFrame asserts iSnapshotCount > 0
			// (mirrors Islands::UpdateActiveIslands' empty-ring skip).
			if (!IsCoordinateRenderable(rCoordinate))
			{
				continue;
			}
			InterpolateFrame(rCoordinate);
		}
	}
	gpProfileManager->CpuStop(game::kCpuTimerFrameInterpolate);
	gpProfileManager->CpuStop(game::kCpuTimerFrameUpdate);

	if constexpr (kbProfiling)
	{
		gpProfileManager->mInterpolateUpdatesInTheLastSecond.Set();
	}

	// Update camera before RenderGlobal. Skip when no coord is renderable (failed reconnect): the camera
	// holds its last state and mRenderInterpolates.at(cameraCoordinate) would throw on the absent entry.
	if (bHaveRenderableCamera)
	{
		gpCamera->Update(mRenderInterpolates.at(cameraCoordinate), static_cast<float>(mfLastRenderFrameSeconds));
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

bool GameBase::HandleDeferredSwapchain()
{
	// Skip rendering/submission/present when recreation is deferred or meDestroyType >= kSwapchain: the retired or
	// torn-down swapchain has a stale framebuffer index and unsignaled acquire semaphore, risking a fence-wait timeout.
	// At frame head the destroy tier comes from a failed acquire/present tail or, on the first boot iteration, the
	// Graphics constructor's acquire; Create()'s Refresh() consumes settings escalations at every tail. Create is
	// retried each frame, mirroring RenderMainPresentAcquire. The last rendered tail already waited for present, and
	// skipped frames enqueue none, so no mPresent.Wait is needed here.
	if (!gpGraphics->mbSwapchainRecreateDeferred && gpGraphics->meDestroyType < DestroyType::kSwapchain) [[likely]]
	{
		return false;
	}

	gpGraphics->Create();
	// Successful recreation clears mbSwapchainRecreateDeferred and resets meDestroyType through Destroy. Every deferred path sets the flag, including after Destroy has torn down the swapchain, so acquire must wait until the flag clears.
	if (!gpGraphics->mbSwapchainRecreateDeferred)
	{
		gpProfileManager->DiscardSkippedFrameSamples();
		gpSwapchainManager->AcquireNextImage();
		mMinimizedThrottleLast.reset(); // Recreate resumed: drop the throttle timestamp so a fresh minimize starts clean.
		gpGraphics->mRenderFrameTimer.Reset(); // The next frame delta steps wind, so it must not span the skipped interval.
	}
	else
	{
		// Still deferred (minimized / off-screen): this branch loops every frame with no vkQueuePresentKHR to
		// throttle it, so wait out the rest of one sim tick's wall duration, bounded between the ~1 ms floor and the
		// unscaled tick — fast time scales cannot busy-spin a core and re-issue a
		// vkGetPhysicalDeviceSurfaceCapabilitiesKHR per spin, and slow ones cannot stall the next iteration's message pump
		// and agent command drain. Mirrors ServerSessionRuntime::WaitForTick's high-resolution
		// waitable timer minus its precision spin (nothing minimized needs sub-ms accuracy).
		if (mMinimizedThrottleTimer == nullptr)
		{
			mMinimizedThrottleTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
		}

		if (mMinimizedThrottleLast.has_value())
		{
			static constexpr std::chrono::nanoseconds kMinimumThrottleNanoseconds = 1'000'000ns;
			std::chrono::nanoseconds elapsedNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - *mMinimizedThrottleLast);
			std::chrono::nanoseconds budgetNanoseconds = std::clamp(mTimeStep.SimulationToWall(kTickNanoseconds), kMinimumThrottleNanoseconds, kTickNanoseconds);
			std::chrono::nanoseconds remainingNanoseconds = budgetNanoseconds - elapsedNanoseconds;
			if (remainingNanoseconds > 0ns)
			{
				LARGE_INTEGER dueTime {.QuadPart = -(remainingNanoseconds.count() / 100),}; // Negative = relative, 100ns units.
				if (mMinimizedThrottleTimer != nullptr && SetWaitableTimerEx(mMinimizedThrottleTimer, &dueTime, 0, nullptr, nullptr, nullptr, 0) != 0)
				{
					WaitForSingleObject(mMinimizedThrottleTimer, INFINITE);
				}
				else
				{
					// Timer create or arm failed (OS API results are a trust boundary — waiting on an unarmed
					// auto-reset timer would block forever): fall back to Sleep at ambient granularity.
					Sleep(static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(remainingNanoseconds).count()));
				}
			}
		}

		// Timestamp AFTER the wait so the next iteration's remainder measures one full loop.
		mMinimizedThrottleLast = std::chrono::steady_clock::now();
	}
	return true;
}

void GameBase::Render()
{
	const std::vector<GridCoord>& rActiveCoordinates = mActiveCoordinates;

	// Interpolate elapsed time with the sub-step remainder for smooth rendering
	float fCurrentTime = mfCurrentTime + std::max(0.0f, common::NanosecondsToFloatSeconds<float>(mTimeStep.mTickRemainderNanoseconds));

	GridCoord cameraCoordinate = game::gpGame->mClientGridCoordinate;
	bool bHaveRenderableCamera = false;
	SelectRenderCamera(rActiveCoordinates, cameraCoordinate, bHaveRenderableCamera);
	UpdateRenderInterpolation(rActiveCoordinates, cameraCoordinate, bHaveRenderableCamera);

	if (HandleDeferredSwapchain())
	{
		// RenderGlobal is skipped, so drop this frame's staged spawns rather than letting them pile up past the staging limit.
		gpParticleManager->DiscardStagedSpawns();
		// RenderMainPresentAcquire is skipped too, so signal the upload thread here to keep transfer uploads moving while the swapchain is deferred.
		gpTextureUploadManager->SignalFrame();
		return;
	}

	// Pack island instances after the current camera update and only once the acquired framebuffer index is valid.
	game::gpGame->UpdateActiveIslands();
	gpGraphics->RenderGlobal(std::chrono::duration<float>(fCurrentTime));

	// Capture command buffer index before async launch to avoid re-reading in async thread
	int64_t iCommandBuffer = gpSwapchainManager->miFramebufferIndex;
	gpGraphics->RenderMainPresentAcquire(iCommandBuffer, mRenderInterpolates, rActiveCoordinates, cameraCoordinate, std::chrono::duration<float>(fCurrentTime));

}
#endif // BT_CLIENT

#if defined(BT_SERVER)
void GameBase::RefreshReplayActiveSet()
{
	// During replay, all coords with live readers are active; SyncReplayTick retires ended readers before dispatch.
	// Heap: vector clear/push_back, unordered_map insertion + make_unique<Frame>
	ScopedSuppressAllocationTracking suppress;
	mActiveCoordinates.clear();
	mActiveCoordinates.reserve(gpReplay->mReplayReaders.size());
	for (const auto& [rCoordinate, rpReader] : gpReplay->mReplayReaders)
	{
		mActiveCoordinates.push_back(rCoordinate);
		Cell& rCell = mCells.try_emplace(rCoordinate).first->second;
		if (rCell.pNext == nullptr)
		{
			rCell.pNext = std::make_unique<game::Frame>();
		}
	}
}
#endif // BT_SERVER

void GameBase::CreateCellAtCoordinate(GridCoord coordinate)
{
	// Heap: unordered_map insertion + make_unique<Frame>. Frame persists across game lifetime
	ScopedSuppressAllocationTracking suppress;

	Cell& rCell = mCells.try_emplace(coordinate).first->second;
#if defined(BT_CLIENT)
	// Client uses snapshot ring as the source of truth — seed slot 0.
	rCell.iSnapshotHead = 0;
	rCell.iSnapshotCount = 1;
	rCell.snapshots[0] = std::make_unique<game::Frame>();
	game::Frame& rFrame = *rCell.snapshots[0];
#else
	rCell.pCurrent = std::make_unique<game::Frame>();
	game::Frame& rFrame = *rCell.pCurrent;
#endif
	rFrame.interpolate.iTick = miTickCounter;
	rFrame.interpolate.fCurrentTime = mfCurrentTime;
	rFrame.interpolate.gameFlags.Set(game::GameFlags::kGame);
	game::gpGame->InitializeFramePostRender(rFrame);

	CellStaticData& rStaticData = rCell.staticData;
	rStaticData.coordinate = coordinate;
	GenerateIslandChain(coordinate, rStaticData.islands);
	// navigationData stays empty; RunFrameTick builds it lazily on the per-coord dispatch thread.
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
	for (auto& [rCoordinate, rCell] : mCells)
	{
		std::swap(rCell.pCurrent, rCell.pNext);
	}

	// After swap, .next holds old current frames (stale data, reusable memory).
	// Ensure active entries exist for next iteration's AllocateAndCopy.
	if (!mbReplaying)
	{
		game::gpGame->EnsureNextFrames();
	}
	else if (mCells.contains(game::gpGame->mClientGridCoordinate) && mCells.at(game::gpGame->mClientGridCoordinate).pNext == nullptr)
	{
		// Heap: make_unique<Frame> for replay target coordinate
		ScopedSuppressAllocationTracking suppress;
		mCells.at(game::gpGame->mClientGridCoordinate).pNext = std::make_unique<game::Frame>();
	}
}
#endif // BT_SERVER

} // namespace engine
