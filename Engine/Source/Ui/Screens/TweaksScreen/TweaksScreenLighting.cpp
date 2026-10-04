#include "TweaksScreenBase.h"

#if defined(BT_CLIENT)

#include "Ui/CurveWidget.h"
#include "Ui/LightingWrappersBase.h"
#include "TweaksSliderMap.h"

namespace engine
{

const TweaksSliderMapRegistrar gLightingRegistrar
{
	{"Lighting Blur Sigma", &gLightingBlurSigma},
	{"Lighting Blur Sample Count", &gLightingBlurSampleCount},
	{"Lighting Blur Edge Falloff", &gLightingBlurEdgeFalloff},
	{"Deposit Texture Multiplier", &gLightingDepositTextureMultiplier},
	{"Deposit Threshold", &gLightingDepositThreshold},
	{"Deposit Compress", &gLightingDepositCompress},
	{"Spread Pass Count", &gSpreadPassCount},
	{"Spread Decay", &gSpreadDecay},
	{"Spread Accumulation Decay", &gSpreadAccumulationDecay},
	{"Spread Texture Multiplier Start", &gSpreadTextureMultiplierStart},
	{"Spread Directionality", &gSpreadDirectionality},
	{"Spread Direction Count", &gSpreadDirectionCount},
	{"Spread Distance", &gSpreadDistance},
	{"Spread Ring Count", &gSpreadRingCount},
	{"Spread Jitter", &gSpreadJitter},
	{"Spread Sample Jitter Range Start", &gSpreadSampleJitterRangeStart},
	{"Spread Sample Jitter Clustering Start", &gSpreadSampleJitterClusteringStart},
	{"Spread Distance Falloff", &gSpreadDistanceFalloff},
	{"Spread Height Multiplier", &gSpreadHeightMultiplier},
	{"Spread Height End Height", &gSpreadHeightEndHeight},
	{"Spread Height Power", &gSpreadHeightPower},
	{"Spread Output Threshold", &gSpreadOutputThreshold},
	{"Spread Output Compress", &gSpreadOutputCompress},
	{"Spread Texture Multiplier End", &gSpreadTextureMultiplierEnd},
	{"Spread Directionality End", &gSpreadDirectionalityEnd},
	{"Spread Direction Count End", &gSpreadDirectionCountEnd},
	{"Spread Distance End Start Height", &gSpreadDistanceEnd.startHeight},
	{"Spread Distance End End Height", &gSpreadDistanceEnd.endHeight},
	{"Spread Distance End Low", &gSpreadDistanceEnd.low},
	{"Spread Distance End High", &gSpreadDistanceEnd.high},
	{"Spread Ring Count End", &gSpreadRingCountEnd},
	{"Spread Jitter End", &gSpreadJitterEnd},
	{"Spread Sample Jitter Range End", &gSpreadSampleJitterRangeEnd},
	{"Spread Sample Jitter Clustering End", &gSpreadSampleJitterClusteringEnd},
	{"Spread Decay End", &gSpreadDecayEnd},
	{"Spread Accumulation Decay End", &gSpreadAccumulationDecayEnd},
	{"Spread Distance Falloff End", &gSpreadDistanceFalloffEnd},
	{"Spread Output Threshold End", &gSpreadOutputThresholdEnd},
	{"Spread Output Compress End", &gSpreadOutputCompressEnd},
	{"Texel Ramp Speed", &gLightingTexelRampMetersPerSecond},
	{"Temporal Blend", &gLightingTemporalBlend},
	{"Lighting Update Cadence", &gLightingUpdateCadence},
	{"Combine Max Brightness", &gCombineMaxBrightness},
	{"Combine Contrast", &gCombineContrast},
	{"Combine Linear Start", &gCombineLinearStart},
	{"Combine Linear Length", &gCombineLinearLength},
	{"Combine Toe", &gCombineToe},
	{"Combine Black Tightness", &gCombineBlackTightness},
	{"Combine Pass Normalize", &gCombinePassNormalize},
	{"Combine Exposure Pass Scale", &gCombineExposurePassScale},
	{"Combine Hue Preserve", &gCombineHuePreserve},
	{"Directional Intensity", &gLightingDirectionalIntensity},
	{"Directional Power", &gLightingDirectionalPower},
	{"Directional Power Mode", &gLightingDirectionalPowerMode},
	{"Ambient Intensity", &gLightingAmbientIntensity},
	{"Ambient Power", &gLightingAmbientPower},
	{"Ambient Power Mode", &gLightingAmbientPowerMode},
	{"Terrain", &gLightingTerrain},
	{"Terrain Add", &gLightingAddTerrain},
	{"Terrain Below Base Multiplier", &gLightingTerrainBelowBaseMultiplier},
	{"Terrain Below Base Power", &gLightingTerrainBelowBasePower},
	{"Objects", &gLightingObjects},
	{"Objects Add", &gLightingObjectsAdd},
	{"Day Final Multiplier", &gLightingDayFinalMultiplier},
	{"Night Final Multiplier", &gLightingNightFinalMultiplier},
	{"Water EWNS Pow", &gLightingWaterEastWestNorthSouthPower},
	{"Water EWNS Pow Mode", &gLightingWaterEastWestNorthSouthPowerMode},
	{"Water Ambient Intensity", &gLightingWaterAmbientIntensity},
	{"Water Ambient Power", &gLightingWaterAmbientPower},
	{"Water Ambient Power Mode", &gLightingWaterAmbientPowerMode},
	{"Water Normal Soften", &gLightingWaterNormalSoften},
	{"Water Normal Blend Wave", &gLightingWaterNormalBlendWave},
	{"Water Intensity", &gLightingWaterIntensity},
	{"Water Add", &gLightingWaterAdd},
	{"Water One", &gLightingWaterOne},
	{"Water One Power", &gLightingWaterOnePower},
	{"Water Two", &gLightingWaterTwo},
	{"Water Two Power", &gLightingWaterTwoPower},
	{"Water Three", &gLightingWaterThree},
	{"Water Three Power", &gLightingWaterThreePower},
	{"Water Power Mode", &gLightingWaterPowerMode},
	{"Water Reflected Amount", &gLightingWaterReflectedAmount},
	{"Water Reflected Normal Blend Wave", &gLightingWaterReflectedNormalBlendWave},
	{"Water Reflected Distortion", &gLightingWaterReflectedDistortion},
	{"Water Reflected Falloff Start", &gLightingWaterReflectedFalloffStart},
	{"Water Reflected Falloff Power", &gLightingWaterReflectedFalloffPower},
	{"Water Reflected Fresnel", &gLightingWaterReflectedFresnel},
	{"Water Reflected Intensity", &gLightingWaterReflectedIntensity},
};

void RenderLightingSection(TweaksScreenBase& rScreen)
{
	int64_t iSection = giTweakSectionLighting;

	if (ImGui::BeginTabBar("LightingTabs"))
	{
		if (rScreen.BeginSubtab("Write", iSection, 0))
		{
			if (ImGui::BeginTable("LightingWriteColumns", 2))
			{
				ImGui::TableNextColumn();

				rScreen.WrapperSeparatorText("1. Pre-Blur");
				rScreen.WrapperSlider("Sigma", iSection, 1.0f, "Lighting Blur Sigma");
				rScreen.WrapperSlider("Sample Count", iSection, 1.0f, "Lighting Blur Sample Count");
				rScreen.WrapperSlider("Edge Falloff", iSection, 1.0f, "Lighting Blur Edge Falloff");

				rScreen.WrapperSeparatorText("2. Deposit");
				rScreen.WrapperSlider("Texture Multiplier", iSection, 1.0f, "Deposit Texture Multiplier");
				rScreen.WrapperSlider("Threshold", iSection, 1.0f, "Deposit Threshold");
				rScreen.WrapperSlider("Compress", iSection, 1.0f, "Deposit Compress");

				rScreen.WrapperSeparatorText("3a. Spread");
				rScreen.WrapperSlider("Pass Count", iSection, 1.0f, "Spread Pass Count");
				rScreen.WrapperSlider("Decay", iSection, 1.0f, "Spread Decay");
				rScreen.WrapperSlider("Accumulation Decay", iSection, 1.0f, "Spread Accumulation Decay");

				float fSpreadStartY = ImGui::GetCursorPosY();

				rScreen.WrapperSeparatorText("3b. Spread Start");
				rScreen.WrapperSlider("Texture Multiplier Start", iSection, 1.0f, "Spread Texture Multiplier Start");
				rScreen.WrapperSlider("Directionality", iSection, 1.0f, "Spread Directionality");
				rScreen.WrapperSlider("Direction Count", iSection, 1.0f, "Spread Direction Count");
				rScreen.WrapperSlider("Distance", iSection, 1.0f, "Spread Distance");
				rScreen.WrapperSlider("Ring Count", iSection, 1.0f, "Spread Ring Count");
				rScreen.WrapperSlider("Jitter", iSection, 1.0f, "Spread Jitter");
				rScreen.WrapperSlider("Sample Jitter Range", iSection, 1.0f, "Spread Sample Jitter Range Start");
				rScreen.WrapperSlider("Sample Jitter Clustering", iSection, 1.0f, "Spread Sample Jitter Clustering Start");
				rScreen.WrapperSlider("Distance Falloff", iSection, 1.0f, "Spread Distance Falloff");
				rScreen.WrapperSlider("Height Multiplier", iSection, 1.0f, "Spread Height Multiplier");
				rScreen.WrapperSlider("Height End Height", iSection, 1.0f, "Spread Height End Height");
				rScreen.WrapperSlider("Height Power", iSection, 1.0f, "Spread Height Power");
				rScreen.WrapperSlider("Output Threshold", iSection, 1.0f, "Spread Output Threshold");
				rScreen.WrapperSlider("Output Compress", iSection, 1.0f, "Spread Output Compress");

				ImGui::TableNextColumn();

				ImGui::SetCursorPosY(fSpreadStartY);

				rScreen.WrapperSeparatorText("3c. Spread End");
				rScreen.WrapperSlider("Texture Multiplier End", iSection, 1.0f, "Spread Texture Multiplier End");
				rScreen.WrapperSlider("Directionality", iSection, 1.0f, "Spread Directionality End");
				rScreen.WrapperSlider("Direction Count", iSection, 1.0f, "Spread Direction Count End");
				rScreen.WrapperSlider("Distance Start Height", iSection, 1.0f, "Spread Distance End Start Height");
				rScreen.WrapperSlider("Distance End Height", iSection, 1.0f, "Spread Distance End End Height");
				rScreen.WrapperSlider("Distance Low", iSection, 1.0f, "Spread Distance End Low");
				rScreen.WrapperSlider("Distance High", iSection, 1.0f, "Spread Distance End High");
				rScreen.WrapperSlider("Ring Count", iSection, 1.0f, "Spread Ring Count End");
				rScreen.WrapperSlider("Jitter", iSection, 1.0f, "Spread Jitter End");
				rScreen.WrapperSlider("Sample Jitter Range", iSection, 1.0f, "Spread Sample Jitter Range End");
				rScreen.WrapperSlider("Sample Jitter Clustering", iSection, 1.0f, "Spread Sample Jitter Clustering End");
				rScreen.WrapperSlider("Decay", iSection, 1.0f, "Spread Decay End");
				rScreen.WrapperSlider("Accumulation Decay", iSection, 1.0f, "Spread Accumulation Decay End");
				rScreen.WrapperSlider("Distance Falloff", iSection, 1.0f, "Spread Distance Falloff End");
				rScreen.WrapperSlider("Output Threshold", iSection, 1.0f, "Spread Output Threshold End");
				rScreen.WrapperSlider("Output Compress", iSection, 1.0f, "Spread Output Compress End");

				rScreen.WrapperSeparatorText("4. Temporal");
				rScreen.WrapperSlider("Texel Contraction Speed", iSection, 1.0f, "Texel Ramp Speed");
				rScreen.WrapperSlider("Temporal Blend", iSection, 1.0f, "Temporal Blend");
				rScreen.WrapperSlider("Update Cadence", iSection, 1.0f, "Lighting Update Cadence");

				ImGui::EndTable();
			}
			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Combine", iSection, 1))
		{
			if (ImGui::BeginTable("LightingCombineColumns", 2))
			{
				ImGui::TableNextColumn();

				rScreen.WrapperSeparatorText("Tone Curve");
				rScreen.WrapperSlider("Max Brightness", iSection, 1.0f, "Combine Max Brightness");
				rScreen.WrapperSlider("Contrast", iSection, 1.0f, "Combine Contrast");
				rScreen.WrapperSlider("Linear Start", iSection, 1.0f, "Combine Linear Start");
				rScreen.WrapperSlider("Linear Length", iSection, 1.0f, "Combine Linear Length");
				rScreen.WrapperSlider("Toe", iSection, 1.0f, "Combine Toe");
				rScreen.WrapperSlider("Black Tightness", iSection, 1.0f, "Combine Black Tightness");
				rScreen.WrapperSlider("Pass Normalize", iSection, 1.0f, "Combine Pass Normalize");
				rScreen.WrapperSlider("Exposure Pass Scale", iSection, 1.0f, "Combine Exposure Pass Scale");
				rScreen.WrapperSlider("Hue Preserve", iSection, 1.0f, "Combine Hue Preserve");

				ImGui::TableNextColumn();

				if (ImGui::Button(gbUseCombineCurveNew ? "Using: New Curve" : "Using: Old Curve"))
				{
					gbUseCombineCurveNew = !gbUseCombineCurveNew;
				}

				CurveData& rActiveCurve = gbUseCombineCurveNew ? gCombineCurveNew : gCombineCurveOld;
				if (CurveWidget("Pass Contribution Curve", rActiveCurve))
				{
					rScreen.mActiveSlider = "Combine Curve";
					rScreen.miActiveSliderSection = iSection;
				}

				if (ImGui::Button("Copy Curve To Clipboard"))
				{
					char pcBuffer[2'048] {};
					const char* pcName = gbUseCombineCurveNew ? "gCombineCurveNew" : "gCombineCurveOld";
					int iOffset = std::snprintf(pcBuffer, sizeof(pcBuffer), "CurveData %s({", pcName);
					for (int64_t i = 0; i < std::ssize(rActiveCurve.mPoints); ++i)
					{
						const ImVec2& rPoint = rActiveCurve.mPoints.at(i);
						iOffset += std::snprintf(pcBuffer + iOffset, sizeof(pcBuffer) - iOffset, "%sImVec2(%.4ff, %.4ff)", i == 0 ? "" : ", ", rPoint.x, rPoint.y);
					}
					std::snprintf(pcBuffer + iOffset, sizeof(pcBuffer) - iOffset, "}, %.4ff, %.4ff);", rActiveCurve.mfYMinimum, rActiveCurve.mfYMaximum);
					ImGui::SetClipboardText(pcBuffer);
					LOG(kGraphics, kInfo, "{}", pcBuffer);
				}

				ImGui::EndTable();
			}

			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Read", iSection, 2))
		{
			if (ImGui::BeginTable("LightingReadColumns", 2))
			{
				ImGui::TableNextColumn();

				rScreen.WrapperSeparatorText("Terrain Lighting");
				rScreen.WrapperSlider("Directional Intensity", iSection, 1.0f);
				rScreen.WrapperSlider("Directional Power", iSection, 1.0f);
				rScreen.WrapperSlider("Directional Power Mode", iSection, 1.0f);
				rScreen.WrapperSlider("Ambient Intensity", iSection, 1.0f);
				rScreen.WrapperSlider("Ambient Power", iSection, 1.0f);
				rScreen.WrapperSlider("Ambient Power Mode", iSection, 1.0f);
				rScreen.WrapperSlider("Terrain", iSection, 1.0f);
				rScreen.WrapperSlider("Terrain Add", iSection, 1.0f);
				rScreen.WrapperSlider("Below Base Multiplier", iSection, 1.0f, "Terrain Below Base Multiplier");
				rScreen.WrapperSlider("Below Base Power", iSection, 1.0f, "Terrain Below Base Power");
				rScreen.WrapperSlider("Objects", iSection, 1.0f);
				rScreen.WrapperSlider("Objects Add", iSection, 1.0f);
				rScreen.WrapperSlider("Day Final Multiplier", iSection, 1.0f);
				rScreen.WrapperSlider("Night Final Multiplier", iSection, 1.0f);

				ImGui::TableNextColumn();

				rScreen.WrapperSeparatorText("Water Lighting");
				rScreen.WrapperSlider("Water EWNS Pow", iSection, 1.0f);
				rScreen.WrapperSlider("Water EWNS Pow Mode", iSection, 1.0f);
				rScreen.WrapperSlider("Water Ambient Intensity", iSection, 1.0f);
				rScreen.WrapperSlider("Water Ambient Power", iSection, 1.0f);
				rScreen.WrapperSlider("Water Ambient Power Mode", iSection, 1.0f);
				rScreen.WrapperSlider("Normal Soften", iSection, 1.0f, "Water Normal Soften");
				rScreen.WrapperSlider("Normal Blend Wave", iSection, 1.0f, "Water Normal Blend Wave");
				rScreen.WrapperSlider("Intensity", iSection, 1.0f, "Water Intensity");
				rScreen.WrapperSlider("Add", iSection, 1.0f, "Water Add");
				rScreen.WrapperSlider("One", iSection, 1.0f, "Water One");
				rScreen.WrapperSlider("One Power", iSection, 1.0f, "Water One Power");
				rScreen.WrapperSlider("Two", iSection, 1.0f, "Water Two");
				rScreen.WrapperSlider("Two Power", iSection, 1.0f, "Water Two Power");
				rScreen.WrapperSlider("Three", iSection, 1.0f, "Water Three");
				rScreen.WrapperSlider("Three Power", iSection, 1.0f, "Water Three Power");
				rScreen.WrapperSlider("Power Mode", iSection, 1.0f, "Water Power Mode");

				rScreen.WrapperSeparatorText("Water Reflected");
				rScreen.WrapperSlider("Reflected Amount", iSection, 1.0f, "Water Reflected Amount");
				rScreen.WrapperSlider("Reflected Normal Blend Wave", iSection, 1.0f, "Water Reflected Normal Blend Wave");
				rScreen.WrapperSlider("Reflected Distortion", iSection, 1.0f, "Water Reflected Distortion");
				rScreen.WrapperSlider("Reflected Falloff Start", iSection, 1.0f, "Water Reflected Falloff Start");
				rScreen.WrapperSlider("Reflected Falloff Power", iSection, 1.0f, "Water Reflected Falloff Power");
				rScreen.WrapperSlider("Reflected Fresnel", iSection, 1.0f, "Water Reflected Fresnel");
				rScreen.WrapperSlider("Reflected Intensity", iSection, 1.0f, "Water Reflected Intensity");

				ImGui::EndTable();
			}

			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Visible", iSection, 3))
		{
			rScreen.RenderLightingEffectsVisibleTab();
			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Lighting", iSection, 4))
		{
			rScreen.RenderLightingEffectsLightingTab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

} // namespace engine

#endif // defined(BT_CLIENT)
