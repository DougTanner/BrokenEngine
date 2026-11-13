#pragma once

#if defined(BT_CLIENT)

#include "Ui/WrapperBase.h"

namespace engine
{

enum class GraphicsQualityLevel : uint8_t
{
	kLow,
	kMedium,
	kHigh,

	kCount,
};

// Player-facing quality levels. Each three-level wrapper holds a GraphicsQualityLevel as its int64_t value. Water is
// consumed directly by per-frame rendering; the other levels use the Apply functions below to write engine wrappers.
inline engine::Wrapper gWaterLevel(2i64, std::vector<int64_t> {0, 1, 2});
inline engine::Wrapper gTerrainShadowsLevel(1i64, std::vector<int64_t> {0, 1, 2});
inline engine::Wrapper gObjectShadowsLevel(1i64, std::vector<int64_t> {0, 1, 2});
inline engine::Wrapper gLightingLevel(1i64, std::vector<int64_t> {0, 1, 2});
inline engine::Wrapper gSmokeDetailLevel(1i64, std::vector<int64_t> {0, 1, 2});
// Two levels only: 0 is Low, 1 is High.
inline engine::Wrapper gObjectShadowsBlurLevel(0i64, std::vector<int64_t> {0, 1});

// Write the underlying engine wrappers for one feature from its current level. Call right after the level
// changes: Set() leaves the underlying wrappers' previous value intact, so Graphics::Refresh still sees the
// change on its next poll, and these never call Changed<T>() (that single poll belongs to Refresh).
void ApplyTerrainShadowsLevel();
void ApplyObjectShadowsLevel();
void ApplyLightingLevel();
void ApplySmokeDetailLevel();
void ApplyObjectShadowsBlurLevel();

void ApplyAllGraphicsQualityLevels();

} // namespace engine

#endif // defined(BT_CLIENT)
