#pragma once

#if defined(BT_CLIENT)

namespace engine
{

struct FrameInterpolateBase;

// Minimum eye height (LOD pivot floor). Single source for both the engine LOD-bucket math in
// Camera::CalculateMatricesAndVisibleArea and the zoom-clamp floor in Camera::UpdateEyeHeight.
inline constexpr float kfMinEyeHeight = 150.0f;

// Camera (display-rate, client-only, continuous)
struct CameraInput
{
	XMFLOAT2 f2Move {};
	int iScrollDelta = 0;
};

// Initial menu pose the derived camera hands to the generic base at construction.
struct CameraSetup
{
	XMVECTOR vecInitialPosition {};
};

class Camera;

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

	friend class Camera;
};

class Camera
{
public:

	static constexpr float kfDefaultSunAngle = 1.8f;
	static constexpr float kfCameraEyeHeightDefault = 300.0f;
	// Camera-height zoom-factor fade endpoint (2x default eye height = fully zoomed out). Single-sources the water
	// (WaterUniforms) and lighting (LightingUniforms) LerpAtHeight calls -- both reference this constant.
	static constexpr float kfWaveFadeEndHeight = 2.0f * kfCameraEyeHeightDefault;
	// Release zoom-out ceiling (dev builds zoom further; see EngineCamera.cpp). NOT a texel reference: the shadow/lighting
	// texel grids hold a constant on-screen pixel size at any height (the texels coarsen with zoom instead of cropping
	// coverage), so this is purely the gameplay limit on how far the camera can pull back.
	static constexpr float kfEyeHeightMaxRelease = 600.0f;
	// Headroom multipliers: the shadow and lighting (deposit/spread/combine) textures are allocated this much larger
	// than the wanted on-screen pixel size. Because their texel-height references never fall below live eye height,
	// this margin keeps the raw live-frustum footprints inside their textures throughout zoom transitions. At settled
	// height, the raw live-frustum window equals the wanted pixel size regardless of this value.
	static constexpr float kfShadowHeadroomMultiplier = 1.5f;
	static constexpr float kfLightingHeadroomMultiplier = 1.5f;
	// Initial zoom-target on construction: a few wheel-clicks above the default for a comfortable opening frame with
	// zoom range either way. WHEEL_DELTA (120) * kfEyeHeightPerWheelTick (0.1) = 12 units per click.
	static constexpr float kfCameraEyeHeightInitial = kfCameraEyeHeightDefault + 48.0f;

	XMMATRIX mMatView {};
	XMMATRIX mMatPerspective {};

	XMFLOAT2 f2VisibleAreaQuadSize {};
	XMFLOAT4 f4RenderVisibleArea {};
	XMFLOAT4 f4LargeVisibleArea {};

	XMFLOAT4 f4VisibleTopLeft {};
	XMFLOAT4 f4VisibleTopRight {};
	XMFLOAT4 f4VisibleBottomLeft {};
	XMFLOAT4 f4VisibleBottomRight {};

	XMVECTOR mVecPosition {};
	XMVECTOR mVecEyePosition {};
	XMVECTOR mVecToEyeNormal {};

	float mfShake = 0.0f;
	int64_t miFrame = 0;

	// Visible-area LOD (read by renderer to pick which mesh-LOD region to draw). Equation:
	// floor(log4(eyeDistance / kfMinEyeHeight)) clamped to BufferManager::kiVisibleAreaLodCount-1.
	// Higher LOD = fewer mesh quads (each dim halved per LOD; total quads /4 per LOD).
	int miVisibleAreaLod = 0;

	CameraInput mCameraInput {};

	float mfTime = 0.0f;

	float mfCameraEyeHeight = kfCameraEyeHeightInitial;
	float mfCameraEyeHeightTarget = kfCameraEyeHeightInitial;
	XMVECTOR mVecLastKnownPlayerPosition {};
	XMVECTOR mVecLastKnownPlayerVelocity {};
	float mfLastKnownPlayerTime = 0.0f;

	XMVECTOR mVecJumpStartPosition {};
	XMVECTOR mVecPreviousTargetPosition {};
	float mfJumpStartTime = 0.0f;
	bool mbJumping = false;

	float mfEyeStartHeight = 0.0f;
	float mfEyeStartVelocity = 0.0f;
	float mfEyeStartTime = 0.0f;
	float mfEyeVelocity = 0.0f;
	bool mbEyeZooming = false;

	// Eye-height reference that sizes the shadow texel grid. Expands immediately with outward zoom and contracts
	// at the shadow rate inward. 0 = uninitialized (snap directly to live height on first frame).
	float mfShadowTexelEyeHeight = 0.0f;

	// Eye-height reference that sizes the lighting texel grid. Expands immediately with outward zoom and contracts
	// at the independent lighting rate inward. 0 = uninitialized (snap directly to live height on first frame).
	float mfLightingTexelEyeHeight = 0.0f;

	explicit Camera(const CameraSetup& rCameraSetup);
	virtual ~Camera();

	void Update(const FrameInterpolateBase& rFrameInterpolate, float fDeltaTime);

	void ResetForSession();
	void RestoreEyeHeight(float fEyeHeight);

	virtual float SunAngle() const { return mfSunAngle; }
	float RawSunAngle() const { return mfSunAngle; }
	void ResetSunAngle() { mfSunAngle = kfDefaultSunAngle; }

	XMVECTOR XM_CALLCONV ScreenToWorld(FXMVECTOR vecScreenPos, float fHeight);
	XMVECTOR XM_CALLCONV WorldToScreen(FXMVECTOR vecWorldPos) const;

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

	float mfSunAngle = 1.4f;

	// Derived-camera policy. The concrete camera knows the real interpolate frame type and casts back to it.
	virtual bool IsMainMenuFrame(const FrameInterpolateBase& rFrameInterpolate) const = 0;
	virtual CameraTarget PullTarget(const FrameInterpolateBase& rFrameInterpolate) = 0;
	virtual void OnUpdateComplete() = 0;

	void CalculateMatricesAndVisibleArea();

private:

	XMVECTOR ResolveTarget(const CameraTarget& rCameraTarget);
	void UpdatePosition(FXMVECTOR vecTargetPosition, float fDeltaTime);
	void UpdateEyeHeight();

	XMFLOAT2 mf2LatchedQuadSize {};
	// 0 is unreachable as a real bucket (eye distance always >= kfMinEyeHeight = 150), so the
	// first-frame change-detect always fires. Using INT_MIN here would cause signed-integer
	// overflow when the hysteresis check evaluates `miVisibleAreaZoomBucket - 1`.
	int miVisibleAreaZoomBucket = 0;
	uint32_t muiVisibleAreaLatchKey = 0;
};

inline Camera* gpCamera = nullptr;

} // namespace engine

#endif // BT_CLIENT
