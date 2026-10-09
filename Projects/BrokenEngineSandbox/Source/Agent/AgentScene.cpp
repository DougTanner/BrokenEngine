#include "Agent/AgentScene.h"

#if defined(BT_CLIENT)

#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Players/Players.h"
#include "Frame/Collections/Spaceships/Spaceships.h"
#include "Game.h"

namespace game
{

// Coordinates and positions use the shared agent formatting (engine::AgentCoordinateJson / AgentLocalPositionJson);
// this local helper covers the remaining plain XY pairs, which are screen pixels and template footprints.
static nlohmann::json Vector2ToJson(float fX, float fY)
{
	return nlohmann::json::array({fX, fY});
}

// Boolean PlayerFlags only — the packed nav-direction (bits 8-10) and nav-waypoint (bits 12-13) ranges are
// excluded; iterating them as single bits would emit meaningless names.
static nlohmann::json PlayerFlagNames(PlayerFlags_t flags)
{
	nlohmann::json names = nlohmann::json::array();
	if (flags & PlayerFlags::kExploding)
	{
		names.push_back("kExploding");
	}
	if (flags & PlayerFlags::kFireBlaster)
	{
		names.push_back("kFireBlaster");
	}
	if (flags & PlayerFlags::kBlasterSpawnLeft)
	{
		names.push_back("kBlasterSpawnLeft");
	}
	if (flags & PlayerFlags::kFireMissile)
	{
		names.push_back("kFireMissile");
	}
	if (flags & PlayerFlags::kMissileSpawnLeft)
	{
		names.push_back("kMissileSpawnLeft");
	}
	if (flags & PlayerFlags::kTransfer)
	{
		names.push_back("kTransfer");
	}
	if (flags & PlayerFlags::kUseMissiles)
	{
		names.push_back("kUseMissiles");
	}
	if (flags & PlayerFlags::kIsFlagship)
	{
		names.push_back("kIsFlagship");
	}
	if (flags & PlayerFlags::kPendingUseMissiles)
	{
		names.push_back("kPendingUseMissiles");
	}
	return names;
}

static nlohmann::json SpaceshipFlagNames(SpaceshipFlags_t flags)
{
	nlohmann::json names = nlohmann::json::array();
	if (flags & SpaceshipFlags::kFleePlayer)
	{
		names.push_back("kFleePlayer");
	}
	if (flags & SpaceshipFlags::kExploding)
	{
		names.push_back("kExploding");
	}
	if (flags & SpaceshipFlags::kReturnToIslandCenter)
	{
		names.push_back("kReturnToIslandCenter");
	}
	if (flags & SpaceshipFlags::kTransfer)
	{
		names.push_back("kTransfer");
	}
	return names;
}

// Only the focused fleet reports its members; non-focused fleets get index + focused flag only.
static nlohmann::json BuildFleets()
{
	nlohmann::json fleets = nlohmann::json::array();
	int64_t iFleetCount = std::ssize(gpGame->mFleetSelection.mClientFleets);
	int64_t iFocusedIndex = gpGame->mFleetSelection.miFocusedFleetIndex;
	const Fleet* pFocused = gpGame->mFleetSelection.FocusedFleet();
	for (int64_t i = 0; i < iFleetCount; ++i)
	{
		bool bFocused = i == iFocusedIndex;
		nlohmann::json fleetJson;
		fleetJson["index"] = i;
		fleetJson["focused"] = bFocused;
		if (bFocused)
		{
			nlohmann::json members = nlohmann::json::array();
			for (const FleetMember& rMember : pFocused->members)
			{
				members.push_back({{"globalPlayerId", rMember.globalPlayerId.iValue}, {"alive", !(rMember.flags & FleetMemberFlags::kIsDead)}});
			}
			fleetJson["members"] = std::move(members);
		}
		fleets.push_back(std::move(fleetJson));
	}
	return fleets;
}

void CommandDescribeScene(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (rParameters.contains("includeUnits") && !rParameters.at("includeUnits").is_boolean())
	{
		throw std::runtime_error("describe_scene 'includeUnits' must be a bool");
	}
	if (rParameters.contains("maxUnits") && !rParameters.at("maxUnits").is_number_integer())
	{
		throw std::runtime_error("describe_scene 'maxUnits' must be an integer");
	}
	if (rParameters.contains("unitTypes") && !rParameters.at("unitTypes").is_array())
	{
		throw std::runtime_error("describe_scene 'unitTypes' must be an array of strings");
	}
	bool bIncludeUnits = !rParameters.contains("includeUnits") || rParameters.at("includeUnits").get<bool>();
	int64_t iMaximumUnits = rParameters.contains("maxUnits") ? rParameters.at("maxUnits").get<int64_t>() : 200;
	if (iMaximumUnits < 0)
	{
		throw std::runtime_error("describe_scene 'maxUnits' must be a non-negative integer");
	}

	// Absent 'unitTypes' means every type; a present list (including an empty one) selects exactly what it names.
	bool bHasUnitTypes = rParameters.contains("unitTypes");
	bool bIncludePlayers = !bHasUnitTypes;
	bool bIncludeSpaceships = !bHasUnitTypes;
	bool bIncludeBlasters = !bHasUnitTypes;
	if (bHasUnitTypes)
	{
		for (const nlohmann::json& rUnitType : rParameters.at("unitTypes"))
		{
			if (!rUnitType.is_string())
			{
				throw std::runtime_error("describe_scene 'unitTypes' must be an array of strings");
			}
			std::string unitType = rUnitType.get<std::string>();
			if (unitType == "player")
			{
				bIncludePlayers = true;
			}
			else if (unitType == "spaceship")
			{
				bIncludeSpaceships = true;
			}
			else if (unitType == "blaster")
			{
				bIncludeBlasters = true;
			}
			else
			{
				throw std::runtime_error("describe_scene 'unitTypes' entries must be 'player', 'spaceship', or 'blaster'");
			}
		}
	}

	// Camera / UI / game state — always present (graceful empty state: no subscribed coords still returns these).
	XMFLOAT4 f4VisibleArea = engine::gpCamera->mf4RenderVisibleArea;
	// Camera eye and visibleArea use the local frame identified by camera.basisCoord.
	rResult["camera"] =
	{
		{"basisCoord", engine::AgentCoordinateJson(engine::gpCamera->mBasisCoordinate)},
		{"eye", engine::AgentLocalPositionJson(engine::gpCamera->mVecEyePosition)},
		{"visibleArea", nlohmann::json::array({f4VisibleArea.x, f4VisibleArea.y, f4VisibleArea.z, f4VisibleArea.w})},
		{"lod", engine::gpCamera->miVisibleAreaLevelOfDetail},
	};
	rResult["uiState"] = engine::UiStateName(gpGame->meUiState);
	rResult["tweaksVisible"] = gpGame->mbShowImGui;
	rResult["gameFlags"] = engine::GameFlagNames(gpGame->mGameFlags);
	rResult["tick"] = gpGame->miTickCounter;
	rResult["clientGridCoord"] = engine::AgentCoordinateJson(gpGame->mClientGridCoordinate);
	rResult["fleets"] = BuildFleets();

	nlohmann::json subscribedCoordinates = nlohmann::json::array();
	nlohmann::json units = nlohmann::json::array();
	nlohmann::json islands = nlohmann::json::array();
	int64_t iPlayerTotal = 0;
	int64_t iSpaceshipTotal = 0;
	int64_t iMissileTotal = 0;
	int64_t iBlasterTotal = 0;
	int64_t iUnitCount = 0;
	bool bTruncated = false;

	for (const auto& [rCoordinate, rCell] : gpGame->mCells)
	{
		subscribedCoordinates.push_back(engine::AgentCoordinateJson(rCoordinate));

		// Island placements come from staticData, available regardless of whether a snapshot has arrived.
		const engine::CellStaticData& rStaticData = rCell.staticData;
		bool bHasFootprint = std::ssize(rStaticData.islandRenderQueries) == std::ssize(rStaticData.islands);
		for (int64_t i = 0; i < std::ssize(rStaticData.islands); ++i)
		{
			const engine::IslandPlacement& rPlacement = rStaticData.islands.at(i);
			nlohmann::json islandJson;
			islandJson["coord"] = engine::AgentCoordinateJson(rCoordinate);
			// The placement center, in the owning cell's local meters like every other position here.
			islandJson["local"] = Vector2ToJson(rPlacement.f2WorldPosition.x, rPlacement.f2WorldPosition.y);
			islandJson["rotation"] = rPlacement.fRotation;
			if (bHasFootprint)
			{
				const engine::IslandRenderQuery& rQuery = rStaticData.islandRenderQueries.at(i);
				islandJson["footprint"] = Vector2ToJson(rQuery.fFootprintX, rQuery.fFootprintY);
			}
			islands.push_back(std::move(islandJson));
		}

		// Units and counts need a rendered snapshot; RenderFrame asserts iSnapshotCount > 0.
		if (rCell.iSnapshotCount <= 0)
		{
			continue;
		}
		const Frame& rFrame = gpGame->RenderFrame(rCoordinate);

		// Unit positions are local to this cell, while the visible area and the screen projection are in the
		// camera cell's frame, so every row rebases once before the cull and the projection and reports the
		// unrebased local value.
		engine::RenderBasis basis = engine::MakeRenderBasis(rCoordinate, engine::gpCamera->mBasisCoordinate);

		iPlayerTotal += rFrame.postRender.pPlayers->iCount;
		iSpaceshipTotal += rFrame.postRender.pSpaceships->iCount;
		iMissileTotal += rFrame.postRender.pMissiles->iCount;
		iBlasterTotal += rFrame.postRender.pBlasters->iCount;

		if (!bIncludeUnits)
		{
			continue;
		}

		if (bIncludePlayers)
		{
			const PlayersInterpolate& rPlayersInterpolate = *rFrame.interpolate.pPlayers;
			const PlayersPostRender& rPlayersPost = *rFrame.postRender.pPlayers;
			for (int64_t i = 0; i < rPlayersPost.iCount; ++i)
			{
				XMVECTOR vecLocalPosition = rPlayersInterpolate.pVecPositions[i];
				XMVECTOR vecRenderPosition = engine::Rebase(basis, vecLocalPosition);
				if (!engine::gpCamera->InVisibleArea(f4VisibleArea, vecRenderPosition))
				{
					continue;
				}
				if (iUnitCount >= iMaximumUnits)
				{
					bTruncated = true;
					break;
				}
				XMVECTOR vecScreen = engine::gpCamera->WorldToScreen(vecRenderPosition);
				units.push_back(
				{
					{"type", "player"},
					{"globalId", rPlayersPost.pGlobalPlayerIds[i].iValue},
					{"coord", engine::AgentCoordinateJson(rCoordinate)},
					{"local", engine::AgentLocalPositionJson(vecLocalPosition)},
					{"screen", Vector2ToJson(XMVectorGetX(vecScreen), XMVectorGetY(vecScreen))},
					{"armor", rPlayersPost.pfArmors[i]},
					{"shield", rPlayersPost.pfShields[i]},
					{"alignment", rPlayersPost.pAlignments[i].uiValue},
					{"flags", PlayerFlagNames(rPlayersPost.pFlags[i])},
				});
				++iUnitCount;
			}
		}

		if (bIncludeSpaceships)
		{
			const SpaceshipsInterpolate& rShipsInterpolate = *rFrame.interpolate.pSpaceships;
			const SpaceshipsPostRender& rShipsPost = *rFrame.postRender.pSpaceships;
			for (int64_t i = 0; i < rShipsPost.iCount; ++i)
			{
				XMVECTOR vecLocalPosition = rShipsInterpolate.pVecPositions[i];
				XMVECTOR vecRenderPosition = engine::Rebase(basis, vecLocalPosition);
				if (!engine::gpCamera->InVisibleArea(f4VisibleArea, vecRenderPosition))
				{
					continue;
				}
				if (iUnitCount >= iMaximumUnits)
				{
					bTruncated = true;
					break;
				}
				XMVECTOR vecScreen = engine::gpCamera->WorldToScreen(vecRenderPosition);
				units.push_back(
				{
					{"type", "spaceship"},
					{"coord", engine::AgentCoordinateJson(rCoordinate)},
					{"local", engine::AgentLocalPositionJson(vecLocalPosition)},
					{"screen", Vector2ToJson(XMVectorGetX(vecScreen), XMVectorGetY(vecScreen))},
					{"health", rShipsPost.pfHealths[i]},
					{"alignment", rShipsPost.pAlignments[i].uiValue},
					{"flags", SpaceshipFlagNames(rShipsPost.pFlags[i])},
				});
				++iUnitCount;
			}
		}

		// Blasters stay last so a cell over the unit budget truncates these rows instead of displacing the others.
		if (bIncludeBlasters)
		{
			const BlastersInterpolate& rBlastersInterpolate = *rFrame.interpolate.pBlasters;
			const BlastersPostRender& rBlastersPost = *rFrame.postRender.pBlasters;
			for (int64_t i = 0; i < rBlastersPost.iCount; ++i)
			{
				XMVECTOR vecLocalPosition = rBlastersInterpolate.pVecPositions[i];
				XMVECTOR vecRenderPosition = engine::Rebase(basis, vecLocalPosition);
				if (!engine::gpCamera->InVisibleArea(f4VisibleArea, vecRenderPosition))
				{
					continue;
				}
				if (iUnitCount >= iMaximumUnits)
				{
					bTruncated = true;
					break;
				}
				XMVECTOR vecScreen = engine::gpCamera->WorldToScreen(vecRenderPosition);
				units.push_back(
				{
					{"type", "blaster"},
					{"coord", engine::AgentCoordinateJson(rCoordinate)},
					{"local", engine::AgentLocalPositionJson(vecLocalPosition)},
					{"screen", Vector2ToJson(XMVectorGetX(vecScreen), XMVectorGetY(vecScreen))},
					{"alignment", rBlastersPost.pAlignments[i].uiValue},
					{"windTrailIntensity", rBlastersInterpolate.pfWindTrailIntensities[i]},
					{"windTrailWidth", rBlastersInterpolate.pfWindTrailWidths[i]},
					{"windTrailLengthMultiplier", rBlastersInterpolate.pfWindTrailLengthMultipliers[i]},
				});
				++iUnitCount;
			}
		}
	}

	rResult["subscribedCoords"] = std::move(subscribedCoordinates);
	rResult["units"] = std::move(units);
	rResult["counts"] =
	{
		{"players", iPlayerTotal},
		{"spaceships", iSpaceshipTotal},
		{"missiles", iMissileTotal},
		{"blasters", iBlasterTotal},
	};
	rResult["islands"] = std::move(islands);
	rResult["truncated"] = bTruncated;
}

} // namespace game

#endif // defined(BT_CLIENT)
