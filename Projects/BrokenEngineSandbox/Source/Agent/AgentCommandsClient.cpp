#include "Agent/AgentCommands.h"

#if defined(BT_CLIENT)

#include "Agent/AgentScene.h"

namespace game
{

bool ExecuteAgentCommandClient(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (command == "describe_scene")
	{
		CommandDescribeScene(rParameters, rResult);
		return true;
	}
	return false;
}

} // namespace game

#endif // defined(BT_CLIENT)
