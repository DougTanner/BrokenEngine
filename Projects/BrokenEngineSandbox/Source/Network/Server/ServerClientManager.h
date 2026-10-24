#pragma once

#if defined(BT_SERVER)

#include "Fleet.h"

namespace game
{

class ServerClientManager
{
public:

	void NewClients();
	void SpawnFleetMember(const engine::ClientGuid& rClientGuid, const Fleet& rFleet, FleetMember& rMember);
	void OnClientDisconnected(int64_t iClientId, const engine::ClientGuid& rClientGuid);
	void ResetState();

	std::unordered_set<int64_t> mProcessedClientIds;
	// Per fleet, the first spawn tick at which its Battleship may launch another Fighter. Outside the Frame and never in a
	// grid save: replay fleet records carry it, and a launch reaches clients only as its spawn status change.
	std::unordered_map<FleetGuid, int64_t> mNextFighterLaunchTicks;

private:

	void LogConnectingClientDiagnostic(const engine::ClientConnection& rClient);
};

} // namespace game

#endif // BT_SERVER
