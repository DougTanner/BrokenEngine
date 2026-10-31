#if defined(BT_CLIENT)

#include "MapScreen.h"

#include "Game.h"

namespace game
{

constexpr ImVec4 kMapBackgroundColor(0.02f, 0.05f, 0.09f, 1.0f);
constexpr ImVec4 kMapCellLineColor(0.55f, 0.75f, 0.9f, 1.0f);
// At the 2160-pixel reference height.
constexpr float kfMapCellLineThickness = 3.0f;

void MapScreen::Render()
{
	float fOpacity = gpGame->MapOpacity();
	if (fOpacity <= 0.0f)
	{
		return;
	}

	// Heap: ImGui may grow the background draw list's vertex and index buffers.
	ScopedSuppressAllocationTracking suppress;

	ImDrawList* pDrawList = ImGui::GetBackgroundDrawList();
	pDrawList->AddRectFilled(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, ImGui::ColorConvertFloat4ToU32(ImVec4(kMapBackgroundColor.x, kMapBackgroundColor.y, kMapBackgroundColor.z, fOpacity)));

	// Flat-top hexagon corners around a cell's centre, counter-clockwise from east.
	static constexpr float kfHalfWidth = engine::kfCellHalfWidth;
	static constexpr float kfHalfHeight = engine::kfCellHalfHeight;
	static constexpr XMFLOAT2 kpf2Corners[6] {{kfHalfWidth, 0.0f}, {0.5f * kfHalfWidth, kfHalfHeight}, {-0.5f * kfHalfWidth, kfHalfHeight}, {-kfHalfWidth, 0.0f}, {-0.5f * kfHalfWidth, -kfHalfHeight}, {0.5f * kfHalfWidth, -kfHalfHeight}};

	// The camera works in its own cell's frame, centred on that cell. A cell at grid distance k has its centre at least
	// k * sqrt(3) * apothem from there, and a cell overlapping the visible area has its centre within a half width of an
	// area point, so the farthest area corner bounds the rings to visit.
	XMFLOAT4 f4VisibleArea = engine::gpCamera->mf4LargeVisibleArea;
	float fFarthestX = std::max(std::abs(f4VisibleArea.x), std::abs(f4VisibleArea.z));
	float fFarthestY = std::max(std::abs(f4VisibleArea.y), std::abs(f4VisibleArea.w));
	float fFarthestDistance = std::sqrt(fFarthestX * fFarthestX + fFarthestY * fFarthestY);
	int64_t iRadius = static_cast<int64_t>(std::ceil((fFarthestDistance + kfHalfWidth) / (std::numbers::sqrt3_v<float> * engine::kfCellApothem)));

	ImU32 uiLineColor = ImGui::ColorConvertFloat4ToU32(ImVec4(kMapCellLineColor.x, kMapCellLineColor.y, kMapCellLineColor.z, fOpacity));
	float fLineThickness = kfMapCellLineThickness * engine::UiScale();
	for (int64_t i = -iRadius; i <= iRadius; ++i)
	{
		for (int64_t j = std::max(-iRadius, -i - iRadius); j <= std::min(iRadius, -i + iRadius); ++j)
		{
			XMVECTOR vecCenter = common::HexCenterOffset(i, j, engine::kCellDirections);
			if (!engine::gpCamera->InVisibleArea(f4VisibleArea, vecCenter, kfHalfWidth, kfHalfWidth, kfHalfWidth, kfHalfWidth))
			{
				continue;
			}

			ImVec2 pPoints[std::size(kpf2Corners)] {};
			bool bProjected = true;
			for (const auto& [k, rf2Corner] : std::views::enumerate(kpf2Corners))
			{
				XMVECTOR vecScreen = engine::gpCamera->WorldToScreen(XMVectorSet(XMVectorGetX(vecCenter) + rf2Corner.x, XMVectorGetY(vecCenter) + rf2Corner.y, 0.0f, 1.0f));
				float fDepth = XMVectorGetZ(vecScreen);
				if (fDepth < 0.0f || fDepth > 1.0f)
				{
					bProjected = false;
					break;
				}
				pPoints[k] = ImVec2(XMVectorGetX(vecScreen), XMVectorGetY(vecScreen));
			}
			if (bProjected)
			{
				pDrawList->AddPolyline(pPoints, static_cast<int>(std::size(pPoints)), uiLineColor, ImDrawFlags_Closed, fLineThickness);
			}
		}
	}
}

} // namespace game

#endif // defined(BT_CLIENT)
