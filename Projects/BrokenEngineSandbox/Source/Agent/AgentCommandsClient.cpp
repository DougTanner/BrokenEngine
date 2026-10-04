#include "Agent/AgentCommands.h"

#include "Agent/Commands/AgentCommandsAudioStreaming.h"
#include "Agent/Commands/ClientDesyncProbe.h"
#include "Agent/Commands/ClientFullStateFixture.h"
#include "Agent/Commands/ClientPacketFaultFixture.h"
#include "Agent/Commands/ClientSubscriptionFixtures.h"

#if defined(BT_CLIENT)

#include "Network/Client/Client.h"
#include "Network/Client/ClientSessionRuntime.h"

#include "Agent/AgentScene.h"
#include "Network/Client/ClientSession.h"
#include "Game.h"

namespace game
{

int32_t ClientGridCoordinateValue(const nlohmann::json& rValue, std::string_view command)
{
	if (!rValue.is_number_integer())
	{
		throw std::runtime_error(std::format("{} 'coord' must be an array of 2 integers", command));
	}

	// Every signed-int32 coordinate identifies a usable cell, so the only rejected integers are the ones a
	// GridCoord cannot hold. The 3x3 neighbour ring and server adjacency deltas use checked addition
	// (engine::TryAddGridCoordinate), which omits the neighbours a numeric-edge cell cannot represent.
	if (rValue.is_number_unsigned())
	{
		uint64_t uiValue = rValue.get<uint64_t>();
		if (uiValue > static_cast<uint64_t>(std::numeric_limits<int32_t>::max()))
		{
			throw std::runtime_error(std::format("{} 'coord' values must fit in a signed 32-bit integer", command));
		}
		return static_cast<int32_t>(uiValue);
	}

	int64_t iValue = rValue.get<int64_t>();
	if (iValue < static_cast<int64_t>(std::numeric_limits<int32_t>::min()) || iValue > static_cast<int64_t>(std::numeric_limits<int32_t>::max()))
	{
		throw std::runtime_error(std::format("{} 'coord' values must fit in a signed 32-bit integer", command));
	}
	return static_cast<int32_t>(iValue);
}

// set_client_grid_coord: move the client's grid cell so automation can drive the cross-cell subscribe and
// full-state adoption path. Schema: {"coord":[x,y]}.
static void CommandSetClientGridCoordinate(const nlohmann::json& rParameters, nlohmann::json& rResult)
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

	const nlohmann::json& rCoordinate = rParameters.at("coord");
	if (!rCoordinate.is_array() || rCoordinate.size() != 2)
	{
		throw std::runtime_error("set_client_grid_coord 'coord' must be an array of 2 integers");
	}

	engine::GridCoord coordinate {.iX = ClientGridCoordinateValue(rCoordinate.at(0), "set_client_grid_coord"), .iY = ClientGridCoordinateValue(rCoordinate.at(1), "set_client_grid_coord")};

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
	if ((gpGame->mGameFlags & engine::GameFlags::kMainMenu))
	{
		throw std::runtime_error("set_client_grid_coord requires a connected live client/server session with an assigned player");
	}
	if (!(gpGame->ClientPlayerIdentifier().iValue != 0))
	{
		throw std::runtime_error("set_client_grid_coord requires a connected live client/server session with an assigned player");
	}

	// Order is an invariant: the setter clears the visible-neighbour cache keyed by the old cell.
	gpGame->SetClientGridCoordinate(coordinate);
	gpClientSession->UpdateDesiredCoordinates(SubscriptionChangeReason::kPollTick);

	rResult["clientGridCoord"] = {coordinate.iX, coordinate.iY};
}

bool ExecuteAgentCommandClient(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (command == "audio_streaming_fixture")
	{
		CommandAudioStreamingFixture(rParameters, rResult);
		return true;
	}
	if (command == "client_subscribe_accept_fixture")
	{
		CommandClientSubscribeAcceptFixture(rParameters, rResult);
		return true;
	}
	if (command == "client_stale_update_fixture")
	{
		CommandClientStaleUpdateFixture(rParameters, rResult);
		return true;
	}
	if (command == "client_cancelled_subscription_fixture")
	{
		CommandClientCancelledSubscriptionFixture(rParameters, rResult);
		return true;
	}
	if (command == "client_packet_fault_fixture")
	{
		CommandClientPacketFaultFixture(rParameters, rResult);
		return true;
	}
	if (command == "client_full_state_fixture")
	{
		CommandClientFullStateFixture(rParameters, rResult);
		return true;
	}
	if (command == "describe_scene")
	{
		CommandDescribeScene(rParameters, rResult);
		return true;
	}
	if (command == "desync_probe")
	{
		CommandDesynchronizationProbe(rParameters, rResult);
		return true;
	}
	if (command == "set_client_grid_coord")
	{
		CommandSetClientGridCoordinate(rParameters, rResult);
		return true;
	}
	return false;
}

} // namespace game

#endif // defined(BT_CLIENT)
