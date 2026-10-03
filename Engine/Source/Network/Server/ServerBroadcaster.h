#pragma once

#if defined(BT_SERVER)

// The engine owns per-cell publication assembly and the deferred-injection drain: the broadcast snapshot of this
// tick's status changes, the deterministic type grouping the wire format depends on, and the per-coordinate update
// handed to ServerSessionRuntime::PublishTick. The game supplies the StatusChange payloads. The game Agent fixture
// owns its deferred injection queue and drains it at the existing pre-snapshot phase. Naming game types keeps this
// header out of the Engine.h aggregation; its consumers include it directly.

namespace engine
{

class ServerSessionRuntime;

struct PendingUpdatePlayerRequest
{
	int64_t iClientId = 0;
	engine::GlobalId globalId {};
	bool bUseMissiles = false;
	std::chrono::duration<float> navigationDelaySeconds = std::chrono::duration<float>(60.0f);
};

class ServerBroadcaster
{
public:

	void BuildFrameInputs();
	void BuildTickPublication(int64_t iTick, engine::ServerSessionRuntime& rRuntime, const common::ScopedWorkbufferArena& rPublicationArena);
	void ProcessUpdatePlayerRequests();

	void ResetState();

	std::unordered_map<engine::GridCoord, std::vector<game::StatusChange>> mBroadcastStatusChanges;

	std::vector<PendingUpdatePlayerRequest> mPendingUpdatePlayerRequests;
};

} // namespace engine

#endif // BT_SERVER
