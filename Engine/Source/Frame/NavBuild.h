#pragma once

namespace engine
{

struct IslandPlacement;

// Canonical island contour in UV space [0,1]x[0,1], built once from heightmap
struct NavContour
{
	std::vector<XMFLOAT2> vertices;
	std::vector<int64_t> polygonOffsets;
};

inline constexpr int64_t kiNavDataVersion = 15;

// Broad-phase edge grid dimensions (tunable). Mirrors the fixed-zone grid in Collision.h; finer here
// because obstacle edges are denser than collision objects. Only affects derived (non-serialized) data.
inline constexpr int64_t kiNavZonesX = 64;
inline constexpr int64_t kiNavZonesY = 64;

// Map a world coordinate to a nav-grid cell index on one axis. Shared by the builder and the query so
// bucketing and lookup always agree. Degenerate (near-zero) extent collapses to cell 0.
inline int64_t NavGridCell(float fValue, float fMinimum, float fMaximum, int64_t iZones)
{
	float fExtent = fMaximum - fMinimum;
	if (fExtent <= 1.0e-6f)
	{
		return 0;
	}
	int64_t iCell = static_cast<int64_t>((fValue - fMinimum) / fExtent * static_cast<float>(iZones));
	return std::clamp(iCell, 0i64, iZones - 1);
}

// Cell-local navigation data stored in CellStaticData.
struct NavData
{
	// Serialized content; layout changes require a kiNavDataVersion bump.
	// BuildCellNavigationData builds the whole-cell visibility graph on the server and ships it to clients;
	// island templates carry contour geometry only.
	std::vector<XMFLOAT2> vertices;
	std::vector<int32_t> polygonOffsets;
	std::vector<int32_t> visibilityEdgeA;
	std::vector<int32_t> visibilityEdgeB;

	// Derived acceleration is rebuilt by BuildNavigationAcceleration after server cell construction and
	// client/load deserialization. It is not serialized and does not change kiNavDataVersion.
	std::vector<XMFLOAT2> polygonMinimum;     // per polygon: AABB min (xy)
	std::vector<XMFLOAT2> polygonMaximum;     // per polygon: AABB max (xy)
	std::vector<int64_t> edgeA;           // obstacle perimeter edges: endpoint vertex indices
	std::vector<int64_t> edgeB;
	std::vector<int32_t> gridEdgeOffsets; // CSR offsets, size kiNavZonesX*kiNavZonesY + 1
	std::vector<int64_t> gridEdges;       // CSR payload: edge indices bucketed per grid cell
	std::vector<int32_t> adjacencyOffsets;      // CSR offsets per vertex, size vertices.size() + 1
	std::vector<int64_t> adjacencyNeighbors;    // CSR payload: neighbor vertex indices (visibility + polygon)
	XMFLOAT2 f2GridMinimum {};                  // global vertex AABB min (grid origin)
	XMFLOAT2 f2GridMaximum {};                  // global vertex AABB max

	void Write(std::ostream& rStream) const;
	void Read(std::istream& rStream);
};

void BuildNavContour(NavContour& rContour, std::span<const float> heightmapData, int64_t iHeightmapWidth, int64_t iHeightmapHeight, float fWorldThreshold, float fClearanceMeters, float fFootprintXMeters, float fFootprintYMeters);
void BuildCellNavigationData(NavData& rNavData, const std::vector<IslandPlacement>& rPlacements);

// Client and server must derive identical acceleration structures from identical serialized input.
void BuildNavigationAcceleration(NavData& rNavData);

} // namespace engine
