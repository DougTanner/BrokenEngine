#pragma once

#if defined(BT_SERVER)

// The engine owns per-cell publication assembly and the deferred-injection drain: the broadcast snapshot of this
// tick's status changes, the deterministic type grouping the wire format depends on, and the per-coordinate update
// handed to ServerSessionRuntime::PublishTick. The game supplies the StatusChange payloads. The game Agent harness rig
// owns its deferred injection queue; it drains once per tick before that tick's snapshot. This header stays out of
// the Engine.h aggregation and its consumers include it directly, though nothing in it requires that:
// game::StatusChange, the one game type it needs, is already complete where the game Pch.h includes Engine.h.

namespace engine
{

class ServerSessionRuntime;

class ServerBroadcaster
{
public:

	void BuildFrameInputs();
	void PrepareTickStatusChanges();
	void BuildTickPublication(int64_t iTick, engine::ServerSessionRuntime& rRuntime, const common::ScopedWorkbufferArena& rPublicationArena);

	void ResetState();

	std::unordered_map<engine::GridCoord, std::vector<game::StatusChange>> mBroadcastStatusChanges;
};

} // namespace engine

#endif // BT_SERVER
