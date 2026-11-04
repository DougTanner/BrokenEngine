#include "PbrWrappersBase.h"

namespace engine
{

Wrapper gPhysicallyBasedRenderingSun(0.8f, 0.5f, 1.0f);
Wrapper gPhysicallyBasedRenderingDayBrightness(3.0f, 1.0f, 4.0f);
Wrapper gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionDiffuse(15.018f, 0.0f, 20.0f);
Wrapper gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionDiffusePower(0.795f, 0.5f, 2.0f);
Wrapper gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionSpecular(0.822f, 0.0f, 2.0f);
Wrapper gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionSpecularPower(0.349f, 0.1f, 2.0f);
Wrapper gPhysicallyBasedRenderingExposure(0.924f, 0.0f, 2.0f);
Wrapper gPhysicallyBasedRenderingGamma(0.558f, shaders::kfEpsilon, 2.0f); // Nonzero min load-bearing: LightingUniforms.cpp uploads 1.0f / gPhysicallyBasedRenderingGamma.mfCurrent as the HdrResolve.frag pow exponent.
Wrapper gColorGradingSaturation(1.207f, 0.0f, 2.0f);
Wrapper gColorGradingContrast(1.205f, 0.5f, 2.0f);
Wrapper gColorGradingTemperature(-0.205f, -1.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingLightingSpecular(0.5f, 0.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingLightingSpecularPower(0.5f, 0.1f, 1.0f);
Wrapper gPhysicallyBasedRenderingLighting(0.2f, 0.0f, 0.3f);
Wrapper gPhysicallyBasedRenderingLightingPower(1.5f, 0.5f, 2.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingAmbient(1.000f, 0.0f, 10.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingDiffuse(2.008f, 0.0f, 3.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingDiffusePower(0.811f, 0.5f, 2.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingSpecular(7.088f, 2.0f, 10.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingSpecularPower(0.704f, 0.5f, 2.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingShadowBlend(0.3f, 0.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingCubemapLevelOfDetailPower(0.803f, 0.1f, 2.0f);
Wrapper gPhysicallyBasedRenderingCubemapLevelOfDetailOffset(0.000f, 0.0f, 100.0f);
Wrapper gPhysicallyBasedRenderingShadowFloor(0.3f, 0.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingSmoke(0.5f, 0.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingEmissive(0.687f, 0.0f, 5.0f);

} // namespace engine
