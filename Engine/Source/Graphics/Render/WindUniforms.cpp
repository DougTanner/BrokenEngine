#if defined(BT_CLIENT)

#include "Ui/SmokeWrappersBase.h"
#include "Ui/WindWrappersBase.h"
#include "Render.h"

namespace engine
{

void RenderWindGlobal(int64_t iCommandBuffer)
{
	// Accumulate wind time (always tick to avoid delta spikes after toggle)
	static common::Timer sWindTimer;
	static float sfWindTime = 0.0f;
	float fDeltaTime = common::NanosecondsToFloatSeconds<float>(sWindTimer.GetDeltaNs(true));
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

	uint32_t uiWindWidth = gpTextureManager->mRenderTargetTextures.mWindTextureOne.mInfo.vkExtent3D.width;
	uint32_t uiWindHeight = gpTextureManager->mRenderTargetTextures.mWindTextureOne.mInfo.vkExtent3D.height;
	uint32_t uiWindTilesX = TileCount(uiWindWidth);
	uint32_t uiWindTilesY = TileCount(uiWindHeight);
	rGlobalLayout.uiWindTilesX = uiWindTilesX;
	rGlobalLayout.uiWindTilesY = uiWindTilesY;
	rGlobalLayout.f2WindTilesInverse.x = 1.0f / static_cast<float>(uiWindTilesX);
	rGlobalLayout.f2WindTilesInverse.y = 1.0f / static_cast<float>(uiWindTilesY);

	giWindTextureIndex = 1 - giWindTextureIndex;

	rGlobalLayout.fWindTextureIndex = static_cast<float>(giWindTextureIndex);

	// Wind shares smoke's f4SmokeArea / f4PreviousSmokeArea (already populated by RenderSmokeGlobal earlier in
	// the frame per the global-pass ordering contract); RenderWindGlobal does not write them.
}

} // namespace engine

#endif // defined(BT_CLIENT)
