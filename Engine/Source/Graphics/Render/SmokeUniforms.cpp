#if defined(BT_CLIENT)

#include "Graphics/EngineCamera.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/SmokeWrappersBase.h"
#include "Render.h"

namespace engine
{

// Reciprocal of the previous smoke area's signed extents (Smoke/Wind OccupancyDilate remap divisor). Preserves
// the shader's component order exactly: X = 1/(z-x) (width), Y = 1/(w-y) (negative — max-Y is .y, min-Y is .w).
static void PopulatePreviousSmokeAreaSizeInverse(shaders::GlobalLayout& rGlobalLayout, const XMFLOAT4& rf4Area)
{
	rGlobalLayout.f2PreviousSmokeAreaSizeInverse.x = 1.0f / (rf4Area.z - rf4Area.x);
	rGlobalLayout.f2PreviousSmokeAreaSizeInverse.y = 1.0f / (rf4Area.w - rf4Area.y);
}

// Smoke has three publishing exits, each with its own area pair; report whichever pair this frame actually published.
static void PublishSmokeContinuity(const XMFLOAT4& rf4CurrentArea, const XMFLOAT4& rf4PreviousArea)
{
	gPresentationContinuity.smoke.f4CurrentArea = rf4CurrentArea;
	gPresentationContinuity.smoke.f4PreviousArea = rf4PreviousArea;
}

void RenderSmokeGlobal(int64_t iCommandBuffer)
{
	shaders::GlobalLayout& rGlobalLayout = *reinterpret_cast<shaders::GlobalLayout*>(&gpBufferManager->mGlobalLayoutUniformBuffers.at(iCommandBuffer).mpMappedMemory[0]);

	rGlobalLayout.fSmokeMax = gSmokeMaximum.mfCurrent;
	rGlobalLayout.fSmokePower = gSmokePower.mfCurrent;
	rGlobalLayout.fSmokeDecay = gSmokeDecay.mfCurrent;

	rGlobalLayout.fSmokeColorMin = gSmokeColorMinimum.mfCurrent;
	rGlobalLayout.fSmokeColorMultiplier = gSmokeColorMultiplier.mfCurrent;
	rGlobalLayout.fSmokeLightingMultiplier = gSmokeLightingMultiplier.mfCurrent;
	rGlobalLayout.fSmokeIntensityFalloff = gSmokeIntensityFalloff.mfCurrent;
	rGlobalLayout.fSmokeWindNoiseScale = gSmokeWindNoiseScale.mfCurrent;
	rGlobalLayout.fSmokeWindNoiseQuantity = gSmokeWindNoiseQuantity.mfCurrent;
	rGlobalLayout.fSmokeNoiseQuantity = gSmokeNoiseQuantity.mfCurrent;

	rGlobalLayout.fSmokeNoiseScaleOne = gSmokeNoiseScaleOne.mfCurrent;
	rGlobalLayout.fSmokeNoiseScaleTwo = gSmokeNoiseScaleTwo.mfCurrent;
	rGlobalLayout.fSmokeObjectHeightInv = 1.0f / gSmokeObjectHeight.mfCurrent;
	rGlobalLayout.fSmokeEdgeDecayDistanceInverse = 1.0f / gSmokeEdgeDecayDistance.mfCurrent;

	uint32_t uiTextureOneWidth = gpTextureManager->mRenderTargetTextures.mSmokeTextureOne.mInfo.vkExtent3D.width;
	uint32_t uiTextureOneHeight = gpTextureManager->mRenderTargetTextures.mSmokeTextureOne.mInfo.vkExtent3D.height;
	uint32_t uiMaxWidth = std::max(uiTextureOneWidth, gpTextureManager->mRenderTargetTextures.mSmokeTextureTwo.mInfo.vkExtent3D.width);
	uint32_t uiMaxHeight = std::max(uiTextureOneHeight, gpTextureManager->mRenderTargetTextures.mSmokeTextureTwo.mInfo.vkExtent3D.height);
	rGlobalLayout.uiSmokeTilesX = TileCount(uiMaxWidth);
	rGlobalLayout.uiSmokeTilesY = TileCount(uiMaxHeight);
	rGlobalLayout.fSmokeDepositTileScale = static_cast<float>(uiMaxWidth) / static_cast<float>(uiTextureOneWidth);

	// World-area follows the visible area each frame: aspect inherits from the framebuffer,
	// size grows with camera zoom-out. gSmokeSimulationArea acts as a margin multiplier.
	const XMFLOAT4& rf4Visible = engine::gpCamera->mf4RenderVisibleArea;
	float fCenterX = 0.5f * (rf4Visible.x + rf4Visible.z);
	float fCenterY = 0.5f * (rf4Visible.y + rf4Visible.w);
	float fHalfWidth = 0.5f * (rf4Visible.z - rf4Visible.x) * gSmokeSimulationArea.mfCurrent;
	float fHalfHeight = 0.5f * (rf4Visible.y - rf4Visible.w) * gSmokeSimulationArea.mfCurrent;
	XMFLOAT4 f4CurrentSmokeArea {fCenterX - fHalfWidth, fCenterY + fHalfHeight, fCenterX + fHalfWidth, fCenterY - fHalfHeight};

	static bool sbSmoke = false;
	if (sbSmoke != gSmokeEnabled.Get<bool>())
	{
		sbSmoke = gSmokeEnabled.Get<bool>();
		gbSmokeClear = true;
	}

	// Persisted across frames so the next frame's spread starts with previous == current
	// after a clear/disabled span — avoids divide-by-zero in WorldToSmokeTexcoord(zero, ...)
	static bool sbPreviousAreaInitialized = false;
	static XMFLOAT4 sf4PreviousSmokeArea {};
	if (!sbPreviousAreaInitialized)
	{
		sf4PreviousSmokeArea = f4CurrentSmokeArea;
		sbPreviousAreaInitialized = true;
	}

	// The retained area — which wind also consumes through f4PreviousSmokeArea — is in the camera cell's frame, so
	// follow a camera cell change before it is published beside this frame's area. A one-cell step keeps the smoke
	// and wind textures usable; a larger step leaves no overlap, so it takes the existing clear below.
	static RetainedAreaBasis sRetainedAreaBasis {};
	if (std::optional<XMFLOAT2> of2Shift = sRetainedAreaBasis.Advance(engine::gpCamera->mBasisCoordinate))
	{
		ShiftArea(sf4PreviousSmokeArea, *of2Shift);
	}
	else
	{
		gbSmokeClear = true;
		++gPresentationContinuity.smoke.iHistoryResets;
	}

	if (gbSmokeClear)
	{
		gbSmokeClear = false;

		rGlobalLayout.f4SmokeArea = f4CurrentSmokeArea;
		rGlobalLayout.f4PreviousSmokeArea = f4CurrentSmokeArea;
		PopulatePreviousSmokeAreaSizeInverse(rGlobalLayout, f4CurrentSmokeArea);
		sf4PreviousSmokeArea = f4CurrentSmokeArea;
		PublishSmokeContinuity(f4CurrentSmokeArea, f4CurrentSmokeArea);

		gpPipelineManager->mpPipelines[kPipelineSmokeClearA].WriteIndirectBuffer(iCommandBuffer, 1);
		gpPipelineManager->mpPipelines[kPipelineSmokeClearB].WriteIndirectBuffer(iCommandBuffer, 1);

		return;
	}

	if (!gSmokeEnabled.Get<bool>())
	{
		rGlobalLayout.f4SmokeArea = sf4PreviousSmokeArea;
		rGlobalLayout.f4PreviousSmokeArea = sf4PreviousSmokeArea;
		PopulatePreviousSmokeAreaSizeInverse(rGlobalLayout, sf4PreviousSmokeArea);
		PublishSmokeContinuity(sf4PreviousSmokeArea, sf4PreviousSmokeArea);

		gpPipelineManager->mpPipelines[kPipelineSmokeClearA].WriteIndirectBuffer(iCommandBuffer, 0);
		gpPipelineManager->mpPipelines[kPipelineSmokeClearB].WriteIndirectBuffer(iCommandBuffer, 0);

		return;
	}

	rGlobalLayout.f4SmokeArea = f4CurrentSmokeArea;
	rGlobalLayout.f4PreviousSmokeArea = sf4PreviousSmokeArea;
	PopulatePreviousSmokeAreaSizeInverse(rGlobalLayout, sf4PreviousSmokeArea);
	PublishSmokeContinuity(f4CurrentSmokeArea, sf4PreviousSmokeArea);
	sf4PreviousSmokeArea = f4CurrentSmokeArea;

	gpPipelineManager->mpPipelines[kPipelineSmokeClearA].WriteIndirectBuffer(iCommandBuffer, 0);
	gpPipelineManager->mpPipelines[kPipelineSmokeClearB].WriteIndirectBuffer(iCommandBuffer, 0);
}

} // namespace engine

#endif // defined(BT_CLIENT)
