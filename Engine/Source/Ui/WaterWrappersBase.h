#pragma once

#include "WrapperBase.h"

namespace engine
{

// Specular - Normals (per-sample row layout: chevron | size | weight-min | weight-max | rotation | speed-min | speed-max | speed-direction)
extern Wrapper gWaterNormalIndexOne;
extern Wrapper gLightingSampledNormalsOneSize;
extern Wrapper gLightingSampledNormalsWeightOneMinimum;
extern Wrapper gLightingSampledNormalsWeightOneMaximum;
extern Wrapper gWaterNormalRotationOne;
extern Wrapper gLightingSampledNormalsSpeedOneMinimum;
extern Wrapper gLightingSampledNormalsSpeedOneMaximum;
extern Wrapper gWaterNormalSpeedDirectionOne;
extern Wrapper gWaterNormalIndexTwo;
extern Wrapper gLightingSampledNormalsTwoSize;
extern Wrapper gLightingSampledNormalsWeightTwoMinimum;
extern Wrapper gLightingSampledNormalsWeightTwoMaximum;
extern Wrapper gWaterNormalRotationTwo;
extern Wrapper gLightingSampledNormalsSpeedTwoMinimum;
extern Wrapper gLightingSampledNormalsSpeedTwoMaximum;
extern Wrapper gWaterNormalSpeedDirectionTwo;
extern Wrapper gWaterNormalIndexThree;
extern Wrapper gLightingSampledNormalsThreeSize;
extern Wrapper gLightingSampledNormalsWeightThreeMinimum;
extern Wrapper gLightingSampledNormalsWeightThreeMaximum;
extern Wrapper gWaterNormalRotationThree;
extern Wrapper gLightingSampledNormalsSpeedThreeMinimum;
extern Wrapper gLightingSampledNormalsSpeedThreeMaximum;
extern Wrapper gWaterNormalSpeedDirectionThree;
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
extern Wrapper gLightingWaterSkyboxOnePower;
extern Wrapper gLightingWaterSkyboxTwo;
extern Wrapper gLightingWaterSkyboxTwoPower;
extern Wrapper gLightingWaterSkyboxThree;
extern Wrapper gLightingWaterSkyboxThreePower;
extern Wrapper gLightingWaterSkyboxLevelOfDetail;
extern Wrapper gWaterSpecularAntialiasingVariance;
extern Wrapper gWaterSpecularAntialiasingThreshold;
extern Wrapper gWaterSpecularAntialiasingMipmapScale;
extern Wrapper gWaterNormalMipmapBias;

// Specular - Height Darken
extern Wrapper gWaterHeightDarkenTop;
extern Wrapper gWaterHeightDarkenBottom;
extern Wrapper gWaterHeightDarkenTarget;
extern Wrapper gWaterHeightDarkenSource;
extern Wrapper gWaterHeightDarkenLighting;

// Low frequency waves
extern Wrapper gWaterLowCount; // wave-count radio selector, not slider-mapped
extern Wrapper gWaterLowMaximum;
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
extern Wrapper gWaterTerrainFadeClamp;
extern Wrapper gWaterHeight;
extern Wrapper gWaterEarlyOut;
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

// Beach
extern Wrapper gWaterBreakStartDepth;
extern Wrapper gWaterBreakEndDepth;
extern Wrapper gWaterBreakBlendCurve;
extern Wrapper gWaterMediumShoreSoftness;
extern Wrapper gLightingWaterSkyboxOneBeachReduction;
extern Wrapper gLightingWaterSkyboxTwoBeachReduction;
extern Wrapper gLightingWaterSkyboxThreeBeachReduction;

} // namespace engine
