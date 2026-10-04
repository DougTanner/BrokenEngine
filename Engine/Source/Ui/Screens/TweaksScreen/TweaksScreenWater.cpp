#include "TweaksScreenBase.h"

#if defined(BT_CLIENT)

#include "Graphics/Managers/TextureManager.h"
#include "Ui/WaterWrappersBase.h"
#include "TweaksSliderMap.h"

namespace engine
{

const TweaksSliderMapRegistrar gWaterRegistrar
{
	// Specular - Normals (per-sample row layout: chevron | size | weight-min | weight-max | rotation | speed-min | speed-max | speed-direction)
	{"Size 1", &gLightingSampledNormalsOneSize},
	{"Weight Min 1", &gLightingSampledNormalsWeightOneMinimum},
	{"Weight Max 1", &gLightingSampledNormalsWeightOneMaximum},
	{"Rotation 1", &gWaterNormalRotationOne},
	{"Speed Min 1", &gLightingSampledNormalsSpeedOneMinimum},
	{"Speed Max 1", &gLightingSampledNormalsSpeedOneMaximum},
	{"Speed Direction 1", &gWaterNormalSpeedDirectionOne},
	{"Size 2", &gLightingSampledNormalsTwoSize},
	{"Weight Min 2", &gLightingSampledNormalsWeightTwoMinimum},
	{"Weight Max 2", &gLightingSampledNormalsWeightTwoMaximum},
	{"Rotation 2", &gWaterNormalRotationTwo},
	{"Speed Min 2", &gLightingSampledNormalsSpeedTwoMinimum},
	{"Speed Max 2", &gLightingSampledNormalsSpeedTwoMaximum},
	{"Speed Direction 2", &gWaterNormalSpeedDirectionTwo},
	{"Size 3", &gLightingSampledNormalsThreeSize},
	{"Weight Min 3", &gLightingSampledNormalsWeightThreeMinimum},
	{"Weight Max 3", &gLightingSampledNormalsWeightThreeMaximum},
	{"Rotation 3", &gWaterNormalRotationThree},
	{"Speed Min 3", &gLightingSampledNormalsSpeedThreeMinimum},
	{"Speed Max 3", &gLightingSampledNormalsSpeedThreeMaximum},
	{"Speed Direction 3", &gWaterNormalSpeedDirectionThree},
	{"Depth Reflection Feather", &gWaterDepthReflectionFeather},
	{"Wave Normal Blend (Global)", &gWaterWaveNormalBlend},
	// Specular - Skybox
	{"Sun Bias", &gLightingWaterSkyboxSunBias},
	{"Normal Soften Sunrise", &gLightingWaterSkyboxNormalSoftenSunrise},
	{"Normal Soften Noon", &gLightingWaterSkyboxNormalSoftenNoon},
	{"Normal Blend Wave", &gLightingWaterSkyboxNormalBlendWave},
	{"Intensity", &gLightingWaterSkyboxIntensity},
	{"Add", &gLightingWaterSkyboxAdd},
	{"Skybox 1", &gLightingWaterSkyboxOne},
	{"Skybox 1 Power", &gLightingWaterSkyboxOnePower},
	{"Skybox 2", &gLightingWaterSkyboxTwo},
	{"Skybox 2 Power", &gLightingWaterSkyboxTwoPower},
	{"Skybox 3", &gLightingWaterSkyboxThree},
	{"Skybox 3 Power", &gLightingWaterSkyboxThreePower},
	{"Skybox Lod", &gLightingWaterSkyboxLevelOfDetail},
	{"Spec AA Variance", &gWaterSpecularAntialiasingVariance},
	{"Spec AA Threshold", &gWaterSpecularAntialiasingThreshold},
	{"Spec AA Mip Scale", &gWaterSpecularAntialiasingMipmapScale},
	{"Normal Mip Bias", &gWaterNormalMipmapBias},
	// Specular - Height Darken
	{"Height Darken Top", &gWaterHeightDarkenTop},
	{"Height Darken Bottom", &gWaterHeightDarkenBottom},
	{"Height Darken Target", &gWaterHeightDarkenTarget},
	{"Height Darken Source", &gWaterHeightDarkenSource},
	{"Height Darken Lighting", &gWaterHeightDarkenLighting},
	// Low - Wave
	{"Low Max", &gWaterLowMaximum},
	{"Angle", &gWaterLowAngle},
	{"Wavelength", &gWaterLowWavelength},
	{"Amplitude", &gWaterLowAmplitude},
	{"Speed", &gWaterLowSpeed},
	{"Steepness", &gWaterLowSteepness},
	// Low - Adjustments
	{"Angle Adjust", &gWaterLowAngleAdjust},
	{"Wavelength Adjust", &gWaterLowWavelengthAdjust},
	{"Amplitude Adjust", &gWaterLowAmplitudeAdjust},
	{"Speed Adjust", &gWaterLowSpeedAdjust},
	// Low - Camera Fade
	{"Low Camera Fade Start", &gWaterLowAmplitudeFadeStart},
	{"Low Camera Fade End", &gWaterLowAmplitudeFadeEnd},
	// Medium - Wave
	{"Medium Wavelength", &gWaterMediumWavelength},
	{"Medium Amplitude", &gWaterMediumAmplitude},
	{"Medium Speed", &gWaterMediumSpeed},
	{"Medium Steepness", &gWaterMediumSteepness},
	// Medium - Adjustments
	{"Medium Angle Adjust", &gWaterMediumAngleAdjust},
	{"Medium Wavelength Adjust", &gWaterMediumWavelengthAdjust},
	{"Medium Amplitude Adjust", &gWaterMediumAmplitudeAdjust},
	{"Medium Speed Adjust", &gWaterMediumSpeedAdjust},
	// Medium - Camera Fade
	{"Medium Camera Fade Start", &gWaterMediumAmplitudeFadeStart},
	{"Medium Camera Fade End", &gWaterMediumAmplitudeFadeEnd},
	// Depth
	{"Water Terrain Height", &gWaterTerrainHeight},
	{"Water Terrain Fade", &gWaterTerrainFade},
	{"Water Terrain Fade Clamp", &gWaterTerrainFadeClamp},
	{"Water Height", &gWaterHeight},
	{"Water Early Out", &gWaterEarlyOut},
	{"Water Depth Lut Feather", &gWaterDepthLookupTableFeather},
	{"Water Depth Lut Sunset Fade Power", &gWaterDepthLookupTableSunsetFadePower},
	{"Water Depth Lut Sunset Fade Intensity", &gWaterDepthLookupTableSunsetFadeIntensity},
	{"Water Depth Color Feather", &gWaterDepthColorFeather},
	{"Water Depth Color Floor", &gWaterDepthColorFloor},
	{"Undersea Compression", &gWaterUnderseaCompression},
	{"Water Color Bottom", &gWaterColorBottom},
	{"Water Color Height", &gWaterColorHeight},
	{"Water Fresnel", &gWaterFresnel},
	{"Water Color Noise Frequency", &gWaterColorNoiseFrequency},
	{"Water Color Noise Amount", &gWaterColorNoiseAmount},
	{"Water Color Noise Weight One", &gWaterColorNoiseWeightOne},
	{"Water Color Noise Multiplier One", &gWaterColorNoiseMultiplierOne},
	{"Water Color Noise Weight Two", &gWaterColorNoiseWeightTwo},
	{"Water Color Noise Multiplier Two", &gWaterColorNoiseMultiplierTwo},
	// Beach
	{"Break Start Depth", &gWaterBreakStartDepth},
	{"Break End Depth", &gWaterBreakEndDepth},
	{"Break Blend Curve", &gWaterBreakBlendCurve},
	{"Medium Shore Softness", &gWaterMediumShoreSoftness},
	{"Beach Skybox 1 Reduction", &gLightingWaterSkyboxOneBeachReduction},
	{"Beach Skybox 2 Reduction", &gLightingWaterSkyboxTwoBeachReduction},
	{"Beach Skybox 3 Reduction", &gLightingWaterSkyboxThreeBeachReduction},
};

void RenderWaterSection(TweaksScreenBase& rScreen)
{
	int64_t iSection = giTweakSectionWater;

	if (ImGui::BeginTabBar("WaterTabs"))
	{
		if (rScreen.BeginSubtab("Specular", iSection, 0))
		{
			if (ImGui::BeginTable("WaterSpecularColumns", 2))
			{
				ImGui::TableNextColumn();

				rScreen.WrapperSeparatorText("Normals");
				rScreen.ChevronIndexSelector("Sample 1", gWaterNormalIndexOne, TextureManager::kpWaterNormalNames);
				rScreen.WrapperSlider("Size 1", iSection, 1.0f);
				rScreen.WrapperSlider("Weight Min 1", iSection, 1.0f);
				rScreen.WrapperSlider("Weight Max 1", iSection, 1.0f);
				rScreen.WrapperSlider("Rotation 1", iSection, 1.0f);
				rScreen.WrapperSlider("Speed Min 1", iSection, 1.0f);
				rScreen.WrapperSlider("Speed Max 1", iSection, 1.0f);
				rScreen.WrapperSlider("Speed Direction 1", iSection, 1.0f);

				rScreen.ChevronIndexSelector("Sample 2", gWaterNormalIndexTwo, TextureManager::kpWaterNormalNames);
				rScreen.WrapperSlider("Size 2", iSection, 1.0f);
				rScreen.WrapperSlider("Weight Min 2", iSection, 1.0f);
				rScreen.WrapperSlider("Weight Max 2", iSection, 1.0f);
				rScreen.WrapperSlider("Rotation 2", iSection, 1.0f);
				rScreen.WrapperSlider("Speed Min 2", iSection, 1.0f);
				rScreen.WrapperSlider("Speed Max 2", iSection, 1.0f);
				rScreen.WrapperSlider("Speed Direction 2", iSection, 1.0f);

				rScreen.ChevronIndexSelector("Sample 3", gWaterNormalIndexThree, TextureManager::kpWaterNormalNames);
				rScreen.WrapperSlider("Size 3", iSection, 1.0f);
				rScreen.WrapperSlider("Weight Min 3", iSection, 1.0f);
				rScreen.WrapperSlider("Weight Max 3", iSection, 1.0f);
				rScreen.WrapperSlider("Rotation 3", iSection, 1.0f);
				rScreen.WrapperSlider("Speed Min 3", iSection, 1.0f);
				rScreen.WrapperSlider("Speed Max 3", iSection, 1.0f);
				rScreen.WrapperSlider("Speed Direction 3", iSection, 1.0f);

				rScreen.WrapperSlider("Depth Reflection Feather", iSection, 1.0f);
				rScreen.WrapperSlider("Wave Normal Blend (Global)", iSection, 1.0f);

				ImGui::TableNextColumn();

				rScreen.WrapperSeparatorText("Skybox");
				rScreen.WrapperSlider("Sun Bias", iSection, 1.0f);
				rScreen.WrapperSlider("Normal Soften Sunrise", iSection, 1.0f);
				rScreen.WrapperSlider("Normal Soften Noon", iSection, 1.0f);
				rScreen.WrapperSlider("Normal Blend Wave", iSection, 1.0f);
				rScreen.WrapperSlider("Intensity", iSection, 1.0f);
				rScreen.WrapperSlider("Add", iSection, 1.0f);
				rScreen.WrapperSlider("Skybox 1", iSection, 1.0f);
				rScreen.WrapperSlider("Skybox 1 Power", iSection, 1.0f);
				rScreen.WrapperSlider("Skybox 2", iSection, 1.0f);
				rScreen.WrapperSlider("Skybox 2 Power", iSection, 1.0f);
				rScreen.WrapperSlider("Skybox 3", iSection, 1.0f);
				rScreen.WrapperSlider("Skybox 3 Power", iSection, 1.0f);
				rScreen.WrapperSlider("Skybox Lod", iSection, 1.0f);
				rScreen.WrapperSlider("Spec AA Variance", iSection, 1.0f);
				rScreen.WrapperSlider("Spec AA Threshold", iSection, 1.0f);
				rScreen.WrapperSlider("Spec AA Mip Scale", iSection, 1.0f);
				rScreen.WrapperSlider("Normal Mip Bias", iSection, 1.0f);

				rScreen.WrapperSeparatorText("Height Darken");
				rScreen.WrapperSlider("Height Darken Top", iSection, 1.0f);
				rScreen.WrapperSlider("Height Darken Bottom", iSection, 1.0f);
				rScreen.WrapperSlider("Height Darken Target", iSection, 1.0f);
				rScreen.WrapperSlider("Height Darken Source", iSection, 1.0f);
				rScreen.WrapperSlider("Height Darken Lighting", iSection, 1.0f);

				ImGui::EndTable();
			}
			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Low", iSection, 1))
		{
			rScreen.RenderWaveCountRadioButtons(gWaterLowCount);

			rScreen.WrapperSeparatorText("Wave");
			rScreen.WrapperSlider("Low Max", iSection);
			rScreen.WrapperSlider("Angle", iSection);
			rScreen.WrapperSlider("Wavelength", iSection);
			rScreen.WrapperSlider("Amplitude", iSection);
			rScreen.WrapperSlider("Speed", iSection);
			rScreen.WrapperSlider("Steepness", iSection);

			rScreen.WrapperSeparatorText("Adjustments");
			rScreen.WrapperSlider("Angle Adjust", iSection);
			rScreen.WrapperSlider("Wavelength Adjust", iSection);
			rScreen.WrapperSlider("Amplitude Adjust", iSection);
			rScreen.WrapperSlider("Speed Adjust", iSection);

			rScreen.WrapperSeparatorText("Camera Fade");
			rScreen.WrapperSlider("Low Camera Fade Start", iSection);
			rScreen.WrapperSlider("Low Camera Fade End", iSection);

			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Medium", iSection, 2))
		{
			rScreen.RenderWaveCountRadioButtons(gWaterMediumCount);

			rScreen.WrapperSeparatorText("Wave");
			rScreen.WrapperSlider("Medium Wavelength", iSection);
			rScreen.WrapperSlider("Medium Amplitude", iSection);
			rScreen.WrapperSlider("Medium Speed", iSection);
			rScreen.WrapperSlider("Medium Steepness", iSection);

			rScreen.WrapperSeparatorText("Adjustments");
			rScreen.WrapperSlider("Medium Angle Adjust", iSection);
			rScreen.WrapperSlider("Medium Wavelength Adjust", iSection);
			rScreen.WrapperSlider("Medium Amplitude Adjust", iSection);
			rScreen.WrapperSlider("Medium Speed Adjust", iSection);

			rScreen.WrapperSeparatorText("Camera Fade");
			rScreen.WrapperSlider("Medium Camera Fade Start", iSection);
			rScreen.WrapperSlider("Medium Camera Fade End", iSection);

			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Depth", iSection, 3))
		{
			rScreen.WrapperSeparatorText("Terrain Fade");
			rScreen.WrapperSlider("Water Terrain Height", iSection);
			rScreen.WrapperSlider("Water Terrain Fade", iSection);
			rScreen.WrapperSlider("Water Terrain Fade Clamp", iSection);

			rScreen.WrapperSeparatorText("Surface");
			rScreen.WrapperSlider("Water Height", iSection);
			rScreen.WrapperSlider("Water Early Out", iSection);

			rScreen.WrapperSeparatorText("Depth Color");
			rScreen.WrapperSlider("Water Depth Lut Feather", iSection);
			rScreen.WrapperSlider("Water Depth Lut Sunset Fade Power", iSection);
			rScreen.WrapperSlider("Water Depth Lut Sunset Fade Intensity", iSection);
			rScreen.WrapperSlider("Water Depth Color Feather", iSection);
			rScreen.WrapperSlider("Water Depth Color Floor", iSection);
			rScreen.WrapperSlider("Undersea Compression", iSection);
			rScreen.WrapperSlider("Water Color Bottom", iSection);
			rScreen.WrapperSlider("Water Color Height", iSection);

			rScreen.WrapperSeparatorText("Fresnel");
			rScreen.WrapperSlider("Water Fresnel", iSection);

			rScreen.WrapperSeparatorText("Color Noise");
			rScreen.WrapperSlider("Water Color Noise Frequency", iSection);
			rScreen.WrapperSlider("Water Color Noise Amount", iSection);
			rScreen.WrapperSlider("Water Color Noise Weight One", iSection);
			rScreen.WrapperSlider("Water Color Noise Multiplier One", iSection);
			rScreen.WrapperSlider("Water Color Noise Weight Two", iSection);
			rScreen.WrapperSlider("Water Color Noise Multiplier Two", iSection);

			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Beach", iSection, 4))
		{
			rScreen.WrapperSeparatorText("Breaking");
			rScreen.WrapperSlider("Break Start Depth", iSection);
			rScreen.WrapperSlider("Break End Depth", iSection);
			rScreen.WrapperSlider("Break Blend Curve", iSection);
			rScreen.WrapperSlider("Medium Shore Softness", iSection);

			rScreen.WrapperSeparatorText("Skybox Lobe Reduction");
			rScreen.WrapperSlider("Beach Skybox 1 Reduction", iSection);
			rScreen.WrapperSlider("Beach Skybox 2 Reduction", iSection);
			rScreen.WrapperSlider("Beach Skybox 3 Reduction", iSection);

			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

} // namespace engine

#endif // BT_CLIENT
