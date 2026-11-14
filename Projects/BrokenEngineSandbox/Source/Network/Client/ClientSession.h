#pragma once

#if defined(BT_CLIENT)

#include "Frame/GridCoord.h"
#include "Network/Client/ClientSessionBase.h"

#include "Fleet.h"

namespace engine
{

class Client;
class ClientSessionRuntime;
struct ReceivedDebugFrame;

} // namespace engine

namespace game
{

class ClientReconciler;
struct Frame;
enum class GamePacketType : uint8_t;

enum class SubscriptionChangeReason : uint8_t
{
	kAssigned,
	kSpawned,
	kChangedCell,
	kDied,
	kFleetSynchronization,
	kPollTick,
	kFocusNextFleet,
	kFocusPreviousFleet,
	kSelectOwnedEntity,
};

const char* ToString(SubscriptionChangeReason eReason);

class ClientSession : public engine::ClientSessionBase, public common::Singleton<ClientSession>
{
public:

	ClientSession();
	~ClientSession();

	void ConnectToServer(std::string_view serverAddress);

	void SendCreateFleetRequest();
	void SendSpawnIntoFleetRequest(const FleetGuid& rFleetGuid, ShipType eType);
	void SendDestroyFleetMemberRequest(engine::GlobalId globalId);
	void SendSquadronOrdersRequest(engine::GlobalId globalId, FleetMemberFlags_t flags);
	void SendFleetNavigationRequest(const FleetGuid& rFleetGuid, float fDelay, FleetBehaviour eBehaviour, engine::GridCoord holdCoordinate);

	std::unique_ptr<ClientReconciler> mpReconciler;

private:
	friend class engine::ClientSessionBase;
	friend class engine::ClientSessionRuntime;

	void OnConnectionRejected(std::string_view reason);
	void OnConnectionFailed();
	void OnConnectionAccepted();
	void OnConnectionLost();
	void OnServerLoad();
	void ProcessReceivedGamePackets();
	void HydrateReceivedFullState(Frame& rReceived, const Frame* pRingTail);

	// engine::ClientSessionBase calls this by name
	void ApplyOwnedEntityEvent(const engine::ReceivedOwnedEntityEvent& rEvent);
	void UpdateOwnedEntityCoordinate(engine::GlobalId globalId, engine::GridCoord coordinate);

	// When the last map fleet markers request went out since the map opened; empty while the map is closed.
	std::optional<std::chrono::steady_clock::time_point> mOptionalMapFleetMarkersRequestTime;
};

inline ClientSession* gpClientSession = nullptr;

} // namespace game

#endif // BT_CLIENT
