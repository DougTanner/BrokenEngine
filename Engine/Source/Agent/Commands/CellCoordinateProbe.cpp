#include "Pch.h"

#include "Agent/Commands/CellCoordinateProbe.h"

#include "Agent/AgentCommandsShared.h"
#include "Frame/FrameUtils.h"
#include "Frame/IslandChainPlacement.h"
#include "Frame/IslandTerrain.h"

namespace engine
{

// How many of the terrain grid's kiElevationGridDimension sample positions along one axis resolve to distinct floats;
// fewer means neighbouring samples share a float at this coordinate. The positions increase monotonically, so
// exact inequality against the predecessor — not an approximate comparison — finds every such collapse.
static constexpr int64_t DistinctAxisSamplePositions()
{
	static constexpr float kfGridPitch = kfCellWidth / static_cast<float>(kiElevationGridDimension);
	int64_t iDistinct = 0;
	float fPrevious = 0.0f;
	for (int64_t i = 0; i < kiElevationGridDimension; ++i)
	{
		float fPosition = kfBaseAreaMinimumX + (static_cast<float>(i) + 0.5f) * kfGridPitch;
		if (i == 0 || fPosition != fPrevious)
		{
			++iDistinct;
		}
		fPrevious = fPosition;
	}
	return iDistinct;
}

// Fold the placements field by field in generation order, matching CellStaticData::Write's member order rather
// than hashing the struct's object representation, whose tail padding is not guaranteed equal between endpoints.
static common::crc_t PlacementsCrc(const std::vector<IslandPlacement>& rPlacements)
{
	common::crc_t crc = common::Crc(std::ssize(rPlacements));
	for (const IslandPlacement& rPlacement : rPlacements)
	{
		crc = (crc ^ common::Crc(rPlacement.islandCrc)) * common::kCrcMultiplier;
		crc = (crc ^ common::Crc(rPlacement.f2WorldPosition)) * common::kCrcMultiplier;
		crc = (crc ^ common::Crc(rPlacement.fRotation)) * common::kCrcMultiplier;
	}
	return crc;
}

// cell_coordinate_probe: report the cell geometry a requested coordinate produces, on either endpoint, without
// subscribing to it or touching live cells. Schema: {"coord":[x,y]}.
void CommandCellCoordinateProbe(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!rParameters.is_object() || rParameters.size() != 1 || !rParameters.contains("coord"))
	{
		throw std::runtime_error("cell_coordinate_probe accepts only 'coord'");
	}
	const nlohmann::json& rCoordinate = rParameters.at("coord");
	if (!rCoordinate.is_array() || rCoordinate.size() != 2)
	{
		throw std::runtime_error("cell_coordinate_probe 'coord' must be an array of 2 integers");
	}
	GridCoord coordinate {.iX = static_cast<int32_t>(AgentGridCoordinateValue(rCoordinate.at(0), "cell_coordinate_probe 'coord'")), .iY = static_cast<int32_t>(AgentGridCoordinateValue(rCoordinate.at(1), "cell_coordinate_probe 'coord'"))};

	if (gpIslandTerrain == nullptr)
	{
		throw std::runtime_error("cell_coordinate_probe requires loaded island templates");
	}

	// Placement generation and elevation building share their implementations and allocation tracking suppression
	// with cell creation and the frame tick. One coordinate at a time limits peak elevation storage to one
	// 1024x1024 grid (~4 MB).
	ScopedSuppressAllocationTracking suppress;

	std::vector<IslandPlacement> placements;
	GenerateIslandChain(coordinate, placements);

	std::vector<float> elevationGrid;
	gpIslandTerrain->BuildElevationGrid(placements, elevationGrid);

	bool bSamplesFinite = std::ranges::all_of(elevationGrid, [](const float& fSample)
	{
		return std::isfinite(fSample);
	});

	CellBounds bounds = ComputeCellBounds(LocalCellArea());
	rResult["coord"] = AgentCoordinateJson(coordinate);
	rResult["area"] = {{"width", bounds.fMaxX - bounds.fMinX}, {"height", bounds.fMaxY - bounds.fMinY}};
	rResult["terrain"] = {{"axisSamplePositions", DistinctAxisSamplePositions()}, {"samplesFinite", bSamplesFinite}};
	rResult["placements"] = {{"count", std::ssize(placements)}, {"crc", PlacementsCrc(placements)}};
	rResult["elevation"] = {{"crc", common::Crc(std::span<const float>(elevationGrid.data(), static_cast<size_t>(std::ssize(elevationGrid))))}};
}

} // namespace engine
