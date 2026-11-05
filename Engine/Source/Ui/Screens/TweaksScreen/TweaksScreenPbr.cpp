#include "TweaksScreen.h"

#if defined(BT_CLIENT)

#include "Ui/PbrWrappersBase.h"
#include "TweaksSliderMap.h"

namespace engine
{

const TweaksSliderMapRegistrar gPhysicallyBasedRenderingRegistrar
{
	{"Sun", &gPbrSun},
	{"Day Brightness", &gPbrDayBrightness},
	{"BRDF Diffuse", &gPbrBrdfDiffuse},
	{"BRDF Diffuse Power", &gPbrBrdfDiffusePower},
	{"BRDF Specular", &gPbrBrdfSpecular},
	{"BRDF Specular Power", &gPbrBrdfSpecularPower},
	{"Exposure", &gPbrExposure},
	{"Gamma", &gPbrGamma},
	{"Saturation", &gColorGradingSaturation},
	{"Contrast", &gColorGradingContrast},
	{"Temperature", &gColorGradingTemperature},
	{"Lighting Specular", &gPbrLightingSpecular},
	{"Lighting Specular Power", &gPbrLightingSpecularPower},
	{"Lighting", &gPbrLighting},
	{"Lighting Power", &gPbrLightingPower},
	{"IBL Ambient", &gPbrIblAmbient},
	{"IBL Diffuse", &gPbrIblDiffuse},
	{"IBL Diffuse Power", &gPbrIblDiffusePower},
	{"IBL Specular", &gPbrIblSpecular},
	{"IBL Specular Power", &gPbrIblSpecularPower},
	{"IBL Shadow Blend", &gPbrIblShadowBlend},
	{"Cubemap Lod Power", &gPbrCubemapLodPower},
	{"Cubemap Lod Offset", &gPbrCubemapLodOffset},
	{"Shadow Floor", &gPbrShadowFloor},
	{"Smoke", &gPbrSmoke},
	{"Emissive", &gPbrEmissive},
};

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
				rScreen.WrapperSlider("Pbr Sun", iSection, 1.0f, "Sun");
				rScreen.WrapperSlider("Pbr Day Brightness", iSection, 1.0f, "Day Brightness");

				rScreen.WrapperSeparatorText("BRDF");
				rScreen.WrapperSlider("Pbr Brdf Diffuse", iSection, 1.0f, "BRDF Diffuse");
				rScreen.WrapperSlider("Pbr Brdf Diffuse Power", iSection, 1.0f, "BRDF Diffuse Power");
				rScreen.WrapperSlider("Pbr Brdf Specular", iSection, 1.0f, "BRDF Specular");
				rScreen.WrapperSlider("Pbr Brdf Specular Power", iSection, 1.0f, "BRDF Specular Power");

				rScreen.WrapperSeparatorText("Post Lighting");
				rScreen.WrapperSlider("Pbr Lighting Specular", iSection, 1.0f, "Lighting Specular");
				rScreen.WrapperSlider("Pbr Lighting Specular Power", iSection, 1.0f, "Lighting Specular Power");
				rScreen.WrapperSlider("Pbr Lighting", iSection, 1.0f, "Lighting");
				rScreen.WrapperSlider("Pbr Lighting Power", iSection, 1.0f, "Lighting Power");

				ImGui::TableNextColumn();

				rScreen.WrapperSeparatorText("IBL");
				rScreen.WrapperSlider("Pbr Ibl Ambient", iSection, 1.0f, "IBL Ambient");
				rScreen.WrapperSlider("Pbr Ibl Diffuse", iSection, 1.0f, "IBL Diffuse");
				rScreen.WrapperSlider("Pbr Ibl Diffuse Power", iSection, 1.0f, "IBL Diffuse Power");
				rScreen.WrapperSlider("Pbr Ibl Specular", iSection, 1.0f, "IBL Specular");
				rScreen.WrapperSlider("Pbr Ibl Specular Power", iSection, 1.0f, "IBL Specular Power");
				rScreen.WrapperSlider("Pbr Ibl Shadow Blend", iSection, 1.0f, "IBL Shadow Blend");
				rScreen.WrapperSlider("Pbr Cubemap Lod Power", iSection, 1.0f, "Cubemap Lod Power");
				rScreen.WrapperSlider("Pbr Cubemap Lod Offset", iSection, 1.0f, "Cubemap Lod Offset");
				rScreen.WrapperSlider("Pbr Shadow Floor", iSection, 1.0f, "Shadow Floor");

				rScreen.WrapperSeparatorText("Smoke");
				rScreen.WrapperSlider("Pbr Smoke", iSection, 1.0f, "Smoke");

				rScreen.WrapperSeparatorText("Emissive");
				rScreen.WrapperSlider("Pbr Emissive", iSection, 1.0f, "Emissive");

				ImGui::EndTable();
			}

			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Post Processing", iSection, 1))
		{
			rScreen.WrapperSeparatorText("Tone Mapping");
			rScreen.WrapperSlider("Pbr Exposure", iSection, 2.0f, "Exposure");
			rScreen.WrapperSlider("Pbr Gamma", iSection, 2.0f, "Gamma");

			rScreen.WrapperSeparatorText("Color Grading");
			rScreen.WrapperSlider("Color Grading Saturation", iSection, 2.0f, "Saturation");
			rScreen.WrapperSlider("Color Grading Contrast", iSection, 2.0f, "Contrast");
			rScreen.WrapperSlider("Color Grading Temperature", iSection, 2.0f, "Temperature");

			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

} // namespace engine

#endif // BT_CLIENT
