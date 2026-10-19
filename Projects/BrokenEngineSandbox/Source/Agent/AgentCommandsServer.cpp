#include "Agent/AgentCommands.h"

#if defined(BT_SERVER)

#include "Network/Server/ServerTransferManager.h"

#include "Agent/Commands/ServerFrameEdit.h"
#include "Agent/Commands/ServerSimulationHarnessRigs.h"
#include "Agent/AgentCommandsServerQueries.h"
#include "Network/Server/ServerFleetManager.h"
#include "Network/Server/ServerSession.h"

namespace game
{

static void CommandStatus(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	engine::CommandStatus(rParameters, rResult);
	rResult["pendingFlagshipUpdateCount"] = std::ssize(gpServerSession->mpFleetManager->mNavigation.mPendingFlagshipUpdates);
	rResult["pendingAgentStatusChangeCount"] = CountPendingAgentStatusChanges(*gpServerSession) + gpServerSession->mpTransferManager->PendingSpawnCount();
}

static void CommandLoad(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	engine::CommandLoad(rParameters, rResult);
	rResult["pendingFlagshipUpdateCount"] = std::ssize(gpServerSession->mpFleetManager->mNavigation.mPendingFlagshipUpdates);
}

bool ExecuteAgentCommandServer(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (command == "status")
	{
		CommandStatus(rParameters, rResult);
		return true;
	}
	if (command == "load")
	{
		CommandLoad(rParameters, rResult);
		return true;
	}
	if (command == "edit_frame")
	{
		CommandEditFrame(rParameters, rResult);
		return true;
	}
	if (command == "read_frame")
	{
		CommandReadFrame(rParameters, rResult);
		return true;
	}
	if (ExecuteServerSimulationHarnessRigCommand(command, rParameters, rResult))
	{
		return true;
	}
	if (command == "query_frame")
	{
		CommandQueryFrame(rParameters, rResult);
		return true;
	}
	if (command == "query_collection")
	{
		CommandQueryCollection(rParameters, rResult);
		return true;
	}
	return false;
}

} // namespace game

#endif // defined(BT_SERVER)
