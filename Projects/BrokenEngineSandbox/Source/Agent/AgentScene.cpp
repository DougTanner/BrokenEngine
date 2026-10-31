#include "Agent/AgentScene.h"

#if defined(BT_CLIENT)

#include "Agent/Commands/DescribeScene.h"
#include "Ui/WrapperBase.h"

#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Ships/ShipQueries.h"
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

// Boolean ShipOrders and ShipFlags only — the packed nav-direction (bits 8-11) and nav-waypoint (bits 12-13) ranges
// are excluded; iterating them as single bits would emit meaningless names.
static nlohmann::json ShipFlagNames(engine::LifecycleFlags_t lifecycleFlags, ShipOrders_t orders, ShipFlags_t flags)
{
	nlohmann::json names = nlohmann::json::array();
	AppendLifecycleFlagNames(lifecycleFlags, names);
	if (orders & ShipOrders::kIsFlagship)
	{
		names.push_back("kIsFlagship");
	}
	if (flags & ShipFlags::kVeerOff)
	{
		names.push_back("kVeerOff");
	}
	if (flags & ShipFlags::kHealing)
	{
		names.push_back("kHealing");
	}
	if (flags & ShipFlags::kFireBlaster)
	{
		names.push_back("kFireBlaster");
	}
	if (flags & ShipFlags::kBlasterSpawnLeft)
	{
		names.push_back("kBlasterSpawnLeft");
	}
	if (flags & ShipFlags::kFlee)
	{
		names.push_back("kFlee");
	}
	if (flags & ShipFlags::kReturnToIslandCenter)
	{
		names.push_back("kReturnToIslandCenter");
	}
	return names;
}

// Indexed by ShipType.
constexpr std::array<std::string_view, std::to_underlying(ShipType::kCount)> kShipUnitTypes {"fighter", "omni", "battleship"};

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
	std::array<bool, std::to_underlying(ShipType::kCount)> includeShips {};
	includeShips.fill(!bHasUnitTypes);
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
			auto shipIt = std::ranges::find(kShipUnitTypes, unitType);
			if (shipIt != kShipUnitTypes.end())
			{
				includeShips[static_cast<size_t>(shipIt - kShipUnitTypes.begin())] = true;
			}
			else if (unitType == "blaster")
			{
				bIncludeBlasters = true;
			}
			else
			{
				throw std::runtime_error("describe_scene 'unitTypes' entries must be 'fighter', 'omni', 'battleship', or 'blaster'");
			}
		}
	}

	engine::CommandDescribeScene(rParameters, rResult);

	XMFLOAT4 f4VisibleArea = engine::gpCamera->mf4RenderVisibleArea;
	rResult["fleets"] = BuildFleets();

	nlohmann::json units = nlohmann::json::array();
	std::array<int64_t, std::to_underlying(ShipType::kCount)> shipTotals {};
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

		iMissileTotal += rFrame.postRender.pMissiles->iCount;
		iBlasterTotal += rFrame.postRender.pBlasters->iCount;
		ForEachShipType(rFrame, [&](ShipType eType, const auto& rShipInterpolate, const auto& rShipPost)
		{
			shipTotals[std::to_underlying(eType)] += rShipPost.iCount;
			if (!bIncludeUnits)
			{
				return;
			}

			if (!includeShips[std::to_underlying(eType)])
			{
				return;
			}
			AppendVisibleUnits(kShipUnitTypes[std::to_underlying(eType)], rShipInterpolate.pVecPositions, rShipPost.iCount, rCoordinate, basis, f4VisibleArea, iMaximumUnits, iUnitCount, bTruncated, units, [&](int64_t i) -> nlohmann::json
			{
				nlohmann::json row =
				{
					{"globalId", rShipPost.pGlobalIds[i].iValue},
					{"alignment", rShipPost.pAlignments[i].uiValue},
					{"flags", ShipFlagNames(rShipPost.pLifecycleFlags[i], rShipPost.pOrders[i], rShipPost.pFlags[i])},
				};
				if constexpr (requires { rShipPost.pfArmors; })
				{
					row["armor"] = rShipPost.pfArmors[i];
					row["shield"] = rShipPost.pfShields[i];
				}
				else
				{
					row["health"] = rShipPost.pfHealths[i];
				}
				return row;
			});
		});

		if (!bIncludeUnits)
		{
			continue;
		}

		// Blasters stay last so a cell over the unit budget truncates these rows instead of displacing the others.
		if (bIncludeBlasters)
		{
			const BlastersInterpolate& rBlastersInterpolate = *rFrame.interpolate.pBlasters;
			const BlastersPostRender& rBlastersPost = *rFrame.postRender.pBlasters;
			AppendVisibleUnits("blaster", rBlastersInterpolate.pVecPositions, rBlastersPost.iCount, rCoordinate, basis, f4VisibleArea, iMaximumUnits, iUnitCount, bTruncated, units, [&](int64_t i) -> nlohmann::json
			{
				const BlastersType& rType = BlastersInterpolate::sTypes.at(rBlastersInterpolate.puiTypeIndices[i]);
				return
				{
					{"alignment", rBlastersPost.pAlignments[i].uiValue},
					{"windTrailIntensity", rType.pWindTrailIntensityWrapper->mfCurrent},
					{"windTrailWidth", rType.pWindTrailWidthWrapper->mfCurrent},
					{"windTrailLengthMultiplier", rType.pWindTrailLengthMultiplierWrapper->mfCurrent},
				};
			});
		}
	}

	rResult["units"] = std::move(units);
	rResult["counts"] =
	{
		{"fighters", shipTotals[std::to_underlying(ShipType::kFighter)]},
		{"omnis", shipTotals[std::to_underlying(ShipType::kOmni)]},
		{"battleships", shipTotals[std::to_underlying(ShipType::kBattleship)]},
		{"missiles", iMissileTotal},
		{"blasters", iBlasterTotal},
	};
	rResult["truncated"] = bTruncated;
}

} // namespace game

#endif // defined(BT_CLIENT)
