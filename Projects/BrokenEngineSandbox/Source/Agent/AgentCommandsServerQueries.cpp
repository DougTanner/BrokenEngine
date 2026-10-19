#include "Agent/AgentCommandsServerQueries.h"

#if defined(BT_SERVER)

#include "Frame/Collections/Battleships/Battleships.h"
#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Fighters/Fighters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Omnis/Omnis.h"
#include "Game.h"

namespace game
{

static int64_t OptionalCount(const nlohmann::json& rParameters, std::string_view key, int64_t iDefault)
{
	if (!rParameters.contains(key))
	{
		return iDefault;
	}
	int64_t iCount = rParameters.at(std::string(key)).get<int64_t>();
	if (iCount < 0)
	{
		std::string message("'");
		message.append(key);
		message.append("' must be a non-negative integer");
		throw std::runtime_error(message);
	}
	return iCount;
}

static nlohmann::json Vector3ToJson(XMVECTOR vec)
{
	return nlohmann::json::array({XMVectorGetX(vec), XMVectorGetY(vec), XMVectorGetZ(vec)});
}

// Bound a non-negative offset/limit window to the collection's live count → [iBegin, iEnd).
static void ClampWindow(int64_t iTotal, int64_t iOffset, int64_t iLimit, int64_t& riBegin, int64_t& riEnd)
{
	riBegin = std::min(iOffset, iTotal);
	riEnd = iLimit >= iTotal - riBegin ? iTotal : riBegin + iLimit;
}

// Every row of one of these reports belongs to the cell the request named, so the position is the local
// component alone; the caller's own 'coord' parameter identifies the cell.
template <typename INTERPOLATE, typename POST_RENDER>
static nlohmann::json ExtractShips(const INTERPOLATE& rInterpolate, const POST_RENDER& rPost, int64_t iOffset, int64_t iLimit)
{
	int64_t iBegin = 0;
	int64_t iEnd = 0;
	ClampWindow(rPost.iCount, iOffset, iLimit, iBegin, iEnd);
	nlohmann::json items = nlohmann::json::array();
	for (int64_t i = iBegin; i < iEnd; ++i)
	{
		nlohmann::json item =
		{
			{"index", i},
			{"uuid", rPost.pIds[i].uuid.iValue},
			{"globalId", rPost.pGlobalIds[i].iValue},
			{"controller", EnumToken(rPost.pController[i])},
			{"orders", std::to_underlying(rPost.pOrders[i].meFlags)},
			{"flags", std::to_underlying(rPost.pFlags[i].meFlags)},
			{"lifecycleFlags", std::to_underlying(rPost.pLifecycleFlags[i].meFlags)},
			{"alignment", rPost.pAlignments[i].uiValue},
			{"local", engine::AgentLocalPositionJson(rInterpolate.pVecPositions[i])},
			{"dir", Vector3ToJson(rInterpolate.pVecDirections[i])},
			{"vel", Vector3ToJson(rPost.pVecVelocities[i])},
			{"arrivalGrace", rPost.pArrivalGracePeriods[i].count()},
			{"flagshipGlobalId", rPost.pFlagshipGlobalIds[i].iValue},
			{"registryId", rInterpolate.pRegistryIds[i].uuid.iValue},
		};
		if constexpr (requires { rPost.pfArmors; })
		{
			item["armor"] = rPost.pfArmors[i];
			item["shield"] = rPost.pfShields[i];
		}
		else
		{
			item["health"] = rPost.pfHealths[i];
		}
		items.push_back(std::move(item));
	}
	return items;
}

static nlohmann::json ExtractMissiles(const Frame& rFrame, int64_t iOffset, int64_t iLimit)
{
	const MissilesInterpolate& rInterpolate = *rFrame.interpolate.pMissiles;
	const MissilesPostRender& rPost = *rFrame.postRender.pMissiles;
	int64_t iBegin = 0;
	int64_t iEnd = 0;
	ClampWindow(rPost.iCount, iOffset, iLimit, iBegin, iEnd);
	nlohmann::json items = nlohmann::json::array();
	for (int64_t i = iBegin; i < iEnd; ++i)
	{
		items.push_back(
		{
			{"index", i},
			{"local", engine::AgentLocalPositionJson(rInterpolate.pVecPositions[i])},
			{"dir", Vector3ToJson(rInterpolate.pVecDirections[i])},
			{"deltaRotation", rPost.pfDeltaRotations[i]},
			{"deltaRotationDelay", rPost.pDeltaRotationDelays[i].count()},
			{"alignment", rPost.pAlignments[i].uiValue},
			{"registryTargetId", rPost.puiRegistryTargets[i].uuid.iValue},
		});
	}
	return items;
}

static nlohmann::json ExtractBlasters(const Frame& rFrame, int64_t iOffset, int64_t iLimit)
{
	const BlastersInterpolate& rInterpolate = *rFrame.interpolate.pBlasters;
	const BlastersPostRender& rPost = *rFrame.postRender.pBlasters;
	int64_t iBegin = 0;
	int64_t iEnd = 0;
	ClampWindow(rPost.iCount, iOffset, iLimit, iBegin, iEnd);
	nlohmann::json items = nlohmann::json::array();
	for (int64_t i = iBegin; i < iEnd; ++i)
	{
		items.push_back(
		{
			{"index", i},
			{"local", engine::AgentLocalPositionJson(rInterpolate.pVecPositions[i])},
			{"dir", Vector3ToJson(rInterpolate.pVecDirections[i])},
			{"alignment", rPost.pAlignments[i].uiValue},
		});
	}
	return items;
}

Frame& QueryFrame(const nlohmann::json& rParameters)
{
	engine::GridCoord coordinate = engine::CoordinateFromParameter(rParameters);
	auto it = gpGame->mCells.find(coordinate);
	if (it == gpGame->mCells.end())
	{
		throw std::runtime_error("coord has no loaded frame");
	}
	return (*it->second.pCurrent);
}

void CommandQueryFrame(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	const Frame& rFrame = QueryFrame(rParameters);
	rResult["fighters"] = {{"count", rFrame.postRender.pFighters->iCount}};
	rResult["omnis"] = {{"count", rFrame.postRender.pOmnis->iCount}};
	rResult["battleships"] = {{"count", rFrame.postRender.pBattleships->iCount}};
	rResult["missiles"] = {{"count", rFrame.postRender.pMissiles->iCount}};
	rResult["blasters"] = {{"count", rFrame.postRender.pBlasters->iCount}};
}

void CommandQueryCollection(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	const Frame& rFrame = QueryFrame(rParameters);
	if (!rParameters.contains("collection") || !rParameters.at("collection").is_string())
	{
		throw std::runtime_error("query_collection requires string 'collection'");
	}
	std::string collection = rParameters.at("collection").get<std::string>();
	int64_t iOffset = OptionalCount(rParameters, "offset", 0);
	int64_t iLimit = OptionalCount(rParameters, "limit", 256);

	int64_t iTotal = 0;
	nlohmann::json items;
	if (collection == "fighters")
	{
		iTotal = rFrame.postRender.pFighters->iCount;
		items = ExtractShips(*rFrame.interpolate.pFighters, *rFrame.postRender.pFighters, iOffset, iLimit);
	}
	else if (collection == "omnis")
	{
		iTotal = rFrame.postRender.pOmnis->iCount;
		items = ExtractShips(*rFrame.interpolate.pOmnis, *rFrame.postRender.pOmnis, iOffset, iLimit);
	}
	else if (collection == "battleships")
	{
		iTotal = rFrame.postRender.pBattleships->iCount;
		items = ExtractShips(*rFrame.interpolate.pBattleships, *rFrame.postRender.pBattleships, iOffset, iLimit);
	}
	else if (collection == "missiles")
	{
		iTotal = rFrame.postRender.pMissiles->iCount;
		items = ExtractMissiles(rFrame, iOffset, iLimit);
	}
	else if (collection == "blasters")
	{
		iTotal = rFrame.postRender.pBlasters->iCount;
		items = ExtractBlasters(rFrame, iOffset, iLimit);
	}
	else
	{
		throw std::runtime_error("'collection' must be fighters|omnis|battleships|missiles|blasters");
	}
	rResult["total"] = iTotal;
	rResult["items"] = std::move(items);
}

} // namespace game

#endif // defined(BT_SERVER)
