#include "Agent/AgentCommands.h"
#include "Agent/Commands/AgentCommandsAudioStreaming.h"
#include "Agent/Commands/ClientDesyncProbe.h"
#include "Agent/Commands/ClientFullStateFixture.h"
#include "Agent/Commands/ClientPacketFaultFixture.h"
#include "Agent/Commands/ClientSubscriptionFixtures.h"

#if defined(BT_CLIENT)

#include "Agent/AgentScene.h"
#include "Game.h"
#include "Network/Client/Client.h"
#include "Network/Client/ClientSession.h"
#include "Network/Client/ClientSessionRuntime.h"

namespace game
{

namespace
{

// Keeps agent-supplied cells far from the int32 extremes that server adjacency deltas and the client 3x3
// neighbour ring compute from them.
constexpr int32_t kiAgentGridCoordLimit = 1'000'000;

} // namespace

int32_t ClientGridCoordValue(const nlohmann::json& rValue, std::string_view command)
{
	if (!rValue.is_number_integer())
	{
		throw std::runtime_error(std::format("{} 'coord' must be an array of 2 integers", command));
	}

	if (rValue.is_number_unsigned())
	{
		uint64_t uiValue = rValue.get<uint64_t>();
		if (uiValue > static_cast<uint64_t>(kiAgentGridCoordLimit))
		{
			throw std::runtime_error(std::format("{} 'coord' values must be within +/-1000000", command));
		}
		return static_cast<int32_t>(uiValue);
	}

	int64_t iValue = rValue.get<int64_t>();
	if (iValue < -static_cast<int64_t>(kiAgentGridCoordLimit) || iValue > static_cast<int64_t>(kiAgentGridCoordLimit))
	{
		throw std::runtime_error(std::format("{} 'coord' values must be within +/-1000000", command));
	}
	return static_cast<int32_t>(iValue);
}

namespace
{

// set_client_grid_coord: move the client's grid cell so automation can drive the cross-cell subscribe and
// full-state adoption path. Schema: {"coord":[x,y]}.
void CommandSetClientGridCoord(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	// Heap: validation errors and JSON result
	ScopedSuppressAllocationTracking suppress;

	if (!rParameters.is_object())
	{
		throw std::runtime_error("set_client_grid_coord params must be an object");
	}
	if (rParameters.size() != 1 || !rParameters.contains("coord"))
	{
		throw std::runtime_error("set_client_grid_coord accepts only 'coord'");
	}

	const nlohmann::json& rCoord = rParameters.at("coord");
	if (!rCoord.is_array() || rCoord.size() != 2)
	{
		throw std::runtime_error("set_client_grid_coord 'coord' must be an array of 2 integers");
	}

	const engine::GridCoord coord {ClientGridCoordValue(rCoord.at(0), "set_client_grid_coord"), ClientGridCoordValue(rCoord.at(1), "set_client_grid_coord")};

	// Before player assignment the subscription policy falls back to origin, so the requested cell would be dropped.
	if (gpGame == nullptr)
	{
		throw std::runtime_error("set_client_grid_coord requires a connected live client/server session with an assigned player");
	}
	if (gpClientSession == nullptr)
	{
		throw std::runtime_error("set_client_grid_coord requires a connected live client/server session with an assigned player");
	}
	if (gpClientSession->mpRuntime->mpClient == nullptr)
	{
		throw std::runtime_error("set_client_grid_coord requires a connected live client/server session with an assigned player");
	}
	if (!(gpClientSession->mpRuntime->mpClient->mStateFlags & engine::Client::ClientStateFlags::kConnected))
	{
		throw std::runtime_error("set_client_grid_coord requires a connected live client/server session with an assigned player");
	}
	if (gpClientSession->mpRuntime->mpClient->mpServerPeer == nullptr)
	{
		throw std::runtime_error("set_client_grid_coord requires a connected live client/server session with an assigned player");
	}
	if (gpGame->InMainMenu())
	{
		throw std::runtime_error("set_client_grid_coord requires a connected live client/server session with an assigned player");
	}
	if (!gpGame->ClientPlayerId().IsValid())
	{
		throw std::runtime_error("set_client_grid_coord requires a connected live client/server session with an assigned player");
	}

	// Order is an invariant: the setter clears the visible-neighbour cache keyed by the old cell.
	gpGame->SetClientGridCoord(coord);
	gpClientSession->UpdateDesiredCoords(SubscriptionChangeReason::kPollTick);

	rResult["clientGridCoord"] = {coord.x, coord.y};
}

} // namespace

bool ExecuteAgentCommandClient(std::string_view cmd, const nlohmann::json& rParams, nlohmann::json& rResult)
{
	if (cmd == "audio_streaming_fixture")
	{
		CommandAudioStreamingFixture(rParams, rResult);
		return true;
	}
	if (cmd == "client_subscribe_accept_fixture")
	{
		CommandClientSubscribeAcceptFixture(rParams, rResult);
		return true;
	}
	if (cmd == "client_stale_update_fixture")
	{
		CommandClientStaleUpdateFixture(rParams, rResult);
		return true;
	}
	if (cmd == "client_cancelled_subscription_fixture")
	{
		CommandClientCancelledSubscriptionFixture(rParams, rResult);
		return true;
	}
	if (cmd == "client_packet_fault_fixture")
	{
		CommandClientPacketFaultFixture(rParams, rResult);
		return true;
	}
	if (cmd == "client_full_state_fixture")
	{
		CommandClientFullStateFixture(rParams, rResult);
		return true;
	}
	if (cmd == "describe_scene")
	{
		CommandDescribeScene(rParams, rResult);
		return true;
	}
	if (cmd == "desync_probe")
	{
		CommandDesyncProbe(rParams, rResult);
		return true;
	}
	if (cmd == "set_client_grid_coord")
	{
		CommandSetClientGridCoord(rParams, rResult);
		return true;
	}
	return false;
}

} // namespace game

#endif // defined(BT_CLIENT)
