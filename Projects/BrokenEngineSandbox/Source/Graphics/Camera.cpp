#include "Camera.h"

#if defined(BT_CLIENT)

#include "Ui/GraphicsSettingsWrappersBase.h"

#include "Frame/Collections/Ships/ShipQueries.h"
#include "Frame/Frame.h"
#include "Game.h"

namespace game
{

Camera::Camera()
: engine::CameraBase(engine::CameraSetup {.vecInitialPosition = XMVectorAdd(XMVectorAdd(kVecMenuIslandCenter, kVecMenuCameraOffset), XMVectorSet(0.0f, 0.0f, engine::gpFlightPlanes->Lowest().fHeightMeters, 0.0f))})
{
}

engine::CameraTarget Camera::PullTarget(const engine::FrameInterpolateBase& rFrameInterpolate)
{
	const FrameInterpolate& rGameInterpolate = static_cast<const FrameInterpolate&>(rFrameInterpolate);

	// The main menu and an invalid client owned-entity identity use the canonical menu pose to avoid retaining a stale gameplay target.
	if (gpGame->mGameFlags & engine::GameFlags::kMainMenu)
	{
		return engine::CameraTarget::Direct(XMVectorAdd(XMVectorAdd(kVecMenuIslandCenter, kVecMenuCameraOffset), XMVectorSet(0.0f, 0.0f, engine::gpFlightPlanes->Lowest().fHeightMeters, 0.0f)));
	}

	if (!(gpGame->ClientOwnedEntityIdentifier().iValue != 0))
	{
		return engine::CameraTarget::Direct(XMVectorAdd(XMVectorAdd(kVecMenuIslandCenter, kVecMenuCameraOffset), XMVectorSet(0.0f, 0.0f, engine::gpFlightPlanes->Lowest().fHeightMeters, 0.0f)));
	}

	// The interpolate's own cell, not the client cell: its positions are local to that cell, and the camera works in
	// that same frame, so a target taken from it needs no conversion.
	engine::GridCoord coordinate = rGameInterpolate.renderBasis.coordinate;
	auto it = gpGame->mCells.find(coordinate);
	bool bHasCoordinate = it != gpGame->mCells.end() && it->second.iSnapshotCount > 0;
	std::optional<ShipLocator> optionalOwnedEntity = bHasCoordinate ? gpGame->ClientOwnedEntity(gpGame->RenderFrame(coordinate)) : std::nullopt;
	if (optionalOwnedEntity)
	{
		engine::GlobalId focusedOwnedEntityIdentifier = gpGame->ClientOwnedEntityIdentifier();
		XMVECTOR vecOwnedEntityPosition = ShipPosition(rGameInterpolate, *optionalOwnedEntity);

		if (focusedOwnedEntityIdentifier != mLastTrackedOwnedEntityIdentifier)
		{
			LOG(kGraphics, kVerbose, "Camera NowTracking GlobalId: {} Coord: ({},{}) Type: {} Row: {}", focusedOwnedEntityIdentifier, coordinate.iX, coordinate.iY, ShipTypeName(optionalOwnedEntity->eType), optionalOwnedEntity->iRow);
			mLastTrackedOwnedEntityIdentifier = focusedOwnedEntityIdentifier;
		}

		return engine::CameraTarget::Tracked(vecOwnedEntityPosition, mVecVisualErrorOffset);
	}

	static std::chrono::duration<float> sLastLogTime(-1.0f);
	if (std::chrono::duration<float>(mfTime) - sLastLogTime >= 1s)
	{
		sLastLogTime = std::chrono::duration<float>(mfTime);
		if (bHasCoordinate)
		{
			LOG(kGraphics, kVerbose, "Camera OwnedEntityNotFound FocusedGlobalId: {} Coord: ({},{})", gpGame->ClientOwnedEntityIdentifier(), coordinate.iX, coordinate.iY);
			ForEachShipType(rGameInterpolate, gpGame->RenderFrame(coordinate).postRender, [](ShipType eType, const auto& rInterpolate, const auto& rPostRender)
			{
				LOG(kGraphics, kVerbose, "  {} PostRenderCount: {} InterpolateCount: {}", ShipTypeName(eType), rPostRender.iCount, rInterpolate.iCount);
				for (int64_t i = 0; i < rPostRender.iCount; ++i)
				{
					LOG(kGraphics, kVerbose, "  PostRender[{}] GlobalId: {}", i, rPostRender.pGlobalIds[i]);
				}
			});
		}
		else
		{
			LOG(kGraphics, kVerbose, "Camera CoordNotFound FocusedGlobalId: {} Coord: ({},{})", gpGame->ClientOwnedEntityIdentifier(), coordinate.iX, coordinate.iY);
		}
	}

	// Owned entity not found — extrapolate from last known position and velocity
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
