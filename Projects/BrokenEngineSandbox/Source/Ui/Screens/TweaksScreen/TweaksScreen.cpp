#include "TweaksScreen.h"

#include "Game.h"

#if defined(BT_CLIENT)

namespace game
{

void TweaksScreen::Render()
{
	if constexpr (kbDebugInput)
	{
		if (!game::gpGame->mbShowImGui)
		{
			return;
		}
	}

	TweaksScreenBase::Render();
}

void RegisterGameTweakSections()
{
	engine::TweaksScreenBase::RegisterSection(giTweakSectionHexShield,
	{
		.displayName = "Hex Shield", .stableKey = "HexShield",
		.pRender = RenderHexShieldSection,
	});
	engine::TweaksScreenBase::RegisterSection(giTweakSectionParticles,
	{
		.displayName = "Particles", .stableKey = "Particles",
		.pRender = RenderParticlesSection,
	});
}

} // namespace game

#endif // BT_CLIENT
