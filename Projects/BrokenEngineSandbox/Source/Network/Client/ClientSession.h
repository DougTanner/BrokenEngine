#pragma once

#if defined(BT_CLIENT)

#include "Frame/GridCoord.h"
#include "Network/Client/ClientDesyncCore.h"

#include "Network/Client/ClientReconciler.h"
#include "Fleet.h"

namespace engine
{

class Client;
class ClientSessionRuntime;
struct ReceivedDebugFrame;

} // namespace engine

namespace game
{

struct Frame;
struct ReceivedPlayerEvent;
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
	kSelectPlayer,
};

const char* ToString(SubscriptionChangeReason eReason);

class ClientSession
{
public:

	ClientSession();
	~ClientSession();

	void ConnectToServer(std::string_view serverAddress);

	void Reconcile();


	void ApplyReceivedStaticData();

	void SendUpdatePlayerRequest(int64_t iGlobalPlayerId, bool bUseMissiles, float fNavigationDelay);
	void SendCreateFleetRequest();
	void SendSpawnIntoFleetRequest(const FleetGuid& rFleetGuid);
	void SendRespawnInFleetRequest(const FleetGuid& rFleetGuid, engine::GlobalId memberGlobalPlayerId);
	void SendDeleteFleetRequest(const FleetGuid& rFleetGuid);
	void SendFleetNavigationDelayRequest(const FleetGuid& rFleetGuid, float fDelay);

	void UpdateDesiredCoordinates(SubscriptionChangeReason eReason);

	// Game policy the engine desync core calls back into
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

	void ApplyPlayerEvent(const ReceivedPlayerEvent& rEvent);
	void UpdatePlayerCoordinate(engine::GlobalId globalPlayerId, engine::GridCoord coordinate);

};

inline ClientSession* gpClientSession = nullptr;

} // namespace game

#endif // BT_CLIENT
