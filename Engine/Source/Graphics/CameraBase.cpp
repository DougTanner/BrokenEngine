#if defined(BT_CLIENT)

#include "CameraBase.h"

#include "Ui/LightingWrappersBase.h"
#include "Ui/ShadowWrappersBase.h"
#include "Ui/WrapperBase.h"

namespace engine
{

// Mouse-wheel zoom: per-frame scroll delta nudges target height; current eases toward target
constexpr float kfEyeHeightPerWheelTick = 0.1f;
constexpr float kfEyeBlendDuration = 0.35f;

constexpr float kfCameraPositionBlend = 8.0f;
constexpr float kfJumpDistanceThreshold = 50.0f;
constexpr float kfJumpDuration = 2.0f;
constexpr float kfJumpCancelThreshold = 5.0f;

static void UpdateTexelEyeHeightReference(float fLiveEyeHeight, float fContractionMetersPerSecond, float fDeltaTime, float& rfReferenceEyeHeight)
{
	if (rfReferenceEyeHeight == 0.0f || fLiveEyeHeight > rfReferenceEyeHeight)
	{
		rfReferenceEyeHeight = fLiveEyeHeight;
		return;
	}

	float fMaxContraction = fContractionMetersPerSecond * fDeltaTime;
	rfReferenceEyeHeight = std::max(rfReferenceEyeHeight - fMaxContraction, fLiveEyeHeight);
}

static constexpr float Smoothstep(float fParameter)
{
	fParameter = std::clamp(fParameter, 0.0f, 1.0f);
	return fParameter * fParameter * (3.0f - 2.0f * fParameter);
}

CameraTarget::CameraTarget(Kind eKind, XMVECTOR vecPosition, XMVECTOR vecVisualOffset)
: meKind(eKind)
, mVecPosition(vecPosition)
, mVecVisualOffset(vecVisualOffset)
{
}

CameraTarget CameraTarget::Direct(FXMVECTOR vecPosition)
{
	return CameraTarget(Kind::kDirect, vecPosition, XMVectorZero());
}

CameraTarget CameraTarget::Tracked(FXMVECTOR vecRawPosition, FXMVECTOR vecVisualOffset)
{
	return CameraTarget(Kind::kTracked, vecRawPosition, vecVisualOffset);
}

CameraTarget CameraTarget::Extrapolate()
{
	return CameraTarget(Kind::kExtrapolate, XMVectorZero(), XMVectorZero());
}

CameraBase::CameraBase(const CameraSetup& rCameraSetup)
: common::Singleton<CameraBase>(gpCamera)
{
	mVecPosition = rCameraSetup.vecInitialPosition;
}

void CameraBase::DiscardTrackingCaches()
{
	mVecLastKnownPlayerPosition = {};
	mVecLastKnownPlayerVelocity = {};
	mfLastKnownPlayerTime = 0.0f;
	mVecJumpStartPosition = {};
	mVecPreviousTargetPosition = {};
	mfJumpStartTime = 0.0f;
	mbJumping = false;
}

void CameraBase::ShiftToRenderedCell(GridCoord cameraCoordinate)
{
	if (cameraCoordinate == mBasisCoordinate)
	{
		return;
	}

	int64_t iStepX = static_cast<int64_t>(cameraCoordinate.iX) - static_cast<int64_t>(mBasisCoordinate.iX);
	int64_t iStepY = static_cast<int64_t>(cameraCoordinate.iY) - static_cast<int64_t>(mBasisCoordinate.iY);
	// Z and W stay zero: the step is planar, and the cached values it moves are homogeneous points.
	RenderBasis previousBasis = MakeRenderBasis(mBasisCoordinate, cameraCoordinate);
	XMVECTOR vecShift = XMVectorSet(previousBasis.f2Offset.x, previousBasis.f2Offset.y, 0.0f, 0.0f);
	mBasisCoordinate = cameraCoordinate;

	if (iStepX < -1 || iStepX > 1 || iStepY < -1 || iStepY > 1)
	{
		// Farther than the subscribed ring: nothing the camera cached describes a place inside the new cell, so drop
		// the tracking state and re-enter the cell at its center. The sun angle and the zoom state are not positional
		// and keep running.
		mVecPosition = XMVectorSetY(XMVectorSetX(mVecPosition, 0.0f), 0.0f);
		DiscardTrackingCaches();
		return;
	}

	// One-cell step: move the whole cached set by the same exact whole-cell delta. Both flight endpoints shift
	// together, so the smoothstep parameter and the elapsed jump clock are untouched and the flight stays monotonic
	// across the boundary.
	mVecPosition = XMVectorAdd(mVecPosition, vecShift);
	mVecEyePosition = XMVectorAdd(mVecEyePosition, vecShift);
	mVecJumpStartPosition = XMVectorAdd(mVecJumpStartPosition, vecShift);
	mVecPreviousTargetPosition = XMVectorAdd(mVecPreviousTargetPosition, vecShift);
	mVecLastKnownPlayerPosition = XMVectorAdd(mVecLastKnownPlayerPosition, vecShift);
}

void CameraBase::Update(const FrameInterpolateBase& rFrameInterpolate, float fDeltaTime)
{
	// fDeltaTime is the sim-scaled render delta the engine just measured: wall delta multiplied by the active time
	// ratio (equal to wall time at ratio 1.0). Driving camera blend, shake decay, and mfTime off the same source
	// keeps the camera in sync with the interpolated player across vsync misses; otherwise different deltas would
	// produce visible relative stutter at high zoom.
	mfTime += fDeltaTime;

	// Before any position is read or written this frame: the camera lives in its own cell's local frame, so a change
	// of rendered cell must move every cached position into the new frame first.
	ShiftToRenderedCell(rFrameInterpolate.renderBasis.coordinate);

	mfShake = std::max(mfShake - fDeltaTime * 2.0f, 0.0f);

	miFrame = rFrameInterpolate.iTick;

	bool bMainMenuFrame = IsMainMenuFrame(rFrameInterpolate);

	if (!bMainMenuFrame)
	{
		static constexpr float kfNightSpeedStart = XM_PI;
		static constexpr float kfNightSpeedEnd = XM_2PI;
		if (mfSunAngle >= kfNightSpeedStart && mfSunAngle < kfNightSpeedEnd)
		{
			mfSunAngle = mfSunAngle + rFrameInterpolate.fDeltaTime * 0.075f;
		}
		else
		{
			mfSunAngle = mfSunAngle + rFrameInterpolate.fDeltaTime * 0.01f;
		}

		if (mfSunAngle >= XM_2PI)
		{
			mfSunAngle = 0.0f;
		}
	}

	// Main-menu WASD moves the camera when kbFreeCamera is enabled and uses its position as the target.
	// Movement has W=0 so it preserves the position's W=1.
	bool bFreeCameraActive = false;
	if constexpr (kbFreeCamera)
	{
		if (bMainMenuFrame)
		{
			bFreeCameraActive = true;
			XMVECTOR vecMove = XMVectorSet(mCameraInput.f2Move.x, mCameraInput.f2Move.y, 0.0f, 0.0f);
			static constexpr float kfFreeCameraSpeed = 200.0f;
			mVecPosition = XMVectorAdd(mVecPosition, XMVectorScale(vecMove, kfFreeCameraSpeed * fDeltaTime));
			mVecPreviousTargetPosition = mVecPosition;
			mbJumping = false;
		}
	}

	// Free camera already is the target, so the derived policy is not consulted at all while it drives the pose.
	XMVECTOR vecTargetPosition = ResolveTarget(bFreeCameraActive ? CameraTarget::Direct(mVecPosition) : PullTarget(rFrameInterpolate));

	UpdatePosition(vecTargetPosition, fDeltaTime);
	UpdateEyeHeight();

	// Keep each world-texel reference at or above the live eye height: zero initialization and outward zoom snap
	// immediately for full viewport coverage, while inward zoom contracts at the existing independent rates so the
	// density change remains gradual. At a settled height both references converge to the live height.
	UpdateTexelEyeHeightReference(mfCameraEyeHeight, gShadowTexelRampMetersPerSecond.mfCurrent, fDeltaTime, mfShadowTexelEyeHeight);
	UpdateTexelEyeHeightReference(mfCameraEyeHeight, gLightingTexelRampMetersPerSecond.mfCurrent, fDeltaTime, mfLightingTexelEyeHeight);

	// Eye sits directly above target along +Z (straight-down view).
	// W=0 — eye-local offset, not a homogeneous point; added to mVecPosition (W=1) preserves position.
	auto vecEyePositionRelative = XMVectorSet(0.0f, 0.0f, mfCameraEyeHeight, 0.0f);
	mVecToEyeNormal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	mVecEyePosition = XMVectorAdd(mVecPosition, vecEyePositionRelative);

	float fVibration = std::pow(mfShake, 0.5f);
	gpRawInputManager->SetVibration(fVibration, fVibration);

	CalculateMatricesAndVisibleArea();
}

XMVECTOR CameraBase::ResolveTarget(const CameraTarget& rCameraTarget)
{
	if (rCameraTarget.meKind == CameraTarget::Kind::kDirect)
	{
		return rCameraTarget.mVecPosition;
	}

	if (rCameraTarget.meKind == CameraTarget::Kind::kExtrapolate)
	{
		// Player not found — extrapolate from last known position and velocity
		float fElapsedTime = std::clamp(mfTime - mfLastKnownPlayerTime, 0.0f, 2.0f);
		return XMVectorMultiplyAdd(XMVectorReplicate(fElapsedTime), mVecLastKnownPlayerVelocity, mVecLastKnownPlayerPosition);
	}

	// Compute velocity from position delta using sim-scaled render time. The raw tracked position feeds the velocity
	// and last-known state; the reconciliation offset is added only to the returned target.
	if (mfLastKnownPlayerTime > 0.0f && mfTime > mfLastKnownPlayerTime)
	{
		float fElapsedTime = mfTime - mfLastKnownPlayerTime;
		mVecLastKnownPlayerVelocity = XMVectorScale(XMVectorSubtract(rCameraTarget.mVecPosition, mVecLastKnownPlayerPosition), 1.0f / fElapsedTime);
	}

	mVecLastKnownPlayerPosition = rCameraTarget.mVecPosition;
	mfLastKnownPlayerTime = mfTime;
	return XMVectorAdd(rCameraTarget.mVecPosition, rCameraTarget.mVecVisualOffset);
}

void CameraBase::UpdatePosition(FXMVECTOR vecTargetPosition, float fDeltaTime)
{
	// Detect target switch during jump: target jumped far from where it was last frame
	float fDistanceToTarget = XMVectorGetX(XMVector2Length(XMVectorSubtract(vecTargetPosition, mVecPosition)));
	float fTargetShift = XMVectorGetX(XMVector2Length(XMVectorSubtract(vecTargetPosition, mVecPreviousTargetPosition)));
	if (mbJumping)
	{
		if (fTargetShift > kfJumpDistanceThreshold)
		{
			mVecJumpStartPosition = mVecPosition;
			mfJumpStartTime = mfTime;
		}
	}
	else if (fDistanceToTarget > kfJumpDistanceThreshold)
	{
		mbJumping = true;
		mVecJumpStartPosition = mVecPosition;
		mfJumpStartTime = mfTime;
	}

	if (mbJumping)
	{
		if (fDistanceToTarget < kfJumpCancelThreshold)
		{
			mbJumping = false;
		}
		else
		{
			float fElapsed = mfTime - mfJumpStartTime;
			if (fElapsed >= kfJumpDuration)
			{
				mbJumping = false;
				mVecPosition = vecTargetPosition;
			}
			else
			{
				float fParameter = Smoothstep(fElapsed / kfJumpDuration);
				mVecPosition = XMVectorLerp(mVecJumpStartPosition, vecTargetPosition, fParameter);
			}
		}
	}

	if (!mbJumping)
	{
		// Scale chase rate linearly with eye height: slow/loose at low altitude (close-up view stays calm), fast/tight at altitude (zoomed-out stays responsive).
		float fAdaptiveBlend = kfCameraPositionBlend * (mfCameraEyeHeight / kfCameraEyeHeightDefault);
		float fBlend = std::clamp(fDeltaTime * fAdaptiveBlend, 0.0f, 1.0f);
		mVecPosition = XMVectorMultiplyAdd(XMVectorReplicate(fBlend), vecTargetPosition, XMVectorMultiply(XMVectorReplicate(1.0f - fBlend), mVecPosition));
	}

	mVecPreviousTargetPosition = vecTargetPosition;
}

void CameraBase::UpdateEyeHeight()
{
	int64_t iScrollDelta = mCameraInput.iScrollDelta;
	if (iScrollDelta != 0)
	{
		// Scale per-tick zoom delta with current eye height, bounded by sqrt so high altitudes don't get 4x ticks and over-build Hermite velocity that carries into low-altitude territory.
		float fEyeHeightDelta = static_cast<float>(iScrollDelta) * kfEyeHeightPerWheelTick * std::sqrt(mfCameraEyeHeight / kfCameraEyeHeightDefault);
		float fNewTarget = std::clamp(mfCameraEyeHeightTarget - fEyeHeightDelta, kfMinimumEyeHeight, kfEyeHeightMaximum);
		if (fNewTarget != mfCameraEyeHeightTarget)
		{
			// Capture current height and velocity whenever the zoom target changes so the Hermite curve remains continuous.
			mfEyeStartHeight = mfCameraEyeHeight;
			mfEyeStartVelocity = mfEyeVelocity;
			mfEyeStartTime = mfTime;
			mfCameraEyeHeightTarget = fNewTarget;
			mbEyeZooming = true;
		}
	}

	if (mbEyeZooming)
	{
		float fEyeElapsed = mfTime - mfEyeStartTime;
		if (fEyeElapsed >= kfEyeBlendDuration)
		{
			mfCameraEyeHeight = mfCameraEyeHeightTarget;
			mfEyeVelocity = 0.0f;
			mbEyeZooming = false;
		}
		else
		{
			// Cubic Hermite from (mfEyeStartHeight, mfEyeStartVelocity) to (mfCameraEyeHeightTarget, 0) over kfEyeBlendDuration.
			float fParameter = fEyeElapsed / kfEyeBlendDuration;
			float fParameterSquared = fParameter * fParameter;
			float fParameterCubed = fParameterSquared * fParameter;
			float fStartHeightBasis = 2.0f * fParameterCubed - 3.0f * fParameterSquared + 1.0f;
			float fStartVelocityBasis = fParameterCubed - 2.0f * fParameterSquared + fParameter;
			float fTargetHeightBasis = -2.0f * fParameterCubed + 3.0f * fParameterSquared;
			mfCameraEyeHeight = fStartHeightBasis * mfEyeStartHeight + fStartVelocityBasis * mfEyeStartVelocity * kfEyeBlendDuration + fTargetHeightBasis * mfCameraEyeHeightTarget;
			float fStartHeightBasisDerivative = 6.0f * fParameterSquared - 6.0f * fParameter;
			float fStartVelocityBasisDerivative = 3.0f * fParameterSquared - 4.0f * fParameter + 1.0f;
			float fTargetHeightBasisDerivative = -6.0f * fParameterSquared + 6.0f * fParameter;
			mfEyeVelocity = (fStartHeightBasisDerivative * mfEyeStartHeight + fStartVelocityBasisDerivative * mfEyeStartVelocity * kfEyeBlendDuration + fTargetHeightBasisDerivative * mfCameraEyeHeightTarget) / kfEyeBlendDuration;
		}
	}
}

void CameraBase::ResetForSession()
{
	mfSunAngle = kfDefaultSunAngle;
	DiscardTrackingCaches();
	// Reinitialize both references directly to the new session's live zoom on the next camera update; do not carry
	// contraction across sessions.
	mfShadowTexelEyeHeight = 0.0f;
	mfLightingTexelEyeHeight = 0.0f;
}

void CameraBase::RestoreEyeHeight(float fEyeHeight)
{
	mfCameraEyeHeight = fEyeHeight;
	mfCameraEyeHeightTarget = fEyeHeight;
}

XMVECTOR XM_CALLCONV CameraBase::WorldToScreen(FXMVECTOR vecWorldPosition) const
{
	// X/Y are screen pixels and Z is projected depth.
	float fViewportWidth = static_cast<float>(gpGraphics->mFramebufferVkExtent2D.width);
	float fViewportHeight = static_cast<float>(gpGraphics->mFramebufferVkExtent2D.height);
	// The scalar XMVector3Project overload builds its viewport offset with W=0, so the returned screen position
	// carries W=0; force the position W invariant.
	return XMVectorSetW(XMVector3Project(vecWorldPosition, 0.0f, 0.0f, fViewportWidth, fViewportHeight, 0.0f, 1.0f, mMatPerspective, mMatView, XMMatrixIdentity()), 1.0f);
}

void CameraBase::CalculateMatricesAndVisibleArea()
{
	auto vecToEyeNormal = XMVector3Normalize(XMVectorSubtract(mVecEyePosition, mVecPosition));
	auto vecUpNormal = XMVector3Cross(vecToEyeNormal, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
	mMatView = XMMatrixLookAtRH(mVecEyePosition, mVecPosition, vecUpNormal);

	static constexpr float kfNearClip = 1.0f;
	static constexpr float kfMinimumFarClip = 400.0f;
	static constexpr float kfFarClipPerEyeDistance = 2.667f;
	float fEyeDistance = XMVectorGetX(XMVector3Length(XMVectorSubtract(mVecEyePosition, mVecPosition)));
	float fFarClip = std::max(kfMinimumFarClip, fEyeDistance * kfFarClipPerEyeDistance);
	float fViewportWidth = static_cast<float>(gpGraphics->mFramebufferVkExtent2D.width);
	float fViewportHeight = static_cast<float>(gpGraphics->mFramebufferVkExtent2D.height);
	float fAspectRatio = gpSwapchainManager->mfAspectRatio;
	mMatPerspective = XMMatrixPerspectiveFovRH(XMConvertToRadians(gFieldOfView.mfCurrent / fAspectRatio), fAspectRatio, kfNearClip, fFarClip);

	XMVECTOR vecPlane = XMPlaneFromPointNormal(XMVectorZero(), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));

	XMMATRIX matIdentity = XMMatrixIdentity();

	XMVECTOR vecRayStart {};
	XMVECTOR vecRayEnd {};
	XMVECTOR vecPlaneIntersection {};

	XMFLOAT3 f3ScreenPosition { 0.0f, 0.0f, 0.0f };

	struct VisibleCorner
	{
		float fScreenX = 0.0f;
		float fScreenY = 0.0f;
		XMFLOAT4* pTarget = nullptr;
	};
	const VisibleCorner corners[] =
	{
		{ .fScreenX = 0.0f, .fScreenY = 0.0f, .pTarget = &mf4VisibleTopLeft },
		{ .fScreenX = fViewportWidth, .fScreenY = 0.0f, .pTarget = &mf4VisibleTopRight },
		{ .fScreenX = 0.0f, .fScreenY = fViewportHeight, .pTarget = &mf4VisibleBottomLeft },
		{ .fScreenX = fViewportWidth, .fScreenY = fViewportHeight, .pTarget = &mf4VisibleBottomRight },
	};

	for (const VisibleCorner& rCorner : corners)
	{
		f3ScreenPosition.x = rCorner.fScreenX;
		f3ScreenPosition.y = rCorner.fScreenY;

		f3ScreenPosition.z = 0.0f;
		vecRayStart = XMVector3Unproject(XMLoadFloat3(&f3ScreenPosition), 0.0f, 0.0f, fViewportWidth, fViewportHeight, 0.0f, 1.0f, mMatPerspective, mMatView, matIdentity);
		f3ScreenPosition.z = 1.0f;
		vecRayEnd = XMVector3Unproject(XMLoadFloat3(&f3ScreenPosition), 0.0f, 0.0f, fViewportWidth, fViewportHeight, 0.0f, 1.0f, mMatPerspective, mMatView, matIdentity);

		vecPlaneIntersection = XMPlaneIntersectLine(vecPlane, vecRayStart, vecRayEnd);
		// The SDK returns all-lane QNaN when the corner ray is parallel to the Z=0 plane; a NaN corner would
		// poison the visible area and every grid-snapped extent derived from it.
		if (XMVector3IsNaN(vecPlaneIntersection)) [[unlikely]]
		{
			vecPlaneIntersection = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		}

		XMStoreFloat4(rCorner.pTarget, vecPlaneIntersection);
	}

	mf4LargeVisibleArea = XMFLOAT4 {mf4VisibleTopLeft.x, mf4VisibleTopLeft.y, mf4VisibleTopRight.x, mf4VisibleBottomRight.y};

	if (gpGraphics->mFramebufferVkExtent2D.width > gpGraphics->mFramebufferVkExtent2D.height) [[likely]]
	{
		mf4RenderVisibleArea = mf4LargeVisibleArea;
		float fVisibleHeight = mf4RenderVisibleArea.y - mf4RenderVisibleArea.w;
		mf4RenderVisibleArea.x -= gVisibleAreaExtraTop.mfCurrent * fVisibleHeight;
		mf4RenderVisibleArea.y += gVisibleAreaExtraTop.mfCurrent * fVisibleHeight;
		mf4RenderVisibleArea.z += gVisibleAreaExtraTop.mfCurrent * fVisibleHeight;
		mf4RenderVisibleArea.w -= gVisibleAreaExtraBottom.mfCurrent * fVisibleHeight;
	}
	else [[unlikely]]
	{
		mf4RenderVisibleArea = mf4LargeVisibleArea;
	}

	// Each level of detail halves mesh quads per dimension, coupling mesh density to the visible-area snap grid.
	// The integer eye-distance bucket latches quad size within a level of detail to prevent sub-pixel height drift from jittering snapped edges.
	// floor(area / quadSize) amplifies quad-size jitter by approximately area / quadSize, so quad size must stay stable while the latch key is unchanged.
	[[maybe_unused]] XMFLOAT4 f4RawInputArea = mf4RenderVisibleArea;

	int64_t iLevelOfDetail = std::clamp<int64_t>(static_cast<int64_t>(std::floor(std::log2(std::max(fEyeDistance, kfMinimumEyeHeight) / kfMinimumEyeHeight) * 0.5f)), 0, BufferManager::kiVisibleAreaLodCount - 1);
	// LOD hysteresis: refuse to flip back across the shared boundary if eye distance is still
	// near it. Boundaries are at kfMinimumEyeHeight * 4^L; 5% band absorbs FP rounding around
	// asymptotic settling.
	static constexpr float kfLevelOfDetailHysteresisFraction = 0.05f;
	if (iLevelOfDetail == miVisibleAreaLevelOfDetail - 1)
	{
		float fBoundary = kfMinimumEyeHeight * std::pow(4.0f, static_cast<float>(miVisibleAreaLevelOfDetail));
		if (fEyeDistance > fBoundary * (1.0f - kfLevelOfDetailHysteresisFraction))
		{
			iLevelOfDetail = miVisibleAreaLevelOfDetail;
		}
	}
	else if (iLevelOfDetail == miVisibleAreaLevelOfDetail + 1)
	{
		float fBoundary = kfMinimumEyeHeight * std::pow(4.0f, static_cast<float>(miVisibleAreaLevelOfDetail + 1));
		if (fEyeDistance < fBoundary * (1.0f + kfLevelOfDetailHysteresisFraction))
		{
			iLevelOfDetail = miVisibleAreaLevelOfDetail;
		}
	}

	// Visible-area snapping uses the water LOD table's concatenated quad grid. The composite G-buffer render targets use that snapped area even
	// though terrain uses per-island Gaea2 meshes.
	const BufferManager::VisibleAreaMeshLod& rLevelOfDetailMesh = gpBufferManager->mWaterMeshLods[iLevelOfDetail];
	float fQuadsX = static_cast<float>(rLevelOfDetailMesh.iQuadCountX);
	float fQuadsY = static_cast<float>(rLevelOfDetailMesh.iQuadCountY);

	int64_t iZoomBucket = static_cast<int64_t>(std::floor(fEyeDistance));
	static constexpr float kfZoomBucketHysteresis = 0.1f;
	if (iZoomBucket == miVisibleAreaZoomBucket - 1 && fEyeDistance > static_cast<float>(miVisibleAreaZoomBucket) - kfZoomBucketHysteresis)
	{
		iZoomBucket = miVisibleAreaZoomBucket;
	}
	else if (iZoomBucket == miVisibleAreaZoomBucket + 1 && fEyeDistance < static_cast<float>(miVisibleAreaZoomBucket + 1) + kfZoomBucketHysteresis)
	{
		iZoomBucket = miVisibleAreaZoomBucket;
	}

	uint32_t uiLatchKey = static_cast<uint32_t>(rLevelOfDetailMesh.iQuadCountX)
	                    ^ (static_cast<uint32_t>(rLevelOfDetailMesh.iQuadCountY) << 16)
	                    ^ gpGraphics->mFramebufferVkExtent2D.width
	                    ^ (gpGraphics->mFramebufferVkExtent2D.height << 16);

	if (iZoomBucket != miVisibleAreaZoomBucket || uiLatchKey != muiVisibleAreaLatchKey || iLevelOfDetail != miVisibleAreaLevelOfDetail)
	{
		miVisibleAreaZoomBucket = iZoomBucket;
		muiVisibleAreaLatchKey = uiLatchKey;
		miVisibleAreaLevelOfDetail = iLevelOfDetail;
		mf2LatchedQuadSize.x = (mf4RenderVisibleArea.z - mf4RenderVisibleArea.x) / fQuadsX;
		mf2LatchedQuadSize.y = (mf4RenderVisibleArea.y - mf4RenderVisibleArea.w) / fQuadsY;
	}

	mf2VisibleAreaQuadSize = mf2LatchedQuadSize;
	mf4RenderVisibleArea.x = common::RoundDown(mf4RenderVisibleArea.x, mf2VisibleAreaQuadSize.x);
	mf4RenderVisibleArea.y = common::RoundDown(mf4RenderVisibleArea.y + mf2VisibleAreaQuadSize.y, mf2VisibleAreaQuadSize.y);
	mf4RenderVisibleArea.z = mf4RenderVisibleArea.x + fQuadsX * mf2VisibleAreaQuadSize.x;
	mf4RenderVisibleArea.w = mf4RenderVisibleArea.y - fQuadsY * mf2VisibleAreaQuadSize.y;
}

} // namespace engine

#endif // BT_CLIENT
