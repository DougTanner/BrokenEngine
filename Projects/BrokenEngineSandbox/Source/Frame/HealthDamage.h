#pragma once

#include "Frame/Alignments.h"

namespace game
{

enum Damages
{
	kDamageSpaceshipBlaster,
	kDamageSpaceshipCollision,

	kDamagesCount,
};

inline constexpr float kppfDamages[kDamagesCount][3] =
{
	{2.0f, 3.0f, 4.0f}, // kDamageSpaceshipBlaster
	{5.0f, 10.0f, 15.0f}, // kDamageSpaceshipCollision
};

inline constexpr float kfPlayerArmor = 50.0f;
inline constexpr float kfPlayerShield = 100.0f;
inline constexpr float kfPlayerShieldRegeneration = 5.0f;

inline constexpr float kfPlayerEnergy = 25.0f;

inline constexpr float kfSpaceshipArmorShardChance = 0.1f;

// Arrival grace period: arriving entities are skipped in targeting/behavior scans (not collision)
inline constexpr std::chrono::duration<float> kArrivalGracePeriod(1.0f);

inline constexpr float kfBlasterDamage = 6.0f;
inline constexpr float kfMissileCollisionRadius = 0.5f;
inline constexpr float kfMissileDamageRadius = 7.0f;
inline constexpr float kfMissileDamage = 30.0f;
inline constexpr std::chrono::duration<float> kMissileLifetime(10.0f);
inline constexpr float kfMissileGravity = 9.8f;

inline constexpr float kfSpaceshipHealth = 10.0f;
inline constexpr float kfSpaceshipCollisionDamage = 5.0f;

namespace CollisionCategory
{
	inline constexpr uint16_t kuiNone      = 0x0000;
	inline constexpr uint16_t kuiBlaster   = 0x0001;
	inline constexpr uint16_t kuiSpaceship = 0x0004;
	inline constexpr uint16_t kuiPlayer    = 0x0008;
	inline constexpr uint16_t kuiMissile   = 0x0010;
} // namespace CollisionCategory

namespace CollidesWith
{
	inline constexpr uint16_t kuiNone = CollisionCategory::kuiNone;

	// Alignment filters same-team collisions.
	inline constexpr uint16_t kuiBlaster = CollisionCategory::kuiSpaceship | CollisionCategory::kuiPlayer;

	inline constexpr uint16_t kuiMissile = CollisionCategory::kuiSpaceship;

	inline constexpr uint16_t kuiSpaceship = CollisionCategory::kuiPlayer | CollisionCategory::kuiBlaster | CollisionCategory::kuiMissile;

	inline constexpr uint16_t kuiPlayer = CollisionCategory::kuiSpaceship | CollisionCategory::kuiBlaster;
} // namespace CollidesWith

} // namespace game
