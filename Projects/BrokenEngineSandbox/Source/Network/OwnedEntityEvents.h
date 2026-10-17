#pragma once

namespace game
{

struct Fleet;

enum class OwnedEntityStateWireType : uint8_t
{
	kSpawned,
	kChangedCell,
	kDied,

	kCount,
};

enum class OwnedEntityEventType : uint8_t
{
	kAssigned,
	kSpawned,
	kChangedCell,
	kDied,
};

struct ReceivedOwnedEntityEvent
{
	OwnedEntityEventType eType {};
	engine::GlobalId globalId {};
	engine::GridCoord coordinate {};
};

void ParseOwnedEntityEvents(const std::vector<std::pair<int64_t, std::vector<uint8_t>>>& rRawPackets, common::ScopedWorkbufferArena& rOutputEventsArena);

bool ParseFleetSynchronization(std::vector<std::pair<int64_t, std::vector<uint8_t>>>& rRawPackets, std::vector<Fleet>& rOutputFleets);

} // namespace game
