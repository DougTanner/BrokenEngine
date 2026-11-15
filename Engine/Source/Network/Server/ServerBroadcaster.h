#pragma once

#if defined(BT_SERVER)

// The engine owns per-cell publication assembly and the deferred-injection drain: the type sort of this tick's status
// changes in each frame input, matching the order the wire format's type grouping gives clients, and the per-coordinate
// update handed to ServerSessionRuntime::PublishTick. The game supplies the StatusChange payloads. The game Agent
// harness rig owns its deferred injection queue; it drains once per tick before that tick's type sort. This header
// stays out of the Engine.h aggregation and its consumers include it directly, though it names no game type.

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
};

} // namespace engine

#endif // BT_SERVER
