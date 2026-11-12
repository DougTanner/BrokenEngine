#pragma once

#include "Frame/GridCoord.h"
#include "Frame/IslandChainPlacement.h"
#include "Frame/NavBuild.h"

namespace engine
{

class IslandTerrain;

struct IslandRenderQuery
{
	float fCosine = 1.0f;
	float fSine = 0.0f;
	XMFLOAT2 f2WorldPosition {};
	float fFootprintX = 0.0f;
	float fFootprintY = 0.0f;
	const uint16_t* puiHeightmapHalf = nullptr;
	int64_t iHeightmapWidth = 0;
	int64_t iHeightmapHeight = 0;
};

enum class CellStaticDataFlags : uint8_t
{
	kNavigationDataBuilt = 1ui32 << 0,
	// Empty island lists are prepared without allocating a grid.
	kTerrainPrepared = 1ui32 << 1,
};

struct CellStaticData
{
	GridCoord coordinate {};
	std::vector<IslandPlacement> islands;
	// Derived from islands + per-template NavContours. Built lazily on the per-coord
	// frame-tick thread (server-only) and refreshed when the network resends staticData,
	// so it lives outside the persisted save format. mutable lets RunFrameTick fill it
	// through the const CellStaticData& it gets from ActiveFrameReference. One NavData per
	// baked graph, index-parallel with the server's NavigationGraphBake entries; a client cell whose static
	// data has not arrived holds an empty vector.
	mutable std::vector<NavData> navigationData;

	// A completed navigation build can have no vertices (degenerate or underwater contours), so emptiness cannot
	// signal an unbuilt cell. BuildCellNavigationData on the server and Read on the client set this flag; a cell a save
	// reload creates starts with it clear. This flag is outside the CRC and save format.
	mutable common::Flags<CellStaticDataFlags> flags;

	// Derived from placements and resident heightmaps, outside persistence and CRC. Live clients prepare it
	// before publication; server and menu ticks fill it through ActiveFrameReference's const static data.
	mutable std::vector<float> elevationGrid;

	// Client-only queries for GlobalElevation (ProjectToBaseHeight), index-parallel to islands.
	// Prepared alongside elevationGrid, avoiding per-island hash lookups and trigonometry. Menu placement edits
	// clear the cache for the next synchronous tick. Derived from shared templates and never serialized.
	mutable std::vector<IslandRenderQuery> islandRenderQueries;

	// Network-only compact geometry and server-built visibility; Read rebuilds navigation acceleration.
	void Write(std::ostream& rStream) const;
	void Read(std::istream& rStream);

	void BuildRenderPlacementCache(const IslandTerrain& rIslandTerrain) const;
};

} // namespace engine
