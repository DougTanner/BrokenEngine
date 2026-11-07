#include "Tweaks.h"

#include "Ui/LightingWrappers.h"

#if defined(BT_CLIENT)

namespace game
{

void RenderLightingEffectsVisibleTab(engine::TweaksScreen& rScreen)
{
	int64_t iSection = engine::giTweakSectionLighting;

	if (ImGui::BeginTable("LightingEffectsVisibleColumns", 2))
	{
		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Explosions - Primary");
		rScreen.WrapperSlider("Explosion Primary Visible Area One", gExplosionPrimaryVisibleAreaOne, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Primary Visible Area Two", gExplosionPrimaryVisibleAreaTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Primary Visible Area Three", gExplosionPrimaryVisibleAreaThree, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Primary Visible Intensity One", gExplosionPrimaryVisibleIntensityOne, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Primary Visible Intensity Two", gExplosionPrimaryVisibleIntensityTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Primary Visible Intensity Three", gExplosionPrimaryVisibleIntensityThree, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Explosions - Secondary");
		rScreen.WrapperSlider("Explosion Secondary Visible Area One", gExplosionSecondaryVisibleAreaOne, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Secondary Visible Area Two", gExplosionSecondaryVisibleAreaTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Secondary Visible Area Three", gExplosionSecondaryVisibleAreaThree, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Secondary Visible Intensity One", gExplosionSecondaryVisibleIntensityOne, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Secondary Visible Intensity Two", gExplosionSecondaryVisibleIntensityTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Secondary Visible Intensity Three", gExplosionSecondaryVisibleIntensityThree, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Blasters - Terrain Crater");
		rScreen.WrapperSlider("Crater Visible Area One", gCraterVisibleAreaOne, iSection, 1.0f);
		rScreen.WrapperSlider("Crater Visible Area Two", gCraterVisibleAreaTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Crater Visible Area Three", gCraterVisibleAreaThree, iSection, 1.0f);
		rScreen.WrapperSlider("Crater Visible Area Four", gCraterVisibleAreaFour, iSection, 1.0f);
		rScreen.WrapperSlider("Crater Visible Intensity One", gCraterVisibleIntensityOne, iSection, 1.0f);
		rScreen.WrapperSlider("Crater Visible Intensity Two", gCraterVisibleIntensityTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Crater Visible Intensity Three", gCraterVisibleIntensityThree, iSection, 1.0f);
		rScreen.WrapperSlider("Crater Visible Intensity Four", gCraterVisibleIntensityFour, iSection, 1.0f);

		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Omnis - Area Light");
		rScreen.WrapperSlider("Omni Area Light Visible Intensity", gOmniAreaLightVisibleIntensity, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Omnis - Impact Light");
		rScreen.WrapperSlider("Omni Impact Visible Area One", gOmniImpactVisibleAreaOne, iSection, 1.0f);
		rScreen.WrapperSlider("Omni Impact Visible Area Two", gOmniImpactVisibleAreaTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Omni Impact Visible Intensity One", gOmniImpactVisibleIntensityOne, iSection, 1.0f);
		rScreen.WrapperSlider("Omni Impact Visible Intensity Two", gOmniImpactVisibleIntensityTwo, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Omnis - Hex Shield");
		rScreen.WrapperSlider("Hex Shield Intensity Decay", gHexShieldIntensityDecay, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Missiles - Exhaust");
		rScreen.WrapperSlider("Missile Exhaust Visible Intensity", gMissileExhaustVisibleIntensity, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Fighters - Hit Flash");
		rScreen.WrapperSlider("Hit Flash Visible Area One", gHitFlashVisibleAreaOne, iSection, 1.0f);
		rScreen.WrapperSlider("Hit Flash Visible Area Two", gHitFlashVisibleAreaTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Hit Flash Visible Intensity One", gHitFlashVisibleIntensityOne, iSection, 1.0f);
		rScreen.WrapperSlider("Hit Flash Visible Intensity Two", gHitFlashVisibleIntensityTwo, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Fighters - Beam Impact");
		rScreen.WrapperSlider("Fighter Beam Impact Visible Area One", gFighterBeamImpactVisibleAreaOne, iSection, 1.0f);
		rScreen.WrapperSlider("Fighter Beam Impact Visible Area Two", gFighterBeamImpactVisibleAreaTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Fighter Beam Impact Visible Intensity One", gFighterBeamImpactVisibleIntensityOne, iSection, 1.0f);
		rScreen.WrapperSlider("Fighter Beam Impact Visible Intensity Two", gFighterBeamImpactVisibleIntensityTwo, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Fighters - Beam");
		rScreen.WrapperSlider("Fighter Beam Width", gFighterBeamWidth, iSection, 1.0f);

		ImGui::EndTable();
	}
}

} // namespace game

#endif // BT_CLIENT
