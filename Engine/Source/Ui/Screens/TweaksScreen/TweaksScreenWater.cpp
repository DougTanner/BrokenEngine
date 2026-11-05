#include "TweaksScreen.h"

#if defined(BT_CLIENT)

#include "Graphics/Managers/TextureManager.h"
#include "Ui/MenuUtils.h"
#include "Ui/WaterWrappersBase.h"
#include "TweaksSliderMap.h"

namespace engine
{

const TweaksSliderMapRegistrar gWaterRegistrar
{
	// Global
	{"Water Scale", &gWaterScale},
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
	{"Skybox 1 Night", &gLightingWaterSkyboxOneNight},
	{"Skybox 1 Power", &gLightingWaterSkyboxOnePower},
	{"Skybox 2", &gLightingWaterSkyboxTwo},
	{"Skybox 2 Night", &gLightingWaterSkyboxTwoNight},
	{"Skybox 2 Power", &gLightingWaterSkyboxTwoPower},
	{"Skybox 3", &gLightingWaterSkyboxThree},
	{"Skybox 3 Night", &gLightingWaterSkyboxThreeNight},
	{"Skybox 3 Power", &gLightingWaterSkyboxThreePower},
	{"Skybox Lod", &gLightingWaterSkyboxLevelOfDetail},
	// Specular - Skybox One Sampling
	{"Water Skybox One Sample Count", &gWaterSkyboxOneSampleCount},
	{"Water Skybox One Sample Radius", &gWaterSkyboxOneSampleRadius},
	{"Water Skybox One Sample Blend", &gWaterSkyboxOneSampleBlend},
	// Specular - Height Darken
	{"Height Darken Top", &gWaterHeightDarkenTop},
	{"Height Darken Bottom", &gWaterHeightDarkenBottom},
	{"Height Darken Target", &gWaterHeightDarkenTarget},
	{"Height Darken Lighting", &gWaterHeightDarkenLighting},
	// Low - Wave
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
	// Foam
	{"Foam Emission Rate", &gWaterFoamEmissionRate},
	{"Foam J Start", &gWaterFoamJStart},
	{"Foam J Full", &gWaterFoamJFull},
	{"Foam Half Life", &gWaterFoamHalfLife},
	{"Foam Texture Tile Size", &gWaterFoamTextureTileSize},
	{"Foam Texture Strength", &gWaterFoamTextureStrength},
	{"Foam Breakup Tile Size", &gWaterFoamBreakupTileSize},
	{"Foam Breakup Amount", &gWaterFoamBreakupAmount},
	{"Foam Spawn Noise Amount", &gWaterFoamSpawnNoiseAmount},
	{"Foam Spawn Noise Tile Size", &gWaterFoamSpawnNoiseTileSize},
};

void RenderWaterSection(TweaksScreen& rScreen)
{
	int64_t iSection = giTweakSectionWater;

	rScreen.WrapperSlider("Water Scale", iSection);

	if (ImGui::BeginTabBar("WaterTabs"))
	{
		if (rScreen.BeginSubtab("Specular", iSection, 0))
		{
			if (ImGui::BeginTable("WaterSpecularColumns", 2))
			{
				ImGui::TableNextColumn();

				rScreen.WrapperSeparatorText("Normals");
				rScreen.ChevronIndexSelector("Sample 1", gWaterNormalIndexOne, TextureManager::kpWaterNormalNames);
				rScreen.WrapperSlider("Lighting Sampled Normals One Size", iSection, 1.0f, "Size 1");
				rScreen.WrapperSlider("Lighting Sampled Normals Weight One Minimum", iSection, 1.0f, "Weight Min 1");
				rScreen.WrapperSlider("Lighting Sampled Normals Weight One Maximum", iSection, 1.0f, "Weight Max 1");
				rScreen.WrapperSlider("Water Normal Rotation One", iSection, 1.0f, "Rotation 1");
				rScreen.WrapperSlider("Lighting Sampled Normals Speed One Minimum", iSection, 1.0f, "Speed Min 1");
				rScreen.WrapperSlider("Lighting Sampled Normals Speed One Maximum", iSection, 1.0f, "Speed Max 1");
				rScreen.WrapperSlider("Water Normal Speed Direction One", iSection, 1.0f, "Speed Direction 1");

				rScreen.ChevronIndexSelector("Sample 2", gWaterNormalIndexTwo, TextureManager::kpWaterNormalNames);
				rScreen.WrapperSlider("Lighting Sampled Normals Two Size", iSection, 1.0f, "Size 2");
				rScreen.WrapperSlider("Lighting Sampled Normals Weight Two Minimum", iSection, 1.0f, "Weight Min 2");
				rScreen.WrapperSlider("Lighting Sampled Normals Weight Two Maximum", iSection, 1.0f, "Weight Max 2");
				rScreen.WrapperSlider("Water Normal Rotation Two", iSection, 1.0f, "Rotation 2");
				rScreen.WrapperSlider("Lighting Sampled Normals Speed Two Minimum", iSection, 1.0f, "Speed Min 2");
				rScreen.WrapperSlider("Lighting Sampled Normals Speed Two Maximum", iSection, 1.0f, "Speed Max 2");
				rScreen.WrapperSlider("Water Normal Speed Direction Two", iSection, 1.0f, "Speed Direction 2");

				rScreen.ChevronIndexSelector("Sample 3", gWaterNormalIndexThree, TextureManager::kpWaterNormalNames);
				rScreen.WrapperSlider("Lighting Sampled Normals Three Size", iSection, 1.0f, "Size 3");
				rScreen.WrapperSlider("Lighting Sampled Normals Weight Three Minimum", iSection, 1.0f, "Weight Min 3");
				rScreen.WrapperSlider("Lighting Sampled Normals Weight Three Maximum", iSection, 1.0f, "Weight Max 3");
				rScreen.WrapperSlider("Water Normal Rotation Three", iSection, 1.0f, "Rotation 3");
				rScreen.WrapperSlider("Lighting Sampled Normals Speed Three Minimum", iSection, 1.0f, "Speed Min 3");
				rScreen.WrapperSlider("Lighting Sampled Normals Speed Three Maximum", iSection, 1.0f, "Speed Max 3");
				rScreen.WrapperSlider("Water Normal Speed Direction Three", iSection, 1.0f, "Speed Direction 3");

				rScreen.WrapperSlider("Water Depth Reflection Feather", iSection, 1.0f, "Depth Reflection Feather");
				rScreen.WrapperSlider("Water Wave Normal Blend", iSection, 1.0f, "Wave Normal Blend (Global)");

				ImGui::TableNextColumn();

				rScreen.WrapperSeparatorText("Skybox");
				rScreen.WrapperSlider("Lighting Water Skybox Sun Bias", iSection, 1.0f, "Sun Bias");
				rScreen.WrapperSlider("Lighting Water Skybox Normal Soften Sunrise", iSection, 1.0f, "Normal Soften Sunrise");
				rScreen.WrapperSlider("Lighting Water Skybox Normal Soften Noon", iSection, 1.0f, "Normal Soften Noon");
				rScreen.WrapperSlider("Lighting Water Skybox Normal Blend Wave", iSection, 1.0f, "Normal Blend Wave");
				rScreen.WrapperSlider("Lighting Water Skybox Intensity", iSection, 1.0f, "Intensity");
				rScreen.WrapperSlider("Lighting Water Skybox Add", iSection, 1.0f, "Add");
				rScreen.WrapperSlider("Lighting Water Skybox One", iSection, 1.0f, "Skybox 1");
				rScreen.WrapperSlider("Lighting Water Skybox One Night", iSection, 1.0f, "Skybox 1 Night");
				rScreen.WrapperSlider("Lighting Water Skybox One Power", iSection, 1.0f, "Skybox 1 Power");
				rScreen.WrapperSlider("Lighting Water Skybox Two", iSection, 1.0f, "Skybox 2");
				rScreen.WrapperSlider("Lighting Water Skybox Two Night", iSection, 1.0f, "Skybox 2 Night");
				rScreen.WrapperSlider("Lighting Water Skybox Two Power", iSection, 1.0f, "Skybox 2 Power");
				rScreen.WrapperSlider("Lighting Water Skybox Three", iSection, 1.0f, "Skybox 3");
				rScreen.WrapperSlider("Lighting Water Skybox Three Night", iSection, 1.0f, "Skybox 3 Night");
				rScreen.WrapperSlider("Lighting Water Skybox Three Power", iSection, 1.0f, "Skybox 3 Power");
				rScreen.WrapperSlider("Lighting Water Skybox Level Of Detail", iSection, 1.0f, "Skybox Lod");

				rScreen.WrapperSeparatorText("Skybox One Sampling");
				rScreen.WrapperSlider("Water Skybox One Sample Count", iSection, 1.0f);
				rScreen.WrapperSlider("Water Skybox One Sample Radius", iSection, 1.0f);
				rScreen.WrapperSlider("Water Skybox One Sample Blend", iSection, 1.0f);

				rScreen.WrapperSeparatorText("Height Darken");
				rScreen.WrapperSlider("Water Height Darken Top", iSection, 1.0f, "Height Darken Top");
				rScreen.WrapperSlider("Water Height Darken Bottom", iSection, 1.0f, "Height Darken Bottom");
				rScreen.WrapperSlider("Water Height Darken Target", iSection, 1.0f, "Height Darken Target");
				rScreen.WrapperSlider("Water Height Darken Lighting", iSection, 1.0f, "Height Darken Lighting");

				ImGui::EndTable();
			}
			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Low", iSection, 1))
		{
			rScreen.RenderWaveCountRadioButtons(gWaterLowCount);

			rScreen.WrapperSeparatorText("Wave");
			rScreen.WrapperSlider("Water Low Angle", iSection, 2.0f, "Angle");
			rScreen.WrapperSlider("Water Low Wavelength", iSection, 2.0f, "Wavelength");
			rScreen.WrapperSlider("Water Low Amplitude", iSection, 2.0f, "Amplitude");
			rScreen.WrapperSlider("Water Low Speed", iSection, 2.0f, "Speed");
			rScreen.WrapperSlider("Water Low Steepness", iSection, 2.0f, "Steepness");

			rScreen.WrapperSeparatorText("Adjustments");
			rScreen.WrapperSlider("Water Low Angle Adjust", iSection, 2.0f, "Angle Adjust");
			rScreen.WrapperSlider("Water Low Wavelength Adjust", iSection, 2.0f, "Wavelength Adjust");
			rScreen.WrapperSlider("Water Low Amplitude Adjust", iSection, 2.0f, "Amplitude Adjust");
			rScreen.WrapperSlider("Water Low Speed Adjust", iSection, 2.0f, "Speed Adjust");

			rScreen.WrapperSeparatorText("Camera Fade");
			rScreen.WrapperSlider("Water Low Amplitude Fade Start", iSection, 2.0f, "Low Camera Fade Start");
			rScreen.WrapperSlider("Water Low Amplitude Fade End", iSection, 2.0f, "Low Camera Fade End");

			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Medium", iSection, 2))
		{
			rScreen.RenderWaveCountRadioButtons(gWaterMediumCount);

			rScreen.WrapperSeparatorText("Wave");
			rScreen.WrapperSlider("Water Medium Wavelength", iSection, 2.0f, "Medium Wavelength");
			rScreen.WrapperSlider("Water Medium Amplitude", iSection, 2.0f, "Medium Amplitude");
			rScreen.WrapperSlider("Water Medium Speed", iSection, 2.0f, "Medium Speed");
			rScreen.WrapperSlider("Water Medium Steepness", iSection, 2.0f, "Medium Steepness");

			rScreen.WrapperSeparatorText("Adjustments");
			rScreen.WrapperSlider("Water Medium Angle Adjust", iSection, 2.0f, "Medium Angle Adjust");
			rScreen.WrapperSlider("Water Medium Wavelength Adjust", iSection, 2.0f, "Medium Wavelength Adjust");
			rScreen.WrapperSlider("Water Medium Amplitude Adjust", iSection, 2.0f, "Medium Amplitude Adjust");
			rScreen.WrapperSlider("Water Medium Speed Adjust", iSection, 2.0f, "Medium Speed Adjust");

			rScreen.WrapperSeparatorText("Camera Fade");
			rScreen.WrapperSlider("Water Medium Amplitude Fade Start", iSection, 2.0f, "Medium Camera Fade Start");
			rScreen.WrapperSlider("Water Medium Amplitude Fade End", iSection, 2.0f, "Medium Camera Fade End");

			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Depth", iSection, 3))
		{
			rScreen.WrapperSeparatorText("Terrain Fade");
			rScreen.WrapperSlider("Water Terrain Height", iSection);
			rScreen.WrapperSlider("Water Terrain Fade", iSection);

			rScreen.WrapperSeparatorText("Depth Color");
			rScreen.WrapperSlider("Water Depth Lookup Table Feather", iSection, 2.0f, "Water Depth Lut Feather");
			rScreen.WrapperSlider("Water Depth Lookup Table Sunset Fade Power", iSection, 2.0f, "Water Depth Lut Sunset Fade Power");
			rScreen.WrapperSlider("Water Depth Lookup Table Sunset Fade Intensity", iSection, 2.0f, "Water Depth Lut Sunset Fade Intensity");
			rScreen.WrapperSlider("Water Depth Color Feather", iSection);
			rScreen.WrapperSlider("Water Depth Color Floor", iSection);
			rScreen.WrapperSlider("Water Undersea Compression", iSection, 2.0f, "Undersea Compression");
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
			rScreen.WrapperSlider("Water Break Start Depth", iSection, 2.0f, "Break Start Depth");
			rScreen.WrapperSlider("Water Break End Depth", iSection, 2.0f, "Break End Depth");
			rScreen.WrapperSlider("Water Break Blend Curve", iSection, 2.0f, "Break Blend Curve");
			rScreen.WrapperSlider("Water Medium Shore Softness", iSection, 2.0f, "Medium Shore Softness");

			rScreen.WrapperSeparatorText("Skybox Lobe Reduction");
			rScreen.WrapperSlider("Lighting Water Skybox One Beach Reduction", iSection, 2.0f, "Beach Skybox 1 Reduction");
			rScreen.WrapperSlider("Lighting Water Skybox Two Beach Reduction", iSection, 2.0f, "Beach Skybox 2 Reduction");
			rScreen.WrapperSlider("Lighting Water Skybox Three Beach Reduction", iSection, 2.0f, "Beach Skybox 3 Reduction");

			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Foam", iSection, 5))
		{
			WrapperToggle("Foam Enabled", &gWaterFoamEnabled);
			rScreen.WrapperSlider("Water Foam Emission Rate", iSection, 2.0f, "Foam Emission Rate");
			rScreen.WrapperSlider("Water Foam J Start", iSection, 2.0f, "Foam J Start");
			rScreen.WrapperSlider("Water Foam J Full", iSection, 2.0f, "Foam J Full");
			rScreen.WrapperSlider("Water Foam Half Life", iSection, 2.0f, "Foam Half Life");
			rScreen.ChevronIndexSelector("Foam Texture", gWaterFoamTextureIndex, TextureManager::kpWaterFoamNames);
			rScreen.WrapperSlider("Water Foam Texture Tile Size", iSection, 2.0f, "Foam Texture Tile Size");
			rScreen.WrapperSlider("Water Foam Texture Strength", iSection, 2.0f, "Foam Texture Strength");
			rScreen.ChevronIndexSelector("Foam Breakup Texture", gWaterFoamBreakupTextureIndex, TextureManager::kpWaterFoamBreakupNames);
			rScreen.WrapperSlider("Water Foam Breakup Tile Size", iSection, 2.0f, "Foam Breakup Tile Size");
			rScreen.WrapperSlider("Water Foam Breakup Amount", iSection, 2.0f, "Foam Breakup Amount");
			rScreen.WrapperSlider("Water Foam Spawn Noise Amount", iSection, 2.0f, "Foam Spawn Noise Amount");
			rScreen.WrapperSlider("Water Foam Spawn Noise Tile Size", iSection, 2.0f, "Foam Spawn Noise Tile Size");
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

} // namespace engine

#endif // BT_CLIENT
