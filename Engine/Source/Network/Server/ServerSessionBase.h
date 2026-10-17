#pragma once

#if defined(BT_SERVER)

namespace engine
{

class ServerBroadcaster;
class ServerSessionRuntime;
class ServerTransferManager;

// The reusable server session state and mechanics the game server session inherits: owned-entity tracking rebuilt from
// the Frame ownership layer, the deferred client-packet drain and its debug-control handlers, tick preparation, and load
// reset. Game functions are called by name through game::gpServerSession.
class ServerSessionBase
{
public:

	ServerSessionBase();
	~ServerSessionBase();

	void PrepareTick();
	void StepTimescale(bool bFaster); // step time scale one notch (faster/slower) and broadcast; shared by the packet handler and the agent command
	void ResetForLoad();

	OwnedEntityRegistry mClientOwnedEntities;

	std::unique_ptr<ServerTransferManager> mpTransferManager;
	std::unique_ptr<ServerBroadcaster> mpBroadcaster;
	std::unique_ptr<ServerSessionRuntime> mpRuntime;

private:
	friend class ServerSessionRuntime;

	void DispatchDeferredClientPackets();
	void HandleDebugControlPacket(const DeferredClientPacket& rPacket);
	void RebuildOwnedEntities();
	void AddOwnedEntityCoordinates();
	void SendOwnedEntityState(int64_t iClientId, OwnedEntityState eState, GlobalId globalId, GridCoord coordinate);
};

} // namespace engine

#endif // BT_SERVER
