#include "TweaksScreen.h"

#include "Ui/Screens/TweaksScreen/TweaksSliderMap.h"

#include "Ui/SmokeWrappers.h"

#if defined(BT_CLIENT)

namespace game
{

// SmokeDeposits is the sole consumer of these labels, despite their gExplosion*/gBlasterPuff*/gPlayerImpactPuff*/gMissileTrail* globals living in SmokeWrappers.h. Owning them here keeps registration co-located with the WrapperSlider call sites below.
const engine::TweaksSliderMapRegistrar gSmokeDepositsRegistrar
{
	{"Explosion Primary Puff Area One", &gExplosionPrimaryPuffAreaOne},
	{"Explosion Primary Puff Area Two", &gExplosionPrimaryPuffAreaTwo},
	{"Explosion Primary Puff Intensity One", &gExplosionPrimaryPuffIntensityOne},
	{"Explosion Primary Puff Intensity Two", &gExplosionPrimaryPuffIntensityTwo},
	{"Explosion Secondary Puff Area One", &gExplosionSecondaryPuffAreaOne},
	{"Explosion Secondary Puff Area Two", &gExplosionSecondaryPuffAreaTwo},
	{"Explosion Secondary Puff Intensity One", &gExplosionSecondaryPuffIntensityOne},
	{"Explosion Secondary Puff Intensity Two", &gExplosionSecondaryPuffIntensityTwo},
	{"Explosion Primary Trail Intensity", &gExplosionPrimaryTrailIntensity},
	{"Explosion Primary Trail Length", &gExplosionPrimaryTrailLength},
	{"Explosion Primary Trail Duration", &gExplosionPrimaryTrailDuration},
	{"Explosion Secondary Trail Intensity", &gExplosionSecondaryTrailIntensity},
	{"Explosion Secondary Trail Length", &gExplosionSecondaryTrailLength},
	{"Explosion Secondary Trail Duration", &gExplosionSecondaryTrailDuration},
	{"Blaster Puff Area Start", &gBlasterPuffAreaStart},
	{"Blaster Puff Area End", &gBlasterPuffAreaEnd},
	{"Blaster Puff Intensity Start", &gBlasterPuffIntensityStart},
	{"Blaster Puff Intensity End", &gBlasterPuffIntensityEnd},
	{"Player Impact Puff Area One", &gPlayerImpactPuffAreaOne},
	{"Player Impact Puff Area Two", &gPlayerImpactPuffAreaTwo},
	{"Player Impact Puff Intensity One", &gPlayerImpactPuffIntensityOne},
	{"Player Impact Puff Intensity Two", &gPlayerImpactPuffIntensityTwo},
	{"Missile Trail Intensity", &gMissileTrailIntensity},
};

void RenderSmokeDepositsTab(engine::TweaksScreenBase& rScreen)
{
	int64_t iSection = engine::giTweakSectionSmoke;

	if (ImGui::BeginTable("SmokeDepositsColumns", 3))
	{
		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Explosions - Primary Puff");
		rScreen.WrapperSlider("Area One", iSection, 1.0f, "Explosion Primary Puff Area One");
		rScreen.WrapperSlider("Area Two", iSection, 1.0f, "Explosion Primary Puff Area Two");
		rScreen.WrapperSlider("Intensity One", iSection, 1.0f, "Explosion Primary Puff Intensity One");
		rScreen.WrapperSlider("Intensity Two", iSection, 1.0f, "Explosion Primary Puff Intensity Two");

		rScreen.WrapperSeparatorText("Explosions - Secondary Puff");
		rScreen.WrapperSlider("Area One", iSection, 1.0f, "Explosion Secondary Puff Area One");
		rScreen.WrapperSlider("Area Two", iSection, 1.0f, "Explosion Secondary Puff Area Two");
		rScreen.WrapperSlider("Intensity One", iSection, 1.0f, "Explosion Secondary Puff Intensity One");
		rScreen.WrapperSlider("Intensity Two", iSection, 1.0f, "Explosion Secondary Puff Intensity Two");

		rScreen.WrapperSeparatorText("Explosions - Primary Trail");
		rScreen.WrapperSlider("Intensity", iSection, 1.0f, "Explosion Primary Trail Intensity");
		rScreen.WrapperSlider("Length", iSection, 1.0f, "Explosion Primary Trail Length");
		rScreen.WrapperSlider("Duration", iSection, 1.0f, "Explosion Primary Trail Duration");

		rScreen.WrapperSeparatorText("Explosions - Secondary Trail");
		rScreen.WrapperSlider("Intensity", iSection, 1.0f, "Explosion Secondary Trail Intensity");
		rScreen.WrapperSlider("Length", iSection, 1.0f, "Explosion Secondary Trail Length");
		rScreen.WrapperSlider("Duration", iSection, 1.0f, "Explosion Secondary Trail Duration");

		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Blasters - Terrain Puff");
		rScreen.WrapperSlider("Area Start", iSection, 1.0f, "Blaster Puff Area Start");
		rScreen.WrapperSlider("Area End", iSection, 1.0f, "Blaster Puff Area End");
		rScreen.WrapperSlider("Intensity Start", iSection, 1.0f, "Blaster Puff Intensity Start");
		rScreen.WrapperSlider("Intensity End", iSection, 1.0f, "Blaster Puff Intensity End");

		rScreen.WrapperSeparatorText("Players - Impact Puff");
		rScreen.WrapperSlider("Area One", iSection, 1.0f, "Player Impact Puff Area One");
		rScreen.WrapperSlider("Area Two", iSection, 1.0f, "Player Impact Puff Area Two");
		rScreen.WrapperSlider("Intensity One", iSection, 1.0f, "Player Impact Puff Intensity One");
		rScreen.WrapperSlider("Intensity Two", iSection, 1.0f, "Player Impact Puff Intensity Two");

		rScreen.WrapperSeparatorText("Missiles - Trail");
		rScreen.WrapperSlider("Intensity", iSection, 1.0f, "Missile Trail Intensity");

		// Column 3: Engine smoke trail rendering params
		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Trails");
		rScreen.WrapperSlider("Smoke Trails Quantity", iSection, 1.0f);
		rScreen.WrapperSlider("Smoke Trails Width Current", iSection, 1.0f);
		rScreen.WrapperSlider("Smoke Trails Width Previous", iSection, 1.0f);
		rScreen.WrapperSlider("Smoke Trails Length", iSection, 1.0f);
		rScreen.WrapperSlider("Smoke Trails Length Jitter", iSection, 1.0f);
		rScreen.WrapperSlider("Smoke Trails Side Jitter", iSection, 1.0f);
		rScreen.WrapperSlider("Smoke Intensity Falloff", iSection, 1.0f);

		ImGui::EndTable();
	}
}

} // namespace game

#endif // BT_CLIENT
