#pragma once

#include "WrapperBase.h"

namespace engine
{

extern Wrapper gSmokeMaximum;
extern Wrapper gSmokePower;
extern Wrapper gSmokeDecay;
extern Wrapper gSmokeEdgeDecayDistance;
extern Wrapper gSmokeUpdateCadence;

extern Wrapper gSmokeColorMinimum;
extern Wrapper gSmokeColorMultiplier;
extern Wrapper gSmokeLightingMultiplier;

extern Wrapper gSmokeNoiseScaleOne;
extern Wrapper gSmokeNoiseScaleTwo;
extern Wrapper gSmokeWindNoiseScale;
extern Wrapper gSmokeNoiseQuantity;
extern Wrapper gSmokeWindNoiseQuantity;
extern Wrapper gSmokeCurlStrengthLow;
extern Wrapper gSmokeCurlStrengthHigh;
extern Wrapper gSmokeCurlScale;
extern Wrapper gSmokeCurlSpeed;

// Wind Displacement (Smoke tab's Wind Displacement section)
extern Wrapper gWindToSmokeStrength;
extern Wrapper gWindToSmokePower;
extern Wrapper gWindDisplacementNoiseScale;
extern Wrapper gWindSmokeRetention;
extern Wrapper gWindSmokeAdvection;

extern Wrapper gSmokeObjectHeight;

// Trails (also rendered in game-side TweaksScreenSmokeDeposits column 3)
extern Wrapper gSmokeTrailsQuantity;
extern Wrapper gSmokeTrailsWidthCurrent;
extern Wrapper gSmokeTrailsWidthPrevious;
extern Wrapper gSmokeTrailsLength;
extern Wrapper gSmokeTrailsLengthJitter;
extern Wrapper gSmokeTrailsSideJitter;
extern Wrapper gSmokeIntensityFalloff;

} // namespace engine
