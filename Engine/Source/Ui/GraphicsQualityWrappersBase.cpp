#if defined(BT_CLIENT)

#include "GraphicsQualityWrappersBase.h"

#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/LightingWrappersBase.h"
#include "Ui/ShadowWrappersBase.h"

namespace engine
{

// int64_t 0/1/2 == GraphicsQualityLevel kLow/kMedium/kHigh
engine::Wrapper gWaterLevel(2i64, std::vector<int64_t> {0, 1, 2});
engine::Wrapper gTerrainShadowsLevel(1i64, std::vector<int64_t> {0, 1, 2});
engine::Wrapper gObjectShadowsLevel(1i64, std::vector<int64_t> {0, 1, 2});
engine::Wrapper gLightingLevel(1i64, std::vector<int64_t> {0, 1, 2});
engine::Wrapper gSmokeDetailLevel(1i64, std::vector<int64_t> {0, 1, 2});

constexpr int64_t kiLevelCount = static_cast<int64_t>(GraphicsQualityLevel::kCount);

constexpr float kfTerrainShadowsRenderMultipliers[kiLevelCount] {0.05f, 0.1f, 0.2f};

constexpr float kfObjectShadowsRenderMultipliers[kiLevelCount] {0.5f, 1.0f, 2.0f};

struct LightingLevelValues
{
	float fDepositTextureMultiplier = 0.0f;
	float fSpreadTextureMultiplierStart = 0.0f;
	float fSpreadTextureMultiplierEnd = 0.0f;
	float fSpreadPassCount = 0.0f;
	float fBlurSampleCount = 0.0f;
};

constexpr LightingLevelValues kLightingLevels[kiLevelCount]
{
	{.fDepositTextureMultiplier = 0.3f, .fSpreadTextureMultiplierStart = 0.03f, .fSpreadTextureMultiplierEnd = 0.0075f, .fSpreadPassCount = 26.0f, .fBlurSampleCount = 132.0f, },
	{.fDepositTextureMultiplier = 0.4f, .fSpreadTextureMultiplierStart = 0.05f, .fSpreadTextureMultiplierEnd = 0.01f, .fSpreadPassCount = 40.0f, .fBlurSampleCount = 200.0f, },
	{.fDepositTextureMultiplier = 0.6f, .fSpreadTextureMultiplierStart = 0.075f, .fSpreadTextureMultiplierEnd = 0.015f, .fSpreadPassCount = 40.0f, .fBlurSampleCount = 300.0f, },
};

constexpr float kfSmokeSimulationPixels[kiLevelCount] {0.75f, 1.0f, 1.5f};

static int64_t LevelIndex(const engine::Wrapper& rWrapper)
{
	return std::clamp(rWrapper.Get<int64_t>(), 0i64, static_cast<int64_t>(kiLevelCount) - 1);
}

void ApplyTerrainShadowsLevel()
{
	engine::gShadowRenderMultiplier.Set(kfTerrainShadowsRenderMultipliers[LevelIndex(gTerrainShadowsLevel)]);
}

void ApplyObjectShadowsLevel()
{
	engine::gObjectShadowsRenderMultiplier.Set(kfObjectShadowsRenderMultipliers[LevelIndex(gObjectShadowsLevel)]);
}

void ApplyLightingLevel()
{
	const LightingLevelValues& rValues = kLightingLevels[LevelIndex(gLightingLevel)];
	engine::gLightingDepositTextureMultiplier.Set(rValues.fDepositTextureMultiplier);
	engine::gSpreadTextureMultiplierStart.Set(rValues.fSpreadTextureMultiplierStart);
	engine::gSpreadTextureMultiplierEnd.Set(rValues.fSpreadTextureMultiplierEnd);
	engine::gSpreadPassCount.Set(rValues.fSpreadPassCount);
	engine::gLightingBlurSampleCount.Set(rValues.fBlurSampleCount);
}

void ApplySmokeDetailLevel()
{
	engine::gSmokeSimulationPixels.Set(kfSmokeSimulationPixels[LevelIndex(gSmokeDetailLevel)]);
}

void ApplyAllGraphicsQualityLevels()
{
	ApplyTerrainShadowsLevel();
	ApplyObjectShadowsLevel();
	ApplyLightingLevel();
	ApplySmokeDetailLevel();
}

} // namespace engine

#endif // defined(BT_CLIENT)
