#include "Camera.h"

#if defined(BT_CLIENT)

#include "Ui/GraphicsSettingsWrappersBase.h"

#include "Frame/Collections/Ships/ShipQueries.h"
#include "Frame/Frame.h"
#include "Game.h"

namespace game
{

Camera::Camera()
: engine::CameraBase(engine::CameraSetup {.vecInitialPosition = XMVectorAdd(kVecMenuIslandCenter, kVecMenuCameraOffset)})
{
	// The client starts in the main menu, so it opens at the menu pose even when no ClientState.bin loads.
	ResetMenuPose();
	ApplyPose(true);
}

// StorePose, ApplyPose, ResetMenuPose, and SunAngle are game-agnostic apart from the menu pose constants
// (kVecMenuIslandCenter, kVecMenuCameraOffset, kfMenuCameraFraming): a future project may want them moved into shared
// engine code, depending on its requirements.
void Camera::StorePose(bool bMainMenu)
{
	if (!bMainMenu)
	{
		mfGameFraming = mfCameraFramingTarget;
		return;
	}

	if constexpr (kbFreeCamera)
	{
		XMStoreFloat2(&mf2MenuPosition, mVecPosition);
		mfMenuFraming = mfCameraFramingTarget;
	}
}

void Camera::ApplyPose(bool bMainMenu)
{
	if (!bMainMenu)
	{
		RestoreFraming(mfGameFraming);
		return;
	}

	RestoreFraming(mfMenuFraming);
	// The menu renders the origin cell; rebasing now keeps the next update from shifting the stored position.
	mBasisCoordinate = engine::kOriginCoordinate;
	mVecPosition = XMVectorSet(mf2MenuPosition.x, mf2MenuPosition.y, engine::gpFlightPlanes->Lowest().fHeightMeters, 1.0f);
}

void Camera::ResetMenuPose()
{
	XMStoreFloat2(&mf2MenuPosition, XMVectorAdd(kVecMenuIslandCenter, kVecMenuCameraOffset));
	mfMenuFraming = kfMenuCameraFraming;
}

engine::CameraTarget Camera::PullTarget(const engine::FrameInterpolateBase& rFrameInterpolate)
{
	const FrameInterpolate& rGameInterpolate = static_cast<const FrameInterpolate&>(rFrameInterpolate);

	// The main menu and an invalid client owned-entity identity use the canonical menu pose to avoid retaining a stale gameplay target.
	if (gpGame->mGameFlags & engine::GameFlags::kMainMenu)
	{
		return engine::CameraTarget::Direct(XMVectorAdd(kVecMenuIslandCenter, kVecMenuCameraOffset));
	}

	if (!(gpGame->ClientOwnedEntityIdentifier().iValue != 0))
	{
		return engine::CameraTarget::Direct(XMVectorAdd(kVecMenuIslandCenter, kVecMenuCameraOffset));
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
			LOG(kGraphics, kVerbose, "Camera NowTracking GlobalId: {} Coord: ({},{}) Type: {} Row: {}", focusedOwnedEntityIdentifier, coordinate.iQ, coordinate.iR, ShipTypeName(optionalOwnedEntity->eType), optionalOwnedEntity->iRow);
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
			LOG(kGraphics, kVerbose, "Camera OwnedEntityNotFound FocusedGlobalId: {} Coord: ({},{})", gpGame->ClientOwnedEntityIdentifier(), coordinate.iQ, coordinate.iR);
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
			LOG(kGraphics, kVerbose, "Camera CoordNotFound FocusedGlobalId: {} Coord: ({},{})", gpGame->ClientOwnedEntityIdentifier(), coordinate.iQ, coordinate.iR);
		}
	}

	// Owned entity not found — extrapolate from last known position and velocity
	return engine::CameraTarget::Extrapolate();
}

// Return sun angle, applying the Sun Angle Override slider while the Graphics settings screen is up
float Camera::SunAngle() const
{
	if (game::gpGame->meUiState == engine::UiState::kGraphicsSettings)
	{
		return engine::gSunAngleOverride.mfCurrent;
	}
	return mfSunAngle;
}

} // namespace game

#endif // BT_CLIENT
