#include "TweaksScreenBase.h"

#if defined(BT_CLIENT)

#include "Ui/WindWrappersBase.h"
#include "TweaksSliderMap.h"

namespace engine
{

const TweaksSliderMapRegistrar gWindRegistrar
{
	{"Wind Time Scale", &gWindTimeScale},
	{"Wind Threshold Low", &gWindThresholdLow},
	{"Wind Threshold High", &gWindThresholdHigh},
	{"Wind Advection Scale High", &gWindAdvectionScaleHigh},
	{"Wind Advection Scale Low", &gWindAdvectionScaleLow},
	{"Wind Swirl Scale High", &gWindSwirlScaleHigh},
	{"Wind Swirl Scale Low", &gWindSwirlScaleLow},
	{"Wind Swirl Amount High", &gWindSwirlAmountHigh},
	{"Wind Swirl Amount Low", &gWindSwirlAmountLow},
	{"Wind Swirl Speed High", &gWindSwirlSpeedHigh},
	{"Wind Swirl Speed Low", &gWindSwirlSpeedLow},
	{"Wind Vorticity Confinement High", &gWindVorticityConfinementHigh},
	{"Wind Vorticity Confinement Low", &gWindVorticityConfinementLow},
	{"Wind Decay High", &gWindDecayHigh},
	{"Wind Decay Low", &gWindDecayLow},
	{"Wind Momentum High", &gWindMomentumHigh},
	{"Wind Momentum Low", &gWindMomentumLow},
	{"Wind Diffusion High", &gWindDiffusionHigh},
	{"Wind Diffusion Low", &gWindDiffusionLow},
};

void RenderWindSection(TweaksScreenBase& rScreen)
{
	int64_t iSection = giTweakSectionWind;

	if (ImGui::BeginTabBar("WindTabs"))
	{
		if (rScreen.BeginSubtab("Wind", iSection, 0))
		{
			rScreen.WrapperSeparatorText("Time & Global");
			rScreen.WrapperSlider("Wind Time Scale", iSection);
			rScreen.WrapperSlider("Wind Threshold Low", iSection);
			rScreen.WrapperSlider("Wind Threshold High", iSection);

			rScreen.WrapperSeparatorText("Propagation");
			if (ImGui::BeginTable("WindPropagation", 2))
			{
				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Advection Scale Low", iSection, 1.0f);
				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Advection Scale High", iSection, 1.0f);

				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Swirl Scale Low", iSection, 1.0f);
				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Swirl Scale High", iSection, 1.0f);

				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Swirl Amount Low", iSection, 1.0f);
				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Swirl Amount High", iSection, 1.0f);

				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Swirl Speed Low", iSection, 1.0f);
				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Swirl Speed High", iSection, 1.0f);

				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Vorticity Confinement Low", iSection, 1.0f);
				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Vorticity Confinement High", iSection, 1.0f);

				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Decay Low", iSection, 1.0f);
				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Decay High", iSection, 1.0f);

				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Momentum Low", iSection, 1.0f);
				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Momentum High", iSection, 1.0f);

				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Diffusion Low", iSection, 1.0f);
				ImGui::TableNextColumn(); rScreen.WrapperSlider("Wind Diffusion High", iSection, 1.0f);

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
