#if defined(BT_CLIENT)

#include "GraphicsQualityWrappersBase.h"

#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/LightingWrappersBase.h"
#include "Ui/ShadowWrappersBase.h"

namespace engine
{

constexpr int64_t kiLevelCount = static_cast<int64_t>(GraphicsQualityLevel::kCount);

constexpr float kpfTerrainShadowsRenderMultipliers[kiLevelCount] {0.05f, 0.1f, 0.2f};

constexpr float kpfObjectShadowsRenderMultipliers[kiLevelCount] {0.5f, 1.0f, 2.0f};

struct LightingLevelValues
{
	float fDepositTextureMultiplier = 0.0f;
	float fSpreadTextureMultiplierStart = 0.0f;
	float fSpreadTextureMultiplierEnd = 0.0f;
	float fSpreadPassCount = 0.0f;
	float fBlurSampleCount = 0.0f;
};

constexpr LightingLevelValues kpLightingLevels[kiLevelCount]
{
	{.fDepositTextureMultiplier = 0.3f, .fSpreadTextureMultiplierStart = 0.03f, .fSpreadTextureMultiplierEnd = 0.0075f, .fSpreadPassCount = 26.0f, .fBlurSampleCount = 132.0f},
	{.fDepositTextureMultiplier = 0.4f, .fSpreadTextureMultiplierStart = 0.05f, .fSpreadTextureMultiplierEnd = 0.01f, .fSpreadPassCount = 40.0f, .fBlurSampleCount = 200.0f},
	{.fDepositTextureMultiplier = 0.6f, .fSpreadTextureMultiplierStart = 0.075f, .fSpreadTextureMultiplierEnd = 0.015f, .fSpreadPassCount = 40.0f, .fBlurSampleCount = 300.0f},
};

constexpr float kpfSmokeSimulationPixels[kiLevelCount] {0.75f, 1.0f, 1.5f};

constexpr float kpfObjectShadowsBlurMultipliers[] {0.6f, 1.0f};

static int64_t LevelIndex(const engine::Wrapper& rWrapper)
{
	return std::clamp(rWrapper.Get<int64_t>(), 0i64, static_cast<int64_t>(kiLevelCount) - 1);
}

void ApplyTerrainShadowsLevel()
{
	engine::gShadowRenderMultiplier.Set(kpfTerrainShadowsRenderMultipliers[LevelIndex(gTerrainShadowsLevel)]);
}

void ApplyObjectShadowsLevel()
{
	engine::gObjectShadowsRenderMultiplier.Set(kpfObjectShadowsRenderMultipliers[LevelIndex(gObjectShadowsLevel)]);
}

void ApplyLightingLevel()
{
	const LightingLevelValues& rValues = kpLightingLevels[LevelIndex(gLightingLevel)];
	engine::gLightingDepositTextureMultiplier.Set(rValues.fDepositTextureMultiplier);
	engine::gSpreadTextureMultiplierStart.Set(rValues.fSpreadTextureMultiplierStart);
	engine::gSpreadTextureMultiplierEnd.Set(rValues.fSpreadTextureMultiplierEnd);
	engine::gSpreadPassCount.Set(rValues.fSpreadPassCount);
	engine::gLightingBlurSampleCount.Set(rValues.fBlurSampleCount);
}

void ApplySmokeDetailLevel()
{
	engine::gSmokeSimulationPixels.Set(kpfSmokeSimulationPixels[LevelIndex(gSmokeDetailLevel)]);
}

void ApplyObjectShadowsBlurLevel()
{
	engine::gObjectShadowsBlurMultiplier.Set(kpfObjectShadowsBlurMultipliers[gObjectShadowsBlurLevel.Get<int64_t>()]);
}

void ApplyAllGraphicsQualityLevels()
{
	ApplyTerrainShadowsLevel();
	ApplyObjectShadowsLevel();
	ApplyLightingLevel();
	ApplySmokeDetailLevel();
	ApplyObjectShadowsBlurLevel();
}

} // namespace engine

#endif // defined(BT_CLIENT)
