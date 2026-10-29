#include "TweaksScreen.h"

#if defined(BT_CLIENT)

#include "Ui/Screens/TweaksScreen/Tweaks.h"

namespace engine
{

void RenderLightingSection(TweaksScreen& rScreen)
{
	int64_t iSection = giTweakSectionLighting;

	if (ImGui::BeginTabBar("LightingTabs"))
	{
		if (rScreen.BeginSubtab("Comparison", iSection, 0))
		{
			LightingComparisonSettings settings = gpGraphics->mpLighting->requestedComparisonSettings;
			bool bChanged = false;
			int iMode = static_cast<int>(settings.eMode);
			bChanged |= ImGui::RadioButton("Disabled##LightingMode", &iMode, 0);
			ImGui::SameLine();
			bChanged |= ImGui::RadioButton("Fixture zero##LightingMode", &iMode, 1);
			ImGui::SameLine();
			bChanged |= ImGui::RadioButton("Fixture reference##LightingMode", &iMode, 2);
			settings.eMode = static_cast<LightingComparisonMode>(iMode);
			int iCamera = static_cast<int>(settings.eCamera);
			bChanged |= ImGui::RadioButton("Lowest zoom##LightingCamera", &iCamera, 0);
			ImGui::SameLine();
			bChanged |= ImGui::RadioButton("Default zoom##LightingCamera", &iCamera, 1);
			ImGui::SameLine();
			bChanged |= ImGui::RadioButton("Event##LightingCamera", &iCamera, 2);
			settings.eCamera = static_cast<LightingComparisonCamera>(iCamera);
			int iScenario = static_cast<int>(settings.eScenario);
			bChanged |= ImGui::RadioButton("Density##LightingScenario", &iScenario, 0);
			ImGui::SameLine();
			bChanged |= ImGui::RadioButton("Point##LightingScenario", &iScenario, 1);
			ImGui::SameLine();
			bChanged |= ImGui::RadioButton("Rectangle##LightingScenario", &iScenario, 2);
			settings.eScenario = static_cast<LightingFixtureScenario>(iScenario);
			int iCount = static_cast<int>(settings.iSourceCount);
			for (int64_t iValue : {0, 100, 300, 1'000})
			{
				char pcLabel[32] {};
				std::format_to_n(pcLabel, std::size(pcLabel) - 1, "{}##LightingCount", iValue);
				bChanged |= ImGui::RadioButton(pcLabel, &iCount, static_cast<int>(iValue));
				ImGui::SameLine();
			}
			ImGui::NewLine();
			settings.iSourceCount = iCount;
			int iSamples = static_cast<int>(settings.iAreaSamples);
			bChanged |= ImGui::RadioButton("256##LightingSamples", &iSamples, 256);
			ImGui::SameLine();
			bChanged |= ImGui::RadioButton("512##LightingSamples", &iSamples, 512);
			settings.iAreaSamples = iSamples;
			int iEvent = static_cast<int>(settings.iEvent);
			bChanged |= ImGui::SliderInt("Event index##LightingEvent", &iEvent, 0, 119);
			settings.iEvent = iEvent;
			ImGui::BeginDisabled(settings.eMode == LightingComparisonMode::kDisabled);
			if (ImGui::Button("Capture##LightingCapture"))
			{
				++settings.uiCaptureRequest;
				bChanged = true;
			}
			ImGui::EndDisabled();
			if (bChanged)
			{
				gpGraphics->mpLighting->requestedComparisonSettings = settings;
			}
			const LightingDiagnostics& rDiagnostics = gpGraphics->mpLighting->diagnostics;
			ImGui::Text("Sources: %lld; triangles: %lld", rDiagnostics.iSources, rDiagnostics.iTriangles);
			ImGui::Text("Capture: %.*s; tiles: %lld/%lld", static_cast<int>(rDiagnostics.status.size()), rDiagnostics.status.data(), rDiagnostics.iCompletedTiles, rDiagnostics.iTotalTiles);
			ImGui::EndTabItem();
		}
		game::RenderLightingTweakExtensions(rScreen, iSection, 1);
		ImGui::EndTabBar();
	}
}

} // namespace engine

#endif // defined(BT_CLIENT)
