#pragma once

#if defined(BT_CLIENT)

#include "Ui/Screens/TweaksScreen/TweaksScreenBase.h"

namespace game
{

class TweaksScreen : public engine::TweaksScreenBase
{
public:

	void Render();
};

// Startup-assigned dense section indices, written by RegisterGameTweakSections(). engine::kiInvalidTweakSection until then.
inline int64_t giTweakSectionHexShield = engine::kiInvalidTweakSection;
inline int64_t giTweakSectionParticles = engine::kiInvalidTweakSection;

void RenderHexShieldSection(engine::TweaksScreenBase& rScreen);
void RenderParticlesSection(engine::TweaksScreenBase& rScreen);
void RenderSmokeDepositsTab(engine::TweaksScreenBase& rScreen);
void RenderWindDepositsTab(engine::TweaksScreenBase& rScreen);
void RenderLightingEffectsVisibleTab(engine::TweaksScreenBase& rScreen);
void RenderLightingEffectsLightingTab(engine::TweaksScreenBase& rScreen);
void RenderSoundEffects(engine::TweaksScreenBase& rScreen);

inline constexpr engine::TweakExtensionHook kTweakExtensionHooks[]
{
	{.piSection = &engine::giTweakSectionLighting, .pcLabel = "Visible", .pRender = RenderLightingEffectsVisibleTab},
	{.piSection = &engine::giTweakSectionLighting, .pcLabel = "Lighting", .pRender = RenderLightingEffectsLightingTab},
	{.piSection = &engine::giTweakSectionSound, .pRender = RenderSoundEffects},
	{.piSection = &engine::giTweakSectionSmoke, .pcLabel = "Deposits", .pRender = RenderSmokeDepositsTab},
	{.piSection = &engine::giTweakSectionWind, .pcLabel = "Deposits", .pRender = RenderWindDepositsTab},
};

// Registers the game's tweaks sections. Called from Main.cpp immediately after engine::RegisterEngineTweakSections(),
// before the Graphics ctor builds ImGuiManager -> TweaksScreen.
void RegisterGameTweakSections();

} // namespace game

#endif // BT_CLIENT
