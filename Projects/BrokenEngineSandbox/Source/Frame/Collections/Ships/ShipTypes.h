#pragma once

namespace game
{

// Enumerator order is the frame tuple order, the ForEachShipType order, the registry source-layer order, and the
// OwnershipLayers order. Serialized as one byte in the spawn request, FleetSync, the fleet save, and the tick-input
// payloads.
enum class ShipType : uint8_t
{
	kFighter,
	kOmni,
	kBattleship,

	kCount,
};

// Harness token, the enumerator name without the k.
constexpr const char* ShipTypeName(ShipType eType)
{
	switch (eType)
	{
		case ShipType::kFighter:    return "Fighter";
		case ShipType::kOmni:       return "Omni";
		case ShipType::kBattleship: return "Battleship";
		case ShipType::kCount:      break;
	}
	return "Unknown";
}

// CRC'd per-row column: which AI drives the row, and which of the type's two models the client draws.
enum class ShipController : uint8_t
{
	kFleet,
	kRandomSpawn,

	kCount,
};

// Harness token, the enumerator name without the k.
constexpr const char* ShipControllerName(ShipController eController)
{
	switch (eController)
	{
		case ShipController::kFleet:       return "Fleet";
		case ShipController::kRandomSpawn: return "RandomSpawn";
		case ShipController::kCount:       break;
	}
	return "Unknown";
}

// Set by controllers (the fleet path through the spawn and kUpdateShip, and the random-wave spawn); kUpdateShip also
// writes the target flight plane. CountdownPendingOrders also applies kPendingUseMissiles to kUseMissiles when the
// weapon-mode countdown ends.
enum class ShipOrders : uint16_t
{
	kIsFlagship         = 0x0001,
	kUseMissiles        = 0x0002,
	kPendingUseMissiles = 0x0004,

	// Target flight plane index in bits 8-15 (0 = lowest)
};
using ShipOrders_t = common::Flags<ShipOrders>;

inline constexpr int64_t GetTargetFlightPlane(ShipOrders_t orders)
{
	return static_cast<int64_t>((std::to_underlying(orders.meFlags) >> 8) & 0xFF);
}

inline void SetTargetFlightPlane(ShipOrders_t& rOrders, int64_t iFlightPlane)
{
	uint16_t uiRaw = std::to_underlying(rOrders.meFlags);
	uiRaw = static_cast<uint16_t>((uiRaw & ~0xFF00) | (static_cast<uint16_t>(iFlightPlane) << 8));
	rOrders.meFlags = static_cast<ShipOrders>(uiRaw);
}

// Ship-internal state written by the ship's own phases.
enum class ShipFlags : uint16_t
{
	kFireBlaster          = 0x0004,
	kBlasterSpawnLeft     = 0x0008,
	kFireMissile          = 0x0010,
	kMissileSpawnLeft     = 0x0020,
	kFlee                 = 0x0040,
	kReturnToIslandCenter = 0x0080,

	// Nav direction in bits 8-11 (4 bits, stored as value+1: 0-8 representing -1 to 7): -1 roaming, 0-5 the
	// engine::kCellDirections index of the neighbour headed for, 6 island tour, 7 follow flagship
	kNavigationDirectionBit0 = 0x0100,
	kNavigationDirectionBit1 = 0x0200,
	kNavigationDirectionBit2 = 0x0400,
	kNavigationDirectionBit3 = 0x0800,

	// Nav waypoint index in bits 12-13 (2 bits): 0 = largest island, 1 = smallest, 2 = random (saturates at 2)
	kNavigationWaypointBit0 = 0x1000,
	kNavigationWaypointBit1 = 0x2000,
};
using ShipFlags_t = common::Flags<ShipFlags>;

inline constexpr int64_t GetNavigationDirection(ShipFlags_t flags)
{
	return static_cast<int64_t>(((std::to_underlying(flags.meFlags) >> 8) & 0xF) - 1);
}

inline void SetNavigationDirection(ShipFlags_t& rFlags, int64_t iDirection)
{
	uint16_t uiRaw = std::to_underlying(rFlags.meFlags);
	uiRaw = static_cast<uint16_t>((uiRaw & ~0x0F00) | (static_cast<uint16_t>(iDirection + 1) << 8));
	rFlags.meFlags = static_cast<ShipFlags>(uiRaw);
}

inline constexpr int64_t GetNavigationWaypointIndex(ShipFlags_t flags)
{
	return static_cast<int64_t>((std::to_underlying(flags.meFlags) >> 12) & 0x3);
}

inline void SetNavigationWaypointIndex(ShipFlags_t& rFlags, int64_t iIndex)
{
	uint16_t uiRaw = std::to_underlying(rFlags.meFlags);
	uiRaw = static_cast<uint16_t>((uiRaw & ~0x3000) | (static_cast<uint16_t>(iIndex) << 12));
	rFlags.meFlags = static_cast<ShipFlags>(uiRaw);
}

// Row address across the three ship collections, valid for one frame and never across a tick or a transfer.
struct ShipLocator
{
	ShipType eType {};
	int64_t iRow = 0;
};

} // namespace game
