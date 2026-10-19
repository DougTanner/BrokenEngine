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
		rScreen.WrapperSlider("Volume", iSection, 1.0f, "Omni Blaster Volume");

		rScreen.WrapperSeparatorText("Blasters - Fighter");
		rScreen.WrapperSlider("Volume", iSection, 1.0f, "Fighter Blaster Volume");

		rScreen.WrapperSeparatorText("Blasters - Terrain Impact");
		rScreen.WrapperSlider("Volume", iSection, 1.0f, "Terrain Impact Volume");

		rScreen.WrapperSeparatorText("Missiles");
		rScreen.WrapperSlider("Launch Volume", iSection, 1.0f, "Missile Launch Volume");
		rScreen.WrapperSlider("Loop Volume", iSection, 1.0f, "Missile Loop Volume");

		rScreen.WrapperSeparatorText("Explosions");
		rScreen.WrapperSlider("Volume", iSection, 1.0f, "Explosion Volume");

		rScreen.WrapperSeparatorText("Omnis - Shield");
		rScreen.WrapperSlider("Hit Volume Base", iSection, 1.0f, "Shield Hit Volume Base");
		rScreen.WrapperSlider("Hit Volume Scale", iSection, 1.0f, "Shield Hit Volume Scale");
		rScreen.WrapperSlider("Down Volume", iSection, 1.0f, "Shield Down Volume");

		rScreen.WrapperSeparatorText("Omnis - Armor");
		rScreen.WrapperSlider("Hit Volume Base", iSection, 1.0f, "Armor Hit Volume Base");
		rScreen.WrapperSlider("Hit Volume Scale", iSection, 1.0f, "Armor Hit Volume Scale");

		rScreen.WrapperSeparatorText("Fighters");
		rScreen.WrapperSlider("Death Volume", iSection, 1.0f, "Fighter Death Volume");
		rScreen.WrapperSlider("Hit Volume", iSection, 1.0f, "Fighter Hit Volume");

		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Blasters - Omni");
		rScreen.WrapperSlider("Pitch Min", iSection, 1.0f, "Omni Blaster Pitch Min");
		rScreen.WrapperSlider("Pitch Random", iSection, 1.0f, "Omni Blaster Pitch Random");

		rScreen.WrapperSeparatorText("Blasters - Fighter");
		rScreen.WrapperSlider("Pitch Min", iSection, 1.0f, "Fighter Blaster Pitch Min");
		rScreen.WrapperSlider("Pitch Random", iSection, 1.0f, "Fighter Blaster Pitch Random");

		rScreen.WrapperSeparatorText("Missiles");
		rScreen.WrapperSlider("Pitch Min", iSection, 1.0f, "Missile Pitch Min");
		rScreen.WrapperSlider("Pitch Random", iSection, 1.0f, "Missile Pitch Random");

		rScreen.WrapperSeparatorText("Fighters");
		rScreen.WrapperSlider("Death Pitch Min", iSection, 1.0f, "Fighter Death Pitch Min");
		rScreen.WrapperSlider("Death Pitch Random", iSection, 1.0f, "Fighter Death Pitch Random");

		ImGui::EndTable();
	}
}

} // namespace game

#endif // BT_CLIENT
