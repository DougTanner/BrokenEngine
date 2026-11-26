#include "GameBase.h"

#include "Game.h"
#if defined(BT_CLIENT)
#include "Graphics/Camera.h"
#include "Input/GameKeybinds.h"
#include "Network/Client/ClientSession.h"
#endif
#if defined(BT_SERVER)
#include "Agent/Commands/ReplayHarnessRigs.h"
#include "File/Replay.h"
#include "Network/Server/ServerBroadcaster.h"
#include "Network/Server/ServerSession.h"
#include "Network/Server/ServerTransferManager.h"
#endif
#include "Frame/FrameBase.h"
#include "Input/Input.h"
#if defined(BT_CLIENT)
#include "Ui/GameSettings.h"
#include "Ui/GraphicsSettings.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/SoundSettings.h"
#endif

#include "Profile/ProfileRows.h"

namespace engine
{

GameBase::GameBase()
{
	// Registration order fixes type indices: engine collections, then the game's.
	ForEachRegister(InterpolateTypes {});
	game::FrameInterpolate::Register();

#if defined(BT_SERVER)
	mpReplay = std::make_unique<Replay>();
#endif // BT_SERVER
}

GameBase::~GameBase()
{
#if defined(BT_CLIENT)
	if (mHandleMinimizedThrottleTimer != nullptr)
	{
		CloseHandle(mHandleMinimizedThrottleTimer);
	}

	if (!(mMenuFlags & MenuFlags::kMouseVisible))
	{
		ShowCursor(TRUE);
	}
#endif // BT_CLIENT
}

#if defined(BT_CLIENT)
// Fixed policy order: InputPoll (BeginPoll) -> quit -> modal gate -> chat gate -> cursor -> pause/back-out -> engine toggles ->
// game::gpGame->ProcessGameMenuInput -> ~InputPoll (EndPoll). The modal and chat gates skip everything after them except
// ~InputPoll, so every return path still resolves quit and still advances the previous snapshot that the next frame's
// edge detection needs.
void GameBase::ProcessInput(bool bLostFocus)
{
	MenuInput menuInput {};
	InputPoll inputPoll(bLostFocus, meUiState != UiState::kNone, menuInput);

	// Escape quits only from the basic main menu; with a settings sub-menu open it backs out to the menu below instead
	if (menuInput.flags & MenuInputFlags::kQuit || (menuInput.flags & MenuInputFlags::kPauseMenu && (mGameFlags & GameFlags::kMainMenu) && meUiState == UiState::kPause))
	{
		mGameFlags.Set(GameFlags::kQuit);
	}

	if (meUiState == UiState::kModal)
	{
		return;
	}

	// The opening frame takes the gate too, so its key never reaches the engine toggles or ProcessGameMenuInput. The
	// box opens only while the Tweaks overlay is hidden, because ChatScreen renders only then.
	if (!mChatLog.bInputOpen && inputPoll.Pressed(KeyAction::kOpenChat) && !(mGameFlags & GameFlags::kMainMenu) && meUiState == UiState::kNone && !gbShowImGui)
	{
		mChatLog.bInputOpen = true;
		gpInput->mSuppressedChatOpenKey = KeybindFor(KeyAction::kOpenChat);
	}
	if (mChatLog.bInputOpen)
	{
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
		if (meUiState == UiState::kGameSettings || meUiState == UiState::kGraphicsSettings || meUiState == UiState::kKeybinds || meUiState == UiState::kSound)
		{
			LeaveSettingsScreen();
		}
		else if (meUiState == UiState::kNone)
		{
			meUiState = UiState::kPause;
		}
		else if (!(mGameFlags & GameFlags::kMainMenu))
		{
			meUiState = UiState::kNone;
		}
	}

	if (inputPoll.Pressed(KeyAction::kToggleFullscreen))
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
			gbShowImGui = !gbShowImGui;
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

		// Local pause state flips here so the pause request sent below reports the state the client just entered.
		if (menuInput.flags & MenuInputFlags::kTogglePauseFrame)
		{
			SetPaused(!(mGameFlags & GameFlags::kPaused));
		}

		// A server built without kbDebugInput strikes every debug-control request as corrupt data, so send
		// them only to a server whose accepted connection response said it takes them.
		if (gpClient != nullptr && (gpClient->mStateFlags & Client::ClientStateFlags::kServerDebugInput))
		{
			if (menuInput.flags & MenuInputFlags::kQuicksave)
			{
				gpClient->SendSimplePacket(PacketType::kClientSaveRequest, NetworkManager::kiChannelReliable, ENET_PACKET_FLAG_RELIABLE);
			}
			if (menuInput.flags & MenuInputFlags::kQuickload)
			{
				gpClient->SendSimplePacket(PacketType::kClientLoadRequest, NetworkManager::kiChannelReliable, ENET_PACKET_FLAG_RELIABLE);
			}
			if (menuInput.flags & MenuInputFlags::kSaveReplay)
			{
				gpClient->SendSimplePacket(PacketType::kClientReplayRecordRequest, NetworkManager::kiChannelReliable, ENET_PACKET_FLAG_RELIABLE);
			}
			if (menuInput.flags & MenuInputFlags::kLoadReplay)
			{
				gpClient->SendSimplePacket(PacketType::kClientReplayPlaybackRequest, NetworkManager::kiChannelReliable, ENET_PACKET_FLAG_RELIABLE);
			}
			if (inputPoll.KeyboardPressed(VK_BACK))
			{
				gpClient->SendSimplePacket(PacketType::kClientResetRequest, NetworkManager::kiChannelReliable, ENET_PACKET_FLAG_RELIABLE);
			}

			if (menuInput.flags & MenuInputFlags::kSlowTime)
			{
				gpClient->SendSimplePacket(PacketType::kClientTimespeedRequest, NetworkManager::kiChannelReliable, ENET_PACKET_FLAG_RELIABLE, 0ui8);
			}
			else if (menuInput.flags & MenuInputFlags::kSpeedUpTime)
			{
				gpClient->SendSimplePacket(PacketType::kClientTimespeedRequest, NetworkManager::kiChannelReliable, ENET_PACKET_FLAG_RELIABLE, 1ui8);
			}

			if (menuInput.flags & MenuInputFlags::kTogglePauseFrame)
			{
				gpClient->SendSimplePacket(PacketType::kClientPauseRequest, NetworkManager::kiChannelReliable, ENET_PACKET_FLAG_RELIABLE, static_cast<uint8_t>((mGameFlags & GameFlags::kPaused) ? 1 : 0));
			}
		}
	}

	if constexpr (kbScreenshots)
	{
		// Continuous dev capture queues one-shot requests each frame. An empty path uses the default %TEMP% location.
		static bool sbContinuousScreenshots = false;
		if (inputPoll.Pressed(KeyAction::kToggleScreenshots))
		{
			sbContinuousScreenshots = !sbContinuousScreenshots;
		}
		if (sbContinuousScreenshots && !gpGraphics->mOptionalScreenshotRequest)
		{
			// Fill only when empty so the per-frame default request can't overwrite an agent-issued request's
			// parameters (path / maxWidth / iCaptureToken) before the capture site consumes it.
			gpGraphics->mOptionalScreenshotRequest = ScreenshotRequest {};
		}
	}

	game::gpGame->ProcessGameMenuInput(inputPoll);
}

// The one writer of the PAUSED note; returning on no change keeps the server's echo of a SPACE pause from rewriting
// the note.
void GameBase::SetPaused(bool bPaused)
{
	if (bPaused == (mGameFlags & GameFlags::kPaused))
	{
		return;
	}
	mGameFlags.Set(GameFlags::kPaused, bPaused);

	if constexpr (kbDebugInput)
	{
		gpImGuiManager->UpdateTextArea(kTextDebug, bPaused ? "PAUSED" : "");
	}
}
#endif // BT_CLIENT

static NetworkTimeState MakeNetworkTimeState(const TimeStep& rTimeStep)
{
	return NetworkTimeState
	{
		.bFastForward = rTimeStep.miTimeMultiply > 1,
		.iExpectedUpdateIntervalMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(rTimeStep.SimulationToWall(kTickNanoseconds)).count(),
		.iExpectedUpdatesPerSecond = kiTickRate * rTimeStep.miTimeMultiply / rTimeStep.miTimeDivide,
	};
}

#if defined(BT_CLIENT)
void GameBase::ClientUpdate()
{
	common::LogTickScope logTickScope(miTickCounter);

	{
		// Heap: transport receive buffers and game packet/frame adoption
		ScopedSuppressAllocationTracking suppress;
		NetworkTimeState networkTimeState = MakeNetworkTimeState(mTimeStep);
		game::gpClientSession->mpRuntime->PollAndDrain(networkTimeState);
	}

	if constexpr (kbDebugInput)
	{
		// ProcessInput's pause toggle also writes kTextDebug and runs earlier each frame: this note replaces a same-frame PAUSED, and a later pause press replaces this note.
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

	game::gpClientSession->mpRuntime->UpdateDesiredCoordinates("kPollTick");
	game::gpClientSession->mpRuntime->SynchronizeSubscriptions();
	PrepareActiveSet();

	// Hard ceiling: clock-servo target + kiSimulationCeilingSlackTicks. EvaluateClock steers the
	// sim toward the bare target, and the target and this ceiling both read the estimated latest server
	// tick, which keeps advancing through an arrival gap for up to kiMaximumEstimatedServerTickLead ticks,
	// so this clamp freezes the sim until the next arrival only once a gap outlasts that cap, not on per-packet
	// jitter or ordinary loss bursts. While a lead left by a gap, a debug pause, or a server hitch longer than the
	// server's kiMaximumAccumulatorTicks cap is above the slack, the estimate eases back by holding still at some
	// arrivals, and each such hold stalls a sim at the ceiling.
	// Extreme "sim way behind target" is handled by the snap path in Reconcile.
	const engine::ClientSessionRuntime& rRuntime = *game::gpClientSession->mpRuntime;
	int64_t iEstimatedLatestServerTick = rRuntime.EstimatedLatestServerTick();
	int64_t iCeiling = iEstimatedLatestServerTick < 0 ? -1 : iEstimatedLatestServerTick - rRuntime.miCurrentTargetBehind + engine::kiSimulationCeilingSlackTicks;
	int64_t iAbsorbedTicks = 0;
	if (iEstimatedLatestServerTick >= 0 && iFullTicks > 0)
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

	// When the estimated latest server tick stops at its cap (an arrival gap longer than
	// kiMaximumEstimatedServerTickLead, such as a server stop), the ceiling freezes the sim entirely, then
	// releases the backlog as a burst — log at the transition. Frequent stalls without such a gap, a debug pause,
	// or a server hitch indicate a clock-servo / kiSimulationCeilingSlackTicks tuning problem (the servo should
	// keep steady-state jitter away from the ceiling).
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
	// lockstep with miTickCounter. PrepareActiveSet runs earlier and has no client-side reader of this delta.
	mfLastDeltaTime = static_cast<float>(iFullTicks) * kfDeltaTime;

	miTickCounter += iFullTicks;

	gpProfileManager->CpuStart(game::kCpuTimerFrameUpdate);
	{
		if constexpr (kbProfilingFrameSpike)
		{
			gpProfileManager->mReconciliationDiagnostics = {};
		}
		ScopedSlowOperation scopedSlowOperation(kNetwork, "Client reconciliation", &gpProfileManager->mReconciliationDiagnostics);
		game::gpClientSession->Reconcile();
	}
	gpProfileManager->CpuStop(game::kCpuTimerFrameUpdate);

	if constexpr (kbProfiling)
	{
		gpProfileManager->mFullUpdatesInTheLastSecond.Set(iFullTicks);
	}
}
#endif // BT_CLIENT

#if defined(BT_SERVER)
bool GameBase::HasReplayTickWork() const
{
	return (!gpReplay->mReplayWriters.empty()) || mbReplaying || (mGameFlags & GameFlags::kSaveReplay);
}

void GameBase::ServerUpdate()
{
	common::LogTickScope logTickScope(miTickCounter);

	NetworkTimeState networkTimeState = MakeNetworkTimeState(mTimeStep);
	game::gpServerSession->mpRuntime->Poll(networkTimeState);

	// Drain agent commands with the debug-control packets, before SaveLoadReplay/PrepareActiveSet so
	// flag-setting commands are consumed the same update and injected StatusChanges enter the tick's frame input.
	if (gpAgentCommandServer != nullptr) [[unlikely]]
	{
		gpAgentCommandServer->Drain();
	}

	gpReplay->SaveLoadReplay();

	game::gpServerSession->mpRuntime->WaitForTick(mTimeStep);

	int64_t iFullTicks = mTimeStep.TickRealtime();
	// Second poll of the update: drains commands that arrived during WaitForTick so they enter the imminent
	// tick rather than the next one. It runs before the pause/timespeed decision below, so a pause request
	// arriving in the boundary window (when kbDebugInput is enabled) prevents the imminent tick. A record-stop
	// or load-replay request arriving here is serviced next update by design -- SaveLoadReplay already ran; a
	// record-start request waits for the first dispatched tick's SyncReplayTick.
	game::gpServerSession->mpRuntime->PollTickBoundary(networkTimeState);
	bool bAcceptRawCpuTimers = false;
	if constexpr (kbProfiling)
	{
		bAcceptRawCpuTimers = iFullTicks == 1 && mTimeStep.miTimeMultiply == 1 && mTimeStep.miTimeDivide == 1
		                   && !(mGameFlags & GameFlags::kPaused) && !HasReplayTickWork();
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
	if (ReplayHarnessRigs::IsWriterPauseArmed(*gpReplay) && iFullTicks > 1)
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
		++miTickCounter;

		common::LogTickScope perTickScope(miTickCounter);

		game::gpServerSession->PrepareTick();

		if (mbReplaying) [[unlikely]]
		{
			// Pick up readers activated by the previous iteration's SyncReplayTick, which the pre-loop PrepareActiveSet
			// only sees once per update. Must run before this iteration's SyncReplayTick: the recording never simulated
			// a coord at its activation tick E, so it may not enter E's dispatch list — its first dispatch is E + 1.
			RefreshReplayActiveSet();
		}

		if (HasReplayTickWork()) [[unlikely]]
		{
			if (gpReplay->SyncReplayTick() == Replay::ReplayTickDecision::kStopBeforeDispatch)
			{
				if constexpr (kbProfiling)
				{
					gpProfileManager->LatchRawCpuTimers(false);
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
					iFullTicks = iFinalizedTicks;
					mfLastDeltaTime = static_cast<float>(iFinalizedTicks) * kfDeltaTime;
					// Rebuild only active frames; normal input/manager progression resumes on the next server update.
					game::gpServerSession->mpRuntime->ComputeActiveSet();
				}
				break;
			}
		}

		BuildAndDispatchFrameTicks(rActiveCoordinates);
		if constexpr (kbProfiling)
		{
			gpProfileManager->LatchRawCpuTimers(bAcceptRawCpuTimers);
		}
		FinalizeFrameTick();
		++iFinalizedTicks;
		if (miTickCounter == miPauseAtTick) [[unlikely]]
		{
			mGameFlags.Set(GameFlags::kPaused);
			miPauseAtTick = -1;
		}
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
			gpProfileManager->LatchRawCpuTimers(false);
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

	gpProfileManager->mFullUpdatesInTheLastSecond.Set(iFullTicks);

	mServerSaveLoad.TickAutosave();
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
			RunFrameTick(rActiveFrameReferences.at(static_cast<size_t>(j)), miTickCounter);
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
		game::gpServerSession->mpTransferManager->HarvestTransfers();
	}
	else
	{
		game::gpServerSession->mpTransferManager->ApplyReplayTransfers();
	}

	SwapFrames();

	game::gpServerSession->mpRuntime->CompleteTick(miTickCounter);

	if (HasReplayTickWork()) [[unlikely]]
	{
		// Heap: the game's replay checksum serialization and the recording's per-tick checksum list
		ScopedSuppressAllocationTracking suppress;
		game::FinalizeReplayTick(!gpReplay->mReplayWriters.empty());
	}

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
	// the newer committed ticks up to tail held as starvation cushion. When the ring holds fewer
	// slots (cold start, or a reconcile whose base has fewer committed ticks above it, such as a
	// just-adopted full state), fall back to the oldest available; callers force fDeltaTime = 0
	// for that coord so no extrapolation occurs.
	const Cell& rCell = mCells.at(coordinate);
	ASSERT(rCell.iSnapshotCount > 0);
	int64_t iDesiredLogical = rCell.iSnapshotCount - 1 - kiRenderBehindTicks;
	int64_t iLogical = std::max<int64_t>(0, iDesiredLogical);
	int64_t iPhysical = SnapshotIndex(rCell.iSnapshotHead, iLogical);
	ASSERT(rCell.ppSnapshots[iPhysical] != nullptr);
	return *rCell.ppSnapshots[iPhysical];
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
	// this frame, so the camera-anchored render-clock / interpolate / RenderFrame work in
	// RenderInterpolation::UpdateRenderInterpolation is skipped.
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
		mOptionalMinimizedThrottleLast.reset(); // Recreate resumed: drop the throttle timestamp so a fresh minimize starts clean.
		gpGraphics->mRenderFrameTimer.Reset(); // The next frame delta steps wind, so it must not span the skipped interval.
		gpGraphics->mRenderFrameDeltaNanoseconds = 0ns;
	}
	else
	{
		// Still deferred (minimized / off-screen): this branch loops every frame with no vkQueuePresentKHR to
		// throttle it, so wait out the rest of one sim tick's wall duration, bounded between the ~1 ms floor and the
		// unscaled tick — fast time scales cannot busy-spin a core and re-issue a
		// vkGetPhysicalDeviceSurfaceCapabilitiesKHR per spin, and slow ones cannot stall the next iteration's message pump
		// and agent command drain. Mirrors ServerSessionRuntime::WaitForTick's high-resolution
		// waitable timer minus its precision spin (nothing minimized needs sub-ms accuracy).
		if (mHandleMinimizedThrottleTimer == nullptr)
		{
			mHandleMinimizedThrottleTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
		}

		if (mOptionalMinimizedThrottleLast.has_value())
		{
			static constexpr std::chrono::nanoseconds kMinimumThrottleNanoseconds = 1'000'000ns;
			std::chrono::nanoseconds elapsedNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - *mOptionalMinimizedThrottleLast);
			std::chrono::nanoseconds budgetNanoseconds = std::clamp(mTimeStep.SimulationToWall(kTickNanoseconds), kMinimumThrottleNanoseconds, kTickNanoseconds);
			std::chrono::nanoseconds remainingNanoseconds = budgetNanoseconds - elapsedNanoseconds;
			if (remainingNanoseconds > 0ns)
			{
				LARGE_INTEGER dueTime {.QuadPart = -(remainingNanoseconds.count() / 100)}; // Negative = relative, 100ns units.
				if (mHandleMinimizedThrottleTimer != nullptr && SetWaitableTimerEx(mHandleMinimizedThrottleTimer, &dueTime, 0, nullptr, nullptr, nullptr, 0) != 0)
				{
					WaitForSingleObject(mHandleMinimizedThrottleTimer, INFINITE);
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
		mOptionalMinimizedThrottleLast = std::chrono::steady_clock::now();
	}
	return true;
}

// The camera-relative effects a world-hidden stretch would leave stale at the old view, cleared once as the world hides
// so they restart clean when it returns.
static void ClearRetainedEffects()
{
	gbSmokeClear = true;
	gpParticleManager->mbReset = true;
	WindTrailsInterpolate::sPreviousPositions.clear();
	gbWaterFoamReset = true;
	gbObjectShadowsReset = true;
	// Wind has no per-frame clear; the one-shot clear needs the graphics queue drained.
	CHECK_VK(vkQueueWaitIdle(gpDeviceManager->mGraphicsVkQueue));
	gpTextureManager->mRenderTargetTextures.ClearWindTextures();
}

void GameBase::Render()
{
	const std::vector<GridCoord>& rActiveCoordinates = mActiveCoordinates;

	// Interpolate elapsed time with the sub-step remainder for smooth rendering; pinned graphics drop it to stay
	// tick-exact.
	std::chrono::nanoseconds tickRemainder = gbGraphicsPinned ? 0ns : std::max(0ns, mTimeStep.mTickRemainderNanoseconds);
	std::chrono::duration<float> elapsedTime(common::NanosecondsToFloatSeconds<float>((TickToSimulationTime(miTickCounter) + tickRemainder).time_since_epoch()));

	GridCoord cameraCoordinate = mClientGridCoordinate;
	bool bHaveRenderableCamera = false;
	SelectRenderCamera(rActiveCoordinates, cameraCoordinate, bHaveRenderableCamera);
	bool bHasRenderableWorldBasis = mRenderInterpolation.UpdateRenderInterpolation(*this, rActiveCoordinates, cameraCoordinate, bHaveRenderableCamera);

	if (HandleDeferredSwapchain())
	{
		// RenderGlobal is skipped, so drop this frame's staged spawns rather than letting them pile up past the staging limit.
		gpParticleManager->DiscardStagedSpawns();
		// RenderMainPresentAcquire is skipped too, so signal the upload thread here to keep transfer uploads moving while the swapchain is deferred.
		gpTextureUploadManager->SignalFrame();
		return;
	}

	// Island cells: confirmed cells and the local render camera (Observer mode restricts to confirmed active
	// subscriptions, matching panning), skipping empty-ring cells that have nothing rendered. The pointers use
	// per-frame workbuffer storage that must stay open through the render call that writes them.
	bool bObserverActive = gpCamera->meObserverState == CameraBase::ObserverState::kObserving && !(mGameFlags & GameFlags::kMainMenu);
	common::ScopedWorkbufferArena islandCellsArena = common::gpThreadLocal->mWorkbuffer.Push();
	for (const GridCoord& rCoordinate : rActiveCoordinates)
	{
		auto it = mCells.find(rCoordinate);
		if (it == mCells.end() || it->second.iSnapshotCount == 0)
		{
			continue;
		}
		if (bObserverActive)
		{
			if (it->second.iConfirmedTick < 0 || gpClient == nullptr)
			{
				continue;
			}
			bool bSubscribed = std::ranges::any_of(gpClient->mSubscriptions.mCoordinateSlots, [&](const ClientCoordSlot& rSlot)
			{
				return rSlot.eState == CoordSubscriptionState::kActive && rSlot.coordinate == rCoordinate;
			});
			if (!bSubscribed)
			{
				continue;
			}
		}
		else if (it->second.iConfirmedTick < 0 && !(bHaveRenderableCamera && rCoordinate == cameraCoordinate))
		{
			continue;
		}
		islandCellsArena.mBuffer.PushBack<const CellStaticData*>(&it->second.staticData);
	}
	std::span<const CellStaticData* const> activeIslandCells = islandCellsArena.mBuffer.Span<const CellStaticData* const>();

	// Behind an opaque map overlay, or while Observer mode awaits a renderable subscribed basis, no world pass is
	// recorded or submitted; the frame still runs the game's world-hidden step and the residency step, then submits
	// the acquire command buffer when recorded and the UI.
	bool bWorldHidden = game::gpGame->MapOpacity() >= 1.0f || !bHasRenderableWorldBasis;
	static bool sbWorldHidden = false;
	if (bWorldHidden && !sbWorldHidden)
	{
		ClearRetainedEffects();
	}
	sbWorldHidden = bWorldHidden;
	if (bWorldHidden)
	{
		// The skipped collection render walk also issues each model's once-only texture request; the game's world-hidden step
		// does it with no GPU write, so the textures start loading while the world is hidden.
		game::FrameInterpolate::UpdateWorldHidden(mRenderInterpolation.mRenderInterpolates, rActiveCoordinates);
		// RenderGlobal is skipped, so drop this frame's staged spawns as the deferred-swapchain skip does.
		gpParticleManager->DiscardStagedSpawns();
		gpGraphics->RenderUiPresentAcquire(activeIslandCells, gpSwapchainManager->miFramebufferIndex);
		return;
	}

	gbGameplayPaused = mGameFlags & GameFlags::kPaused;
	gpGraphics->RenderGlobal(activeIslandCells, elapsedTime);

	// Capture command buffer index before async launch to avoid re-reading in async thread
	int64_t iCommandBuffer = gpSwapchainManager->miFramebufferIndex;
	gpGraphics->RenderMainPresentAcquire(iCommandBuffer, mRenderInterpolation.mRenderInterpolates, rActiveCoordinates, gpCamera->mBasisCoordinate, elapsedTime);

}

bool GameBase::ShouldUseCrosshair() const
{
	auto it = mCells.find(mClientGridCoordinate);
	return it != mCells.end() && it->second.TailFrame() != nullptr && !(mGameFlags & GameFlags::kMainMenu) && meUiState == UiState::kNone;
}
#endif // BT_CLIENT

#if defined(BT_SERVER)
void GameBase::RefreshReplayActiveSet()
{
	// During replay, all coords with live readers are active; SyncReplayTick retires ended readers before dispatch.
	// Heap: vector clear/reserve/push_back
	ScopedSuppressAllocationTracking suppress;
	mActiveCoordinates.clear();
	mActiveCoordinates.reserve(gpReplay->mReplayReaders.size());
	for (const auto& [rCoordinate, rpReader] : gpReplay->mReplayReaders)
	{
		mActiveCoordinates.push_back(rCoordinate);
	}
}
#endif // BT_SERVER

game::Frame& GameBase::CreateCellFrame(Cell& rCell, GridCoord coordinate)
{
#if defined(BT_CLIENT)
	// Client uses snapshot ring as the source of truth — seed slot 0.
	rCell.iSnapshotHead = 0;
	rCell.iSnapshotCount = 1;
	rCell.ppSnapshots[0] = std::make_unique<game::Frame>();
	game::Frame& rFrame = *rCell.ppSnapshots[0];
#else
	rCell.pCurrent = std::make_unique<game::Frame>();
	game::Frame& rFrame = *rCell.pCurrent;
#endif
	rFrame.postRender.uiFrameIdentifier = muiNextFrameId++;
	rFrame.postRender.randomEngine.TimeSeed();
	game::gpGame->InitializeFramePostRender(rFrame);

	rCell.staticData.coordinate = coordinate;
	return rFrame;
}

void GameBase::CreateCellAtCoordinate(GridCoord coordinate)
{
#if defined(BT_CLIENT)
	if (!(mGameFlags & GameFlags::kMainMenu))
	{
		game::gpClientSession->mpRuntime->StageLocalCell(coordinate);
		return;
	}
#endif
	// Heap: unordered_map insertion + make_unique<Frame>. Frame persists across game lifetime
	ScopedSuppressAllocationTracking suppress;

	Cell& rCell = mCells.try_emplace(coordinate).first->second;
	game::Frame& rFrame = CreateCellFrame(rCell, coordinate);
	rFrame.interpolate.iTick = miTickCounter;

	GenerateIslandChain(coordinate, rCell.staticData.islands);
	// navigationData stays empty; RunFrameTick builds it lazily on the per-coord dispatch thread.
}

void GameBase::CreateNewFrame()
{
	// Heap: make_unique<Frame> with all its SOA collections. Frame persists across the entire
	// game state lifetime, so workbuffer (lost on Pop) can't hold it.
	ScopedSuppressAllocationTracking suppress;

#if defined(BT_CLIENT)
	if (!(mGameFlags & GameFlags::kMainMenu))
	{
		game::gpClientSession->mpRuntime->ClearSubscriptionState();
		mCells.clear();
		CreateCellAtCoordinate(kOriginCoordinate);
		return;
	}
#endif
	mCells.clear();
	Cell& rCell = mCells.try_emplace(kOriginCoordinate).first->second;
	CreateCellFrame(rCell, kOriginCoordinate);

#if defined(BT_CLIENT)
	// kbDebugInput turns the main-menu cell into a single centered island browser ('E' cycles it);
	// otherwise it keeps the procedural island chain. Gameplay cells always use the chain.
	bool bMenuBrowse = false;
	if constexpr (kbDebugInput)
	{
		bMenuBrowse = static_cast<bool>(mGameFlags & GameFlags::kMainMenu);
	}
	if (bMenuBrowse)
	{
		BuildMenuIslandPlacement(miMenuIslandIndex, rCell.staticData.islands);
		return;
	}
#endif // BT_CLIENT
	GenerateIslandChain(kOriginCoordinate, rCell.staticData.islands);
	// navigationData stays empty; RunFrameTick builds it lazily on the per-coord dispatch thread.
}

#if defined(BT_CLIENT)
// Developer main-menu island browser: build the origin cell as a single island centered at (0,0),
// selected by index into the boot-fixed area-sorted list of all packed islands (largest footprint
// first). clear()+push_back reuses the vector's capacity (grown once under
// ScopedSuppressAllocationTracking in CreateNewFrame), so the cycle path never heap-allocates in the
// main loop.
void GameBase::BuildMenuIslandPlacement(int64_t iIndex, std::vector<IslandPlacement>& rOutput)
{
	rOutput.clear();
	common::crc_t islandCrc = gpIslandTerrain->mIslandCrcsByArea.at(static_cast<size_t>(iIndex));
	rOutput.push_back({.islandCrc = islandCrc, .f2WorldPosition = {0.0f, 0.0f}, .fRotation = 0.0f});
}

void GameBase::ChangeFrameToMainMenu()
{
	if (meUiState == UiState::kKeybinds)
	{
		LeaveSettingsScreen();
	}

	game::gpClientSession->mpRuntime->Disconnect();

	ASSERT(!(mGameFlags & GameFlags::kMainMenu));

	game::gpGame->StartMenuMusic();
	static_cast<game::Camera&>(*gpCamera).StorePose(false);

	mGameFlags.Set(GameFlags::kMainMenu);
	CreateNewFrame();
	ResetSession();
	static_cast<game::Camera&>(*gpCamera).ApplyPose(true);
}

void GameBase::ChangeFrameToGame()
{
	if (meUiState == UiState::kKeybinds)
	{
		LeaveSettingsScreen();
	}

	ASSERT(mGameFlags & GameFlags::kMainMenu);

	game::gpGame->StartGameMusic();
	static_cast<game::Camera&>(*gpCamera).StorePose(true);

	mGameFlags.Set(GameFlags::kMainMenu, false);
	ResetSession();
	CreateNewFrame();
	static_cast<game::Camera&>(*gpCamera).ApplyPose(false);
	meUiState = UiState::kNone;
}

void GameBase::ApplyStandardMenuAction(StandardMenuAction eAction)
{
	switch (eAction)
	{
		case StandardMenuAction::kStartDiscovery:
			game::gpClientSession->mpRuntime->StartDiscovery();
			break;
		case StandardMenuAction::kConnectToDiscoveredServer:
			game::gpClientSession->mpRuntime->ConnectToDiscoveredServer(kiDefaultPort);
			break;
		case StandardMenuAction::kChangeFrameToMainMenu:
			ChangeFrameToMainMenu();
			break;
	}
}

void GameBase::LeaveSettingsScreen()
{
	if (meUiState == UiState::kGameSettings)
	{
		SaveGameSettings();
	}
	else if (meUiState == UiState::kGraphicsSettings)
	{
		SaveGraphicsSettings();
	}
	else if (meUiState == UiState::kKeybinds)
	{
		gpInput->CancelKeybindCapture();
		SaveEngineKeybinds();
		game::SaveGameKeybinds();
	}
	else if (meUiState == UiState::kSound)
	{
		SaveSoundSettings();
	}

	meUiState = UiState::kPause;
}
#endif // BT_CLIENT

void GameBase::PrepareActiveSet()
{
#if defined(BT_SERVER)
	if (mbReplaying)
	{
		RefreshReplayActiveSet();
		// SyncReplayTick creates and overwrites each recorded FrameInput. Do not advance normal server managers here.
		return;
	}

	game::gpServerSession->mpRuntime->ComputeActiveSet();
	game::gpServerSession->mpBroadcaster->BuildFrameInputs();
#else
	game::gpClientSession->mpRuntime->ComputeActiveSet();
	game::gpGame->UpdateCameraShake();
#endif // BT_SERVER
}

void GameBase::SetClientGridCoordinate(GridCoord coordinate)
{
#if defined(BT_CLIENT)
	if (game::gpClientSession->mpRuntime->mStateFlags & ClientSessionStateFlags::kClientGridCoordinatePinned)
	{
		return;
	}
#endif // BT_CLIENT
	mClientGridCoordinate = coordinate;
#if defined(BT_CLIENT)
	game::gpClientSession->mpRuntime->miVisibleNeighborCount = 0;
#endif // BT_CLIENT
}

void GameBase::ResetSession()
{
	LOG(kDefault, kDebug, "GameBase::ResetSession()");

	miTickCounter = 0;

#if defined(BT_SERVER)
	gpReplay->ResetStreams();
	mGameFlags.Set(GameFlags::kSaveReplay, false);
#endif // BT_SERVER

#if defined(BT_CLIENT)
	game::gpClientSession->mpRuntime->ClearSubscriptionState();
	gpCamera->ResetForSession();
	gbSmokeClear = true;
	gpParticleManager->mbReset = true;
	WindTrailsInterpolate::sPreviousPositions.clear();
#endif // BT_CLIENT

	mGameFlags.Set(GameFlags::kPaused, false);
	game::gpGame->ResetGameState();
#if defined(BT_CLIENT)
	game::gpClientSession->mpRuntime->mStateFlags.Set(ClientSessionStateFlags::kClientGridCoordinatePinned, false);
#endif // BT_CLIENT
	SetClientGridCoordinate(kOriginCoordinate);
	mActiveCoordinates.clear();
#if defined(BT_CLIENT)
	if (!(mGameFlags & GameFlags::kMainMenu))
	{
		return;
	}
#endif
	mActiveCoordinates.push_back(mClientGridCoordinate);
}

#if defined(BT_SERVER)
void GameBase::SwapFrames()
{
	for (auto& [rCoordinate, rCell] : mCells)
	{
		std::swap(rCell.pCurrent, rCell.pNext);
	}

	// After swap, .next holds old current frames (stale data, reusable memory).
}
#endif // BT_SERVER

} // namespace engine
