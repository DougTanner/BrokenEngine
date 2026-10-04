#pragma once

namespace game
{

enum class GamePacketType : uint8_t
{
	kServerAssignPlayer = static_cast<uint8_t>(engine::PacketType::kGamePacketStart),
	kServerPlayerState,
	kClientUpdatePlayerRequest,
	kClientCreateFleetRequest,
	kClientSpawnIntoFleetRequest,
	kClientRespawnInFleetRequest,
	kServerFleetSync,
	kClientDeleteFleetRequest,
	kClientFleetNavigationDelay,
	// Server-side debug control requests (one-way client -> server); handlers live in ServerSession::ParseReceivedGamePackets
	kClientSaveRequest,           // Client requests server quicksave (debug only)
	kClientLoadRequest,           // Client requests server quickload (debug only)
	kClientResetRequest,          // Client requests server reset (debug only)
	kClientReplayRecordRequest,   // Client requests server replay record start/stop (debug only)
	kClientReplayPlaybackRequest, // Client requests server replay playback (debug only)
	kClientPauseRequest,          // Client requests server pause/unpause (debug only)
	kClientTimespeedRequest,      // Client requests timescale change (debug only)
};

// Game-range contracts are checked once before dispatch in ServerSession::ParseReceivedGamePackets.
// Sizes include the type byte; Server::Receive strips it, and Server::AdmitGamePacket checks payload size + 1.
// A zero iMaximumSize marks server-to-client or unknown types as not client-sendable: a contract violation.
constexpr engine::ClientPacketContract GetGamePacketContract(GamePacketType eType)
{
	switch (eType)
	{
		case GamePacketType::kClientUpdatePlayerRequest: return {.iMinimumSize = 14, .iMaximumSize = 14, .iMaximumPerTick = 8};   // 8B globalId + 1B bUseMissiles + 4B navDelay + 1B type
		case GamePacketType::kClientFleetNavigationDelay: return {.iMinimumSize = 21, .iMaximumSize = 21, .iMaximumPerTick = 8};  // 16B fleetGuid + 4B delay + 1B type (edge-triggered send)
		case GamePacketType::kClientCreateFleetRequest: return {.iMinimumSize = 1, .iMaximumSize = 1, .iMaximumPerTick = 4};      // 1B type only
		case GamePacketType::kClientDeleteFleetRequest: return {.iMinimumSize = 17, .iMaximumSize = 17, .iMaximumPerTick = 8};    // 16B fleetGuid + 1B type
		case GamePacketType::kClientSpawnIntoFleetRequest: return {.iMinimumSize = 17, .iMaximumSize = 17, .iMaximumPerTick = 8}; // 16B fleetGuid + 1B type
		case GamePacketType::kClientRespawnInFleetRequest: return {.iMinimumSize = 25, .iMaximumSize = 25, .iMaximumPerTick = 16}; // 16B fleetGuid + 8B member globalId + 1B type

		// With kbDebugInput false, debug-control requests return the non-sendable sentinel, preventing handshaken clients from resetting, pausing, or changing server speed.
		case GamePacketType::kClientSaveRequest:
		case GamePacketType::kClientLoadRequest:
		case GamePacketType::kClientResetRequest:
		case GamePacketType::kClientReplayRecordRequest:
		case GamePacketType::kClientReplayPlaybackRequest:
			if constexpr (kbDebugInput)
			{
				return {.iMinimumSize = 1, .iMaximumSize = 1, .iMaximumPerTick = 2}; // 1B type only
			}
			else
			{
				return {};
			}
		case GamePacketType::kClientPauseRequest:
			if constexpr (kbDebugInput)
			{
				return {.iMinimumSize = 2, .iMaximumSize = 2, .iMaximumPerTick = 4}; // 1B paused + 1B type
			}
			else
			{
				return {};
			}
		case GamePacketType::kClientTimespeedRequest:
			if constexpr (kbDebugInput)
			{
				return {.iMinimumSize = 2, .iMaximumSize = 2, .iMaximumPerTick = 8}; // 1B direction + 1B type
			}
			else
			{
				return {};
			}

		default: return {};
	}
}

} // namespace game
