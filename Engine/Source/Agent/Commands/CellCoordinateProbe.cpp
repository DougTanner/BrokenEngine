#include "Pch.h"

#include "Agent/Commands/CellCoordinateProbe.h"

#include "Agent/AgentCommandsShared.h"
#include "Frame/CellStaticData.h"
#include "Frame/FrameUtils.h"
#include "Frame/IslandChainPlacement.h"
#include "Frame/IslandTerrain.h"
#include "Frame/NavQuery.h"

#include "Game.h"

namespace engine
{

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
// subscribing to it. A live cell is only read, for liveCrc.
void CommandCellCoordinateProbe(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!rParameters.contains("coord") || rParameters.size() != (rParameters.contains("navigation") ? 2 : 1))
	{
		throw std::runtime_error("cell_coordinate_probe accepts only 'coord' and optional 'navigation'");
	}
#if defined(BT_CLIENT)
	if (rParameters.contains("navigation"))
	{
		throw std::runtime_error("cell_coordinate_probe 'navigation' is server-only");
	}
#endif
	const nlohmann::json& rCoordinate = rParameters.at("coord");
	if (!rCoordinate.is_array() || rCoordinate.size() != 2)
	{
		throw std::runtime_error("cell_coordinate_probe 'coord' must be an array of 2 integers");
	}
	GridCoord coordinate {.iQ = static_cast<int32_t>(AgentGridCoordinateValue(rCoordinate.at(0), "cell_coordinate_probe 'coord'")), .iR = static_cast<int32_t>(AgentGridCoordinateValue(rCoordinate.at(1), "cell_coordinate_probe 'coord'"))};

	if (gpIslandTerrain == nullptr)
	{
		throw std::runtime_error("cell_coordinate_probe requires loaded island templates");
	}

	// Heap: Production builders generate temporary cell geometry; the probe writes JSON without retaining live state.
	ScopedSuppressAllocationTracking suppress;

	CellStaticData staticData {.coordinate = coordinate};
#if defined(BT_SERVER)
	std::vector<int64_t> graphs;
	std::vector<XMFLOAT2> points;
	if (rParameters.contains("navigation"))
	{
		const nlohmann::json& rNavigation = rParameters.at("navigation");
		if (!rNavigation.is_object() || rNavigation.size() != 2 || !rNavigation.contains("graphs") || !rNavigation.contains("points"))
		{
			throw std::runtime_error("cell_coordinate_probe 'navigation' requires only 'graphs' and 'points'");
		}
		const nlohmann::json& rGraphs = rNavigation.at("graphs");
		if (!rGraphs.is_array() || rGraphs.empty())
		{
			throw std::runtime_error("cell_coordinate_probe 'graphs' must be a nonempty array of distinct valid graph indices");
		}
		graphs.reserve(rGraphs.size());
		for (const nlohmann::json& rGraph : rGraphs)
		{
			if (!rGraph.is_number_integer() || (rGraph.is_number_unsigned() && !std::in_range<int64_t>(rGraph.get<uint64_t>())))
			{
				throw std::runtime_error("cell_coordinate_probe 'graphs' must contain valid integer graph indices");
			}
			int64_t iGraph = rGraph.get<int64_t>();
			if (iGraph < 0 || iGraph >= gpIslandTerrain->miNavigationGraphCount || std::ranges::contains(graphs, iGraph))
			{
				throw std::runtime_error("cell_coordinate_probe 'graphs' must contain distinct valid graph indices");
			}
			graphs.push_back(iGraph);
		}
		const nlohmann::json& rPoints = rNavigation.at("points");
		if (!rPoints.is_array())
		{
			throw std::runtime_error("cell_coordinate_probe 'points' must be an array of XY pairs");
		}
		points.reserve(rPoints.size());
		for (const nlohmann::json& rPoint : rPoints)
		{
			if (!rPoint.is_array() || rPoint.size() != 2)
			{
				throw std::runtime_error("cell_coordinate_probe 'points' must be an array of XY pairs");
			}
			points.push_back({AgentFloatValue(rPoint.at(0), "cell_coordinate_probe point X"), AgentFloatValue(rPoint.at(1), "cell_coordinate_probe point Y")});
		}
	}
#endif
	GenerateIslandChain(coordinate, staticData.islands);

	gpIslandTerrain->BuildElevationGrid(staticData.islands, staticData.elevationGrid);

	bool bSamplesFinite = std::ranges::all_of(staticData.elevationGrid, [](const float& fSample)
	{
		return std::isfinite(fSample);
	});

	rResult["coord"] = AgentCoordinateJson(coordinate);
	rResult["area"] = {{"width", 2.0f * kfCellHalfWidth}, {"height", 2.0f * kfCellHalfHeight}};
	rResult["terrain"] = {{"samplesFinite", bSamplesFinite}};
	rResult["placements"] = {{"count", std::ssize(staticData.islands)}, {"crc", PlacementsCrc(staticData.islands)}};
	auto it = game::gpGame->mCells.find(coordinate);
	if (it != game::gpGame->mCells.end())
	{
		rResult["placements"]["liveCrc"] = PlacementsCrc(it->second.staticData.islands);
	}
	nlohmann::json islands = nlohmann::json::array();
	for (const IslandPlacement& rPlacement : staticData.islands)
	{
		const IslandTemplate& rTemplate = gpIslandTerrain->mIslands.at(rPlacement.islandCrc);
		islands.push_back({{"crc", rPlacement.islandCrc}, {"x", rPlacement.f2WorldPosition.x}, {"y", rPlacement.f2WorldPosition.y}, {"rotation", rPlacement.fRotation}, {"footprint", {rTemplate.fWorldFootprintXMeters, rTemplate.fWorldFootprintYMeters}}, {"peak", rTemplate.fPeakHeightMeters}});
	}
	rResult["islands"] = std::move(islands);
	rResult["elevation"] = {{"crc", common::Crc(std::span<const float>(staticData.elevationGrid.data(), static_cast<size_t>(std::ssize(staticData.elevationGrid))))}};
#if defined(BT_SERVER)
	if (rParameters.contains("navigation"))
	{
		BuildCellNavigationData(staticData.navigationData, staticData.islands);
		nlohmann::json& rNavigation = rResult["navigation"];
		rNavigation["graphs"] = nlohmann::json::array();
		for (int64_t iGraph : graphs)
		{
			const NavData& rGraph = staticData.navigationData.at(iGraph);
			nlohmann::json vertices = nlohmann::json::array();
			for (const XMFLOAT2& rVertex : rGraph.vertices)
			{
				vertices.push_back({rVertex.x, rVertex.y});
			}
			nlohmann::json reachableEdges = nlohmann::json::array();
			for (int64_t i = 0; i < common::kiHexDirectionCount; ++i)
			{
				reachableEdges.push_back(NavQueryEdgeReachable(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), i, rGraph));
			}
			rNavigation["graphs"].push_back({{"index", iGraph}, {"vertices", std::move(vertices)}, {"polygonOffsets", rGraph.polygonOffsets}, {"reachableEdges", std::move(reachableEdges)}});
		}
		rNavigation["samples"] = nlohmann::json::array();
		CellElevationSampler sampler = gpIslandTerrain->MakeCellElevationSampler(staticData);
		for (const XMFLOAT2& rPoint : points)
		{
			XMVECTOR vecPosition = XMVectorSet(rPoint.x, rPoint.y, 0.0f, 1.0f);
			nlohmann::json blocked = nlohmann::json::array();
			for (int64_t iGraph : graphs)
			{
				blocked.push_back(NavQueryPointBlocked(vecPosition, staticData.navigationData.at(iGraph)));
			}
			rNavigation["samples"].push_back({{"xy", {rPoint.x, rPoint.y}}, {"elevation", sampler.Sample(vecPosition)}, {"blocked", std::move(blocked)}});
		}
	}
#endif
}

} // namespace engine
