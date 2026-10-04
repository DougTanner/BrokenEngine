#include "TweaksScreenBase.h"

#if defined(BT_CLIENT)

#include "Ui/PbrWrappersBase.h"
#include "TweaksSliderMap.h"

namespace engine
{

const TweaksSliderMapRegistrar gPhysicallyBasedRenderingRegistrar
{
	{"Sun", &gPhysicallyBasedRenderingSun},
	{"Day Brightness", &gPhysicallyBasedRenderingDayBrightness},
	{"Model Data Mip Bias", &gPhysicallyBasedRenderingModelDataMipmapLevelOfDetailBias},
	{"BRDF Diffuse", &gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionDiffuse},
	{"BRDF Diffuse Power", &gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionDiffusePower},
	{"BRDF Specular", &gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionSpecular},
	{"BRDF Specular Power", &gPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionSpecularPower},
	{"Exposure", &gPhysicallyBasedRenderingExposure},
	{"Gamma", &gPhysicallyBasedRenderingGamma},
	{"Saturation", &gColorGradingSaturation},
	{"Contrast", &gColorGradingContrast},
	{"Temperature", &gColorGradingTemperature},
	{"Lighting Specular", &gPhysicallyBasedRenderingLightingSpecular},
	{"Lighting Specular Power", &gPhysicallyBasedRenderingLightingSpecularPower},
	{"Lighting", &gPhysicallyBasedRenderingLighting},
	{"Lighting Power", &gPhysicallyBasedRenderingLightingPower},
	{"IBL Ambient", &gPhysicallyBasedRenderingImageBasedLightingAmbient},
	{"IBL Diffuse", &gPhysicallyBasedRenderingImageBasedLightingDiffuse},
	{"IBL Diffuse Power", &gPhysicallyBasedRenderingImageBasedLightingDiffusePower},
	{"IBL Specular", &gPhysicallyBasedRenderingImageBasedLightingSpecular},
	{"IBL Specular Power", &gPhysicallyBasedRenderingImageBasedLightingSpecularPower},
	{"IBL Shadow Blend", &gPhysicallyBasedRenderingImageBasedLightingShadowBlend},
	{"IBL Ambient Color Blend", &gPhysicallyBasedRenderingImageBasedLightingAmbientColorBlend},
	{"Cubemap Lod Power", &gPhysicallyBasedRenderingCubemapLevelOfDetailPower},
	{"Cubemap Lod Offset", &gPhysicallyBasedRenderingCubemapLevelOfDetailOffset},
	{"Shadow Floor", &gPhysicallyBasedRenderingShadowFloor},
	{"Smoke", &gPhysicallyBasedRenderingSmoke},
	{"Emissive", &gPhysicallyBasedRenderingEmissive},
};

void RenderPhysicallyBasedRenderingSection(TweaksScreenBase& rScreen)
{
	int64_t iSection = giTweakSectionPbr;

	if (ImGui::BeginTable("PbrColumns", 2))
	{
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
