#include "TweaksScreenBase.h"

#if defined(BT_CLIENT)

#include "Ui/PbrWrappersBase.h"
#include "TweaksSliderMap.h"

namespace engine
{

const TweaksSliderMapRegistrar gPbrRegistrar
{
	// Engine Variables
	{"Sun", &gPbrSun},
	{"Day Brightness", &gPbrDayBrightness},
	{"Model Data Mip Bias", &gPbrModelDataMipLodBias},
	// BRDF
	{"BRDF Diffuse", &gPbrBrdfDiffuse},
	{"BRDF Diffuse Power", &gPbrBrdfDiffusePower},
	{"BRDF Specular", &gPbrBrdfSpecular},
	{"BRDF Specular Power", &gPbrBrdfSpecularPower},
	// Tone Mapping
	{"Exposure", &gPbrExposure},
	{"Gamma", &gPbrGamma},
	// Color Grading
	{"Saturation", &gColorGradingSaturation},
	{"Contrast", &gColorGradingContrast},
	{"Temperature", &gColorGradingTemperature},
	// Post Lighting
	{"Lighting Specular", &gPbrLightingSpecular},
	{"Lighting Specular Power", &gPbrLightingSpecularPower},
	{"Lighting", &gPbrLighting},
	{"Lighting Power", &gPbrLightingPower},
	// IBL
	{"IBL Ambient", &gPbrIblAmbient},
	{"IBL Diffuse", &gPbrIblDiffuse},
	{"IBL Diffuse Power", &gPbrIblDiffusePower},
	{"IBL Specular", &gPbrIblSpecular},
	{"IBL Specular Power", &gPbrIblSpecularPower},
	{"IBL Shadow Blend", &gPbrIblShadowBlend},
	{"IBL Ambient Color Blend", &gPbrIblAmbientColorBlend},
	{"Cubemap Lod Power", &gPbrCubemapLodPower},
	{"Cubemap Lod Offset", &gPbrCubemapLodOffset},
	{"Shadow Floor", &gPbrShadowFloor},
	// Smoke
	{"Smoke", &gPbrSmoke},
	// Emissive
	{"Emissive", &gPbrEmissive},
};

void RenderPbrSection(TweaksScreenBase& rScreen)
{
	int64_t iSection = giTweakSectionPbr;

	if (ImGui::BeginTable("PbrColumns", 2))
	{
		// Left column
		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("Engine Variables");
		rScreen.WrapperSlider("Sun", iSection, 1.0f);
		rScreen.WrapperSlider("Day Brightness", iSection, 1.0f);
		rScreen.WrapperSlider("Model Data Mip Bias", iSection, 1.0f);

		rScreen.WrapperSeparatorText("BRDF");
		rScreen.WrapperSlider("BRDF Diffuse", iSection, 1.0f);
		rScreen.WrapperSlider("BRDF Diffuse Power", iSection, 1.0f);
		rScreen.WrapperSlider("BRDF Specular", iSection, 1.0f);
		rScreen.WrapperSlider("BRDF Specular Power", iSection, 1.0f);

		rScreen.WrapperSeparatorText("Tone Mapping");
		rScreen.WrapperSlider("Exposure", iSection, 1.0f);
		rScreen.WrapperSlider("Gamma", iSection, 1.0f);

		rScreen.WrapperSeparatorText("Color Grading");
		rScreen.WrapperSlider("Saturation", iSection, 1.0f);
		rScreen.WrapperSlider("Contrast", iSection, 1.0f);
		rScreen.WrapperSlider("Temperature", iSection, 1.0f);

		rScreen.WrapperSeparatorText("Post Lighting");
		rScreen.WrapperSlider("Lighting Specular", iSection, 1.0f);
		rScreen.WrapperSlider("Lighting Specular Power", iSection, 1.0f);
		rScreen.WrapperSlider("Lighting", iSection, 1.0f);
		rScreen.WrapperSlider("Lighting Power", iSection, 1.0f);

		// Right column
		ImGui::TableNextColumn();

		rScreen.WrapperSeparatorText("IBL");
		rScreen.WrapperSlider("IBL Ambient", iSection, 1.0f);
		rScreen.WrapperSlider("IBL Diffuse", iSection, 1.0f);
		rScreen.WrapperSlider("IBL Diffuse Power", iSection, 1.0f);
		rScreen.WrapperSlider("IBL Specular", iSection, 1.0f);
		rScreen.WrapperSlider("IBL Specular Power", iSection, 1.0f);
		rScreen.WrapperSlider("IBL Shadow Blend", iSection, 1.0f);
		rScreen.WrapperSlider("IBL Ambient Color Blend", iSection, 1.0f);
		rScreen.WrapperSlider("Cubemap Lod Power", iSection, 1.0f);
		rScreen.WrapperSlider("Cubemap Lod Offset", iSection, 1.0f);
		rScreen.WrapperSlider("Shadow Floor", iSection, 1.0f);

		rScreen.WrapperSeparatorText("Smoke");
		rScreen.WrapperSlider("Smoke", iSection, 1.0f);

		rScreen.WrapperSeparatorText("Emissive");
		rScreen.WrapperSlider("Emissive", iSection, 1.0f);

		ImGui::EndTable();
	}
}

} // namespace engine

#endif // BT_CLIENT
