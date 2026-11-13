#pragma once

#include "WrapperBase.h"

namespace engine
{

inline Wrapper gPbrSun(0.8f, 0.5f, 1.0f);
inline Wrapper gPbrDayBrightness(3.0f, 1.0f, 4.0f);
inline Wrapper gPbrBrdfDiffuse(4.0f, 0.0f, 20.0f);
inline Wrapper gPbrBrdfDiffusePower(0.795f, 0.5f, 2.0f);
inline Wrapper gPbrBrdfSpecular(0.822f, 0.0f, 2.0f);
inline Wrapper gPbrBrdfSpecularPower(0.349f, 0.1f, 2.0f);
inline Wrapper gPbrExposure(1.0f, 0.0f, 2.0f);
inline Wrapper gPbrGamma(0.558f, shaders::kfEpsilon, 2.0f); // Nonzero min load-bearing: LightingUniforms.cpp uploads 1.0f / gPbrGamma.mfCurrent as the HdrResolve.frag pow exponent.
inline Wrapper gColorGradingSaturation(1.207f, 0.0f, 2.0f);
inline Wrapper gColorGradingContrast(1.205f, 0.5f, 2.0f);
inline Wrapper gColorGradingTemperature(-0.205f, -1.0f, 1.0f);
inline Wrapper gPbrLightingSpecular(0.5f, 0.0f, 1.0f);
inline Wrapper gPbrLightingSpecularPower(0.5f, 0.1f, 1.0f);
inline Wrapper gPbrLighting(0.2f, 0.0f, 0.3f);
inline Wrapper gPbrLightingPower(1.5f, 0.5f, 2.0f);
inline Wrapper gPbrIblAmbient(1.000f, 0.0f, 10.0f);
inline Wrapper gPbrIblDiffuse(2.008f, 0.0f, 3.0f);
inline Wrapper gPbrIblDiffusePower(0.811f, 0.5f, 2.0f);
inline Wrapper gPbrIblSpecular(5.0f, 0.0f, 10.0f);
inline Wrapper gPbrIblSpecularPower(0.6f, 0.1f, 2.0f);
inline Wrapper gPbrIblShadowBlend(0.3f, 0.0f, 1.0f);
inline Wrapper gPbrCubemapLodPower(0.4f, 0.1f, 2.0f);
inline Wrapper gPbrCubemapLodOffset(0.000f, 0.0f, 100.0f);
inline Wrapper gPbrShadowFloor(0.3f, 0.0f, 1.0f);
inline Wrapper gPbrSmoke(0.5f, 0.0f, 1.0f);
inline Wrapper gPbrEmissive(0.687f, 0.0f, 5.0f);

} // namespace engine
