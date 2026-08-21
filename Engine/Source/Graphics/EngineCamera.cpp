#if defined(BT_CLIENT)

#include "EngineCamera.h"

#include "Ui/LightingWrappersBase.h"
#include "Ui/ShadowWrappersBase.h"
#include "Ui/WrapperBase.h"

namespace engine
{

// Mouse-wheel zoom: per-frame scroll delta nudges target height; current eases toward target
constexpr float kfEyeHeightPerWheelTick = 0.1f;
constexpr float kfEyeBlendDuration = 0.35f;
#if defined(BT_RELEASE)
constexpr float kfEyeHeightMax = Camera::kfEyeHeightMaxRelease; // Shipping: gameplay zoom-out ceiling
#else
constexpr float kfEyeHeightMax = 2000.0f; // Dev: full zoom range (texels just coarsen further, coverage preserved)
#endif

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

constexpr float Smoothstep(float t)
{
	t = std::clamp(t, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
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

Camera::Camera(const CameraSetup& rCameraSetup)
{
	ASSERT(gpCamera == nullptr);

	gpCamera = this;

	mVecPosition = rCameraSetup.vecInitialPosition;
}

Camera::~Camera()
{
	if (gpCamera == this)
	{
		gpCamera = nullptr;
	}
}

void Camera::Update(const FrameInterpolateBase& rFrameInterpolate, float fDeltaTime)
{
	// fDeltaTime is the sim-scaled render delta the engine just measured: wall delta multiplied by the active time
	// ratio (equal to wall time at ratio 1.0). Driving camera blend, shake decay, and mfTime off the same source
	// keeps the camera in sync with the interpolated player across vsync misses; otherwise different deltas would
	// produce visible relative stutter at high zoom.
	mfTime += fDeltaTime;

	// Decay camera shake using sim-scaled render time
	mfShake = std::max(mfShake - fDeltaTime * 2.0f, 0.0f);

	miFrame = rFrameInterpolate.iTick;

	bool bMainMenuFrame = IsMainMenuFrame(rFrameInterpolate);

	// Update sun angle with varying speeds (only during gameplay)
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

	// Free camera: WASD in main menu (debug only). Bypasses target/blend; W=0 movement vector preserves position W=1.
	bool bFreeCameraActive = false;
	if constexpr (kbFreeCamera)
	{
		if (bMainMenuFrame)
		{
			bFreeCameraActive = true;
			XMVECTOR vecMove = XMVectorSet(mCameraInput.f2Move.x, mCameraInput.f2Move.y, 0.0f, 0.0f);
			constexpr float kfFreeCameraSpeed = 200.0f;
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
	UpdateTexelEyeHeightReference(mfCameraEyeHeight, gShadowTexelRampMetersPerSec.Get(), fDeltaTime, mfShadowTexelEyeHeight);
	UpdateTexelEyeHeightReference(mfCameraEyeHeight, gLightingTexelRampMetersPerSec.Get(), fDeltaTime, mfLightingTexelEyeHeight);

	// Eye sits directly above target along +Z (straight-down view).
	// W=0 — eye-local offset, not a homogeneous point; added to mVecPosition (W=1) preserves position.
	auto vecEyePositionRelative = XMVectorSet(0.0f, 0.0f, mfCameraEyeHeight, 0.0f);
	mVecToEyeNormal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	mVecEyePosition = XMVectorAdd(mVecPosition, vecEyePositionRelative);

	// Set controller vibration based on camera shake
	float fVibration = std::pow(mfShake, 0.5f);
	gpRawInputManager->SetVibration(0, fVibration, fVibration);

	// Calculate matrices and visible area
	CalculateMatricesAndVisibleArea();

	OnUpdateComplete();
}

XMVECTOR Camera::ResolveTarget(const CameraTarget& rCameraTarget)
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

void Camera::UpdatePosition(FXMVECTOR vecTargetPosition, float fDeltaTime)
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
				float fT = Smoothstep(fElapsed / kfJumpDuration);
				mVecPosition = XMVectorLerp(mVecJumpStartPosition, vecTargetPosition, fT);
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

void Camera::UpdateEyeHeight()
{
	int iScrollDelta = mCameraInput.iScrollDelta;
	if (iScrollDelta != 0)
	{
		// Scale per-tick zoom delta with current eye height, bounded by sqrt so high altitudes don't get 4x ticks and over-build Hermite velocity that carries into low-altitude territory.
		float fEyeHeightDelta = static_cast<float>(iScrollDelta) * kfEyeHeightPerWheelTick * std::sqrt(mfCameraEyeHeight / kfCameraEyeHeightDefault);
		float fNewTarget = std::clamp(mfCameraEyeHeightTarget - fEyeHeightDelta, kfMinEyeHeight, kfEyeHeightMax);
		if (fNewTarget != mfCameraEyeHeightTarget)
		{
			// Re-anchor every tick that actually moves the target. Snapshot current position AND velocity so the new Hermite curve picks up continuously, eliminating mid-flight stepping that an endpoint-shifted smoothstep would produce.
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
			float fT = fEyeElapsed / kfEyeBlendDuration;
			float fT2 = fT * fT;
			float fT3 = fT2 * fT;
			float fH00 = 2.0f * fT3 - 3.0f * fT2 + 1.0f;
			float fH10 = fT3 - 2.0f * fT2 + fT;
			float fH01 = -2.0f * fT3 + 3.0f * fT2;
			mfCameraEyeHeight = fH00 * mfEyeStartHeight + fH10 * mfEyeStartVelocity * kfEyeBlendDuration + fH01 * mfCameraEyeHeightTarget;
			float fDH00 = 6.0f * fT2 - 6.0f * fT;
			float fDH10 = 3.0f * fT2 - 4.0f * fT + 1.0f;
			float fDH01 = -6.0f * fT2 + 6.0f * fT;
			mfEyeVelocity = (fDH00 * mfEyeStartHeight + fDH10 * mfEyeStartVelocity * kfEyeBlendDuration + fDH01 * mfCameraEyeHeightTarget) / kfEyeBlendDuration;
		}
	}
}

void Camera::ResetForSession()
{
	ResetSunAngle();
	mVecLastKnownPlayerPosition = {};
	mVecLastKnownPlayerVelocity = {};
	mfLastKnownPlayerTime = 0.0f;
	mVecJumpStartPosition = {};
	mVecPreviousTargetPosition = {};
	mfJumpStartTime = 0.0f;
	mbJumping = false;
	// Reinitialize both references directly to the new session's live zoom on the next camera update; do not carry
	// contraction across sessions.
	mfShadowTexelEyeHeight = 0.0f;
	mfLightingTexelEyeHeight = 0.0f;
}

void Camera::RestoreEyeHeight(float fEyeHeight)
{
	mfCameraEyeHeight = fEyeHeight;
	mfCameraEyeHeightTarget = fEyeHeight;
}

XMVECTOR XM_CALLCONV Camera::ScreenToWorld(FXMVECTOR vecScreenPos, float fHeight)
{
	XMVECTOR vecPlane = XMPlaneFromPointNormal(XMVectorSet(0.0f, 0.0f, fHeight, 1.0f), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));

	float fViewportWidth = static_cast<float>(gpGraphics->mFramebufferExtent2D.width);
	float fViewportHeight = static_cast<float>(gpGraphics->mFramebufferExtent2D.height);
	auto vecWorldPos = XMVectorMultiply(XMVectorSet(fViewportWidth, fViewportHeight, 1.0f, 1.0f), vecScreenPos);

	vecWorldPos = XMVectorSetZ(vecWorldPos, 0.0f);
	auto vecRayStart = XMVector3Unproject(vecWorldPos, 0.0f, 0.0f, fViewportWidth, fViewportHeight, 0.0f, 1.0f, mMatPerspective, mMatView, XMMatrixIdentity());
	vecWorldPos = XMVectorSetZ(vecWorldPos, 1.0f);
	auto vecRayEnd = XMVector3Unproject(vecWorldPos, 0.0f, 0.0f, fViewportWidth, fViewportHeight, 0.0f, 1.0f, mMatPerspective, mMatView, XMMatrixIdentity());

	// The SDK returns all-lane QNaN when the line is parallel to the plane (a camera looking exactly along Z=fHeight).
	XMVECTOR vecIntersect = XMPlaneIntersectLine(vecPlane, vecRayStart, vecRayEnd);
	return XMVector3IsNaN(vecIntersect) ? XMVectorSet(0.0f, 0.0f, fHeight, 1.0f) : vecIntersect;
}

XMVECTOR XM_CALLCONV Camera::WorldToScreen(FXMVECTOR vecWorldPos) const
{
	// Exact inverse of ScreenToWorld's unproject: identical viewport / matrix arguments so the Y-sign and
	// viewport convention are resolved by construction. Returns screen pixels in X/Y, projected depth in Z.
	float fViewportWidth = static_cast<float>(gpGraphics->mFramebufferExtent2D.width);
	float fViewportHeight = static_cast<float>(gpGraphics->mFramebufferExtent2D.height);
	// The scalar XMVector3Project overload builds its viewport offset with W=0, so the returned screen position
	// carries W=0; force the position W invariant.
	return XMVectorSetW(XMVector3Project(vecWorldPos, 0.0f, 0.0f, fViewportWidth, fViewportHeight, 0.0f, 1.0f, mMatPerspective, mMatView, XMMatrixIdentity()), 1.0f);
}

void Camera::CalculateMatricesAndVisibleArea()
{
	auto vecToEyeNormal = XMVector3Normalize(XMVectorSubtract(mVecEyePosition, mVecPosition));
	auto vecUp = XMVector3Cross(vecToEyeNormal, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
	mMatView = XMMatrixLookAtRH(mVecEyePosition, mVecPosition, vecUp);

	static constexpr float kfNearClip = 1.0f;
	static constexpr float kfMinFarClip = 400.0f;
	static constexpr float kfFarClipPerEyeDistance = 2.667f;
	float fEyeDistance = XMVectorGetX(XMVector3Length(XMVectorSubtract(mVecEyePosition, mVecPosition)));
	float fFarClip = std::max(kfMinFarClip, fEyeDistance * kfFarClipPerEyeDistance);
	float fViewportWidth = static_cast<float>(gpGraphics->mFramebufferExtent2D.width);
	float fViewportHeight = static_cast<float>(gpGraphics->mFramebufferExtent2D.height);
	float fAspectRatio = gpSwapchainManager->mfAspectRatio;
	mMatPerspective = XMMatrixPerspectiveFovRH(XMConvertToRadians(gFov.Get() / fAspectRatio), fAspectRatio, kfNearClip, fFarClip);

	// Create plane at Z=0 for projecting screen corners to world space
	XMVECTOR vecPlane = XMPlaneFromPointNormal(XMVectorZero(), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));

	XMMATRIX matIdentity = XMMatrixIdentity();

	XMVECTOR vecRayStart {};
	XMVECTOR vecRayEnd {};
	XMVECTOR vecIntersectPlane {};

	// Calculate visible area corners by unprojecting screen corners to world space at Z=0
	XMFLOAT3 f3ScreenPos { 0.0f, 0.0f, 0.0f };

	struct VisibleCorner
	{
		float fScreenX;
		float fScreenY;
		XMFLOAT4* pTarget;
	};
	const VisibleCorner corners[] =
	{
		{ 0.0f, 0.0f, &f4VisibleTopLeft },
		{ fViewportWidth, 0.0f, &f4VisibleTopRight },
		{ 0.0f, fViewportHeight, &f4VisibleBottomLeft },
		{ fViewportWidth, fViewportHeight, &f4VisibleBottomRight },
	};

	for (const VisibleCorner& rCorner : corners)
	{
		f3ScreenPos.x = rCorner.fScreenX;
		f3ScreenPos.y = rCorner.fScreenY;

		f3ScreenPos.z = 0.0f;
		vecRayStart = XMVector3Unproject(XMLoadFloat3(&f3ScreenPos), 0.0f, 0.0f, fViewportWidth, fViewportHeight, 0.0f, 1.0f, mMatPerspective, mMatView, matIdentity);
		f3ScreenPos.z = 1.0f;
		vecRayEnd = XMVector3Unproject(XMLoadFloat3(&f3ScreenPos), 0.0f, 0.0f, fViewportWidth, fViewportHeight, 0.0f, 1.0f, mMatPerspective, mMatView, matIdentity);

		vecIntersectPlane = XMPlaneIntersectLine(vecPlane, vecRayStart, vecRayEnd);
		// The SDK returns all-lane QNaN when the corner ray is parallel to the Z=0 plane; a NaN corner would
		// poison the visible area and every grid-snapped extent derived from it.
		if (XMVector3IsNaN(vecIntersectPlane)) [[unlikely]]
		{
			vecIntersectPlane = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		}

		XMStoreFloat4(rCorner.pTarget, vecIntersectPlane);
	}

	f4LargeVisibleArea = XMFLOAT4 {f4VisibleTopLeft.x, f4VisibleTopLeft.y, f4VisibleTopRight.x, f4VisibleBottomRight.y};

	if (gpGraphics->mFramebufferExtent2D.width > gpGraphics->mFramebufferExtent2D.height) [[likely]]
	{
		f4RenderVisibleArea = f4LargeVisibleArea;
		float fVisibleHeight = f4RenderVisibleArea.y - f4RenderVisibleArea.w;
		f4RenderVisibleArea.x -= gVisibleAreaExtraTop.Get() * fVisibleHeight;
		f4RenderVisibleArea.y += gVisibleAreaExtraTop.Get() * fVisibleHeight;
		f4RenderVisibleArea.z += gVisibleAreaExtraTop.Get() * fVisibleHeight;
		f4RenderVisibleArea.w -= gVisibleAreaExtraBottom.Get() * fVisibleHeight;
	}
	else [[unlikely]]
	{
		f4RenderVisibleArea = f4LargeVisibleArea;
	}

	// Adjust visible area in world space to align with terrain and water polygon grid.
	// LOD bucket: floor(log4(eyeDist / kfMinEyeHeight)) selects mesh density and snap-grid
	// coarseness in lockstep. Each LOD reduces per-dim mesh quads by 2 (total by 4); the snap
	// quad size scales accordingly so the visible-area edges only move when the camera crosses
	// a full coarse-LOD quad. Within a LOD, the integer-eye-distance bucket below latches the
	// per-frame quadSize, so sub-pixel FP drift in mfCameraEyeHeight cannot oscillate the snap
	// (see GitHub flicker investigation: floor(area/quadSize) amplifies any quadSize jitter by
	// ~area/quadSize, so quadSize must be bit-stable across consecutive frames).
	XMFLOAT4 f4RawAreaIn = f4RenderVisibleArea;

	int iLod = std::clamp(static_cast<int>(std::floor(std::log2(std::max(fEyeDistance, kfMinEyeHeight) / kfMinEyeHeight) * 0.5f)), 0, static_cast<int>(BufferManager::kiVisibleAreaLodCount) - 1);
	// LOD hysteresis: refuse to flip back across the shared boundary if eye distance is still
	// near it. Boundaries are at kfMinEyeHeight * 4^L; 5% band absorbs FP rounding around
	// asymptotic settling.
	constexpr float kfLodHysteresisFraction = 0.05f;
	if (iLod == miVisibleAreaLod - 1)
	{
		float fBoundary = kfMinEyeHeight * std::pow(4.0f, static_cast<float>(miVisibleAreaLod));
		if (fEyeDistance > fBoundary * (1.0f - kfLodHysteresisFraction))
		{
			iLod = miVisibleAreaLod;
		}
	}
	else if (iLod == miVisibleAreaLod + 1)
	{
		float fBoundary = kfMinEyeHeight * std::pow(4.0f, static_cast<float>(miVisibleAreaLod + 1));
		if (fEyeDistance < fBoundary * (1.0f + kfLodHysteresisFraction))
		{
			iLod = miVisibleAreaLod;
		}
	}

	// Quad counts for visible-area snap math come from the water mesh's LOD table; water still uses
	// the concat quad-grid scheme and matches the historical terrain quad density per LOD. Terrain
	// itself now uses per-island Gaea2 meshes (see CommandBufferRecordMain.cpp), but the visible-area
	// composite G-buffer RTTs continue to be sized by this snap-grid.
	const BufferManager::VisibleAreaMeshLod& rLodMesh = gpBufferManager->mWaterMeshLods[iLod];
	float fQuadsX = static_cast<float>(rLodMesh.iQuadCountX);
	float fQuadsY = static_cast<float>(rLodMesh.iQuadCountY);

	int iZoomBucket = static_cast<int>(std::floor(fEyeDistance));
	constexpr float kfZoomBucketHysteresis = 0.1f;
	if (iZoomBucket == miVisibleAreaZoomBucket - 1 && fEyeDistance > static_cast<float>(miVisibleAreaZoomBucket) - kfZoomBucketHysteresis)
	{
		iZoomBucket = miVisibleAreaZoomBucket;
	}
	else if (iZoomBucket == miVisibleAreaZoomBucket + 1 && fEyeDistance < static_cast<float>(miVisibleAreaZoomBucket + 1) + kfZoomBucketHysteresis)
	{
		iZoomBucket = miVisibleAreaZoomBucket;
	}

	uint32_t uiLatchKey = static_cast<uint32_t>(rLodMesh.iQuadCountX)
	                    ^ (static_cast<uint32_t>(rLodMesh.iQuadCountY) << 16)
	                    ^ gpGraphics->mFramebufferExtent2D.width
	                    ^ (gpGraphics->mFramebufferExtent2D.height << 16);

	if (iZoomBucket != miVisibleAreaZoomBucket || uiLatchKey != muiVisibleAreaLatchKey || iLod != miVisibleAreaLod)
	{
		miVisibleAreaZoomBucket = iZoomBucket;
		muiVisibleAreaLatchKey = uiLatchKey;
		miVisibleAreaLod = iLod;
		mf2LatchedQuadSize.x = (f4RenderVisibleArea.z - f4RenderVisibleArea.x) / fQuadsX;
		mf2LatchedQuadSize.y = (f4RenderVisibleArea.y - f4RenderVisibleArea.w) / fQuadsY;
	}

	f2VisibleAreaQuadSize = mf2LatchedQuadSize;
	f4RenderVisibleArea.x = common::RoundDown(f4RenderVisibleArea.x, f2VisibleAreaQuadSize.x);
	f4RenderVisibleArea.y = common::RoundDown(f4RenderVisibleArea.y + f2VisibleAreaQuadSize.y, f2VisibleAreaQuadSize.y);
	f4RenderVisibleArea.z = f4RenderVisibleArea.x + fQuadsX * f2VisibleAreaQuadSize.x;
	f4RenderVisibleArea.w = f4RenderVisibleArea.y - fQuadsY * f2VisibleAreaQuadSize.y;
}

} // namespace engine

#endif // BT_CLIENT
