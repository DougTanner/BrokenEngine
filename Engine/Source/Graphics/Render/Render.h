#pragma once

#if defined(BT_CLIENT)

namespace game
{

struct FrameInterpolate;

}

namespace engine
{

struct WorldSizedTexelArea
{
	XMFLOAT4 f4Area {};
	float fAspect = 0.0f;
	float fTanHalfFov = 0.0f;
	float fWorldTexelX = 0.0f;
	float fWorldTexelY = 0.0f;
	float fFullWidth = 0.0f;
	float fFullHeight = 0.0f;

	XMFLOAT2 ComputeVisibleArea(float fEyeHeight) const
	{
		return {2.0f * fEyeHeight * fAspect * fTanHalfFov, 2.0f * fEyeHeight * fTanHalfFov};
	}
};

inline WorldSizedTexelArea XM_CALLCONV ComputeWorldSizedTexelArea(float fHeadroomMultiplier, float fTexelEyeHeight, float fTextureWidth, float fTextureHeight, float fAspect, float fFov, FXMVECTOR vecCameraPosition)
{
	float fTanHalfFov = std::tan(0.5f * XMConvertToRadians(fFov / fAspect));
	float fWorldTexelX = (2.0f * fHeadroomMultiplier * fAspect * fTanHalfFov / fTextureWidth) * fTexelEyeHeight;
	float fWorldTexelY = (2.0f * fHeadroomMultiplier * fTanHalfFov / fTextureHeight) * fTexelEyeHeight;
	float fFullWidth = fTextureWidth * fWorldTexelX;
	float fFullHeight = fTextureHeight * fWorldTexelY;
	XMFLOAT4A f4CameraPosition {};
	XMStoreFloat4A(&f4CameraPosition, vecCameraPosition);
	int64_t iLeftTexel = static_cast<int64_t>(std::floor((f4CameraPosition.x - fFullWidth * 0.5f) / fWorldTexelX));
	int64_t iTopTexel = static_cast<int64_t>(std::floor((f4CameraPosition.y + fFullHeight * 0.5f) / fWorldTexelY));
	float fLeft = static_cast<float>(iLeftTexel) * fWorldTexelX;
	float fTop = static_cast<float>(iTopTexel) * fWorldTexelY;
	return WorldSizedTexelArea {
		.f4Area = {fLeft, fTop, fLeft + fFullWidth, fTop - fFullHeight},
		.fAspect = fAspect,
		.fTanHalfFov = fTanHalfFov,
		.fWorldTexelX = fWorldTexelX,
		.fWorldTexelY = fWorldTexelY,
		.fFullWidth = fFullWidth,
		.fFullHeight = fFullHeight,
	};
}

// Retained world rectangles (shadow and lighting history, and the held visible area) are expressed in the camera
// cell's frame, so they follow the camera when it changes cell. The whole-cell step keeps GPU history usable for the
// one-cell case the 3x3 subscription allows; a larger step leaves no overlap, so the owner resets instead.
struct RetainedAreaBasis
{
	GridCoord coord {};

	[[nodiscard]] std::optional<XMFLOAT2> Advance(GridCoord cameraCoord)
	{
		int64_t iStepX = static_cast<int64_t>(cameraCoord.x) - static_cast<int64_t>(coord.x);
		int64_t iStepY = static_cast<int64_t>(cameraCoord.y) - static_cast<int64_t>(coord.y);
		XMFLOAT2 f2Offset = MakeRenderBasis(coord, cameraCoord).f2Offset;
		coord = cameraCoord;
		if (iStepX < -1 || iStepX > 1 || iStepY < -1 || iStepY > 1)
		{
			return std::nullopt;
		}

		return f2Offset;
	}
};

inline void ShiftArea(XMFLOAT4& rf4Area, XMFLOAT2 f2Offset)
{
	rf4Area.x += f2Offset.x;
	rf4Area.z += f2Offset.x;
	rf4Area.y += f2Offset.y;
	rf4Area.w += f2Offset.y;
}

struct TemporalAreaLatch
{
	bool bInitialized = false;
	XMFLOAT4 f4PreviousArea {};

	float Update(const XMFLOAT4& rf4CurrentArea, bool& rbReset, float fBlend, XMFLOAT4& rf4PreviousArea)
	{
		if (rbReset)
		{
			rbReset = false;
			bInitialized = false;
		}

		float fResolvedBlend = fBlend;
		if (!bInitialized)
		{
			f4PreviousArea = rf4CurrentArea;
			bInitialized = true;
			fResolvedBlend = 1.0f;
		}

		rf4PreviousArea = f4PreviousArea;
		f4PreviousArea = rf4CurrentArea;
		return fResolvedBlend;
	}
};

void RenderFrameGlobal(int64_t iCommandBuffer, float fCurrentTime);
void RenderFrameMain(int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoords, GridCoord cameraCoord);

// Shadow
inline bool gbShadowTemporalReset = false; // Set by CreateShadowTextures; re-arms the PopulateShadowParameters first-frame guard so a recreate doesn't blend stale history for one frame

// Lighting
// Set by CreateLightingTextures; re-arms the PopulateLightingParameters first-frame guard so a recreate doesn't blend stale history for one frame.
inline bool gbLightingTemporalReset = false;
void RenderLightingGlobal(int64_t iCommandBuffer);
void RenderLightingMain(int64_t iCommandBuffer);
void RenderLightingSpreadIndirect(int64_t iCommandBuffer);

// Smoke
inline bool gbSmokeClear = true;

void RenderSmokeGlobal(int64_t iCommandBuffer);

// Wind
inline int64_t giWindTextureIndex = 0; // 0 = write TextureOne, 1 = write TextureTwo

void RenderWindGlobal(int64_t iCommandBuffer);

// Water
void PopulateWaterParameters(shaders::GlobalLayout& rGlobalLayout, float fSunAngle, float fDayPercent);

} // namespace engine

#endif // defined(BT_CLIENT)
