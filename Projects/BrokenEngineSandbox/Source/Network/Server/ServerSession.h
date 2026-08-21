#pragma once

#if defined(BT_SERVER)

#include "Network/PlayerEvents.h"

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
	engine::GridCoord newCoord {};
	engine::global_id_t globalPlayerId {};
};

class ServerSession
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
	void ParseReceivedGamePackets();
	void SendAssignPlayer(int64_t iClientId, engine::global_id_t globalId, engine::GridCoord coord);
	void SendPlayerState(int64_t iClientId, PlayerStateWireType eWireType, int64_t iGlobalPlayerId, engine::GridCoord coord);
	void StepTimescale(bool bFaster); // step time scale one notch (faster/slower) and broadcast; shared by the packet handler and the agent command
	void SubscriptionUpdates();
	void ResetClientsForLoad();
	int64_t RelinkFromFrames(int64_t iClientId, const engine::ClientGuid& rGuid, RelinkContext eContext);
	void WriteFleetData(std::fstream& rFileStream) const;

	engine::OwnedEntityRegistry mClientPlayers;

	// Game-policy transient queues the engine transfer/broadcast machinery drains and fills. They stay here because
	// every consumer outside that machinery is game policy: the agent fixture and injection commands, the death-scan
	// mid-transfer skip, replay start/stop state clearing, and SubscriptionUpdates.
	std::unordered_map<engine::GridCoord, std::vector<StatusChange>> mReplayTransferFixtures;
	std::vector<SubscriptionUpdate> mPendingSubscriptionUpdates;

	// Agent-injected StatusChanges accumulated at the command drain point; consumed in ServerBroadcaster::BuildFrameInputs
	// before the mBroadcastStatusChanges snapshot, per-coord, only when ticking (not paused/zero-tick), not during replay
	// playback, no clients awaiting spawn, and the coord is active + frame-ready — else held for a later tick. Cross-update
	// accumulator: cleared only in ServerBroadcaster::ResetState(), never in ClearPendingRequests() (which runs before the
	// agent Drain). ServerBroadcaster.cpp carries detail.
	std::unordered_map<engine::GridCoord, std::vector<StatusChange>> mPendingAgentStatusChanges;

	std::unique_ptr<ServerFleetManager> mpFleetManager;
	std::unique_ptr<engine::ServerTransferManager> mpTransferManager;
	std::unique_ptr<engine::ServerBroadcaster> mpBroadcaster;
	std::unique_ptr<ServerClientManager> mpClientManager;
	std::unique_ptr<engine::ServerSessionRuntime> mpRuntime;

private:
	friend class engine::ServerSessionRuntime;

	void BeforeNetworkPoll();
	void AfterNetworkPoll();
	void FinalizeTickClients();

	// ServerSessionRuntime::ComputeActiveSet hooks
	void AddGameRequiredCoords();
	void OnFrameRetiring(engine::GridCoord coord, std::unique_ptr<game::Frame>& rpFrame);

};

inline ServerSession* gpServerSession = nullptr;

} // namespace game

#endif // BT_SERVER
