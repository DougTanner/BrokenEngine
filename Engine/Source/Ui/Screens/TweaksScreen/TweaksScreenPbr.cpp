#include "TweaksScreen.h"

#if defined(BT_CLIENT)

#include "Ui/PbrWrappersBase.h"

namespace engine
{

void RenderPhysicallyBasedRenderingSection(TweaksScreen& rScreen)
{
	int64_t iSection = giTweakSectionPbr;

	if (ImGui::BeginTabBar("PbrTabs"))
	{
		if (rScreen.BeginSubtab("Model", iSection, 0))
		{
			if (ImGui::BeginTable("PbrColumns", 2))
			{
				ImGui::TableNextColumn();

				rScreen.WrapperSeparatorText("Engine Variables");
				rScreen.WrapperSlider("Pbr Sun", gPbrSun, iSection, 1.0f);
				rScreen.WrapperSlider("Pbr Day Brightness", gPbrDayBrightness, iSection, 1.0f);

				rScreen.WrapperSeparatorText("BRDF");
				rScreen.WrapperSlider("Pbr Brdf Diffuse", gPbrBrdfDiffuse, iSection, 1.0f);
				rScreen.WrapperSlider("Pbr Brdf Diffuse Power", gPbrBrdfDiffusePower, iSection, 1.0f);
				rScreen.WrapperSlider("Pbr Brdf Specular", gPbrBrdfSpecular, iSection, 1.0f);
				rScreen.WrapperSlider("Pbr Brdf Specular Power", gPbrBrdfSpecularPower, iSection, 1.0f);

				rScreen.WrapperSeparatorText("Post Lighting");
				rScreen.WrapperSlider("Pbr Lighting Specular", gPbrLightingSpecular, iSection, 1.0f);
				rScreen.WrapperSlider("Pbr Lighting Specular Power", gPbrLightingSpecularPower, iSection, 1.0f);
				rScreen.WrapperSlider("Pbr Lighting", gPbrLighting, iSection, 1.0f);
				rScreen.WrapperSlider("Pbr Lighting Power", gPbrLightingPower, iSection, 1.0f);

				ImGui::TableNextColumn();

				rScreen.WrapperSeparatorText("IBL");
				rScreen.WrapperSlider("Pbr Ibl Ambient", gPbrIblAmbient, iSection, 1.0f);
				rScreen.WrapperSlider("Pbr Ibl Diffuse", gPbrIblDiffuse, iSection, 1.0f);
				rScreen.WrapperSlider("Pbr Ibl Diffuse Power", gPbrIblDiffusePower, iSection, 1.0f);
				rScreen.WrapperSlider("Pbr Ibl Specular", gPbrIblSpecular, iSection, 1.0f);
				rScreen.WrapperSlider("Pbr Ibl Specular Power", gPbrIblSpecularPower, iSection, 1.0f);
				rScreen.WrapperSlider("Pbr Ibl Shadow Blend", gPbrIblShadowBlend, iSection, 1.0f);
				rScreen.WrapperSlider("Pbr Cubemap Lod Power", gPbrCubemapLodPower, iSection, 1.0f);
				rScreen.WrapperSlider("Pbr Cubemap Lod Offset", gPbrCubemapLodOffset, iSection, 1.0f);
				rScreen.WrapperSlider("Pbr Shadow Floor", gPbrShadowFloor, iSection, 1.0f);

				rScreen.WrapperSeparatorText("Smoke");
				rScreen.WrapperSlider("Pbr Smoke", gPbrSmoke, iSection, 1.0f);

				rScreen.WrapperSeparatorText("Emissive");
				rScreen.WrapperSlider("Pbr Emissive", gPbrEmissive, iSection, 1.0f);

				ImGui::EndTable();
			}

			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Post Processing", iSection, 1))
		{
			rScreen.WrapperSeparatorText("Tone Mapping");
			rScreen.WrapperSlider("Pbr Exposure", gPbrExposure, iSection);
			rScreen.WrapperSlider("Pbr Gamma", gPbrGamma, iSection);

			rScreen.WrapperSeparatorText("Color Grading");
			rScreen.WrapperSlider("Color Grading Saturation", gColorGradingSaturation, iSection);
			rScreen.WrapperSlider("Color Grading Contrast", gColorGradingContrast, iSection);
			rScreen.WrapperSlider("Color Grading Temperature", gColorGradingTemperature, iSection);

			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

} // namespace engine

#endif // BT_CLIENT
