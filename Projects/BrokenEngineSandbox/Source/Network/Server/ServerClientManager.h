#pragma once

#if defined(BT_SERVER)

#include "Fleet.h"

namespace game
{

struct ClientSpawnInformation
{
	int64_t iClientId = 0;
	engine::ClientGuid clientGuid {};
	// Default-constructed (empty) guid means this spawn is not fleet-triggered.
	FleetGuid fleetGuid {};
	// Invalid {} spawns a new fleet member; a valid ID respawns that member and keeps its ID.
	engine::GlobalId memberGlobalPlayerId {};
};

class ServerClientManager
{
public:

	void QueueSpawnForClient(int64_t iClientId, const engine::ClientGuid& rClientGuid, const FleetGuid& rFleetGuid = {}, engine::GlobalId memberGlobalPlayerId = {});
	void NewClients();
	void SpawnWaitingClients();
	void Disconnects();
	void DetectPlayerDeaths();
	void ResetState();

	std::vector<ClientSpawnInformation> mClientsWaitingForSpawn;
	std::unordered_set<int64_t> mDeadClientIds;
	std::unordered_set<int64_t> mProcessedClientIds;

private:

	void LogConnectingClientDiagnostic(const engine::ClientConnection& rClient);
};

} // namespace game

#endif // BT_SERVER
