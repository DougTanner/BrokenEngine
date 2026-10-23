#pragma once

#if defined(BT_SERVER)

#include "Frame/GridCoord.h"

namespace game
{

struct StatusChange;

} // namespace game

namespace engine
{

enum class SubscriptionFlags : uint8_t
{
	kActive            = 1ui32 << 0,
	kFirstUpdateLogged = 1ui32 << 1,
};

struct ClientCoordSubscription
{
	GridCoord coordinate {};
	common::Flags<SubscriptionFlags> flags;
};

struct PendingNewSubscription
{
	int64_t iClientId = 0;
	int64_t iSlot = 0;
	GridCoord coordinate {};
	// The static-data packet ENet still holds, nullptr when none is in flight. ENet owns and frees it, so it is only
	// compared, never dereferenced.
	ENetPacket* pStaticDataPacket = nullptr;
	bool bStaticDataDelivered = false;
};

struct GridUpdateData
{
	common::crc_t uiSharedCrc = 0;
	std::span<const game::StatusChange> statusChanges;
};

} // namespace engine

#endif // BT_SERVER
