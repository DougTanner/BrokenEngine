#include "Pch.h"

#include "Network/PlayerEvents.h"

#include "Fleet.h"
#include "Game.h"
#include "Network/GamePacketType.h"

namespace game
{

void ParsePlayerEvents(std::vector<std::pair<uint8_t, std::vector<uint8_t>>>& rRawPackets, common::ScopedWorkbufferArena& rOutEventsArena)
{
	for (const auto& [uiPacketType, rPayload] : rRawPackets)
	{
		GamePacketType eType = static_cast<GamePacketType>(uiPacketType);

		if (eType == GamePacketType::kServerAssignPlayer)
		{
			if (rPayload.size() != static_cast<size_t>(GameMessages::AssignPlayerMessage::kiSize))
			{
				engine::NetworkMessages::ThrowCorruptStream("ParsePlayerEvents kServerAssignPlayer");
			}
			GameMessages::AssignPlayerMessage message {};
			engine::NetworkMessages::Read(rPayload, message);
			rOutEventsArena.PushBack(ReceivedPlayerEvent{PlayerEventType::kAssigned, engine::global_id_t {message.iGlobalPlayerId}, message.coord});
		}
		else if (eType == GamePacketType::kServerPlayerState)
		{
			if (rPayload.size() != static_cast<size_t>(GameMessages::PlayerStateMessage::kiSize))
			{
				engine::NetworkMessages::ThrowCorruptStream("ParsePlayerEvents kServerPlayerState");
			}
			GameMessages::PlayerStateMessage message {};
			engine::NetworkMessages::Read(rPayload, message);
			const GameMessages::PlayerStateDescriptor* pDescriptor = GameMessages::FindPlayerStateDescriptor(message.uiWireType);
			if (pDescriptor == nullptr)
			{
				engine::NetworkMessages::ThrowCorruptStream("ParsePlayerEvents kServerPlayerState wire type");
			}
			rOutEventsArena.PushBack(ReceivedPlayerEvent{pDescriptor->eEventType, engine::global_id_t {message.iGlobalPlayerId}, message.coord});
		}
	}
}

bool ParseFleetSync(std::vector<std::pair<uint8_t, std::vector<uint8_t>>>& rRawPackets, std::vector<Fleet>& rOutFleets)
{
	// A valid sync (including a valid zero-fleet sync) commits into rOutFleets but leaves it empty when
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

		// Network payload is a trust boundary: ReadPayload bounds every read against the payload end and
		// throws on malformed input. Parse into a local list and commit only once it returns, so a
		// malformed sync is never partially applied and cannot clobber a valid sync parsed earlier in this drain
		std::vector<Fleet> parsedFleets;
		GameMessages::FleetSyncMessage::ReadPayload(it->second, parsedFleets);
		rOutFleets = std::move(parsedFleets);
		bApplied = true;

		it = rRawPackets.erase(it);
	}

	return bApplied;
}

} // namespace game
