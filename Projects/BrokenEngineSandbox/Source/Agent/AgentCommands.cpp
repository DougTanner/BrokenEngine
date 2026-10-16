#include "Agent/AgentCommands.h"

#include "Game.h"

#if defined(BT_CLIENT)
#include "Profile/ProfileManager.h"
#endif

namespace game
{

void ExecuteAgentCommand(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (engine::ExecuteSharedAgentCommand(command, rParameters, rResult, gpGame != nullptr ? gpGame->miTickCounter : -1))
	{
		return;
	}

#if defined(BT_CLIENT)
	// Engine-generic client automation (capture, window, UI, synthetic input, GPU profile) runs before the game client handler.
	if (engine::ExecuteClientAgentCommand(command, rParameters, rResult))
	{
		return;
	}

	if (!ExecuteAgentCommandClient(command, rParameters, rResult))
	{
		throw std::runtime_error("unknown command");
	}
#elif defined(BT_SERVER)
	if (!ExecuteAgentCommandServer(command, rParameters, rResult))
	{
		throw std::runtime_error("unknown command");
	}
#else
	throw std::runtime_error("unknown command");
#endif
}

} // namespace game
