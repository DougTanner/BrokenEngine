#pragma once

#include "WrapperBase.h"

namespace engine
{

extern Wrapper gShadowRenderMultiplier;
extern Wrapper gShadowTexelRampMetersPerSecond;
extern Wrapper gShadowTemporalBlend;
extern Wrapper gShadowUpdateCadence;

extern Wrapper gShadowFeatherNoon;
extern Wrapper gShadowFeatherNoonOffset;
extern Wrapper gShadowFeatherSunset;
extern Wrapper gShadowFeatherSunsetOffset;
extern Wrapper gShadowFeatherPower;
extern Wrapper gShadowDistanceFalloff;
extern Wrapper gShadowBlurSigma;
extern Wrapper gShadowAffectAmbient;
extern Wrapper gShadowHeightFadeTop;
extern Wrapper gShadowHeightFadeBottom;

extern Wrapper gObjectShadowsRenderMultiplier;
extern Wrapper gObjectShadowsBlurMultiplier;
extern Wrapper gObjectShadowsNoon;
extern Wrapper gObjectShadowsSunset;
extern Wrapper gObjectShadowsSunsetStretch;
extern Wrapper gObjectShadowsBlurSigmaLow;
extern Wrapper gObjectShadowsBlurSigmaHigh;
extern Wrapper gObjectShadowsBlurRadiusNoon;
extern Wrapper gObjectShadowsBlurRadiusSunset;
extern Wrapper gObjectShadowsTerrainShadowFade;
inline Wrapper gSmokeShadowIntensityBottom(1.0f, 0.0f, 1.0f);
inline Wrapper gSmokeShadowIntensityTop(0.7f, 0.0f, 1.0f);

} // namespace engine
