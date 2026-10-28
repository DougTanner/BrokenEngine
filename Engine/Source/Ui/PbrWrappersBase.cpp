#include "PbrWrappersBase.h"

namespace engine
{

Wrapper gPhysicallyBasedRenderingSun(0.8f, 0.5f, 1.0f);
Wrapper gPhysicallyBasedRenderingDayBrightness(3.0f, 1.0f, 4.0f);
Wrapper gPhysicallyBasedRenderingModelDataMipmapLevelOfDetailBias(-0.17f, -2.0f, 2.0f);
Wrapper gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionDiffuse(8.566f, 0.0f, 20.0f);
Wrapper gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionDiffusePower(0.8f, 0.5f, 2.0f);
Wrapper gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionSpecular(5.964f, 0.0f, 30.0f);
Wrapper gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionSpecularPower(0.559f, 0.1f, 2.0f);
Wrapper gPhysicallyBasedRenderingExposure(0.924f, 0.0f, 2.0f);
Wrapper gPhysicallyBasedRenderingGamma(0.558f, shaders::kfEpsilon, 2.0f); // Nonzero min load-bearing: LightingUniforms.cpp uploads 1.0f / gPhysicallyBasedRenderingGamma.mfCurrent as the HdrResolve.frag pow exponent.
Wrapper gColorGradingSaturation(1.207f, 0.0f, 2.0f);
Wrapper gColorGradingContrast(1.205f, 0.5f, 2.0f);
Wrapper gColorGradingTemperature(-0.205f, -1.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingAmbient(0.809f, 0.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingDiffuse(1.254f, 0.0f, 3.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingDiffusePower(0.759f, 0.5f, 2.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingSpecular(8.064f, 2.0f, 10.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingSpecularPower(0.812f, 0.5f, 2.0f);
Wrapper gPhysicallyBasedRenderingImageBasedLightingShadowBlend(0.3f, 0.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingCubemapLevelOfDetailPower(1.139f, 0.1f, 2.0f);
Wrapper gPhysicallyBasedRenderingCubemapLevelOfDetailOffset(10.159f, 0.0f, 100.0f);
Wrapper gPhysicallyBasedRenderingShadowFloor(0.3f, 0.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingSmoke(0.5f, 0.0f, 1.0f);
Wrapper gPhysicallyBasedRenderingEmissive(1.014f, 0.0f, 5.0f);

} // namespace engine
