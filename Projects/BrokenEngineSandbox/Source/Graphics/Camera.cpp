#include "Camera.h"

#if defined(BT_CLIENT)

#include "Ui/GraphicsSettingsWrappersBase.h"

#include "Frame/Collections/Players/Players.h"
#include "Frame/Frame.h"
#include "Game.h"

namespace game
{

Camera::Camera()
: engine::CameraBase(engine::CameraSetup {.vecInitialPosition = XMVectorAdd(XMVectorAdd(kVecMenuIslandCenter, kVecMenuCameraOffset), XMVectorSet(0.0f, 0.0f, engine::gBaseHeight.mfCurrent, 0.0f))})
{
}

bool Camera::IsMainMenuFrame(const engine::FrameInterpolateBase& rFrameInterpolate) const
{
	// GameBase guarantees the concrete render interpolate is the game frame type.
	return static_cast<const FrameInterpolate&>(rFrameInterpolate).gameFlags & GameFlags::kMainMenu;
}

engine::CameraTarget Camera::PullTarget(const engine::FrameInterpolateBase& rFrameInterpolate)
{
	const FrameInterpolate& rGameInterpolate = static_cast<const FrameInterpolate&>(rFrameInterpolate);

	// Main-menu frames and an invalid client-player identity use the canonical menu pose to avoid retaining a stale gameplay target.
	if (rGameInterpolate.gameFlags & GameFlags::kMainMenu)
	{
		return engine::CameraTarget::Direct(XMVectorAdd(XMVectorAdd(kVecMenuIslandCenter, kVecMenuCameraOffset), XMVectorSet(0.0f, 0.0f, engine::gBaseHeight.mfCurrent, 0.0f)));
	}

	if (!(gpGame->ClientPlayerIdentifier().iValue != 0))
	{
		return engine::CameraTarget::Direct(XMVectorAdd(XMVectorAdd(kVecMenuIslandCenter, kVecMenuCameraOffset), XMVectorSet(0.0f, 0.0f, engine::gBaseHeight.mfCurrent, 0.0f)));
	}

	// The interpolate's own cell, not the client cell: its positions are local to that cell, and the camera works in
	// that same frame, so a target taken from it needs no conversion. Reading the index from a different cell's
	// players would pick a position a whole cell away.
	engine::GridCoord coordinate = rGameInterpolate.renderBasis.coordinate;
	auto it = gpGame->mCells.find(coordinate);
	bool bHasCoordinate = it != gpGame->mCells.end() && it->second.iSnapshotCount > 0;
	std::optional<int64_t> oPlayerIndex = bHasCoordinate ? gpGame->ClientPlayerIndex(*gpGame->RenderFrame(coordinate).postRender.pPlayers) : std::nullopt;
	if (oPlayerIndex)
	{
		engine::GlobalId focusedPlayerIdentifier = gpGame->ClientPlayerIdentifier();
		XMVECTOR vecPlayerPosition = rGameInterpolate.pPlayers->pVecPositions[*oPlayerIndex];

		if (focusedPlayerIdentifier != mLastTrackedPlayerIdentifier)
		{
			LOG(kGraphics, kVerbose, "Camera NowTracking GlobalPlayerId: {} Coord: ({},{}) Index: {}", focusedPlayerIdentifier, coordinate.iX, coordinate.iY, *oPlayerIndex);
			mLastTrackedPlayerIdentifier = focusedPlayerIdentifier;
		}

		return engine::CameraTarget::Tracked(vecPlayerPosition, gpGame->mVecVisualErrorOffset);
	}

	static std::chrono::duration<float> sLastLogTime(-1.0f);
	if (std::chrono::duration<float>(mfTime) - sLastLogTime >= 1s)
	{
		sLastLogTime = std::chrono::duration<float>(mfTime);
		if (bHasCoordinate)
		{
			const PlayersPostRender& rPlayers = *gpGame->RenderFrame(coordinate).postRender.pPlayers;
			LOG(kGraphics, kVerbose, "Camera PlayerNotFound FocusedGlobalId: {} Coord: ({},{}) PostRenderCount: {} InterpolateCount: {}", gpGame->ClientPlayerIdentifier(), coordinate.iX, coordinate.iY, rPlayers.iCount, rGameInterpolate.pPlayers->iCount);
			for (int64_t i = 0; i < rPlayers.iCount; ++i)
			{
				LOG(kGraphics, kVerbose, "  PostRender[{}] GlobalPlayerId: {}", i, rPlayers.pGlobalPlayerIds[i]);
			}
		}
		else
		{
			LOG(kGraphics, kVerbose, "Camera CoordNotFound FocusedGlobalId: {} Coord: ({},{})", gpGame->ClientPlayerIdentifier(), coordinate.iX, coordinate.iY);
		}
	}

	// Player not found — extrapolate from last known position and velocity
	return engine::CameraTarget::Extrapolate();
}

// Return sun angle, applying UI slider override when in Graphics or ImGui mode
float Camera::SunAngle() const
{
	bool bUseOverride = (game::gpGame->meUiState == engine::UiState::kGraphicsSettings);
	if constexpr (kbDebugInput)
	{
		bUseOverride = bUseOverride || game::gpGame->mbShowImGui;
	}
	if (bUseOverride)
	{
		return engine::gSunAngleOverride.mfCurrent;
	}
	return mfSunAngle;
}

} // namespace game

#endif // BT_CLIENT
