#include "Tweaks.h"

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
	{"Omni Area Light Visible Intensity", &gOmniAreaLightVisibleIntensity},
	{"Omni Impact Visible Area One", &gOmniImpactVisibleAreaOne},
	{"Omni Impact Visible Area Two", &gOmniImpactVisibleAreaTwo},
	{"Omni Impact Visible Intensity One", &gOmniImpactVisibleIntensityOne},
	{"Omni Impact Visible Intensity Two", &gOmniImpactVisibleIntensityTwo},
	{"Hex Shield Intensity Decay", &gHexShieldIntensityDecay},
	{"Missile Exhaust Visible Intensity", &gMissileExhaustVisibleIntensity},
	{"Hit Flash Visible Area One", &gHitFlashVisibleAreaOne},
	{"Hit Flash Visible Area Two", &gHitFlashVisibleAreaTwo},
	{"Hit Flash Visible Intensity One", &gHitFlashVisibleIntensityOne},
	{"Hit Flash Visible Intensity Two", &gHitFlashVisibleIntensityTwo},
	{"Fighter Beam Impact Visible Area One", &gFighterBeamImpactVisibleAreaOne},
	{"Fighter Beam Impact Visible Area Two", &gFighterBeamImpactVisibleAreaTwo},
	{"Fighter Beam Impact Visible Intensity One", &gFighterBeamImpactVisibleIntensityOne},
	{"Fighter Beam Impact Visible Intensity Two", &gFighterBeamImpactVisibleIntensityTwo},
	{"Fighter Beam Width", &gFighterBeamWidth},
};

void RenderLightingEffectsVisibleTab(engine::TweaksScreen& rScreen)
{
	int64_t iSection = engine::giTweakSectionLighting;

	if (ImGui::BeginTable("LightingEffectsVisibleColumns", 2))
	{
		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Explosions - Primary");
		rScreen.WrapperSlider("Explosion Primary Visible Area One", iSection, 1.0f, "Explosion Primary Visible Area One");
		rScreen.WrapperSlider("Explosion Primary Visible Area Two", iSection, 1.0f, "Explosion Primary Visible Area Two");
		rScreen.WrapperSlider("Explosion Primary Visible Area Three", iSection, 1.0f, "Explosion Primary Visible Area Three");
		rScreen.WrapperSlider("Explosion Primary Visible Intensity One", iSection, 1.0f, "Explosion Primary Visible Intensity One");
		rScreen.WrapperSlider("Explosion Primary Visible Intensity Two", iSection, 1.0f, "Explosion Primary Visible Intensity Two");
		rScreen.WrapperSlider("Explosion Primary Visible Intensity Three", iSection, 1.0f, "Explosion Primary Visible Intensity Three");

		rScreen.WrapperSeparatorText("Explosions - Secondary");
		rScreen.WrapperSlider("Explosion Secondary Visible Area One", iSection, 1.0f, "Explosion Secondary Visible Area One");
		rScreen.WrapperSlider("Explosion Secondary Visible Area Two", iSection, 1.0f, "Explosion Secondary Visible Area Two");
		rScreen.WrapperSlider("Explosion Secondary Visible Area Three", iSection, 1.0f, "Explosion Secondary Visible Area Three");
		rScreen.WrapperSlider("Explosion Secondary Visible Intensity One", iSection, 1.0f, "Explosion Secondary Visible Intensity One");
		rScreen.WrapperSlider("Explosion Secondary Visible Intensity Two", iSection, 1.0f, "Explosion Secondary Visible Intensity Two");
		rScreen.WrapperSlider("Explosion Secondary Visible Intensity Three", iSection, 1.0f, "Explosion Secondary Visible Intensity Three");

		rScreen.WrapperSeparatorText("Blasters - Terrain Crater");
		rScreen.WrapperSlider("Crater Visible Area One", iSection, 1.0f, "Crater Visible Area One");
		rScreen.WrapperSlider("Crater Visible Area Two", iSection, 1.0f, "Crater Visible Area Two");
		rScreen.WrapperSlider("Crater Visible Area Three", iSection, 1.0f, "Crater Visible Area Three");
		rScreen.WrapperSlider("Crater Visible Area Four", iSection, 1.0f, "Crater Visible Area Four");
		rScreen.WrapperSlider("Crater Visible Intensity One", iSection, 1.0f, "Crater Visible Intensity One");
		rScreen.WrapperSlider("Crater Visible Intensity Two", iSection, 1.0f, "Crater Visible Intensity Two");
		rScreen.WrapperSlider("Crater Visible Intensity Three", iSection, 1.0f, "Crater Visible Intensity Three");
		rScreen.WrapperSlider("Crater Visible Intensity Four", iSection, 1.0f, "Crater Visible Intensity Four");

		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Omnis - Area Light");
		rScreen.WrapperSlider("Omni Area Light Visible Intensity", iSection, 1.0f, "Omni Area Light Visible Intensity");

		rScreen.WrapperSeparatorText("Omnis - Impact Light");
		rScreen.WrapperSlider("Omni Impact Visible Area One", iSection, 1.0f, "Omni Impact Visible Area One");
		rScreen.WrapperSlider("Omni Impact Visible Area Two", iSection, 1.0f, "Omni Impact Visible Area Two");
		rScreen.WrapperSlider("Omni Impact Visible Intensity One", iSection, 1.0f, "Omni Impact Visible Intensity One");
		rScreen.WrapperSlider("Omni Impact Visible Intensity Two", iSection, 1.0f, "Omni Impact Visible Intensity Two");

		rScreen.WrapperSeparatorText("Omnis - Hex Shield");
		rScreen.WrapperSlider("Hex Shield Intensity Decay", iSection, 1.0f, "Hex Shield Intensity Decay");

		rScreen.WrapperSeparatorText("Missiles - Exhaust");
		rScreen.WrapperSlider("Missile Exhaust Visible Intensity", iSection, 1.0f, "Missile Exhaust Visible Intensity");

		rScreen.WrapperSeparatorText("Fighters - Hit Flash");
		rScreen.WrapperSlider("Hit Flash Visible Area One", iSection, 1.0f, "Hit Flash Visible Area One");
		rScreen.WrapperSlider("Hit Flash Visible Area Two", iSection, 1.0f, "Hit Flash Visible Area Two");
		rScreen.WrapperSlider("Hit Flash Visible Intensity One", iSection, 1.0f, "Hit Flash Visible Intensity One");
		rScreen.WrapperSlider("Hit Flash Visible Intensity Two", iSection, 1.0f, "Hit Flash Visible Intensity Two");

		rScreen.WrapperSeparatorText("Fighters - Beam Impact");
		rScreen.WrapperSlider("Fighter Beam Impact Visible Area One", iSection, 1.0f, "Fighter Beam Impact Visible Area One");
		rScreen.WrapperSlider("Fighter Beam Impact Visible Area Two", iSection, 1.0f, "Fighter Beam Impact Visible Area Two");
		rScreen.WrapperSlider("Fighter Beam Impact Visible Intensity One", iSection, 1.0f, "Fighter Beam Impact Visible Intensity One");
		rScreen.WrapperSlider("Fighter Beam Impact Visible Intensity Two", iSection, 1.0f, "Fighter Beam Impact Visible Intensity Two");

		rScreen.WrapperSeparatorText("Fighters - Beam");
		rScreen.WrapperSlider("Fighter Beam Width", iSection, 1.0f, "Fighter Beam Width");

		ImGui::EndTable();
	}
}

} // namespace game

#endif // BT_CLIENT
