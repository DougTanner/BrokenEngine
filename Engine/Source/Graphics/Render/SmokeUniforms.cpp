#if defined(BT_CLIENT)

#include "Graphics/CameraBase.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/SmokeWrappersBase.h"
#include "Render.h"

namespace engine
{

// Reciprocal of the plane's previous smoke area's signed extents (Smoke/Wind OccupancyDilate remap divisor). Preserves
// the shader's component order exactly: X = 1/(z-x) (width), Y = 1/(w-y) (negative — max-Y is .y, min-Y is .w).
static void PopulatePreviousSmokeAreaSizeInverse(shaders::GlobalLayout& rGlobalLayout, int64_t iPlane, const XMFLOAT4& rf4Area)
{
	rGlobalLayout.pf2PreviousSmokeAreaSizeInverses[iPlane].x = 1.0f / (rf4Area.z - rf4Area.x);
	rGlobalLayout.pf2PreviousSmokeAreaSizeInverses[iPlane].y = 1.0f / (rf4Area.w - rf4Area.y);
}

// Smoke has four publishing exits, each publishing an area pair per plane; report the pair plane 0 actually published,
// the pair wind reads.
static void PublishSmokeContinuity(int64_t iPlane, const XMFLOAT4& rf4CurrentArea, const XMFLOAT4& rf4PreviousArea)
{
	if (iPlane == 0)
	{
		gPresentationContinuity.smoke.f4CurrentArea = rf4CurrentArea;
		gPresentationContinuity.smoke.f4PreviousArea = rf4PreviousArea;
	}
}

// One refresh advances the plane's smoke by every frame since its previous refresh. The decay is an integer power by
// repeated multiplication so a single step publishes gSmokeDecay exactly.
static void PublishSmokeStep(shaders::GlobalLayout& rGlobalLayout, int64_t iPlane, int64_t iStepCount)
{
	float fDecay = 1.0f;
	for (int64_t i = 0; i < iStepCount; ++i)
	{
		fDecay *= gSmokeDecay.mfCurrent;
	}
	rGlobalLayout.pfSmokeDecays[iPlane] = fDecay;
	rGlobalLayout.pfSmokeStepCounts[iPlane] = static_cast<float>(iStepCount);
}

// A zero Z group count skips both of the plane's dilates and leaves the active-tile list at its reset, so neither of
// the plane's spreads runs.
static void WriteSmokeDilateDispatch(int64_t iCommandBuffer, int64_t iPlane, int64_t iDilateGroups, int64_t iGroupCountZ)
{
	gpPipelineManager->SmokePipeline(iPlane, kSmokeOccupancyDilate).WriteIndirectComputeBuffer(iCommandBuffer, iDilateGroups, 1, iGroupCountZ);
	gpPipelineManager->SmokePipeline(iPlane, kSmokeOccupancyDilateRemap).WriteIndirectComputeBuffer(iCommandBuffer, iDilateGroups, 1, iGroupCountZ);
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
	rGlobalLayout.fSmokeEdgeDecayDistanceInverse = 1.0f / gSmokeEdgeDecayDistance.mfCurrent;

	// Every plane's smoke textures share plane 0's extent, so its tile counts serve every plane.
	int64_t iTextureOneWidth = gpTextureManager->mRenderTargetTextures.mSmokeTexturesOne[0].mInfo.vkExtent3D.width;
	int64_t iTextureOneHeight = gpTextureManager->mRenderTargetTextures.mSmokeTexturesOne[0].mInfo.vkExtent3D.height;
	int64_t iSmokeTilesX = TileCount(iTextureOneWidth);
	int64_t iSmokeTilesY = TileCount(iTextureOneHeight);
	rGlobalLayout.uiSmokeTilesX = static_cast<uint32_t>(iSmokeTilesX);
	rGlobalLayout.uiSmokeTilesY = static_cast<uint32_t>(iSmokeTilesY);
	int64_t iDilateGroups = (iSmokeTilesX * iSmokeTilesY + shaders::kiOccupancyDilateGroupSize - 1) / shaders::kiOccupancyDilateGroupSize;

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
	// publishes, for each plane, this area or that plane's held area, an earlier current area that ShiftArea moved
	// without resizing. All share this construction's signs and framebuffer aspect, and the area size cancels in the
	// quotient, so one value serves every plane and exit. After a framebuffer resize a plane's held area, published by
	// the skip and disabled exits and read as previous by that plane's next refresh, keeps the older aspect until a
	// clear or that plane's refresh replaces it.
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

	int64_t iPlaneCount = gpFlightPlanes->Count();

	// Persisted across frames so the next frame's spread starts with previous == current
	// after a clear/disabled span — avoids divide-by-zero in WorldToSmokeTexcoord(zero, ...)
	static bool sbPreviousAreaInitialized = false;
	static XMFLOAT4 spf4PreviousSmokeAreas[shaders::kiMaxFlightPlanes] {};
	if (!sbPreviousAreaInitialized)
	{
		for (int64_t i = 0; i < iPlaneCount; ++i)
		{
			spf4PreviousSmokeAreas[i] = f4CurrentSmokeArea;
		}
		sbPreviousAreaInitialized = true;
	}

	// The retained areas — plane 0's of which wind also consumes through pf4PreviousSmokeAreas[0] — are in the camera
	// cell's frame, so follow a camera cell change before they are published beside this frame's area. A one-cell step
	// keeps the smoke and wind textures usable; a larger step leaves no overlap, so it takes the existing clear below.
	static RetainedAreaBasis sRetainedAreaBasis {};
	if (std::optional<XMFLOAT2> optionalShift = sRetainedAreaBasis.Advance(engine::gpCamera->mBasisCoordinate))
	{
		for (int64_t i = 0; i < iPlaneCount; ++i)
		{
			ShiftArea(spf4PreviousSmokeAreas[i], *optionalShift);
		}
	}
	else
	{
		gbSmokeClear = true;
		++gPresentationContinuity.smoke.iHistoryResets;
	}

	// A plane's refresh runs its dilates and spreads once, advancing its smoke by every frame since its previous
	// refresh. Between refreshes only deposits change that plane's smoke texture and its occupancy, and its held area
	// (spf4PreviousSmokeAreas) stays as its last refresh left it.
	static int64_t spiSmokeFramesSinceRefresh[shaders::kiMaxFlightPlanes] {};
	for (int64_t i = 0; i < iPlaneCount; ++i)
	{
		++spiSmokeFramesSinceRefresh[i];
	}

	if (gbSmokeClear)
	{
		gbSmokeClear = false;

		for (int64_t i = 0; i < iPlaneCount; ++i)
		{
			rGlobalLayout.pf4SmokeAreas[i] = f4CurrentSmokeArea;
			rGlobalLayout.pf4PreviousSmokeAreas[i] = f4CurrentSmokeArea;
			PopulatePreviousSmokeAreaSizeInverse(rGlobalLayout, i, f4CurrentSmokeArea);
			spf4PreviousSmokeAreas[i] = f4CurrentSmokeArea;
			PublishSmokeContinuity(i, f4CurrentSmokeArea, f4CurrentSmokeArea);
			PublishSmokeStep(rGlobalLayout, i, 1);
			spiSmokeFramesSinceRefresh[i] = 0;
			WriteSmokeDilateDispatch(iCommandBuffer, i, iDilateGroups, 1);
		}

		gpPipelineManager->mpPipelines[kPipelineSmokeClearA].WriteIndirectBuffer(iCommandBuffer, 1);
		gpPipelineManager->mpPipelines[kPipelineSmokeClearB].WriteIndirectBuffer(iCommandBuffer, 1);

		return;
	}

	if (!gSmokeEnabled.Get<bool>())
	{
		for (int64_t i = 0; i < iPlaneCount; ++i)
		{
			rGlobalLayout.pf4SmokeAreas[i] = spf4PreviousSmokeAreas[i];
			rGlobalLayout.pf4PreviousSmokeAreas[i] = spf4PreviousSmokeAreas[i];
			PopulatePreviousSmokeAreaSizeInverse(rGlobalLayout, i, spf4PreviousSmokeAreas[i]);
			PublishSmokeContinuity(i, spf4PreviousSmokeAreas[i], spf4PreviousSmokeAreas[i]);
			PublishSmokeStep(rGlobalLayout, i, 1);
			spiSmokeFramesSinceRefresh[i] = 0;
			WriteSmokeDilateDispatch(iCommandBuffer, i, iDilateGroups, 1);
		}

		gpPipelineManager->mpPipelines[kPipelineSmokeClearA].WriteIndirectBuffer(iCommandBuffer, 0);
		gpPipelineManager->mpPipelines[kPipelineSmokeClearB].WriteIndirectBuffer(iCommandBuffer, 0);

		return;
	}

	gpPipelineManager->mpPipelines[kPipelineSmokeClearA].WriteIndirectBuffer(iCommandBuffer, 0);
	gpPipelineManager->mpPipelines[kPipelineSmokeClearB].WriteIndirectBuffer(iCommandBuffer, 0);

	// With the stagger on, one plane per frame refreshes on schedule; with it off, every plane refreshes every frame. A
	// plane also refreshes early once the camera leaves its held area so no unsimulated edge comes into view.
	bool bSmokeLayerStagger = gSmokeLayerStagger.Get<bool>();
	int64_t iScheduledPlane = gpGraphics->miFrameCounter % iPlaneCount;
	for (int64_t i = 0; i < iPlaneCount; ++i)
	{
		if (bSmokeLayerStagger && i != iScheduledPlane && IsVisibleAreaInsideHeldArea(rf4Visible, spf4PreviousSmokeAreas[i]))
		{
			// Skip: the plane's remap reduces to the identity over its held area (for plane 0, wind's too), and its
			// next refresh remaps held -> current.
			rGlobalLayout.pf4SmokeAreas[i] = spf4PreviousSmokeAreas[i];
			rGlobalLayout.pf4PreviousSmokeAreas[i] = spf4PreviousSmokeAreas[i];
			PopulatePreviousSmokeAreaSizeInverse(rGlobalLayout, i, spf4PreviousSmokeAreas[i]);
			PublishSmokeContinuity(i, spf4PreviousSmokeAreas[i], spf4PreviousSmokeAreas[i]);
			PublishSmokeStep(rGlobalLayout, i, 1);
			WriteSmokeDilateDispatch(iCommandBuffer, i, iDilateGroups, 0);

			continue;
		}

		rGlobalLayout.pf4SmokeAreas[i] = f4CurrentSmokeArea;
		rGlobalLayout.pf4PreviousSmokeAreas[i] = spf4PreviousSmokeAreas[i];
		PopulatePreviousSmokeAreaSizeInverse(rGlobalLayout, i, spf4PreviousSmokeAreas[i]);
		PublishSmokeContinuity(i, f4CurrentSmokeArea, spf4PreviousSmokeAreas[i]);
		spf4PreviousSmokeAreas[i] = f4CurrentSmokeArea;
		PublishSmokeStep(rGlobalLayout, i, spiSmokeFramesSinceRefresh[i]);
		spiSmokeFramesSinceRefresh[i] = 0;
		WriteSmokeDilateDispatch(iCommandBuffer, i, iDilateGroups, 1);
	}
}

} // namespace engine

#endif // defined(BT_CLIENT)
