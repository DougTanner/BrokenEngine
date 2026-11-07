#include "Tweaks.h"

#include "Ui/LightingWrappers.h"

#if defined(BT_CLIENT)

namespace game
{

void RenderLightingEffectsLightingTab(engine::TweaksScreen& rScreen)
{
	int64_t iSection = engine::giTweakSectionLighting;

	if (ImGui::BeginTable("LightingEffectsLightingColumns", 2))
	{
		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Explosions - Primary");
		rScreen.WrapperSlider("Explosion Primary Lighting Area One", gExplosionPrimaryLightingAreaOne, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Primary Lighting Area Two", gExplosionPrimaryLightingAreaTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Primary Lighting Area Three", gExplosionPrimaryLightingAreaThree, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Primary Lighting Intensity One", gExplosionPrimaryLightingIntensityOne, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Primary Lighting Intensity Two", gExplosionPrimaryLightingIntensityTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Primary Lighting Intensity Three", gExplosionPrimaryLightingIntensityThree, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Explosions - Secondary");
		rScreen.WrapperSlider("Explosion Secondary Lighting Area One", gExplosionSecondaryLightingAreaOne, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Secondary Lighting Area Two", gExplosionSecondaryLightingAreaTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Secondary Lighting Area Three", gExplosionSecondaryLightingAreaThree, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Secondary Lighting Intensity One", gExplosionSecondaryLightingIntensityOne, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Secondary Lighting Intensity Two", gExplosionSecondaryLightingIntensityTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Secondary Lighting Intensity Three", gExplosionSecondaryLightingIntensityThree, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Blasters - Terrain Crater");
		rScreen.WrapperSlider("Crater Lighting Area One", gCraterLightingAreaOne, iSection, 1.0f);
		rScreen.WrapperSlider("Crater Lighting Area Two", gCraterLightingAreaTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Crater Lighting Area Three", gCraterLightingAreaThree, iSection, 1.0f);
		rScreen.WrapperSlider("Crater Lighting Area Four", gCraterLightingAreaFour, iSection, 1.0f);
		rScreen.WrapperSlider("Crater Lighting Intensity One", gCraterLightingIntensityOne, iSection, 1.0f);
		rScreen.WrapperSlider("Crater Lighting Intensity Two", gCraterLightingIntensityTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Crater Lighting Intensity Three", gCraterLightingIntensityThree, iSection, 1.0f);
		rScreen.WrapperSlider("Crater Lighting Intensity Four", gCraterLightingIntensityFour, iSection, 1.0f);

		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Omnis - Area Light");
		rScreen.WrapperSlider("Omni Blaster Lighting Area", gOmniBlasterLightingArea, iSection, 1.0f);
		rScreen.WrapperSlider("Omni Blaster Lighting Intensity", gOmniBlasterLightingIntensity, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Omnis - Impact Light");
		rScreen.WrapperSlider("Omni Impact Lighting Area One", gOmniImpactLightingAreaOne, iSection, 1.0f);
		rScreen.WrapperSlider("Omni Impact Lighting Area Two", gOmniImpactLightingAreaTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Omni Impact Lighting Intensity One", gOmniImpactLightingIntensityOne, iSection, 1.0f);
		rScreen.WrapperSlider("Omni Impact Lighting Intensity Two", gOmniImpactLightingIntensityTwo, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Omnis - Hex Shield");
		rScreen.WrapperSlider("Hex Shield Lighting Intensity", gHexShieldLightingIntensity, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Missiles - Exhaust");
		rScreen.WrapperSlider("Missile Exhaust Lighting Area", gMissileExhaustLightingArea, iSection, 1.0f);
		rScreen.WrapperSlider("Missile Exhaust Lighting Intensity", gMissileExhaustLightingIntensity, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Fighters - Hit Flash");
		rScreen.WrapperSlider("Hit Flash Lighting Area One", gHitFlashLightingAreaOne, iSection, 1.0f);
		rScreen.WrapperSlider("Hit Flash Lighting Area Two", gHitFlashLightingAreaTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Hit Flash Lighting Intensity One", gHitFlashLightingIntensityOne, iSection, 1.0f);
		rScreen.WrapperSlider("Hit Flash Lighting Intensity Two", gHitFlashLightingIntensityTwo, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Fighters - Beam Impact");
		rScreen.WrapperSlider("Fighter Beam Impact Lighting Area One", gFighterBeamImpactLightingAreaOne, iSection, 1.0f);
		rScreen.WrapperSlider("Fighter Beam Impact Lighting Area Two", gFighterBeamImpactLightingAreaTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Fighter Beam Impact Lighting Intensity One", gFighterBeamImpactLightingIntensityOne, iSection, 1.0f);
		rScreen.WrapperSlider("Fighter Beam Impact Lighting Intensity Two", gFighterBeamImpactLightingIntensityTwo, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Fighters - Beam");
		rScreen.WrapperSlider("Fighter Beam Lighting Area", gFighterBeamLightingArea, iSection, 1.0f);
		rScreen.WrapperSlider("Fighter Beam Lighting Intensity", gFighterBeamLightingIntensity, iSection, 1.0f);

		ImGui::EndTable();
	}
}

} // namespace game

#endif // BT_CLIENT
