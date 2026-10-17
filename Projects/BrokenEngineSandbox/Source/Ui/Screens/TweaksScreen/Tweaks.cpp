#include "Tweaks.h"

#if defined(BT_CLIENT)

namespace game
{

void RegisterGameTweakSections()
{
	engine::TweaksScreen::RegisterSection(giTweakSectionParticles,
	{
		.displayName = "Particles", .stableKey = "Particles",
		.pRender = RenderParticlesSection,
	});
}

void RenderLightingTweakExtensions(engine::TweaksScreen& rScreen, int64_t iSection, int64_t iFirstSubtab)
{
	if (rScreen.BeginSubtab("Visible", iSection, iFirstSubtab))
	{
		RenderLightingEffectsVisibleTab(rScreen);
		ImGui::EndTabItem();
	}
	if (rScreen.BeginSubtab("Lighting", iSection, iFirstSubtab + 1))
	{
		RenderLightingEffectsLightingTab(rScreen);
		ImGui::EndTabItem();
	}
}

void RenderSmokeTweakExtensions(engine::TweaksScreen& rScreen, int64_t iSection, int64_t iFirstSubtab)
{
	if (rScreen.BeginSubtab("Deposits", iSection, iFirstSubtab))
	{
		RenderSmokeDepositsTab(rScreen);
		ImGui::EndTabItem();
	}
}

void RenderWindTweakExtensions(engine::TweaksScreen& rScreen, int64_t iSection, int64_t iFirstSubtab)
{
	if (rScreen.BeginSubtab("Deposits", iSection, iFirstSubtab))
	{
		RenderWindDepositsTab(rScreen);
		ImGui::EndTabItem();
	}
}

void RenderSoundTweakExtensions(engine::TweaksScreen& rScreen)
{
	RenderSoundEffects(rScreen);
}

} // namespace game

#endif // BT_CLIENT
