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

struct FrameStaticData
{
	GridCoord coordinate {};
	std::vector<IslandPlacement> islands;
	// Derived from islands + per-template NavContour. Built lazily on the per-coord
	// frame-tick thread (server-only) and refreshed when the network resends staticData,
	// so it lives outside the persisted save format. mutable lets RunFrameTick fill it
	// through the const FrameStaticData& it gets from ActiveFrameReference.
	mutable NavData navigationData;

	// A completed navigation build can have no vertices (degenerate or underwater contours), so emptiness cannot
	// signal an unbuilt cell. BuildCellNavigationData on the server and Read(true) on the client set this flag; Read(false)
	// clears it and navigationData for a save reload. This flag is outside the CRC and save format.
	mutable bool bNavigationDataBuilt = false;

	// Derived from islands + per-template heightmaps. Built lazily on the per-coord
	// frame-tick thread (both client and server build their own bit-identical copy from
	// the same deterministic placements), refreshed when the network resends staticData,
	// so it lives outside the persisted save format. mutable lets RunFrameTick fill it
	// through the const FrameStaticData& it gets from ActiveFrameReference. Sized
	// kiElevationGridDimension^2 when populated.
	mutable std::vector<float> elevationGrid;

	// Client-only queries for GlobalElevation/GlobalNormal (ProjectToBaseHeight), index-parallel to islands.
	// RunFrameTick caches each placement's inverse rotation and template footprint, heightmap pointer and dimensions
	// alongside elevationGrid, avoiding per-island hash lookups and trigonometry. Placement edits clear the cache;
	// the next tick rebuilds an empty cache. It is derived from placements and shared templates and never serialized.
	mutable std::vector<IslandRenderQuery> islandRenderQueries;

	void Write(std::ostream& rStream, bool bIncludeNavigationData) const;
	void Read(std::istream& rStream, bool bIncludeNavigationData);

	void BuildRenderPlacementCache(const IslandTerrain& rIslandTerrain) const;
};

} // namespace engine
