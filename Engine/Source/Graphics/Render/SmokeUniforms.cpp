#if defined(BT_CLIENT)

#include "Graphics/CameraBase.h"
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

// Repeated multiplication preserves gSmokeDecay exactly for a single step.
static void PublishSmokeStep(shaders::GlobalLayout& rGlobalLayout, int64_t iStepCount)
{
	float fDecay = 1.0f;
	for (int64_t i = 0; i < iStepCount; ++i)
	{
		fDecay *= gSmokeDecay.mfCurrent;
	}
	rGlobalLayout.fSmokeDecay = fDecay;
	rGlobalLayout.fSmokeStepCount = static_cast<float>(iStepCount);
}

// A zero Z group count skips both dilates and leaves the active-tile list at its reset, so neither spread runs.
static void WriteSmokeDilateDispatch(int64_t iCommandBuffer, int64_t iDilateGroups, int64_t iGroupCountZ)
{
	gpPipelineManager->mpPipelines[kPipelineSmokeOccupancyDilate].WriteIndirectComputeBuffer(iCommandBuffer, iDilateGroups, 1, iGroupCountZ);
	gpPipelineManager->mpPipelines[kPipelineSmokeOccupancyDilateRemap].WriteIndirectComputeBuffer(iCommandBuffer, iDilateGroups, 1, iGroupCountZ);
}

// Every exit publishes smoke through this, before its own held-area and frame-counter updates: the area and step
// operands may be those values.
static void PublishSmoke(shaders::GlobalLayout& rGlobalLayout, int64_t iCommandBuffer, const XMFLOAT4& rf4CurrentArea, const XMFLOAT4& rf4PreviousArea, int64_t iStepCount, int64_t iDilateGroups, int64_t iGroupCountZ)
{
	rGlobalLayout.f4SmokeArea = rf4CurrentArea;
	rGlobalLayout.f4PreviousSmokeArea = rf4PreviousArea;
	PopulatePreviousSmokeAreaSizeInverse(rGlobalLayout, rf4PreviousArea);
	gPresentationContinuity.smoke.f4CurrentArea = rf4CurrentArea;
	gPresentationContinuity.smoke.f4PreviousArea = rf4PreviousArea;
	PublishSmokeStep(rGlobalLayout, iStepCount);
	WriteSmokeDilateDispatch(iCommandBuffer, iDilateGroups, iGroupCountZ);
}

void RenderSmokeGlobal(int64_t iCommandBuffer)
{
	shaders::GlobalLayout& rGlobalLayout = *reinterpret_cast<shaders::GlobalLayout*>(&gpBufferManager->mGlobalLayoutUniformBuffers.at(iCommandBuffer).mpMappedMemory[0]);

	rGlobalLayout.fSmokeMax = gSmokeMaximum.mfCurrent;
	rGlobalLayout.fSmokePower = gSmokePower.mfCurrent;

	rGlobalLayout.fSmokeColorMin = gSmokeColorMinimum.mfCurrent;
	rGlobalLayout.fSmokeColorMultiplier = gSmokeColorMultiplier.mfCurrent;
	rGlobalLayout.fSmokeIntensityFalloff = gSmokeIntensityFalloff.mfCurrent;
	rGlobalLayout.fSmokeWindNoiseScale = gSmokeWindNoiseScale.mfCurrent;
	rGlobalLayout.fSmokeWindNoiseQuantity = gSmokeWindNoiseQuantity.mfCurrent;
	rGlobalLayout.fSmokeNoiseQuantity = gSmokeNoiseQuantity.mfCurrent;

	rGlobalLayout.fSmokeNoiseScaleOne = gSmokeNoiseScaleOne.mfCurrent;
	rGlobalLayout.fSmokeNoiseScaleTwo = gSmokeNoiseScaleTwo.mfCurrent;
	rGlobalLayout.fSmokeCurlScale = gSmokeCurlScale.mfCurrent;
	rGlobalLayout.fSmokeObjectHeightInverse = 1.0f / gSmokeObjectHeight.mfCurrent;
	rGlobalLayout.fSmokeObjectHeightPower = gSmokeObjectHeightPower.mfCurrent;
	rGlobalLayout.fSmokeEdgeDecayDistanceInverse = 1.0f / gSmokeEdgeDecayDistance.mfCurrent;

	int64_t iTextureOneWidth = gpTextureManager->mRenderTargetTextures.mSmokeTextureOne.mInfo.vkExtent3D.width;
	int64_t iTextureOneHeight = gpTextureManager->mRenderTargetTextures.mSmokeTextureOne.mInfo.vkExtent3D.height;
	int64_t iSmokeTilesX = TileCount(iTextureOneWidth);
	int64_t iSmokeTilesY = TileCount(iTextureOneHeight);
	rGlobalLayout.uiSmokeTilesX = static_cast<uint32_t>(iSmokeTilesX);
	rGlobalLayout.uiSmokeTilesY = static_cast<uint32_t>(iSmokeTilesY);
	int64_t iDilateGroups = (iSmokeTilesX * iSmokeTilesY + shaders::kiOccupancyDilateGroupSize - 1) / shaders::kiOccupancyDilateGroupSize;

	// World-area follows the visible area each frame: aspect inherits from the framebuffer,
	// size grows with camera zoom-out. gSmokeSimulationArea acts as a margin multiplier.
	const XMFLOAT4& rf4Visible = engine::gpCamera->mf4RenderVisibleArea;
	XMFLOAT4 f4CurrentSmokeArea = ScaleAreaAboutCenter(rf4Visible, gSmokeSimulationArea.mfCurrent);

	// The CPU provides SmokeCurlOffset with the smaller world-space dimension of a TextureOne texel divided by the
	// signed current-area extents. Uniform area scaling cancels when the current and held areas share an aspect. Two
	// zero strengths skip curl noise reads. ShiftArea preserves the held area's extent, and a framebuffer rebuild
	// reseeds it on the clear frame.
	float fSmokeAreaWidth = f4CurrentSmokeArea.z - f4CurrentSmokeArea.x;
	float fSmokeAreaHeight = f4CurrentSmokeArea.w - f4CurrentSmokeArea.y;
	float fMetersPerTexelX = std::abs(fSmokeAreaWidth) / static_cast<float>(iTextureOneWidth);
	float fMetersPerTexelY = std::abs(fSmokeAreaHeight) / static_cast<float>(iTextureOneHeight);
	bool bCurlEnabled = gSmokeCurlStrengthLow.mfCurrent != 0.0f || gSmokeCurlStrengthHigh.mfCurrent != 0.0f;
	float fCurlMeters = bCurlEnabled ? std::min(fMetersPerTexelX, fMetersPerTexelY) : 0.0f;
	rGlobalLayout.f2SmokeCurlOffsetScale.x = fCurlMeters / fSmokeAreaWidth;
	rGlobalLayout.f2SmokeCurlOffsetScale.y = fCurlMeters / fSmokeAreaHeight;
	rGlobalLayout.fSmokeCurlStrengthLow = gSmokeCurlStrengthLow.mfCurrent;
	rGlobalLayout.fSmokeCurlStrengthHigh = gSmokeCurlStrengthHigh.mfCurrent;

	static bool sbSmoke = false;
	if (sbSmoke != gSmokeEnabled.Get<bool>())
	{
		sbSmoke = gSmokeEnabled.Get<bool>();
		gbSmokeClear = true;
	}

	// Held smoke area, first written by the clear exit: gbSmokeClear starts true, so the first call always takes
	// that exit, before a zero-sized initial area can reach PopulatePreviousSmokeAreaSizeInverse.
	static XMFLOAT4 sf4PreviousSmokeArea {};

	// The retained area — which wind also consumes through f4PreviousSmokeArea — is in the camera cell's frame,
	// so follow a camera cell change before it is published beside this frame's area; the shift keeps the smoke
	// and wind textures usable.
	static RetainedAreaBasis sRetainedAreaBasis {};
	ShiftArea(sf4PreviousSmokeArea, sRetainedAreaBasis.Advance(engine::gpCamera->mBasisCoordinate));

	// A normal refresh runs both dilates and spreads once for the accumulated unpaused steps. On skipped frames, only
	// deposits update the smoke texture and occupancy.
	static int64_t siSmokeFramesSinceRefresh = 0;
	if (!gbGameplayPaused)
	{
		++siSmokeFramesSinceRefresh;
	}

	if (gbSmokeClear)
	{
		gbSmokeClear = false;

		PublishSmoke(rGlobalLayout, iCommandBuffer, f4CurrentSmokeArea, f4CurrentSmokeArea, 1, iDilateGroups, 1);
		sf4PreviousSmokeArea = f4CurrentSmokeArea;
		siSmokeFramesSinceRefresh = 0;

		gpPipelineManager->mpPipelines[kPipelineSmokeClearA].WriteIndirectBuffer(iCommandBuffer, 1);
		gpPipelineManager->mpPipelines[kPipelineSmokeClearB].WriteIndirectBuffer(iCommandBuffer, 1);

		return;
	}

	if (!gSmokeEnabled.Get<bool>())
	{
		PublishSmoke(rGlobalLayout, iCommandBuffer, sf4PreviousSmokeArea, sf4PreviousSmokeArea, 1, iDilateGroups, 1);
		siSmokeFramesSinceRefresh = 0;

		gpPipelineManager->mpPipelines[kPipelineSmokeClearA].WriteIndirectBuffer(iCommandBuffer, 0);
		gpPipelineManager->mpPipelines[kPipelineSmokeClearB].WriteIndirectBuffer(iCommandBuffer, 0);

		return;
	}

	gpPipelineManager->mpPipelines[kPipelineSmokeClearA].WriteIndirectBuffer(iCommandBuffer, 0);
	gpPipelineManager->mpPipelines[kPipelineSmokeClearB].WriteIndirectBuffer(iCommandBuffer, 0);

	// Smoke refreshes on the cadence's multiples, or early on a teleport.
	int64_t iSmokeUpdateCadence = gSmokeUpdateCadence.Get<int64_t>();
	bool bScheduled = gpGraphics->miFrameCounter % iSmokeUpdateCadence == 0;
	if (gbGameplayPaused || (!bScheduled && !IsVisibleCenterOutsideArea(rf4Visible, sf4PreviousSmokeArea)))
	{
		// Skip: smoke's and wind's remaps reduce to the identity over the held area, and the next refresh remaps
		// held -> current.
		PublishSmoke(rGlobalLayout, iCommandBuffer, sf4PreviousSmokeArea, sf4PreviousSmokeArea, 1, iDilateGroups, 0);
		return;
	}

	PublishSmoke(rGlobalLayout, iCommandBuffer, f4CurrentSmokeArea, sf4PreviousSmokeArea, siSmokeFramesSinceRefresh, iDilateGroups, 1);
	sf4PreviousSmokeArea = f4CurrentSmokeArea;
	siSmokeFramesSinceRefresh = 0;
}

} // namespace engine

#endif // defined(BT_CLIENT)
