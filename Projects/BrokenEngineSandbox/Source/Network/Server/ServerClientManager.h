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
	// rTakenSpawnPoints is the open workbuffer frame holding, as XMFLOAT2s, the points earlier Omni and Battleship spawns
	// of the same TickRespawnTimers pass took.
	void SpawnFleetMember(const engine::ClientGuid& rClientGuid, const Fleet& rFleet, FleetMember& rMember, common::ScopedWorkbufferArena& rTakenSpawnPoints);
	void OnClientDisconnected(int64_t iClientId, const engine::ClientGuid& rClientGuid);
	void ResetState();

	std::unordered_set<int64_t> mProcessedClientIds;
	// Per fleet, the first spawn tick at which its Battleship may launch another Fighter. Outside the Frame and never in a
	// grid save: replay fleet records carry it, and a launch reaches clients only as its spawn status change.
	std::unordered_map<FleetGuid, int64_t> mNextFighterLaunchTicks;
	// The fleet Omni and Battleship spawn candidates of the origin cell in search order, built once from its generated
	// islands, which never change.
	std::vector<XMFLOAT2> mFleetSpawnCandidates;
	// Set by a fleet spawn search that found every candidate crowded and cleared by one that succeeds, so a blocked
	// episode logs one warning.
	bool mbFleetSpawnBlocked = false;

private:

	void LogConnectingClientDiagnostic(const engine::ClientConnection& rClient);
};

} // namespace game

#endif // BT_SERVER
