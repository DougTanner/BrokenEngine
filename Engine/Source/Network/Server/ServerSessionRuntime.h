#pragma once

#if defined(BT_SERVER)

#include "Network/Server/Server.h"

namespace game
{

class ServerSession;
struct Frame;

} // namespace game

namespace engine
{

class NetworkDiscoveryResponder;

class ServerSessionRuntime
{
public:

	explicit ServerSessionRuntime(game::ServerSession& rSession, int64_t iPort);
	~ServerSessionRuntime();

	void Poll(const NetworkTimeState& rTimeState);
	void PollTickBoundary(const NetworkTimeState& rTimeState);
	void WaitForTick(TimeStep& rTimeStep);
	void CompleteTick(int64_t iTick);
	void CompleteUpdate(int64_t iFullTicks, int64_t iTick);
	void SendNewSubscriptionFullStates();
	void ResetTransportForLoad();
	void PublishTick(int64_t iTick, std::span<const std::pair<GridCoord, GridUpdateData>> gridUpdates, std::span<const std::pair<GridCoord, const game::Frame*>> fullFrames);
	void ComputeActiveSet();

	game::ServerSession& mrSession;
	std::unique_ptr<Server> mpServer;
	std::unique_ptr<NetworkDiscoveryResponder> mpDiscoveryResponder;
	HANDLE mTimerHandle = nullptr;

private:

	void PreparePausedSubscriptions();
	void HandleResyncRequests();
	void ResetOnLastClientLeave();

	void AddSubscribedCoords();
	void SyncActiveCells();

	// Tracks whether the engine client set was non-empty as of the previous post-poll sample so the
	// network-driven pause/timescale reset fires on the non-empty -> empty transition.
	bool mbHadClients = false;
};

} // namespace engine

#endif // BT_SERVER
