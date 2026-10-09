#include "SmokeWrappersBase.h"

namespace engine
{

Wrapper gSmokeMaximum(0.2f, 0.0f, 1.0f);
Wrapper gSmokePower(0.18f, 0.1f, 1.0f);
Wrapper gSmokeDecay(0.998f, 0.990f, 1.0f);
Wrapper gSmokeEdgeDecayDistance(0.05f, 0.0f, 0.1f);
Wrapper gSmokeUpdateCadence(2.0f, 1.0f, 4.0f, 1.0f);

Wrapper gSmokeColorMinimum(0.2f, 0.0f, 1.0f);
Wrapper gSmokeColorMultiplier(2.0f, 0.1f, 4.0f);
Wrapper gSmokeLightingMultiplier(1.0f, 0.0f, 2.0f);

Wrapper gSmokeNoiseScaleOne(3.0f, 0.1f, 8.0f);
Wrapper gSmokeNoiseScaleTwo(0.2f, 0.01f, 1.0f);
Wrapper gSmokeWindNoiseScale(0.06f, 0.001f, 0.1f);
Wrapper gSmokeNoiseQuantity(0.000055f, 0.00001f, 0.0002f);
Wrapper gSmokeWindNoiseQuantity(0.00006f, 0.0f, 0.0001f);
// Each strength is the curl offset in smoke texels per step (SmokeCurlOffset blends between them). Above 4 at Smoke
// Update Cadence 4 the per-refresh offset passes the radius-2 tile (16 texel) dilate halo, so strong-wind plume fronts thin.
Wrapper gSmokeCurlStrengthLow(0.075f, 0.0f, 0.5f);
Wrapper gSmokeCurlStrengthHigh(2.0f, 0.0f, 5.0f);
Wrapper gSmokeCurlScale(0.005f, 0.0001f, 0.01f);
Wrapper gSmokeCurlSpeed(1.0f, 0.0f, 4.0f);

// Wind Displacement (Smoke tab's Wind Displacement section)
Wrapper gWindToSmokeStrength(0.003f, 0.0f, 0.01f);
Wrapper gWindToSmokePower(1.05f, 0.6f, 2.0f);
Wrapper gWindDisplacementNoiseScale(1.0f, 0.0f, 4.0f);
Wrapper gWindSmokeRetention(0.6f, 0.0f, 1.0f);
Wrapper gWindSmokeAdvection(0.5f, 0.0f, 2.0f);

Wrapper gSmokeObjectHeight(10.0f, 0.5f, 40.0f);

// Trails (also rendered in game-side TweaksScreenSmokeDeposits column 3)
Wrapper gSmokeTrailsQuantity(800.0f, 0.0f, 2'000.0f);
Wrapper gSmokeTrailsWidthCurrent(0.01f, 0.0f, 0.02f);
Wrapper gSmokeTrailsWidthPrevious(0.01f, 0.0f, 0.02f);
Wrapper gSmokeTrailsLength(3.5f, 0.0f, 10.0f);
Wrapper gSmokeTrailsLengthJitter(0.0f, 0.0f, 10.0f);
Wrapper gSmokeTrailsSideJitter(3.0f, 0.0f, 6.0f);
Wrapper gSmokeIntensityFalloff(3.0f, 0.1f, 10.0f);

} // namespace engine
