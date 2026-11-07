#include "TweaksScreen.h"

#if defined(BT_CLIENT)

#include "Ui/CurveWidget.h"
#include "Ui/LightingWrappersBase.h"

#include "Ui/Screens/TweaksScreen/Tweaks.h"

namespace engine
{

void RenderLightingSection(TweaksScreen& rScreen)
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
				rScreen.WrapperSlider("Lighting Blur Sigma", gLightingBlurSigma, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Blur Sample Count", gLightingBlurSampleCount, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Blur Edge Falloff", gLightingBlurEdgeFalloff, iSection, 1.0f);

				rScreen.WrapperSeparatorText("2. Deposit");
				rScreen.WrapperSlider("Lighting Deposit Texture Multiplier", gLightingDepositTextureMultiplier, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Deposit Threshold", gLightingDepositThreshold, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Deposit Compress", gLightingDepositCompress, iSection, 1.0f);

				rScreen.WrapperSeparatorText("3a. Spread");
				rScreen.WrapperSlider("Spread Pass Count", gSpreadPassCount, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Decay", gSpreadDecay, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Accumulation Decay", gSpreadAccumulationDecay, iSection, 1.0f);

				float fSpreadStartY = ImGui::GetCursorPosY();

				rScreen.WrapperSeparatorText("3b. Spread Start");
				rScreen.WrapperSlider("Spread Texture Multiplier Start", gSpreadTextureMultiplierStart, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Directionality", gSpreadDirectionality, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Direction Count", gSpreadDirectionCount, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Distance", gSpreadDistance, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Ring Count", gSpreadRingCount, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Jitter", gSpreadJitter, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Sample Jitter Range Start", gSpreadSampleJitterRangeStart, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Sample Jitter Clustering Start", gSpreadSampleJitterClusteringStart, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Distance Falloff", gSpreadDistanceFalloff, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Height Multiplier", gSpreadHeightMultiplier, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Height End Height", gSpreadHeightEndHeight, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Height Power", gSpreadHeightPower, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Output Threshold", gSpreadOutputThreshold, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Output Compress", gSpreadOutputCompress, iSection, 1.0f);

				ImGui::TableNextColumn();

				ImGui::SetCursorPosY(fSpreadStartY);

				rScreen.WrapperSeparatorText("3c. Spread End");
				rScreen.WrapperSlider("Spread Texture Multiplier End", gSpreadTextureMultiplierEnd, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Directionality End", gSpreadDirectionalityEnd, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Direction Count End", gSpreadDirectionCountEnd, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Distance End Start Height", gSpreadDistanceEnd.startHeight, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Distance End End Height", gSpreadDistanceEnd.endHeight, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Distance End Low", gSpreadDistanceEnd.low, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Distance End High", gSpreadDistanceEnd.high, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Ring Count End", gSpreadRingCountEnd, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Jitter End", gSpreadJitterEnd, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Sample Jitter Range End", gSpreadSampleJitterRangeEnd, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Sample Jitter Clustering End", gSpreadSampleJitterClusteringEnd, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Decay End", gSpreadDecayEnd, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Accumulation Decay End", gSpreadAccumulationDecayEnd, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Distance Falloff End", gSpreadDistanceFalloffEnd, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Output Threshold End", gSpreadOutputThresholdEnd, iSection, 1.0f);
				rScreen.WrapperSlider("Spread Output Compress End", gSpreadOutputCompressEnd, iSection, 1.0f);

				rScreen.WrapperSeparatorText("4. Temporal");
				rScreen.WrapperSlider("Lighting Texel Ramp Meters Per Second", gLightingTexelRampMetersPerSecond, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Temporal Blend", gLightingTemporalBlend, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Update Cadence", gLightingUpdateCadence, iSection, 1.0f);

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
				rScreen.WrapperSlider("Combine Max Brightness", gCombineMaxBrightness, iSection, 1.0f);
				rScreen.WrapperSlider("Combine Contrast", gCombineContrast, iSection, 1.0f);
				rScreen.WrapperSlider("Combine Linear Start", gCombineLinearStart, iSection, 1.0f);
				rScreen.WrapperSlider("Combine Linear Length", gCombineLinearLength, iSection, 1.0f);
				rScreen.WrapperSlider("Combine Toe", gCombineToe, iSection, 1.0f);
				rScreen.WrapperSlider("Combine Black Tightness", gCombineBlackTightness, iSection, 1.0f);
				rScreen.WrapperSlider("Combine Pass Normalize", gCombinePassNormalize, iSection, 1.0f);
				rScreen.WrapperSlider("Combine Exposure Pass Scale", gCombineExposurePassScale, iSection, 1.0f);
				rScreen.WrapperSlider("Combine Hue Preserve", gCombineHuePreserve, iSection, 1.0f);

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
					// Heap: the clipboard text is a std::string built on one button click.
					ScopedSuppressAllocationTracking suppress;
					const char* pcName = gbUseCombineCurveNew ? "gCombineCurveNew" : "gCombineCurveOld";
					std::string curveText = std::format("CurveData {}({{", pcName);
					for (int64_t i = 0; i < std::ssize(rActiveCurve.mPoints); ++i)
					{
						const ImVec2& rPoint = rActiveCurve.mPoints.at(i);
						curveText += std::format("{}ImVec2({:.4f}f, {:.4f}f)", i == 0 ? "" : ", ", rPoint.x, rPoint.y);
					}
					curveText += std::format("}}, {:.4f}f, {:.4f}f);", rActiveCurve.mfYMinimum, rActiveCurve.mfYMaximum);
					ImGui::SetClipboardText(curveText.c_str());
					LOG(kGraphics, kInfo, "{}", curveText.c_str());
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
				rScreen.WrapperSlider("Lighting Directional Intensity", gLightingDirectionalIntensity, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Directional Power", gLightingDirectionalPower, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Directional Power Mode", gLightingDirectionalPowerMode, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Ambient Intensity", gLightingAmbientIntensity, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Ambient Power", gLightingAmbientPower, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Ambient Power Mode", gLightingAmbientPowerMode, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Terrain", gLightingTerrain, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Add Terrain", gLightingAddTerrain, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Terrain Below Base Multiplier", gLightingTerrainBelowBaseMultiplier, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Terrain Below Base Power", gLightingTerrainBelowBasePower, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Objects", gLightingObjects, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Objects Add", gLightingObjectsAdd, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Day Final Multiplier", gLightingDayFinalMultiplier, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Night Final Multiplier", gLightingNightFinalMultiplier, iSection, 1.0f);

				ImGui::TableNextColumn();

				rScreen.WrapperSeparatorText("Water Lighting");
				rScreen.WrapperSlider("Lighting Water East West North South Power", gLightingWaterEastWestNorthSouthPower, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water East West North South Power Mode", gLightingWaterEastWestNorthSouthPowerMode, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Ambient Intensity", gLightingWaterAmbientIntensity, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Ambient Power", gLightingWaterAmbientPower, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Ambient Power Mode", gLightingWaterAmbientPowerMode, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Normal Soften", gLightingWaterNormalSoften, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Normal Blend Wave", gLightingWaterNormalBlendWave, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Intensity", gLightingWaterIntensity, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Add", gLightingWaterAdd, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water One", gLightingWaterOne, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water One Power", gLightingWaterOnePower, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Two", gLightingWaterTwo, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Two Power", gLightingWaterTwoPower, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Three", gLightingWaterThree, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Three Power", gLightingWaterThreePower, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Power Mode", gLightingWaterPowerMode, iSection, 1.0f);

				rScreen.WrapperSeparatorText("Water Reflected");
				rScreen.WrapperSlider("Lighting Water Reflected Amount", gLightingWaterReflectedAmount, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Reflected Normal Blend Wave", gLightingWaterReflectedNormalBlendWave, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Reflected Distortion", gLightingWaterReflectedDistortion, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Reflected Falloff Start", gLightingWaterReflectedFalloffStart, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Reflected Falloff Power", gLightingWaterReflectedFalloffPower, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Reflected Fresnel", gLightingWaterReflectedFresnel, iSection, 1.0f);
				rScreen.WrapperSlider("Lighting Water Reflected Intensity", gLightingWaterReflectedIntensity, iSection, 1.0f);

				ImGui::EndTable();
			}

			ImGui::EndTabItem();
		}
		game::RenderLightingTweakExtensions(rScreen, iSection, 3);
		ImGui::EndTabBar();
	}
}

} // namespace engine

#endif // defined(BT_CLIENT)
