#include "Agent/AgentScene.h"

#if defined(BT_CLIENT)

#include "Agent/Commands/DescribeScene.h"

#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Omnis/Omnis.h"
#include "Frame/Collections/Spaceships/Spaceships.h"
#include "Game.h"

namespace game
{

// Coordinates and positions use the shared agent formatting (engine::AgentCoordinateJson / AgentLocalPositionJson);
// this local helper covers the remaining plain XY pair, the unit screen pixels.
static nlohmann::json Vector2ToJson(float fX, float fY)
{
	return nlohmann::json::array({fX, fY});
}

static void AppendLifecycleFlagNames(engine::LifecycleFlags_t lifecycleFlags, nlohmann::json& rNames)
{
	if (lifecycleFlags & engine::LifecycleFlags::kTransfer)
	{
		rNames.push_back("kTransfer");
	}
	if (lifecycleFlags & engine::LifecycleFlags::kDestroy)
	{
		rNames.push_back("kDestroy");
	}
	if (lifecycleFlags & engine::LifecycleFlags::kDying)
	{
		rNames.push_back("kDying");
	}
}

// Boolean OmniFlags only — the packed nav-direction (bits 8-10) and nav-waypoint (bits 12-13) ranges are
// excluded; iterating them as single bits would emit meaningless names.
static nlohmann::json OmniFlagNames(engine::LifecycleFlags_t lifecycleFlags, OmniFlags_t flags)
{
	nlohmann::json names = nlohmann::json::array();
	AppendLifecycleFlagNames(lifecycleFlags, names);
	if (flags & OmniFlags::kFireBlaster)
	{
		names.push_back("kFireBlaster");
	}
	if (flags & OmniFlags::kBlasterSpawnLeft)
	{
		names.push_back("kBlasterSpawnLeft");
	}
	if (flags & OmniFlags::kFireMissile)
	{
		names.push_back("kFireMissile");
	}
	if (flags & OmniFlags::kMissileSpawnLeft)
	{
		names.push_back("kMissileSpawnLeft");
	}
	if (flags & OmniFlags::kUseMissiles)
	{
		names.push_back("kUseMissiles");
	}
	if (flags & OmniFlags::kIsFlagship)
	{
		names.push_back("kIsFlagship");
	}
	if (flags & OmniFlags::kPendingUseMissiles)
	{
		names.push_back("kPendingUseMissiles");
	}
	return names;
}

static nlohmann::json SpaceshipFlagNames(engine::LifecycleFlags_t lifecycleFlags, SpaceshipFlags_t flags)
{
	nlohmann::json names = nlohmann::json::array();
	AppendLifecycleFlagNames(lifecycleFlags, names);
	if (flags & SpaceshipFlags::kFleeOmni)
	{
		names.push_back("kFleeOmni");
	}
	if (flags & SpaceshipFlags::kReturnToIslandCenter)
	{
		names.push_back("kReturnToIslandCenter");
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
				members.push_back({{"globalId", rMember.globalId.iValue}, {"alive", !(rMember.flags & FleetMemberFlags::kIsDead)}});
			}
			fleetJson["members"] = std::move(members);
		}
		fleets.push_back(std::move(fleetJson));
	}
	return fleets;
}

// rBuildRow(i) returns only the type-specific fields; this adds the shared type, coord, local, and screen keys.
template <typename BUILD_ROW>
static void AppendVisibleUnits(std::string_view type, const XMVECTOR* pVecPositions, int64_t iRowCount, engine::GridCoord coordinate, const engine::RenderBasis& rBasis, XMFLOAT4 f4VisibleArea, int64_t iMaximumUnits, int64_t& riUnitCount, bool& rbTruncated, nlohmann::json& rUnits, BUILD_ROW&& rBuildRow)
{
	for (int64_t i = 0; i < iRowCount; ++i)
	{
		XMVECTOR vecLocalPosition = pVecPositions[i];
		XMVECTOR vecRenderPosition = engine::Rebase(rBasis, vecLocalPosition);
		if (!engine::gpCamera->InVisibleArea(f4VisibleArea, vecRenderPosition))
		{
			continue;
		}
		if (riUnitCount >= iMaximumUnits)
		{
			rbTruncated = true;
			break;
		}
		XMVECTOR vecScreen = engine::gpCamera->WorldToScreen(vecRenderPosition);
		nlohmann::json row = rBuildRow(i);
		row["type"] = type;
		row["coord"] = engine::AgentCoordinateJson(coordinate);
		row["local"] = engine::AgentLocalPositionJson(vecLocalPosition);
		row["screen"] = Vector2ToJson(XMVectorGetX(vecScreen), XMVectorGetY(vecScreen));
		rUnits.push_back(std::move(row));
		++riUnitCount;
	}
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
	bool bIncludeOmnis = !bHasUnitTypes;
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
			if (unitType == "omni")
			{
				bIncludeOmnis = true;
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
				throw std::runtime_error("describe_scene 'unitTypes' entries must be 'omni', 'spaceship', or 'blaster'");
			}
		}
	}

	engine::CommandDescribeScene(rParameters, rResult);

	XMFLOAT4 f4VisibleArea = engine::gpCamera->mf4RenderVisibleArea;
	rResult["fleets"] = BuildFleets();

	nlohmann::json units = nlohmann::json::array();
	int64_t iOmniTotal = 0;
	int64_t iSpaceshipTotal = 0;
	int64_t iMissileTotal = 0;
	int64_t iBlasterTotal = 0;
	int64_t iUnitCount = 0;
	bool bTruncated = false;

	for (const auto& [rCoordinate, rCell] : gpGame->mCells)
	{
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

		iOmniTotal += rFrame.postRender.pOmnis->iCount;
		iSpaceshipTotal += rFrame.postRender.pSpaceships->iCount;
		iMissileTotal += rFrame.postRender.pMissiles->iCount;
		iBlasterTotal += rFrame.postRender.pBlasters->iCount;

		if (!bIncludeUnits)
		{
			continue;
		}

		if (bIncludeOmnis)
		{
			const OmnisInterpolate& rOmnisInterpolate = *rFrame.interpolate.pOmnis;
			const OmnisPostRender& rOmnisPost = *rFrame.postRender.pOmnis;
			AppendVisibleUnits("omni", rOmnisInterpolate.pVecPositions, rOmnisPost.iCount, rCoordinate, basis, f4VisibleArea, iMaximumUnits, iUnitCount, bTruncated, units, [&](int64_t i) -> nlohmann::json
			{
				return
				{
					{"globalId", rOmnisPost.pGlobalIds[i].iValue},
					{"armor", rOmnisPost.pfArmors[i]},
					{"shield", rOmnisPost.pfShields[i]},
					{"alignment", rOmnisPost.pAlignments[i].uiValue},
					{"flags", OmniFlagNames(rOmnisPost.pLifecycleFlags[i], rOmnisPost.pFlags[i])},
				};
			});
		}

		if (bIncludeSpaceships)
		{
			const SpaceshipsInterpolate& rShipsInterpolate = *rFrame.interpolate.pSpaceships;
			const SpaceshipsPostRender& rShipsPost = *rFrame.postRender.pSpaceships;
			AppendVisibleUnits("spaceship", rShipsInterpolate.pVecPositions, rShipsPost.iCount, rCoordinate, basis, f4VisibleArea, iMaximumUnits, iUnitCount, bTruncated, units, [&](int64_t i) -> nlohmann::json
			{
				return
				{
					{"health", rShipsPost.pfHealths[i]},
					{"alignment", rShipsPost.pAlignments[i].uiValue},
					{"flags", SpaceshipFlagNames(rShipsPost.pLifecycleFlags[i], rShipsPost.pFlags[i])},
				};
			});
		}

		// Blasters stay last so a cell over the unit budget truncates these rows instead of displacing the others.
		if (bIncludeBlasters)
		{
			const BlastersInterpolate& rBlastersInterpolate = *rFrame.interpolate.pBlasters;
			const BlastersPostRender& rBlastersPost = *rFrame.postRender.pBlasters;
			AppendVisibleUnits("blaster", rBlastersInterpolate.pVecPositions, rBlastersPost.iCount, rCoordinate, basis, f4VisibleArea, iMaximumUnits, iUnitCount, bTruncated, units, [&](int64_t i) -> nlohmann::json
			{
				return
				{
					{"alignment", rBlastersPost.pAlignments[i].uiValue},
					{"windTrailIntensity", rBlastersInterpolate.pfWindTrailIntensities[i]},
					{"windTrailWidth", rBlastersInterpolate.pfWindTrailWidths[i]},
					{"windTrailLengthMultiplier", rBlastersInterpolate.pfWindTrailLengthMultipliers[i]},
				};
			});
		}
	}

	rResult["units"] = std::move(units);
	rResult["counts"] =
	{
		{"omnis", iOmniTotal},
		{"spaceships", iSpaceshipTotal},
		{"missiles", iMissileTotal},
		{"blasters", iBlasterTotal},
	};
	rResult["truncated"] = bTruncated;
}

} // namespace game

#endif // defined(BT_CLIENT)
