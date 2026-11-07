#include "Tweaks.h"

#include "Ui/SmokeWrappersBase.h"

#include "Ui/SmokeWrappers.h"

#if defined(BT_CLIENT)

namespace game
{

void RenderSmokeDepositsTab(engine::TweaksScreen& rScreen)
{
	int64_t iSection = engine::giTweakSectionSmoke;

	if (ImGui::BeginTable("SmokeDepositsColumns", 3))
	{
		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Explosions - Primary Puff");
		rScreen.WrapperSlider("Explosion Primary Puff Area One", gExplosionPrimaryPuffAreaOne, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Primary Puff Area Two", gExplosionPrimaryPuffAreaTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Primary Puff Intensity One", gExplosionPrimaryPuffIntensityOne, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Primary Puff Intensity Two", gExplosionPrimaryPuffIntensityTwo, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Explosions - Secondary Puff");
		rScreen.WrapperSlider("Explosion Secondary Puff Area One", gExplosionSecondaryPuffAreaOne, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Secondary Puff Area Two", gExplosionSecondaryPuffAreaTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Secondary Puff Intensity One", gExplosionSecondaryPuffIntensityOne, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Secondary Puff Intensity Two", gExplosionSecondaryPuffIntensityTwo, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Explosions - Primary Trail");
		rScreen.WrapperSlider("Explosion Primary Trail Intensity", gExplosionPrimaryTrailIntensity, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Primary Trail Length", gExplosionPrimaryTrailLength, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Primary Trail Duration", gExplosionPrimaryTrailDuration, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Explosions - Secondary Trail");
		rScreen.WrapperSlider("Explosion Secondary Trail Intensity", gExplosionSecondaryTrailIntensity, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Secondary Trail Length", gExplosionSecondaryTrailLength, iSection, 1.0f);
		rScreen.WrapperSlider("Explosion Secondary Trail Duration", gExplosionSecondaryTrailDuration, iSection, 1.0f);

		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Blasters - Terrain Puff");
		rScreen.WrapperSlider("Blaster Puff Area Start", gBlasterPuffAreaStart, iSection, 1.0f);
		rScreen.WrapperSlider("Blaster Puff Area End", gBlasterPuffAreaEnd, iSection, 1.0f);
		rScreen.WrapperSlider("Blaster Puff Intensity Start", gBlasterPuffIntensityStart, iSection, 1.0f);
		rScreen.WrapperSlider("Blaster Puff Intensity End", gBlasterPuffIntensityEnd, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Omnis - Impact Puff");
		rScreen.WrapperSlider("Omni Impact Puff Area One", gOmniImpactPuffAreaOne, iSection, 1.0f);
		rScreen.WrapperSlider("Omni Impact Puff Area Two", gOmniImpactPuffAreaTwo, iSection, 1.0f);
		rScreen.WrapperSlider("Omni Impact Puff Intensity One", gOmniImpactPuffIntensityOne, iSection, 1.0f);
		rScreen.WrapperSlider("Omni Impact Puff Intensity Two", gOmniImpactPuffIntensityTwo, iSection, 1.0f);

		rScreen.WrapperSeparatorText("Missiles - Trail");
		rScreen.WrapperSlider("Missile Trail Intensity", gMissileTrailIntensity, iSection, 1.0f);

		// Column 3: Engine smoke trail rendering params
		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Trails");
		rScreen.WrapperSlider("Smoke Trails Quantity", engine::gSmokeTrailsQuantity, iSection, 1.0f);
		rScreen.WrapperSlider("Smoke Trails Width Current", engine::gSmokeTrailsWidthCurrent, iSection, 1.0f);
		rScreen.WrapperSlider("Smoke Trails Width Previous", engine::gSmokeTrailsWidthPrevious, iSection, 1.0f);
		rScreen.WrapperSlider("Smoke Trails Length", engine::gSmokeTrailsLength, iSection, 1.0f);
		rScreen.WrapperSlider("Smoke Trails Length Jitter", engine::gSmokeTrailsLengthJitter, iSection, 1.0f);
		rScreen.WrapperSlider("Smoke Trails Side Jitter", engine::gSmokeTrailsSideJitter, iSection, 1.0f);
		rScreen.WrapperSlider("Smoke Intensity Falloff", engine::gSmokeIntensityFalloff, iSection, 1.0f);

		ImGui::EndTable();
	}
}

} // namespace game

#endif // BT_CLIENT
