#pragma once

#if defined(BT_CLIENT)

#include "Frame/FrameUtils.h"

namespace engine
{

struct FrameInterpolateBase;

// The presentation basis for a cell whose positions are shown against cameraCoordinate's cell. The offset is the
// center offset of the two cells' axial delta, so it is deterministic for a given pair of cells, and it is the only
// value a grid coordinate contributes to presentation. Collection render bodies read the one stamped on their
// interpolate copy; every other conversion point builds its own here. Nothing publishes one render-wide for a
// renderer to find.
inline RenderBasis MakeRenderBasis(GridCoord coordinate, GridCoord cameraCoordinate)
{
	RenderBasis basis {.coordinate = coordinate};
	XMStoreFloat2(&basis.f2Offset, CellCenterOffset(cameraCoordinate, coordinate));
	return basis;
}

// Minimum framing (LOD pivot floor). Single source for both the engine LOD-bucket math in
// CameraBase::CalculateMatricesAndVisibleArea and the zoom-clamp floor in CameraBase::UpdateFraming.
inline constexpr float kfMinimumFraming = 150.0f;

// Camera (display-rate, client-only, continuous)
struct CameraInput
{
	XMFLOAT2 f2Move {};
	int64_t iScrollDelta = 0;
};

// Initial menu pose the derived camera hands to the generic base at construction.
struct CameraSetup
{
	XMVECTOR vecInitialPosition {};
};

class CameraBase;

// Closed target value produced by the derived camera's PullTarget and resolved by the generic update.
class CameraTarget
{
public:

	static CameraTarget Direct(FXMVECTOR vecPosition);
	static CameraTarget Tracked(FXMVECTOR vecRawPosition, FXMVECTOR vecVisualOffset);
	static CameraTarget Extrapolate();

private:

	enum class Kind : uint8_t
	{
		kDirect,
		kTracked,
		kExtrapolate,
	};

	CameraTarget(Kind eKind, XMVECTOR vecPosition, XMVECTOR vecVisualOffset);

	Kind meKind {};
	XMVECTOR mVecPosition {};
	XMVECTOR mVecVisualOffset {};

	friend class CameraBase;
};

class CameraBase : public common::Singleton<CameraBase>
{
public:

	static constexpr float kfDefaultSunAngle = 1.8f;
	// The zoom value is the framing: the eye height above the look-at that would show the current view at this vertical
	// field of view, in degrees. Every projection and field of view shows the same look-at-plane half-height,
	// framing * tan(kfFramingFieldOfView / 2); the eye is derived from it.
	static constexpr float kfFramingFieldOfView = 25.3125f;
	static constexpr float kfCameraFramingDefault = 300.0f;
	// Camera-height zoom-factor fade endpoint (2x default framing = fully zoomed out). Single-sources the water
	// (WaterUniforms) and lighting (LightingUniforms) LerpAtHeight calls -- both reference this constant.
	static constexpr float kfWaveFadeEndFraming = 2.0f * kfCameraFramingDefault;
	// Release zoom-out ceiling (dev builds zoom further; see kfFramingMaximum). NOT a texel reference: the shadow
	// texel grids hold a constant on-screen pixel size at any framing (the texels coarsen with zoom instead of cropping
	// coverage), so this is purely the gameplay limit on how far the camera can pull back.
	static constexpr float kfFramingMaximumRelease = 600.0f;
#if defined(BT_RELEASE)
	static constexpr float kfFramingMaximum = kfFramingMaximumRelease; // Shipping: gameplay zoom-out ceiling
#else
	static constexpr float kfFramingMaximum = 2'000.0f; // Dev: full zoom range (texels just coarsen further, coverage preserved)
#endif
	// Headroom multiplier: the shadow textures are allocated this much larger
	// than the wanted on-screen pixel size. Because the texel-framing reference never falls below live framing,
	// this margin keeps the raw live-frustum footprints inside their textures throughout zoom transitions. At settled
	// framing, the raw live-frustum window equals the wanted pixel size regardless of this value.
	static constexpr float kfShadowHeadroomMultiplier = 1.5f;
	// Initial zoom-target on construction: a few wheel-clicks above the default for a comfortable opening frame with
	// zoom range either way. WHEEL_DELTA (120) * kfFramingPerWheelTick (0.1) = 12 units per click.
	static constexpr float kfCameraFramingInitial = kfCameraFramingDefault + 48.0f;

	XMMATRIX mMatView {};
	XMMATRIX mMatProjection {};

	XMFLOAT2 mf2VisibleAreaQuadSize {};
	XMFLOAT4 mf4RenderVisibleArea {};
	XMFLOAT4 mf4LargeVisibleArea {};

	XMFLOAT4 mf4VisibleTopLeft {};
	XMFLOAT4 mf4VisibleTopRight {};
	XMFLOAT4 mf4VisibleBottomLeft {};
	XMFLOAT4 mf4VisibleBottomRight {};

	XMVECTOR mVecPosition {};
	XMVECTOR mVecEyePosition {};
	// The eye at kfFramingFieldOfView (framing above the look-at); the water eye vector, reflected ray, and reflection
	// Fresnel read it so they do not change with field of view or projection mode.
	XMVECTOR mVecWaterEyePosition {};
	XMVECTOR mVecToEyeNormal {};

	// gOrthographicProjection latched by CalculateMatricesAndVisibleArea; per-frame readers use this, never the wrapper.
	// Under orthographic projection the eye is a virtual eye at the default field of view and eye lines follow
	// mVecToEyeNormal.
	bool mbOrthographicProjection = false;

	// The cell every cached camera position, and every basis built for this render, is local to. Update follows the
	// rendered camera coord: on a one-cell step it shifts the cached positions so the flight stays continuous, on a
	// larger step it restarts tracking.
	GridCoord mBasisCoordinate {};

	float mfShake = 0.0f;
	int64_t miFrame = 0;

	XMVECTOR mVecVisualErrorOffset {};
	static constexpr float kfVisualErrorDecayRate = 15.0f;
	static constexpr float kfVisualErrorMaxDistance = 5.0f;
	static constexpr float kfVisualErrorMinDistance = 0.001f;

	// Visible-area LOD (read by renderer to pick which mesh-LOD region to draw). Equation:
	// floor(log4(framing / kfMinimumFraming)) clamped to BufferManager::kiVisibleAreaLodCount-1.
	// Higher LOD = fewer mesh quads (each dim halved per LOD; total quads /4 per LOD).
	int64_t miVisibleAreaLevelOfDetail = 0;

	CameraInput mCameraInput {};

	float mfTime = 0.0f;

	float mfCameraFraming = kfCameraFramingInitial;
	float mfCameraFramingTarget = kfCameraFramingInitial;
	XMVECTOR mVecLastKnownTargetPosition {};
	XMVECTOR mVecLastKnownTargetVelocity {};
	float mfLastKnownTargetTime = 0.0f;

	XMVECTOR mVecJumpStartPosition {};
	XMVECTOR mVecPreviousTargetPosition {};
	float mfJumpStartTime = 0.0f;
	bool mbJumping = false;

	float mfFramingStart = 0.0f;
	float mfFramingStartVelocity = 0.0f;
	float mfFramingStartTime = 0.0f;
	float mfFramingVelocity = 0.0f;
	bool mbZooming = false;

	// Framing reference that sizes the shadow texel grid. Expands immediately with outward zoom and contracts
	// at the shadow rate inward. 0 = uninitialized (snap directly to live framing on first frame).
	float mfShadowTexelFraming = 0.0f;

	explicit CameraBase(const CameraSetup& rCameraSetup);

	void Update(const FrameInterpolateBase& rFrameInterpolate, float fDeltaTime, float fSunDeltaTime, bool bMainMenuFrame, const CameraTarget& rPulledTarget);

	void ResetForSession();
	void RestoreFraming(float fFraming);

	// Adds the error to the offset, or zeroes the offset when the sum is longer than kfVisualErrorMaxDistance. Returns
	// the sum's length either way.
	float XM_CALLCONV AddVisualErrorOffset(FXMVECTOR vecError);

	float mfSunAngle = 1.4f;
	// World-space model sun rotation, identity unless the host writes it.
	glm::quat mModelSunRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

	XMVECTOR XM_CALLCONV WorldToScreen(FXMVECTOR vecWorldPosition) const;
	float GetRenderFraming() const;
	void UpdateComparisonCamera();

	inline bool XM_CALLCONV InVisibleArea(XMFLOAT4 f4VisibleArea, XMFLOAT4 f4Position, float fAdjustLeft = 0.0f, float fAdjustRight = 0.0f, float fAdjustTop = 0.0f, float fAdjustBottom = 0.0f)
	{
		return !(f4Position.x < f4VisibleArea.x - fAdjustLeft || f4Position.x > f4VisibleArea.z + fAdjustRight || f4Position.y > f4VisibleArea.y + fAdjustTop || f4Position.y < f4VisibleArea.w - fAdjustBottom);
	}

	inline bool XM_CALLCONV InVisibleArea(XMFLOAT4 f4VisibleArea, FXMVECTOR vecPosition, float fAdjustLeft = 0.0f, float fAdjustRight = 0.0f, float fAdjustTop = 0.0f, float fAdjustBottom = 0.0f)
	{
		XMFLOAT4A f4Position {};
		XMStoreFloat4A(&f4Position, vecPosition);
		return InVisibleArea(f4VisibleArea, f4Position, fAdjustLeft, fAdjustRight, fAdjustTop, fAdjustBottom);
	}

protected:

	void CalculateMatricesAndVisibleArea();

private:

	void DiscardTrackingCaches();
	void ShiftToRenderedCell(GridCoord cameraCoordinate);
	XMVECTOR ResolveTarget(const CameraTarget& rCameraTarget);
	void UpdatePosition(FXMVECTOR vecTargetPosition, float fDeltaTime);
	void UpdateFraming();

	XMFLOAT2 mf2LatchedQuadSize {};
	// Zero is below every real framing bucket, forcing the first update while keeping adjacent-bucket hysteresis arithmetic in range.
	int64_t miVisibleAreaZoomBucket = 0;
	uint32_t muiVisibleAreaLatchKey = 0;
};

inline CameraBase* gpCamera = nullptr;

} // namespace engine

#endif // BT_CLIENT
