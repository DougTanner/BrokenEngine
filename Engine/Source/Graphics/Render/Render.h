#pragma once

#if defined(BT_CLIENT)

namespace game
{

struct FrameInterpolate;

} // namespace game

namespace engine
{

struct WorldSizedTexelArea
{
	XMFLOAT4 f4Area {};
	float fAspect = 0.0f;
	float fTangentHalfFieldOfView = 0.0f;
	float fWorldTexelX = 0.0f;
	float fWorldTexelY = 0.0f;
	float fFullWidth = 0.0f;
	float fFullHeight = 0.0f;

	XMFLOAT2 ComputeVisibleArea(float fEyeHeight) const
	{
		return {2.0f * fEyeHeight * fAspect * fTangentHalfFieldOfView, 2.0f * fEyeHeight * fTangentHalfFieldOfView};
	}
};

inline WorldSizedTexelArea XM_CALLCONV ComputeWorldSizedTexelArea(float fHeadroomMultiplier, float fTexelEyeHeight, float fTextureWidth, float fTextureHeight, float fAspect, float fFieldOfView, FXMVECTOR vecCameraPosition)
{
	float fTangentHalfFieldOfView = std::tan(0.5f * XMConvertToRadians(fFieldOfView / fAspect));
	float fWorldTexelX = (2.0f * fHeadroomMultiplier * fAspect * fTangentHalfFieldOfView / fTextureWidth) * fTexelEyeHeight;
	float fWorldTexelY = (2.0f * fHeadroomMultiplier * fTangentHalfFieldOfView / fTextureHeight) * fTexelEyeHeight;
	float fFullWidth = fTextureWidth * fWorldTexelX;
	float fFullHeight = fTextureHeight * fWorldTexelY;
	XMFLOAT4A f4CameraPosition {};
	XMStoreFloat4A(&f4CameraPosition, vecCameraPosition);
	int64_t iLeftTexel = static_cast<int64_t>(std::floor((f4CameraPosition.x - fFullWidth * 0.5f) / fWorldTexelX));
	int64_t iTopTexel = static_cast<int64_t>(std::floor((f4CameraPosition.y + fFullHeight * 0.5f) / fWorldTexelY));
	float fLeft = static_cast<float>(iLeftTexel) * fWorldTexelX;
	float fTop = static_cast<float>(iTopTexel) * fWorldTexelY;
	return WorldSizedTexelArea
	{
		.f4Area = {fLeft, fTop, fLeft + fFullWidth, fTop - fFullHeight},
		.fAspect = fAspect,
		.fTangentHalfFieldOfView = fTangentHalfFieldOfView,
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
	GridCoord coordinate {};

	[[nodiscard]] std::optional<XMFLOAT2> Advance(GridCoord cameraCoordinate)
	{
		int64_t iStepX = static_cast<int64_t>(cameraCoordinate.iX) - static_cast<int64_t>(coordinate.iX);
		int64_t iStepY = static_cast<int64_t>(cameraCoordinate.iY) - static_cast<int64_t>(coordinate.iY);
		XMFLOAT2 f2Offset = MakeRenderBasis(coordinate, cameraCoordinate).f2Offset;
		coordinate = cameraCoordinate;
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

// What the last published frame actually sent, for the read-only presentation_continuity_probe agent command; nothing
// in rendering reads it back. Every field is written on the client main loop thread inside RenderFrameGlobal, each by
// its own owner: GlobalUniforms.cpp writes shadow, LightingUniforms.cpp writes lighting, SmokeUniforms.cpp writes
// smoke, and WaterUniforms.cpp writes the camera basis, the water origins, fNoiseFrequency, and — because it is
// populated last — iPublishedFrames, so the counter advances only for a frame whose whole snapshot is complete. Each
// iHistoryResets counts only the multi-cell basis advances that discarded that owner's history.
struct RetainedAreaReport
{
	XMFLOAT4 f4CurrentArea {};
	XMFLOAT4 f4PreviousArea {};
	int64_t iHistoryResets = 0;
};

struct PresentationContinuitySnapshot
{
	int64_t iPublishedFrames = 0;
	GridCoord cameraBasisCoordinate {};
	XMFLOAT2 f2WaterOrigin {};
	XMFLOAT2 f2ReducedNoiseOrigin {};
	float fNoiseFrequency = 0.0f;
	RetainedAreaReport shadow {};
	RetainedAreaReport lighting {};
	RetainedAreaReport smoke {};
};

inline PresentationContinuitySnapshot gPresentationContinuity {};

void RenderFrameGlobal(int64_t iCommandBuffer, std::chrono::duration<float> currentTime);
void RenderFrameMain(int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates, GridCoord cameraCoordinate, std::chrono::duration<float> currentTime);

inline bool gbShadowTemporalReset = false; // Set by CreateShadowTextures; re-arms the PopulateShadowParameters first-frame guard so a recreate doesn't blend stale history for one frame

// Set by CreateLightingTextures; re-arms the PopulateLightingParameters first-frame guard so a recreate doesn't blend stale history for one frame.
inline bool gbLightingTemporalReset = false;
void RenderLightingGlobal(int64_t iCommandBuffer);
void RenderLightingMain(int64_t iCommandBuffer);
void RenderLightingSpreadIndirect(int64_t iCommandBuffer);

inline bool gbSmokeClear = true;

void RenderSmokeGlobal(int64_t iCommandBuffer);

inline int64_t giWindTextureIndex = 0; // 0 = write TextureOne, 1 = write TextureTwo

void RenderWindGlobal(int64_t iCommandBuffer);

void PopulateWaterParameters(shaders::GlobalLayout& rGlobalLayout, float fSunAngle, float fDayPercent, std::chrono::duration<float> elapsedTime);

} // namespace engine

#endif // defined(BT_CLIENT)
