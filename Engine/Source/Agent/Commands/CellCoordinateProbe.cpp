#include "Pch.h"

#include "Agent/Commands/CellCoordinateProbe.h"

#include "Agent/AgentCommandsShared.h"
#include "Frame/FrameUtils.h"
#include "Frame/IslandChainPlacement.h"
#include "Frame/IslandTerrain.h"

namespace engine
{

namespace
{

// Trust boundary: the whole signed-int32 domain is a legal cell identity, so the only rejected integers are the
// ones no GridCoord can hold.
int32_t ProbeGridCoordValue(const nlohmann::json& rValue)
{
	if (!rValue.is_number_integer())
	{
		throw std::runtime_error("cell_coordinate_probe 'coord' must be an array of 2 integers");
	}

	if (rValue.is_number_unsigned())
	{
		uint64_t uiValue = rValue.get<uint64_t>();
		if (uiValue > static_cast<uint64_t>(std::numeric_limits<int32_t>::max()))
		{
			throw std::runtime_error("cell_coordinate_probe 'coord' values must fit in a signed 32-bit integer");
		}
		return static_cast<int32_t>(uiValue);
	}

	int64_t iValue = rValue.get<int64_t>();
	if (iValue < static_cast<int64_t>(std::numeric_limits<int32_t>::min()) || iValue > static_cast<int64_t>(std::numeric_limits<int32_t>::max()))
	{
		throw std::runtime_error("cell_coordinate_probe 'coord' values must fit in a signed 32-bit integer");
	}
	return static_cast<int32_t>(iValue);
}

// How many of the terrain grid's kiElevationGridDim sample positions along one axis resolve to distinct floats;
// fewer means neighbouring samples share a float at this coordinate. The positions increase monotonically, so
// exact inequality against the predecessor — not an approximate comparison — finds every such collapse.
int64_t DistinctAxisSamplePositions()
{
	constexpr float kfGridPitch = kfCellWidth / static_cast<float>(kiElevationGridDim);
	int64_t iDistinct = 0;
	float fPrevious = 0.0f;
	for (int64_t i = 0; i < kiElevationGridDim; ++i)
	{
		float fPosition = kfBaseAreaMinX + (static_cast<float>(i) + 0.5f) * kfGridPitch;
		if (i == 0 || fPosition != fPrevious)
		{
			++iDistinct;
		}
		fPrevious = fPosition;
	}
	return iDistinct;
}

// Fold the placements field by field in generation order, matching FrameStaticData::Write's member order rather
// than hashing the struct's object representation, whose tail padding is not guaranteed equal between endpoints.
common::crc_t PlacementsCrc(const std::vector<IslandPlacement>& rPlacements)
{
	common::crc_t crc = common::Crc(static_cast<int64_t>(rPlacements.size()));
	for (const IslandPlacement& rPlacement : rPlacements)
	{
		crc = (crc ^ common::Crc(rPlacement.islandCrc)) * common::kCrcMultiplier;
		crc = (crc ^ common::Crc(rPlacement.f2WorldPos)) * common::kCrcMultiplier;
		crc = (crc ^ common::Crc(rPlacement.fRotation)) * common::kCrcMultiplier;
	}
	return crc;
}

} // namespace

// cell_coordinate_probe: report the cell geometry a requested coordinate produces, on either endpoint, without
// subscribing to it or touching live cells. Schema: {"coord":[x,y]}.
void CommandCellCoordinateProbe(const nlohmann::json& rParams, nlohmann::json& rResult)
{
	if (!rParams.is_object() || rParams.size() != 1 || !rParams.contains("coord"))
	{
		throw std::runtime_error("cell_coordinate_probe accepts only 'coord'");
	}
	const nlohmann::json& rCoord = rParams.at("coord");
	if (!rCoord.is_array() || rCoord.size() != 2)
	{
		throw std::runtime_error("cell_coordinate_probe 'coord' must be an array of 2 integers");
	}
	const GridCoord coord {ProbeGridCoordValue(rCoord.at(0)), ProbeGridCoordValue(rCoord.at(1))};

	if (gpIslandTerrain == nullptr)
	{
		throw std::runtime_error("cell_coordinate_probe requires loaded island templates");
	}

	// Heap: a locally owned placement list and one 1024x1024 elevation grid (~4 MB), built here through the same
	// free functions the frame tick and cell creation call, and freed when this scope ends. Both of those sites
	// suppress tracking for the same allocations. One coord at a time keeps the peak at a single grid.
	ScopedSuppressAllocationTracking suppress;

	std::vector<IslandPlacement> placements;
	GenerateIslandChain(coord, placements);

	std::vector<float> elevationGrid;
	gpIslandTerrain->BuildElevationGrid(placements, elevationGrid);

	bool bSamplesFinite = true;
	for (float fSample : elevationGrid)
	{
		if (!std::isfinite(fSample))
		{
			bSamplesFinite = false;
			break;
		}
	}

	FrameBounds bounds = ComputeFrameBounds(LocalFrameArea());
	rResult["coord"] = AgentCoordJson(coord);
	rResult["area"] = {{"width", bounds.fMaxX - bounds.fMinX}, {"height", bounds.fMaxY - bounds.fMinY}};
	rResult["terrain"] = {{"axisSamplePositions", DistinctAxisSamplePositions()}, {"samplesFinite", bSamplesFinite}};
	rResult["placements"] = {{"count", static_cast<int64_t>(placements.size())}, {"crc", PlacementsCrc(placements)}};
	rResult["elevation"] = {{"crc", common::Crc(elevationGrid.data(), static_cast<int64_t>(elevationGrid.size()))}};
}

} // namespace engine
