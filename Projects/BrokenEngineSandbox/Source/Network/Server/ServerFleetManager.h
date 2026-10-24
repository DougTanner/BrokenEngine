#pragma once

#if defined(BT_SERVER)

#include "Network/Server/FleetNavigationController.h"
#include "Network/Server/ServerFleetSerialization.h"
#include "Fleet.h"

namespace game
{

struct PendingCreateFleetRequest
{
	int64_t iClientId = 0;
};

struct PendingSpawnIntoFleetRequest
{
	int64_t iClientId = 0;
	FleetGuid fleetGuid {};
	ShipType eType {};
};

class ServerFleetManager
{
public:

	ServerFleetManager();

	void ProcessCreateFleetRequests();
	void ProcessSpawnIntoFleetRequests();
	void TickFleetTimers();
	void TickRespawnTimers();

	void SendFleetSyncToClient(const engine::ClientGuid& rClientGuid);

	void ClearPendingRequests();

	void OnOwnedEntityTransferred(const engine::ClientGuid& rGuid, engine::GlobalId globalId, engine::GridCoord destination);
	void OnClientConnected(int64_t iClientId, const engine::ClientGuid& rClientGuid);
	void OnClientDisconnected(int64_t iClientId, const engine::ClientGuid& rClientGuid);
	void OnResetForLoad(int64_t iClientId, const engine::ClientGuid& rClientGuid);
	void RebuildFleetMembersForLoad();

	void UpdateFleetNavigationDelay(const engine::ClientGuid& rGuid, const FleetGuid& rFleetGuid, std::chrono::duration<float> navigationDelaySeconds);

	void DetectFleetMemberDeaths();

	// Destructive wipe of all fleet state (mFleets, mGuidToClientId, pending requests).
	// Use ONLY for fresh-game flows (ServerReset, including the client reset request it serves).
	// Do NOT call from the load path — ReadFleetData is authoritative there; use ClearPendingRequests() instead.
	void ResetState();

	int64_t FindClientIdForGuid(const engine::ClientGuid& rGuid) const;

	// All fleets keyed by persistent ClientGuid (survives disconnect/reconnect)
	std::unordered_map<engine::ClientGuid, std::vector<Fleet>> mFleets;

	// Connected client mapping: ClientGuid -> iClientId (0 = disconnected)
	std::unordered_map<engine::ClientGuid, int64_t> mGuidToClientId;

	FleetNavigationController mNavigation;

	common::RandomEngine mRandomEngine;

	ReplayFleetLog mReplayLog;

private:

	// Position of rFleetGuid within one client's fleet vector; -1 when absent.
	int64_t FindFleetIndexByGuid(const std::vector<Fleet>& rFleets, const FleetGuid& rFleetGuid) const;

	void CreateFleet(const engine::ClientGuid& rClientGuid);

public:

	std::vector<PendingCreateFleetRequest> mPendingCreateFleetRequests;
	std::vector<PendingSpawnIntoFleetRequest> mPendingSpawnIntoFleetRequests;
};

} // namespace game

#endif // BT_SERVER
