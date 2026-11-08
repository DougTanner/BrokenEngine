#pragma once

#include "WrapperBase.h"

namespace engine
{

// Global
extern Wrapper gWaterScale;

// Specular - Normals (per-sample row layout: chevron | size | weight-min | weight-max | rotation | speed-min | speed-max | speed-direction | patch-blend)
extern Wrapper gWaterNormalIndexOne;
extern Wrapper gLightingSampledNormalsOneSize;
extern Wrapper gLightingSampledNormalsWeightOneMinimum;
extern Wrapper gLightingSampledNormalsWeightOneMaximum;
extern Wrapper gWaterNormalRotationOne;
extern Wrapper gLightingSampledNormalsSpeedOneMinimum;
extern Wrapper gLightingSampledNormalsSpeedOneMaximum;
extern Wrapper gWaterNormalSpeedDirectionOne;
inline Wrapper gWaterNormalPatchBlendOne(1.0f, 0.0f, 1.0f);
extern Wrapper gWaterNormalIndexTwo;
extern Wrapper gLightingSampledNormalsTwoSize;
extern Wrapper gLightingSampledNormalsWeightTwoMinimum;
extern Wrapper gLightingSampledNormalsWeightTwoMaximum;
extern Wrapper gWaterNormalRotationTwo;
extern Wrapper gLightingSampledNormalsSpeedTwoMinimum;
extern Wrapper gLightingSampledNormalsSpeedTwoMaximum;
extern Wrapper gWaterNormalSpeedDirectionTwo;
inline Wrapper gWaterNormalPatchBlendTwo(1.0f, 0.0f, 1.0f);
extern Wrapper gWaterNormalIndexThree;
extern Wrapper gLightingSampledNormalsThreeSize;
extern Wrapper gLightingSampledNormalsWeightThreeMinimum;
extern Wrapper gLightingSampledNormalsWeightThreeMaximum;
extern Wrapper gWaterNormalRotationThree;
extern Wrapper gLightingSampledNormalsSpeedThreeMinimum;
extern Wrapper gLightingSampledNormalsSpeedThreeMaximum;
extern Wrapper gWaterNormalSpeedDirectionThree;
inline Wrapper gWaterNormalPatchBlendThree(1.0f, 0.0f, 1.0f);
extern Wrapper gWaterDepthReflectionFeather;
extern Wrapper gWaterWaveNormalBlend;

// Specular - Skybox
extern Wrapper gLightingWaterSkyboxSunBias;
extern Wrapper gLightingWaterSkyboxNormalSoftenSunrise;
extern Wrapper gLightingWaterSkyboxNormalSoftenNoon;
extern Wrapper gLightingWaterSkyboxNormalBlendWave;
extern Wrapper gLightingWaterSkyboxIntensity;
extern Wrapper gLightingWaterSkyboxAdd;
extern Wrapper gLightingWaterSkyboxOne;
extern Wrapper gLightingWaterSkyboxOneNight;
extern Wrapper gLightingWaterSkyboxOnePower;
extern Wrapper gLightingWaterSkyboxTwo;
extern Wrapper gLightingWaterSkyboxTwoNight;
extern Wrapper gLightingWaterSkyboxTwoPower;
extern Wrapper gLightingWaterSkyboxThree;
extern Wrapper gLightingWaterSkyboxThreeNight;
extern Wrapper gLightingWaterSkyboxThreePower;
extern Wrapper gLightingWaterSkyboxLevelOfDetail;

// Specular - Skybox One Sampling
extern Wrapper gWaterSkyboxOneSampleCount;
extern Wrapper gWaterSkyboxOneSampleRadius;
extern Wrapper gWaterSkyboxOneSampleBlend;

// Specular - Height Darken
extern Wrapper gWaterHeightDarkenTop;
extern Wrapper gWaterHeightDarkenBottom;
extern Wrapper gWaterHeightDarkenTarget;
extern Wrapper gWaterHeightDarkenLighting;

// Low frequency waves
extern Wrapper gWaterLowCount; // wave-count radio selector, not slider-mapped
extern Wrapper gWaterLowAngle;
extern Wrapper gWaterLowWavelength;
extern Wrapper gWaterLowAmplitude;
extern Wrapper gWaterLowSpeed;
extern Wrapper gWaterLowSteepness;
extern Wrapper gWaterLowAngleAdjust;
extern Wrapper gWaterLowWavelengthAdjust;
extern Wrapper gWaterLowAmplitudeAdjust;
extern Wrapper gWaterLowSpeedAdjust;
extern Wrapper gWaterLowAmplitudeFadeStart;
extern Wrapper gWaterLowAmplitudeFadeEnd;

// Medium frequency waves
extern Wrapper gWaterMediumCount; // wave-count radio selector, not slider-mapped
extern Wrapper gWaterMediumWavelength;
extern Wrapper gWaterMediumAmplitude;
extern Wrapper gWaterMediumSpeed;
extern Wrapper gWaterMediumSteepness;
extern Wrapper gWaterMediumAngleAdjust;
extern Wrapper gWaterMediumWavelengthAdjust;
extern Wrapper gWaterMediumAmplitudeAdjust;
extern Wrapper gWaterMediumSpeedAdjust;
extern Wrapper gWaterMediumAmplitudeFadeStart;
extern Wrapper gWaterMediumAmplitudeFadeEnd;

// Depth
extern Wrapper gWaterTerrainHeight;
extern Wrapper gWaterTerrainFade;
extern Wrapper gWaterDepthLookupTableFeather;
extern Wrapper gWaterDepthLookupTableSunsetFadePower;
extern Wrapper gWaterDepthLookupTableSunsetFadeIntensity;
extern Wrapper gWaterDepthColorFeather;
extern Wrapper gWaterDepthColorFloor;
extern Wrapper gWaterUnderseaCompression;
extern Wrapper gWaterColorBottom;
extern Wrapper gWaterColorHeight;
extern Wrapper gWaterFresnel;
extern Wrapper gWaterColorNoiseFrequency;
extern Wrapper gWaterColorNoiseAmount;
extern Wrapper gWaterColorNoiseWeightOne;
extern Wrapper gWaterColorNoiseMultiplierOne;
extern Wrapper gWaterColorNoiseWeightTwo;
extern Wrapper gWaterColorNoiseMultiplierTwo;
inline Wrapper gWaterColorNoisePatchBlend(1.0f, 0.0f, 1.0f);

// Beach
extern Wrapper gWaterBreakStartDepth;
extern Wrapper gWaterBreakEndDepth;
extern Wrapper gWaterBreakBlendCurve;
extern Wrapper gWaterMediumShoreSoftness;
extern Wrapper gLightingWaterSkyboxOneBeachReduction;
extern Wrapper gLightingWaterSkyboxTwoBeachReduction;
extern Wrapper gLightingWaterSkyboxThreeBeachReduction;

// Foam
extern Wrapper gWaterFoamEnabled; // checkbox, not slider-mapped
extern Wrapper gWaterFoamEmissionRate;
extern Wrapper gWaterFoamJStart;
extern Wrapper gWaterFoamJFull;
extern Wrapper gWaterFoamHalfLife;
extern Wrapper gWaterFoamTextureIndex;
extern Wrapper gWaterFoamTextureTileSize;
extern Wrapper gWaterFoamTextureStrength;
inline Wrapper gWaterFoamTexturePatchBlend(1.0f, 0.0f, 1.0f);
extern Wrapper gWaterFoamBreakupTextureIndex;
extern Wrapper gWaterFoamBreakupTileSize;
extern Wrapper gWaterFoamBreakupAmount;
inline Wrapper gWaterFoamBreakupPatchBlend(1.0f, 0.0f, 1.0f);
extern Wrapper gWaterFoamSpawnNoiseAmount;
extern Wrapper gWaterFoamSpawnNoiseTileSize;
inline Wrapper gWaterFoamSpawnNoisePatchBlend(1.0f, 0.0f, 1.0f);

} // namespace engine
