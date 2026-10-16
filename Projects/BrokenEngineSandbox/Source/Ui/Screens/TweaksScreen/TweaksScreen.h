#pragma once

#if defined(BT_CLIENT)

#include "Ui/Screens/TweaksScreen/TweaksScreenBase.h"

namespace game
{

// Startup-assigned dense section index, written by RegisterGameTweakSections(). engine::kiInvalidTweakSection until then.
inline int64_t giTweakSectionParticles = engine::kiInvalidTweakSection;

void RenderParticlesSection(engine::TweaksScreenBase& rScreen);
void RenderSmokeDepositsTab(engine::TweaksScreenBase& rScreen);
void RenderWindDepositsTab(engine::TweaksScreenBase& rScreen);
void RenderLightingEffectsVisibleTab(engine::TweaksScreenBase& rScreen);
void RenderLightingEffectsLightingTab(engine::TweaksScreenBase& rScreen);
void RenderSoundEffects(engine::TweaksScreenBase& rScreen);

// Called by name from the engine Lighting, Smoke, Wind, and Sound sections. Sub-tabs are numbered from iFirstSubtab in the order drawn; Sound draws inline in the engine Volumes sub-tab.
void RenderLightingTweakExtensions(engine::TweaksScreenBase& rScreen, int64_t iSection, int64_t iFirstSubtab);
void RenderSmokeTweakExtensions(engine::TweaksScreenBase& rScreen, int64_t iSection, int64_t iFirstSubtab);
void RenderWindTweakExtensions(engine::TweaksScreenBase& rScreen, int64_t iSection, int64_t iFirstSubtab);
void RenderSoundTweakExtensions(engine::TweaksScreenBase& rScreen);

// Registers the game's tweaks sections. Called from Main.cpp immediately after engine::RegisterEngineTweakSections(),
// before the Graphics ctor builds ImGuiManager -> TweaksScreenBase.
void RegisterGameTweakSections();

} // namespace game

#endif // BT_CLIENT
