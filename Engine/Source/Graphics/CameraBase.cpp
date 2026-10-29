#if defined(BT_CLIENT)

#include "CameraBase.h"

#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/ShadowWrappersBase.h"
#include "Ui/WrapperBase.h"

namespace engine
{

// Mouse-wheel zoom: per-frame scroll delta nudges target framing; current eases toward target
constexpr float kfFramingPerWheelTick = 0.1f;
constexpr float kfFramingBlendDuration = 0.35f;

constexpr float kfCameraPositionBlend = 8.0f;
constexpr float kfJumpDistanceThreshold = 50.0f;
constexpr float kfJumpDuration = 2.0f;
constexpr float kfJumpCancelThreshold = 5.0f;

static void UpdateTexelFramingReference(float fLiveFraming, float fContractionMetersPerSecond, float fDeltaTime, float& rfReferenceFraming)
{
	if (rfReferenceFraming == 0.0f || fLiveFraming > rfReferenceFraming)
	{
		rfReferenceFraming = fLiveFraming;
		return;
	}

	float fMaxContraction = fContractionMetersPerSecond * fDeltaTime;
	rfReferenceFraming = std::max(rfReferenceFraming - fMaxContraction, fLiveFraming);
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
: common::Singleton<CameraBase>(gpCamera, this)
{
	// Gameplay starts on the lowest flight plane; CameraBase::Update keeps the look-at there, so the opening pose needs no chase-blend ease.
	mVecPosition = XMVectorSetZ(rCameraSetup.vecInitialPosition, gpFlightPlanes->Lowest().fHeightMeters);
}

void CameraBase::DiscardTrackingCaches()
{
	mVecLastKnownTargetPosition = {};
	mVecLastKnownTargetVelocity = {};
	mfLastKnownTargetTime = 0.0f;
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

	int64_t iStepDistance = GridDistance(mBasisCoordinate, cameraCoordinate);
	// Z and W stay zero: the step is planar, and the cached values it moves are homogeneous points.
	RenderBasis previousBasis = MakeRenderBasis(mBasisCoordinate, cameraCoordinate);
	XMVECTOR vecShift = XMVectorSet(previousBasis.f2Offset.x, previousBasis.f2Offset.y, 0.0f, 0.0f);
	mBasisCoordinate = cameraCoordinate;

	if (iStepDistance > 1)
	{
		// Farther than the subscribed ring: nothing the camera cached describes a place inside the new cell, so drop
		// the tracking state and re-enter the cell at its center. The sun angle and the zoom state are not positional
		// and keep running.
		mVecPosition = XMVectorSetY(XMVectorSetX(mVecPosition, 0.0f), 0.0f);
		DiscardTrackingCaches();
		return;
	}

	// Edge-neighbour step: move the whole cached set by the same whole-cell delta. Both flight endpoints shift
	// together, so the smoothstep parameter and the elapsed jump clock are untouched and the flight stays monotonic
	// across the boundary.
	mVecPosition = XMVectorAdd(mVecPosition, vecShift);
	mVecEyePosition = XMVectorAdd(mVecEyePosition, vecShift);
	mVecWaterEyePosition = XMVectorAdd(mVecWaterEyePosition, vecShift);
	mVecJumpStartPosition = XMVectorAdd(mVecJumpStartPosition, vecShift);
	mVecPreviousTargetPosition = XMVectorAdd(mVecPreviousTargetPosition, vecShift);
	mVecLastKnownTargetPosition = XMVectorAdd(mVecLastKnownTargetPosition, vecShift);
}

void CameraBase::Update(const FrameInterpolateBase& rFrameInterpolate, float fDeltaTime, float fSunDeltaTime, bool bMainMenuFrame, const CameraTarget& rPulledTarget)
{
	// fDeltaTime is the sim-scaled render delta the engine just measured: wall delta multiplied by the active time
	// ratio (equal to wall time at ratio 1.0). Driving camera blend, shake decay, and mfTime off the same source
	// keeps the camera in sync with the interpolated target across vsync misses; otherwise different deltas would
	// produce visible relative stutter at high zoom.
	mfTime += fDeltaTime;

	// Before any position is read or written this frame: the camera lives in its own cell's local frame, so a change
	// of rendered cell must move every cached position into the new frame first.
	ShiftToRenderedCell(rFrameInterpolate.renderBasis.coordinate);

	mfShake = std::max(mfShake - fDeltaTime * 2.0f, 0.0f);

	miFrame = rFrameInterpolate.iTick;

	if (!bMainMenuFrame)
	{
		static constexpr float kfNightSpeedStart = XM_PI;
		static constexpr float kfNightSpeedEnd = XM_2PI;
		if (mfSunAngle >= kfNightSpeedStart && mfSunAngle < kfNightSpeedEnd)
		{
			mfSunAngle = mfSunAngle + fSunDeltaTime * 0.075f;
		}
		else
		{
			mfSunAngle = mfSunAngle + fSunDeltaTime * 0.01f;
		}

		if (mfSunAngle >= XM_2PI)
		{
			mfSunAngle = 0.0f;
		}
	}

	// Main-menu WASD moves the camera when kbFreeCamera is enabled and drives the pose directly, ignoring the pulled target.
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

	if (!bFreeCameraActive)
	{
		// The look-at sits on the lowest flight plane whatever the target's height, so a climb neither raises the eye nor
		// widens the Z = 0 visible area.
		XMVECTOR vecTargetPosition = XMVectorSetZ(ResolveTarget(rPulledTarget), gpFlightPlanes->Lowest().fHeightMeters);
		UpdatePosition(vecTargetPosition, fDeltaTime);
	}
	UpdateFraming();

	// Keep the shadow texel reference at or above the live framing: zero initialization and outward zoom snap
	// immediately for full viewport coverage, while inward zoom contracts at the existing rate so the
	// density change remains gradual. At a settled framing the reference converges to the live framing.
	UpdateTexelFramingReference(mfCameraFraming, gShadowTexelRampMetersPerSecond.mfCurrent, fDeltaTime, mfShadowTexelFraming);

	CalculateMatricesAndVisibleArea();

	// Decay the visual error offset for smooth reconciliation corrections on the sim-scaled render delta the camera and
	// entity interpolation use. The caller's PullTarget read it before this update.
	float fDecay = std::exp(-kfVisualErrorDecayRate * fDeltaTime);
	mVecVisualErrorOffset = XMVectorScale(mVecVisualErrorOffset, fDecay);
	if (XMVectorGetX(XMVector3Length(mVecVisualErrorOffset)) < kfVisualErrorMinDistance)
	{
		mVecVisualErrorOffset = {};
	}
}

XMVECTOR CameraBase::ResolveTarget(const CameraTarget& rCameraTarget)
{
	if (rCameraTarget.meKind == CameraTarget::Kind::kDirect)
	{
		return rCameraTarget.mVecPosition;
	}

	if (rCameraTarget.meKind == CameraTarget::Kind::kExtrapolate)
	{
		// Target not found — extrapolate from last known position and velocity
		float fElapsedTime = std::clamp(mfTime - mfLastKnownTargetTime, 0.0f, 2.0f);
		return XMVectorMultiplyAdd(XMVectorReplicate(fElapsedTime), mVecLastKnownTargetVelocity, mVecLastKnownTargetPosition);
	}

	// Compute velocity from position delta using sim-scaled render time. The raw tracked position feeds the velocity
	// and last-known state; the reconciliation offset is added only to the returned target.
	if (mfLastKnownTargetTime > 0.0f && mfTime > mfLastKnownTargetTime)
	{
		float fElapsedTime = mfTime - mfLastKnownTargetTime;
		mVecLastKnownTargetVelocity = XMVectorScale(XMVectorSubtract(rCameraTarget.mVecPosition, mVecLastKnownTargetPosition), 1.0f / fElapsedTime);
	}

	mVecLastKnownTargetPosition = rCameraTarget.mVecPosition;
	mfLastKnownTargetTime = mfTime;
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
		// Scale chase rate linearly with framing: slow/loose when zoomed in (close-up view stays calm), fast/tight when zoomed out (stays responsive).
		float fAdaptiveBlend = kfCameraPositionBlend * (mfCameraFraming / kfCameraFramingDefault);
		float fBlend = std::clamp(fDeltaTime * fAdaptiveBlend, 0.0f, 1.0f);
		mVecPosition = XMVectorMultiplyAdd(XMVectorReplicate(fBlend), vecTargetPosition, XMVectorMultiply(XMVectorReplicate(1.0f - fBlend), mVecPosition));
	}

	mVecPreviousTargetPosition = vecTargetPosition;
}

void CameraBase::UpdateFraming()
{
	int64_t iScrollDelta = mCameraInput.iScrollDelta;
	if (iScrollDelta != 0)
	{
		// Scale per-tick zoom delta with current framing, bounded by sqrt so wide framings don't get 4x ticks and over-build Hermite velocity that carries into close framings.
		float fFramingDelta = static_cast<float>(iScrollDelta) * kfFramingPerWheelTick * std::sqrt(mfCameraFraming / kfCameraFramingDefault);
		float fNewTarget = std::clamp(mfCameraFramingTarget - fFramingDelta, kfMinimumFraming, kfFramingMaximum);
		if (fNewTarget != mfCameraFramingTarget)
		{
			// Capture current framing and velocity whenever the zoom target changes so the Hermite curve remains continuous.
			mfFramingStart = mfCameraFraming;
			mfFramingStartVelocity = mfFramingVelocity;
			mfFramingStartTime = mfTime;
			mfCameraFramingTarget = fNewTarget;
			mbZooming = true;
		}
	}

	if (mbZooming)
	{
		float fFramingElapsed = mfTime - mfFramingStartTime;
		if (fFramingElapsed >= kfFramingBlendDuration)
		{
			mfCameraFraming = mfCameraFramingTarget;
			mfFramingVelocity = 0.0f;
			mbZooming = false;
		}
		else
		{
			// Cubic Hermite from (mfFramingStart, mfFramingStartVelocity) to (mfCameraFramingTarget, 0) over kfFramingBlendDuration.
			float fParameter = fFramingElapsed / kfFramingBlendDuration;
			float fParameterSquared = fParameter * fParameter;
			float fParameterCubed = fParameterSquared * fParameter;
			float fStartHeightBasis = 2.0f * fParameterCubed - 3.0f * fParameterSquared + 1.0f;
			float fStartVelocityBasis = fParameterCubed - 2.0f * fParameterSquared + fParameter;
			float fTargetHeightBasis = -2.0f * fParameterCubed + 3.0f * fParameterSquared;
			mfCameraFraming = fStartHeightBasis * mfFramingStart + fStartVelocityBasis * mfFramingStartVelocity * kfFramingBlendDuration + fTargetHeightBasis * mfCameraFramingTarget;
			float fStartHeightBasisDerivative = 6.0f * fParameterSquared - 6.0f * fParameter;
			float fStartVelocityBasisDerivative = 3.0f * fParameterSquared - 4.0f * fParameter + 1.0f;
			float fTargetHeightBasisDerivative = -6.0f * fParameterSquared + 6.0f * fParameter;
			mfFramingVelocity = (fStartHeightBasisDerivative * mfFramingStart + fStartVelocityBasisDerivative * mfFramingStartVelocity * kfFramingBlendDuration + fTargetHeightBasisDerivative * mfCameraFramingTarget) / kfFramingBlendDuration;
		}
	}
}

void CameraBase::ResetForSession()
{
	mfSunAngle = kfDefaultSunAngle;
	DiscardTrackingCaches();
	// Reinitialize both references directly to the new session's live zoom on the next camera update; do not carry
	// contraction across sessions.
	mfShadowTexelFraming = 0.0f;

	mVecVisualErrorOffset = {};
}

void CameraBase::RestoreFraming(float fFraming)
{
	mfCameraFraming = fFraming;
	mfCameraFramingTarget = fFraming;
}

float XM_CALLCONV CameraBase::AddVisualErrorOffset(FXMVECTOR vecError)
{
	XMVECTOR vecTotal = XMVectorAdd(mVecVisualErrorOffset, vecError);
	float fTotal = XMVectorGetX(XMVector3Length(vecTotal));
	if (fTotal > kfVisualErrorMaxDistance)
	{
		mVecVisualErrorOffset = {};
	}
	else
	{
		mVecVisualErrorOffset = vecTotal;
	}
	return fTotal;
}

XMVECTOR XM_CALLCONV CameraBase::WorldToScreen(FXMVECTOR vecWorldPosition) const
{
	// X/Y are screen pixels and Z is projected depth.
	float fViewportWidth = static_cast<float>(gpGraphics->mFramebufferVkExtent2D.width);
	float fViewportHeight = static_cast<float>(gpGraphics->mFramebufferVkExtent2D.height);
	// The scalar XMVector3Project overload builds its viewport offset with W=0, so the returned screen position
	// carries W=0; force the position W invariant.
	return XMVectorSetW(XMVector3Project(vecWorldPosition, 0.0f, 0.0f, fViewportWidth, fViewportHeight, 0.0f, 1.0f, mMatProjection, mMatView, XMMatrixIdentity()), 1.0f);
}

void CameraBase::CalculateMatricesAndVisibleArea()
{
	// The eye derives from the framing: it sits where the projection shows the framing's look-at-plane half-height.
	// Orthographic shows that half-height at any distance, so it places a virtual eye at the default field of view.
	mbOrthographicProjection = gOrthographicProjection.Get<bool>();
	float fFramingHalfHeight = mfCameraFraming * std::tan(0.5f * XMConvertToRadians(kfFramingFieldOfView));
	float fEyeFieldOfView = mbOrthographicProjection ? gFieldOfView.mfDefault : gFieldOfView.mfCurrent;
	float fEyeDistance = fFramingHalfHeight / std::tan(0.5f * XMConvertToRadians(fEyeFieldOfView));
	// Eye sits directly above target along +Z (straight-down view). W=0 direction added to mVecPosition (W=1) preserves position.
	mVecToEyeNormal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	mVecEyePosition = XMVectorAdd(mVecPosition, XMVectorScale(mVecToEyeNormal, fEyeDistance));
	mVecWaterEyePosition = XMVectorAdd(mVecPosition, XMVectorScale(mVecToEyeNormal, mfCameraFraming));

	auto vecToEyeNormal = XMVector3Normalize(XMVectorSubtract(mVecEyePosition, mVecPosition));
	auto vecUpNormal = XMVector3Cross(vecToEyeNormal, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
	mMatView = XMMatrixLookAtRH(mVecEyePosition, mVecPosition, vecUpNormal);

	// The near clip scales with eye distance so conventional-Z depth precision at the look-at stays bounded at narrow
	// fields of view and wide framings.
	float fNearClip = fEyeDistance * kfNearClipPerEyeDistance;
	float fFarClip = std::max(kfMinimumFarClip, fEyeDistance * kfFarClipPerEyeDistance);
	float fViewportWidth = static_cast<float>(gpGraphics->mFramebufferVkExtent2D.width);
	float fViewportHeight = static_cast<float>(gpGraphics->mFramebufferVkExtent2D.height);
	float fAspectRatio = gpSwapchainManager->mfAspectRatio;
	mMatProjection = mbOrthographicProjection ? XMMatrixOrthographicRH(2.0f * fFramingHalfHeight * fAspectRatio, 2.0f * fFramingHalfHeight, fNearClip, fFarClip) : XMMatrixPerspectiveFovRH(XMConvertToRadians(gFieldOfView.mfCurrent), fAspectRatio, fNearClip, fFarClip);

	float fVisibleAreaFraming = mfCameraFraming;
	float fComparisonEyeHeight = 0.0f;
	float fProjectionFieldOfView = XMConvertToRadians(gFieldOfView.mfCurrent);
	LightingCameraPose comparisonPose;
	if (gpGraphics->mpLighting != nullptr && gpGraphics->mpLighting->GetComparisonCamera(comparisonPose))
	{
		RenderBasis basis = MakeRenderBasis(comparisonPose.coordinate, mBasisCoordinate);
		mVecEyePosition = Rebase(basis, XMVectorSetW(XMLoadFloat3A(&comparisonPose.f3Eye), 1.0f));
		mVecPosition = Rebase(basis, XMVectorSetW(XMLoadFloat3A(&comparisonPose.f3Target), 1.0f));
		mVecToEyeNormal = XMVector3Normalize(XMVectorSubtract(mVecEyePosition, mVecPosition));
		mVecWaterEyePosition = mVecEyePosition;
		mbOrthographicProjection = false;
		fVisibleAreaFraming = comparisonPose.fFraming;
		fComparisonEyeHeight = comparisonPose.f3Eye.z;
		fProjectionFieldOfView = comparisonPose.fVerticalFieldOfView;
		mfShadowTexelFraming = std::max(mfShadowTexelFraming, comparisonPose.fFraming);
		mMatView = XMMatrixLookAtRH(mVecEyePosition, mVecPosition, XMLoadFloat3A(&comparisonPose.f3Up));
		mMatProjection = XMMatrixPerspectiveFovRH(comparisonPose.fVerticalFieldOfView, fAspectRatio, comparisonPose.fNear, comparisonPose.fFar);
	}

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
	const VisibleCorner pCorners[] =
	{
		{ .fScreenX = 0.0f, .fScreenY = 0.0f, .pTarget = &mf4VisibleTopLeft },
		{ .fScreenX = fViewportWidth, .fScreenY = 0.0f, .pTarget = &mf4VisibleTopRight },
		{ .fScreenX = 0.0f, .fScreenY = fViewportHeight, .pTarget = &mf4VisibleBottomLeft },
		{ .fScreenX = fViewportWidth, .fScreenY = fViewportHeight, .pTarget = &mf4VisibleBottomRight },
	};

	for (const VisibleCorner& rCorner : pCorners)
	{
		f3ScreenPosition.x = rCorner.fScreenX;
		f3ScreenPosition.y = rCorner.fScreenY;

		f3ScreenPosition.z = 0.0f;
		vecRayStart = XMVector3Unproject(XMLoadFloat3(&f3ScreenPosition), 0.0f, 0.0f, fViewportWidth, fViewportHeight, 0.0f, 1.0f, mMatProjection, mMatView, matIdentity);
		f3ScreenPosition.z = 1.0f;
		vecRayEnd = XMVector3Unproject(XMLoadFloat3(&f3ScreenPosition), 0.0f, 0.0f, fViewportWidth, fViewportHeight, 0.0f, 1.0f, mMatProjection, mMatView, matIdentity);

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
	// The integer framing bucket latches quad size within a level of detail to prevent sub-pixel framing drift from jittering snapped edges.
	// floor(area / quadSize) amplifies quad-size jitter by approximately area / quadSize, so quad size must stay stable while the latch key is unchanged.
	[[maybe_unused]] XMFLOAT4 f4RawInputArea = mf4RenderVisibleArea;

	int64_t iLevelOfDetail = std::clamp<int64_t>(static_cast<int64_t>(std::floor(std::log2(std::max(fVisibleAreaFraming, kfMinimumFraming) / kfMinimumFraming) * 0.5f)), 0, BufferManager::kiVisibleAreaLodCount - 1);
	// LOD hysteresis: refuse to flip back across the shared boundary if framing is still
	// near it. Boundaries are at kfMinimumFraming * 4^L; 5% band absorbs FP rounding around
	// asymptotic settling.
	static constexpr float kfLevelOfDetailHysteresisFraction = 0.05f;
	if (iLevelOfDetail == miVisibleAreaLevelOfDetail - 1)
	{
		float fBoundary = kfMinimumFraming * std::pow(4.0f, static_cast<float>(miVisibleAreaLevelOfDetail));
		if (fVisibleAreaFraming > fBoundary * (1.0f - kfLevelOfDetailHysteresisFraction))
		{
			iLevelOfDetail = miVisibleAreaLevelOfDetail;
		}
	}
	else if (iLevelOfDetail == miVisibleAreaLevelOfDetail + 1)
	{
		float fBoundary = kfMinimumFraming * std::pow(4.0f, static_cast<float>(miVisibleAreaLevelOfDetail + 1));
		if (fVisibleAreaFraming < fBoundary * (1.0f + kfLevelOfDetailHysteresisFraction))
		{
			iLevelOfDetail = miVisibleAreaLevelOfDetail;
		}
	}

	// Visible-area snapping uses the water LOD table's concatenated quad grid. The composite G-buffer render targets use that snapped area even
	// though terrain uses per-island Gaea2 meshes.
	const BufferManager::VisibleAreaMeshLod& rLevelOfDetailMesh = gpBufferManager->mpWaterMeshLods[iLevelOfDetail];
	float fQuadsX = static_cast<float>(rLevelOfDetailMesh.iQuadCountX);
	float fQuadsY = static_cast<float>(rLevelOfDetailMesh.iQuadCountY);

	int64_t iZoomBucket = static_cast<int64_t>(std::floor(fVisibleAreaFraming));
	static constexpr float kfZoomBucketHysteresis = 0.1f;
	if (iZoomBucket == miVisibleAreaZoomBucket - 1 && fVisibleAreaFraming > static_cast<float>(miVisibleAreaZoomBucket) - kfZoomBucketHysteresis)
	{
		iZoomBucket = miVisibleAreaZoomBucket;
	}
	else if (iZoomBucket == miVisibleAreaZoomBucket + 1 && fVisibleAreaFraming < static_cast<float>(miVisibleAreaZoomBucket + 1) + kfZoomBucketHysteresis)
	{
		iZoomBucket = miVisibleAreaZoomBucket;
	}

	// Field of view and projection move the Z = 0 area at a fixed framing without moving the bucket, so they re-latch
	// the quad size through the key; otherwise the snapped span could fall short of the raw area.
	uint64_t uiLatchKey = static_cast<uint32_t>(rLevelOfDetailMesh.iQuadCountX) ^ (static_cast<uint32_t>(rLevelOfDetailMesh.iQuadCountY) << 16) ^ gpGraphics->mFramebufferVkExtent2D.width ^ (gpGraphics->mFramebufferVkExtent2D.height << 16) ^ std::bit_cast<uint32_t>(fProjectionFieldOfView) ^ (static_cast<uint32_t>(mbOrthographicProjection) << 31);
	// Fixture zoom changes both height and tilt; fixed-height event pans only translate the footprint.
	uiLatchKey |= static_cast<uint64_t>(std::bit_cast<uint32_t>(fComparisonEyeHeight)) << 32;

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

float CameraBase::GetRenderFraming() const
{
	LightingCameraPose pose;
	return gpGraphics->mpLighting->GetComparisonCamera(pose) ? pose.fFraming : mfCameraFraming;
}

void CameraBase::UpdateComparisonCamera()
{
	LightingCameraPose pose;
	if (gpGraphics->mpLighting->GetComparisonCamera(pose))
	{
		CalculateMatricesAndVisibleArea();
	}
}

} // namespace engine

#endif // BT_CLIENT
