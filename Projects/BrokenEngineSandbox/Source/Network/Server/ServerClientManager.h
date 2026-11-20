#pragma once

#if defined(BT_SERVER)

#include "Fleet.h"

namespace game
{

class ServerClientManager
{
public:

	ServerClientManager();

	void NewClients();
	// The spawn's cell, or nullopt when the member stays dead. rTakenSpawnPoints is the open workbuffer frame holding, as
	// XMFLOAT2s, the points earlier Omni and Battleship spawns of the same TickRespawnTimers pass took.
	std::optional<engine::GridCoord> SpawnFleetMember(const engine::ClientGuid& rClientGuid, const Fleet& rFleet, const FleetMember& rMember, int64_t& riNextFighterLaunchBay, common::ScopedWorkbufferArena& rTakenSpawnPoints);
	void OnClientDisconnected(int64_t iClientId, const engine::ClientGuid& rClientGuid);
	void ResetState();

	std::unordered_set<int64_t> mProcessedClientIds;
	// The fleet Omni and Battleship spawn candidates of the origin cell in search order, built once from its generated
	// islands, which never change.
	std::vector<XMFLOAT2> mFleetSpawnCandidates;
	// Set by a fleet spawn search that found every candidate crowded and cleared by one that succeeds, so a blocked
	// episode logs one warning.
	bool mbFleetSpawnBlocked = false;
};

} // namespace game

#endif // BT_SERVER
