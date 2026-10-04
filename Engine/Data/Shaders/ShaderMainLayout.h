#if defined(BT_ENGINE)
#pragma once
#endif

#if !defined(SHADER_MAIN_LAYOUT_H)
#define SHADER_MAIN_LAYOUT_H
struct MainLayout
{
	int32_t iFrameNumber INIT;
	int32_t iRenderNumber INIT;

	vec4 f4x4ViewProjection[4] INIT;

	vec4 f4EyePosition INIT;
	vec4 f4ToEyeNormal INIT;
	// MainUniforms.cpp derives right/up from f4ToEyeNormal using +Z when |z| < 0.999 and +X otherwise.
	// DebugRenderBillboard.vert uses f4ToEyeNormal as forward; basis w components are unused.
	vec4 f4BillboardRight INIT;
	vec4 f4BillboardUp INIT;

	vec4 pf4LowWavesOne[256] INIT;
	vec4 pf4LowWavesTwo[256] INIT;

	vec4 pf4MediumWavesOne[256] INIT;
	vec4 pf4MediumWavesTwo[256] INIT;
	int32_t iWaterLowCount INIT;
	int32_t iWaterMediumCount INIT;
	int32_t iWaterActiveQuadX INIT;
	int32_t iWaterActiveQuadY INIT;

	// Lighting — water normal map atlas (3 weighted samples)
	float fLightingSampledNormalsOneSize INIT;
	float fLightingSampledNormalsTwoSize INIT;
	float fLightingSampledNormalsThreeSize INIT;
	uint32_t uiWaterNormalIndexOne INIT;
	uint32_t uiWaterNormalIndexTwo INIT;
	uint32_t uiWaterNormalIndexThree INIT;
	float fWaterNormalWeightOne INIT;
	float fWaterNormalWeightTwo INIT;
	float fWaterNormalWeightThree INIT;
	float fWaterNormalRelativeWeightSquaredOne INIT;    // (fWaterNormalWeightOne / weightTotal)^2 (Water.frag MIP_HANDOFF variance share)
	float fWaterNormalRelativeWeightSquaredTwo INIT;
	float fWaterNormalRelativeWeightSquaredThree INIT;
	float fWaterNormalWeightSumInverse INIT; // 1 / max(3*(fWaterNormalWeightOne+Two+Three), kfEpsilon) (Water.frag mode-3 agreement)
	float fWaterHeightDarkenBottom INIT;
	float fWaterHeightDarkenRangeInverse INIT; // 1 / (gWaterHeightDarkenTop - fWaterHeightDarkenBottom) (Water.frag height darken); magnitude floored at kfEpsilon, sign kept
	float fWaterHeightDarkenTarget INIT;
	float fWaterHeightDarkenSource INIT;
	float fWaterHeightDarkenLighting INIT;

	float fLightingWaterSkyboxNormalBlendWave INIT;
	float fLightingWaterSkyboxIntensity INIT;
	float fLightingWaterSkyboxAdd INIT;
	float fLightingWaterSkyboxOnePower INIT;
	float fLightingWaterSkyboxTwo INIT;
	float fLightingWaterSkyboxTwoPower INIT;
	float fLightingWaterSkyboxThree INIT;
	float fLightingWaterSkyboxThreePower INIT;
	float fLightingWaterSkyboxOneBeachReduction INIT; // 0 = identity, 1 = remove at the shoreline (WaterSpecular.h)
	float fLightingWaterSkyboxTwoBeachReduction INIT;
	float fLightingWaterSkyboxThreeBeachReduction INIT;
	// Per-lobe FilteredPowerLobe (Water.frag WATER_SPEC_AA_MODE 2/3) constants folded from the skybox lobe
	// powers: xyz = lobes One/Two/Three. The varying (1+p') numerator math stays in-shader.
	vec4 f4WaterSkyboxLobeAlphaSquared INIT;        // 2/(power+2) per lobe (Beckmann-equivalent kernel base)
	vec4 f4WaterSkyboxLobeOnePlusPowerInverse INIT; // 1/(1+power) per lobe (amplitude normalization)
	float fLightingWaterSkyboxLevelOfDetail INIT;
	// Specular-AA tuning for Water.frag's WATER_SPEC_AA_MODE variants (variance: modes 1-3; threshold: modes 2-3)
	float fWaterSpecularAntialiasingVariance INIT;
	float fWaterSpecularAntialiasingThreshold INIT;
	// WATER_SPEC_AA_MIP_HANDOFF inputs: per-mip Toksvig variance tables for the three selected
	// octave-group textures (DataPacker-baked into TextureHeader, padded past the real chain with
	// the last value), the handoff scale slider, the water-normal sampler's mip LOD bias (added to
	// the shader's analytic LOD so the table lookup tracks the hardware fetch), and the three
	// near-camera full-detail weights (WATER_SPEC_AA_FADE_HANDOFF reference; ratio-clamped in-shader)
	float pfWaterSpecularAntialiasingMipmapVariance[3 * kiWaterSpecularAntialiasingMipTableSize] INIT;
	float fWaterSpecularAntialiasingMipmapScale INIT;
	float fWaterNormalMipmapBias INIT;
	float fWaterNormalWeightFullOne INIT;
	float fWaterNormalWeightFullTwo INIT;
	float fWaterNormalWeightFullThree INIT;

	float fLightingWaterReflectedAmount INIT;
	float fLightingWaterReflectedNormalBlendWave INIT;
	float fLightingWaterReflectedDistortion INIT;
	float fLightingWaterReflectedFalloffStart INIT;
	float fLightingWaterReflectedFalloffPower INIT;
	float fLightingWaterReflectedFresnel INIT;
	float fLightingWaterReflectedIntensity INIT;

	float fLightingWaterNormalSoften INIT;
	float fLightingWaterNormalBlendWave INIT;
	float fLightingWaterIntensity INIT;
	float fLightingWaterAdd INIT;
	float fLightingWaterOne INIT;
	float fLightingWaterOnePower INIT;
	float fLightingWaterTwo INIT;
	float fLightingWaterTwoPower INIT;
	float fLightingWaterThree INIT;
	float fLightingWaterThreePower INIT;
	float fLightingWaterPowerMode INIT;

	float fLightingDirectionalIntensity INIT;
	float fLightingDirectionalPower INIT;
	float fLightingDirectionalPowerMode INIT;
	float fLightingAmbientIntensity INIT;
	float fLightingAmbientPower INIT;
	float fLightingAmbientPowerMode INIT;
	float fLightingWaterEastWestNorthSouthPower INIT;
	float fLightingWaterEastWestNorthSouthPowerMode INIT;
	float fLightingWaterAmbientIntensity INIT;
	float fLightingWaterAmbientPower INIT;
	float fLightingWaterAmbientPowerMode INIT;
	float fLightingTerrainBelowBaseMultiplier INIT;
	float fLightingTerrainBelowBasePower INIT;

	float fPhysicallyBasedRenderingExposure INIT;
	float fPhysicallyBasedRenderingGammaInverse INIT; // 1 / gamma, applied in HdrResolve.frag
	float fColorGradingSaturation INIT;
	float fColorGradingContrast INIT;
	float fColorGradingTemperature INIT;
	float fPhysicallyBasedRenderingDayBrightness INIT;
	float fPhysicallyBasedRenderingAmbient INIT;

	float fPhysicallyBasedRenderingMipmapCount INIT;
	float fPhysicallyBasedRenderingSmoke INIT;

	float fPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionDiffuse INIT;
	float fPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionDiffusePower INIT;
	float fPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionSpecular INIT;
	float fPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionSpecularPower INIT;
	float fPhysicallyBasedRenderingImageBasedLightingDiffuse INIT;
	float fPhysicallyBasedRenderingImageBasedLightingDiffusePower INIT;
	float fPhysicallyBasedRenderingImageBasedLightingSpecular INIT;
	float fPhysicallyBasedRenderingImageBasedLightingSpecularPower INIT;
	float fPhysicallyBasedRenderingSun INIT;
	float fPhysicallyBasedRenderingLighting INIT;
	float fPhysicallyBasedRenderingLightingPower INIT;
	float fPhysicallyBasedRenderingLightingSpecular INIT;
	float fPhysicallyBasedRenderingLightingSpecularPower INIT;
	float fPhysicallyBasedRenderingEmissive INIT;
	float fPhysicallyBasedRenderingImageBasedLightingShadowBlend INIT;
	float fPhysicallyBasedRenderingImageBasedLightingAmbientColorBlend INIT;
	float fPhysicallyBasedRenderingShadowFloor INIT;
	float fPhysicallyBasedRenderingCubemapLevelOfDetailPower INIT;
	float fPhysicallyBasedRenderingCubemapLevelOfDetailOffset INIT;

	float fSmokeShadowIntensity INIT;

	float fHexShieldGrow INIT;
	float fHexShieldEdgeDistance INIT;
	float fHexShieldEdgePower INIT;
	float fHexShieldEdgeMultiplier INIT;

	float fHexShieldWaveMultiplier INIT;
	float fHexShieldWaveDotMultiplier INIT;
	float fHexShieldWaveIntensityMultiplier INIT;
	float fHexShieldWaveIntensityPower INIT;
	float fHexShieldWaveFalloffPower INIT;

	float fHexShieldDirectionFalloffPower INIT;
	float fHexShieldDirectionMultiplier INIT;
};

#endif // SHADER_MAIN_LAYOUT_H
