#pragma once

#include "Frame/GridCoord.h"

namespace engine
{

// One island listed in a cell, emitted by GenerateIslandChain. A cell holds 0..kiMaximumIslandsPerCell
// placements, those centred in the cell first (CentredIslands). An island that reaches several cells is
// listed in each of them; its copies hold equal islandCrc and fRotation bits and equal positions to rounding.
struct IslandPlacement
{
	common::crc_t islandCrc = 0;
	// Centered cell-local meters, possibly outside the cell's hexagon; the owning cell's GridCoord supplies the identity.
	XMFLOAT2 f2WorldPosition {};
	float fRotation = 0.0f;
};

// The world divides into rhombus regions this many cells a side, each generated as a whole.
inline constexpr int64_t kiRegionSideCells = 4;

// A ceiling on one cell's list that GenerateIslandChain asserts; the render arena sizes from it.
inline constexpr int64_t kiMaximumIslandsPerCell = 108;

// Deterministic archipelago generator. A region grows routes and island groups in its anchor cell's frame, packing
// every island by its rotated true valid-area hull (bounding rectangles may overlap, hidden underwater) inside the
// region's padded rhombus, so two regions never overlap. The region coordinate only seeds the RNG streams and no
// coordinate-derived term enters the geometry, so a layout depends only on that coordinate and the templates, and
// client and server agree in any generation order. A cell lists every placement whose hull comes within
// kfCellLandMarginMeters of its hexagon, in its own frame, regions ascending, and adds its own filler when none is
// centred in it. Main thread only: regions memoize in IslandTerrain::mRegionPlacements.
void GenerateIslandChain(GridCoord coordinate, std::vector<IslandPlacement>& rOut);

// The prefix of a cell's list whose positions pass IsInsideCell; indices into it are indices into the list.
std::span<const IslandPlacement> CentredIslands(std::span<const IslandPlacement> islands);

} // namespace engine
