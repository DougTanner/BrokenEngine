#include "TweaksScreenBase.h"

#if defined(BT_CLIENT)

#include "Ui/SmokeWrappersBase.h"
#include "TweaksSliderMap.h"

namespace engine
{

const TweaksSliderMapRegistrar gSmokeRegistrar
{
	{"Smoke Max", &gSmokeMaximum},
	{"Smoke Power", &gSmokePower},
	{"Smoke Decay", &gSmokeDecay},
	{"Smoke Edge Decay Distance", &gSmokeEdgeDecayDistance},
	{"Smoke Color Min", &gSmokeColorMinimum},
	{"Smoke Color Multiplier", &gSmokeColorMultiplier},
	{"Smoke Lighting Multiplier", &gSmokeLightingMultiplier},
	{"Smoke Noise Scale One", &gSmokeNoiseScaleOne},
	{"Smoke Noise Scale Two", &gSmokeNoiseScaleTwo},
	{"Smoke Wind Noise Scale", &gSmokeWindNoiseScale},
	{"Smoke Noise Quantity", &gSmokeNoiseQuantity},
	{"Smoke Wind Noise Quantity", &gSmokeWindNoiseQuantity},
	{"Wind To Smoke Strength", &gWindToSmokeStrength},
	{"Wind To Smoke Power", &gWindToSmokePower},
	{"Wind Displacement Noise Scale", &gWindDisplacementNoiseScale},
	{"Wind Smoke Retention", &gWindSmokeRetention},
	{"Wind Smoke Advection", &gWindSmokeAdvection},
	{"Smoke Object Height", &gSmokeObjectHeight},
	// Trails (also rendered in game-side TweaksScreenSmokeDeposits column 3)
	{"Smoke Trails Quantity", &gSmokeTrailsQuantity},
	{"Smoke Trails Width Current", &gSmokeTrailsWidthCurrent},
	{"Smoke Trails Width Previous", &gSmokeTrailsWidthPrevious},
	{"Smoke Trails Length", &gSmokeTrailsLength},
	{"Smoke Trails Length Jitter", &gSmokeTrailsLengthJitter},
	{"Smoke Trails Side Jitter", &gSmokeTrailsSideJitter},
	{"Smoke Intensity Falloff", &gSmokeIntensityFalloff},
};

void RenderSmokeSection(TweaksScreenBase& rScreen)
{
	int64_t iSection = giTweakSectionSmoke;

	if (ImGui::BeginTabBar("SmokeTabs"))
	{
		if (rScreen.BeginSubtab("Smoke", iSection, 0))
		{
			if (ImGui::BeginTable("SmokeColumns", 2))
			{
				ImGui::TableNextColumn();

				rScreen.WrapperSeparatorText("Decay");
				rScreen.WrapperSlider("Smoke Max", iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Power", iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Decay", iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Edge Decay Distance", iSection, 1.0f);

				rScreen.WrapperSeparatorText("Color");
				rScreen.WrapperSlider("Smoke Color Min", iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Color Multiplier", iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Lighting Multiplier", iSection, 1.0f);

				rScreen.WrapperSeparatorText("Noise");
				rScreen.WrapperSlider("Smoke Noise Scale One", iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Noise Scale Two", iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Wind Noise Scale", iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Noise Quantity", iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Wind Noise Quantity", iSection, 1.0f);

				ImGui::TableNextColumn();

				rScreen.WrapperSeparatorText("Wind Displacement");
				rScreen.WrapperSlider("Wind To Smoke Strength", iSection, 1.0f);
				rScreen.WrapperSlider("Wind To Smoke Power", iSection, 1.0f);
				rScreen.WrapperSlider("Wind Displacement Noise Scale", iSection, 1.0f);
				rScreen.WrapperSlider("Wind Smoke Retention", iSection, 1.0f);
				rScreen.WrapperSlider("Wind Smoke Advection", iSection, 1.0f);

				rScreen.WrapperSeparatorText("Object");
				rScreen.WrapperSlider("Smoke Object Height", iSection, 1.0f);

				ImGui::EndTable();
			}

			ImGui::EndTabItem();
		}
		RenderTweakExtensionSubtabs(rScreen, iSection, 1);
		ImGui::EndTabBar();
	}
}

} // namespace engine

#endif // BT_CLIENT
