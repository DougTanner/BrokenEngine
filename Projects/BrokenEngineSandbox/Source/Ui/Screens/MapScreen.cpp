#if defined(BT_CLIENT)

#include "MapScreen.h"

#include "Game.h"

namespace game
{

constexpr ImVec4 kMapBackgroundColor(0.02f, 0.05f, 0.09f, 1.0f);
constexpr ImVec4 kMapCellLineColor(0.55f, 0.75f, 0.9f, 1.0f);
// At the 2160-pixel reference height.
constexpr float kfMapCellLineThickness = 3.0f;
// Fraction of the squared map opacity.
constexpr float kfMapHoverFillAlpha = 0.15f;
// Fraction of the hexagon width.
constexpr float kfMapHoverTextWidth = 0.8f;
// Fraction of the squared map opacity.
constexpr float kfMapHoverTextAlpha = 0.5f;

// Projects a ground point in the camera cell's frame at z = 0; false when its depth falls outside [0, 1].
static bool ProjectGroundPoint(float fX, float fY, ImVec2& rOutPoint)
{
	XMVECTOR vecScreen = engine::gpCamera->WorldToScreen(XMVectorSet(fX, fY, 0.0f, 1.0f));
	float fDepth = XMVectorGetZ(vecScreen);
	if (fDepth < 0.0f || fDepth > 1.0f)
	{
		return false;
	}
	rOutPoint = ImVec2(XMVectorGetX(vecScreen), XMVectorGetY(vecScreen));
	return true;
}

void MapScreen::Render()
{
	float fOpacity = gpGame->MapOpacity();
	if (fOpacity <= 0.0f)
	{
		mOptionalCellPlacements.reset();
		return;
	}

	// Heap: ImGui may grow the background draw list's vertex and index buffers and the font atlas; the first open adds
	// the island pictures to the atlas, and the placement cache grows per newly shown cell.
	ScopedSuppressAllocationTracking suppress;

	const engine::IslandTerrain& rIslandTerrain = *engine::gpIslandTerrain;
	ImFontAtlas* pAtlas = ImGui::GetIO().Fonts;
	if (mThumbnailRectIds.empty())
	{
		mThumbnailRectIds.reserve(static_cast<size_t>(std::ssize(rIslandTerrain.mIslandCrcsSorted)));
		for (common::crc_t uiIslandCrc : rIslandTerrain.mIslandCrcsSorted)
		{
			const engine::IslandTemplate& rTemplate = rIslandTerrain.mIslands.at(uiIslandCrc);
			ImFontAtlasRect rect;
			ImFontAtlasRectId iRectId = pAtlas->AddCustomRect(static_cast<int>(rTemplate.iThumbnailWidth), static_cast<int>(rTemplate.iThumbnailHeight), &rect);
			ASSERT(iRectId != ImFontAtlasRectId_Invalid);
			// Read after the add, which can grow the atlas into a new texture.
			ImTextureData* pTexture = pAtlas->TexData;
			int64_t iRowBytes = rTemplate.iThumbnailWidth * 4;
			for (int64_t i = 0; i < rTemplate.iThumbnailHeight; ++i)
			{
				std::memcpy(pTexture->GetPixelsAt(rect.x, rect.y + static_cast<int>(i)), rTemplate.puiThumbnailPixels + i * iRowBytes, static_cast<size_t>(iRowBytes));
			}
			mThumbnailRectIds.push_back(iRectId);
		}
	}
	if (!mOptionalCellPlacements.has_value())
	{
		mOptionalCellPlacements.emplace();
	}

	ImDrawList* pDrawList = ImGui::GetBackgroundDrawList();
	pDrawList->AddRectFilled(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, ImGui::ColorConvertFloat4ToU32(ImVec4(kMapBackgroundColor.x, kMapBackgroundColor.y, kMapBackgroundColor.z, fOpacity)));

	// Flat-top hexagon corners around a cell's centre, counter-clockwise from east.
	static constexpr float kfHalfWidth = engine::kfCellHalfWidth;
	static constexpr float kfHalfHeight = engine::kfCellHalfHeight;
	static constexpr XMFLOAT2 kpf2Corners[6] {{kfHalfWidth, 0.0f}, {0.5f * kfHalfWidth, kfHalfHeight}, {-0.5f * kfHalfWidth, kfHalfHeight}, {-kfHalfWidth, 0.0f}, {-0.5f * kfHalfWidth, -kfHalfHeight}, {0.5f * kfHalfWidth, -kfHalfHeight}};

	// A centred island draws only from its own cell, and its quad can overhang that cell's hexagon by up to the largest
	// template's half footprint diagonal.
	float fLargestHalfDiagonal = 0.0f;
	for ([[maybe_unused]] const auto& [rCrc, rIsland] : rIslandTerrain.mIslands)
	{
		fLargestHalfDiagonal = std::max(fLargestHalfDiagonal, 0.5f * std::hypot(rIsland.fQuadFootprintX, rIsland.fQuadFootprintY));
	}
	float fCellReach = kfHalfWidth + fLargestHalfDiagonal;

	// The camera works in its own cell's frame, centred on that cell. A cell at grid distance k has its centre at least
	// k * sqrt(3) * apothem from there, and a cell whose hexagon or islands overlap the visible area has its centre within
	// fCellReach of an area point, so the farthest area corner bounds the rings to visit.
	XMFLOAT4 f4VisibleArea = engine::gpCamera->mf4LargeVisibleArea;
	float fFarthestX = std::max(std::abs(f4VisibleArea.x), std::abs(f4VisibleArea.z));
	float fFarthestY = std::max(std::abs(f4VisibleArea.y), std::abs(f4VisibleArea.w));
	float fFarthestDistance = std::sqrt(fFarthestX * fFarthestX + fFarthestY * fFarthestY);
	int64_t iRadius = static_cast<int64_t>(std::ceil((fFarthestDistance + fCellReach) / (std::numbers::sqrt3_v<float> * engine::kfCellApothem)));

	ImU32 uiLineColor = ImGui::ColorConvertFloat4ToU32(ImVec4(kMapCellLineColor.x, kMapCellLineColor.y, kMapCellLineColor.z, fOpacity));
	ImU32 uiHoverFillColor = ImGui::ColorConvertFloat4ToU32(ImVec4(kMapCellLineColor.x, kMapCellLineColor.y, kMapCellLineColor.z, fOpacity * fOpacity * kfMapHoverFillAlpha));
	ImU32 uiPictureColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, fOpacity));
	float fLineThickness = kfMapCellLineThickness * engine::UiScale();
	const ImVec2& rvMousePosition = ImGui::GetIO().MousePos;
	XMFLOAT2 f2MousePosition(rvMousePosition.x, rvMousePosition.y);
	bool bHovered = false;
	engine::GridCoord hoveredCoordinate {};
	ImVec2 vHoveredCenter {};
	for (int64_t i = -iRadius; i <= iRadius; ++i)
	{
		for (int64_t j = std::max(-iRadius, -i - iRadius); j <= std::min(iRadius, -i + iRadius); ++j)
		{
			XMVECTOR vecCenter = common::HexCenterOffset(i, j, engine::kCellDirections);
			if (!engine::gpCamera->InVisibleArea(f4VisibleArea, vecCenter, fCellReach, fCellReach, fCellReach, fCellReach))
			{
				continue;
			}
			float fCenterX = XMVectorGetX(vecCenter);
			float fCenterY = XMVectorGetY(vecCenter);

			engine::GridCoord coordinate {};
			bool bHasCoordinate = engine::TryAddGridCoordinate(engine::gpCamera->mBasisCoordinate, i, j, coordinate);
			if (bHasCoordinate)
			{
				auto [itCell, bInserted] = mOptionalCellPlacements->try_emplace(coordinate);
				if (bInserted)
				{
					engine::GenerateIslandChain(coordinate, itCell->second);
				}

				for (const engine::IslandPlacement& rPlacement : engine::CentredIslands(itCell->second))
				{
					const engine::IslandTemplate& rTemplate = rIslandTerrain.mIslands.at(rPlacement.islandCrc);
					ImFontAtlasRect rect;
					pAtlas->GetCustomRect(mThumbnailRectIds.at(rTemplate.iTemplateArrayIndex), &rect);

					// North-west, north-east, south-east, south-west in the island's local frame, rotated by the transpose
					// of the sampling rotation as PlacementPeakPosition does; picture row 0 is local +Y, mapped to v0.
					float fHalfX = 0.5f * rTemplate.fQuadFootprintX;
					float fHalfY = 0.5f * rTemplate.fQuadFootprintY;
					const XMFLOAT2 pf2LocalCorners[4] {{-fHalfX, fHalfY}, {fHalfX, fHalfY}, {fHalfX, -fHalfY}, {-fHalfX, -fHalfY}};
					common::SinCos rotation = common::DeterministicSinCos(-rPlacement.fRotation);
					ImVec2 pQuad[4] {};
					bool bQuadProjected = true;
					for (const auto& [k, rf2Local] : std::views::enumerate(pf2LocalCorners))
					{
						float fDeltaX = rf2Local.x * rotation.fCos + rf2Local.y * rotation.fSin;
						float fDeltaY = rf2Local.y * rotation.fCos - rf2Local.x * rotation.fSin;
						if (!ProjectGroundPoint(fCenterX + rPlacement.f2WorldPosition.x + fDeltaX, fCenterY + rPlacement.f2WorldPosition.y + fDeltaY, pQuad[k]))
						{
							bQuadProjected = false;
							break;
						}
					}
					if (bQuadProjected)
					{
						pDrawList->AddImageQuad(pAtlas->TexRef, pQuad[0], pQuad[1], pQuad[2], pQuad[3], rect.uv0, ImVec2(rect.uv1.x, rect.uv0.y), rect.uv1, ImVec2(rect.uv0.x, rect.uv1.y), uiPictureColor);
					}
				}
			}

			ImVec2 pPoints[std::size(kpf2Corners)] {};
			bool bProjected = true;
			for (const auto& [k, rf2Corner] : std::views::enumerate(kpf2Corners))
			{
				if (!ProjectGroundPoint(fCenterX + rf2Corner.x, fCenterY + rf2Corner.y, pPoints[k]))
				{
					bProjected = false;
					break;
				}
			}
			if (bProjected)
			{
				if (bHasCoordinate && !bHovered)
				{
					XMFLOAT2 pf2Points[std::size(pPoints)] {};
					for (const auto& [k, rvPoint] : std::views::enumerate(pPoints))
					{
						pf2Points[k] = XMFLOAT2(rvPoint.x, rvPoint.y);
					}
					if (common::PointInPolygon(f2MousePosition, pf2Points))
					{
						bHovered = true;
						hoveredCoordinate = coordinate;
						vHoveredCenter = ImVec2(0.5f * (pPoints[0].x + pPoints[3].x), 0.5f * (pPoints[0].y + pPoints[3].y));
						pDrawList->AddConvexPolyFilled(pPoints, static_cast<int>(std::size(pPoints)), uiHoverFillColor);
					}
				}
				pDrawList->AddPolyline(pPoints, static_cast<int>(std::size(pPoints)), uiLineColor, ImDrawFlags_Closed, fLineThickness);
			}
		}
	}

	// Drawn after the loop so no later cell's outline or island picture covers it.
	if (bHovered)
	{
		char pcCoordinate[32];
		int64_t iCoordinateLength = std::snprintf(pcCoordinate, sizeof(pcCoordinate), "(%d,%d)", hoveredCoordinate.iQ, hoveredCoordinate.iR);

		// A hexagon's width at the look-at on the settled map, so neither camera translation nor the open and close ease
		// changes the font size.
		float fFramingHalfHeight = engine::gpCamera->mfOverrideTarget * std::tan(0.5f * XMConvertToRadians(engine::CameraBase::kfFramingFieldOfView));
		float fHexagonWidth = engine::kfCellHalfWidth * ImGui::GetIO().DisplaySize.y / fFramingHalfHeight;

		ImFont* pFont = ImGui::GetFont();
		float fBaseFontSize = ImGui::GetFontSize();
		// Sized from the label's length, not its text, so labels of one length share one baked font size.
		float fBaseTextWidth = pFont->CalcTextSizeA(fBaseFontSize, FLT_MAX, 0.0f, "8").x * static_cast<float>(iCoordinateLength);
		float fFontSize = fBaseFontSize * kfMapHoverTextWidth * fHexagonWidth / fBaseTextWidth;
		if (fFontSize < 0.5f)
		{
			return;
		}

		ImVec2 vTextSize = pFont->CalcTextSizeA(fFontSize, FLT_MAX, 0.0f, pcCoordinate);
		ImU32 uiTextColor = ImGui::ColorConvertFloat4ToU32(ImVec4(kMapCellLineColor.x, kMapCellLineColor.y, kMapCellLineColor.z, fOpacity * fOpacity * kfMapHoverTextAlpha));

		// Unsnapped, the text moves sub-pixel with the outlines.
		pDrawList->PushDrawFlag(ImDrawFlags_TextNoPixelSnap, true);
		pDrawList->AddText(pFont, fFontSize, ImVec2(vHoveredCenter.x - 0.5f * vTextSize.x, vHoveredCenter.y - 0.5f * vTextSize.y), uiTextColor, pcCoordinate);
		pDrawList->PopDrawFlag();
	}
}

} // namespace game

#endif // defined(BT_CLIENT)
