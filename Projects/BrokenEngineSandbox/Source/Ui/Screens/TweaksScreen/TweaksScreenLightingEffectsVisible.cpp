#include "TweaksScreen.h"

#include "Ui/Screens/TweaksScreen/TweaksSliderMap.h"

#include "Ui/LightingWrappers.h"

#if defined(BT_CLIENT)

namespace game
{

const engine::TweaksSliderMapRegistrar gLightingEffectsVisibleRegistrar
{
	{"Explosion Primary Visible Area One", &gExplosionPrimaryVisibleAreaOne},
	{"Explosion Primary Visible Area Two", &gExplosionPrimaryVisibleAreaTwo},
	{"Explosion Primary Visible Area Three", &gExplosionPrimaryVisibleAreaThree},
	{"Explosion Primary Visible Intensity One", &gExplosionPrimaryVisibleIntensityOne},
	{"Explosion Primary Visible Intensity Two", &gExplosionPrimaryVisibleIntensityTwo},
	{"Explosion Primary Visible Intensity Three", &gExplosionPrimaryVisibleIntensityThree},
	{"Explosion Secondary Visible Area One", &gExplosionSecondaryVisibleAreaOne},
	{"Explosion Secondary Visible Area Two", &gExplosionSecondaryVisibleAreaTwo},
	{"Explosion Secondary Visible Area Three", &gExplosionSecondaryVisibleAreaThree},
	{"Explosion Secondary Visible Intensity One", &gExplosionSecondaryVisibleIntensityOne},
	{"Explosion Secondary Visible Intensity Two", &gExplosionSecondaryVisibleIntensityTwo},
	{"Explosion Secondary Visible Intensity Three", &gExplosionSecondaryVisibleIntensityThree},
	{"Crater Visible Area One", &gCraterVisibleAreaOne},
	{"Crater Visible Area Two", &gCraterVisibleAreaTwo},
	{"Crater Visible Area Three", &gCraterVisibleAreaThree},
	{"Crater Visible Area Four", &gCraterVisibleAreaFour},
	{"Crater Visible Intensity One", &gCraterVisibleIntensityOne},
	{"Crater Visible Intensity Two", &gCraterVisibleIntensityTwo},
	{"Crater Visible Intensity Three", &gCraterVisibleIntensityThree},
	{"Crater Visible Intensity Four", &gCraterVisibleIntensityFour},
	{"Player Area Light Visible Intensity", &gPlayerAreaLightVisibleIntensity},
	{"Player Impact Visible Area One", &gPlayerImpactVisibleAreaOne},
	{"Player Impact Visible Area Two", &gPlayerImpactVisibleAreaTwo},
	{"Player Impact Visible Intensity One", &gPlayerImpactVisibleIntensityOne},
	{"Player Impact Visible Intensity Two", &gPlayerImpactVisibleIntensityTwo},
	{"Hex Shield Intensity Decay", &gHexShieldIntensityDecay},
	{"Missile Exhaust Visible Intensity", &gMissileExhaustVisibleIntensity},
	{"Enemy Blaster Visible Intensity", &gEnemyBlasterVisibleIntensity},
	{"Hit Flash Visible Area One", &gHitFlashVisibleAreaOne},
	{"Hit Flash Visible Area Two", &gHitFlashVisibleAreaTwo},
	{"Hit Flash Visible Intensity One", &gHitFlashVisibleIntensityOne},
	{"Hit Flash Visible Intensity Two", &gHitFlashVisibleIntensityTwo},
};

void RenderLightingEffectsVisibleTab(engine::TweaksScreenBase& rScreen)
{
	int64_t iSection = engine::giTweakSectionLighting;

	if (ImGui::BeginTable("LightingEffectsVisibleColumns", 2))
	{
		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Explosions - Primary");
		rScreen.WrapperSlider("Visible Area One", iSection, 1.0f, "Explosion Primary Visible Area One");
		rScreen.WrapperSlider("Visible Area Two", iSection, 1.0f, "Explosion Primary Visible Area Two");
		rScreen.WrapperSlider("Visible Area Three", iSection, 1.0f, "Explosion Primary Visible Area Three");
		rScreen.WrapperSlider("Visible Int One", iSection, 1.0f, "Explosion Primary Visible Intensity One");
		rScreen.WrapperSlider("Visible Int Two", iSection, 1.0f, "Explosion Primary Visible Intensity Two");
		rScreen.WrapperSlider("Visible Int Three", iSection, 1.0f, "Explosion Primary Visible Intensity Three");

		rScreen.WrapperSeparatorText("Explosions - Secondary");
		rScreen.WrapperSlider("Visible Area One", iSection, 1.0f, "Explosion Secondary Visible Area One");
		rScreen.WrapperSlider("Visible Area Two", iSection, 1.0f, "Explosion Secondary Visible Area Two");
		rScreen.WrapperSlider("Visible Area Three", iSection, 1.0f, "Explosion Secondary Visible Area Three");
		rScreen.WrapperSlider("Visible Int One", iSection, 1.0f, "Explosion Secondary Visible Intensity One");
		rScreen.WrapperSlider("Visible Int Two", iSection, 1.0f, "Explosion Secondary Visible Intensity Two");
		rScreen.WrapperSlider("Visible Int Three", iSection, 1.0f, "Explosion Secondary Visible Intensity Three");

		rScreen.WrapperSeparatorText("Blasters - Terrain Crater");
		rScreen.WrapperSlider("Visible Area One", iSection, 1.0f, "Crater Visible Area One");
		rScreen.WrapperSlider("Visible Area Two", iSection, 1.0f, "Crater Visible Area Two");
		rScreen.WrapperSlider("Visible Area Three", iSection, 1.0f, "Crater Visible Area Three");
		rScreen.WrapperSlider("Visible Area Four", iSection, 1.0f, "Crater Visible Area Four");
		rScreen.WrapperSlider("Visible Int One", iSection, 1.0f, "Crater Visible Intensity One");
		rScreen.WrapperSlider("Visible Int Two", iSection, 1.0f, "Crater Visible Intensity Two");
		rScreen.WrapperSlider("Visible Int Three", iSection, 1.0f, "Crater Visible Intensity Three");
		rScreen.WrapperSlider("Visible Int Four", iSection, 1.0f, "Crater Visible Intensity Four");

		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Players - Area Light");
		rScreen.WrapperSlider("Visible Intensity", iSection, 1.0f, "Player Area Light Visible Intensity");

		rScreen.WrapperSeparatorText("Players - Impact Light");
		rScreen.WrapperSlider("Visible Area One", iSection, 1.0f, "Player Impact Visible Area One");
		rScreen.WrapperSlider("Visible Area Two", iSection, 1.0f, "Player Impact Visible Area Two");
		rScreen.WrapperSlider("Visible Int One", iSection, 1.0f, "Player Impact Visible Intensity One");
		rScreen.WrapperSlider("Visible Int Two", iSection, 1.0f, "Player Impact Visible Intensity Two");

		rScreen.WrapperSeparatorText("Players - Hex Shield");
		rScreen.WrapperSlider("Intensity Decay", iSection, 1.0f, "Hex Shield Intensity Decay");

		rScreen.WrapperSeparatorText("Missiles - Exhaust");
		rScreen.WrapperSlider("Visible Intensity", iSection, 1.0f, "Missile Exhaust Visible Intensity");

		rScreen.WrapperSeparatorText("Spaceships - Enemy Blaster");
		rScreen.WrapperSlider("Visible Intensity", iSection, 1.0f, "Enemy Blaster Visible Intensity");

		rScreen.WrapperSeparatorText("Spaceships - Hit Flash");
		rScreen.WrapperSlider("Visible Area One", iSection, 1.0f, "Hit Flash Visible Area One");
		rScreen.WrapperSlider("Visible Area Two", iSection, 1.0f, "Hit Flash Visible Area Two");
		rScreen.WrapperSlider("Visible Int One", iSection, 1.0f, "Hit Flash Visible Intensity One");
		rScreen.WrapperSlider("Visible Int Two", iSection, 1.0f, "Hit Flash Visible Intensity Two");

		ImGui::EndTable();
	}
}

} // namespace game

#endif // BT_CLIENT
