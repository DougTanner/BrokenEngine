#pragma once

#include "WrapperBase.h"

namespace engine
{

// Global
// Scales the water pattern lengths (wavelengths, wave height, normal-map, color-noise and foam tile sizes, height
// thresholds) at the CPU publication sites; depth and terrain lengths stay unscaled, and wave and normal-map scroll
// speeds scale by its square root.
inline Wrapper gWaterScale(1.0f, 0.3f, 4.0f);

// Specular - Normals (per-sample row layout: chevron | size | weight-min | weight-max | rotation | speed-min | speed-max | speed-direction | patch-blend)
// Water normal map atlas: 3 weighted samples each indexed into TextureManager::kpWaterNormalCrcs.
inline Wrapper gWaterNormalIndexOne(12i64, std::vector<int64_t> {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
inline Wrapper gLightingSampledNormalsOneSize(0.2f, 0.05f, 1.0f);
inline Wrapper gLightingSampledNormalsWeightOneMinimum(4.0f, 0.0f, 4.0f);
inline Wrapper gLightingSampledNormalsWeightOneMaximum(4.0f, 0.0f, 4.0f);
inline Wrapper gWaterNormalRotationOne(0.16f, -XM_PI, XM_PI);
inline Wrapper gLightingSampledNormalsSpeedOneMinimum(0.01f, 0.0f, 0.1f);
inline Wrapper gLightingSampledNormalsSpeedOneMaximum(0.05f, 0.0f, 0.2f);
inline Wrapper gWaterNormalSpeedDirectionOne(1.73f, -XM_PI, XM_PI);
inline Wrapper gWaterNormalPatchBlendOne(1.0f, 0.0f, 1.0f);
inline Wrapper gWaterNormalIndexTwo(15i64, std::vector<int64_t> {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
inline Wrapper gLightingSampledNormalsTwoSize(0.05f, 0.025f, 0.1f);
inline Wrapper gLightingSampledNormalsWeightTwoMinimum(0.7f, 0.0f, 4.0f);
inline Wrapper gLightingSampledNormalsWeightTwoMaximum(0.75f, 0.0f, 4.0f);
inline Wrapper gWaterNormalRotationTwo(0.75f, -XM_PI, XM_PI);
inline Wrapper gLightingSampledNormalsSpeedTwoMinimum(0.05f, 0.0f, 0.2f);
inline Wrapper gLightingSampledNormalsSpeedTwoMaximum(0.075f, 0.0f, 0.2f);
inline Wrapper gWaterNormalSpeedDirectionTwo(-1.0f, -XM_PI, XM_PI);
inline Wrapper gWaterNormalPatchBlendTwo(1.0f, 0.0f, 1.0f);
inline Wrapper gWaterNormalIndexThree(4i64, std::vector<int64_t> {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
inline Wrapper gLightingSampledNormalsThreeSize(0.02f, 0.02f, 0.1f);
inline Wrapper gLightingSampledNormalsWeightThreeMinimum(0.0f, 0.0f, 4.0f);
inline Wrapper gLightingSampledNormalsWeightThreeMaximum(0.0f, 0.0f, 4.0f);
inline Wrapper gWaterNormalRotationThree(-2.0f, -XM_PI, XM_PI);
inline Wrapper gLightingSampledNormalsSpeedThreeMinimum(0.1f, 0.0f, 0.2f);
inline Wrapper gLightingSampledNormalsSpeedThreeMaximum(0.15f, 0.0f, 1.0f);
inline Wrapper gWaterNormalSpeedDirectionThree(-1.7f, -XM_PI, XM_PI);
inline Wrapper gWaterNormalPatchBlendThree(1.0f, 0.0f, 1.0f);
inline Wrapper gWaterDepthReflectionFeather(0.05f, 0.001f, 0.1f);
inline Wrapper gWaterWaveNormalBlend(0.8f, 0.0f, 1.0f);

// Specular - Skybox
inline Wrapper gLightingWaterSkyboxSunBias(2.0424f, 0.0f, 4.0f);
inline Wrapper gLightingWaterSkyboxNormalSoftenSunrise(0.5323f, 0.0f, 1.0f);
inline Wrapper gLightingWaterSkyboxNormalSoftenNoon(0.4586f, 0.0f, 1.0f);
inline Wrapper gLightingWaterSkyboxNormalBlendWave(0.1216f, 0.0f, 0.4f);
inline Wrapper gLightingWaterSkyboxIntensity(0.000289f, 0.0001f, 0.002f);
inline Wrapper gLightingWaterSkyboxAdd(1.6619f, 0.0f, 4.0f);
inline Wrapper gLightingWaterSkyboxOne(4'000.0f, 0.0f, 10'000.0f);
inline Wrapper gLightingWaterSkyboxOneNight(750.0f, 0.0f, 10'000.0f);
inline Wrapper gLightingWaterSkyboxOnePower(200.0f, 50.0f, 400.0f);
inline Wrapper gLightingWaterSkyboxTwo(1'200.0f, 0.0f, 5'000.0f);
inline Wrapper gLightingWaterSkyboxTwoNight(400.0f, 0.0f, 5'000.0f);
inline Wrapper gLightingWaterSkyboxTwoPower(25.035f, 1.0f, 100.0f);
inline Wrapper gLightingWaterSkyboxThree(455.0f, 1.0f, 800.0f);
inline Wrapper gLightingWaterSkyboxThreeNight(800.0f, 1.0f, 1'600.0f);
inline Wrapper gLightingWaterSkyboxThreePower(0.9f, 0.001f, 4.0f);
inline Wrapper gLightingWaterSkyboxLevelOfDetail(6.5f, 0.0f, 10.0f);

// Specular - Skybox One Sampling
inline Wrapper gWaterSkyboxOneSampleCount(2, std::vector<int32_t> {1, 2, 3, 4, 5, 6, 7, 8, 9});
inline Wrapper gWaterSkyboxOneSampleRadius(0.5f, 0.0f, 1.0f);
inline Wrapper gWaterSkyboxOneSampleBlend(1.0f, 0.0f, 1.0f);

// Specular - Height Darken
inline Wrapper gWaterHeightDarkenTop(0.05f, -0.1f, 0.05f);
inline Wrapper gWaterHeightDarkenBottom(-0.1f, -0.5f, 0.0f);
inline Wrapper gWaterHeightDarkenTarget(1.0f, 0.0f, 2.0f);
inline Wrapper gWaterHeightDarkenLighting(0.1f, 0.0f, 1.0f);

// Low frequency waves
inline Wrapper gWaterLowCount(31i64, std::vector<int64_t> {15, 31, 63, 127, 255}); // wave-count radio selector, not slider-mapped
inline Wrapper gWaterLowAngle(4.8f, 0.0f, XM_2PI);
inline Wrapper gWaterLowWavelength(4.0f, 1.0f, 20.0f);
inline Wrapper gWaterLowAmplitude(0.05f, 0.0f, 0.1f);
inline Wrapper gWaterLowSpeed(0.3f, 0.0f, 1.0f);
inline Wrapper gWaterLowSteepness(0.5f, 0.0f, 2.0f);
inline Wrapper gWaterLowAngleAdjust(0.2f, 0.0f, 0.5f);
inline Wrapper gWaterLowWavelengthAdjust(-0.6f, -2.0f, 0.0f);
inline Wrapper gWaterLowAmplitudeAdjust(1.0f, 0.0f, 4.0f);
inline Wrapper gWaterLowSpeedAdjust(0.288f, 0.0f, 4.0f);
// Camera-height fade for low-frequency wave amplitudes: 1.0 at or below Start, 0.0 at or above End, and linear between.
inline Wrapper gWaterLowAmplitudeFadeStart(300.0f, 0.0f, 10'000.0f);
inline Wrapper gWaterLowAmplitudeFadeEnd(600.0f, 0.0f, 10'000.0f);

// Medium frequency waves
inline Wrapper gWaterMediumCount(255i64, std::vector<int64_t> {15, 31, 63, 127, 255}); // wave-count radio selector, not slider-mapped
inline Wrapper gWaterMediumWavelength(5.0f, 0.01f, 10.0f);
inline Wrapper gWaterMediumAmplitude(0.012f, 0.0f, 0.02f);
inline Wrapper gWaterMediumSpeed(0.2f, 0.001f, 0.3f);
inline Wrapper gWaterMediumSteepness(1.0f, 0.0f, 2.0f);
inline Wrapper gWaterMediumAngleAdjust(40.0f, 0.0f, 80.0f);
inline Wrapper gWaterMediumWavelengthAdjust(0.8f, 0.0f, 1.0f);
inline Wrapper gWaterMediumAmplitudeAdjust(0.9f, 0.0f, 5.0f);
inline Wrapper gWaterMediumSpeedAdjust(2.0f, 0.0f, 5.0f);
// Camera-height fade for medium-frequency wave amplitudes (independent of low). Same start/end convention as low.
inline Wrapper gWaterMediumAmplitudeFadeStart(300.0f, 0.0f, 10'000.0f);
inline Wrapper gWaterMediumAmplitudeFadeEnd(600.0f, 0.0f, 10'000.0f);

// Depth
inline Wrapper gWaterTerrainHeight(3.5f, 1.0f, 8.0f);
inline Wrapper gWaterTerrainFade(0.3f, 0.1f, 5.0f);
inline Wrapper gWaterDepthLookupTableFeather(0.1f, 0.01f, 0.2f);
inline Wrapper gWaterDepthLookupTableSunsetFadePower(10.0f, 0.1f, 10.0f);
inline Wrapper gWaterDepthLookupTableSunsetFadeIntensity(0.3f, 0.0f, 1.0f);
inline Wrapper gWaterDepthColorFeather(0.3f, 0.01f, 0.4f);
inline Wrapper gWaterDepthColorFloor(0.2f, 0.0f, 1.0f);
inline Wrapper gWaterUnderseaCompression(0.8f, 0.1f, 1.0f);
inline Wrapper gWaterColorBottom(0.08f, -2.0f, 2.0f);
inline Wrapper gWaterColorHeight(1.86f, 0.0f, 4.0f);
inline Wrapper gWaterFresnel(0.07f, 0.0f, 0.2f);
inline Wrapper gWaterColorNoiseFrequency(0.0013f, 0.0f, 0.01f);
inline Wrapper gWaterColorNoiseAmount(0.1f, 0.0f, 0.2f);
inline Wrapper gWaterColorNoiseWeightOne(-1.5f, -2.0f, 2.0f);
// Step 0.1 keeps mult*10 integer for the Water.frag fract()-wrap precision contract (see Water.frag color-noise UV block).
inline Wrapper gWaterColorNoiseMultiplierOne(0.2f, 0.0f, 1.0f, 0.1f);
inline Wrapper gWaterColorNoiseWeightTwo(0.5f, -2.0f, 2.0f);
inline Wrapper gWaterColorNoiseMultiplierTwo(1.0f, 0.0f, 4.0f, 0.1f);
inline Wrapper gWaterColorNoisePatchBlend(1.0f, 0.0f, 1.0f);

// Beach
inline Wrapper gWaterBreakStartDepth(0.0f, 0.0f, 5.0f);
inline Wrapper gWaterBreakEndDepth(3.0f, 0.1f, 50.0f);
inline Wrapper gWaterBreakBlendCurve(0.4f, 0.05f, 1.0f);
inline Wrapper gWaterMediumShoreSoftness(0.5f, 0.0f, 1.0f);
inline Wrapper gLightingWaterSkyboxOneBeachReduction(0.4f, 0.0f, 1.0f);
inline Wrapper gLightingWaterSkyboxTwoBeachReduction(0.0f, 0.0f, 1.0f);
inline Wrapper gLightingWaterSkyboxThreeBeachReduction(0.0f, 0.0f, 1.0f);

// Foam
inline Wrapper gWaterFoamEnabled(true); // checkbox, not slider-mapped
inline Wrapper gWaterFoamEmissionRate(20.0f, 0.0f, 40.0f);
inline Wrapper gWaterFoamJStart(0.75f, 0.01f, 1.0f);
inline Wrapper gWaterFoamJFull(0.4f, -1.0f, 0.99f);
inline Wrapper gWaterFoamHalfLife(0.75f, 0.1f, 2.0f);
inline Wrapper gWaterFoamTextureIndex(0i64, std::vector<int64_t> {0, 1, 2});
inline Wrapper gWaterFoamTextureTileSize(3.020149f, 0.1f, 4.0f);
inline Wrapper gWaterFoamTextureStrength(0.2f, 0.0f, 2.0f);
inline Wrapper gWaterFoamTexturePatchBlend(1.0f, 0.0f, 1.0f);
inline Wrapper gWaterFoamBreakupTextureIndex(1i64, std::vector<int64_t> {0, 1, 2});
inline Wrapper gWaterFoamBreakupTileSize(8.0f, 0.1f, 16.0f);
inline Wrapper gWaterFoamBreakupAmount(1.0f, 0.0f, 1.0f);
inline Wrapper gWaterFoamBreakupPatchBlend(1.0f, 0.0f, 1.0f);
inline Wrapper gWaterFoamSpawnNoiseAmount(0.199005f, 0.0f, 1.0f);
inline Wrapper gWaterFoamSpawnNoiseTileSize(30.854477f, 1.0f, 128.0f);
inline Wrapper gWaterFoamSpawnNoisePatchBlend(1.0f, 0.0f, 1.0f);

// Break Repetition
inline Wrapper gPatchWeightThreshold(0.4f, 0.0f, 1.0f); // MakePatchTriangle caps the subtracted threshold below the largest normalized cube, so the excess sum stays positive.
inline Wrapper gWaterNormalPatchBlendOneEnabled(true);
inline Wrapper gWaterNormalPatchBlendTwoEnabled(false);
inline Wrapper gWaterNormalPatchBlendThreeEnabled(false);
inline Wrapper gWaterColorNoisePatchBlendEnabled(false);
inline Wrapper gWaterFoamTexturePatchBlendEnabled(false);
inline Wrapper gWaterFoamBreakupPatchBlendEnabled(false);
inline Wrapper gWaterFoamSpawnNoisePatchBlendEnabled(false);
inline Wrapper gWaterNormalExtraSamplePatchBlendEnabled(true);

} // namespace engine
