#if defined(BT_CLIENT)

#include "Graphics/CameraBase.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/HeightLerpWrapperQuartet.h"
#include "Ui/PbrWrappersBase.h"
#include "Ui/ShadowWrappersBase.h"
#include "Ui/WaterWrappersBase.h"
#include "Render.h"

namespace engine
{

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
	float fWaterNormalWeightOne = engine::LerpAtHeight(engine::gpCamera->GetRenderEyeHeight(), engine::CameraBase::kfCameraEyeHeightDefault, engine::CameraBase::kfWaveFadeEndHeight, gLightingSampledNormalsWeightOneMinimum.mfCurrent, gLightingSampledNormalsWeightOneMaximum.mfCurrent);
	float fWaterNormalWeightTwo = engine::LerpAtHeight(engine::gpCamera->GetRenderEyeHeight(), engine::CameraBase::kfCameraEyeHeightDefault, engine::CameraBase::kfWaveFadeEndHeight, gLightingSampledNormalsWeightTwoMinimum.mfCurrent, gLightingSampledNormalsWeightTwoMaximum.mfCurrent);
	float fWaterNormalWeightThree = engine::LerpAtHeight(engine::gpCamera->GetRenderEyeHeight(), engine::CameraBase::kfCameraEyeHeightDefault, engine::CameraBase::kfWaveFadeEndHeight, gLightingSampledNormalsWeightThreeMinimum.mfCurrent, gLightingSampledNormalsWeightThreeMaximum.mfCurrent);
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
	rMainLayout.fPhysicallyBasedRenderingEmissive = gPhysicallyBasedRenderingEmissive.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingImageBasedLightingShadowBlend = gPhysicallyBasedRenderingImageBasedLightingShadowBlend.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingShadowFloor = gPhysicallyBasedRenderingShadowFloor.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingCubemapLevelOfDetailPower = gPhysicallyBasedRenderingCubemapLevelOfDetailPower.mfCurrent;
	rMainLayout.fPhysicallyBasedRenderingCubemapLevelOfDetailOffset = gPhysicallyBasedRenderingCubemapLevelOfDetailOffset.mfCurrent;

	rMainLayout.fSmokeShadowIntensity = gSmokeShadowIntensity.mfCurrent;
}

} // namespace engine

#endif // defined(BT_CLIENT)
