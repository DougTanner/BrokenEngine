#pragma once

#if defined(BT_CLIENT)

#include "Frame/GridCoord.h"
#include "Network/Client/ClientDesyncCore.h"

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
struct ReceivedOwnedEntityEvent;
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

class ClientSession : public common::Singleton<ClientSession>
{
public:

	ClientSession();
	~ClientSession();

	void ConnectToServer(std::string_view serverAddress);

	void Reconcile();


	void ApplyReceivedStaticData();

	void SendUpdateOwnedEntityRequest(int64_t iGlobalId, bool bUseMissiles, float fNavigationDelay);
	void SendCreateFleetRequest();
	void SendSpawnIntoFleetRequest(const FleetGuid& rFleetGuid);
	void SendRespawnInFleetRequest(const FleetGuid& rFleetGuid, engine::GlobalId memberGlobalId);
	void SendDeleteFleetRequest(const FleetGuid& rFleetGuid);
	void SendFleetNavigationDelayRequest(const FleetGuid& rFleetGuid, float fDelay);

	// Game policy the engine desync core calls by name
	void ResetCoordinateStatesForResynchronization();

	std::unique_ptr<engine::ClientDesyncCore> mpDesynchronizationCore;
	std::unique_ptr<ClientReconciler> mpReconciler;
	std::unique_ptr<engine::ClientSessionRuntime> mpRuntime;

private:
	friend class engine::ClientSessionRuntime;

	void OnConnectionRejected(std::string_view reason);
	void OnConnectionFailed();
	void OnConnectionAccepted();
	void PollDesynchronizationState();
	void OnConnectionLost();
	void OnServerLoad();
	void OnRuntimeDisconnected();
	void ProcessReceivedGamePackets();
	void HydrateReceivedFullState(Frame& rReceived, const Frame* pRingTail);

	void ApplyOwnedEntityEvent(const ReceivedOwnedEntityEvent& rEvent);
	void UpdateOwnedEntityCoordinate(engine::GlobalId globalId, engine::GridCoord coordinate);

};

inline ClientSession* gpClientSession = nullptr;

} // namespace game

#endif // BT_CLIENT
