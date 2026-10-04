#include "Ui/Screens/TweaksScreen/TweaksSliderMap.h"

#include "Ui/LightingWrappers.h"
#include "TweaksScreen.h"

#if defined(BT_CLIENT)

namespace game
{

const engine::TweaksSliderMapRegistrar gLightingEffectsLightingRegistrar
{
	// Explosion Primary Lighting
	{"Explosion Primary Lighting Area One", &gExplosionPrimaryLightingAreaOne},
	{"Explosion Primary Lighting Area Two", &gExplosionPrimaryLightingAreaTwo},
	{"Explosion Primary Lighting Area Three", &gExplosionPrimaryLightingAreaThree},
	{"Explosion Primary Lighting Intensity One", &gExplosionPrimaryLightingIntensityOne},
	{"Explosion Primary Lighting Intensity Two", &gExplosionPrimaryLightingIntensityTwo},
	{"Explosion Primary Lighting Intensity Three", &gExplosionPrimaryLightingIntensityThree},
	// Explosion Secondary Lighting
	{"Explosion Secondary Lighting Area One", &gExplosionSecondaryLightingAreaOne},
	{"Explosion Secondary Lighting Area Two", &gExplosionSecondaryLightingAreaTwo},
	{"Explosion Secondary Lighting Area Three", &gExplosionSecondaryLightingAreaThree},
	{"Explosion Secondary Lighting Intensity One", &gExplosionSecondaryLightingIntensityOne},
	{"Explosion Secondary Lighting Intensity Two", &gExplosionSecondaryLightingIntensityTwo},
	{"Explosion Secondary Lighting Intensity Three", &gExplosionSecondaryLightingIntensityThree},
	// Crater Lighting
	{"Crater Lighting Area One", &gCraterLightingAreaOne},
	{"Crater Lighting Area Two", &gCraterLightingAreaTwo},
	{"Crater Lighting Area Three", &gCraterLightingAreaThree},
	{"Crater Lighting Area Four", &gCraterLightingAreaFour},
	{"Crater Lighting Intensity One", &gCraterLightingIntensityOne},
	{"Crater Lighting Intensity Two", &gCraterLightingIntensityTwo},
	{"Crater Lighting Intensity Three", &gCraterLightingIntensityThree},
	{"Crater Lighting Intensity Four", &gCraterLightingIntensityFour},
	// Players
	{"Player Area Light Lighting Size", &gPlayerBlasterLightingArea},
	{"Player Area Light Lighting Intensity", &gPlayerBlasterLightingIntensity},
	{"Player Impact Lighting Area One", &gPlayerImpactLightingAreaOne},
	{"Player Impact Lighting Area Two", &gPlayerImpactLightingAreaTwo},
	{"Player Impact Lighting Intensity One", &gPlayerImpactLightingIntensityOne},
	{"Player Impact Lighting Intensity Two", &gPlayerImpactLightingIntensityTwo},
	// Hex Shield
	{"Hex Shield Lighting Intensity", &gHexShieldLightingIntensity},
	// Missiles
	{"Missile Exhaust Lighting Area", &gMissileExhaustLightingArea},
	{"Missile Exhaust Lighting Intensity", &gMissileExhaustLightingIntensity},
	// Spaceships
	{"Enemy Blaster Lighting Area", &gEnemyBlasterLightingArea},
	{"Enemy Blaster Lighting Intensity", &gEnemyBlasterLightingIntensity},
	{"Hit Flash Lighting Area One", &gHitFlashLightingAreaOne},
	{"Hit Flash Lighting Area Two", &gHitFlashLightingAreaTwo},
	{"Hit Flash Lighting Intensity One", &gHitFlashLightingIntensityOne},
	{"Hit Flash Lighting Intensity Two", &gHitFlashLightingIntensityTwo},
};

void RenderLightingEffectsLightingTab(engine::TweaksScreenBase& rScreen)
{
	int64_t iSection = engine::giTweakSectionLighting;

	if (ImGui::BeginTable("LightingEffectsLightingColumns", 2))
	{
		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Explosions - Primary");
		rScreen.WrapperSlider("Lighting Area One", iSection, 1.0f, "Explosion Primary Lighting Area One");
		rScreen.WrapperSlider("Lighting Area Two", iSection, 1.0f, "Explosion Primary Lighting Area Two");
		rScreen.WrapperSlider("Lighting Area Three", iSection, 1.0f, "Explosion Primary Lighting Area Three");
		rScreen.WrapperSlider("Lighting Int One", iSection, 1.0f, "Explosion Primary Lighting Intensity One");
		rScreen.WrapperSlider("Lighting Int Two", iSection, 1.0f, "Explosion Primary Lighting Intensity Two");
		rScreen.WrapperSlider("Lighting Int Three", iSection, 1.0f, "Explosion Primary Lighting Intensity Three");

		rScreen.WrapperSeparatorText("Explosions - Secondary");
		rScreen.WrapperSlider("Lighting Area One", iSection, 1.0f, "Explosion Secondary Lighting Area One");
		rScreen.WrapperSlider("Lighting Area Two", iSection, 1.0f, "Explosion Secondary Lighting Area Two");
		rScreen.WrapperSlider("Lighting Area Three", iSection, 1.0f, "Explosion Secondary Lighting Area Three");
		rScreen.WrapperSlider("Lighting Int One", iSection, 1.0f, "Explosion Secondary Lighting Intensity One");
		rScreen.WrapperSlider("Lighting Int Two", iSection, 1.0f, "Explosion Secondary Lighting Intensity Two");
		rScreen.WrapperSlider("Lighting Int Three", iSection, 1.0f, "Explosion Secondary Lighting Intensity Three");

		rScreen.WrapperSeparatorText("Blasters - Terrain Crater");
		rScreen.WrapperSlider("Lighting Area One", iSection, 1.0f, "Crater Lighting Area One");
		rScreen.WrapperSlider("Lighting Area Two", iSection, 1.0f, "Crater Lighting Area Two");
		rScreen.WrapperSlider("Lighting Area Three", iSection, 1.0f, "Crater Lighting Area Three");
		rScreen.WrapperSlider("Lighting Area Four", iSection, 1.0f, "Crater Lighting Area Four");
		rScreen.WrapperSlider("Lighting Int One", iSection, 1.0f, "Crater Lighting Intensity One");
		rScreen.WrapperSlider("Lighting Int Two", iSection, 1.0f, "Crater Lighting Intensity Two");
		rScreen.WrapperSlider("Lighting Int Three", iSection, 1.0f, "Crater Lighting Intensity Three");
		rScreen.WrapperSlider("Lighting Int Four", iSection, 1.0f, "Crater Lighting Intensity Four");

		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Players - Area Light");
		rScreen.WrapperSlider("Lighting Size", iSection, 1.0f, "Player Area Light Lighting Size");
		rScreen.WrapperSlider("Lighting Intensity", iSection, 1.0f, "Player Area Light Lighting Intensity");

		rScreen.WrapperSeparatorText("Players - Impact Light");
		rScreen.WrapperSlider("Lighting Area One", iSection, 1.0f, "Player Impact Lighting Area One");
		rScreen.WrapperSlider("Lighting Area Two", iSection, 1.0f, "Player Impact Lighting Area Two");
		rScreen.WrapperSlider("Lighting Int One", iSection, 1.0f, "Player Impact Lighting Intensity One");
		rScreen.WrapperSlider("Lighting Int Two", iSection, 1.0f, "Player Impact Lighting Intensity Two");

		rScreen.WrapperSeparatorText("Players - Hex Shield");
		rScreen.WrapperSlider("Lighting Intensity", iSection, 1.0f, "Hex Shield Lighting Intensity");

		rScreen.WrapperSeparatorText("Missiles - Exhaust");
		rScreen.WrapperSlider("Lighting Area", iSection, 1.0f, "Missile Exhaust Lighting Area");
		rScreen.WrapperSlider("Lighting Intensity", iSection, 1.0f, "Missile Exhaust Lighting Intensity");

		rScreen.WrapperSeparatorText("Spaceships - Enemy Blaster");
		rScreen.WrapperSlider("Lighting Area", iSection, 1.0f, "Enemy Blaster Lighting Area");
		rScreen.WrapperSlider("Lighting Intensity", iSection, 1.0f, "Enemy Blaster Lighting Intensity");

		rScreen.WrapperSeparatorText("Spaceships - Hit Flash");
		rScreen.WrapperSlider("Lighting Area One", iSection, 1.0f, "Hit Flash Lighting Area One");
		rScreen.WrapperSlider("Lighting Area Two", iSection, 1.0f, "Hit Flash Lighting Area Two");
		rScreen.WrapperSlider("Lighting Int One", iSection, 1.0f, "Hit Flash Lighting Intensity One");
		rScreen.WrapperSlider("Lighting Int Two", iSection, 1.0f, "Hit Flash Lighting Intensity Two");

		ImGui::EndTable();
	}
}

} // namespace game

#endif // BT_CLIENT
