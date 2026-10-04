#include "Pch.h"

#if defined(BT_CLIENT)

#include "Network/Client/Client.h"
#include "ProfileManagerBase.h"

#include "Profile/ProfileManager.h"

namespace engine
{

static ImPlotPoint SmoothedGetter(int iIndex, void* pData)
{
	const common::Smoothed<int64_t>* pSmoothed = static_cast<const common::Smoothed<int64_t>*>(pData);
	int64_t iStart = pSmoothed->miNext - pSmoothed->miCount;
	if (iStart < 0)
	{
		iStart += common::Smoothed<int64_t>::kiCapacity;
	}
	int64_t iActual = (iStart + iIndex) % common::Smoothed<int64_t>::kiCapacity;
	return ImPlotPoint(static_cast<double>(iIndex), static_cast<double>(pSmoothed->mpValues[iActual]));
}

static void PlotSmoothed(const char* pcLabel, const common::Smoothed<int64_t>& rSmoothed)
{
	if (ImPlot::BeginPlot(pcLabel, ImVec2(-1.0f, 120.0f * engine::UiScale())))
	{
		ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_AutoFit);
		ImPlot::PlotLineG(pcLabel, SmoothedGetter, const_cast<common::Smoothed<int64_t>*>(&rSmoothed), static_cast<int>(rSmoothed.miCount));
		ImPlot::EndPlot();
	}
}

void RenderImPlotGraphs()
{
	if (gpProfileManager->meProfileScreen != engine::ProfileScreen::kNetwork)
	{
		return;
	}

	if (engine::gpClient == nullptr)
	{
		return;
	}

	float fUiScale = engine::UiScale();
	ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, 10.0f * fUiScale), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x * 0.48f, ImGui::GetIO().DisplaySize.y - 20.0f * fUiScale), ImGuiCond_Always);

	if (ImGui::Begin("Network Graphs", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse))
	{
		PlotSmoothed("RTT (ms)", gpProfileManager->mSmoothedRoundTripTime);
		PlotSmoothed("Jitter (ms)", gpProfileManager->mSmoothedJitter);
		PlotSmoothed("Rollback (ticks)", gpProfileManager->mSmoothedRollback);
		PlotSmoothed("Server Buffer", gpProfileManager->mSmoothedBuffer);
		PlotSmoothed("Clock Error", gpProfileManager->mSmoothedClockError);
	}
	ImGui::End();
}

} // namespace engine

#endif // BT_CLIENT
