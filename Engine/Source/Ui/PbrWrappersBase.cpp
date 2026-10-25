#include "PbrWrappersBase.h"

namespace engine
{

Wrapper gPhysicallyBasedRenderingSun(0.8f, 0.5f, 1.0f);
Wrapper gPhysicallyBasedRenderingDayBrightness(3.0f, 1.0f, 4.0f);
Wrapper gPhysicallyBasedRenderingModelDataMipmapLevelOfDetailBias(-0.615f, -2.0f, 2.0f);
Wrapper gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionDiffuse(8.951f, 0.0f, 20.0f);
Wrapper gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionDiffusePower(0.891f, 0.5f, 2.0f);
Wrapper gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionSpecular(16.731f, 0.0f, 30.0f);
Wrapper gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionSpecularPower(1.279f, 0.1f, 2.0f);
Wrapper gPhysicallyBasedRenderingExposure(0.85f, 0.0f, 2.0f);
Wrapper gPhysicallyBasedRenderingGamma(0.6f, shaders::kfEpsilon, 2.0f); // Nonzero min load-bearing: LightingUniforms.cpp uploads 1.0f / gPhysicallyBasedRenderingGamma.mfCurrent as the HdrResolve.frag pow exponent.
Wrapper gColorGradingSaturation(1.2f, 0.0f, 2.0f);
Wrapper gColorGradingContrast(1.2f, 0.5f, 2.0f);
Wrapper gColorGradingTemperature(0.0f, -1.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingLightingSpecular(0.5f, 0.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingLightingSpecularPower(0.5f, 0.1f, 1.0f);
Wrapper gPhysicallyBasedRenderingLighting(0.2f, 0.0f, 0.3f);
Wrapper gPhysicallyBasedRenderingLightingPower(1.5f, 0.5f, 2.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingAmbient(0.136f, 0.0f, 0.4f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingDiffuse(0.81f, 0.0f, 3.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingDiffusePower(0.845f, 0.5f, 2.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingSpecular(4.007f, 0.0f, 6.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingSpecularPower(1.124f, 0.5f, 2.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingShadowBlend(0.3f, 0.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingAmbientColorBlend(0.321f, 0.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingCubemapLevelOfDetailPower(0.505f, 0.1f, 2.0f);
Wrapper gPhysicallyBasedRenderingCubemapLevelOfDetailOffset(0.0f, 0.0f, 100.0f);
Wrapper gPhysicallyBasedRenderingShadowFloor(0.3f, 0.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingSmoke(0.5f, 0.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingEmissive(1.367f, 0.0f, 5.0f);

} // namespace engine
