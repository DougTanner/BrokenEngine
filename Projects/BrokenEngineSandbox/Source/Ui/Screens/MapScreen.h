#pragma once

#if defined(BT_CLIENT)

namespace game
{

// Full-screen map overlay: an opaque background, each shown cell's islands as top-down pictures from the font atlas, and
// the world's cell hexagons, projected through the live camera, all at the map opacity.
class MapScreen
{
public:

	void Render();

	// One font-atlas rect per island template, indexed by IslandTemplate::iTemplateArrayIndex; added on the first open.
	std::vector<ImFontAtlasRectId> mThumbnailRectIds;

	// Client-generated placements of each shown cell while the map opacity is above zero. Held in std::optional so
	// the implicit constructor allocates nothing: a default-constructed std::unordered_map allocates.
	std::optional<std::unordered_map<engine::GridCoord, std::vector<engine::IslandPlacement>>> mOptionalCellPlacements;
};

} // namespace game

#endif // defined(BT_CLIENT)
