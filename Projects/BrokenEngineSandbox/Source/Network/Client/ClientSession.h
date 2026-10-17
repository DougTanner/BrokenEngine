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

	void SendUpdateOwnedEntityRequest(int64_t iGlobalId, bool bUseMissiles, float fNavigationDelay);
	void SendCreateFleetRequest();
	void SendSpawnIntoFleetRequest(const FleetGuid& rFleetGuid);
	void SendRespawnInFleetRequest(const FleetGuid& rFleetGuid, engine::GlobalId memberGlobalId);
	void SendDeleteFleetRequest(const FleetGuid& rFleetGuid);
	void SendFleetNavigationDelayRequest(const FleetGuid& rFleetGuid, float fDelay);

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

};

inline ClientSession* gpClientSession = nullptr;

} // namespace game

#endif // BT_CLIENT
