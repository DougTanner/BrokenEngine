#include "TweaksScreen.h"

#if defined(BT_CLIENT)

#include "Ui/ShadowWrappersBase.h"
#include "Ui/SmokeWrappersBase.h"

#include "Ui/Screens/TweaksScreen/Tweaks.h"

namespace engine
{

void RenderSmokeSection(TweaksScreen& rScreen)
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
				rScreen.WrapperSlider("Smoke Maximum", gSmokeMaximum, iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Power", gSmokePower, iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Decay", gSmokeDecay, iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Edge Decay Distance", gSmokeEdgeDecayDistance, iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Update Cadence", gSmokeUpdateCadence, iSection, 1.0f);

				rScreen.WrapperSeparatorText("Color");
				rScreen.WrapperSlider("Smoke Color Minimum", gSmokeColorMinimum, iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Color Multiplier", gSmokeColorMultiplier, iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Lighting Multiplier", gSmokeLightingMultiplier, iSection, 1.0f);

				rScreen.WrapperSeparatorText("Shadow");
				rScreen.WrapperSlider("Smoke Shadow Intensity Bottom", gSmokeShadowIntensityBottom, iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Shadow Intensity Top", gSmokeShadowIntensityTop, iSection, 1.0f);

				rScreen.WrapperSeparatorText("Noise");
				rScreen.WrapperSlider("Smoke Noise Scale One", gSmokeNoiseScaleOne, iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Noise Scale Two", gSmokeNoiseScaleTwo, iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Wind Noise Scale", gSmokeWindNoiseScale, iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Noise Quantity", gSmokeNoiseQuantity, iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Wind Noise Quantity Bottom", gSmokeWindNoiseQuantityBottom, iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Wind Noise Quantity Top", gSmokeWindNoiseQuantityTop, iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Curl Strength Low", gSmokeCurlStrengthLow, iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Curl Strength High", gSmokeCurlStrengthHigh, iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Curl Scale", gSmokeCurlScale, iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Curl Speed", gSmokeCurlSpeed, iSection, 1.0f);

				ImGui::TableNextColumn();

				rScreen.WrapperSeparatorText("Wind Displacement");
				rScreen.WrapperSlider("Wind To Smoke Strength", gWindToSmokeStrength, iSection, 1.0f);
				rScreen.WrapperSlider("Wind To Smoke Power", gWindToSmokePower, iSection, 1.0f);
				rScreen.WrapperSlider("Wind Displacement Noise Scale", gWindDisplacementNoiseScale, iSection, 1.0f);
				rScreen.WrapperSlider("Wind Smoke Retention", gWindSmokeRetention, iSection, 1.0f);
				rScreen.WrapperSlider("Wind Smoke Advection", gWindSmokeAdvection, iSection, 1.0f);

				rScreen.WrapperSeparatorText("Object");
				rScreen.WrapperSlider("Smoke Object Height", gSmokeObjectHeight, iSection, 1.0f);
				rScreen.WrapperSlider("Smoke Object Height Power", gSmokeObjectHeightPower, iSection, 1.0f);

				ImGui::EndTable();
			}

			ImGui::EndTabItem();
		}
		game::RenderSmokeTweakExtensions(rScreen, iSection, 1);
		ImGui::EndTabBar();
	}
}

} // namespace engine

#endif // BT_CLIENT
