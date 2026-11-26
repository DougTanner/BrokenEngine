#if defined(BT_CLIENT)

#include "CameraBase.h"

#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/LightingWrappersBase.h"
#include "Ui/MiscWrappersBase.h"
#include "Ui/ShadowWrappersBase.h"
#include "Ui/WrapperBase.h"

namespace engine
{

// Mouse-wheel zoom: per-frame scroll delta nudges target framing; current eases toward target
constexpr float kfFramingPerWheelTick = 0.1f;
constexpr float kfFramingBlendDuration = 0.35f;

constexpr float kfCameraPositionBlend = 8.0f;
// The blend alone stalls a rounding step short of a still target; snapping makes it land exactly. Sized for a 1:1 time ratio.
constexpr float kfCameraChaseSnapDistance = 0.01f;
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

// Cubic Hermite from (fStart, fStartVelocity) to (fTarget, 0) over fDuration.
static float EvaluateFramingEase(float fStart, float fStartVelocity, float fTarget, float fElapsed, float fDuration, float& rfVelocity)
{
	float fParameter = fElapsed / fDuration;
	float fParameterSquared = fParameter * fParameter;
	float fParameterCubed = fParameterSquared * fParameter;
	float fStartHeightBasis = 2.0f * fParameterCubed - 3.0f * fParameterSquared + 1.0f;
	float fStartVelocityBasis = fParameterCubed - 2.0f * fParameterSquared + fParameter;
	float fTargetHeightBasis = -2.0f * fParameterCubed + 3.0f * fParameterSquared;
	float fStartHeightBasisDerivative = 6.0f * fParameterSquared - 6.0f * fParameter;
	float fStartVelocityBasisDerivative = 3.0f * fParameterSquared - 4.0f * fParameter + 1.0f;
	float fTargetHeightBasisDerivative = -6.0f * fParameterSquared + 6.0f * fParameter;
	rfVelocity = (fStartHeightBasisDerivative * fStart + fStartVelocityBasisDerivative * fStartVelocity * fDuration + fTargetHeightBasisDerivative * fTarget) / fDuration;
	return fStartHeightBasis * fStart + fStartVelocityBasis * fStartVelocity * fDuration + fTargetHeightBasis * fTarget;
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
	// Match the Direct menu target's lowest-plane height so the opening pose needs no chase-blend ease.
	mVecPosition = XMVectorSetZ(rCameraSetup.vecInitialPosition, gpFlightPlanes->Lowest().fHeightMeters);
}

void CameraBase::ClearCellLoadedArea()
{
	mf4CellLoadedArea = {std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max()};
	mbPreviousCellLoadedAreaValid = false;
	mOptionalSubscriptionCenter = std::nullopt;
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
	if (meObserverState == ObserverState::kAwaitingReturn)
	{
		meObserverState = ObserverState::kFollowing;
	}
	ClearCellLoadedArea();
}

void CameraBase::SetObserverMode(bool bEnabled)
{
	if (bEnabled)
	{
		meObserverState = ObserverState::kObserving;
		mbJumping = false;
	}
	else if (meObserverState == ObserverState::kObserving)
	{
		meObserverState = ObserverState::kAwaitingReturn;
	}
	mVecObserverVelocity = {};
}

void CameraBase::StepObserverPan(float fDeltaTime, std::span<const GridCoord> allowedCoordinates)
{
	auto IsAllowedCell = [&](GridCoord coordinate)
	{
		return std::ranges::contains(allowedCoordinates, coordinate);
	};

	if (!IsInsideCell(mVecPosition))
	{
		for (GridCoord coordinate : allowedCoordinates)
		{
			if (IsInsideCell(XMVectorSubtract(mVecPosition, CellCenterOffset(mBasisCoordinate, coordinate))))
			{
				ShiftToRenderedCell(coordinate);
				break;
			}
		}
	}

	float fBaseHeight = gpFlightPlanes->Lowest().fHeightMeters;
	if (!IsAllowedCell(mBasisCoordinate))
	{
		GridCoord nearestCoordinate = allowedCoordinates.front();
		float fNearestDistanceSquared = XMVectorGetX(XMVector2LengthSq(XMVectorSubtract(mVecPosition, CellCenterOffset(mBasisCoordinate, nearestCoordinate))));
		for (GridCoord coordinate : allowedCoordinates.subspan(1))
		{
			float fDistanceSquared = XMVectorGetX(XMVector2LengthSq(XMVectorSubtract(mVecPosition, CellCenterOffset(mBasisCoordinate, coordinate))));
			if (fDistanceSquared < fNearestDistanceSquared)
			{
				fNearestDistanceSquared = fDistanceSquared;
				nearestCoordinate = coordinate;
			}
		}

		ShiftToRenderedCell(nearestCoordinate);
		if (!IsInsideCell(mVecPosition))
		{
			SegmentHit exitHit = TracePointToCellExit(XMVectorSet(0.0f, 0.0f, fBaseHeight, 1.0f), mVecPosition);
			mVecPosition = XMVectorSetW(XMVectorSetZ(NudgeIntoCell(exitHit.vecPosition), fBaseHeight), 1.0f);
		}
		mVecObserverVelocity = {};
	}

	if (fDeltaTime <= 0.0f)
	{
		return;
	}

	float fZoomScale = std::max(0.1f, 1.0f + gObserverZoomSpeedScaling.mfCurrent * ((mfCameraFraming / kfCameraFramingDefault) - 1.0f));
	float fMaximumSpeed = gObserverPanSpeed.mfCurrent * fZoomScale;
	bool bHasInput = mCameraInput.f2Move.x != 0.0f || mCameraInput.f2Move.y != 0.0f;
	XMVECTOR vecTargetVelocity = XMVectorSet(mCameraInput.f2Move.x * fMaximumSpeed, mCameraInput.f2Move.y * fMaximumSpeed, 0.0f, 0.0f);

	float fRate = bHasInput ? gObserverPanAcceleration.mfCurrent : gObserverPanDeceleration.mfCurrent;
	float fWeight = -std::expm1(-fRate * fDeltaTime);
	XMVECTOR vecVelocityDelta = XMVectorSubtract(mVecObserverVelocity, vecTargetVelocity);
	XMVECTOR vecDisplacement = XMVectorAdd(XMVectorScale(vecTargetVelocity, fDeltaTime), XMVectorScale(vecVelocityDelta, fWeight / fRate));
	mVecObserverVelocity = XMVectorAdd(mVecObserverVelocity, XMVectorScale(XMVectorSubtract(vecTargetVelocity, mVecObserverVelocity), fWeight));
	if (!bHasInput && XMVectorGetX(XMVector2Length(mVecObserverVelocity)) < 0.05f)
	{
		mVecObserverVelocity = {};
	}

	XMVECTOR vecPosition = mVecPosition;
	if (!IsInsideCell(vecPosition))
	{
		SegmentHit clampHit = TracePointToCellExit(XMVectorSet(0.0f, 0.0f, fBaseHeight, 1.0f), vecPosition);
		vecPosition = XMVectorSetW(XMVectorSetZ(NudgeIntoCell(clampHit.vecPosition), fBaseHeight), 1.0f);
	}

	auto PassesEdge = [](int64_t iDirection, FXMVECTOR vecDot, FXMVECTOR vecThreshold)
	{
		if (iDirection >= 1 && iDirection <= 3)
		{
			return XMVector2LessOrEqual(vecDot, vecThreshold);
		}
		return XMVector2Less(vecDot, vecThreshold);
	};

	GridCoord currentCoordinate = mBasisCoordinate;
	XMVECTOR vecRemaining = vecDisplacement;
	int64_t iBlockedDirection = -1;
	for (int64_t i = 0; i < 16; ++i)
	{
		XMVECTOR vecEnd = XMVectorSetW(XMVectorSetZ(XMVectorAdd(vecPosition, vecRemaining), fBaseHeight), 1.0f);
		if (IsInsideCell(vecEnd))
		{
			vecPosition = vecEnd;
			break;
		}

		// Find the first hex edge crossed along vecPosition -> vecEnd in travel order.
		int64_t iExitDirection = -1;
		float fBestTime = 2.0f;
		float fBestEndDot = std::numeric_limits<float>::lowest();
		for (int64_t k = 0; k < common::kiHexDirectionCount; ++k)
		{
			XMVECTOR vecNormal = XMLoadFloat2(&kCellDirections.pEntries[k].f2CenterOffset);
			XMVECTOR vecThreshold = XMVectorScale(XMVector2Dot(vecNormal, vecNormal), 0.5f);
			XMVECTOR vecEndDot = XMVector2Dot(vecEnd, vecNormal);
			if (PassesEdge(k, vecEndDot, vecThreshold))
			{
				continue;
			}

			float fStartDot = XMVectorGetX(XMVector2Dot(vecPosition, vecNormal));
			float fEndDot = XMVectorGetX(vecEndDot);
			float fDeltaDot = fEndDot - fStartDot;
			float fTime = fDeltaDot > 0.0f ? std::clamp((XMVectorGetX(vecThreshold) - fStartDot) / fDeltaDot, 0.0f, 1.0f) : 0.0f;
			if (iExitDirection < 0 || fTime < fBestTime || (fTime == fBestTime && fEndDot > fBestEndDot))
			{
				iExitDirection = k;
				fBestTime = fTime;
				fBestEndDot = fEndDot;
			}
		}

		XMVECTOR vecHit = XMVectorLerp(vecPosition, vecEnd, fBestTime);
		vecRemaining = XMVectorScale(vecRemaining, 1.0f - fBestTime);

		const common::HexDirection& rDirection = kCellDirections.pEntries[iExitDirection];
		GridCoord neighborCoordinate {};
		if (TryAddGridCoordinate(currentCoordinate, rDirection.iDeltaQ, rDirection.iDeltaR, neighborCoordinate) && IsAllowedCell(neighborCoordinate))
		{
			currentCoordinate = neighborCoordinate;
			iBlockedDirection = -1;
			vecPosition = XMVectorSetW(XMVectorSetZ(NudgeIntoCell(XMVectorSubtract(vecHit, XMLoadFloat2(&rDirection.f2CenterOffset))), fBaseHeight), 1.0f);
			if (!IsInsideCell(vecPosition))
			{
				SegmentHit clampHit = TracePointToCellExit(XMVectorSet(0.0f, 0.0f, fBaseHeight, 1.0f), vecPosition);
				vecPosition = XMVectorSetW(XMVectorSetZ(NudgeIntoCell(clampHit.vecPosition), fBaseHeight), 1.0f);
			}
			continue;
		}

		vecPosition = XMVectorSetW(XMVectorSetZ(NudgeIntoCell(vecHit), fBaseHeight), 1.0f);
		if (!IsInsideCell(vecPosition))
		{
			SegmentHit clampHit = TracePointToCellExit(XMVectorSet(0.0f, 0.0f, fBaseHeight, 1.0f), vecPosition);
			vecPosition = XMVectorSetW(XMVectorSetZ(NudgeIntoCell(clampHit.vecPosition), fBaseHeight), 1.0f);
		}

		// Two distinct blocked edges in the same cell meet at a convex corner; stop there instead of
		// projecting sequentially and bouncing back along the second border.
		if (iBlockedDirection >= 0 && iExitDirection != iBlockedDirection)
		{
			mVecObserverVelocity = {};
			break;
		}
		iBlockedDirection = iExitDirection;

		XMVECTOR vecEdgeNormal = XMVector2Normalize(XMLoadFloat2(&rDirection.f2CenterOffset));
		float fOutwardSpeed = XMVectorGetX(XMVector2Dot(mVecObserverVelocity, vecEdgeNormal));
		if (fOutwardSpeed > 0.0f)
		{
			mVecObserverVelocity = XMVectorSubtract(mVecObserverVelocity, XMVectorScale(vecEdgeNormal, fOutwardSpeed));
		}

		float fRemainingOutward = XMVectorGetX(XMVector2Dot(vecRemaining, vecEdgeNormal));
		if (fRemainingOutward > 0.0f)
		{
			vecRemaining = XMVectorSubtract(vecRemaining, XMVectorScale(vecEdgeNormal, fRemainingOutward));
		}
		else if (fBestTime <= 0.0f)
		{
			break;
		}

		if (XMVectorGetX(XMVector2LengthSq(vecRemaining)) <= 1.0e-8f)
		{
			break;
		}
	}

	if (currentCoordinate != mBasisCoordinate)
	{
		ShiftToRenderedCell(currentCoordinate);
	}
	mVecPosition = vecPosition;
}

void CameraBase::ShiftToRenderedCell(GridCoord cameraCoordinate)
{
	if (cameraCoordinate == mBasisCoordinate)
	{
		return;
	}

	// Z and W stay zero: the step is planar, and the cached values it moves are homogeneous points.
	RenderBasis previousBasis = MakeRenderBasis(mBasisCoordinate, cameraCoordinate);
	XMVECTOR vecShift = XMVectorSet(previousBasis.f2Offset.x, previousBasis.f2Offset.y, 0.0f, 0.0f);
	mBasisCoordinate = cameraCoordinate;

	// A step of any distance moves the whole cached set by the same cell-centre offset. Both flight endpoints shift
	// together, so the smoothstep parameter and the elapsed jump clock are untouched and the flight stays monotonic
	// across the boundary.
	mVecPosition = XMVectorAdd(mVecPosition, vecShift);
	mVecEyePosition = XMVectorAdd(mVecEyePosition, vecShift);
	mVecWaterEyePosition = XMVectorAdd(mVecWaterEyePosition, vecShift);
	mVecJumpStartPosition = XMVectorAdd(mVecJumpStartPosition, vecShift);
	mVecPreviousTargetPosition = XMVectorAdd(mVecPreviousTargetPosition, vecShift);
	mVecLastKnownTargetPosition = XMVectorAdd(mVecLastKnownTargetPosition, vecShift);
	if (mOptionalSubscriptionCenter.has_value())
	{
		*mOptionalSubscriptionCenter = XMVectorAdd(*mOptionalSubscriptionCenter, vecShift);
	}
	ShiftArea(mf4PreviousCellLoadedArea, previousBasis.f2Offset);
}

void CameraBase::Update(const FrameInterpolateBase& rFrameInterpolate, float fDeltaTime, float fSunDeltaTime, bool bMainMenuFrame, const CameraTarget& rPulledTarget, std::optional<XMVECTOR> optionalSubscriptionCenter)
{
	// fDeltaTime is the sim-scaled render delta the engine just measured: wall delta multiplied by the active time
	// ratio (equal to wall time at ratio 1.0). Driving camera blend, shake decay, and mfTime off the same source
	// keeps the camera in sync with the interpolated target across vsync misses; otherwise different deltas would
	// produce visible relative stutter at high zoom.
	mfTime += fDeltaTime;

	// Rebase cached camera positions before using them in a new rendered cell. In Observer mode, StepObserverPan owns
	// mBasisCoordinate and handles rebasing before render interpolation.
	if (!(meObserverState == ObserverState::kObserving && !bMainMenuFrame))
	{
		ShiftToRenderedCell(rFrameInterpolate.renderBasis.coordinate);
	}

	mOptionalSubscriptionCenter = optionalSubscriptionCenter;

	mfShake = std::max(mfShake - fDeltaTime * 2.0f, 0.0f);

	miFrame = rFrameInterpolate.iTick;

	if (!bMainMenuFrame)
	{
		static constexpr float kfNightSpeedStart = XM_PI;
		static constexpr float kfNightSpeedEnd = XM_2PI;
		if (mfSunAngle >= kfNightSpeedStart && mfSunAngle < kfNightSpeedEnd)
		{
			mfSunAngle = mfSunAngle + fSunDeltaTime * 0.15f;
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

	// Observer mode and main-menu kbFreeCamera drive the pose directly, ignoring the pulled target.
	// Movement has W=0 so it preserves the position's W=1.
	bool bFreeCameraActive = false;
	if (meObserverState == ObserverState::kObserving && !bMainMenuFrame)
	{
		bFreeCameraActive = true;
		mbJumping = false;
	}
	else if constexpr (kbFreeCamera)
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
		// Hold the panned pose across ship-data gaps; resume normal following on any non-extrapolated target.
		bool bReturningFromObserver = false;
		if (meObserverState == ObserverState::kAwaitingReturn && rPulledTarget.meKind != CameraTarget::Kind::kExtrapolate)
		{
			meObserverState = ObserverState::kFollowing;
			bReturningFromObserver = true;
		}
		if (meObserverState != ObserverState::kAwaitingReturn)
		{
			// Until the first tracked target after DiscardTrackingCaches the position holds; that target sets it, no jump.
			// Leaving Observer mode always resumes through UpdatePosition from the panned pose.
			bool bHolding = mfLastKnownTargetTime == 0.0f && !bReturningFromObserver;
			XMVECTOR vecTargetPosition = ResolveTarget(rPulledTarget);
			if (bReturningFromObserver && mfLastKnownTargetTime == 0.0f)
			{
				mVecLastKnownTargetPosition = vecTargetPosition;
				mVecLastKnownTargetVelocity = {};
				mfLastKnownTargetTime = mfTime;
			}
			if (!bHolding)
			{
				UpdatePosition(vecTargetPosition, fDeltaTime);
			}
			else if (rPulledTarget.meKind == CameraTarget::Kind::kTracked)
			{
				mVecPosition = vecTargetPosition;
				mVecPreviousTargetPosition = vecTargetPosition;
			}
		}
	}
	UpdateFraming();
	UpdateFramingOverride();

	CalculateMatricesAndVisibleArea();

	// Keep each world-texel reference at or above the rendered framing times the coverage ratio. Increases snap
	// immediately for full viewport coverage; decreases contract at the existing independent rates so the density change
	// remains gradual. Both references converge to that product once the footprint settles.
	float fRenderFraming = RenderFraming();
	float fCoveredFraming = fRenderFraming * mfTexelCoverageRatio;
	// Above the gameplay ceiling (the map zoom) both references track rendered framing, so a return from the map does not
	// leave the texel grids contracting from the map framing.
	if (fRenderFraming > kfFramingMaximum)
	{
		mfShadowTexelFraming = 0.0f;
		mfLightingTexelFraming = 0.0f;
	}
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
		return XMVectorSetZ(rCameraTarget.mVecPosition, gpFlightPlanes->Lowest().fHeightMeters);
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
		if (XMVectorGetX(XMVector3Length(XMVectorSubtract(vecTargetPosition, mVecPosition))) < kfCameraChaseSnapDistance)
		{
			mVecPosition = vecTargetPosition;
		}
	}

	mVecPreviousTargetPosition = vecTargetPosition;
}

void CameraBase::UpdateFraming()
{
	int64_t iScrollDelta = mCameraInput.iScrollDelta;
	if (iScrollDelta != 0 && !mbFramingOverride)
	{
		// Scale per-tick zoom delta with current framing, bounded by sqrt so wide framings don't get 4x ticks and over-build Hermite velocity that carries into close framings.
		float fFramingDelta = static_cast<float>(iScrollDelta) * kfFramingPerWheelTick * std::sqrt(mfCameraFraming / kfCameraFramingDefault);
		float fNewTarget = std::clamp(mfCameraFramingTarget - fFramingDelta, kfFramingMinimum, kfFramingMaximum);
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
			mfCameraFraming = EvaluateFramingEase(mfFramingStart, mfFramingStartVelocity, mfCameraFramingTarget, fFramingElapsed, kfFramingBlendDuration, mfFramingVelocity);
		}
	}
}

void CameraBase::UpdateFramingOverride()
{
	if (!mbFramingOverride)
	{
		return;
	}

	float fTarget = mbFramingOverrideReleasing ? mfCameraFraming : mfOverrideTarget;
	float fOverrideElapsed = mfTime - mfOverrideStartTime;
	if (fOverrideElapsed >= mfOverrideBlendDuration)
	{
		mfOverrideFraming = fTarget;
		if (mbFramingOverrideReleasing)
		{
			mbFramingOverride = false;
		}
	}
	else
	{
		float fVelocity = 0.0f;
		mfOverrideFraming = EvaluateFramingEase(mfOverrideStart, 0.0f, fTarget, fOverrideElapsed, mfOverrideBlendDuration, fVelocity);
	}
}

void CameraBase::RestartOverrideEase(bool bReversing, float fPreviousEnd, float fBlendDuration)
{
	float fPreviousFraction = bReversing ? (mfTime - mfOverrideStartTime) / mfOverrideBlendDuration : 1.0f;
	if (fPreviousFraction < 1.0f)
	{
		// The ease is symmetric in time, so starting from the previous end at the mirrored point keeps the framing and the
		// speed, only reversed.
		mfOverrideStart = fPreviousEnd;
		mfOverrideStartTime = mfTime - (1.0f - fPreviousFraction) * fBlendDuration;
	}
	else
	{
		mfOverrideStart = mfOverrideFraming;
		mfOverrideStartTime = mfTime;
	}
	mfOverrideBlendDuration = fBlendDuration;
}

void CameraBase::SetFramingOverride(float fTarget, float fBlendDuration)
{
	ASSERT(std::isfinite(fTarget) && fTarget > 0.0f);
	if (!mbFramingOverride)
	{
		mfOverrideFraming = mfCameraFraming;
		mbFramingOverride = true;
		mbFramingOverrideReleasing = false;
	}
	RestartOverrideEase(mbFramingOverrideReleasing, mfCameraFraming, fBlendDuration);
	mbFramingOverrideReleasing = false;
	mfOverrideTarget = fTarget;
}

void CameraBase::ReleaseFramingOverride(float fBlendDuration)
{
	RestartOverrideEase(!mbFramingOverrideReleasing, mfOverrideTarget, fBlendDuration);
	mbFramingOverrideReleasing = true;
}

void CameraBase::ResetForSession()
{
	mfSunAngle = kfDefaultSunAngle;
	meObserverState = ObserverState::kFollowing;
	mVecObserverVelocity = {};
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
	// An unfinished wheel ease would otherwise pull the framing back from its old start.
	mfFramingVelocity = 0.0f;
	mbZooming = false;
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

// Draw-time culling tests only the row's position against the unscaled area, so the margin keeps rows a short pan
// would bring into view.
constexpr float kfSpawnVisibleAreaScale = 1.2f;

bool XM_CALLCONV CameraBase::InSpawnVisibleArea(GridCoord coordinate, FXMVECTOR vecLocalPosition)
{
	return InVisibleArea(ScaleAreaAboutCenter(mf4RenderVisibleArea, kfSpawnVisibleAreaScale), Rebase(MakeRenderBasis(coordinate, mBasisCoordinate), vecLocalPosition));
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
	float fRenderFraming = RenderFraming();
	float fTiltDegrees = mbOrthographicProjection ? 0.0f : gCameraTilt.mfCurrent;
	float fTilt = XMConvertToRadians(fTiltDegrees);
	float fFramingHalfHeight = fRenderFraming * std::tan(0.5f * XMConvertToRadians(kfFramingFieldOfView));
	float fEyeFieldOfView = mbOrthographicProjection ? gFieldOfView.mfDefault : gFieldOfView.mfCurrent;
	float fEyeDistance = fFramingHalfHeight / std::tan(0.5f * XMConvertToRadians(fEyeFieldOfView));
	// The eye orbits the look-at about +X: tilt 0 is straight down, positive tilt moves the eye south so the view looks
	// north. W=0 direction added to mVecPosition (W=1) preserves position.
	mVecToEyeNormal = XMVectorSet(0.0f, -std::sin(fTilt), std::cos(fTilt), 0.0f);
	mVecEyePosition = XMVectorAdd(mVecPosition, XMVectorScale(mVecToEyeNormal, fEyeDistance));
	mVecWaterEyePosition = XMVectorAdd(mVecPosition, XMVectorSet(0.0f, 0.0f, fRenderFraming, 0.0f));

	auto vecUpNormal = XMVector3Cross(mVecToEyeNormal, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
	mMatView = XMMatrixLookAtRH(mVecEyePosition, mVecPosition, vecUpNormal);

	// The near clip scales with eye distance so conventional-Z depth precision at the look-at stays bounded at narrow
	// fields of view and wide framings.
	static constexpr float kfNearClipPerEyeDistance = 1.0f / 150.0f;
	static constexpr float kfMinimumFarClip = 400.0f;
	static constexpr float kfFarClipPerEyeDistance = 2.667f;
	float fNearClip = fEyeDistance * kfNearClipPerEyeDistance;
	float fFarClip = std::max(kfMinimumFarClip, fEyeDistance * kfFarClipPerEyeDistance);
	float fAspectRatio = static_cast<float>(gpGraphics->mFramebufferVkExtent2D.width) / static_cast<float>(gpGraphics->mFramebufferVkExtent2D.height);
	mMatProjection = mbOrthographicProjection ? XMMatrixOrthographicRH(2.0f * fFramingHalfHeight * fAspectRatio, 2.0f * fFramingHalfHeight, fNearClip, fFarClip) : XMMatrixPerspectiveFovRH(XMConvertToRadians(gFieldOfView.mfCurrent), fAspectRatio, fNearClip, fFarClip);

	// Normalize the raw footprint by the reference-angle framing dimensions used to size both texel grids.
	float fTangentHalfFieldOfView = std::tan(0.5f * XMConvertToRadians(gFieldOfView.mfCurrent));
	float fLookAtHeight = XMVectorGetZ(mVecPosition);
	XMFLOAT4 f4Footprint = TiltedFootprint(fTilt, mbOrthographicProjection, fEyeDistance, fFramingHalfHeight, fTangentHalfFieldOfView, fAspectRatio, fLookAtHeight);
	mfTexelCoverageRatio = std::max((f4Footprint.z - f4Footprint.x) / (2.0f * fFramingHalfHeight * fAspectRatio), (f4Footprint.y - f4Footprint.w) / (2.0f * fFramingHalfHeight));
	mfTexelCenterOffsetNorth = 0.5f * (f4Footprint.w + f4Footprint.y);

	float fLookAtX = XMVectorGetX(mVecPosition);
	float fLookAtY = XMVectorGetY(mVecPosition);
	mf4LargeVisibleArea = XMFLOAT4 {fLookAtX + f4Footprint.x, fLookAtY + f4Footprint.y, fLookAtX + f4Footprint.z, fLookAtY + f4Footprint.w};

	// The cell loaded area uses the world framing's footprint at the subscription center's height so a framing override
	// or camera height change loads no extra cells, and stays empty when no subscription center is supplied.
	if (mOptionalSubscriptionCenter.has_value())
	{
		float fLoadedHeight = XMVectorGetZ(*mOptionalSubscriptionCenter);
		XMFLOAT4 f4LoadedFootprint = f4Footprint;
		if (mbFramingOverride || fLoadedHeight != fLookAtHeight)
		{
			float fWorldHalfHeight = mfCameraFraming * std::tan(0.5f * XMConvertToRadians(kfFramingFieldOfView));
			float fWorldEyeDistance = fWorldHalfHeight / std::tan(0.5f * XMConvertToRadians(fEyeFieldOfView));
			f4LoadedFootprint = TiltedFootprint(fTilt, mbOrthographicProjection, fWorldEyeDistance, fWorldHalfHeight, fTangentHalfFieldOfView, fAspectRatio, fLoadedHeight);
		}
		XMFLOAT2 f2Center {};
		XMStoreFloat2(&f2Center, *mOptionalSubscriptionCenter);
		ShiftArea(f4LoadedFootprint, f2Center);
		mf4CellLoadedArea = f4LoadedFootprint;
	}
	else
	{
		mf4CellLoadedArea = XMFLOAT4 {std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max()};
	}
	mbPreviousCellLoadedAreaValid = false;

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
	int64_t iLevelOfDetail = std::clamp<int64_t>(static_cast<int64_t>(std::floor(std::log2(std::max(fRenderFraming, kfLevelOfDetailFraming) / kfLevelOfDetailFraming) * 0.5f)), 0, BufferManager::kiVisibleAreaLodCount - 1);
	// LOD hysteresis: refuse to flip back across the shared boundary if framing is still
	// near it. Boundaries are at kfLevelOfDetailFraming * 4^L; 5% band absorbs FP rounding around
	// asymptotic settling.
	static constexpr float kfLevelOfDetailHysteresisFraction = 0.05f;
	if (iLevelOfDetail == miVisibleAreaLevelOfDetail - 1)
	{
		float fBoundary = kfLevelOfDetailFraming * std::pow(4.0f, static_cast<float>(miVisibleAreaLevelOfDetail));
		if (fRenderFraming > fBoundary * (1.0f - kfLevelOfDetailHysteresisFraction))
		{
			iLevelOfDetail = miVisibleAreaLevelOfDetail;
		}
	}
	else if (iLevelOfDetail == miVisibleAreaLevelOfDetail + 1)
	{
		float fBoundary = kfLevelOfDetailFraming * std::pow(4.0f, static_cast<float>(miVisibleAreaLevelOfDetail + 1));
		if (fRenderFraming < fBoundary * (1.0f + kfLevelOfDetailHysteresisFraction))
		{
			iLevelOfDetail = miVisibleAreaLevelOfDetail;
		}
	}

	// Visible-area snapping uses the water LOD table's concatenated quad grid. The composite G-buffer render targets use that snapped area even
	// though terrain uses per-island Gaea2 meshes.
	const BufferManager::VisibleAreaMeshLod& rLevelOfDetailMesh = gpBufferManager->mpWaterMeshLods[iLevelOfDetail];
	float fQuadsX = static_cast<float>(rLevelOfDetailMesh.iQuadCountX);
	float fQuadsY = static_cast<float>(rLevelOfDetailMesh.iQuadCountY);

	int64_t iZoomBucket = static_cast<int64_t>(std::floor(fRenderFraming));
	static constexpr float kfZoomBucketHysteresis = 0.1f;
	if (iZoomBucket == miVisibleAreaZoomBucket - 1 && fRenderFraming > static_cast<float>(miVisibleAreaZoomBucket) - kfZoomBucketHysteresis)
	{
		iZoomBucket = miVisibleAreaZoomBucket;
	}
	else if (iZoomBucket == miVisibleAreaZoomBucket + 1 && fRenderFraming < static_cast<float>(miVisibleAreaZoomBucket + 1) + kfZoomBucketHysteresis)
	{
		iZoomBucket = miVisibleAreaZoomBucket;
	}

	// Field of view, projection, tilt, and look-at height move the ground area at a fixed framing without moving the
	// bucket, so they re-latch the quad size through the key; otherwise the snapped span could fall short of the raw area.
	uint32_t uiLatchKey = static_cast<uint32_t>(rLevelOfDetailMesh.iQuadCountX) ^ (static_cast<uint32_t>(rLevelOfDetailMesh.iQuadCountY) << 16) ^ gpGraphics->mFramebufferVkExtent2D.width ^ (gpGraphics->mFramebufferVkExtent2D.height << 16) ^ std::bit_cast<uint32_t>(gFieldOfView.mfCurrent) ^ std::bit_cast<uint32_t>(fTiltDegrees) ^ std::bit_cast<uint32_t>(fLookAtHeight) ^ (static_cast<uint32_t>(mbOrthographicProjection) << 31);

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
