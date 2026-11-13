#include "Agent/AgentCommands.h"

#if defined(BT_CLIENT)

#include "Agent/AgentScene.h"
#include "Game.h"

namespace game
{

static void CommandRevealUi(const nlohmann::json& rParameters)
{
	if (!rParameters.contains("window") || rParameters.at("window") != "FleetPanel")
	{
		throw std::runtime_error("reveal_ui 'window' must be 'FleetPanel'");
	}
	if (!rParameters.contains("reveal") || !rParameters.at("reveal").is_boolean())
	{
		throw std::runtime_error("reveal_ui 'reveal' must be a bool");
	}
	gpGame->mFleetSelection.mbRevealFleetPanel = rParameters.at("reveal").get<bool>();
}

bool ExecuteAgentCommandClient(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (command == "describe_scene")
	{
		CommandDescribeScene(rParameters, rResult);
		return true;
	}
	if (command == "reveal_ui")
	{
		CommandRevealUi(rParameters);
		return true;
	}
	return false;
}

} // namespace game

#endif // defined(BT_CLIENT)
