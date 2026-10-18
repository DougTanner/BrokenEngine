#include "Agent/AgentCommands.h"

#include "Game.h"

#if defined(BT_CLIENT)
#include "Profile/ProfileRows.h"
#endif

namespace game
{

void ExecuteAgentCommand(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (engine::ExecuteSharedAgentCommand(command, rParameters, rResult, gpGame != nullptr ? gpGame->miTickCounter : -1))
	{
		return;
	}

	// The game side handler runs before the engine side-generic one, so it can take over an engine command: it calls the
	// exported engine command function and appends its own fields.
#if defined(BT_CLIENT)
	if (ExecuteAgentCommandClient(command, rParameters, rResult))
	{
		return;
	}

	if (!engine::ExecuteClientAgentCommand(command, rParameters, rResult))
	{
		throw std::runtime_error("unknown command");
	}
#elif defined(BT_SERVER)
	if (ExecuteAgentCommandServer(command, rParameters, rResult))
	{
		return;
	}

	if (!engine::ExecuteServerAgentCommand(command, rParameters, rResult))
	{
		throw std::runtime_error("unknown command");
	}
#else
	throw std::runtime_error("unknown command");
#endif
}

} // namespace game
