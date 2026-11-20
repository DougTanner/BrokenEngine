#pragma once

#if defined(BT_CLIENT)

#include "Frame/Frame.h"

namespace engine
{

class GameBase;

class RenderInterpolation
{
public:

	void ResetRenderClock();
	void UpdateRenderInterpolation(GameBase& rGame, const std::vector<GridCoord>& rActiveCoordinates, GridCoord cameraCoordinate, bool bHaveRenderableCamera);

	// Pulls the game camera's target, runs the display-rate camera update for the coord the camera renders, and sets
	// controller vibration from the decayed shake; shared by GameBase::Render() and the Main.cpp boot render.
	// fSunDeltaTime is the render clock's advance this frame limited to [0, this frame's sim delta], or 0 while
	// gbGraphicsPinned, so the sun stops whenever on-screen game time stops.
	void UpdateCamera(const GameBase& rGame, GridCoord cameraCoordinate, float fSunDeltaTime);

	// Per-coord render interpolation. GameBase::Render() and the Main.cpp boot path own its lifecycle and forward it by
	// const ref into Graphics::RenderMainPresentAcquire (Graphics never touches it).
	std::unordered_map<GridCoord, game::FrameInterpolate> mRenderInterpolates;

private:

	float AdvanceRenderClock(double fWindowStartTime, bool bPaused, bool bHaveInterpolationWindow, double fSimulationDeltaSeconds);

	// Sim-scaled seconds elapsed since the previous render frame: wall delta multiplied by the active time
	// ratio (equal to wall time at ratio 1.0). UpdateCamera passes it to CameraBase::Update.
	double mfLastRenderFrameSeconds = 0.0;

	// Render clock advances in simulation seconds, with wall time scaled by the current time ratio, so phase holds at any
	// time ratio without a servo, and is clamped to the one-tick window starting at RenderFrame's source time, so
	// rendering never extrapolates past committed ticks. A midpoint seed sets phase; rebasing occurs only outside
	// [source - kfDeltaTime, source + 2 * kfDeltaTime]. Single-tick commits preserve continuity through the
	// Update(N, kfDeltaTime) == Update(N+1, 0) interpolation invariant.
	double mfRenderTime = 0.0;
	bool mbRenderClockSeeded = false;
	common::Timer mRenderTimer;
};

} // namespace engine

#endif // BT_CLIENT
