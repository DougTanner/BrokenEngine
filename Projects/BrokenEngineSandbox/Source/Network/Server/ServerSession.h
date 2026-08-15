#pragma once

#if defined(BT_SERVER)

#include "Network/PlayerEvents.h"

namespace engine
{

class NetworkDiscoveryResponder;
class ServerSessionRuntime;

} // namespace engine

namespace game
{

class ServerFleetManager;
class ServerTransferManager;
class ServerBroadcaster;
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
	void BroadcastTimespeedIfChanged();
	void StepTimescale(bool bFaster); // step time scale one notch (faster/slower) and broadcast; shared by the packet handler and the agent command

	void SendTimespeedToNewClient(ENetPeer* pPeer);
	void SubscriptionUpdates();
	void ResetClientsForLoad();
	int64_t RelinkFromFrames(int64_t iClientId, const engine::ClientGuid& rGuid, RelinkContext eContext);
	void WriteFleetData(std::fstream& rFileStream) const;

	engine::OwnedEntityRegistry mClientPlayers;

	std::unique_ptr<ServerFleetManager> mpFleetManager;
	std::unique_ptr<ServerTransferManager> mpTransferManager;
	std::unique_ptr<ServerBroadcaster> mpBroadcaster;
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
