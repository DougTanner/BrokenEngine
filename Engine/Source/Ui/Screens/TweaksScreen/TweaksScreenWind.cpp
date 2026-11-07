#include "TweaksScreen.h"

#if defined(BT_CLIENT)

#include "Ui/WindWrappersBase.h"

#include "Ui/Screens/TweaksScreen/Tweaks.h"

namespace engine
{

void RenderWindSection(TweaksScreen& rScreen)
{
	int64_t iSection = giTweakSectionWind;

	if (ImGui::BeginTabBar("WindTabs"))
	{
		if (rScreen.BeginSubtab("Wind", iSection, 0))
		{
			rScreen.WrapperSeparatorText("Time & Global");
			rScreen.WrapperSlider("Wind Time Scale", gWindTimeScale, iSection);
			rScreen.WrapperSlider("Wind Threshold Low", gWindThresholdLow, iSection);
			rScreen.WrapperSlider("Wind Threshold High", gWindThresholdHigh, iSection);

			rScreen.WrapperSeparatorText("Propagation");
			if (ImGui::BeginTable("WindPropagation", 2))
			{
				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Advection Scale Low", gWindAdvectionScaleLow, iSection, 1.0f);
				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Advection Scale High", gWindAdvectionScaleHigh, iSection, 1.0f);

				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Swirl Scale Low", gWindSwirlScaleLow, iSection, 1.0f);
				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Swirl Scale High", gWindSwirlScaleHigh, iSection, 1.0f);

				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Swirl Amount Low", gWindSwirlAmountLow, iSection, 1.0f);
				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Swirl Amount High", gWindSwirlAmountHigh, iSection, 1.0f);

				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Swirl Speed Low", gWindSwirlSpeedLow, iSection, 1.0f);
				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Swirl Speed High", gWindSwirlSpeedHigh, iSection, 1.0f);

				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Vorticity Confinement Low", gWindVorticityConfinementLow, iSection, 1.0f);
				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Vorticity Confinement High", gWindVorticityConfinementHigh, iSection, 1.0f);

				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Decay Low", gWindDecayLow, iSection, 1.0f);
				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Decay High", gWindDecayHigh, iSection, 1.0f);

				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Momentum Low", gWindMomentumLow, iSection, 1.0f);
				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Momentum High", gWindMomentumHigh, iSection, 1.0f);

				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Diffusion Low", gWindDiffusionLow, iSection, 1.0f);
				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Diffusion High", gWindDiffusionHigh, iSection, 1.0f);

				ImGui::EndTable();
			}

			ImGui::EndTabItem();
		}
		game::RenderWindTweakExtensions(rScreen, iSection, 1);
		ImGui::EndTabBar();
	}
}

} // namespace engine

#endif // BT_CLIENT
