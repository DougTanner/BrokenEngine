#if defined(BT_CLIENT)

#include "GraphicsQualityWrappersBase.h"

#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/ShadowWrappersBase.h"

namespace engine
{

// int64_t 0/1/2 == GraphicsQualityLevel kLow/kMedium/kHigh
engine::Wrapper gWaterLevel(2i64, std::vector<int64_t> {0, 1, 2});
engine::Wrapper gTerrainShadowsLevel(1i64, std::vector<int64_t> {0, 1, 2});
engine::Wrapper gObjectShadowsLevel(1i64, std::vector<int64_t> {0, 1, 2});
engine::Wrapper gSmokeDetailLevel(1i64, std::vector<int64_t> {0, 1, 2});

constexpr int64_t kiLevelCount = static_cast<int64_t>(GraphicsQualityLevel::kCount);

constexpr float kpfTerrainShadowsRenderMultipliers[kiLevelCount] {0.05f, 0.1f, 0.2f};

constexpr float kpfObjectShadowsRenderMultipliers[kiLevelCount] {0.5f, 1.0f, 2.0f};

constexpr float kpfSmokeSimulationPixels[kiLevelCount] {0.75f, 1.0f, 1.5f};

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

void ApplySmokeDetailLevel()
{
	engine::gSmokeSimulationPixels.Set(kpfSmokeSimulationPixels[LevelIndex(gSmokeDetailLevel)]);
}

void ApplyAllGraphicsQualityLevels()
{
	ApplyTerrainShadowsLevel();
	ApplyObjectShadowsLevel();
	ApplySmokeDetailLevel();
}

} // namespace engine

#endif // defined(BT_CLIENT)
