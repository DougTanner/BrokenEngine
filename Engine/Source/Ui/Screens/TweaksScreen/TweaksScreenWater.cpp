#include "TweaksScreen.h"

#if defined(BT_CLIENT)

#include "Graphics/Managers/TextureManager.h"
#include "Ui/MenuUtils.h"
#include "Ui/WaterWrappersBase.h"

namespace engine
{

void RenderWaterSection(TweaksScreen& rScreen)
{
	int64_t iSection = giTweakSectionWater;

	rScreen.WrapperSlider("Water Scale", gWaterScale, iSection);

	if (ImGui::BeginTabBar("WaterTabs"))
	{
		if (rScreen.BeginSubtab("Specular", iSection, 0))
		{
			if (ImGui::BeginTable("WaterSpecularColumns", 2))
			{
				ImGui::TableNextColumn();

				rScreen.WrapperSeparatorText("Normals");
				rScreen.ChevronIndexSelector("Sample 1", gWaterNormalIndexOne, TextureManager::kpWaterNormalNames);
				rScreen.WrapperSlider("Lighting Sampled Normals One Size", gLightingSampledNormalsOneSize, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Sampled Normals Weight One Minimum", gLightingSampledNormalsWeightOneMinimum, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Sampled Normals Weight One Maximum", gLightingSampledNormalsWeightOneMaximum, iSection, 1.0f);
				rScreen.WrapperSlider("Water Normal Rotation One", gWaterNormalRotationOne, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Sampled Normals Speed One Minimum", gLightingSampledNormalsSpeedOneMinimum, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Sampled Normals Speed One Maximum", gLightingSampledNormalsSpeedOneMaximum, iSection, 1.0f);
				rScreen.WrapperSlider("Water Normal Speed Direction One", gWaterNormalSpeedDirectionOne, iSection, 1.0f);
				rScreen.WrapperSlider("Water Normal Patch Blend One", gWaterNormalPatchBlendOne, iSection, 1.0f);

				rScreen.ChevronIndexSelector("Sample 2", gWaterNormalIndexTwo, TextureManager::kpWaterNormalNames);
				rScreen.WrapperSlider("Lighting Sampled Normals Two Size", gLightingSampledNormalsTwoSize, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Sampled Normals Weight Two Minimum", gLightingSampledNormalsWeightTwoMinimum, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Sampled Normals Weight Two Maximum", gLightingSampledNormalsWeightTwoMaximum, iSection, 1.0f);
				rScreen.WrapperSlider("Water Normal Rotation Two", gWaterNormalRotationTwo, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Sampled Normals Speed Two Minimum", gLightingSampledNormalsSpeedTwoMinimum, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Sampled Normals Speed Two Maximum", gLightingSampledNormalsSpeedTwoMaximum, iSection, 1.0f);
				rScreen.WrapperSlider("Water Normal Speed Direction Two", gWaterNormalSpeedDirectionTwo, iSection, 1.0f);
				rScreen.WrapperSlider("Water Normal Patch Blend Two", gWaterNormalPatchBlendTwo, iSection, 1.0f);

				rScreen.ChevronIndexSelector("Sample 3", gWaterNormalIndexThree, TextureManager::kpWaterNormalNames);
				rScreen.WrapperSlider("Lighting Sampled Normals Three Size", gLightingSampledNormalsThreeSize, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Sampled Normals Weight Three Minimum", gLightingSampledNormalsWeightThreeMinimum, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Sampled Normals Weight Three Maximum", gLightingSampledNormalsWeightThreeMaximum, iSection, 1.0f);
				rScreen.WrapperSlider("Water Normal Rotation Three", gWaterNormalRotationThree, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Sampled Normals Speed Three Minimum", gLightingSampledNormalsSpeedThreeMinimum, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Sampled Normals Speed Three Maximum", gLightingSampledNormalsSpeedThreeMaximum, iSection, 1.0f);
				rScreen.WrapperSlider("Water Normal Speed Direction Three", gWaterNormalSpeedDirectionThree, iSection, 1.0f);
				rScreen.WrapperSlider("Water Normal Patch Blend Three", gWaterNormalPatchBlendThree, iSection, 1.0f);

				rScreen.WrapperSlider("Water Depth Reflection Feather", gWaterDepthReflectionFeather, iSection, 1.0f);
				rScreen.WrapperSlider("Water Wave Normal Blend", gWaterWaveNormalBlend, iSection, 1.0f);

				ImGui::TableNextColumn();

				rScreen.WrapperSeparatorText("Skybox");
				rScreen.WrapperSlider("Lighting Water Skybox Sun Bias", gLightingWaterSkyboxSunBias, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Skybox Normal Soften Sunrise", gLightingWaterSkyboxNormalSoftenSunrise, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Skybox Normal Soften Noon", gLightingWaterSkyboxNormalSoftenNoon, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Skybox Normal Blend Wave", gLightingWaterSkyboxNormalBlendWave, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Skybox Intensity", gLightingWaterSkyboxIntensity, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Skybox Add", gLightingWaterSkyboxAdd, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Skybox One", gLightingWaterSkyboxOne, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Skybox One Night", gLightingWaterSkyboxOneNight, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Skybox One Power", gLightingWaterSkyboxOnePower, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Skybox Two", gLightingWaterSkyboxTwo, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Skybox Two Night", gLightingWaterSkyboxTwoNight, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Skybox Two Power", gLightingWaterSkyboxTwoPower, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Skybox Three", gLightingWaterSkyboxThree, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Skybox Three Night", gLightingWaterSkyboxThreeNight, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Skybox Three Power", gLightingWaterSkyboxThreePower, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Skybox Level Of Detail", gLightingWaterSkyboxLevelOfDetail, iSection, 1.0f);

				rScreen.WrapperSeparatorText("Skybox One Sampling");
				rScreen.WrapperSlider("Water Skybox One Sample Count", gWaterSkyboxOneSampleCount, iSection, 1.0f);
				rScreen.WrapperSlider("Water Skybox One Sample Radius", gWaterSkyboxOneSampleRadius, iSection, 1.0f);
				rScreen.WrapperSlider("Water Skybox One Sample Blend", gWaterSkyboxOneSampleBlend, iSection, 1.0f);

				rScreen.WrapperSeparatorText("Height Darken");
				rScreen.WrapperSlider("Water Height Darken Top", gWaterHeightDarkenTop, iSection, 1.0f);
				rScreen.WrapperSlider("Water Height Darken Bottom", gWaterHeightDarkenBottom, iSection, 1.0f);
				rScreen.WrapperSlider("Water Height Darken Target", gWaterHeightDarkenTarget, iSection, 1.0f);
				rScreen.WrapperSlider("Water Height Darken Lighting", gWaterHeightDarkenLighting, iSection, 1.0f);

				ImGui::EndTable();
			}
			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Low", iSection, 1))
		{
			rScreen.RenderWaveCountRadioButtons(gWaterLowCount);

			rScreen.WrapperSeparatorText("Wave");
			rScreen.WrapperSlider("Water Low Angle", gWaterLowAngle, iSection);
			rScreen.WrapperSlider("Water Low Wavelength", gWaterLowWavelength, iSection);
			rScreen.WrapperSlider("Water Low Amplitude", gWaterLowAmplitude, iSection);
			rScreen.WrapperSlider("Water Low Speed", gWaterLowSpeed, iSection);
			rScreen.WrapperSlider("Water Low Steepness", gWaterLowSteepness, iSection);

			rScreen.WrapperSeparatorText("Adjustments");
			rScreen.WrapperSlider("Water Low Angle Adjust", gWaterLowAngleAdjust, iSection);
			rScreen.WrapperSlider("Water Low Wavelength Adjust", gWaterLowWavelengthAdjust, iSection);
			rScreen.WrapperSlider("Water Low Amplitude Adjust", gWaterLowAmplitudeAdjust, iSection);
			rScreen.WrapperSlider("Water Low Speed Adjust", gWaterLowSpeedAdjust, iSection);

			rScreen.WrapperSeparatorText("Camera Fade");
			rScreen.WrapperSlider("Water Low Amplitude Fade Start", gWaterLowAmplitudeFadeStart, iSection);
			rScreen.WrapperSlider("Water Low Amplitude Fade End", gWaterLowAmplitudeFadeEnd, iSection);

			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Medium", iSection, 2))
		{
			rScreen.RenderWaveCountRadioButtons(gWaterMediumCount);

			rScreen.WrapperSeparatorText("Wave");
			rScreen.WrapperSlider("Water Medium Wavelength", gWaterMediumWavelength, iSection);
			rScreen.WrapperSlider("Water Medium Amplitude", gWaterMediumAmplitude, iSection);
			rScreen.WrapperSlider("Water Medium Speed", gWaterMediumSpeed, iSection);
			rScreen.WrapperSlider("Water Medium Steepness", gWaterMediumSteepness, iSection);

			rScreen.WrapperSeparatorText("Adjustments");
			rScreen.WrapperSlider("Water Medium Angle Adjust", gWaterMediumAngleAdjust, iSection);
			rScreen.WrapperSlider("Water Medium Wavelength Adjust", gWaterMediumWavelengthAdjust, iSection);
			rScreen.WrapperSlider("Water Medium Amplitude Adjust", gWaterMediumAmplitudeAdjust, iSection);
			rScreen.WrapperSlider("Water Medium Speed Adjust", gWaterMediumSpeedAdjust, iSection);

			rScreen.WrapperSeparatorText("Camera Fade");
			rScreen.WrapperSlider("Water Medium Amplitude Fade Start", gWaterMediumAmplitudeFadeStart, iSection);
			rScreen.WrapperSlider("Water Medium Amplitude Fade End", gWaterMediumAmplitudeFadeEnd, iSection);

			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Depth", iSection, 3))
		{
			rScreen.WrapperSeparatorText("Terrain Fade");
			rScreen.WrapperSlider("Water Terrain Height", gWaterTerrainHeight, iSection);
			rScreen.WrapperSlider("Water Terrain Fade", gWaterTerrainFade, iSection);

			rScreen.WrapperSeparatorText("Depth Color");
			rScreen.WrapperSlider("Water Depth Lookup Table Feather", gWaterDepthLookupTableFeather, iSection);
			rScreen.WrapperSlider("Water Depth Lookup Table Sunset Fade Power", gWaterDepthLookupTableSunsetFadePower, iSection);
			rScreen.WrapperSlider("Water Depth Lookup Table Sunset Fade Intensity", gWaterDepthLookupTableSunsetFadeIntensity, iSection);
			rScreen.WrapperSlider("Water Depth Color Feather", gWaterDepthColorFeather, iSection);
			rScreen.WrapperSlider("Water Depth Color Floor", gWaterDepthColorFloor, iSection);
			rScreen.WrapperSlider("Water Undersea Compression", gWaterUnderseaCompression, iSection);
			rScreen.WrapperSlider("Water Color Bottom", gWaterColorBottom, iSection);
			rScreen.WrapperSlider("Water Color Height", gWaterColorHeight, iSection);

			rScreen.WrapperSeparatorText("Fresnel");
			rScreen.WrapperSlider("Water Fresnel", gWaterFresnel, iSection);

			rScreen.WrapperSeparatorText("Color Noise");
			rScreen.WrapperSlider("Water Color Noise Frequency", gWaterColorNoiseFrequency, iSection);
			rScreen.WrapperSlider("Water Color Noise Amount", gWaterColorNoiseAmount, iSection);
			rScreen.WrapperSlider("Water Color Noise Weight One", gWaterColorNoiseWeightOne, iSection);
			rScreen.WrapperSlider("Water Color Noise Multiplier One", gWaterColorNoiseMultiplierOne, iSection);
			rScreen.WrapperSlider("Water Color Noise Weight Two", gWaterColorNoiseWeightTwo, iSection);
			rScreen.WrapperSlider("Water Color Noise Multiplier Two", gWaterColorNoiseMultiplierTwo, iSection);
			rScreen.WrapperSlider("Water Color Noise Patch Blend", gWaterColorNoisePatchBlend, iSection);

			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Beach", iSection, 4))
		{
			rScreen.WrapperSeparatorText("Breaking");
			rScreen.WrapperSlider("Water Break Start Depth", gWaterBreakStartDepth, iSection);
			rScreen.WrapperSlider("Water Break End Depth", gWaterBreakEndDepth, iSection);
			rScreen.WrapperSlider("Water Break Blend Curve", gWaterBreakBlendCurve, iSection);
			rScreen.WrapperSlider("Water Medium Shore Softness", gWaterMediumShoreSoftness, iSection);

			rScreen.WrapperSeparatorText("Skybox Lobe Reduction");
			rScreen.WrapperSlider("Lighting Water Skybox One Beach Reduction", gLightingWaterSkyboxOneBeachReduction, iSection);
			rScreen.WrapperSlider("Lighting Water Skybox Two Beach Reduction", gLightingWaterSkyboxTwoBeachReduction, iSection);
			rScreen.WrapperSlider("Lighting Water Skybox Three Beach Reduction", gLightingWaterSkyboxThreeBeachReduction, iSection);

			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Foam", iSection, 5))
		{
			WrapperToggle("Foam Enabled", &gWaterFoamEnabled);
			rScreen.WrapperSlider("Water Foam Emission Rate", gWaterFoamEmissionRate, iSection);
			rScreen.WrapperSlider("Water Foam J Start", gWaterFoamJStart, iSection);
			rScreen.WrapperSlider("Water Foam J Full", gWaterFoamJFull, iSection);
			rScreen.WrapperSlider("Water Foam Half Life", gWaterFoamHalfLife, iSection);
			rScreen.ChevronIndexSelector("Foam Texture", gWaterFoamTextureIndex, TextureManager::kpWaterFoamNames);
			rScreen.WrapperSlider("Water Foam Texture Tile Size", gWaterFoamTextureTileSize, iSection);
			rScreen.WrapperSlider("Water Foam Texture Strength", gWaterFoamTextureStrength, iSection);
			rScreen.WrapperSlider("Water Foam Texture Patch Blend", gWaterFoamTexturePatchBlend, iSection);
			rScreen.ChevronIndexSelector("Foam Breakup Texture", gWaterFoamBreakupTextureIndex, TextureManager::kpWaterFoamBreakupNames);
			rScreen.WrapperSlider("Water Foam Breakup Tile Size", gWaterFoamBreakupTileSize, iSection);
			rScreen.WrapperSlider("Water Foam Breakup Amount", gWaterFoamBreakupAmount, iSection);
			rScreen.WrapperSlider("Water Foam Breakup Patch Blend", gWaterFoamBreakupPatchBlend, iSection);
			rScreen.WrapperSlider("Water Foam Spawn Noise Amount", gWaterFoamSpawnNoiseAmount, iSection);
			rScreen.WrapperSlider("Water Foam Spawn Noise Tile Size", gWaterFoamSpawnNoiseTileSize, iSection);
			rScreen.WrapperSlider("Water Foam Spawn Noise Patch Blend", gWaterFoamSpawnNoisePatchBlend, iSection);
			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Break Repetition", iSection, 6))
		{
			rScreen.WrapperSlider("Patch Weight Threshold", gPatchWeightThreshold, iSection);
			WrapperToggle("Water Normal Patch Blend One Enabled", &gWaterNormalPatchBlendOneEnabled);
			WrapperToggle("Water Normal Patch Blend Two Enabled", &gWaterNormalPatchBlendTwoEnabled);
			WrapperToggle("Water Normal Patch Blend Three Enabled", &gWaterNormalPatchBlendThreeEnabled);
			WrapperToggle("Water Color Noise Patch Blend Enabled", &gWaterColorNoisePatchBlendEnabled);
			WrapperToggle("Water Foam Texture Patch Blend Enabled", &gWaterFoamTexturePatchBlendEnabled);
			WrapperToggle("Water Foam Breakup Patch Blend Enabled", &gWaterFoamBreakupPatchBlendEnabled);
			WrapperToggle("Water Foam Spawn Noise Patch Blend Enabled", &gWaterFoamSpawnNoisePatchBlendEnabled);
			WrapperToggle("Water Normal Extra Sample Patch Blend Enabled", &gWaterNormalExtraSamplePatchBlendEnabled);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

} // namespace engine

#endif // BT_CLIENT
