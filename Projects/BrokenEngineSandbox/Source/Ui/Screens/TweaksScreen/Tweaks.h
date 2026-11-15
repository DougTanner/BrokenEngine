#pragma once

#if defined(BT_CLIENT)

#include "Ui/Screens/TweaksScreen/TweaksScreen.h"

namespace game
{

// Startup-assigned dense section indices, written by RegisterGameTweakSections(). engine::kiInvalidTweakSection until then.
inline int64_t giTweakSectionParticles = engine::kiInvalidTweakSection;
inline int64_t giTweakSectionMap = engine::kiInvalidTweakSection;

void RenderParticlesSection(engine::TweaksScreen& rScreen);
void RenderMapSection(engine::TweaksScreen& rScreen);
void RenderSmokeDepositsTab(engine::TweaksScreen& rScreen);
void RenderWindDepositsTab(engine::TweaksScreen& rScreen);
void RenderLightingEffectsVisibleTab(engine::TweaksScreen& rScreen);
void RenderLightingEffectsLightingTab(engine::TweaksScreen& rScreen);
void RenderSoundEffects(engine::TweaksScreen& rScreen);

// Registers the game's tweaks sections. Called from Main.cpp immediately after engine::RegisterEngineTweakSections(),
// before the Graphics ctor builds ImGuiManager -> TweaksScreen.
void RegisterGameTweakSections();

} // namespace game

#endif // BT_CLIENT
