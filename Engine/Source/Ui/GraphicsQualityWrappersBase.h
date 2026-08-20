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

// Player-facing quality levels. Each holds a GraphicsQualityLevel as its int64_t value. Water is consumed
// directly by per-frame rendering; the other levels use the Apply functions below to write engine wrappers.
extern engine::Wrapper gWaterLevel;
extern engine::Wrapper gTerrainShadowsLevel;
extern engine::Wrapper gObjectShadowsLevel;
extern engine::Wrapper gLightingLevel;
extern engine::Wrapper gSmokeDetailLevel;

// Write the underlying engine wrappers for one feature from its current level. Call right after the level
// changes: Set() leaves the underlying wrappers' previous value intact, so Graphics::Refresh still sees the
// change on its next poll, and these never call Changed<T>() (that single poll belongs to Refresh).
void ApplyTerrainShadowsLevel();
void ApplyObjectShadowsLevel();
void ApplyLightingLevel();
void ApplySmokeDetailLevel();

void ApplyAllGraphicsQualityLevels();

} // namespace engine

#endif // defined(BT_CLIENT)
