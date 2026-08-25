#include "Camera.h"

#if defined(BT_CLIENT)

#include "Game.h"
#include "Frame/Frame.h"
#include "Frame/Collections/Players/Players.h"
#include "Ui/GraphicsSettingsWrappersBase.h"

namespace game
{

Camera::Camera()
: engine::Camera(engine::CameraSetup {.vecInitialPosition = XMVectorAdd(XMVectorAdd(kVecMenuIslandCenter, kVecMenuCameraOffset), XMVectorSet(0.0f, 0.0f, engine::gBaseHeight.Get(), 0.0f))})
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

	// Main menu, or no fleet found for this client — use the canonical menu pose so we don't strand
	// the camera at whatever stale gameplay position last set mVecPosition.
	if ((rGameInterpolate.gameFlags & GameFlags::kMainMenu) || !gpGame->ClientPlayerId().IsValid())
	{
		return engine::CameraTarget::Direct(XMVectorAdd(XMVectorAdd(kVecMenuIslandCenter, kVecMenuCameraOffset), XMVectorSet(0.0f, 0.0f, engine::gBaseHeight.Get(), 0.0f)));
	}

	auto coordIt = gpGame->mCoordFrames.find(gpGame->mClientGridCoord);
	bool bHasCoord = coordIt != gpGame->mCoordFrames.end() && coordIt->second.iSnapshotCount > 0;
	std::optional<int64_t> oIdx = bHasCoord ? gpGame->ClientPlayerIndex(*gpGame->RenderFrame(gpGame->mClientGridCoord).postRender.pPlayers) : std::nullopt;
	if (oIdx)
	{
		engine::global_id_t focusedId = gpGame->ClientPlayerId();
		XMVECTOR vecPlayerPos = rGameInterpolate.pPlayers->pVecPositions[*oIdx];

		if (focusedId != mLastTrackedPlayerId)
		{
			LOG(kGraphics, kVerbose, "Camera NowTracking GlobalPlayerId: {} Coord: ({},{}) Index: {}", focusedId, gpGame->mClientGridCoord.x, gpGame->mClientGridCoord.y, *oIdx);
			mLastTrackedPlayerId = focusedId;
		}

		return engine::CameraTarget::Tracked(vecPlayerPos, gpGame->mVecVisualErrorOffset);
	}

	static float sfLastLogTime = -1.0f;
	if (mfTime - sfLastLogTime >= 1.0f)
	{
		sfLastLogTime = mfTime;
		if (bHasCoord)
		{
			const PlayersPostRender& rPlayers = *gpGame->RenderFrame(gpGame->mClientGridCoord).postRender.pPlayers;
			LOG(kGraphics, kVerbose, "Camera PlayerNotFound FocusedGlobalId: {} Coord: ({},{}) PostRenderCount: {} InterpolateCount: {}",
				gpGame->ClientPlayerId(), gpGame->mClientGridCoord.x, gpGame->mClientGridCoord.y, rPlayers.iCount, rGameInterpolate.pPlayers->iCount);
			for (int64_t i = 0; i < rPlayers.iCount; ++i)
			{
				LOG(kGraphics, kVerbose, "  PostRender[{}] GlobalPlayerId: {}", i, rPlayers.pGlobalPlayerIds[i]);
			}
		}
		else
		{
			LOG(kGraphics, kVerbose, "Camera CoordNotFound FocusedGlobalId: {} Coord: ({},{})", gpGame->ClientPlayerId(), gpGame->mClientGridCoord.x, gpGame->mClientGridCoord.y);
		}
	}

	// Player not found — extrapolate from last known position and velocity
	return engine::CameraTarget::Extrapolate();
}

void Camera::OnUpdateComplete()
{
	// Refresh the in-memory client-state mirror for zoom-target changes (and any focus changes that came through unhooked paths). Diff-checked, so no-op on most frames.
	gpGame->CaptureClientStateIfChanged();
}

// Return sun angle, applying UI slider override when in Graphics or ImGui mode
float Camera::SunAngle() const
{
	// Apply time of day slider override when in Graphics or Tweaks UI
	bool bUseOverride = (game::gpGame->meUiState == engine::UiState::kGraphicsSettings);
	if constexpr (kbDebugInput)
	{
		bUseOverride = bUseOverride || game::gpGame->mbShowImGui;
	}
	if (bUseOverride)
	{
		return engine::gSunAngleOverride.Get();
	}
	return mfSunAngle;
}

} // namespace game

#endif // BT_CLIENT
