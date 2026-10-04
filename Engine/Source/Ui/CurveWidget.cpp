#include "CurveWidget.h"

#if defined(BT_CLIENT)

namespace engine
{

constexpr int kiCurveSamples = 256;
constexpr float kfPointRadius = 8.0f;    // control-point dot radius and DragPoint grab half-size in pixels
constexpr float kfGrabTolerance = 24.0f; // pixel radius within which a left-click targets an existing point instead of adding a new one
constexpr ImVec4 kGoldColor(0.95f, 0.75f, 0.2f, 1.0f);

bool CurveWidget(std::string_view label, CurveData& rCurve)
{
	ImGuiIO& rInputOutput = ImGui::GetIO();
	ImVec2 size(rInputOutput.DisplaySize.x * 0.5f, rInputOutput.DisplaySize.y * 0.5f);

	char pcIdentifier[128];
	std::snprintf(pcIdentifier, sizeof(pcIdentifier), "##Curve_%.*s", static_cast<int>(label.size()), label.data());

	ImGui::TextUnformatted(label.data(), label.data() + label.size());

	float fYMinimum = rCurve.mfYMinimum;
	float fYMaximum = rCurve.mfYMaximum;

	bool bInteracting = false;

	// CanvasOnly drops title/legend/menus/box-select/mouse-text but keeps the axis gridlines.
	if (ImPlot::BeginPlot(pcIdentifier, size, ImPlotFlags_CanvasOnly))
	{
		// Lock the view to [0,1] x [YMin,YMax] and hide tick labels — the curve editor is a fixed canvas, not a pannable chart.
		static constexpr ImPlotAxisFlags kiAxisFlags = ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_Lock;
		ImPlot::SetupAxes(nullptr, nullptr, kiAxisFlags, kiAxisFlags);
		ImPlot::SetupAxesLimits(0.0, 1.0, static_cast<double>(fYMinimum), static_cast<double>(fYMaximum), ImPlotCond_Always);

		// Faint vertical tick per spread-pass sample position (drawn first so it sits behind the curve).
		float pfPassTicks[shaders::kiMaxSpreadPasses] {};
		for (int64_t i = 0; i < shaders::kiMaxSpreadPasses; ++i)
		{
			float fSamplePosition = 0.5f;
			if constexpr (shaders::kiMaxSpreadPasses > 1)
			{
				fSamplePosition = static_cast<float>(i) / static_cast<float>(shaders::kiMaxSpreadPasses - 1);
			}
			pfPassTicks[i] = fSamplePosition;
		}
		ImPlotSpec passSpecification;
		passSpecification.LineColor = ImVec4(1.0f, 1.0f, 1.0f, 0.08f);
		ImPlot::PlotInfLines("##Passes", pfPassTicks, shaders::kiMaxSpreadPasses, passSpecification);

		// y=1 is the reference level.
		if (fYMinimum <= 1.0f && fYMaximum >= 1.0f)
		{
			float fOne = 1.0f;
			ImPlotSpec oneSpecification;
			oneSpecification.LineColor = ImVec4(0.7f, 0.7f, 0.4f, 0.8f);
			oneSpecification.LineWeight = 2.0f;
			oneSpecification.Flags = ImPlotInfLinesFlags_Horizontal;
			ImPlot::PlotInfLines("##YOne", &fOne, 1, oneSpecification);
		}

		// Curve polyline (257 samples through the monotone-cubic evaluator).
		float pfX[kiCurveSamples + 1] {};
		float pfY[kiCurveSamples + 1] {};
		for (int64_t i = 0; i <= kiCurveSamples; ++i)
		{
			float fSamplePosition = static_cast<float>(i) / static_cast<float>(kiCurveSamples);
			pfX[i] = fSamplePosition;
			pfY[i] = rCurve.Evaluate(fSamplePosition);
		}
		ImPlotSpec curveSpecification;
		curveSpecification.LineColor = kGoldColor;
		curveSpecification.LineWeight = 2.0f;
		ImPlot::PlotLine("##Curve", pfX, pfY, kiCurveSamples + 1, curveSpecification);

		// Nearest existing control point within the grab tolerance (pixel space) — drives add-suppression and
		// right-click deletion, preventing a click aimed at an existing point from adding a stray one.
		ImVec2 mousePixels = ImGui::GetMousePos();
		int64_t iNearestPoint = -1;
		float fGrabTolerance = kfGrabTolerance * UiScale();
		float fNearestDistanceSquared = fGrabTolerance * fGrabTolerance;
		for (int64_t i = 0; i < std::ssize(rCurve.mPoints); ++i)
		{
			const ImVec2& rPoint = rCurve.mPoints.at(i);
			ImVec2 pointPixels = ImPlot::PlotToPixels(static_cast<double>(rPoint.x), static_cast<double>(rPoint.y));
			float fDeltaX = mousePixels.x - pointPixels.x;
			float fDeltaY = mousePixels.y - pointPixels.y;
			float fDistanceSquared = fDeltaX * fDeltaX + fDeltaY * fDeltaY;
			if (fDistanceSquared < fNearestDistanceSquared)
			{
				fNearestDistanceSquared = fDistanceSquared;
				iNearestPoint = i;
			}
		}

		// CanvasOnly disables the context menu; process clicks before DragPoint so a new point can grab the current click.
		if (ImPlot::IsPlotHovered())
		{
			if (iNearestPoint < 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				ImPlotPoint mouse = ImPlot::GetPlotMousePos();
				if (rCurve.AddPoint(ImVec2(static_cast<float>(mouse.x), static_cast<float>(mouse.y))) >= 0)
				{
					bInteracting = true;
				}
			}
			else if (iNearestPoint >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !rCurve.IsEndpoint(iNearestPoint))
			{
				rCurve.RemovePoint(iNearestPoint);
				bInteracting = true;
			}
		}

		// Draggable control points (drawn last so they sit atop the curve). CurveData owns endpoint X-locking and
		// interior neighbor clamping, so the dragged value is fed straight through MovePoint and re-read next frame.
		for (int64_t i = 0; i < std::ssize(rCurve.mPoints); ++i)
		{
			const ImVec2& rPoint = rCurve.mPoints.at(i);
			double fX = rPoint.x;
			double fY = rPoint.y;
			bool bHeld = false;
			if (ImPlot::DragPoint(static_cast<int>(i), &fX, &fY, kGoldColor, kfPointRadius * UiScale(), ImPlotDragToolFlags_Delayed, nullptr, nullptr, &bHeld))
			{
				rCurve.MovePoint(i, ImVec2(static_cast<float>(fX), static_cast<float>(fY)));
				bInteracting = true;
			}
			if (bHeld)
			{
				bInteracting = true;
			}
		}

		ImPlot::EndPlot();
	}

	return bInteracting;
}

} // namespace engine

#endif // BT_CLIENT
