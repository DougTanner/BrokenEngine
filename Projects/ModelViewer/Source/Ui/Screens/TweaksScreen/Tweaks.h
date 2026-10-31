#pragma once

#include "Ui/Screens/TweaksScreen/TweaksScreen.h"

namespace game
{

// Called by name from the engine Lighting, Smoke, Wind, and Sound sections. The viewer adds no sub-tabs, so each is empty.
void RenderLightingTweakExtensions(engine::TweaksScreen& rScreen, int64_t iSection, int64_t iFirstSubtab);
void RenderSmokeTweakExtensions(engine::TweaksScreen& rScreen, int64_t iSection, int64_t iFirstSubtab);
void RenderWindTweakExtensions(engine::TweaksScreen& rScreen, int64_t iSection, int64_t iFirstSubtab);
void RenderSoundTweakExtensions(engine::TweaksScreen& rScreen);

} // namespace game
