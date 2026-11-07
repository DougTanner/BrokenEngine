#include "Tweaks.h"

#include "Ui/SoundWrappers.h"

#if defined(BT_CLIENT)

namespace game
{

void RenderSoundEffects(engine::TweaksScreen& rScreen)
{
	int64_t iSection = engine::giTweakSectionSound;

	if (ImGui::BeginTable("SoundEffectsColumns", 2))
	{
		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Blasters - Omni");
		rScreen.WrapperSlider("Omni Blaster Volume", gOmniBlasterVolume, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Blasters - Fighter");
		rScreen.WrapperSlider("Fighter Blaster Volume", gFighterBlasterVolume, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Blasters - Terrain Impact");
		rScreen.WrapperSlider("Terrain Impact Volume", gTerrainImpactVolume, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Missiles");
		rScreen.WrapperSlider("Missile Launch Volume", gMissileLaunchVolume, iSection, 1.0f);
		rScreen.WrapperSlider("Missile Loop Volume", gMissileLoopVolume, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Explosions");
		rScreen.WrapperSlider("Explosion Volume", gExplosionVolume, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Omnis - Shield");
		rScreen.WrapperSlider("Shield Hit Volume Base", gShieldHitVolumeBase, iSection, 1.0f);
		rScreen.WrapperSlider("Shield Hit Volume Scale", gShieldHitVolumeScale, iSection, 1.0f);
		rScreen.WrapperSlider("Shield Down Volume", gShieldDownVolume, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Omnis - Armor");
		rScreen.WrapperSlider("Armor Hit Volume Base", gArmorHitVolumeBase, iSection, 1.0f);
		rScreen.WrapperSlider("Armor Hit Volume Scale", gArmorHitVolumeScale, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Fighters");
		rScreen.WrapperSlider("Fighter Death Volume", gFighterDeathVolume, iSection, 1.0f);
		rScreen.WrapperSlider("Fighter Hit Volume", gFighterHitVolume, iSection, 1.0f);

		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Blasters - Omni");
		rScreen.WrapperSlider("Omni Blaster Pitch Minimum", gOmniBlasterPitchMinimum, iSection, 1.0f);
		rScreen.WrapperSlider("Omni Blaster Pitch Random", gOmniBlasterPitchRandom, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Blasters - Fighter");
		rScreen.WrapperSlider("Fighter Blaster Pitch Minimum", gFighterBlasterPitchMinimum, iSection, 1.0f);
		rScreen.WrapperSlider("Fighter Blaster Pitch Random", gFighterBlasterPitchRandom, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Missiles");
		rScreen.WrapperSlider("Missile Pitch Min", gMissilePitchMin, iSection, 1.0f);
		rScreen.WrapperSlider("Missile Pitch Random", gMissilePitchRandom, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Fighters");
		rScreen.WrapperSlider("Fighter Death Pitch Minimum", gFighterDeathPitchMinimum, iSection, 1.0f);
		rScreen.WrapperSlider("Fighter Death Pitch Random", gFighterDeathPitchRandom, iSection, 1.0f);

		ImGui::EndTable();
	}
}

} // namespace game

#endif // BT_CLIENT
