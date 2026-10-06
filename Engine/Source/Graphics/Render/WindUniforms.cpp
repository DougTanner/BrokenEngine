#if defined(BT_CLIENT)

#include "Ui/SmokeWrappersBase.h"
#include "Ui/WindWrappersBase.h"
#include "Render.h"

namespace engine
{

void RenderWindGlobal(int64_t iCommandBuffer)
{
	// Step from the Graphics frame delta, which the skipped-render resume resets so a minimize does not land as one step.
	static float sfWindTime = 0.0f;
	float fDeltaTime = common::NanosecondsToFloatSeconds<float>(gpGraphics->mRenderFrameDeltaNanoseconds);
	sfWindTime += fDeltaTime * gWindTimeScale.mfCurrent;

	shaders::GlobalLayout& rGlobalLayout = *reinterpret_cast<shaders::GlobalLayout*>(&gpBufferManager->mGlobalLayoutUniformBuffers.at(iCommandBuffer).mpMappedMemory[0]);

	rGlobalLayout.fWindAdvectionScaleHigh = gWindAdvectionScaleHigh.mfCurrent;
	rGlobalLayout.fWindAdvectionScaleLow = gWindAdvectionScaleLow.mfCurrent;
	rGlobalLayout.fWindSwirlScaleHigh = gWindSwirlScaleHigh.mfCurrent;
	rGlobalLayout.fWindSwirlScaleLow = gWindSwirlScaleLow.mfCurrent;
	rGlobalLayout.fWindSwirlAmountHigh = gWindSwirlAmountHigh.mfCurrent;
	rGlobalLayout.fWindSwirlAmountLow = gWindSwirlAmountLow.mfCurrent;
	rGlobalLayout.fWindSwirlSpeedHigh = gWindSwirlSpeedHigh.mfCurrent;
	rGlobalLayout.fWindSwirlSpeedLow = gWindSwirlSpeedLow.mfCurrent;
	rGlobalLayout.fWindVorticityConfinementHigh = gWindVorticityConfinementHigh.mfCurrent;
	rGlobalLayout.fWindVorticityConfinementLow = gWindVorticityConfinementLow.mfCurrent;
	rGlobalLayout.fWindDecayHigh = gWindDecayHigh.mfCurrent;
	rGlobalLayout.fWindDecayLow = gWindDecayLow.mfCurrent;
	rGlobalLayout.fWindMomentumHigh = gWindMomentumHigh.mfCurrent;
	rGlobalLayout.fWindMomentumLow = gWindMomentumLow.mfCurrent;
	rGlobalLayout.fWindThresholdLow = gWindThresholdLow.mfCurrent;
	rGlobalLayout.fWindThresholdHigh = gWindThresholdHigh.mfCurrent;
	rGlobalLayout.fWindToSmokeStrength = gWindToSmokeStrength.mfCurrent;
	// Wind shader params are tuned against a 60 fps step — normalize the per-frame delta to that reference rate.
	static constexpr float kfWindReferenceFramesPerSecond = 60.0f;
	rGlobalLayout.fWindTimeScale = fDeltaTime * kfWindReferenceFramesPerSecond * gWindTimeScale.mfCurrent;
	rGlobalLayout.fWindTime = sfWindTime;
	rGlobalLayout.fWindSmokeRetention = gWindSmokeRetention.mfCurrent;
	rGlobalLayout.fWindToSmokePower = gWindToSmokePower.mfCurrent;
	rGlobalLayout.fWindDiffusionHigh = gWindDiffusionHigh.mfCurrent;
	rGlobalLayout.fWindDiffusionLow = gWindDiffusionLow.mfCurrent;

	rGlobalLayout.fWindDisplacementNoiseScale = gWindDisplacementNoiseScale.mfCurrent;
	rGlobalLayout.fWindSmokeAdvection = gWindSmokeAdvection.mfCurrent;

	int64_t iWindWidth = gpTextureManager->mRenderTargetTextures.mWindTextureOne.mInfo.vkExtent3D.width;
	int64_t iWindHeight = gpTextureManager->mRenderTargetTextures.mWindTextureOne.mInfo.vkExtent3D.height;
	int64_t iWindTilesX = TileCount(iWindWidth);
	int64_t iWindTilesY = TileCount(iWindHeight);
	rGlobalLayout.uiWindTilesX = static_cast<uint32_t>(iWindTilesX);
	rGlobalLayout.uiWindTilesY = static_cast<uint32_t>(iWindTilesY);
	rGlobalLayout.f2WindTilesInverse.x = 1.0f / static_cast<float>(iWindTilesX);
	rGlobalLayout.f2WindTilesInverse.y = 1.0f / static_cast<float>(iWindTilesY);

	giWindTextureIndex = 1 - giWindTextureIndex;

	rGlobalLayout.fWindTextureIndex = static_cast<float>(giWindTextureIndex);

	// Wind shares smoke's f4SmokeArea / f4PreviousSmokeArea (already populated by RenderSmokeGlobal earlier in
	// the frame per the global-pass ordering contract); RenderWindGlobal does not write them.
}

} // namespace engine

#endif // defined(BT_CLIENT)
