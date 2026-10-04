#include "Pch.h"

#include "Network/PlayerEvents.h"

#include "Network/GamePacketType.h"
#include "Fleet.h"
#include "Game.h"

namespace game
{

void ParsePlayerEvents(const std::vector<std::pair<uint8_t, std::vector<uint8_t>>>& rRawPackets, common::ScopedWorkbufferArena& rOutputEventsArena)
{
	for (const auto& [uiPacketType, rPayload] : rRawPackets)
	{
		GamePacketType eType = static_cast<GamePacketType>(uiPacketType);

		if (eType == GamePacketType::kServerAssignPlayer)
		{
			GameMessages::AssignPlayerMessage message {};
			engine::NetworkMessages::Read(rPayload, message);
			rOutputEventsArena.mBuffer.PushBack(ReceivedPlayerEvent {.eType = PlayerEventType::kAssigned, .globalPlayerId = engine::GlobalId {.iValue = message.iGlobalPlayerIdentifier}, .coordinate = message.coordinate});
		}
		else if (eType == GamePacketType::kServerPlayerState)
		{
			GameMessages::PlayerStateMessage message {};
			engine::NetworkMessages::Read(rPayload, message);
			const GameMessages::PlayerStateDescriptor& rDescriptor = GameMessages::kpPlayerStateDescriptors[static_cast<size_t>(static_cast<PlayerStateWireType>(message.uiWireType))];
			rOutputEventsArena.mBuffer.PushBack(ReceivedPlayerEvent {.eType = rDescriptor.eEventType, .globalPlayerId = engine::GlobalId {.iValue = message.iGlobalPlayerIdentifier}, .coordinate = message.coordinate});
		}
	}
}

bool ParseFleetSynchronization(std::vector<std::pair<uint8_t, std::vector<uint8_t>>>& rRawPackets, std::vector<Fleet>& rOutputFleets)
{
	// A sync, including a zero-fleet sync, commits into rOutputFleets but leaves it empty when
	// fleetCount == 0, so emptiness cannot tell the caller "applied" from "nothing arrived" — report it explicitly
	bool bApplied = false;
	for (auto it = rRawPackets.begin(); it != rRawPackets.end(); )
	{
		GamePacketType eType = static_cast<GamePacketType>(it->first);
		if (eType != GamePacketType::kServerFleetSync)
		{
			++it;
			continue;
		}

		// Each sync replaces the whole list, so parse into a fresh vector: fields the payload does not carry
		// start at their defaults rather than inheriting an earlier sync's values
		std::vector<Fleet> parsedFleets;
		GameMessages::FleetSyncMessage::ReadPayload(it->second, parsedFleets);
		rOutputFleets = std::move(parsedFleets);
		bApplied = true;

		it = rRawPackets.erase(it);
	}

	return bApplied;
}

} // namespace game
