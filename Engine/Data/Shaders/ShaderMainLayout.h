#ifndef SHADER_MAIN_LAYOUT_H
#define SHADER_MAIN_LAYOUT_H
struct MainLayout
{
	int32_t iFrameNumber INIT;
	int32_t iRenderNumber INIT;

	vec4 f4x4ViewProjection[4] INIT;

	vec4 f4EyePosition INIT;
	vec4 f4ToEyeNormal INIT;
	// Camera-facing billboard basis derived CPU-side from f4ToEyeNormal (DebugRenderBillboard.vert); forward
	// stays f4ToEyeNormal. Mirrors the shader's worldUp-select (0.999 threshold, +X fallback). w unused.
	vec4 f4BillboardRight INIT;
	vec4 f4BillboardUp INIT;

	vec4 pf4LowWavesOne[256] INIT;
	vec4 pf4LowWavesTwo[256] INIT;

	vec4 pf4MediumWavesOne[256] INIT;
	vec4 pf4MediumWavesTwo[256] INIT;

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
	float fWaterNormalWRelSqOne INIT;    // (fWaterNormalWeightOne / weightTotal)^2 (Water.frag MIP_HANDOFF variance share)
	float fWaterNormalWRelSqTwo INIT;
	float fWaterNormalWRelSqThree INIT;
	float fWaterNormalWeightSumInv INIT; // 1 / max(3*(fWaterNormalWeightOne+Two+Three), kfEpsilon) (Water.frag mode-3 agreement)
	float fWaterHeightDarkenBottom INIT;
	float fWaterHeightDarkenRangeInv INIT; // 1 / (gWaterHeightDarkenTop - fWaterHeightDarkenBottom) (Water.frag height darken); unguarded
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
	vec4 f4WaterSkyboxLobeAlphaSq INIT;        // 2/(power+2) per lobe (Beckmann-equivalent kernel base)
	vec4 f4WaterSkyboxLobeOnePlusPowerInv INIT; // 1/(1+power) per lobe (amplitude normalization)
	float fLightingWaterSkyboxLod INIT;
	// Specular-AA tuning for Water.frag's WATER_SPEC_AA_MODE variants (variance: modes 1-3; threshold: modes 2-3)
	float fWaterSpecAAVariance INIT;
	float fWaterSpecAAThreshold INIT;
	// WATER_SPEC_AA_MIP_HANDOFF inputs: per-mip Toksvig variance tables for the three selected
	// octave-group textures (DataPacker-baked into TextureHeader, padded past the real chain with
	// the last value), the handoff scale slider, the water-normal sampler's mip LOD bias (added to
	// the shader's analytic LOD so the table lookup tracks the hardware fetch), and the three
	// near-camera full-detail weights (WATER_SPEC_AA_FADE_HANDOFF reference; ratio-clamped in-shader)
	float pfWaterSpecAAMipVariance[3 * kiWaterSpecAAMipTableSize] INIT;
	float fWaterSpecAAMipScale INIT;
	float fWaterNormalMipBias INIT;
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
	float fLightingWaterEwnsPow INIT;
	float fLightingWaterEwnsPowMode INIT;
	float fLightingWaterAmbientIntensity INIT;
	float fLightingWaterAmbientPower INIT;
	float fLightingWaterAmbientPowerMode INIT;
	float fLightingTerrainBelowBaseMultiplier INIT;
	float fLightingTerrainBelowBasePower INIT;

	// Pbr
	float fPbrExposure INIT;
	float fPbrGammaInv INIT; // 1 / gamma, applied in HdrResolve.frag
	float fColorGradingSaturation INIT;
	float fColorGradingContrast INIT;
	float fColorGradingTemperature INIT;
	float fPbrDayBrightness INIT;
	float fPbrAmbient INIT;

	float fPbrMipCount INIT;
	float fPbrSmoke INIT;

	float fPbrBrdfDiffuse INIT;
	float fPbrBrdfDiffusePower INIT;
	float fPbrBrdfSpecular INIT;
	float fPbrBrdfSpecularPower INIT;
	float fPbrIblDiffuse INIT;
	float fPbrIblDiffusePower INIT;
	float fPbrIblSpecular INIT;
	float fPbrIblSpecularPower INIT;
	float fPbrSun INIT;
	float fPbrLighting INIT;
	float fPbrLightingPower INIT;
	float fPbrLightingSpecular INIT;
	float fPbrLightingSpecularPower INIT;
	float fPbrEmissive INIT;
	float fPbrIblShadowBlend INIT;
	float fPbrIblAmbientColorBlend INIT;
	float fPbrShadowFloor INIT;
	float fPbrCubemapLodPower INIT;
	float fPbrCubemapLodOffset INIT;

	// Shadow
	float fSmokeShadowIntensity INIT;

	// Hex shield
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
