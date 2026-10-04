#include "Ui/Screens/TweaksScreen/TweaksSliderMap.h"

#include "Ui/SoundWrappers.h"
#include "TweaksScreen.h"

#if defined(BT_CLIENT)

namespace game
{

const engine::TweaksSliderMapRegistrar gSoundEffectsRegistrar
{
	// Blasters - Player
	{"Player Blaster Volume", &gPlayerBlasterVolume},
	{"Player Blaster Pitch Min", &gPlayerBlasterPitchMin},
	{"Player Blaster Pitch Random", &gPlayerBlasterPitchRandom},
	// Blasters - Enemy
	{"Enemy Blaster Volume", &gEnemyBlasterVolume},
	{"Enemy Blaster Pitch Min", &gEnemyBlasterPitchMin},
	{"Enemy Blaster Pitch Random", &gEnemyBlasterPitchRandom},
	// Blasters - Terrain Impact
	{"Terrain Impact Volume", &gTerrainImpactVolume},
	// Missiles
	{"Missile Launch Volume", &gMissileLaunchVolume},
	{"Missile Loop Volume", &gMissileLoopVolume},
	{"Missile Pitch Min", &gMissilePitchMin},
	{"Missile Pitch Random", &gMissilePitchRandom},
	// Explosions
	{"Explosion Volume", &gExplosionVolume},
	// Players - Shield
	{"Shield Hit Volume Base", &gShieldHitVolumeBase},
	{"Shield Hit Volume Scale", &gShieldHitVolumeScale},
	{"Shield Down Volume", &gShieldDownVolume},
	// Players - Armor
	{"Armor Hit Volume Base", &gArmorHitVolumeBase},
	{"Armor Hit Volume Scale", &gArmorHitVolumeScale},
	// Spaceships
	{"Spaceship Death Volume", &gSpaceshipDeathVolume},
	{"Spaceship Death Pitch Min", &gSpaceshipDeathPitchMin},
	{"Spaceship Death Pitch Random", &gSpaceshipDeathPitchRandom},
	{"Spaceship Hit Volume", &gSpaceshipHitVolume},
};

void RenderSoundEffects(engine::TweaksScreenBase& rScreen)
{
	int64_t iSection = engine::giTweakSectionSound;

	if (ImGui::BeginTable("SoundEffectsColumns", 2))
	{
		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Blasters - Player");
		rScreen.WrapperSlider("Volume", iSection, 1.0f, "Player Blaster Volume");

		rScreen.WrapperSeparatorText("Blasters - Enemy");
		rScreen.WrapperSlider("Volume", iSection, 1.0f, "Enemy Blaster Volume");

		rScreen.WrapperSeparatorText("Blasters - Terrain Impact");
		rScreen.WrapperSlider("Volume", iSection, 1.0f, "Terrain Impact Volume");

		rScreen.WrapperSeparatorText("Missiles");
		rScreen.WrapperSlider("Launch Volume", iSection, 1.0f, "Missile Launch Volume");
		rScreen.WrapperSlider("Loop Volume", iSection, 1.0f, "Missile Loop Volume");

		rScreen.WrapperSeparatorText("Explosions");
		rScreen.WrapperSlider("Volume", iSection, 1.0f, "Explosion Volume");

		rScreen.WrapperSeparatorText("Players - Shield");
		rScreen.WrapperSlider("Hit Volume Base", iSection, 1.0f, "Shield Hit Volume Base");
		rScreen.WrapperSlider("Hit Volume Scale", iSection, 1.0f, "Shield Hit Volume Scale");
		rScreen.WrapperSlider("Down Volume", iSection, 1.0f, "Shield Down Volume");

		rScreen.WrapperSeparatorText("Players - Armor");
		rScreen.WrapperSlider("Hit Volume Base", iSection, 1.0f, "Armor Hit Volume Base");
		rScreen.WrapperSlider("Hit Volume Scale", iSection, 1.0f, "Armor Hit Volume Scale");

		rScreen.WrapperSeparatorText("Spaceships");
		rScreen.WrapperSlider("Death Volume", iSection, 1.0f, "Spaceship Death Volume");
		rScreen.WrapperSlider("Hit Volume", iSection, 1.0f, "Spaceship Hit Volume");

		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Blasters - Player");
		rScreen.WrapperSlider("Pitch Min", iSection, 1.0f, "Player Blaster Pitch Min");
		rScreen.WrapperSlider("Pitch Random", iSection, 1.0f, "Player Blaster Pitch Random");

		rScreen.WrapperSeparatorText("Blasters - Enemy");
		rScreen.WrapperSlider("Pitch Min", iSection, 1.0f, "Enemy Blaster Pitch Min");
		rScreen.WrapperSlider("Pitch Random", iSection, 1.0f, "Enemy Blaster Pitch Random");

		rScreen.WrapperSeparatorText("Missiles");
		rScreen.WrapperSlider("Pitch Min", iSection, 1.0f, "Missile Pitch Min");
		rScreen.WrapperSlider("Pitch Random", iSection, 1.0f, "Missile Pitch Random");

		rScreen.WrapperSeparatorText("Spaceships");
		rScreen.WrapperSlider("Death Pitch Min", iSection, 1.0f, "Spaceship Death Pitch Min");
		rScreen.WrapperSlider("Death Pitch Random", iSection, 1.0f, "Spaceship Death Pitch Random");

		ImGui::EndTable();
	}
}

} // namespace game

#endif // BT_CLIENT
