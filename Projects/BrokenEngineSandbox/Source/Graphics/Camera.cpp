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

engine::CameraTarget Camera::PullTarget(const engine::FrameInterpolateBase& rFrameInterpolate, const std::unordered_map<engine::GridCoord, FrameInterpolate>& rRenderInterpolates)
{
	const FrameInterpolate& rGameInterpolate = static_cast<const FrameInterpolate&>(rFrameInterpolate);

	// The main menu and an invalid client owned-entity identity use the canonical menu pose to avoid retaining a stale gameplay target.
	if (gpGame->mGameFlags & engine::GameFlags::kMainMenu)
	{
		return engine::CameraTarget::Direct(XMVectorAdd(kVecMenuIslandCenter, kVecMenuCameraOffset));
	}

	engine::GlobalId focusedOwnedEntityIdentifier = gpGame->ClientOwnedEntityIdentifier();
	if (focusedOwnedEntityIdentifier.iValue == 0)
	{
		return engine::CameraTarget::Direct(XMVectorAdd(kVecMenuIslandCenter, kVecMenuCameraOffset));
	}

	// Resolve the focused ship in its own cell (which may be a subscribed neighbor when a non-flagship is selected),
	// falling back to the interpolate's cell if not yet in mClientOwnedEntities, and rebase into the camera cell's frame.
	engine::GridCoord coordinate = rGameInterpolate.renderBasis.coordinate;
	auto ownedIt = std::ranges::find(gpGame->mClientOwnedEntities, focusedOwnedEntityIdentifier, &engine::OwnedEntity::globalId);
	if (ownedIt != gpGame->mClientOwnedEntities.end())
	{
		coordinate = ownedIt->coord;
	}

	auto interpolateIt = rRenderInterpolates.find(coordinate);
	const FrameInterpolate* pShipInterpolate = interpolateIt != rRenderInterpolates.end() ? &interpolateIt->second : nullptr;
	if (pShipInterpolate != nullptr)
	{
		std::optional<ShipLocator> optionalOwnedEntity = FindShipByGlobalId(gpGame->RenderFrame(coordinate), focusedOwnedEntityIdentifier);
		if (optionalOwnedEntity)
		{
			XMVECTOR vecLocalPosition = ShipPosition(*pShipInterpolate, *optionalOwnedEntity);
			XMVECTOR vecOwnedEntityPosition = engine::Rebase(pShipInterpolate->renderBasis, vecLocalPosition);

			if (focusedOwnedEntityIdentifier != mLastTrackedOwnedEntityIdentifier)
			{
				LOG(kGraphics, kVerbose, "Camera NowTracking GlobalId: {} Coord: ({},{}) Type: {} Row: {}", focusedOwnedEntityIdentifier, coordinate.iQ, coordinate.iR, ShipTypeName(optionalOwnedEntity->eType), optionalOwnedEntity->iRow);
				mLastTrackedOwnedEntityIdentifier = focusedOwnedEntityIdentifier;
			}

			return engine::CameraTarget::Tracked(vecOwnedEntityPosition, mVecVisualErrorOffset);
		}
	}

	static std::chrono::duration<float> sLastLogTime(-1.0f);
	if (std::chrono::duration<float>(mfTime) - sLastLogTime >= 1s)
	{
		sLastLogTime = std::chrono::duration<float>(mfTime);
		if (pShipInterpolate != nullptr)
		{
			LOG(kGraphics, kVerbose, "Camera OwnedEntityNotFound FocusedGlobalId: {} Coord: ({},{})", focusedOwnedEntityIdentifier, coordinate.iQ, coordinate.iR);
			ForEachShipType(*pShipInterpolate, gpGame->RenderFrame(coordinate).postRender, [](ShipType eType, const auto& rInterpolate, const auto& rPostRender)
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
			LOG(kGraphics, kVerbose, "Camera CoordNotFound FocusedGlobalId: {} Coord: ({},{})", focusedOwnedEntityIdentifier, coordinate.iQ, coordinate.iR);
		}
	}

	// Owned entity not found — extrapolate from last known position and velocity
	return engine::CameraTarget::Extrapolate();
}

void Camera::ClearSubscriptionAnchor()
{
	mRetainedSubscriptionAnchorGlobalId = {};
	mRetainedSubscriptionAnchorCoordinate = {};
	mVecRetainedSubscriptionAnchorLocal = {};
	ClearCellLoadedArea();
}

std::optional<XMVECTOR> Camera::PullSubscriptionCenter(const std::unordered_map<engine::GridCoord, FrameInterpolate>& rRenderInterpolates, engine::GridCoord basisCoordinate)
{
	const engine::OwnedEntity* pFlagship = gpGame->FindLiveFlagshipOwnedEntity();
	if (pFlagship == nullptr)
	{
		ClearSubscriptionAnchor();
		return std::nullopt;
	}

	if (pFlagship->globalId != mRetainedSubscriptionAnchorGlobalId)
	{
		ClearSubscriptionAnchor();
	}

	auto interpolateIt = rRenderInterpolates.find(pFlagship->coord);
	if (interpolateIt != rRenderInterpolates.end())
	{
		std::optional<ShipLocator> optionalFlagship = FindShipByGlobalId(gpGame->RenderFrame(pFlagship->coord), pFlagship->globalId);
		if (optionalFlagship)
		{
			XMVECTOR vecLocalPosition = ShipPosition(interpolateIt->second, *optionalFlagship);
			mRetainedSubscriptionAnchorGlobalId = pFlagship->globalId;
			mRetainedSubscriptionAnchorCoordinate = pFlagship->coord;
			mVecRetainedSubscriptionAnchorLocal = vecLocalPosition;
			return engine::Rebase(interpolateIt->second.renderBasis, vecLocalPosition);
		}
	}

	if (mRetainedSubscriptionAnchorGlobalId == pFlagship->globalId)
	{
		return engine::Rebase(engine::MakeRenderBasis(mRetainedSubscriptionAnchorCoordinate, basisCoordinate), mVecRetainedSubscriptionAnchorLocal);
	}
	return std::nullopt;
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
