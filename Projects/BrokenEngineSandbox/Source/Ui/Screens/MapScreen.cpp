#if defined(BT_CLIENT)

#include "MapScreen.h"

#include "Game.h"

namespace game
{

constexpr ImVec4 kMapBackgroundColor(0.02f, 0.05f, 0.09f, 1.0f);
constexpr ImVec4 kMapCellLineColor(0.55f, 0.75f, 0.9f, 1.0f);
// At the 2160-pixel reference height.
constexpr float kfMapCellLineThickness = 3.0f;

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

	// The camera works in its own cell's frame, centred on that cell. A cell at grid distance k has its centre at least
	// k * sqrt(3) * apothem from there, and a cell overlapping the visible area has its centre within a half width of an
	// area point, so the farthest area corner bounds the rings to visit.
	XMFLOAT4 f4VisibleArea = engine::gpCamera->mf4LargeVisibleArea;
	float fFarthestX = std::max(std::abs(f4VisibleArea.x), std::abs(f4VisibleArea.z));
	float fFarthestY = std::max(std::abs(f4VisibleArea.y), std::abs(f4VisibleArea.w));
	float fFarthestDistance = std::sqrt(fFarthestX * fFarthestX + fFarthestY * fFarthestY);
	int64_t iRadius = static_cast<int64_t>(std::ceil((fFarthestDistance + kfHalfWidth) / (std::numbers::sqrt3_v<float> * engine::kfCellApothem)));

	ImU32 uiLineColor = ImGui::ColorConvertFloat4ToU32(ImVec4(kMapCellLineColor.x, kMapCellLineColor.y, kMapCellLineColor.z, fOpacity));
	ImU32 uiPictureColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, fOpacity));
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
			float fCenterX = XMVectorGetX(vecCenter);
			float fCenterY = XMVectorGetY(vecCenter);

			engine::GridCoord coordinate {};
			if (engine::TryAddGridCoordinate(engine::gpCamera->mBasisCoordinate, i, j, coordinate))
			{
				auto [itCell, bInserted] = mOptionalCellPlacements->try_emplace(coordinate);
				if (bInserted)
				{
					engine::GenerateIslandChain(coordinate, itCell->second);
				}

				for (const engine::IslandPlacement& rPlacement : itCell->second)
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
				pDrawList->AddPolyline(pPoints, static_cast<int>(std::size(pPoints)), uiLineColor, ImDrawFlags_Closed, fLineThickness);
			}
		}
	}
}

} // namespace game

#endif // defined(BT_CLIENT)
