#pragma once

namespace game
{

enum class GamePacketType : uint8_t
{
	kClientUpdateOwnedEntityRequest = static_cast<uint8_t>(engine::PacketType::kGamePacketStart),
	kClientCreateFleetRequest,
	kClientSpawnIntoFleetRequest,
	kClientRespawnInFleetRequest,
	kServerFleetSync,
	kClientDeleteFleetRequest,
	kClientFleetNavigationDelay,
};

// Game-range contracts are checked once before dispatch in ServerSessionBase::DispatchDeferredClientPackets.
// Sizes include the type byte; Server::Receive strips it, and Server::AdmitGamePacket checks payload size + 1.
// A zero iMaximumSize marks server-to-client or unknown types as not client-sendable: a contract violation.
constexpr engine::ClientPacketContract GetGamePacketContract(GamePacketType eType)
{
	switch (eType)
	{
		case GamePacketType::kClientUpdateOwnedEntityRequest: return {.iMinimumSize = 14, .iMaximumSize = 14, .iMaximumPerTick = 8}; // 8B globalId + 1B bUseMissiles + 4B navDelay + 1B type
		case GamePacketType::kClientFleetNavigationDelay: return {.iMinimumSize = 21, .iMaximumSize = 21, .iMaximumPerTick = 8};     // 16B fleetGuid + 4B delay + 1B type (edge-triggered send)
		case GamePacketType::kClientCreateFleetRequest: return {.iMinimumSize = 1, .iMaximumSize = 1, .iMaximumPerTick = 4};         // 1B type only
		case GamePacketType::kClientDeleteFleetRequest: return {.iMinimumSize = 17, .iMaximumSize = 17, .iMaximumPerTick = 8};       // 16B fleetGuid + 1B type
		case GamePacketType::kClientSpawnIntoFleetRequest: return {.iMinimumSize = 18, .iMaximumSize = 18, .iMaximumPerTick = 8};    // 16B fleetGuid + 1B ShipType + 1B type
		case GamePacketType::kClientRespawnInFleetRequest: return {.iMinimumSize = 25, .iMaximumSize = 25, .iMaximumPerTick = 16};   // 16B fleetGuid + 8B member globalId + 1B type

		default: return {};
	}
}

} // namespace game
