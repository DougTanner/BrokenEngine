#pragma once

// Tuning knobs for SubdivideBeachBand. Bundled so the function signature stays short.
struct SubdivisionConfig
{
	float fBandMinMeters;     // Triangles whose Z-range overlaps [fBandMinMeters, fBandMaxMeters] are candidates
	float fBandMaxMeters;
	float fMaxEdgeMeters;     // In-band triangles subdivide until longest XY edge <= this
	int32_t iMaxDepth;        // Safety cap on recursive subdivision depth
};

// After Gaea Mesher load, recursively midpoint-split triangles overlapping the beach-Z band until the
// longest XY edge is <= fMaxEdgeMeters. Out-of-band neighbors absorb shared midpoints with minimal
// 1-to-2/3/4 splits and create none, limiting the cascade to one ring. Exceeding iMaxDepth skips the
// split and increments riDepthCapHits for the caller's warning. Append to the same vectors, compact dead
// triangles so no UINT32_MAX sentinel remains, and let the caller recompute vertex/index counts from
// final sizes.
void SubdivideBeachBand(std::vector<float>& rMeshPositions, std::vector<uint32_t>& rMeshIndices, const SubdivisionConfig& rConfig, int64_t& riDepthCapHits);
