#include "Agent/AgentCommands.h"

#include "Agent/Commands/AgentCommandsAudioStreaming.h"
#include "Agent/Commands/ClientDesyncProbe.h"
#include "Agent/Commands/ClientFullStateHarnessRig.h"
#include "Agent/Commands/ClientPacketFaultHarnessRig.h"
#include "Agent/Commands/ClientSubscriptionHarnessRigs.h"

#if defined(BT_CLIENT)

#include "Network/Client/Client.h"
#include "Network/Client/ClientSessionRuntime.h"

#include "Agent/AgentScene.h"
#include "Network/Client/ClientSession.h"
#include "Game.h"

namespace game
{

int64_t ClientGridCoordinateValue(const nlohmann::json& rValue, std::string_view command)
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
		if (!std::in_range<int32_t>(uiValue))
		{
			throw std::runtime_error(std::format("{} 'coord' values must fit in a signed 32-bit integer", command));
		}
		return static_cast<int64_t>(uiValue);
	}

	int64_t iValue = rValue.get<int64_t>();
	if (!std::in_range<int32_t>(iValue))
	{
		throw std::runtime_error(std::format("{} 'coord' values must fit in a signed 32-bit integer", command));
	}
	return iValue;
}

engine::Client& RequireHarnessRigClient(std::string_view command)
{
	if (gpClientSession->mpRuntime->mpClient == nullptr)
	{
		throw std::runtime_error(std::format("{} requires an accepted connected client", command));
	}
	engine::Client& rClient = *gpClientSession->mpRuntime->mpClient;
	if (!(rClient.mStateFlags & engine::Client::ClientStateFlags::kConnected))
	{
		throw std::runtime_error(std::format("{} requires an accepted connected client", command));
	}
	if (!(rClient.mStateFlags & engine::Client::ClientStateFlags::kConnectionAccepted))
	{
		throw std::runtime_error(std::format("{} requires an accepted connected client", command));
	}
	return rClient;
}

// set_client_grid_coord: move the client's grid cell and pin it against every game writer until
// release_client_grid_coord or Game::Reset, so automation can drive the cross-cell subscribe and full-state
// adoption path. Schema: {"coord":[x,y]}.
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

	engine::GridCoord coordinate {.iX = static_cast<int32_t>(ClientGridCoordinateValue(rCoordinate.at(0), "set_client_grid_coord")), .iY = static_cast<int32_t>(ClientGridCoordinateValue(rCoordinate.at(1), "set_client_grid_coord"))};

	// Before player assignment the subscription policy falls back to origin, so the requested cell would be dropped.
	RequireHarnessRigClient("set_client_grid_coord");
	if ((gpGame->mGameFlags & engine::GameFlags::kMainMenu))
	{
		throw std::runtime_error("set_client_grid_coord requires a connected live client/server session with an assigned player");
	}
	if (!(gpGame->ClientPlayerIdentifier().iValue != 0))
	{
		throw std::runtime_error("set_client_grid_coord requires a connected live client/server session with an assigned player");
	}

	// Order is an invariant: the setter clears the visible-neighbour cache keyed by the old cell.
	gpGame->PinClientGridCoordinate(coordinate);
	gpClientSession->UpdateDesiredCoordinates(SubscriptionChangeReason::kPollTick);

	rResult["clientGridCoord"] = {coordinate.iX, coordinate.iY};
}

// release_client_grid_coord: clear the set_client_grid_coord pin; the cell stays put until the next game writer
// moves it. Schema: {}.
static void CommandReleaseClientGridCoordinate(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	// Heap: validation errors and JSON result
	ScopedSuppressAllocationTracking suppress;

	if (!rParameters.is_object())
	{
		throw std::runtime_error("release_client_grid_coord accepts no params");
	}
	if (!rParameters.empty())
	{
		throw std::runtime_error("release_client_grid_coord accepts no params");
	}

	gpGame->mbClientGridCoordinatePinned = false;

	rResult["clientGridCoord"] = {gpGame->mClientGridCoordinate.iX, gpGame->mClientGridCoordinate.iY};
}

bool ExecuteAgentCommandClient(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (command == "audio_streaming_harness_rig")
	{
		CommandAudioStreamingHarnessRig(rParameters, rResult);
		return true;
	}
	if (command == "client_subscribe_accept_harness_rig")
	{
		CommandClientSubscribeAcceptHarnessRig(rParameters, rResult);
		return true;
	}
	if (command == "client_stale_update_harness_rig")
	{
		CommandClientStaleUpdateHarnessRig(rParameters, rResult);
		return true;
	}
	if (command == "client_cancelled_subscription_harness_rig")
	{
		CommandClientCancelledSubscriptionHarnessRig(rParameters, rResult);
		return true;
	}
	if (command == "client_packet_fault_harness_rig")
	{
		CommandClientPacketFaultHarnessRig(rParameters, rResult);
		return true;
	}
	if (command == "client_full_state_harness_rig")
	{
		CommandClientFullStateHarnessRig(rParameters, rResult);
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
	if (command == "release_client_grid_coord")
	{
		CommandReleaseClientGridCoordinate(rParameters, rResult);
		return true;
	}
	return false;
}

} // namespace game

#endif // defined(BT_CLIENT)
