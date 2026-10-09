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

// Smoke has four publishing exits, each with its own area pair; report whichever pair this frame actually published.
static void PublishSmokeContinuity(const XMFLOAT4& rf4CurrentArea, const XMFLOAT4& rf4PreviousArea)
{
	gPresentationContinuity.smoke.f4CurrentArea = rf4CurrentArea;
	gPresentationContinuity.smoke.f4PreviousArea = rf4PreviousArea;
}

// One refresh advances smoke by every frame since the previous refresh. The decay is an integer power by repeated
// multiplication so a single step publishes gSmokeDecay exactly.
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

// Smoke area .y is max-Y and .w is min-Y. No margin: the held area must still cover the whole live visible area.
static bool IsVisibleAreaInsideHeldSmokeArea(const XMFLOAT4& rf4VisibleArea, const XMFLOAT4& rf4HeldSmokeArea)
{
	return rf4VisibleArea.x >= rf4HeldSmokeArea.x && rf4VisibleArea.z <= rf4HeldSmokeArea.z && rf4VisibleArea.y <= rf4HeldSmokeArea.y
	    && rf4VisibleArea.w >= rf4HeldSmokeArea.w;
}

void RenderSmokeGlobal(int64_t iCommandBuffer)
{
	shaders::GlobalLayout& rGlobalLayout = *reinterpret_cast<shaders::GlobalLayout*>(&gpBufferManager->mGlobalLayoutUniformBuffers.at(iCommandBuffer).mpMappedMemory[0]);

	rGlobalLayout.fSmokeMax = gSmokeMaximum.mfCurrent;
	rGlobalLayout.fSmokePower = gSmokePower.mfCurrent;

	rGlobalLayout.fSmokeColorMin = gSmokeColorMinimum.mfCurrent;
	rGlobalLayout.fSmokeColorMultiplier = gSmokeColorMultiplier.mfCurrent;
	rGlobalLayout.fSmokeLightingMultiplier = gSmokeLightingMultiplier.mfCurrent;
	rGlobalLayout.fSmokeIntensityFalloff = gSmokeIntensityFalloff.mfCurrent;
	rGlobalLayout.fSmokeWindNoiseScale = gSmokeWindNoiseScale.mfCurrent;
	rGlobalLayout.fSmokeWindNoiseQuantity = gSmokeWindNoiseQuantity.mfCurrent;
	rGlobalLayout.fSmokeNoiseQuantity = gSmokeNoiseQuantity.mfCurrent;

	rGlobalLayout.fSmokeNoiseScaleOne = gSmokeNoiseScaleOne.mfCurrent;
	rGlobalLayout.fSmokeNoiseScaleTwo = gSmokeNoiseScaleTwo.mfCurrent;
	rGlobalLayout.fSmokeCurlScale = gSmokeCurlScale.mfCurrent;
	rGlobalLayout.fSmokeObjectHeightInverse = 1.0f / gSmokeObjectHeight.mfCurrent;
	rGlobalLayout.fSmokeEdgeDecayDistanceInverse = 1.0f / gSmokeEdgeDecayDistance.mfCurrent;

	int64_t iTextureOneWidth = gpTextureManager->mRenderTargetTextures.mSmokeTextureOne.mInfo.vkExtent3D.width;
	int64_t iTextureOneHeight = gpTextureManager->mRenderTargetTextures.mSmokeTextureOne.mInfo.vkExtent3D.height;
	int64_t iMaxWidth = std::max(iTextureOneWidth, static_cast<int64_t>(gpTextureManager->mRenderTargetTextures.mSmokeTextureTwo.mInfo.vkExtent3D.width));
	int64_t iMaxHeight = std::max(iTextureOneHeight, static_cast<int64_t>(gpTextureManager->mRenderTargetTextures.mSmokeTextureTwo.mInfo.vkExtent3D.height));
	int64_t iSmokeTilesX = TileCount(iMaxWidth);
	int64_t iSmokeTilesY = TileCount(iMaxHeight);
	rGlobalLayout.uiSmokeTilesX = static_cast<uint32_t>(iSmokeTilesX);
	rGlobalLayout.uiSmokeTilesY = static_cast<uint32_t>(iSmokeTilesY);
	int64_t iDilateGroups = (iSmokeTilesX * iSmokeTilesY + shaders::kiOccupancyDilateGroupSize - 1) / shaders::kiOccupancyDilateGroupSize;
	rGlobalLayout.fSmokeDepositTileScale = static_cast<float>(iMaxWidth) / static_cast<float>(iTextureOneWidth);

	// World-area follows the visible area each frame: aspect inherits from the framebuffer,
	// size grows with camera zoom-out. gSmokeSimulationArea acts as a margin multiplier.
	const XMFLOAT4& rf4Visible = engine::gpCamera->mf4RenderVisibleArea;
	float fCenterX = 0.5f * (rf4Visible.x + rf4Visible.z);
	float fCenterY = 0.5f * (rf4Visible.y + rf4Visible.w);
	float fHalfWidth = 0.5f * (rf4Visible.z - rf4Visible.x) * gSmokeSimulationArea.mfCurrent;
	float fHalfHeight = 0.5f * (rf4Visible.y - rf4Visible.w) * gSmokeSimulationArea.mfCurrent;
	XMFLOAT4 f4CurrentSmokeArea {fCenterX - fHalfWidth, fCenterY + fHalfHeight, fCenterX + fHalfWidth, fCenterY - fHalfHeight};

	// Curl offset scale (SmokeCurlOffset): the finer pass-B input texel in meters over the signed area extents, zero
	// when both curl strengths are zero so the shader's uniform early-out skips the curl noise reads. Every exit
	// publishes this area or the held area, an earlier current area that ShiftArea moved without resizing. Both share
	// this construction's signs and framebuffer aspect, and the area size cancels in the quotient, so one value serves
	// every exit. After a framebuffer resize the held area, published by the skip and disabled exits and read as
	// previous by the next refresh, keeps the older aspect until a clear or refresh replaces it.
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

	// A refresh runs the dilates and spreads once, advancing smoke by every frame since the previous refresh.
	// Between refreshes only deposits change the smoke texture and its occupancy, and the held area
	// (sf4PreviousSmokeArea) stays as the last refresh left it.
	static int64_t siSmokeRenderFrame = 0;
	static int64_t siSmokeFramesSinceRefresh = 0;
	int64_t iSmokeUpdateCadence = gSmokeUpdateCadence.Get<int64_t>();
	++siSmokeRenderFrame;
	++siSmokeFramesSinceRefresh;

	if (gbSmokeClear)
	{
		gbSmokeClear = false;

		rGlobalLayout.f4SmokeArea = f4CurrentSmokeArea;
		rGlobalLayout.f4PreviousSmokeArea = f4CurrentSmokeArea;
		PopulatePreviousSmokeAreaSizeInverse(rGlobalLayout, f4CurrentSmokeArea);
		sf4PreviousSmokeArea = f4CurrentSmokeArea;
		PublishSmokeContinuity(f4CurrentSmokeArea, f4CurrentSmokeArea);
		PublishSmokeStep(rGlobalLayout, 1);
		siSmokeFramesSinceRefresh = 0;

		gpPipelineManager->mpPipelines[kPipelineSmokeClearA].WriteIndirectBuffer(iCommandBuffer, 1);
		gpPipelineManager->mpPipelines[kPipelineSmokeClearB].WriteIndirectBuffer(iCommandBuffer, 1);
		WriteSmokeDilateDispatch(iCommandBuffer, iDilateGroups, 1);

		return;
	}

	if (!gSmokeEnabled.Get<bool>())
	{
		rGlobalLayout.f4SmokeArea = sf4PreviousSmokeArea;
		rGlobalLayout.f4PreviousSmokeArea = sf4PreviousSmokeArea;
		PopulatePreviousSmokeAreaSizeInverse(rGlobalLayout, sf4PreviousSmokeArea);
		PublishSmokeContinuity(sf4PreviousSmokeArea, sf4PreviousSmokeArea);
		PublishSmokeStep(rGlobalLayout, 1);
		siSmokeFramesSinceRefresh = 0;

		gpPipelineManager->mpPipelines[kPipelineSmokeClearA].WriteIndirectBuffer(iCommandBuffer, 0);
		gpPipelineManager->mpPipelines[kPipelineSmokeClearB].WriteIndirectBuffer(iCommandBuffer, 0);
		WriteSmokeDilateDispatch(iCommandBuffer, iDilateGroups, 1);

		return;
	}

	gpPipelineManager->mpPipelines[kPipelineSmokeClearA].WriteIndirectBuffer(iCommandBuffer, 0);
	gpPipelineManager->mpPipelines[kPipelineSmokeClearB].WriteIndirectBuffer(iCommandBuffer, 0);

	// Refresh on schedule, or early once the camera leaves the held area so no unsimulated edge comes into view.
	if (siSmokeRenderFrame % iSmokeUpdateCadence != 0 && IsVisibleAreaInsideHeldSmokeArea(rf4Visible, sf4PreviousSmokeArea))
	{
		// Skip: wind's remap reduces to the identity over the held area, and the next refresh remaps held -> current.
		rGlobalLayout.f4SmokeArea = sf4PreviousSmokeArea;
		rGlobalLayout.f4PreviousSmokeArea = sf4PreviousSmokeArea;
		PopulatePreviousSmokeAreaSizeInverse(rGlobalLayout, sf4PreviousSmokeArea);
		PublishSmokeContinuity(sf4PreviousSmokeArea, sf4PreviousSmokeArea);
		PublishSmokeStep(rGlobalLayout, 1);
		WriteSmokeDilateDispatch(iCommandBuffer, iDilateGroups, 0);

		return;
	}

	rGlobalLayout.f4SmokeArea = f4CurrentSmokeArea;
	rGlobalLayout.f4PreviousSmokeArea = sf4PreviousSmokeArea;
	PopulatePreviousSmokeAreaSizeInverse(rGlobalLayout, sf4PreviousSmokeArea);
	PublishSmokeContinuity(f4CurrentSmokeArea, sf4PreviousSmokeArea);
	sf4PreviousSmokeArea = f4CurrentSmokeArea;
	PublishSmokeStep(rGlobalLayout, siSmokeFramesSinceRefresh);
	siSmokeFramesSinceRefresh = 0;
	WriteSmokeDilateDispatch(iCommandBuffer, iDilateGroups, 1);
}

} // namespace engine

#endif // defined(BT_CLIENT)
