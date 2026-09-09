#include "Agent/AgentCommands.h"

#include "Agent/Commands/CollectionLayoutCapacityFixture.h"
#include "Agent/Commands/RegistryFixture.h"
#include "Game.h"

#if defined(BT_CLIENT)
#include "Profile/ProfileManager.h"
#endif

namespace game
{

void ExecuteAgentCommand(std::string_view cmd, const nlohmann::json& rParams, nlohmann::json& rResult)
{
	if (engine::ExecuteSharedAgentCommand(cmd, rParams, rResult, gpGame != nullptr ? gpGame->TickCounter() : -1))
	{
		return;
	}

	// Game-owned commands reachable on both endpoints dispatch before the side-specific fallthrough.
	if (cmd == "collection_layout_capacity_fixture")
	{
		CommandCollectionLayoutCapacityFixture(rParams, rResult);
		return;
	}

	if (cmd == "registry_fixture")
	{
		CommandRegistryFixture(rParams, rResult);
		return;
	}

#if defined(BT_CLIENT)
	// Engine-generic client automation (capture, window, UI, synthetic input, GPU profile) runs before the game
	// client handler; it reads the live game/profile state through these references instead of the game globals.
	if (engine::ExecuteClientAgentCommand(cmd, rParams, rResult, *gpGame, *gpProfileManager))
	{
		return;
	}

	if (!ExecuteAgentCommandClient(cmd, rParams, rResult))
	{
		throw std::runtime_error("unknown command");
	}
#elif defined(BT_SERVER)
	if (!ExecuteAgentCommandServer(cmd, rParams, rResult))
	{
		throw std::runtime_error("unknown command");
	}
#else
	throw std::runtime_error("unknown command");
#endif
}

} // namespace game
