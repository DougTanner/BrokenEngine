#include "Agent/AgentCommandsServerQueries.h"

#if defined(BT_SERVER)

#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Players/Players.h"
#include "Frame/Collections/Spaceships/Spaceships.h"
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
static nlohmann::json ExtractPlayers(const Frame& rFrame, int64_t iOffset, int64_t iLimit)
{
	const PlayersInterpolate& rInterpolate = *rFrame.interpolate.pPlayers;
	const PlayersPostRender& rPost = *rFrame.postRender.pPlayers;
	int64_t iBegin = 0;
	int64_t iEnd = 0;
	ClampWindow(rPost.iCount, iOffset, iLimit, iBegin, iEnd);
	nlohmann::json items = nlohmann::json::array();
	for (int64_t i = iBegin; i < iEnd; ++i)
	{
		items.push_back(
		{
			{"index", i},
			{"uuid", rPost.pIds[i].uuid.iValue},
			{"globalId", rPost.pGlobalPlayerIds[i].iValue},
			{"local", engine::AgentLocalPositionJson(rInterpolate.pVecPositions[i])},
			{"dir", Vector3ToJson(rInterpolate.pVecDirections[i])},
			{"armor", rPost.pfArmors[i]},
			{"shield", rPost.pfShields[i]},
			{"flags", std::to_underlying(rPost.pFlags[i].meFlags)},
			{"alignment", rPost.pAlignments[i].uiValue},
		});
	}
	return items;
}

static nlohmann::json ExtractSpaceships(const Frame& rFrame, int64_t iOffset, int64_t iLimit)
{
	const SpaceshipsInterpolate& rInterpolate = *rFrame.interpolate.pSpaceships;
	const SpaceshipsPostRender& rPost = *rFrame.postRender.pSpaceships;
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
			{"health", rPost.pfHealths[i]},
			{"deltaRotation", rInterpolate.pfDeltaRotations[i]},
			{"alignment", rPost.pAlignments[i].uiValue},
			{"registryId", rInterpolate.puiRegistryIds[i].uuid.iValue},
		});
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
			{"deltaRotationDelay", rPost.pfDeltaRotationDelays[i]},
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
	engine::GridCoord coordinate = CoordinateFromParameter(rParameters);
	auto it = gpGame->mCoordinateFrames.find(coordinate);
	if (it == gpGame->mCoordinateFrames.end())
	{
		throw std::runtime_error("coord has no loaded frame");
	}
	// Newly activated coordinates can have no current frame until the next post-tick SwapFrames; paused simulation does not swap.
	// The coordinate comes from agent input, so an unavailable frame raises a command error.
	if (it->second.pCurrent == nullptr)
	{
		throw std::runtime_error("coord frame not ready");
	}
	return (*gpGame->mCoordinateFrames.at(coordinate).pCurrent);
}

void CommandQueryFrame(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	const Frame& rFrame = QueryFrame(rParameters);
	rResult["players"] = {{"count", rFrame.postRender.pPlayers->iCount}};
	rResult["spaceships"] = {{"count", rFrame.postRender.pSpaceships->iCount}};
	rResult["missiles"] = {{"count", rFrame.postRender.pMissiles->iCount}};
	rResult["blasters"] = {{"count", rFrame.postRender.pBlasters->iCount}};
}

void CommandQueryPlayers(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	const Frame& rFrame = QueryFrame(rParameters);
	int64_t iOffset = OptionalCount(rParameters, "offset", 0);
	int64_t iLimit = OptionalCount(rParameters, "limit", 256);
	rResult["total"] = rFrame.postRender.pPlayers->iCount;
	rResult["players"] = ExtractPlayers(rFrame, iOffset, iLimit);
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
	if (collection == "spaceships")
	{
		iTotal = rFrame.postRender.pSpaceships->iCount;
		items = ExtractSpaceships(rFrame, iOffset, iLimit);
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
		throw std::runtime_error("'collection' must be spaceships|missiles|blasters");
	}
	rResult["total"] = iTotal;
	rResult["items"] = std::move(items);
}

} // namespace game

#endif // defined(BT_SERVER)
