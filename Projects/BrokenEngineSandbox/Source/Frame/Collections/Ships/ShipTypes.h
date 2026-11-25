#pragma once

namespace game
{

// Enumerator order is the frame tuple order, the ForEachShipType order, and the OwnershipLayers order. Serialized as one
// byte in the spawn request, FleetSync, the fleet save, and the tick-input payloads.
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

// Set by controllers (the fleet path through the spawn, kUpdateShip, and kUpdateSquadronOrders, and the random-wave
// spawn).
enum class ShipOrders : uint16_t
{
	kIsFlagship         = 0x0001,
	// A random-spawn row that chases and prefers to shoot the nearest enemy flagship in its cell.
	kTargetFlagship     = 0x0002,
	// Fleet Fighter and Omni rows only. Set when the row's fleet member is in squadron 1, clear for squadron 0.
	kSquadron1          = 0x0004,
	// Fleet Fighter rows only: the squadron's settings (Fleet::pSquadronOrders, Fleet.h).
	kSeekAndDestroy     = 0x0008,
	kSharedTarget       = 0x0010,
	// Fleet Omni rows only: the squadron's settings (Fleet::pSquadronOrders, Fleet.h).
	kPatrolCell         = 0x0020,
	kPatrolTogether     = 0x0040,
};
using ShipOrders_t = common::Flags<ShipOrders>;

// Every squadron setting bit, the ones kUpdateSquadronOrders replaces.
inline constexpr ShipOrders_t kSquadronOrderBits {ShipOrders::kSeekAndDestroy, ShipOrders::kSharedTarget, ShipOrders::kPatrolCell, ShipOrders::kPatrolTogether};

// Ship-internal state written by the ship's own phases.
enum class ShipFlags : uint16_t
{
	kVeerOff              = 0x0001,
	// A fleet Omni at zero armor: it holds no fire and patrols until healed back to full armor.
	kHealing              = 0x0002,
	kFireBlaster          = 0x0004,
	kBlasterSpawnLeft     = 0x0008,
	// The AI direction is a kept value, not one the ship wants, so the Fighter and Battleship heading capture and the Omni thrust ignore it.
	kNoAiDirection        = 0x0010,
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

// Tuning for ApplyAcceleratedMovement, instantiated as kOmniMovement in Omnis.h.
struct AcceleratedMovementConstants
{
	float fAcceleration = 0.0f;
	float fCatchUpAcceleration = 0.0f;
	float fDrag = 0.0f;
	float fMaximumSpeed = 0.0f;
	float fCatchUpMaximumSpeed = 0.0f;
};

// Tuning for ApplyHeadingMovement and the ramped turn (RampHeadingTurnRate, SteerHeading, CaptureHeading), instantiated in
// Fighters.h and Battleships.h.
struct HeadingMovementConstants
{
	float fChaseAcceleration = 0.0f;
	float fReturnAcceleration = 0.0f;
	float fDrag = 0.0f;
	float fMaximumSpeed = 0.0f;
	// Velocity-to-direction blend (airplane constraint)
	float fVelocityToDirection = 0.0f;
	float fMaxTurnRate = 0.0f;
	float fTurnAcceleration = 0.0f;
	// Within this angle of its wanted heading a ship stops turning and Interpolate Update rotates it the rest of the way.
	float fCaptureAngle = 0.0f;
	float fCaptureRate = 0.0f;
};

// Every ship type climbs at this fraction of its maximum speed.
inline constexpr float kfShipClimbSpeedFraction = 0.25f;

// Firing and target picking test line of sight on this clearance tier's graphs for every shooter; movement decisions use
// the ship type's own tier (ShipTypeClearanceTier, TerrainUtils.h).
inline constexpr int64_t kiFiringClearanceTier = 0;

// The admitted navigation-delay range in seconds, which the HUD slider also spans.
inline constexpr float kfMinimumNavigationDelaySeconds = 0.0f;
inline constexpr float kfMaximumNavigationDelaySeconds = 60.0f;

// Written as one range test so NaN and both infinities fail it too.
[[nodiscard]] constexpr bool IsNavigationDelayInRange(float fDelay)
{
	return fDelay >= kfMinimumNavigationDelaySeconds && fDelay <= kfMaximumNavigationDelaySeconds;
}

// A ship heading or AI direction has Z and W exactly 0 and an x/y length squared in [0.25, 4]: a coarse corruption test
// far wider than the rounding of the unit vectors every simulation write and ship spawn stores, and the only bound on
// an edit_frame write. The length is one range test so NaN and both infinities in any lane fail it.
[[nodiscard]] inline bool XM_CALLCONV IsShipHeadingValid(FXMVECTOR vecDirection)
{
	float fLengthSquared = XMVectorGetX(XMVector2LengthSq(vecDirection));
	return XMVectorGetZ(vecDirection) == 0.0f && XMVectorGetW(vecDirection) == 0.0f && fLengthSquared >= 0.25f && fLengthSquared <= 4.0f;
}

} // namespace game
