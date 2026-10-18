#include "Pch.h"

#if defined(BT_CLIENT)

#include "Agent/Commands/DescribeScene.h"

#include "Game.h"

namespace engine
{

// Coordinates and positions use the shared agent formatting (AgentCoordinateJson / AgentLocalPositionJson); this
// local helper covers the remaining plain XY pairs, which are island centers and template footprints.
static nlohmann::json Vector2ToJson(float fX, float fY)
{
	return nlohmann::json::array({fX, fY});
}

static nlohmann::json BuildCellState(GridCoord coordinate, const Cell& rCell)
{
	nlohmann::json result;
	result["coord"] = AgentCoordinateJson(coordinate);
	result["confirmedTick"] = rCell.iConfirmedTick;
	result["confirmedOffset"] = rCell.iConfirmedOffset;
	result["highWaterValidatedTick"] = rCell.iHighWaterValidatedTick;
	result["lastFullStateTick"] = rCell.iLastFullStateTick;
	result["snapshotHead"] = rCell.iSnapshotHead;
	result["snapshotCount"] = rCell.iSnapshotCount;
	result["lastRenderedTick"] = rCell.iLastRenderedTick;
	result["serverUpdateCount"] = rCell.serverUpdates.size();
	result["lastReplayConfirmedTick"] = rCell.iLastReplayConfirmedTick;
	result["lastReplayServerUpdateCount"] = rCell.iLastReplayServerUpdateCount;
	result["stuckFrameCount"] = rCell.iStuckFrameCount;

	result["pendingFullStateTick"] = nullptr;
	if (rCell.optionalPendingFullState.has_value())
	{
		result["pendingFullStateTick"] = rCell.optionalPendingFullState->iTick;
	}

	result["firstServerUpdateTick"] = nullptr;
	result["lastServerUpdateTick"] = nullptr;
	if (!rCell.serverUpdates.empty())
	{
		auto [firstUpdateIt, lastUpdateIt] = std::ranges::minmax_element(rCell.serverUpdates, {}, [](const auto& rEntry)
		{
			return rEntry.first;
		});
		result["firstServerUpdateTick"] = firstUpdateIt->first;
		result["lastServerUpdateTick"] = lastUpdateIt->first;
	}

	result["tailTick"] = -1;
	const game::Frame* pTailFrame = rCell.TailFrame();
	if (pTailFrame != nullptr)
	{
		result["tailTick"] = pTailFrame->interpolate.iTick;
	}
	return result;
}

void CommandDescribeScene([[maybe_unused]] const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	// Camera / UI / game state — always present (graceful empty state: no subscribed coords still returns these).
	XMFLOAT4 f4VisibleArea = gpCamera->mf4RenderVisibleArea;
	// Camera eye and visibleArea use the local frame identified by camera.basisCoord.
	rResult["camera"] =
	{
		{"basisCoord", AgentCoordinateJson(gpCamera->mBasisCoordinate)},
		{"eye", AgentLocalPositionJson(gpCamera->mVecEyePosition)},
		{"visibleArea", nlohmann::json::array({f4VisibleArea.x, f4VisibleArea.y, f4VisibleArea.z, f4VisibleArea.w})},
		{"lod", gpCamera->miVisibleAreaLevelOfDetail},
	};
	AppendUiStateFields(rResult);
	rResult["tick"] = game::gpGame->miTickCounter;
	rResult["clientGridCoord"] = AgentCoordinateJson(game::gpGame->mClientGridCoordinate);

	nlohmann::json subscribedCoordinates = nlohmann::json::array();
	nlohmann::json cellStates = nlohmann::json::array();
	nlohmann::json islands = nlohmann::json::array();
	for (const auto& [rCoordinate, rCell] : game::gpGame->mCells)
	{
		subscribedCoordinates.push_back(AgentCoordinateJson(rCoordinate));
		cellStates.push_back(BuildCellState(rCoordinate, rCell));

		// Island placements come from staticData, available regardless of whether a snapshot has arrived.
		const CellStaticData& rStaticData = rCell.staticData;
		bool bHasFootprint = std::ssize(rStaticData.islandRenderQueries) == std::ssize(rStaticData.islands);
		for (int64_t i = 0; i < std::ssize(rStaticData.islands); ++i)
		{
			const IslandPlacement& rPlacement = rStaticData.islands.at(i);
			nlohmann::json islandJson;
			islandJson["coord"] = AgentCoordinateJson(rCoordinate);
			// The placement center, in the owning cell's local meters like every other position here.
			islandJson["local"] = Vector2ToJson(rPlacement.f2WorldPosition.x, rPlacement.f2WorldPosition.y);
			islandJson["rotation"] = rPlacement.fRotation;
			if (bHasFootprint)
			{
				const IslandRenderQuery& rQuery = rStaticData.islandRenderQueries.at(i);
				islandJson["footprint"] = Vector2ToJson(rQuery.fFootprintX, rQuery.fFootprintY);
			}
			islands.push_back(std::move(islandJson));
		}
	}

	rResult["subscribedCoords"] = std::move(subscribedCoordinates);
	rResult["cellStates"] = std::move(cellStates);
	rResult["islands"] = std::move(islands);
}

} // namespace engine

#endif // defined(BT_CLIENT)
