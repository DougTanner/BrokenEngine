#pragma once

#if defined(BT_SERVER)

#include "Network/Server/ServerSessionBase.h"

namespace game
{

class ServerFleetManager;
class ServerClientManager;

struct PendingUpdateOwnedEntityRequest
{
	int64_t iClientId = 0;
	engine::GlobalId globalId {};
	bool bUseMissiles = false;
	std::chrono::duration<float> navigationDelaySeconds = std::chrono::duration<float>(60.0f);
};

class ServerSession : public engine::ServerSessionBase, public common::Singleton<ServerSession>
{
public:

	ServerSession();
	~ServerSession();

	void WriteFleetData(std::fstream& rFileStream) const;

	// Engine ServerBroadcaster::BuildFrameInputs calls this once per advancing update, after the frame-input reset and
	// before the first tick's PrepareTickStatusChanges drain.
	void BuildAdvancingFrameInputs();

	// Steps the fleet and respawn timers once per tick. Engine ServerSessionBase::PrepareTick calls this in every
	// normal-play tick, after the tick's frame inputs exist and before PrepareTickStatusChanges takes the broadcast
	// snapshot; replay playback calls it through PrepareReplayPlaybackTick.
	void TickGameTimers(bool bRecording);

	// Engine ServerTransferManager calls this after a transferred owned entity's client GUID is assigned in its destination.
	void OnOwnedEntityChangedCell(const engine::ClientGuid& rClientGuid, engine::GlobalId globalId, engine::GridCoord destination);

	std::vector<PendingUpdateOwnedEntityRequest> mPendingUpdateOwnedEntityRequests;

	std::unique_ptr<ServerFleetManager> mpFleetManager;
	std::unique_ptr<ServerClientManager> mpClientManager;

private:
	friend class engine::ServerSessionBase;
	friend class engine::ServerSessionRuntime;

	void BeforeNetworkPoll();
	void AfterNetworkPoll();
	void FinalizeTickClients();
	void ProcessUpdateOwnedEntityRequests();

	// engine::ServerSessionBase calls these by name
	void HandleGamePacket(const engine::DeferredClientPacket& rPacket);
	void OnResetForLoad();
	void OnOwnedEntityDied(const engine::ClientGuid& rClientGuid, engine::GlobalId globalId);

};

inline ServerSession* gpServerSession = nullptr;

} // namespace game

#endif // BT_SERVER
