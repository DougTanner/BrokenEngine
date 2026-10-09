#if defined(BT_CLIENT)

#include "Graphics/CameraBase.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/HeightLerpWrapperQuartet.h"
#include "Ui/LightingWrappersBase.h"
#include "Ui/PbrWrappersBase.h"
#include "Ui/ShadowWrappersBase.h"
#include "Ui/WaterWrappersBase.h"
#include "Render.h"

namespace engine
{

static bool IsVisibleAreaInsideHeldCombineCrop(const XMFLOAT4& rf4VisibleArea, const XMFLOAT4& rf4HeldVisibleArea, const XMFLOAT4& rf4HeldLightingArea, float fCombineTextureWidth, float fCombineTextureHeight)
{
	float fHeldCombineTexelX = (rf4HeldLightingArea.z - rf4HeldLightingArea.x) / fCombineTextureWidth;
	float fHeldCombineTexelY = (rf4HeldLightingArea.y - rf4HeldLightingArea.w) / fCombineTextureHeight;
	return rf4VisibleArea.x >= rf4HeldVisibleArea.x - fHeldCombineTexelX && rf4VisibleArea.z <= rf4HeldVisibleArea.z + fHeldCombineTexelX
	    && rf4VisibleArea.y <= rf4HeldVisibleArea.y + fHeldCombineTexelY && rf4VisibleArea.w >= rf4HeldVisibleArea.w - fHeldCombineTexelY;
}

struct LightingTemporalAreaLatch
{
	bool bInitialized = false;
	XMFLOAT4 f4CurrentArea {};
	XMFLOAT4 f4PreviousArea {};
	float fBlend = 1.0f;

	float Update(const XMFLOAT4& rf4CurrentArea, bool& rbReset, float fRequestedBlend, XMFLOAT4& rf4PreviousArea)
	{
		if (rbReset)
		{
			rbReset = false;
			bInitialized = false;
		}

		float fResolvedBlend = fRequestedBlend;
		if (!bInitialized)
		{
			f4CurrentArea = rf4CurrentArea;
			f4PreviousArea = rf4CurrentArea;
			bInitialized = true;
			fResolvedBlend = 1.0f;
		}
		else
		{
			f4PreviousArea = f4CurrentArea;
			f4CurrentArea = rf4CurrentArea;
		}

		rf4PreviousArea = f4PreviousArea;
		fBlend = fResolvedBlend;
		return fBlend;
	}
};

static bool sbLightingRefreshFrame = true; // Cached before global lighting publication so the spread, combine, and temporal chain share one refresh epoch

static void PopulateLightingParameters(shaders::GlobalLayout& rGlobalLayout, bool bScheduledRefresh, bool bLightingEnabled)
{
	// Lighting mirrors the shadow area's raw-frustum-safe eye-height reference. RenderTargetTextures sizes deposit/spread/combine through
	// LightingDetailTextureSize with kfLightingHeadroomMultiplier; textureWidth / that multiplier pixels span the frustum at
	// mfLightingTexelEyeHeight. The reference expands immediately outward, contracts at the existing rate inward, and never falls below live
	// height. Actual clamped extents preserve device-independent coverage. At settled height the grid stays fixed; snap the camera-centered
	// f4LightingArea to integer deposit texels because deposit rasterizes lights. Spread/combine/temporal resample that world rectangle at
	// their own resolutions.
	float fLightingTextureWidth = static_cast<float>(gpTextureManager->mRenderTargetTextures.mpLightingTextures[0].mInfo.vkExtent3D.width);
	float fLightingTextureHeight = static_cast<float>(gpTextureManager->mRenderTargetTextures.mpLightingTextures[0].mInfo.vkExtent3D.height);
	float fCombineTextureWidth = static_cast<float>(gpTextureManager->mRenderTargetTextures.mpCombineTextures[0].mInfo.vkExtent3D.width);
	float fCombineTextureHeight = static_cast<float>(gpTextureManager->mRenderTargetTextures.mpCombineTextures[0].mInfo.vkExtent3D.height);
	const XMFLOAT4& rf4VisibleArea = engine::gpCamera->mf4RenderVisibleArea;

	WorldSizedTexelArea area = ComputeWorldSizedTexelArea(engine::CameraBase::kfLightingHeadroomMultiplier, engine::gpCamera->mfLightingTexelEyeHeight, fLightingTextureWidth, fLightingTextureHeight, gpSwapchainManager->mfAspectRatio, gFieldOfView.mfCurrent, engine::gpCamera->mVecPosition);

	// Temporal accumulation publishes the current and previous world areas from one refresh epoch; a skip retains both.
	static LightingTemporalAreaLatch sTemporalAreaLatch {};
	static XMFLOAT4 sf4HeldVisibleArea {};
	static bool sbHeldVisibleArea = false;
	// Every rectangle retained across frames here is in the camera cell's frame; follow a camera cell change before
	// the refresh test compares them with this frame's areas, and let a multi-cell jump take the existing reset.
	static RetainedAreaBasis sRetainedAreaBasis {};
	if (std::optional<XMFLOAT2> of2Shift = sRetainedAreaBasis.Advance(engine::gpCamera->mBasisCoordinate))
	{
		ShiftArea(sTemporalAreaLatch.f4CurrentArea, *of2Shift);
		ShiftArea(sTemporalAreaLatch.f4PreviousArea, *of2Shift);
		ShiftArea(sf4HeldVisibleArea, *of2Shift);
	}
	else
	{
		gbLightingTemporalReset = true;
		sbHeldVisibleArea = false;
		++gPresentationContinuity.lighting.iHistoryResets;
	}
	sbLightingRefreshFrame = bScheduledRefresh || !sbHeldVisibleArea
	                      || (bLightingEnabled && !IsVisibleAreaInsideHeldCombineCrop(rf4VisibleArea, sf4HeldVisibleArea, sTemporalAreaLatch.f4CurrentArea, fCombineTextureWidth, fCombineTextureHeight));
	if (sbLightingRefreshFrame)
	{
		rGlobalLayout.fLightingTemporalBlend = sTemporalAreaLatch.Update(area.f4Area, gbLightingTemporalReset, gLightingTemporalBlend.mfCurrent, rGlobalLayout.f4LightingAreaPrevious);
		rGlobalLayout.f4LightingArea = sTemporalAreaLatch.f4CurrentArea;
		sf4HeldVisibleArea = rf4VisibleArea;
		sbHeldVisibleArea = true;
	}
	else
	{
		rGlobalLayout.f4LightingArea = sTemporalAreaLatch.f4CurrentArea;
		rGlobalLayout.f4LightingAreaPrevious = sTemporalAreaLatch.f4PreviousArea;
		rGlobalLayout.fLightingTemporalBlend = sTemporalAreaLatch.fBlend;
	}
	gPresentationContinuity.lighting.f4CurrentArea = sTemporalAreaLatch.f4CurrentArea;
	gPresentationContinuity.lighting.f4PreviousArea = sTemporalAreaLatch.f4PreviousArea;

	// Lighting-area extent reciprocal (LightingSpread.frag world->texcoord multiply).
	const XMFLOAT4& rf4LightingArea = sTemporalAreaLatch.f4CurrentArea;
	float fLightingAreaWidth = rf4LightingArea.z - rf4LightingArea.x;
	float fLightingAreaHeight = rf4LightingArea.y - rf4LightingArea.w;
	rGlobalLayout.f2LightingAreaExtentInverse.x = 1.0f / fLightingAreaWidth;
	rGlobalLayout.f2LightingAreaExtentInverse.y = 1.0f / fLightingAreaHeight;

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

	[[maybe_unused]] auto [bLightingEnabled, bLightingEnabledPrevious, bLightingEnabledChanged] = gLightingEnabled.Changed<bool>();
	if (bLightingEnabledChanged)
	{
		gbLightingTemporalReset = true;
	}

	static int64_t siLightingRefreshFrame = 0;
	int64_t iLightingUpdateCadence = gLightingUpdateCadence.Get<int64_t>();
	++siLightingRefreshFrame;
	bool bScheduledLightingRefresh = gbLightingTemporalReset || (bLightingEnabled && siLightingRefreshFrame % iLightingUpdateCadence == 0);

	// Generate run-unique seed once and reuse every frame: stable noise pattern across the run, no temporal flicker.
	static const uint32_t suiRandomSeed = []
	{
		common::RandomEngine randomEngine;
		randomEngine.TimeSeed();
		return static_cast<uint32_t>(common::RandomNext(randomEngine) >> 32);
	}();
	rGlobalLayout.uiRandomSeed = suiRandomSeed;

	rGlobalLayout.fLightingObjectsAdd = gLightingObjectsAdd.mfCurrent;
	rGlobalLayout.fLightingDepositThreshold = gLightingDepositThreshold.mfCurrent;
	rGlobalLayout.fLightingDepositCompress = gLightingDepositCompress.mfCurrent;

	float fCombineMaxBrightness = gCombineMaxBrightness.mfCurrent;
	float fCombineContrast = gCombineContrast.mfCurrent;
	float fCombineLinearStart = gCombineLinearStart.mfCurrent;
	float fCombineLinearLength = gCombineLinearLength.mfCurrent;
	rGlobalLayout.fCombineMaxBrightness = fCombineMaxBrightness;
	rGlobalLayout.fCombineContrast = fCombineContrast;
	rGlobalLayout.fCombineLinearStart = fCombineLinearStart;
	rGlobalLayout.fCombineToe = gCombineToe.mfCurrent;
	rGlobalLayout.fCombineBlackTightness = gCombineBlackTightness.mfCurrent;
	rGlobalLayout.fCombineHuePreserve = gCombineHuePreserve.mfCurrent;

	// Precompute Uchimura segment constants S0/S1/CP from six invocation-invariant uniforms for LightCombine.comp and DebugTexture.frag; both
	// use the same P-S1 epsilon guard.
	float fCombineLinearSegmentLength = ((fCombineMaxBrightness - fCombineLinearStart) * fCombineLinearLength) / fCombineContrast;
	float fCombineShoulderStart = fCombineLinearStart + fCombineContrast * fCombineLinearSegmentLength;
	float fCombineShoulderCoefficient = (fCombineContrast * fCombineMaxBrightness) / std::max(fCombineMaxBrightness - fCombineShoulderStart, shaders::kfEpsilon);
	rGlobalLayout.fCombineShoulderInputStart = fCombineLinearStart + fCombineLinearSegmentLength;
	rGlobalLayout.fCombineShoulderOutputStart = fCombineShoulderStart;
	rGlobalLayout.fCombineShoulderExponentCoefficient = -fCombineShoulderCoefficient / fCombineMaxBrightness;

	float fPassCount = gSpreadPassCount.mfCurrent;
	float fPassNormalization = std::lerp(1.0f, 1.0f / fPassCount, gCombinePassNormalize.mfCurrent);
	float fPassScale = std::pow(fPassCount, -gCombineExposurePassScale.mfCurrent);
	float fCombinePassNormalizationScale = fPassNormalization * fPassScale;
	rGlobalLayout.fCombinePassNormalizationScale = fCombinePassNormalizationScale;
	rGlobalLayout.fCombinePassTotalScale = fCombinePassNormalizationScale / fPassCount;
	for (int64_t i = 0; i < _countof(rGlobalLayout.pfCombineCurvePoints); ++i)
	{
		float fPassFraction = 0.5f;
		if constexpr (shaders::kiMaxSpreadPasses > 1)
		{
			fPassFraction = static_cast<float>(i) / static_cast<float>(shaders::kiMaxSpreadPasses - 1);
		}
		rGlobalLayout.pfCombineCurvePoints[i] = (gbUseCombineCurveNew ? gCombineCurveNew : gCombineCurveOld).Evaluate(fPassFraction);
	}
	rGlobalLayout.fLightingTerrain = gLightingTerrain.mfCurrent;
	rGlobalLayout.fLightingObjects = gLightingObjects.mfCurrent;
	rGlobalLayout.fLightingAddTerrain = gLightingAddTerrain.mfCurrent;

	rGlobalLayout.fSpreadDirectionalityStart = gSpreadDirectionality.mfCurrent;
	rGlobalLayout.fSpreadDirectionCountStart = gSpreadDirectionCount.mfCurrent;
	rGlobalLayout.fSpreadDistanceStart = gSpreadDistance.mfCurrent;
	rGlobalLayout.fSpreadRingCountStart = gSpreadRingCount.mfCurrent;
	rGlobalLayout.fSpreadJitterStart = gSpreadJitter.mfCurrent;
	rGlobalLayout.fSpreadSampleJitterRangeStart = gSpreadSampleJitterRangeStart.mfCurrent;
	rGlobalLayout.fSpreadSampleJitterClusteringStart = gSpreadSampleJitterClusteringStart.mfCurrent;
	rGlobalLayout.fSpreadDecayStart = gSpreadDecay.mfCurrent;
	rGlobalLayout.fSpreadAccumulationDecayStart = gSpreadAccumulationDecay.mfCurrent;
	rGlobalLayout.fSpreadDistanceFalloffStart = gSpreadDistanceFalloff.mfCurrent;
	rGlobalLayout.fSpreadOutputThresholdStart = gSpreadOutputThreshold.mfCurrent;
	rGlobalLayout.fSpreadOutputCompressStart = gSpreadOutputCompress.mfCurrent;
	rGlobalLayout.fSpreadPassCount = gSpreadPassCount.mfCurrent;

	// Spread End (interpolation targets for last spread pass)
	rGlobalLayout.fSpreadDirectionalityEnd = gSpreadDirectionalityEnd.mfCurrent;
	rGlobalLayout.fSpreadDirectionCountEnd = gSpreadDirectionCountEnd.mfCurrent;
	rGlobalLayout.fSpreadDistanceEnd = engine::LerpAtHeight(engine::gpCamera->mfCameraEyeHeight, gSpreadDistanceEnd.startHeight.mfCurrent, gSpreadDistanceEnd.endHeight.mfCurrent, gSpreadDistanceEnd.low.mfCurrent, gSpreadDistanceEnd.high.mfCurrent);
	rGlobalLayout.fSpreadRingCountEnd = gSpreadRingCountEnd.mfCurrent;
	rGlobalLayout.fSpreadJitterEnd = gSpreadJitterEnd.mfCurrent;
	rGlobalLayout.fSpreadSampleJitterRangeEnd = gSpreadSampleJitterRangeEnd.mfCurrent;
	rGlobalLayout.fSpreadSampleJitterClusteringEnd = gSpreadSampleJitterClusteringEnd.mfCurrent;
	rGlobalLayout.fSpreadDecayEnd = gSpreadDecayEnd.mfCurrent;
	rGlobalLayout.fSpreadAccumulationDecayEnd = gSpreadAccumulationDecayEnd.mfCurrent;
	rGlobalLayout.fSpreadDistanceFalloffEnd = gSpreadDistanceFalloffEnd.mfCurrent;
	rGlobalLayout.fSpreadOutputThresholdEnd = gSpreadOutputThresholdEnd.mfCurrent;
	rGlobalLayout.fSpreadOutputCompressEnd = gSpreadOutputCompressEnd.mfCurrent;

	rGlobalLayout.fSpreadHeightMultiplier = gSpreadHeightMultiplier.mfCurrent;
	rGlobalLayout.fSpreadHeightEndHeightInverse = 1.0f / std::max(gSpreadHeightEndHeight.mfCurrent, 0.001f);
	rGlobalLayout.fSpreadHeightPower = gSpreadHeightPower.mfCurrent;

	// Per-ring rotation angles: jitter slider sets the seed; the shader scales by interpolated jitter
	// Each ring uses its own seed for uncorrelated rotations
	float fJitter = gSpreadJitter.mfCurrent;
	common::RandomEngine ringRandomEngine(1'000 * static_cast<uint32_t>(static_cast<float>(shaders::kiMaxSpreadPasses) * fJitter));
	for (float& rfSpreadRingRotation : rGlobalLayout.pfSpreadRingRotations)
	{
		rfSpreadRingRotation = common::Random<XM_2PI>(ringRandomEngine);
	}

	PopulateLightingParameters(rGlobalLayout, bScheduledLightingRefresh, bLightingEnabled);
}

void RenderLightingMain(int64_t iCommandBuffer)
{
	shaders::MainLayout& rMainLayout = *reinterpret_cast<shaders::MainLayout*>(&gpBufferManager->mMainLayoutUniformBuffers.at(iCommandBuffer).mpMappedMemory[0]);

	rMainLayout.fLightingSampledNormalsOneSize = gLightingSampledNormalsOneSize.mfCurrent;
	rMainLayout.fLightingSampledNormalsTwoSize = gLightingSampledNormalsTwoSize.mfCurrent;
	rMainLayout.fLightingSampledNormalsThreeSize = gLightingSampledNormalsThreeSize.mfCurrent;
	rMainLayout.uiWaterNormalIndexOne = static_cast<uint32_t>(gWaterNormalIndexOne.Get<int64_t>());
	rMainLayout.uiWaterNormalIndexTwo = static_cast<uint32_t>(gWaterNormalIndexTwo.Get<int64_t>());
	rMainLayout.uiWaterNormalIndexThree = static_cast<uint32_t>(gWaterNormalIndexThree.Get<int64_t>());
	// Normal weights use a fixed fade band from the default eye height to twice that height, with no author controls.
	// engine::CameraBase owns the fade endpoint; LerpAtHeight resolves the weights before upload.
	float fWaterNormalWeightOne = engine::LerpAtHeight(engine::gpCamera->mfCameraEyeHeight, engine::CameraBase::kfCameraEyeHeightDefault, engine::CameraBase::kfWaveFadeEndHeight, gLightingSampledNormalsWeightOneMinimum.mfCurrent, gLightingSampledNormalsWeightOneMaximum.mfCurrent);
	float fWaterNormalWeightTwo = engine::LerpAtHeight(engine::gpCamera->mfCameraEyeHeight, engine::CameraBase::kfCameraEyeHeightDefault, engine::CameraBase::kfWaveFadeEndHeight, gLightingSampledNormalsWeightTwoMinimum.mfCurrent, gLightingSampledNormalsWeightTwoMaximum.mfCurrent);
	float fWaterNormalWeightThree = engine::LerpAtHeight(engine::gpCamera->mfCameraEyeHeight, engine::CameraBase::kfCameraEyeHeightDefault, engine::CameraBase::kfWaveFadeEndHeight, gLightingSampledNormalsWeightThreeMinimum.mfCurrent, gLightingSampledNormalsWeightThreeMaximum.mfCurrent);
	rMainLayout.fWaterNormalWeightOne = fWaterNormalWeightOne;
	rMainLayout.fWaterNormalWeightTwo = fWaterNormalWeightTwo;
	rMainLayout.fWaterNormalWeightThree = fWaterNormalWeightThree;
	// Water.frag MIP_HANDOFF / mode-3 weight chain folded CPU-side (the weights are uniform once resolved by eye
	// height): per-octave relative-weight squares and the mode-3 agreement divisor's reciprocal.
	float fWaterNormalWeightTotal = fWaterNormalWeightOne + fWaterNormalWeightTwo + fWaterNormalWeightThree;
	if (fWaterNormalWeightTotal > 0.0f)
	{
		float fInverseTotalWeight = 1.0f / fWaterNormalWeightTotal;
		float fRelativeWeightOne = fWaterNormalWeightOne * fInverseTotalWeight;
		float fRelativeWeightTwo = fWaterNormalWeightTwo * fInverseTotalWeight;
		float fRelativeWeightThree = fWaterNormalWeightThree * fInverseTotalWeight;
		rMainLayout.fWaterNormalRelativeWeightSquaredOne = fRelativeWeightOne * fRelativeWeightOne;
		rMainLayout.fWaterNormalRelativeWeightSquaredTwo = fRelativeWeightTwo * fRelativeWeightTwo;
		rMainLayout.fWaterNormalRelativeWeightSquaredThree = fRelativeWeightThree * fRelativeWeightThree;
	}
	else
	{
		rMainLayout.fWaterNormalRelativeWeightSquaredOne = 0.0f;
		rMainLayout.fWaterNormalRelativeWeightSquaredTwo = 0.0f;
		rMainLayout.fWaterNormalRelativeWeightSquaredThree = 0.0f;
	}
	rMainLayout.fWaterNormalWeightSumInverse = 1.0f / std::max(3.0f * fWaterNormalWeightTotal, shaders::kfEpsilon);
	// Water.frag height darkening keeps the bottom and uploads only the range reciprocal; the two tunables are independent and their ranges overlap, so
	// floor the range magnitude at kfEpsilon to keep the reciprocal finite, keeping the sign so a top below the bottom still reads as an inverted range.
	float fWaterHeightDarkenTop = gWaterHeightDarkenTop.mfCurrent;
	float fWaterHeightDarkenBottom = gWaterHeightDarkenBottom.mfCurrent;
	rMainLayout.fWaterHeightDarkenBottom = fWaterHeightDarkenBottom;
	float fWaterHeightDarkenRange = fWaterHeightDarkenTop - fWaterHeightDarkenBottom;
	float fRangeMagnitude = std::max(std::abs(fWaterHeightDarkenRange), shaders::kfEpsilon);
	fWaterHeightDarkenRange = fWaterHeightDarkenRange < 0.0f ? -fRangeMagnitude : fRangeMagnitude;
	rMainLayout.fWaterHeightDarkenRangeInverse = 1.0f / fWaterHeightDarkenRange;
	rMainLayout.fWaterHeightDarkenTarget = gWaterHeightDarkenTarget.mfCurrent;
	rMainLayout.fWaterHeightDarkenSource = gWaterHeightDarkenSource.mfCurrent;
	rMainLayout.fWaterHeightDarkenLighting = gWaterHeightDarkenLighting.mfCurrent;

	// GlobalUniforms folds fLightingWaterSkyboxSunBias into globalLayout.f4WaterBiasedSunNormal.
	rMainLayout.fLightingWaterSkyboxNormalBlendWave = gLightingWaterSkyboxNormalBlendWave.mfCurrent;
	rMainLayout.fLightingWaterSkyboxIntensity = gLightingWaterSkyboxIntensity.mfCurrent;
	rMainLayout.fLightingWaterSkyboxAdd = gLightingWaterSkyboxAdd.mfCurrent;
	float fSkyboxPowerOne = gLightingWaterSkyboxOnePower.mfCurrent;
	float fSkyboxPowerTwo = gLightingWaterSkyboxTwoPower.mfCurrent;
	float fSkyboxPowerThree = gLightingWaterSkyboxThreePower.mfCurrent;
	rMainLayout.fLightingWaterSkyboxOnePower = fSkyboxPowerOne;
	rMainLayout.fLightingWaterSkyboxTwo = gLightingWaterSkyboxTwo.mfCurrent;
	rMainLayout.fLightingWaterSkyboxTwoPower = fSkyboxPowerTwo;
	rMainLayout.fLightingWaterSkyboxThree = gLightingWaterSkyboxThree.mfCurrent;
	rMainLayout.fLightingWaterSkyboxThreePower = fSkyboxPowerThree;
	rMainLayout.fLightingWaterSkyboxOneBeachReduction = gLightingWaterSkyboxOneBeachReduction.mfCurrent;
	rMainLayout.fLightingWaterSkyboxTwoBeachReduction = gLightingWaterSkyboxTwoBeachReduction.mfCurrent;
	rMainLayout.fLightingWaterSkyboxThreeBeachReduction = gLightingWaterSkyboxThreeBeachReduction.mfCurrent;
	// Per-lobe FilteredPowerLobe constants (Water.frag WATER_SPEC_AA_MODE 2/3): 2/(power+2) and 1/(1+power), xyz = lobes One/Two/Three.
	rMainLayout.f4WaterSkyboxLobeAlphaSquared = {2.0f / (fSkyboxPowerOne + 2.0f), 2.0f / (fSkyboxPowerTwo + 2.0f), 2.0f / (fSkyboxPowerThree + 2.0f), 0.0f};
	rMainLayout.f4WaterSkyboxLobeOnePlusPowerInverse = {1.0f / (1.0f + fSkyboxPowerOne), 1.0f / (1.0f + fSkyboxPowerTwo), 1.0f / (1.0f + fSkyboxPowerThree), 0.0f};
	rMainLayout.fLightingWaterSkyboxLevelOfDetail = gLightingWaterSkyboxLevelOfDetail.mfCurrent;
	rMainLayout.fWaterSpecularAntialiasingVariance = gWaterSpecularAntialiasingVariance.mfCurrent;
	rMainLayout.fWaterSpecularAntialiasingThreshold = gWaterSpecularAntialiasingThreshold.mfCurrent;
	rMainLayout.fWaterSpecularAntialiasingMipmapScale = gWaterSpecularAntialiasingMipmapScale.mfCurrent;
	rMainLayout.fWaterNormalMipmapBias = gWaterNormalMipmapBias.mfCurrent;
	// WATER_SPEC_AA_MIP_HANDOFF uses header-baked Toksvig variance tables for the three selected octave-group textures.
	// Copying their 30 floats every frame keeps texture selections current without a separate invalidation path.
	std::memcpy(&rMainLayout.pfWaterSpecularAntialiasingMipmapVariance[0 * shaders::kiWaterSpecularAntialiasingMipTableSize], gpTextureManager->mpfWaterNormalMipVariance[gWaterNormalIndexOne.Get<int64_t>()], shaders::kiWaterSpecularAntialiasingMipTableSize * sizeof(float));
	std::memcpy(&rMainLayout.pfWaterSpecularAntialiasingMipmapVariance[1 * shaders::kiWaterSpecularAntialiasingMipTableSize], gpTextureManager->mpfWaterNormalMipVariance[gWaterNormalIndexTwo.Get<int64_t>()], shaders::kiWaterSpecularAntialiasingMipTableSize * sizeof(float));
	std::memcpy(&rMainLayout.pfWaterSpecularAntialiasingMipmapVariance[2 * shaders::kiWaterSpecularAntialiasingMipTableSize], gpTextureManager->mpfWaterNormalMipVariance[gWaterNormalIndexThree.Get<int64_t>()], shaders::kiWaterSpecularAntialiasingMipTableSize * sizeof(float));
	// Full-detail reference weights for WATER_SPEC_AA_FADE_HANDOFF: the same LerpAtHeight the live
	// fWaterNormalWeight* uploads above use, evaluated at the near-camera endpoint height.
	rMainLayout.fWaterNormalWeightFullOne = engine::LerpAtHeight(engine::CameraBase::kfCameraEyeHeightDefault, engine::CameraBase::kfCameraEyeHeightDefault, engine::CameraBase::kfWaveFadeEndHeight, gLightingSampledNormalsWeightOneMinimum.mfCurrent, gLightingSampledNormalsWeightOneMaximum.mfCurrent);
	rMainLayout.fWaterNormalWeightFullTwo = engine::LerpAtHeight(engine::CameraBase::kfCameraEyeHeightDefault, engine::CameraBase::kfCameraEyeHeightDefault, engine::CameraBase::kfWaveFadeEndHeight, gLightingSampledNormalsWeightTwoMinimum.mfCurrent, gLightingSampledNormalsWeightTwoMaximum.mfCurrent);
	rMainLayout.fWaterNormalWeightFullThree = engine::LerpAtHeight(engine::CameraBase::kfCameraEyeHeightDefault, engine::CameraBase::kfCameraEyeHeightDefault, engine::CameraBase::kfWaveFadeEndHeight, gLightingSampledNormalsWeightThreeMinimum.mfCurrent, gLightingSampledNormalsWeightThreeMaximum.mfCurrent);

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

	rMainLayout.fPhysicallyBasedRenderingExposure = gPhysicallyBasedRenderingExposure.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingGammaInverse = 1.0f / gPhysicallyBasedRenderingGamma.mfCurrent;
	rMainLayout.fColorGradingSaturation = gColorGradingSaturation.mfCurrent;
	rMainLayout.fColorGradingContrast = gColorGradingContrast.mfCurrent;
	rMainLayout.fColorGradingTemperature = gColorGradingTemperature.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingDayBrightness = gPhysicallyBasedRenderingDayBrightness.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingAmbient = gPhysicallyBasedRenderingImageBasedLightingAmbient.mfCurrent;

	rMainLayout.fPhysicallyBasedRenderingMipmapCount = static_cast<float>(gpTextureManager->mTextureCache.miPhysicallyBasedRenderingCubeMipmapCount);
	rMainLayout.fPhysicallyBasedRenderingSmoke = gPhysicallyBasedRenderingSmoke.mfCurrent;

	rMainLayout.fPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionDiffuse = gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionDiffuse.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionDiffusePower = gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionDiffusePower.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionSpecular = gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionSpecular.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionSpecularPower = gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionSpecularPower.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingImageBasedLightingDiffuse = gPhysicallyBasedRenderingImageBasedLightingDiffuse.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingImageBasedLightingDiffusePower = gPhysicallyBasedRenderingImageBasedLightingDiffusePower.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingImageBasedLightingSpecular = gPhysicallyBasedRenderingImageBasedLightingSpecular.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingImageBasedLightingSpecularPower = gPhysicallyBasedRenderingImageBasedLightingSpecularPower.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingSun = gPhysicallyBasedRenderingSun.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingLighting = gPhysicallyBasedRenderingLighting.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingLightingPower = gPhysicallyBasedRenderingLightingPower.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingLightingSpecular = gPhysicallyBasedRenderingLightingSpecular.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingLightingSpecularPower = gPhysicallyBasedRenderingLightingSpecularPower.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingEmissive = gPhysicallyBasedRenderingEmissive.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingImageBasedLightingShadowBlend = gPhysicallyBasedRenderingImageBasedLightingShadowBlend.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingImageBasedLightingAmbientColorBlend = gPhysicallyBasedRenderingImageBasedLightingAmbientColorBlend.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingShadowFloor = gPhysicallyBasedRenderingShadowFloor.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingCubemapLevelOfDetailPower = gPhysicallyBasedRenderingCubemapLevelOfDetailPower.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingCubemapLevelOfDetailOffset = gPhysicallyBasedRenderingCubemapLevelOfDetailOffset.mfCurrent;

	rMainLayout.fSmokeShadowIntensity = gSmokeShadowIntensity.mfCurrent;
}

void RenderLightingSpreadIndirect(int64_t iCommandBuffer)
{
	// A disabled refresh still runs combine/temporal/history once to publish black, but the unconditional spread
	// attachment clears already provide its zero inputs, so no spread draw is needed.
	int64_t iInstanceCount = sbLightingRefreshFrame && gLightingEnabled.Get<bool>() ? 1 : 0;

	// Update all kiMaxSpreadPasses pipelines because the recorded Main command buffer selects the active passes.
	// CreateHostVisibleIndirectBuffer zeroes slots during pipeline recreation, so recorded passes need their instance counts restored.
	// Write only iCommandBuffer's framebuffer slot; the other slots may still be in use by the GPU.
	for (Pipeline& rPipeline : gpPipelineManager->mSpreadPipelines)
	{
		rPipeline.WriteIndirectBuffer(iCommandBuffer, iInstanceCount);
	}

	// The combine-sized chain covers the whole texture; the refresh predicate suppresses it by zeroing the Z group
	// count, so a skip dispatches nothing without re-recording the Main CB.
	VkExtent3D vkCombineExtent = gpTextureManager->mRenderTargetTextures.mpCombineTextures[0].mInfo.vkExtent3D;
	int64_t iCombineGroupsX = TileCount(vkCombineExtent.width);
	int64_t iCombineGroupsY = TileCount(vkCombineExtent.height);
	int64_t iCombineGroupsZ = sbLightingRefreshFrame ? 1 : 0;
	gpPipelineManager->mCombinePipeline.WriteIndirectComputeBuffer(iCommandBuffer, iCombineGroupsX, iCombineGroupsY, iCombineGroupsZ);
	gpPipelineManager->mLightingTemporalPipeline.WriteIndirectComputeBuffer(iCommandBuffer, iCombineGroupsX, iCombineGroupsY, iCombineGroupsZ);
	gpPipelineManager->mLightingHistoryCopyPipeline.WriteIndirectComputeBuffer(iCommandBuffer, iCombineGroupsX, iCombineGroupsY, iCombineGroupsZ);
}

} // namespace engine

#endif // defined(BT_CLIENT)
