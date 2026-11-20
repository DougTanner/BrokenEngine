#include "RenderInterpolation.h"

#include "GameBase.h"

#if defined(BT_CLIENT)

#include "Graphics/Camera.h"
#include "Profile/ProfileRows.h"

namespace engine
{

void RenderInterpolation::ResetRenderClock()
{
	mfRenderTime = 0.0;
	mbRenderClockSeeded = false;
	mRenderTimer.Reset();
}

// Render-side sim clock advance. Returns fDeltaTime in [0, kfDt], the sub-tick interpolation offset from the window
// start fWindowStartTime, the source frame's tick instant. bPaused freezes the clock; bHaveInterpolationWindow gates
// the seeded steady-state path. Preserve exact float ops — client-render-clock invariants.
float RenderInterpolation::AdvanceRenderClock(double fWindowStartTime, bool bPaused, bool bHaveInterpolationWindow, double fSimulationDeltaSeconds)
{
	float fDeltaTime = 0.0f;
	if (bPaused)
	{
		// Freeze. Don't advance mfRenderTime; rendered scene stays static until unpause.
		fDeltaTime = static_cast<float>(std::clamp(mfRenderTime - fWindowStartTime, 0.0, static_cast<double>(kfDeltaTime)));
	}
	else if (!bHaveInterpolationWindow)
	{
		// Reset the seed flag so the next steady-state entry re-seeds at midpoint.
		mfRenderTime = fWindowStartTime;
		mbRenderClockSeeded = false;
		fDeltaTime = 0.0f;
	}
	else
	{
		// Seed at window midpoint so jitter has symmetric headroom before hitting either clamp.
		if (!mbRenderClockSeeded)
		{
			mbRenderClockSeeded = true;
			mfRenderTime = fWindowStartTime + 0.5 * kfDeltaTime;
		}

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

void RenderInterpolation::UpdateRenderInterpolation(GameBase& rGame, const std::vector<GridCoord>& rActiveCoordinates, GridCoord cameraCoordinate, bool bHaveRenderableCamera)
{
	// Per-frame render interpolates and camera update before RenderGlobal so that
	// mf4RenderVisibleArea and lighting area are computed from the current camera position
	{
		ScopedSuppressAllocationTracking suppress;

		// Remove render interpolates for deactivated coords AND coords whose snapshot ring is empty, so
		// mRenderInterpolates holds entries for exactly the renderable (iSnapshotCount > 0) active coords.
		// This must run even when the active set is empty or bHaveRenderableCamera is false so no stale
		// entry survives to be rendered by RenderFrameMain.
		std::erase_if(mRenderInterpolates, [&](const std::pair<const GridCoord, game::FrameInterpolate>& rPair)
		{
			return !std::ranges::contains(rActiveCoordinates, rPair.first) || !rGame.IsCoordinateRenderable(rPair.first);
		});
	}
	if (rActiveCoordinates.empty())
	{
		return;
	}

	gpProfileManager->CpuStart(game::kCpuTimerFrameUpdate);
	gpProfileManager->CpuStart(game::kCpuTimerFrameInterpolate);

	double fSimulationDeltaSeconds = common::NanosecondsToFloatSeconds<double>(rGame.mTimeStep.WallToSimulation(mRenderTimer.GetDeltaNs(true)));
	mfLastRenderFrameSeconds = fSimulationDeltaSeconds;
	// Without a renderable camera coord fDeltaTime stays 0; the per-coord interpolate below still runs.
	float fDeltaTime = 0.0f;
	float fSunDeltaTime = 0.0f;
	if (bHaveRenderableCamera)
	{
		const Cell& rCameraCell = rGame.mCells.at(cameraCoordinate);
		bool bHaveInterpolationWindow = (rCameraCell.iSnapshotCount >= kiRenderBehindTicks + 1);
		// The source frame's tick instant is the START of the current render window. Promoted to double so
		// mfRenderTime - fWindowStartTime keeps nanosecond precision even after hours of accumulated game time (float ULP
		// at ~16384s is 2ms, which would otherwise quantize the alpha and visibly stutter at high zoom).
		const game::Frame& rSourceFrame = rGame.RenderFrame(cameraCoordinate);
		double fWindowStartTime = common::NanosecondsToFloatSeconds<double>(TickToSimulationTime(rSourceFrame.interpolate.iTick).time_since_epoch());

		bool bPaused = rGame.mGameFlags & GameFlags::kPaused;

		double fRenderTimeBefore = mfRenderTime;
		fDeltaTime = AdvanceRenderClock(fWindowStartTime, bPaused, bHaveInterpolationWindow, fSimulationDeltaSeconds);
		// The sun follows on-screen game time. The clamp keeps a seed, rebase, or restart jump from turning it backward
		// or moving it more than this frame's sim delta.
		fSunDeltaTime = static_cast<float>(std::clamp(mfRenderTime - fRenderTimeBefore, 0.0, fSimulationDeltaSeconds));
		if (gbGraphicsPinned)
		{
			fDeltaTime = 0.0f;
			fSunDeltaTime = 0.0f;
			gpCamera->mfSunAngle = CameraBase::kfDefaultSunAngle;
		}
	}
	{
		ScopedSuppressAllocationTracking suppress;

		// Heap: try_emplace can allocate frame-interpolation entries, so allocation tracking stays suppressed in this scope.
		// AllocateAndCopy + Update each active frame's render interpolate (camera frame first).
		// fCoordinateDeltaTime is 0 for a coord left transiently under-populated by a post-fast-path shrink.
		auto InterpolateFrame = [&](const GridCoord& rCoordinate)
		{
			const game::Frame& rFrame = rGame.RenderFrame(rCoordinate);
			Cell& rCell = rGame.mCells.at(rCoordinate);
			if (rCell.iConfirmedTick >= 0)
			{
				if (rFrame.interpolate.iTick < rCell.iLastRenderedTick)
				{
					LOG(kNetwork, kError, "Render regressed to older frame Coord: ({},{}) Tick: {} LastTick: {}", rCoordinate.iQ, rCoordinate.iR, rFrame.interpolate.iTick, rCell.iLastRenderedTick);
					DEBUG_BREAK();
				}
				rCell.iLastRenderedTick = rFrame.interpolate.iTick;
			}
			float fCoordinateDeltaTime = (rCell.iSnapshotCount >= kiRenderBehindTicks + 1) ? fDeltaTime : 0.0f;
			game::FrameInterpolate::AllocateAndCopy(mRenderInterpolates.try_emplace(rCoordinate).first->second, rFrame.interpolate);
			game::FrameInterpolate::Update(mRenderInterpolates.at(rCoordinate), rFrame, fCoordinateDeltaTime, rCell.staticData);
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
			// (mirrors the island-cell empty-ring skip in GameBase::Render).
			if (!rGame.IsCoordinateRenderable(rCoordinate))
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

	// Without a renderable camera the camera holds its last state, and
	// mRenderInterpolates.at(cameraCoordinate) would throw on the absent entry.
	if (bHaveRenderableCamera)
	{
		UpdateCamera(rGame, cameraCoordinate, fSunDeltaTime);
	}
}

void RenderInterpolation::UpdateCamera(const GameBase& rGame, GridCoord cameraCoordinate, float fSunDeltaTime)
{
	const game::FrameInterpolate& rFrameInterpolate = mRenderInterpolates.at(cameraCoordinate);
	bool bMainMenuFrame = rGame.mGameFlags & GameFlags::kMainMenu;
	CameraTarget pulledTarget = static_cast<game::Camera&>(*gpCamera).PullTarget(rFrameInterpolate);
	gpCamera->Update(rFrameInterpolate, static_cast<float>(mfLastRenderFrameSeconds), fSunDeltaTime, bMainMenuFrame, pulledTarget);

	// Vibration follows the shake Update just decayed.
	float fVibration = std::pow(gpCamera->mfShake, 0.5f);
	gpRawInputManager->SetVibration(fVibration, fVibration);
}

} // namespace engine

#endif // BT_CLIENT
