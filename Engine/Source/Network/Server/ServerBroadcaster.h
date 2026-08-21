#pragma once

#if defined(BT_SERVER)

// The engine owns per-cell publication assembly and the deferred-injection drain: the broadcast snapshot of this
// tick's status changes, the deterministic type grouping the wire format depends on, and the per-coordinate update
// handed to ServerSessionRuntime::PublishTick. The game supplies the StatusChange payloads and the game-policy
// queues on game::ServerSession that this machinery drains. Naming game types keeps this header out of the Engine.h
// aggregation; its consumers include it directly.

namespace engine
{

class ServerSessionRuntime;

struct PendingUpdatePlayerRequest
{
	int64_t iClientId = 0;
	engine::global_id_t globalId {};
	bool bUseMissiles = false;
	float fNavigationDelay = 60.0f;
};

class ServerBroadcaster
{
public:

	void BuildFrameInputs();
	void BuildTickPublication(int64_t iTick, engine::ServerSessionRuntime& rRuntime, common::ScopedWorkbufferArena& rPublicationArena);
	void ProcessUpdatePlayerRequests();

	void QueueUpdatePlayerRequest(const PendingUpdatePlayerRequest& rRequest);
	void QueueAgentStatusChange(engine::GridCoord coord, const game::StatusChange& rChange);
	void ClearPendingRequests();
	void ResetState();

	std::unordered_map<engine::GridCoord, std::vector<game::StatusChange>> mBroadcastStatusChanges;

private:

	std::vector<PendingUpdatePlayerRequest> mPendingUpdatePlayerRequests;
};

} // namespace engine

#endif // BT_SERVER
