#include "WaterWrappersBase.h"

namespace engine
{

// Specular - Normals
// Water normal map atlas: 3 weighted samples each indexed into TextureManager::kpWaterNormalCrcs.
Wrapper gWaterNormalIndexOne(12i64, std::vector<int64_t> {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
Wrapper gLightingSampledNormalsOneSize(0.2f, 0.05f, 1.0f);
Wrapper gLightingSampledNormalsWeightOneMinimum(1.5f, 0.0f, 4.0f);
Wrapper gLightingSampledNormalsWeightOneMaximum(2.0f, 0.0f, 4.0f);
Wrapper gWaterNormalRotationOne(0.16f, -XM_PI, XM_PI);
Wrapper gLightingSampledNormalsSpeedOneMinimum(0.035f, 0.0f, 0.1f);
Wrapper gLightingSampledNormalsSpeedOneMaximum(0.15f, 0.0f, 1.0f);
Wrapper gWaterNormalSpeedDirectionOne(-2.3f, -XM_PI, XM_PI);
Wrapper gWaterNormalIndexTwo(15i64, std::vector<int64_t> {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
Wrapper gLightingSampledNormalsTwoSize(0.05f, 0.025f, 0.1f);
Wrapper gLightingSampledNormalsWeightTwoMinimum(0.25f, 0.0f, 4.0f);
Wrapper gLightingSampledNormalsWeightTwoMaximum(0.75f, 0.0f, 4.0f);
Wrapper gWaterNormalRotationTwo(0.75f, -XM_PI, XM_PI);
Wrapper gLightingSampledNormalsSpeedTwoMinimum(0.0f, 0.0f, 0.2f);
Wrapper gLightingSampledNormalsSpeedTwoMaximum(0.075f, 0.0f, 1.0f);
Wrapper gWaterNormalSpeedDirectionTwo(-1.0f, -XM_PI, XM_PI);
Wrapper gWaterNormalIndexThree(4i64, std::vector<int64_t> {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
Wrapper gLightingSampledNormalsThreeSize(0.02f, 0.02f, 0.1f);
Wrapper gLightingSampledNormalsWeightThreeMinimum(0.5f, 0.0f, 4.0f);
Wrapper gLightingSampledNormalsWeightThreeMaximum(0.8f, 0.0f, 4.0f);
Wrapper gWaterNormalRotationThree(0.17f, -XM_PI, XM_PI);
Wrapper gLightingSampledNormalsSpeedThreeMinimum(0.1f, 0.0f, 0.2f);
Wrapper gLightingSampledNormalsSpeedThreeMaximum(0.15f, 0.0f, 1.0f);
Wrapper gWaterNormalSpeedDirectionThree(-1.7f, -XM_PI, XM_PI);
Wrapper gWaterDepthReflectionFeather(0.05f, 0.001f, 0.1f);
Wrapper gWaterWaveNormalBlend(0.8f, 0.0f, 1.0f);

// Specular - Skybox
Wrapper gLightingWaterSkyboxSunBias(3.1f, 0.0f, 4.0f);
Wrapper gLightingWaterSkyboxNormalSoftenSunrise(0.75f, 0.0f, 1.0f);
Wrapper gLightingWaterSkyboxNormalSoftenNoon(0.5f, 0.0f, 1.0f);
Wrapper gLightingWaterSkyboxNormalBlendWave(0.1f, 0.0f, 0.4f);
Wrapper gLightingWaterSkyboxIntensity(0.0003f, 0.0001f, 0.002f);
Wrapper gLightingWaterSkyboxAdd(1.5f, 0.0f, 4.0f);
Wrapper gLightingWaterSkyboxOne(2'000.0f, 0.0f, 10'000.0f);
Wrapper gLightingWaterSkyboxOnePower(200.0f, 50.0f, 400.0f);
Wrapper gLightingWaterSkyboxTwo(3'000.0f, 0.0f, 5'000.0f);
Wrapper gLightingWaterSkyboxTwoPower(30.0f, 1.0f, 100.0f);
Wrapper gLightingWaterSkyboxThree(360.0f, 1.0f, 800.0f);
Wrapper gLightingWaterSkyboxThreePower(2.0f, 0.001f, 4.0f);
Wrapper gLightingWaterSkyboxLevelOfDetail(6.5f, 0.0f, 10.0f);
// Water.frag variance scales the specular filter kernel; mode 1 maps 0.25 to the exact pixel footprint.
// In modes 2 and 3, threshold clamps the widening.
Wrapper gWaterSpecularAntialiasingVariance(0.0f, 0.0f, 2.0f);
Wrapper gWaterSpecularAntialiasingThreshold(0.0f, 0.0f, 1.0f);
// WATER_SPEC_AA_MIP_HANDOFF: mip scale multiplies the baked per-mip Toksvig variance term (1.0 = the
// physically-derived kernel); mip bias is the water-normal sampler's LOD bias (negative = sharpen,
// positive = blur; 0 = unbiased, unlike the global -gMipmapLevelOfDetailBias sharpen), sampler recreate on change.
Wrapper gWaterSpecularAntialiasingMipmapScale(0.0f, 0.0f, 4.0f);
Wrapper gWaterNormalMipmapBias(0.3f, 0.0f, 1.0f);

// Specular - Height Darken
Wrapper gWaterHeightDarkenTop(0.05f, -0.1f, 0.05f);
Wrapper gWaterHeightDarkenBottom(-0.1f, -0.5f, 0.0f);
Wrapper gWaterHeightDarkenTarget(1.0f, 0.0f, 2.0f);
Wrapper gWaterHeightDarkenSource(0.0f, 0.0f, 1.0f);
Wrapper gWaterHeightDarkenLighting(0.1f, 0.0f, 1.0f);

// Low frequency waves
Wrapper gWaterLowCount(31i64, std::vector<int64_t> {15, 31, 63, 127, 255});
Wrapper gWaterLowMaximum(255.0f, 0.0f, 255.0f, 1.0f);
Wrapper gWaterLowAngle(4.8f, 0.0f, XM_2PI);
Wrapper gWaterLowWavelength(4.0f, 1.0f, 20.0f);
Wrapper gWaterLowAmplitude(0.05f, 0.0f, 0.1f);
Wrapper gWaterLowSpeed(0.4f, 0.0f, 1.0f);
Wrapper gWaterLowSteepness(0.5f, 0.0f, 2.0f);
Wrapper gWaterLowAngleAdjust(0.2f, 0.0f, 0.5f);
Wrapper gWaterLowWavelengthAdjust(-0.75f, -2.0f, 0.0f);
Wrapper gWaterLowAmplitudeAdjust(1.0f, 0.0f, 4.0f);
Wrapper gWaterLowSpeedAdjust(0.288f, 0.0f, 4.0f);
// Camera-height fade for low-frequency wave amplitudes: 1.0 at or below Start, 0.0 at or above End, and linear between.
Wrapper gWaterLowAmplitudeFadeStart(300.0f, 0.0f, 1'000.0f);
Wrapper gWaterLowAmplitudeFadeEnd(600.0f, 0.0f, 1'000.0f);

// Medium frequency waves
Wrapper gWaterMediumCount(255i64, std::vector<int64_t> {15, 31, 63, 127, 255});
Wrapper gWaterMediumWavelength(5.0f, 0.01f, 10.0f);
Wrapper gWaterMediumAmplitude(0.012f, 0.0f, 0.02f);
Wrapper gWaterMediumSpeed(0.2f, 0.001f, 0.3f);
Wrapper gWaterMediumSteepness(1.0f, 0.0f, 2.0f);
Wrapper gWaterMediumAngleAdjust(40.0f, 0.0f, 80.0f);
Wrapper gWaterMediumWavelengthAdjust(0.8f, 0.0f, 1.0f);
Wrapper gWaterMediumAmplitudeAdjust(0.9f, 0.0f, 5.0f);
Wrapper gWaterMediumSpeedAdjust(2.0f, 0.0f, 5.0f);
// Camera-height fade for medium-frequency wave amplitudes (independent of low). Same start/end convention as low.
Wrapper gWaterMediumAmplitudeFadeStart(300.0f, 0.0f, 1'000.0f);
Wrapper gWaterMediumAmplitudeFadeEnd(600.0f, 0.0f, 1'000.0f);

// Depth
Wrapper gWaterTerrainHeight(3.5f, 1.0f, 8.0f);
Wrapper gWaterTerrainFade(0.3f, 0.1f, 5.0f);
Wrapper gWaterTerrainFadeClamp(0.0f, 0.0f, 0.5f);
Wrapper gWaterHeight(0.0f, -0.1f, 0.03f);
Wrapper gWaterEarlyOut(0.0f, -0.1f, 0.1f);
Wrapper gWaterDepthLookupTableFeather(0.1f, 0.01f, 0.2f);
Wrapper gWaterDepthLookupTableSunsetFadePower(10.0f, 0.1f, 10.0f);
Wrapper gWaterDepthLookupTableSunsetFadeIntensity(0.3f, 0.0f, 1.0f);
Wrapper gWaterDepthColorFeather(0.3f, 0.01f, 0.4f);
Wrapper gWaterDepthColorFloor(0.2f, 0.0f, 1.0f);
Wrapper gWaterUnderseaCompression(0.8f, 0.1f, 1.0f);
Wrapper gWaterColorBottom(0.08f, -2.0f, 2.0f);
Wrapper gWaterColorHeight(1.86f, 0.0f, 4.0f);
Wrapper gWaterFresnel(0.07f, 0.0f, 0.2f);
Wrapper gWaterColorNoiseFrequency(0.0013f, 0.0f, 0.01f);
Wrapper gWaterColorNoiseAmount(0.1f, 0.0f, 0.2f);
Wrapper gWaterColorNoiseWeightOne(-1.5f, -2.0f, 2.0f);
// Step 0.1 keeps mult*10 integer for the Water.frag fract()-wrap precision contract (see Water.frag color-noise UV block).
Wrapper gWaterColorNoiseMultiplierOne(0.2f, 0.0f, 1.0f, 0.1f);
Wrapper gWaterColorNoiseWeightTwo(0.5f, -2.0f, 2.0f);
Wrapper gWaterColorNoiseMultiplierTwo(1.0f, 0.0f, 4.0f, 0.1f);

// Beach
Wrapper gWaterBreakStartDepth(0.0f, 0.0f, 5.0f);
Wrapper gWaterBreakEndDepth(4.0f, 0.1f, 50.0f);
Wrapper gWaterBreakBlendCurve(0.4f, 0.05f, 1.0f);
Wrapper gWaterMediumShoreSoftness(0.3f, 0.0f, 1.0f);
Wrapper gLightingWaterSkyboxOneBeachReduction(0.1f, 0.0f, 1.0f);
Wrapper gLightingWaterSkyboxTwoBeachReduction(0.1f, 0.0f, 1.0f);
Wrapper gLightingWaterSkyboxThreeBeachReduction(0.5f, 0.0f, 1.0f);

} // namespace engine
