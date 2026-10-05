#pragma once

#include "Network/NetworkCursor.h"

namespace engine
{

class NetworkManager
{
public:

	NetworkManager();
	~NetworkManager();

	// Channel 0: Control Reliable (handshake, subscribe/unsubscribe, spawn, assign, desync)
	// Channel 1: Control Unreliable (carries the client->server ACK stream)
	// Channel 2+: Coord slots (pairs of reliable/unreliable per slot)
	static constexpr int64_t kiChannelReliable = 0;
	static constexpr int64_t kiChannelUnreliable = 1;

	static constexpr int64_t kiMaximumEnetCoordinateSlots = 64;
	static constexpr int64_t kiChannelCount = 2 + kiMaximumEnetCoordinateSlots * 2;

	static constexpr int64_t CoordinateSlotReliable(int64_t iSlot)
	{
		return 2 + iSlot * 2;
	}
	static constexpr int64_t CoordinateSlotUnreliable(int64_t iSlot)
	{
		return 2 + iSlot * 2 + 1;
	}
	static constexpr int64_t ChannelToSlot(int64_t iChannel)
	{
		return (iChannel - 2) / 2;
	}
	static constexpr bool IsCoordinateChannel(int64_t iChannel)
	{
		return iChannel >= 2;
	}
	static constexpr bool IsUnreliableChannel(int64_t iChannel)
	{
		return iChannel == kiChannelUnreliable || (IsCoordinateChannel(iChannel) && (iChannel % 2) == 1);
	}

	static inline void SendPacket(ENetPeer* pPeer, int64_t iChannel, common::Workbuffer& rWorkbuffer, uint32_t uiFlags)
	{
		std::span<const uint8_t> packetSpan = rWorkbuffer.Span<uint8_t>();
		ScopedSuppressAllocationTracking suppress;
		// Heap: ENet allocates packet data internally
		ENetPacket* pPacket = enet_packet_create(packetSpan.data(), packetSpan.size(), uiFlags);
		if (enet_peer_send(pPeer, static_cast<enet_uint8>(iChannel), pPacket) < 0)
		{
			// enet_peer_send does not take ownership of the packet on failure — free it to avoid a leak.
			enet_packet_destroy(pPacket);
		}
	}

	template <typename TTYPE, typename... TARGS>
	static void SendSimplePacket(ENetPeer* pPeer, TTYPE eType, int64_t iChannel, uint32_t uiFlags, const TARGS&... rArguments)
	{
		static_assert(std::is_enum_v<TTYPE>, "SendSimplePacket type tag must be an enum (engine::PacketType or game::GamePacketType)");

		common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
		common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();

		rWorkbuffer.PushBack<uint8_t>(static_cast<uint8_t>(eType));
		(PushSimplePacketArgument(rWorkbuffer, rArguments), ...);

		SendPacket(pPeer, iChannel, rWorkbuffer, uiFlags);
	}
};

} // namespace engine
