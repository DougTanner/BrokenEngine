#pragma once

// Private geometry predicates shared by NavBuild (contour building), NavCellData (cell merge,
// acceleration, serialization), and NavQuery (runtime LOS/A*). Definitions stay in their owning
// translation units; NavBuild.h is the public API. Shared epsilon and boundary rules prevent
// build/query disagreement that routes paths through walls.

namespace engine
{

struct NavData;

template <typename T>
inline std::pair<int32_t, int32_t> PolygonRange(const std::vector<int32_t>& rPolygonOffsets, T polygonIndex, int32_t iVertexTotal)
{
	int32_t iStart = rPolygonOffsets.at(static_cast<size_t>(polygonIndex));
	int32_t iEnd = (polygonIndex + 1 < static_cast<T>(rPolygonOffsets.size())) ? rPolygonOffsets.at(static_cast<size_t>(polygonIndex + 1)) : iVertexTotal;
	return {iStart, iEnd};
}

// Returns true if segments (A1,A2) and (B1,B2) properly intersect (interior crossing only, not
// endpoint touching). Defined in NavBuild.cpp.
bool SegmentsIntersect(XMFLOAT2 f2A1, XMFLOAT2 f2A2, XMFLOAT2 f2B1, XMFLOAT2 f2B2);

// Winding-number point-in-polygon test over one polygon's vertices (pointer + count, so callers can
// pass a sub-range of a larger vertex buffer). Defined in NavBuild.cpp.
bool PointInPolygon(XMFLOAT2 f2Point, const XMFLOAT2* pVertices, int32_t iVertexCount);

// Does the segment cross any obstacle edge? Grid-DDA broad phase over the NavData edge CSR. Defined in
// NavQuery.cpp; the whole-cell visibility build in NavCellData.cpp shares it so an edge exists in the
// graph iff the runtime blocked test says the segment is clear.
bool SegmentBlockedByObstacle(XMFLOAT2 f2A, XMFLOAT2 f2B, const XMFLOAT2* pVertices, const NavData& rNavData);

// Is the point inside any obstacle polygon? Per-polygon AABB broad phase over PointInPolygon. Defined
// in NavQuery.cpp; shared with the same visibility build.
bool PointInAnyPolygon(XMFLOAT2 f2Point, const XMFLOAT2* pVertices, const NavData& rNavData);

} // namespace engine
