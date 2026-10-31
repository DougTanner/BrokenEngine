#if defined(BT_CLIENT)

#include "CameraBase.h"

#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/LightingWrappersBase.h"
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

// Bounding rectangle (minX, maxY, maxX, minY), relative to the look-at, of the four screen-corner rays meeting Z = 0
// and the highest flight plane, for an eye orbiting the look-at by fTilt radians about +X. It takes no look-at XY, so
// it stays bit-stable while the camera pans, and at tilt 0 it is exactly symmetric about the look-at.
static XMFLOAT4 TiltedFootprint(float fTilt, bool bOrthographic, float fEyeDistance, float fFramingHalfHeight, float fTangentHalfFieldOfView, float fAspectRatio, float fLookAtHeight)
{
	float fSine = std::sin(fTilt);
	float fCosine = std::cos(fTilt);
	XMFLOAT4 f4Footprint {std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max()};
	for (float fHeight : {0.0f, gpFlightPlanes->Highest().fHeightMeters})
	{
		// -1 is the bottom (south) screen edge, +1 the top (north) one.
		for (float fVertical : {-1.0f, 1.0f})
		{
			float fHalfWidth = 0.0f;
			float fY = 0.0f;
			if (bOrthographic)
			{
				fHalfWidth = fFramingHalfHeight * fAspectRatio;
				fY = fVertical * fFramingHalfHeight * fCosine + (fLookAtHeight + fVertical * fFramingHalfHeight * fSine - fHeight) * fSine / fCosine;
			}
			else
			{
				float fViewDepth = (fLookAtHeight + fEyeDistance * fCosine - fHeight) / (fCosine - fVertical * fSine * fTangentHalfFieldOfView);
				fHalfWidth = fViewDepth * fAspectRatio * fTangentHalfFieldOfView;
				fY = -fEyeDistance * fSine + fViewDepth * (fSine + fVertical * fCosine * fTangentHalfFieldOfView);
			}
			// The camera right vector is +X at any tilt, so the left and right corners mirror about the look-at.
			f4Footprint.x = std::min(f4Footprint.x, -fHalfWidth);
			f4Footprint.y = std::max(f4Footprint.y, fY);
			f4Footprint.z = std::max(f4Footprint.z, fHalfWidth);
			f4Footprint.w = std::min(f4Footprint.w, fY);
		}
	}
	return f4Footprint;
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

	CalculateMatricesAndVisibleArea();

	// Keep each world-texel reference at or above the live framing times the tilt coverage ratio: zero initialization,
	// outward zoom, and a tilt increase snap immediately for full viewport coverage, while inward zoom and a tilt
	// decrease contract at the existing independent rates so the density change remains gradual. At a settled framing
	// and tilt both references converge to that product.
	float fCoveredFraming = mfCameraFraming * mfTexelCoverageRatio;
	UpdateTexelFramingReference(fCoveredFraming, gShadowTexelRampMetersPerSecond.mfCurrent, fDeltaTime, mfShadowTexelFraming);
	UpdateTexelFramingReference(fCoveredFraming, gLightingTexelRampMetersPerSecond.mfCurrent, fDeltaTime, mfLightingTexelFraming);

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
	mfLightingTexelFraming = 0.0f;
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
	// The eye derives from the framing: it sits on the view axis at the distance from the look-at where the projection
	// shows the framing's half-height across the view-perpendicular plane through the look-at. Orthographic shows that
	// half-height at any distance, so it places a virtual eye at the default field of view.
	mbOrthographicProjection = gOrthographicProjection.Get<bool>();
	float fTiltDegrees = gCameraTilt.mfCurrent;
	float fTilt = XMConvertToRadians(fTiltDegrees);
	float fFramingHalfHeight = mfCameraFraming * std::tan(0.5f * XMConvertToRadians(kfFramingFieldOfView));
	float fEyeFieldOfView = mbOrthographicProjection ? gFieldOfView.mfDefault : gFieldOfView.mfCurrent;
	float fEyeDistance = fFramingHalfHeight / std::tan(0.5f * XMConvertToRadians(fEyeFieldOfView));
	// The eye orbits the look-at about +X: tilt 0 is straight down, positive tilt moves the eye south so the view looks
	// north. W=0 direction added to mVecPosition (W=1) preserves position.
	mVecToEyeNormal = XMVectorSet(0.0f, -std::sin(fTilt), std::cos(fTilt), 0.0f);
	mVecEyePosition = XMVectorAdd(mVecPosition, XMVectorScale(mVecToEyeNormal, fEyeDistance));
	mVecWaterEyePosition = XMVectorAdd(mVecPosition, XMVectorSet(0.0f, 0.0f, mfCameraFraming, 0.0f));

	auto vecToEyeNormal = XMVector3Normalize(XMVectorSubtract(mVecEyePosition, mVecPosition));
	auto vecUpNormal = XMVector3Cross(vecToEyeNormal, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
	mMatView = XMMatrixLookAtRH(mVecEyePosition, mVecPosition, vecUpNormal);

	// The near clip scales with eye distance so conventional-Z depth precision at the look-at stays bounded at narrow
	// fields of view and wide framings.
	static constexpr float kfNearClipPerEyeDistance = 1.0f / 150.0f;
	static constexpr float kfMinimumFarClip = 400.0f;
	static constexpr float kfFarClipPerEyeDistance = 2.667f;
	float fNearClip = fEyeDistance * kfNearClipPerEyeDistance;
	float fFarClip = std::max(kfMinimumFarClip, fEyeDistance * kfFarClipPerEyeDistance);
	float fAspectRatio = gpSwapchainManager->mfAspectRatio;
	mMatProjection = mbOrthographicProjection ? XMMatrixOrthographicRH(2.0f * fFramingHalfHeight * fAspectRatio, 2.0f * fFramingHalfHeight, fNearClip, fFarClip) : XMMatrixPerspectiveFovRH(XMConvertToRadians(gFieldOfView.mfCurrent), fAspectRatio, fNearClip, fFarClip);

	// One footprint feeds the visible area and the texel coverage. The tilt-0 call runs the same operations on the same
	// arguments when the tilt is 0, so the ratio is then exactly 1 and the symmetric footprint's center offset exactly 0.
	float fTangentHalfFieldOfView = std::tan(0.5f * XMConvertToRadians(gFieldOfView.mfCurrent));
	float fLookAtHeight = XMVectorGetZ(mVecPosition);
	XMFLOAT4 f4Footprint = TiltedFootprint(fTilt, mbOrthographicProjection, fEyeDistance, fFramingHalfHeight, fTangentHalfFieldOfView, fAspectRatio, fLookAtHeight);
	XMFLOAT4 f4UntiltedFootprint = TiltedFootprint(0.0f, mbOrthographicProjection, fEyeDistance, fFramingHalfHeight, fTangentHalfFieldOfView, fAspectRatio, fLookAtHeight);
	mfTexelCoverageRatio = std::max((f4Footprint.z - f4Footprint.x) / (f4UntiltedFootprint.z - f4UntiltedFootprint.x), (f4Footprint.y - f4Footprint.w) / (f4UntiltedFootprint.y - f4UntiltedFootprint.w));
	mfTexelCenterOffsetNorth = 0.5f * (f4Footprint.w + f4Footprint.y);

	float fLookAtX = XMVectorGetX(mVecPosition);
	float fLookAtY = XMVectorGetY(mVecPosition);
	mf4LargeVisibleArea = XMFLOAT4 {fLookAtX + f4Footprint.x, fLookAtY + f4Footprint.y, fLookAtX + f4Footprint.z, fLookAtY + f4Footprint.w};

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

	int64_t iLevelOfDetail = std::clamp<int64_t>(static_cast<int64_t>(std::floor(std::log2(std::max(mfCameraFraming, kfMinimumFraming) / kfMinimumFraming) * 0.5f)), 0, BufferManager::kiVisibleAreaLodCount - 1);
	// LOD hysteresis: refuse to flip back across the shared boundary if framing is still
	// near it. Boundaries are at kfMinimumFraming * 4^L; 5% band absorbs FP rounding around
	// asymptotic settling.
	static constexpr float kfLevelOfDetailHysteresisFraction = 0.05f;
	if (iLevelOfDetail == miVisibleAreaLevelOfDetail - 1)
	{
		float fBoundary = kfMinimumFraming * std::pow(4.0f, static_cast<float>(miVisibleAreaLevelOfDetail));
		if (mfCameraFraming > fBoundary * (1.0f - kfLevelOfDetailHysteresisFraction))
		{
			iLevelOfDetail = miVisibleAreaLevelOfDetail;
		}
	}
	else if (iLevelOfDetail == miVisibleAreaLevelOfDetail + 1)
	{
		float fBoundary = kfMinimumFraming * std::pow(4.0f, static_cast<float>(miVisibleAreaLevelOfDetail + 1));
		if (mfCameraFraming < fBoundary * (1.0f + kfLevelOfDetailHysteresisFraction))
		{
			iLevelOfDetail = miVisibleAreaLevelOfDetail;
		}
	}

	// Visible-area snapping uses the water LOD table's concatenated quad grid. The composite G-buffer render targets use that snapped area even
	// though terrain uses per-island Gaea2 meshes.
	const BufferManager::VisibleAreaMeshLod& rLevelOfDetailMesh = gpBufferManager->mpWaterMeshLods[iLevelOfDetail];
	float fQuadsX = static_cast<float>(rLevelOfDetailMesh.iQuadCountX);
	float fQuadsY = static_cast<float>(rLevelOfDetailMesh.iQuadCountY);

	int64_t iZoomBucket = static_cast<int64_t>(std::floor(mfCameraFraming));
	static constexpr float kfZoomBucketHysteresis = 0.1f;
	if (iZoomBucket == miVisibleAreaZoomBucket - 1 && mfCameraFraming > static_cast<float>(miVisibleAreaZoomBucket) - kfZoomBucketHysteresis)
	{
		iZoomBucket = miVisibleAreaZoomBucket;
	}
	else if (iZoomBucket == miVisibleAreaZoomBucket + 1 && mfCameraFraming < static_cast<float>(miVisibleAreaZoomBucket + 1) + kfZoomBucketHysteresis)
	{
		iZoomBucket = miVisibleAreaZoomBucket;
	}

	// Field of view, projection, and tilt move the ground area at a fixed framing without moving the bucket, so they
	// re-latch the quad size through the key; otherwise the snapped span could fall short of the raw area.
	uint32_t uiLatchKey = static_cast<uint32_t>(rLevelOfDetailMesh.iQuadCountX) ^ (static_cast<uint32_t>(rLevelOfDetailMesh.iQuadCountY) << 16) ^ gpGraphics->mFramebufferVkExtent2D.width ^ (gpGraphics->mFramebufferVkExtent2D.height << 16) ^ std::bit_cast<uint32_t>(gFieldOfView.mfCurrent) ^ std::bit_cast<uint32_t>(fTiltDegrees) ^ (static_cast<uint32_t>(mbOrthographicProjection) << 31);

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
