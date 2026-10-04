#include "PbrWrappersBase.h"

namespace engine
{

Wrapper gPhysicallyBasedRenderingSun(0.8f, 0.5f, 1.0f);
Wrapper gPhysicallyBasedRenderingDayBrightness(3.0f, 1.0f, 4.0f);
Wrapper gPhysicallyBasedRenderingModelDataMipmapLevelOfDetailBias(0.0f, -2.0f, 2.0f);
Wrapper gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionDiffuse(10.0f, 0.0f, 20.0f);
Wrapper gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionDiffusePower(0.8f, 0.5f, 1.0f);
Wrapper gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionSpecular(20.0f, 0.0f, 30.0f);
Wrapper gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionSpecularPower(0.30f, 0.1f, 1.0f);
Wrapper gPhysicallyBasedRenderingExposure(0.9f, 0.0f, 2.0f);
Wrapper gPhysicallyBasedRenderingGamma(0.9f, shaders::kfEpsilon, 2.0f); // Nonzero min load-bearing: LightingUniforms.cpp uploads 1.0f / gPhysicallyBasedRenderingGamma.mfCurrent as the HdrResolve.frag pow exponent.
Wrapper gColorGradingSaturation(1.05f, 0.0f, 2.0f);
Wrapper gColorGradingContrast(1.1f, 0.5f, 2.0f);
Wrapper gColorGradingTemperature(-0.2f, -1.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingLightingSpecular(0.5f, 0.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingLightingSpecularPower(0.5f, 0.1f, 1.0f);
Wrapper gPhysicallyBasedRenderingLighting(0.2f, 0.0f, 0.3f);
Wrapper gPhysicallyBasedRenderingLightingPower(1.5f, 0.5f, 2.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingAmbient(0.25f, 0.0f, 0.4f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingDiffuse(1.0f, 0.0f, 3.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingDiffusePower(0.95f, 0.5f, 2.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingSpecular(4.0f, 0.0f, 6.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingSpecularPower(1.0f, 0.5f, 2.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingShadowBlend(0.3f, 0.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingAmbientColorBlend(0.5f, 0.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingCubemapLevelOfDetailPower(1.0f, 0.1f, 1.0f);
Wrapper gPhysicallyBasedRenderingCubemapLevelOfDetailOffset(0.0f, 0.0f, 100.0f);
Wrapper gPhysicallyBasedRenderingShadowFloor(0.3f, 0.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingSmoke(0.5f, 0.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingEmissive(1.0f, 0.0f, 5.0f);

} // namespace engine
