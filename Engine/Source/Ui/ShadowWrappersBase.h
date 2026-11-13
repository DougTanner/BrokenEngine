#pragma once

#include "WrapperBase.h"

namespace engine
{

inline Wrapper gShadowRenderMultiplier(0.1f, 0.05f, 1.0f);
inline Wrapper gShadowTexelRampMetersPerSecond(150.0f, 10.0f, 2'000.0f);
inline Wrapper gShadowTemporalBlend(0.2f, 0.05f, 1.0f);
inline Wrapper gShadowUpdateCadence(4.0f, 1.0f, 4.0f, 1.0f);

inline Wrapper gShadowFeatherNoon(2.5f, 0.0f, 8.0f);
inline Wrapper gShadowFeatherNoonOffset(2.1f, 2.1f, 5.0f);
inline Wrapper gShadowFeatherSunset(0.1f, 0.0f, 0.5f);
inline Wrapper gShadowFeatherSunsetOffset(-0.1f, -0.5f, 0.1f);
inline Wrapper gShadowFeatherPower(1.3f, 0.1f, 10.0f);
inline Wrapper gShadowDistanceFalloff(200.0f, 10.0f, 400.0f);
inline Wrapper gShadowBlurSigma(1.0f, 0.1f, 5.0f);
inline Wrapper gShadowAffectAmbient(0.7f, 0.0f, 1.0f);
inline Wrapper gShadowHeightFadeTop(5.0f, 0.0f, 20.0f);
inline Wrapper gShadowHeightFadeBottom(-2.0f, -30.0f, 0.0f);

inline Wrapper gObjectShadowsRenderMultiplier(0.8f, 0.25f, 4.0f);
inline Wrapper gObjectShadowsBlurMultiplier(0.6f, 0.125f, 1.0f);
inline Wrapper gObjectShadowCadence(2.0f, 1.0f, 4.0f, 1.0f);
inline Wrapper gObjectShadowsNoon(0.9f, 0.1f, 1.0f);
inline Wrapper gObjectShadowsSunset(0.45f, 0.0f, 1.0f);
inline Wrapper gObjectShadowsSunsetStretch(2.0f, 0.0f, 10.0f);
inline Wrapper gObjectShadowsStretchPower(2.0f, 0.25f, 4.0f);
inline Wrapper gObjectShadowsBlurSigmaLow(20.0f, 1.0f, 40.0f);
inline Wrapper gObjectShadowsBlurSigmaHigh(5.0f, 0.1f, 10.0f);
inline Wrapper gObjectShadowsBlurRadiusNoon(10.0f, 1.0f, static_cast<float>(shaders::kiMaxObjectShadowsBlurRadius), 1.0f); // Max load-bearing: sizes GlobalLayout pf4ObjectShadowsBlurWeights.
inline Wrapper gObjectShadowsBlurRadiusSunset(30.0f, 1.0f, static_cast<float>(shaders::kiMaxObjectShadowsBlurRadius), 1.0f); // Max load-bearing: sizes GlobalLayout pf4ObjectShadowsBlurWeights.
inline Wrapper gObjectShadowsTerrainShadowFade(30.0f, 1.0f, 200.0f); // min >= 1.0f load-bearing: keeps the folded fObjectShadowsTerrainShadowFadeInverse finite.
inline Wrapper gObjectShadowsUnderwaterFade(0.01f, 0.005f, 1.0f); // Positive minimum keeps fObjectShadowsUnderwaterFadeInverse finite.
inline Wrapper gSmokeShadowIntensityBottom(1.0f, 0.0f, 1.0f);
inline Wrapper gSmokeShadowIntensityTop(0.7f, 0.0f, 1.0f);

} // namespace engine
