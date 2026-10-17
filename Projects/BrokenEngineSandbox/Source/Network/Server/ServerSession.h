#pragma once

#if defined(BT_SERVER)

#include "Network/OwnedEntityEvents.h"

namespace engine
{

class NetworkDiscoveryResponder;
class ServerBroadcaster;
class ServerSessionRuntime;
class ServerTransferManager;

} // namespace engine

namespace game
{

class ServerFleetManager;
class ServerClientManager;

struct SubscriptionUpdate
{
	int64_t iClientId = 0;
	engine::GridCoord newCoordinate {};
	engine::GlobalId globalId {};
};

struct PendingUpdateOwnedEntityRequest
{
	int64_t iClientId = 0;
	engine::GlobalId globalId {};
	bool bUseMissiles = false;
	std::chrono::duration<float> navigationDelaySeconds = std::chrono::duration<float>(60.0f);
};

class ServerSession : public common::Singleton<ServerSession>
{
public:

	enum class RelinkContext
	{
		kConnect,
		kLoad,
	};

	ServerSession();
	~ServerSession();

	void PrepareTick();
	void SendAssignOwnedEntity(int64_t iClientId, engine::GlobalId globalId, engine::GridCoord coordinate);
	void SendOwnedEntityState(int64_t iClientId, OwnedEntityStateWireType eWireType, int64_t iGlobalId, engine::GridCoord coordinate);
	void StepTimescale(bool bFaster); // step time scale one notch (faster/slower) and broadcast; shared by the packet handler and the agent command
	void SubscriptionUpdates();
	void ResetClientsForLoad();
	int64_t RelinkFromFrames(int64_t iClientId, const engine::ClientGuid& rGuid, RelinkContext eContext);
	void WriteFleetData(std::fstream& rFileStream) const;

	// Engine ServerBroadcaster::BuildFrameInputs calls this once per advancing update, after the frame-input reset and
	// before the first tick's PrepareTickStatusChanges drain.
	void BuildAdvancingFrameInputs();

	// Engine ServerTransferManager calls this after a transferred owned entity's client GUID is assigned in its destination.
	void OnOwnedEntityChangedCell(const engine::ClientGuid& rClientGuid, engine::GlobalId globalId, engine::GridCoord destination);

	engine::OwnedEntityRegistry mClientOwnedEntities;

	std::vector<SubscriptionUpdate> mPendingSubscriptionUpdates;
	std::vector<PendingUpdateOwnedEntityRequest> mPendingUpdateOwnedEntityRequests;

	std::unique_ptr<ServerFleetManager> mpFleetManager;
	std::unique_ptr<engine::ServerTransferManager> mpTransferManager;
	std::unique_ptr<engine::ServerBroadcaster> mpBroadcaster;
	std::unique_ptr<ServerClientManager> mpClientManager;
	std::unique_ptr<engine::ServerSessionRuntime> mpRuntime;

private:
	friend class engine::ServerSessionRuntime;

	void BeforeNetworkPoll();
	void AfterNetworkPoll();
	void ParseReceivedGamePackets();
	void FinalizeTickClients();
	void ProcessUpdateOwnedEntityRequests();

	// ServerSessionRuntime::ComputeActiveSet calls this by name
	void AddGameRequiredCoordinates();

};

inline ServerSession* gpServerSession = nullptr;

} // namespace game

#endif // BT_SERVER
