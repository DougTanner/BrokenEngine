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

struct PendingDestroyFleetMemberRequest
{
	int64_t iClientId = 0;
	engine::GlobalId globalId {};
};

struct PendingSquadronOrdersRequest
{
	int64_t iClientId = 0;
	engine::GlobalId globalId {};
	ShipOrders_t orders {};
};

struct FleetMemberLocation
{
	Fleet* pFleet = nullptr;
	FleetMember* pMember = nullptr;
};

struct ConnectedOwner
{
	int64_t iClientId = 0;
	// The CRC of the last FleetSync packet sent to this owner; 0 until one is sent, so the next send always goes.
	common::crc_t uiLastFleetSyncCrc = 0;
};

class ServerFleetManager
{
public:

	ServerFleetManager();

	void ProcessCreateFleetRequests();
	void ProcessSpawnIntoFleetRequests();
	void ProcessDestroyFleetMemberRequests();
	void ProcessSquadronOrdersRequests();
	void TickFleetTimers();
	void TickRespawnTimers();

	// Sends each connected owner its FleetSync when the packet differs from the last one it was sent.
	void SendChangedFleetSyncs();

	void ClearPendingRequests();

	void OnOwnedEntityTransferred(const engine::ClientGuid& rGuid, engine::GlobalId globalId, engine::GridCoord destination);
	void OnClientConnected(int64_t iClientId, const engine::ClientGuid& rClientGuid);
	void OnClientDisconnected(int64_t iClientId, const engine::ClientGuid& rClientGuid);
	void OnResetForLoad(int64_t iClientId, const engine::ClientGuid& rClientGuid);
	void RebuildFleetMembersForLoad();

	void UpdateFleetNavigation(const engine::ClientGuid& rGuid, const FleetGuid& rFleetGuid, std::chrono::duration<float> navigationDelaySeconds, FleetBehaviour eBehaviour, engine::GridCoord holdCoordinate);

	void DetectFleetMemberDeaths();

	// Destructive wipe of all fleet state (mFleets, mGuidToClientId).
	// Use ONLY for fresh-game flows (ServerReset, including the client reset request it serves).
	// Do NOT call from the load path — ReadFleetData is authoritative there; use ClearPendingRequests() instead.
	void ResetState();

	// The owner's fleet and member named by globalId; both null when the owner has no such member.
	FleetMemberLocation FindFleetMember(const engine::ClientGuid& rClientGuid, engine::GlobalId globalId);

	// All fleets keyed by persistent ClientGuid (survives disconnect/reconnect)
	std::unordered_map<engine::ClientGuid, std::vector<Fleet>> mFleets;

	// Connected client mapping: ClientGuid -> client id (0 = disconnected) and last sent FleetSync CRC
	std::unordered_map<engine::ClientGuid, ConnectedOwner> mGuidToClientId;

	FleetNavigationController mNavigation;

	common::RandomEngine mRandomEngine;

	ReplayFleetLog mReplayLog;

private:

	void CreateFleet(const engine::ClientGuid& rClientGuid);
	FleetMember& AddFleetMember(const engine::ClientGuid& rClientGuid, Fleet& rFleet, ShipType eType, uint8_t uiSquadron);

public:

	std::vector<PendingCreateFleetRequest> mPendingCreateFleetRequests;
	std::vector<PendingSpawnIntoFleetRequest> mPendingSpawnIntoFleetRequests;
	std::vector<PendingDestroyFleetMemberRequest> mPendingDestroyFleetMemberRequests;
	std::vector<PendingSquadronOrdersRequest> mPendingSquadronOrdersRequests;
};

} // namespace game

#endif // BT_SERVER
