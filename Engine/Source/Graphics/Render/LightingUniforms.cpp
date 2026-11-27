#if defined(BT_CLIENT)

#include "Graphics/CameraBase.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/HeightLerpWrapperQuartet.h"
#include "Ui/LightingWrappersBase.h"
#include "Ui/MiscWrappersBase.h"
#include "Ui/PbrWrappersBase.h"
#include "Ui/ShadowWrappersBase.h"
#include "Ui/WaterWrappersBase.h"
#include "Render.h"

#include "Graphics/Camera.h"

namespace engine
{

static void PopulateLightingParameters(shaders::GlobalLayout& rGlobalLayout, bool bLightingEnabled)
{
	// Lighting mirrors the shadow area's raw-frustum-safe framing reference. RenderTargetTextures sizes deposit/spread/combine through
	// ExtraAreaDetailTextureSize with gExtraArea; textureWidth / gExtraArea pixels span the framing's reference-angle frustum at
	// mfLightingTexelFraming. The reference expands immediately outward, contracts at the existing rate inward, and never falls below
	// live framing times the coverage ratio. Actual clamped extents preserve device-independent coverage. For a settled footprint the grid
	// stays fixed; snap f4LightingArea, centered on the tilted footprint, to integer deposit texels because deposit rasterizes lights.
	// Spread/combine/temporal resample that world rectangle at their own resolutions.
	float fLightingTextureWidth = static_cast<float>(gpTextureManager->mRenderTargetTextures.mpLightingTextures[0].mInfo.vkExtent3D.width);
	float fLightingTextureHeight = static_cast<float>(gpTextureManager->mRenderTargetTextures.mpLightingTextures[0].mInfo.vkExtent3D.height);
	const XMFLOAT4& rf4VisibleArea = engine::gpCamera->mf4RenderVisibleArea;

	XMVECTOR vecFootprintCenter = XMVectorAdd(engine::gpCamera->mVecPosition, XMVectorSet(0.0f, engine::gpCamera->mfTexelCenterOffsetNorth, 0.0f, 0.0f));
	WorldSizedTexelArea area = ComputeWorldSizedTexelArea(gExtraArea.mfCurrent, engine::gpCamera->mfLightingTexelFraming, fLightingTextureWidth, fLightingTextureHeight, gpSwapchainManager->mfAspectRatio, vecFootprintCenter);

	// Temporal accumulation publishes the current and previous world areas from the lighting refresh epoch, so its
	// temporal pass reprojects only from its previous refresh; a skip retains both.
	static TemporalAreaLatch sTemporalAreaLatch {};
	// Every rectangle retained across frames here is in the camera cell's frame; follow a camera cell change before
	// the refresh test compares them with this frame's areas.
	static RetainedAreaBasis sRetainedAreaBasis {};
	XMFLOAT2 f2Shift = sRetainedAreaBasis.Advance(engine::gpCamera->mBasisCoordinate);
	ShiftArea(sTemporalAreaLatch.f4CurrentArea, f2Shift);
	ShiftArea(sTemporalAreaLatch.f4PreviousArea, f2Shift);

	if (bLightingEnabled)
	{
		// Scheduled refresh frames are the cadence's multiples, which the terrain shadow phase avoids when the cadences
		// share a factor.
		int64_t iLightingUpdateCadence = gLightingUpdateCadence.Get<int64_t>();
		bool bScheduledFrame = gpGraphics->miFrameCounter % iLightingUpdateCadence == 0;
		bool bRefresh = gbLightingTemporalReset || bScheduledFrame
		             || IsVisibleCenterOutsideArea(rf4VisibleArea, sTemporalAreaLatch.f4CurrentArea);
		gbLightingRefreshFrame = bRefresh;

		if (bRefresh)
		{
			rGlobalLayout.fLightingTemporalBlend = sTemporalAreaLatch.Update(area.f4Area, gbLightingTemporalReset, gLightingTemporalBlend.mfCurrent);
		}
		else
		{
			rGlobalLayout.fLightingTemporalBlend = sTemporalAreaLatch.fBlend;
		}
		rGlobalLayout.f4LightingArea = sTemporalAreaLatch.f4CurrentArea;
		PopulateAreaToUv(rGlobalLayout.lightingAreaToUv, sTemporalAreaLatch.f4CurrentArea);
		PopulateAreaToUv(rGlobalLayout.lightingAreaPreviousToUv, sTemporalAreaLatch.f4PreviousArea);
		gPresentationContinuity.lighting.f4CurrentArea = sTemporalAreaLatch.f4CurrentArea;
		gPresentationContinuity.lighting.f4PreviousArea = sTemporalAreaLatch.f4PreviousArea;
	}
	else
	{
		// Leave gbLightingTemporalReset unconsumed so re-enable refreshes from current data; publish area.f4Area
		// directly so UVs stay finite across cold boot and distant camera-cell jumps.
		gbLightingRefreshFrame = false;
		rGlobalLayout.f4LightingArea = area.f4Area;
		rGlobalLayout.fLightingTemporalBlend = 1.0f;
		PopulateAreaToUv(rGlobalLayout.lightingAreaToUv, area.f4Area);
		PopulateAreaToUv(rGlobalLayout.lightingAreaPreviousToUv, area.f4Area);
		gPresentationContinuity.lighting.f4CurrentArea = area.f4Area;
		gPresentationContinuity.lighting.f4PreviousArea = area.f4Area;
	}

	// Edge-fade denominator reciprocal (LightingDepositEdgeFade), deliberately floored unlike smoke/wind's ceil-based
	// full-coverage dispatch grids. The minimum of one keeps the tile count nonzero if a device clamp produces a
	// sub-tile texture extent.
	int64_t iLightTilesX = std::max(1i64, static_cast<int64_t>(fLightingTextureWidth) / shaders::kiComputeTileSize);
	int64_t iLightTilesY = std::max(1i64, static_cast<int64_t>(fLightingTextureHeight) / shaders::kiComputeTileSize);
	rGlobalLayout.f2LightingDepositSizeInverse.x = 1.0f / static_cast<float>(iLightTilesX * shaders::kiComputeTileSize);
	rGlobalLayout.f2LightingDepositSizeInverse.y = 1.0f / static_cast<float>(iLightTilesY * shaders::kiComputeTileSize);
}

void RenderLightingGlobal(int64_t iCommandBuffer)
{
	shaders::GlobalLayout& rGlobalLayout = *reinterpret_cast<shaders::GlobalLayout*>(&gpBufferManager->mGlobalLayoutUniformBuffers.at(iCommandBuffer).mpMappedMemory[0]);

	bool bLightingEnabled = gLightingEnabled.Get<bool>();

	rGlobalLayout.fLightingObjectsAdd = gLightingObjectsAdd.mfCurrent;
	rGlobalLayout.fLightingDepositCompress = gLightingDepositCompress.mfCurrent;

	float fCombineMaxBrightness = gCombineMaxBrightness.mfCurrent;
	float fCombineContrast = gCombineContrast.mfCurrent;
	float fCombineLinearStart = fCombineMaxBrightness * gCombineLinearStart.mfCurrent;
	float fCombineLinearLength = gCombineLinearLength.mfCurrent;
	float fCombineBlackTightness = fCombineLinearStart * gCombineBlackTightness.mfCurrent;
	rGlobalLayout.fCombineMaxBrightness = fCombineMaxBrightness;
	rGlobalLayout.fCombineContrast = fCombineContrast;
	rGlobalLayout.fCombineLinearStart = fCombineLinearStart;
	rGlobalLayout.fCombineToe = gCombineToe.mfCurrent;
	rGlobalLayout.fCombineBlackTightness = fCombineBlackTightness;
	rGlobalLayout.fCombineHuePreserve = gCombineHuePreserve.mfCurrent;

	// LightCombine.comp and DebugTexture.frag share the CPU-computed Uchimura segment constants S0/S1/CP.
	// The bounds documented at gCombineLinearStart keep P - S1 at or above its stated minimum across the slider domain.
	float fCombineLinearOutputSpan = (fCombineMaxBrightness - fCombineLinearStart) * fCombineLinearLength;
	float fCombineShoulderStart = fCombineLinearStart + fCombineLinearOutputSpan;
	rGlobalLayout.fCombineShoulderInputStart = fCombineLinearStart + fCombineLinearOutputSpan / fCombineContrast;
	rGlobalLayout.fCombineShoulderOutputStart = fCombineShoulderStart;
	rGlobalLayout.fCombineShoulderExponentCoefficient = -fCombineContrast / (fCombineMaxBrightness - fCombineShoulderStart);

	float fPassCount = gSpreadPassCount.mfCurrent;
	float fPassNormalization = std::lerp(1.0f, 1.0f / fPassCount, gCombinePassNormalize.mfCurrent);
	float fPassScale = std::pow(fPassCount, -gCombineExposurePassScale.mfCurrent);
	float fCombinePassNormalizationScale = fPassNormalization * fPassScale;
	rGlobalLayout.fCombinePassNormalizationScale = fCombinePassNormalizationScale;
	rGlobalLayout.fCombinePassTotalScale = fCombinePassNormalizationScale / fPassCount;
	float fSpreadDirectionalityStart = gSpreadDirectionality.mfCurrent;
	float fSpreadDirectionalityEnd = gSpreadDirectionalityEnd.mfCurrent;
	float fSpreadDirectionCountStart = gSpreadDirectionCount.mfCurrent;
	float fSpreadDirectionCountEnd = gSpreadDirectionCountEnd.mfCurrent;
	float fSpreadRingCountStart = gSpreadRingCount.mfCurrent;
	float fSpreadRingCountEnd = gSpreadRingCountEnd.mfCurrent;
	float fSpreadDistanceFalloffStart = gSpreadDistanceFalloff.mfCurrent;
	float fSpreadDistanceFalloffEnd = gSpreadDistanceFalloffEnd.mfCurrent;

	int64_t iActivePassCount = gSpreadPassCount.Get<int64_t>();
	float fPassFractionDenominator = static_cast<float>(std::max(iActivePassCount - 1, 1i64));
	const CurveData& rCombineCurve = gbUseCombineCurveNew ? gCombineCurveNew : gCombineCurveOld;
	for (int64_t i = 0; i < iActivePassCount; ++i)
	{
		float fPassFraction = static_cast<float>(i) / fPassFractionDenominator;
		rGlobalLayout.pfCombineCurvePoints[i] = rCombineCurve.Evaluate(fPassFraction);

		float fDirectionality = std::lerp(fSpreadDirectionalityStart, fSpreadDirectionalityEnd, fPassFraction);
		float fDistanceFalloff = std::lerp(fSpreadDistanceFalloffStart, fSpreadDistanceFalloffEnd, fPassFraction);
		uint32_t uiRingCount = static_cast<uint32_t>(std::lerp(fSpreadRingCountStart, fSpreadRingCountEnd, fPassFraction));
		uint32_t uiDirectionCountBase = static_cast<uint32_t>(std::lerp(fSpreadDirectionCountStart, fSpreadDirectionCountEnd, fPassFraction));
		float fRingCount = static_cast<float>(uiRingCount);

		float fWeightSumEastWest = 0.0f;
		float fWeightSumNorthSouth = 0.0f;
		for (uint32_t j = 0; j < uiRingCount; ++j)
		{
			uint32_t uiDirectionCount = uiDirectionCountBase + j;
			float fDirectionCount = static_cast<float>(uiDirectionCount);
			float fRingFalloff = 1.0f - fDistanceFalloff * (static_cast<float>(j + 1) / fRingCount);

			// Closed-form sums over ideal evenly spaced directions: max(+/-cos(2*pi*k/N), 0) (EW) and
			// max(+/-sin(2*pi*k/N), 0) (NS) for k in [0, N-1].
			float fParityScale = (uiDirectionCount % 2ui32 == 0ui32) ? 1.0f : 0.5f;
			float fHalfAngle = fParityScale * XM_PI / fDirectionCount;
			float fPeakSum = fParityScale / std::sin(fHalfAngle);
			float fDirectionSumNorthSouth = fPeakSum * std::cos(fHalfAngle);
			float fDirectionSumEastWest = (uiDirectionCount % 4ui32 == 0ui32) ? fDirectionSumNorthSouth : fPeakSum;

			fWeightSumEastWest += std::lerp(fDirectionCount, fDirectionSumEastWest, fDirectionality) * fRingFalloff;
			fWeightSumNorthSouth += std::lerp(fDirectionCount, fDirectionSumNorthSouth, fDirectionality) * fRingFalloff;
		}

		shaders::SpreadPassNormalizationLayout& rPassNormalization = rGlobalLayout.pSpreadPassNormalizations[i];
		rPassNormalization.f2WeightSumInverse.x = 1.0f / fWeightSumEastWest;
		rPassNormalization.f2WeightSumInverse.y = 1.0f / fWeightSumNorthSouth;
		rPassNormalization.uiRingCount = uiRingCount;
		rPassNormalization.uiDirectionCountBase = uiDirectionCountBase;
	}
	rGlobalLayout.fLightingTerrain = gLightingTerrain.mfCurrent;
	rGlobalLayout.fLightingObjects = gLightingObjects.mfCurrent;
	rGlobalLayout.fLightingAddTerrain = gLightingAddTerrain.mfCurrent;

	rGlobalLayout.fSpreadDirectionalityStart = fSpreadDirectionalityStart;
	rGlobalLayout.fSpreadDistanceStart = gSpreadDistance.mfCurrent;
	rGlobalLayout.fSpreadDecayStart = gSpreadDecay.mfCurrent;
	rGlobalLayout.fSpreadAccumulationDecayStart = gSpreadAccumulationDecay.mfCurrent;
	rGlobalLayout.fSpreadDistanceFalloffStart = fSpreadDistanceFalloffStart;
	rGlobalLayout.fSpreadOutputCompressStart = gSpreadOutputCompress.mfCurrent;
	rGlobalLayout.fSpreadPassCount = gSpreadPassCount.mfCurrent;

	// Spread End (interpolation targets for last spread pass)
	rGlobalLayout.fSpreadDirectionalityEnd = fSpreadDirectionalityEnd;
	rGlobalLayout.fSpreadDistanceEnd = engine::LerpAtHeight(engine::gpCamera->RenderFraming(), gSpreadDistanceEnd);
	rGlobalLayout.fSpreadDecayEnd = gSpreadDecayEnd.mfCurrent;
	rGlobalLayout.fSpreadAccumulationDecayEnd = gSpreadAccumulationDecayEnd.mfCurrent;
	rGlobalLayout.fSpreadDistanceFalloffEnd = fSpreadDistanceFalloffEnd;
	rGlobalLayout.fSpreadOutputCompressEnd = gSpreadOutputCompressEnd.mfCurrent;

	rGlobalLayout.fSpreadHeightMultiplier = gSpreadHeightMultiplier.mfCurrent;
	rGlobalLayout.fSpreadHeightEndHeightInverse = 1.0f / std::max(gSpreadHeightEndHeight.mfCurrent, 0.001f);
	rGlobalLayout.fSpreadHeightPower = gSpreadHeightPower.mfCurrent;

	PopulateLightingParameters(rGlobalLayout, bLightingEnabled);
}

void RenderLightingMain(int64_t iCommandBuffer)
{
	shaders::MainLayout& rMainLayout = *reinterpret_cast<shaders::MainLayout*>(&gpBufferManager->mMainLayoutUniformBuffers.at(iCommandBuffer).mpMappedMemory[0]);

	float fWaterScale = gWaterScale.mfCurrent;
	rMainLayout.fLightingSampledNormalsOneSize = WaterNormalSize(gLightingSampledNormalsOneSize.mfCurrent);
	rMainLayout.fLightingSampledNormalsTwoSize = WaterNormalSize(gLightingSampledNormalsTwoSize.mfCurrent);
	rMainLayout.fLightingSampledNormalsThreeSize = WaterNormalSize(gLightingSampledNormalsThreeSize.mfCurrent);
	rMainLayout.uiWaterNormalIndexOne = static_cast<uint32_t>(gWaterNormalIndexOne.Get<int64_t>());
	rMainLayout.uiWaterNormalIndexTwo = static_cast<uint32_t>(gWaterNormalIndexTwo.Get<int64_t>());
	rMainLayout.uiWaterNormalIndexThree = static_cast<uint32_t>(gWaterNormalIndexThree.Get<int64_t>());
	// Normal weights use a fixed fade band from the default framing to twice that framing, with no author controls.
	// engine::CameraBase owns the fade endpoint; LerpAtHeight resolves the weights before upload.
	float fWaterNormalWeightOne = engine::LerpAtHeight(engine::gpCamera->RenderFraming(), engine::CameraBase::kfCameraFramingDefault, engine::CameraBase::kfWaveFadeEndFraming, gLightingSampledNormalsWeightOneMinimum.mfCurrent, gLightingSampledNormalsWeightOneMaximum.mfCurrent);
	float fWaterNormalWeightTwo = engine::LerpAtHeight(engine::gpCamera->RenderFraming(), engine::CameraBase::kfCameraFramingDefault, engine::CameraBase::kfWaveFadeEndFraming, gLightingSampledNormalsWeightTwoMinimum.mfCurrent, gLightingSampledNormalsWeightTwoMaximum.mfCurrent);
	float fWaterNormalWeightThree = engine::LerpAtHeight(engine::gpCamera->RenderFraming(), engine::CameraBase::kfCameraFramingDefault, engine::CameraBase::kfWaveFadeEndFraming, gLightingSampledNormalsWeightThreeMinimum.mfCurrent, gLightingSampledNormalsWeightThreeMaximum.mfCurrent);
	rMainLayout.fWaterNormalWeightOne = fWaterNormalWeightOne;
	rMainLayout.fWaterNormalWeightTwo = fWaterNormalWeightTwo;
	rMainLayout.fWaterNormalWeightThree = fWaterNormalWeightThree;
	// Water.frag height darkening keeps the bottom and uploads only the range reciprocal; the two tunables are independent and their ranges overlap, so
	// floor the range magnitude at kfEpsilon to keep the reciprocal finite, keeping the sign so a top below the bottom still reads as an inverted range.
	float fWaterHeightDarkenTop = gWaterHeightDarkenTop.mfCurrent * fWaterScale;
	float fWaterHeightDarkenBottom = gWaterHeightDarkenBottom.mfCurrent * fWaterScale;
	rMainLayout.fWaterHeightDarkenBottom = fWaterHeightDarkenBottom;
	float fWaterHeightDarkenRange = fWaterHeightDarkenTop - fWaterHeightDarkenBottom;
	float fRangeMagnitude = std::max(std::abs(fWaterHeightDarkenRange), shaders::kfEpsilon);
	fWaterHeightDarkenRange = fWaterHeightDarkenRange < 0.0f ? -fRangeMagnitude : fRangeMagnitude;
	rMainLayout.fWaterHeightDarkenRangeInverse = 1.0f / fWaterHeightDarkenRange;
	rMainLayout.fWaterHeightDarkenTarget = gWaterHeightDarkenTarget.mfCurrent;
	rMainLayout.fWaterHeightDarkenLighting = gWaterHeightDarkenLighting.mfCurrent;

	// GlobalUniforms folds fLightingWaterSkyboxSunBias into globalLayout.f4WaterBiasedSunNormal.
	rMainLayout.fLightingWaterSkyboxNormalBlendWave = gLightingWaterSkyboxNormalBlendWave.mfCurrent;
	rMainLayout.fLightingWaterSkyboxIntensity = gLightingWaterSkyboxIntensity.mfCurrent;
	rMainLayout.fLightingWaterSkyboxAdd = gLightingWaterSkyboxAdd.mfCurrent;
	float fMoonAmount = MoonAmount(static_cast<game::Camera&>(*engine::gpCamera).SunAngle());
	float fSkyboxPowerOne = gLightingWaterSkyboxOnePower.mfCurrent;
	float fSkyboxPowerTwo = gLightingWaterSkyboxTwoPower.mfCurrent;
	float fSkyboxPowerThree = gLightingWaterSkyboxThreePower.mfCurrent;
	rMainLayout.fLightingWaterSkyboxOnePower = fSkyboxPowerOne;
	float fSampleRadius = gWaterSkyboxOneSampleRadius.mfCurrent;
	int32_t iSampleCount = fSampleRadius > 0.0f ? gWaterSkyboxOneSampleCount.Get<int32_t>() : 1;
	rMainLayout.iLightingWaterSkyboxOneSampleCount = iSampleCount;
	int64_t iRingCount = iSampleCount - 1i64;
	for (int64_t i = 0; i < iRingCount; ++i)
	{
		float fAngle = (XM_2PI / static_cast<float>(iRingCount)) * static_cast<float>(i);
		rMainLayout.pf2LightingWaterSkyboxOneSampleOffsets[i].x = fSampleRadius * std::sin(fAngle);
		rMainLayout.pf2LightingWaterSkyboxOneSampleOffsets[i].y = -fSampleRadius * std::cos(fAngle);
	}
	rMainLayout.fLightingWaterSkyboxOneSampleCountInverse = 1.0f / static_cast<float>(iSampleCount);
	rMainLayout.fLightingWaterSkyboxOneSampleBlend = gWaterSkyboxOneSampleBlend.mfCurrent;
	rMainLayout.fLightingWaterSkyboxTwo = std::lerp(gLightingWaterSkyboxTwo.mfCurrent, gLightingWaterSkyboxTwoNight.mfCurrent, fMoonAmount);
	rMainLayout.fLightingWaterSkyboxTwoPower = fSkyboxPowerTwo;
	rMainLayout.fLightingWaterSkyboxThree = std::lerp(gLightingWaterSkyboxThree.mfCurrent, gLightingWaterSkyboxThreeNight.mfCurrent, fMoonAmount);
	rMainLayout.fLightingWaterSkyboxThreePower = fSkyboxPowerThree;
	rMainLayout.fLightingWaterSkyboxOneBeachReduction = gLightingWaterSkyboxOneBeachReduction.mfCurrent;
	rMainLayout.fLightingWaterSkyboxTwoBeachReduction = gLightingWaterSkyboxTwoBeachReduction.mfCurrent;
	rMainLayout.fLightingWaterSkyboxThreeBeachReduction = gLightingWaterSkyboxThreeBeachReduction.mfCurrent;
	rMainLayout.fLightingWaterSkyboxLevelOfDetail = gLightingWaterSkyboxLevelOfDetail.mfCurrent;

	rMainLayout.fLightingWaterReflectedAmount = gLightingWaterReflectedAmount.mfCurrent;
	rMainLayout.fLightingWaterReflectedNormalBlendWave = gLightingWaterReflectedNormalBlendWave.mfCurrent;
	rMainLayout.fLightingWaterReflectedDistortion = gLightingWaterReflectedDistortion.mfCurrent;
	rMainLayout.fLightingWaterReflectedFalloffStart = gLightingWaterReflectedFalloffStart.mfCurrent;
	rMainLayout.fLightingWaterReflectedFalloffPower = gLightingWaterReflectedFalloffPower.mfCurrent;
	rMainLayout.fLightingWaterReflectedFresnel = gLightingWaterReflectedFresnel.mfCurrent;
	rMainLayout.fLightingWaterReflectedIntensity = gLightingWaterReflectedIntensity.mfCurrent;

	rMainLayout.fLightingWaterNormalSoften = gLightingWaterNormalSoften.mfCurrent;
	rMainLayout.fLightingWaterNormalBlendWave = gLightingWaterNormalBlendWave.mfCurrent;
	rMainLayout.fLightingWaterIntensity = gLightingWaterIntensity.mfCurrent;
	rMainLayout.fLightingWaterAdd = gLightingWaterAdd.mfCurrent;
	rMainLayout.fLightingWaterOne = gLightingWaterOne.mfCurrent;
	rMainLayout.fLightingWaterOnePower = gLightingWaterOnePower.mfCurrent;
	rMainLayout.fLightingWaterTwo = gLightingWaterTwo.mfCurrent;
	rMainLayout.fLightingWaterTwoPower = gLightingWaterTwoPower.mfCurrent;
	rMainLayout.fLightingWaterThree = gLightingWaterThree.mfCurrent;
	rMainLayout.fLightingWaterThreePower = gLightingWaterThreePower.mfCurrent;
	rMainLayout.fLightingWaterPowerMode = gLightingWaterPowerMode.mfCurrent;

	rMainLayout.fLightingDirectionalIntensity = gLightingDirectionalIntensity.mfCurrent;
	rMainLayout.fLightingDirectionalPower = gLightingDirectionalPower.mfCurrent;
	rMainLayout.fLightingDirectionalPowerMode = gLightingDirectionalPowerMode.mfCurrent;
	rMainLayout.fLightingAmbientIntensity = gLightingAmbientIntensity.mfCurrent;
	rMainLayout.fLightingAmbientPower = gLightingAmbientPower.mfCurrent;
	rMainLayout.fLightingAmbientPowerMode = gLightingAmbientPowerMode.mfCurrent;
	rMainLayout.fLightingWaterEastWestNorthSouthPower = gLightingWaterEastWestNorthSouthPower.mfCurrent;
	rMainLayout.fLightingWaterEastWestNorthSouthPowerMode = gLightingWaterEastWestNorthSouthPowerMode.mfCurrent;
	rMainLayout.fLightingWaterAmbientIntensity = gLightingWaterAmbientIntensity.mfCurrent;
	rMainLayout.fLightingWaterAmbientPower = gLightingWaterAmbientPower.mfCurrent;
	rMainLayout.fLightingWaterAmbientPowerMode = gLightingWaterAmbientPowerMode.mfCurrent;
	rMainLayout.fLightingTerrainBelowBaseMultiplier = gLightingTerrainBelowBaseMultiplier.mfCurrent;
	rMainLayout.fLightingTerrainBelowBasePower = gLightingTerrainBelowBasePower.mfCurrent;

	rMainLayout.fPbrExposure = gPbrExposure.mfCurrent;
	rMainLayout.fPbrGammaInverse = 1.0f / gPbrGamma.mfCurrent;
	rMainLayout.fColorGradingSaturation = gColorGradingSaturation.mfCurrent;
	rMainLayout.fColorGradingContrast = gColorGradingContrast.mfCurrent;
	rMainLayout.fColorGradingTemperature = gColorGradingTemperature.mfCurrent;
	rMainLayout.fPbrDayBrightness = gPbrDayBrightness.mfCurrent;
	rMainLayout.fPbrAmbient = gPbrIblAmbient.mfCurrent;

	rMainLayout.fPbrMipmapCount = static_cast<float>(gpTextureManager->mTextureMap.at(TextureManager::kPrefilteredCrc).mInfo.iMipLevels);
	rMainLayout.fPbrSmoke = gPbrSmoke.mfCurrent;

	rMainLayout.fPbrBrdfDiffuse = gPbrBrdfDiffuse.mfCurrent;
	rMainLayout.fPbrBrdfDiffusePower = gPbrBrdfDiffusePower.mfCurrent;
	rMainLayout.fPbrBrdfSpecular = gPbrBrdfSpecular.mfCurrent;
	rMainLayout.fPbrBrdfSpecularPower = gPbrBrdfSpecularPower.mfCurrent;
	rMainLayout.fPbrIblDiffuse = gPbrIblDiffuse.mfCurrent;
	rMainLayout.fPbrIblDiffusePower = gPbrIblDiffusePower.mfCurrent;
	rMainLayout.fPbrIblSpecular = gPbrIblSpecular.mfCurrent;
	rMainLayout.fPbrIblSpecularPower = gPbrIblSpecularPower.mfCurrent;
	rMainLayout.fPbrLighting = gPbrLighting.mfCurrent;
	rMainLayout.fPbrLightingPower = gPbrLightingPower.mfCurrent;
	rMainLayout.fPbrLightingSpecular = gPbrLightingSpecular.mfCurrent;
	rMainLayout.fPbrLightingSpecularPower = gPbrLightingSpecularPower.mfCurrent;
	rMainLayout.fPbrEmissive = gPbrEmissive.mfCurrent;
	rMainLayout.fPbrIblShadowBlend = gPbrIblShadowBlend.mfCurrent;
	rMainLayout.fPbrShadowFloor = gPbrShadowFloor.mfCurrent;
	rMainLayout.fPbrCubemapLodPower = gPbrCubemapLodPower.mfCurrent;
	rMainLayout.fPbrCubemapLodOffset = gPbrCubemapLodOffset.mfCurrent;

	rMainLayout.fSmokeShadowIntensity = gSmokeShadowIntensity.mfCurrent;
}

void RenderLightingSpreadIndirect(int64_t iCommandBuffer)
{
	int64_t iInstanceCount = gbLightingRefreshFrame ? 1 : 0;

	// Update all kiMaxSpreadPasses pipelines because the recorded Main command buffer selects the active passes.
	// CreateHostVisibleIndirectBuffer zeroes slots during pipeline recreation, so recorded passes need their instance counts restored.
	// Write only iCommandBuffer's framebuffer slot; the other slots may still be in use by the GPU.
	for (Pipeline& rPipeline : gpPipelineManager->mpSpreadPipelines)
	{
		rPipeline.WriteIndirectBuffer(iCommandBuffer, iInstanceCount);
	}

	// The combine-sized chain covers the whole texture; the refresh predicate suppresses the chain by zeroing its
	// Z group count without re-recording the Main CB.
	VkExtent3D vkCombineExtent = gpTextureManager->mRenderTargetTextures.mpCombineTextures[0].mInfo.vkExtent3D;
	int64_t iCombineGroupsX = TileCount(vkCombineExtent.width);
	int64_t iCombineGroupsY = TileCount(vkCombineExtent.height);
	int64_t iCombineGroupsZ = gbLightingRefreshFrame ? 1 : 0;
	gpPipelineManager->mCombinePipeline.WriteIndirectComputeBuffer(iCommandBuffer, iCombineGroupsX, iCombineGroupsY, iCombineGroupsZ);
	gpPipelineManager->mLightingTemporalPipeline.WriteIndirectComputeBuffer(iCommandBuffer, iCombineGroupsX, iCombineGroupsY, iCombineGroupsZ);
	gpPipelineManager->mLightingHistoryCopyPipeline.WriteIndirectComputeBuffer(iCommandBuffer, iCombineGroupsX, iCombineGroupsY, iCombineGroupsZ);
}

} // namespace engine

#endif // defined(BT_CLIENT)
