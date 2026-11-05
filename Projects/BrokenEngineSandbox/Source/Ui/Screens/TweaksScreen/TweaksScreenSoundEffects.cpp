#include "Tweaks.h"

#include "Ui/Screens/TweaksScreen/TweaksSliderMap.h"

#include "Ui/SoundWrappers.h"

#if defined(BT_CLIENT)

namespace game
{

const engine::TweaksSliderMapRegistrar gSoundEffectsRegistrar
{
	{"Omni Blaster Volume", &gOmniBlasterVolume},
	{"Omni Blaster Pitch Min", &gOmniBlasterPitchMinimum},
	{"Omni Blaster Pitch Random", &gOmniBlasterPitchRandom},
	{"Fighter Blaster Volume", &gFighterBlasterVolume},
	{"Fighter Blaster Pitch Min", &gFighterBlasterPitchMinimum},
	{"Fighter Blaster Pitch Random", &gFighterBlasterPitchRandom},
	{"Terrain Impact Volume", &gTerrainImpactVolume},
	{"Missile Launch Volume", &gMissileLaunchVolume},
	{"Missile Loop Volume", &gMissileLoopVolume},
	{"Missile Pitch Min", &gMissilePitchMin},
	{"Missile Pitch Random", &gMissilePitchRandom},
	{"Explosion Volume", &gExplosionVolume},
	{"Shield Hit Volume Base", &gShieldHitVolumeBase},
	{"Shield Hit Volume Scale", &gShieldHitVolumeScale},
	{"Shield Down Volume", &gShieldDownVolume},
	{"Armor Hit Volume Base", &gArmorHitVolumeBase},
	{"Armor Hit Volume Scale", &gArmorHitVolumeScale},
	{"Fighter Death Volume", &gFighterDeathVolume},
	{"Fighter Death Pitch Min", &gFighterDeathPitchMinimum},
	{"Fighter Death Pitch Random", &gFighterDeathPitchRandom},
	{"Fighter Hit Volume", &gFighterHitVolume},
};

void RenderSoundEffects(engine::TweaksScreen& rScreen)
{
	int64_t iSection = engine::giTweakSectionSound;

	if (ImGui::BeginTable("SoundEffectsColumns", 2))
	{
		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Blasters - Omni");
		rScreen.WrapperSlider("Omni Blaster Volume", iSection, 1.0f, "Omni Blaster Volume");

		rScreen.WrapperSeparatorText("Blasters - Fighter");
		rScreen.WrapperSlider("Fighter Blaster Volume", iSection, 1.0f, "Fighter Blaster Volume");

		rScreen.WrapperSeparatorText("Blasters - Terrain Impact");
		rScreen.WrapperSlider("Terrain Impact Volume", iSection, 1.0f, "Terrain Impact Volume");

		rScreen.WrapperSeparatorText("Missiles");
		rScreen.WrapperSlider("Missile Launch Volume", iSection, 1.0f, "Missile Launch Volume");
		rScreen.WrapperSlider("Missile Loop Volume", iSection, 1.0f, "Missile Loop Volume");

		rScreen.WrapperSeparatorText("Explosions");
		rScreen.WrapperSlider("Explosion Volume", iSection, 1.0f, "Explosion Volume");

		rScreen.WrapperSeparatorText("Omnis - Shield");
		rScreen.WrapperSlider("Shield Hit Volume Base", iSection, 1.0f, "Shield Hit Volume Base");
		rScreen.WrapperSlider("Shield Hit Volume Scale", iSection, 1.0f, "Shield Hit Volume Scale");
		rScreen.WrapperSlider("Shield Down Volume", iSection, 1.0f, "Shield Down Volume");

		rScreen.WrapperSeparatorText("Omnis - Armor");
		rScreen.WrapperSlider("Armor Hit Volume Base", iSection, 1.0f, "Armor Hit Volume Base");
		rScreen.WrapperSlider("Armor Hit Volume Scale", iSection, 1.0f, "Armor Hit Volume Scale");

		rScreen.WrapperSeparatorText("Fighters");
		rScreen.WrapperSlider("Fighter Death Volume", iSection, 1.0f, "Fighter Death Volume");
		rScreen.WrapperSlider("Fighter Hit Volume", iSection, 1.0f, "Fighter Hit Volume");

		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Blasters - Omni");
		rScreen.WrapperSlider("Omni Blaster Pitch Minimum", iSection, 1.0f, "Omni Blaster Pitch Min");
		rScreen.WrapperSlider("Omni Blaster Pitch Random", iSection, 1.0f, "Omni Blaster Pitch Random");

		rScreen.WrapperSeparatorText("Blasters - Fighter");
		rScreen.WrapperSlider("Fighter Blaster Pitch Minimum", iSection, 1.0f, "Fighter Blaster Pitch Min");
		rScreen.WrapperSlider("Fighter Blaster Pitch Random", iSection, 1.0f, "Fighter Blaster Pitch Random");

		rScreen.WrapperSeparatorText("Missiles");
		rScreen.WrapperSlider("Missile Pitch Min", iSection, 1.0f, "Missile Pitch Min");
		rScreen.WrapperSlider("Missile Pitch Random", iSection, 1.0f, "Missile Pitch Random");

		rScreen.WrapperSeparatorText("Fighters");
		rScreen.WrapperSlider("Fighter Death Pitch Minimum", iSection, 1.0f, "Fighter Death Pitch Min");
		rScreen.WrapperSlider("Fighter Death Pitch Random", iSection, 1.0f, "Fighter Death Pitch Random");

		ImGui::EndTable();
	}
}

} // namespace game

#endif // BT_CLIENT
