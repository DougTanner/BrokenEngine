#pragma once

namespace game
{

struct Fleet;

enum class PlayerStateWireType : uint8_t
{
	kSpawned,
	kChangedFrame,
	kDied,

	kCount,
};

enum class PlayerEventType : uint8_t
{
	kAssigned,
	kSpawned,
	kChangedFrame,
	kDied,
};

struct ReceivedPlayerEvent
{
	PlayerEventType eType {};
	engine::GlobalId globalPlayerId {};
	engine::GridCoord coordinate {};
};

void ParsePlayerEvents(const std::vector<std::pair<int64_t, std::vector<uint8_t>>>& rRawPackets, common::ScopedWorkbufferArena& rOutputEventsArena);

bool ParseFleetSynchronization(std::vector<std::pair<int64_t, std::vector<uint8_t>>>& rRawPackets, std::vector<Fleet>& rOutputFleets);

} // namespace game
