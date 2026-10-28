#pragma once

#if defined(BT_CLIENT)

#include "Ui/Screens/TweaksScreen/TweaksScreen.h"

namespace game
{

// Startup-assigned dense section index, written by RegisterGameTweakSections(). engine::kiInvalidTweakSection until then.
inline int64_t giTweakSectionParticles = engine::kiInvalidTweakSection;

void RenderParticlesSection(engine::TweaksScreen& rScreen);
void RenderSmokeDepositsTab(engine::TweaksScreen& rScreen);
void RenderWindDepositsTab(engine::TweaksScreen& rScreen);
void RenderLightingEffectsVisibleTab(engine::TweaksScreen& rScreen);
void RenderSoundEffects(engine::TweaksScreen& rScreen);

// Called by name from the engine Lighting, Smoke, Wind, and Sound sections. Sub-tabs are numbered from iFirstSubtab in the order drawn; Sound draws inline in the engine Volumes sub-tab.
void RenderLightingTweakExtensions(engine::TweaksScreen& rScreen, int64_t iSection, int64_t iFirstSubtab);
void RenderSmokeTweakExtensions(engine::TweaksScreen& rScreen, int64_t iSection, int64_t iFirstSubtab);
void RenderWindTweakExtensions(engine::TweaksScreen& rScreen, int64_t iSection, int64_t iFirstSubtab);
void RenderSoundTweakExtensions(engine::TweaksScreen& rScreen);

// Registers the game's tweaks sections. Called from Main.cpp immediately after engine::RegisterEngineTweakSections(),
// before the Graphics ctor builds ImGuiManager -> TweaksScreen.
void RegisterGameTweakSections();

} // namespace game

#endif // BT_CLIENT
