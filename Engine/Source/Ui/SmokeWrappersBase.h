#pragma once

#include "WrapperBase.h"

namespace engine
{

inline Wrapper gSmokeMaximum(0.4f, 0.0f, 1.0f);
inline Wrapper gSmokePower(0.2f, 0.1f, 1.0f);
inline Wrapper gSmokeDecay(0.9985f, 0.990f, 1.0f);
inline Wrapper gSmokeEdgeDecayDistance(0.05f, 0.0f, 0.1f);
inline Wrapper gSmokeUpdateCadence(2.0f, 1.0f, 4.0f, 1.0f);

inline Wrapper gSmokeColorMinimum(0.15f, 0.0f, 1.0f);
inline Wrapper gSmokeColorMultiplier(2.0f, 0.1f, 4.0f);
inline Wrapper gSmokeLightingMultiplier(1.0f, 0.0f, 2.0f);

inline Wrapper gSmokeNoiseScaleOne(3.0f, 0.1f, 8.0f);
inline Wrapper gSmokeNoiseScaleTwo(0.2f, 0.01f, 1.0f);
inline Wrapper gSmokeWindNoiseScale(0.015f, 0.001f, 0.1f);
inline Wrapper gSmokeNoiseQuantity(0.00003f, 0.00001f, 0.0002f);
inline Wrapper gSmokeWindNoiseQuantityBottom(0.00005f, 0.0f, 0.001f);
inline Wrapper gSmokeWindNoiseQuantityTop(0.0001f, 0.0f, 0.001f);
// Each strength is the curl offset in smoke texels per step (SmokeCurlOffset blends between them). The per-refresh offset
// is the strength times the step count, which reaches the flight-plane count times Smoke Update Cadence with the stagger
// on (8 at cadence 4); once that passes the radius-2 tile (16 texel) dilate halo, strong-wind plume fronts thin.
inline Wrapper gSmokeCurlStrengthLow(0.05f, 0.0f, 0.5f);
inline Wrapper gSmokeCurlStrengthHigh(2.0f, 0.0f, 5.0f);
inline Wrapper gSmokeCurlScale(0.005f, 0.0001f, 0.01f);
inline Wrapper gSmokeCurlSpeed(1.0f, 0.0f, 4.0f);

// Wind Displacement (Smoke tab's Wind Displacement section)
inline Wrapper gWindToSmokeStrength(0.003f, 0.0f, 0.01f);
inline Wrapper gWindToSmokePower(1.05f, 0.6f, 2.0f);
inline Wrapper gWindDisplacementNoiseScale(1.0f, 0.0f, 4.0f);
inline Wrapper gWindSmokeRetention(0.6f, 0.0f, 1.0f);
inline Wrapper gWindSmokeAdvection(0.5f, 0.0f, 2.0f);

inline Wrapper gSmokeObjectHeight(2.5f, 0.5f, 8.0f);
inline Wrapper gSmokeObjectHeightPower(0.75f, 0.25f, 4.0f);

// Trails (also rendered in game-side TweaksScreenSmokeDeposits column 3)
inline Wrapper gSmokeTrailsQuantity(800.0f, 0.0f, 2'000.0f);
inline Wrapper gSmokeTrailsWidthCurrent(0.01f, 0.0f, 0.02f);
inline Wrapper gSmokeTrailsWidthPrevious(0.01f, 0.0f, 0.02f);
inline Wrapper gSmokeTrailsLength(1.0f, 0.0f, 4.0f);
inline Wrapper gSmokeTrailsLengthJitter(0.0f, 0.0f, 10.0f);
inline Wrapper gSmokeTrailsSideJitter(3.0f, 0.0f, 6.0f);
inline Wrapper gSmokeIntensityFalloff(3.0f, 0.1f, 10.0f);

} // namespace engine
