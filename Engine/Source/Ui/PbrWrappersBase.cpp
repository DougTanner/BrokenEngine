#include "PbrWrappersBase.h"

namespace engine
{

Wrapper gPbrSun(0.8f, 0.5f, 1.0f);
Wrapper gPbrDayBrightness(3.0f, 1.0f, 4.0f);
Wrapper gPbrBrdfDiffuse(4.0f, 0.0f, 20.0f);
Wrapper gPbrBrdfDiffusePower(0.795f, 0.5f, 2.0f);
Wrapper gPbrBrdfSpecular(0.822f, 0.0f, 2.0f);
Wrapper gPbrBrdfSpecularPower(0.349f, 0.1f, 2.0f);
Wrapper gPbrExposure(1.0f, 0.0f, 2.0f);
Wrapper gPbrGamma(0.558f, shaders::kfEpsilon, 2.0f); // Nonzero min load-bearing: LightingUniforms.cpp uploads 1.0f / gPbrGamma.mfCurrent as the HdrResolve.frag pow exponent.
Wrapper gColorGradingSaturation(1.207f, 0.0f, 2.0f);
Wrapper gColorGradingContrast(1.205f, 0.5f, 2.0f);
Wrapper gColorGradingTemperature(-0.205f, -1.0f, 1.0f);
Wrapper gPbrLightingSpecular(0.5f, 0.0f, 1.0f);
Wrapper gPbrLightingSpecularPower(0.5f, 0.1f, 1.0f);
Wrapper gPbrLighting(0.2f, 0.0f, 0.3f);
Wrapper gPbrLightingPower(1.5f, 0.5f, 2.0f);
Wrapper gPbrIblAmbient(1.000f, 0.0f, 10.0f);
Wrapper gPbrIblDiffuse(2.008f, 0.0f, 3.0f);
Wrapper gPbrIblDiffusePower(0.811f, 0.5f, 2.0f);
Wrapper gPbrIblSpecular(2.5f, 0.0f, 10.0f);
Wrapper gPbrIblSpecularPower(0.6f, 0.1f, 2.0f);
Wrapper gPbrIblShadowBlend(0.3f, 0.0f, 1.0f);
Wrapper gPbrCubemapLodPower(0.4f, 0.1f, 2.0f);
Wrapper gPbrCubemapLodOffset(0.000f, 0.0f, 100.0f);
Wrapper gPbrShadowFloor(0.3f, 0.0f, 1.0f);
Wrapper gPbrSmoke(0.5f, 0.0f, 1.0f);
Wrapper gPbrEmissive(0.687f, 0.0f, 5.0f);

} // namespace engine
