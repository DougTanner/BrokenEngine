#pragma once

#include "Frame/Alignments.h"

namespace game
{

inline constexpr float kfOmniArmor = 50.0f;
inline constexpr float kfOmniShield = 100.0f;
inline constexpr float kfOmniShieldRegeneration = 5.0f;

inline constexpr float kfOmniEnergy = 25.0f;

// Arrival grace period: arriving entities are skipped in targeting/behavior scans (not collision)
inline constexpr std::chrono::duration<float> kArrivalGracePeriod(1.0f);

inline constexpr float kfBlasterDamage = 6.0f;
inline constexpr float kfMissileCollisionRadius = 0.5f;
inline constexpr float kfMissileDamageRadius = 7.0f;
inline constexpr float kfMissileDamage = 30.0f;
inline constexpr std::chrono::duration<float> kMissileLifetime(10.0f);
inline constexpr float kfMissileGravity = 9.8f;

inline constexpr float kfFighterHealth = 10.0f;
inline constexpr float kfBattleshipHealth = 60.0f;
// Every ship takes this from a ship collision, whatever the other ship's type; a contact with a spawning Fighter
// (ShipOrders::kSpawning) deals none to either side.
inline constexpr float kfShipCollisionDamage = 5.0f;

namespace CollisionCategory
{
inline constexpr uint16_t kuiNone       = 0x0000;
inline constexpr uint16_t kuiBlaster    = 0x0001;
inline constexpr uint16_t kuiFighter    = 0x0002;
inline constexpr uint16_t kuiOmni       = 0x0004;
inline constexpr uint16_t kuiBattleship = 0x0008;
inline constexpr uint16_t kuiMissile    = 0x0010;

inline constexpr uint16_t kuiShips = kuiFighter | kuiOmni | kuiBattleship;
} // namespace CollisionCategory

namespace CollidesWith
{
inline constexpr uint16_t kuiNone = CollisionCategory::kuiNone;

// Alignment filters same-team collisions.
inline constexpr uint16_t kuiBlaster = CollisionCategory::kuiShips;

inline constexpr uint16_t kuiMissile = CollisionCategory::kuiShips;

// No type collides with itself; Pushers keep same-type ships apart.
inline constexpr uint16_t kuiFighter = CollisionCategory::kuiOmni | CollisionCategory::kuiBattleship | CollisionCategory::kuiBlaster | CollisionCategory::kuiMissile;

inline constexpr uint16_t kuiOmni = CollisionCategory::kuiFighter | CollisionCategory::kuiBattleship | CollisionCategory::kuiBlaster | CollisionCategory::kuiMissile;

inline constexpr uint16_t kuiBattleship = CollisionCategory::kuiFighter | CollisionCategory::kuiOmni | CollisionCategory::kuiBlaster | CollisionCategory::kuiMissile;
} // namespace CollidesWith

// engine::AreaDamage categories of a missile blast, tagged by the side that fired it. They share no space with
// CollisionCategory: only the missile explosion registers a blast.
namespace BlastCategory
{
inline constexpr uint16_t kuiFleetFired  = 0x0001;
inline constexpr uint16_t kuiRandomFired = 0x0002;
} // namespace BlastCategory

inline constexpr uint16_t FiredBlastCategory(engine::AlignmentIdentifier alignment, engine::AlignmentIdentifier fleetAlignment)
{
	return alignment == fleetAlignment ? BlastCategory::kuiFleetFired : BlastCategory::kuiRandomFired;
}

// A blast hurts only the side opposing the one that fired it.
inline constexpr uint16_t OpposingBlastCategory(engine::AlignmentIdentifier alignment, engine::AlignmentIdentifier fleetAlignment)
{
	return alignment == fleetAlignment ? BlastCategory::kuiRandomFired : BlastCategory::kuiFleetFired;
}

} // namespace game
