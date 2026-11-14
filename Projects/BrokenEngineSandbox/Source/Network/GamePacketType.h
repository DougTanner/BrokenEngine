#pragma once

namespace game
{

enum class GamePacketType : uint8_t
{
	kClientCreateFleetRequest = static_cast<uint8_t>(engine::PacketType::kGamePacketStart),
	kClientSpawnIntoFleetRequest,
	kServerFleetSync,
	kClientFleetNavigation,
	kClientDestroyFleetMemberRequest,
	kClientMapFleetMarkersRequest,
	kServerMapFleetMarkers,
	kClientSquadronOrdersRequest,
};

// Game-range contracts are checked once before dispatch in ServerSessionBase::DispatchDeferredClientPackets.
// Sizes include the type byte; Server::Receive strips it, and Server::AdmitGamePacket checks payload size + 1.
// A zero iMaximumSize marks server-to-client or unknown types as not client-sendable: a contract violation.
constexpr engine::ClientPacketContract GetGamePacketContract(GamePacketType eType)
{
	switch (eType)
	{
		case GamePacketType::kClientFleetNavigation: return {.iMinimumSize = GameMessages::FleetNavigationRequestMessage::kiSize + 1, .iMaximumSize = GameMessages::FleetNavigationRequestMessage::kiSize + 1, .iMaximumPerTick = 8}; // edge-triggered send
		case GamePacketType::kClientCreateFleetRequest: return {.iMinimumSize = GameMessages::CreateFleetRequestMessage::kiSize + 1, .iMaximumSize = GameMessages::CreateFleetRequestMessage::kiSize + 1, .iMaximumPerTick = 4};
		case GamePacketType::kClientSpawnIntoFleetRequest: return {.iMinimumSize = GameMessages::SpawnIntoFleetRequestMessage::kiSize + 1, .iMaximumSize = GameMessages::SpawnIntoFleetRequestMessage::kiSize + 1, .iMaximumPerTick = 8};
		case GamePacketType::kClientDestroyFleetMemberRequest: return {.iMinimumSize = GameMessages::DestroyFleetMemberRequestMessage::kiSize + 1, .iMaximumSize = GameMessages::DestroyFleetMemberRequestMessage::kiSize + 1, .iMaximumPerTick = 8};
		// Closing and reopening the map within one tick sends two.
		case GamePacketType::kClientMapFleetMarkersRequest: return {.iMinimumSize = GameMessages::MapFleetMarkersRequestMessage::kiSize + 1, .iMaximumSize = GameMessages::MapFleetMarkersRequestMessage::kiSize + 1, .iMaximumPerTick = 2};
		case GamePacketType::kClientSquadronOrdersRequest: return {.iMinimumSize = GameMessages::SquadronOrdersRequestMessage::kiSize + 1, .iMaximumSize = GameMessages::SquadronOrdersRequestMessage::kiSize + 1, .iMaximumPerTick = 8};

		default: return {};
	}
}

} // namespace game
