#pragma once

// Direct include, not the Engine.h aggregation: the game PCH pulls this header in ahead of Engine.h.
#include "Network/NetworkProtocol.h" // ClientGuid

namespace game
{

// Underlying values are serialized wire/save bytes — enumerators are append-only; never reorder or insert.
enum class StatusChangeType : uint8_t
{
	kSpawnOmni,
	kTransferOmni,
	kTransferSpaceship,
	kTransferBlaster,
	kTransferMissile,
	kDestroyOmni,
	kUpdateOmni,
	kUpdateFleet,

	kCount,
};

// A deserialized type tag is a valid enumerator only in [0, kCount). The replay
// FrameInput reader uses it to reject an unknown tag before it seats the wrong variant alternative.
inline constexpr bool IsKnownStatusChangeType(StatusChangeType eType)
{
	return eType < StatusChangeType::kCount;
}

inline constexpr bool IsTransferType(StatusChangeType eType)
{
	return eType >= StatusChangeType::kTransferOmni && eType <= StatusChangeType::kTransferMissile;
}

static_assert(static_cast<uint8_t>(StatusChangeType::kTransferSpaceship) == static_cast<uint8_t>(StatusChangeType::kTransferOmni) + 1 && static_cast<uint8_t>(StatusChangeType::kTransferBlaster) == static_cast<uint8_t>(StatusChangeType::kTransferOmni) + 2 && static_cast<uint8_t>(StatusChangeType::kTransferMissile) == static_cast<uint8_t>(StatusChangeType::kTransferOmni) + 3, "IsTransferType assumes kTransferOmni..kTransferMissile stay contiguous; inserting an enumerator between them breaks the range check.");

// The transfer type whose rows carry the owned-entity identity Frame::OwnershipLayer binds.
inline constexpr StatusChangeType kOwnedEntityTransferType = StatusChangeType::kTransferOmni;

inline const char* StatusChangeTypeName(StatusChangeType eType)
{
	switch (eType)
	{
		case StatusChangeType::kSpawnOmni:         return "SpawnOmni";
		case StatusChangeType::kTransferOmni:      return "TransferOmni";
		case StatusChangeType::kTransferSpaceship: return "TransferSpaceship";
		case StatusChangeType::kTransferBlaster:   return "TransferBlaster";
		case StatusChangeType::kTransferMissile:   return "TransferMissile";
		case StatusChangeType::kDestroyOmni:       return "DestroyOmni";
		case StatusChangeType::kUpdateOmni:        return "UpdateOmni";
		case StatusChangeType::kUpdateFleet:       return "UpdateFleet";
		case StatusChangeType::kCount:             break;
	}
	return "Unknown";
}

struct SpawnOmniData
{
	int64_t iGlobalId = 0;
	bool bIsFlagship = false;
	engine::GridCoord fleetWantedCoordinate {};
	uint8_t uiPendingFleetWantedCoordinateTicks = 0;
	// Spawn point in meters from the target cell's center.
	float fSpawnOffsetX = 45.0f;
	float fSpawnOffsetY = -12.0f;
	// Owning client GUID, so a requested omni row is born owned (not serialized over network, like the transfer payload's GUID).
	engine::ClientGuid clientGuid {};
	bool operator==(const SpawnOmniData&) const = default;
};

struct DestroyOmniData
{
	int64_t iOmniUuid = 0;
	bool operator==(const DestroyOmniData&) const = default;
};

struct UpdateOmniData
{
	int64_t iOmniUuid = 0;
	bool bUseMissiles = false;
	std::chrono::duration<float> navigationDelaySeconds = std::chrono::duration<float>(60.0f);
	uint8_t uiPendingWeaponModeTicks = 0;
	bool operator==(const UpdateOmniData&) const = default;
};

struct UpdateFleetData
{
	int64_t iOmniUuid = 0;
	bool bIsFlagship = false;
	engine::GridCoord fleetWantedCoordinate {};
	uint8_t uiPendingFleetWantedCoordinateTicks = 0;
	bool operator==(const UpdateFleetData&) const = default;
};

struct TransferData
{
	auto SharedMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.vecPosition, rSelf.vecDirection, rSelf.vecVelocity, rSelf.alignment, rSelf.fHealth, rSelf.fShield, rSelf.uiTypeIndex, rSelf.fAcceleration, rSelf.nextBlasterFireTimeSeconds, rSelf.nextSecondarySpawnTimeSeconds, rSelf.shieldCooldownSeconds, rSelf.shieldDownSoundCooldownSeconds, rSelf.animationTimeSeconds, rSelf.uiOmniFlags, rSelf.nextBlasterSpawnTimeSeconds, rSelf.navigationDelaySeconds, rSelf.deltaRotationDelaySeconds, rSelf.timeSeconds, rSelf.nextJitterSeconds, rSelf.fDeltaRotation, rSelf.fDeltaRotationMaximum, rSelf.fPitch, rSelf.globalId, rSelf.fleetWantedCoordinate, rSelf.uiPendingFleetWantedCoordinateTicks, rSelf.uiPendingWeaponModeTicks);
	}

	bool operator==(const TransferData& rOther) const
	{
		return SharedMembers() == rOther.SharedMembers();
	}

	XMVECTOR vecPosition = DirectX::XMVectorZero();
	XMVECTOR vecDirection = DirectX::XMVectorZero();
	XMVECTOR vecVelocity = DirectX::XMVectorZero();
	engine::AlignmentIdentifier alignment {};
	float fHealth = 0.0f;
	float fShield = 0.0f;
	uint8_t uiTypeIndex = 0;
	float fAcceleration = 0.0f;

	// Omni timers
	std::chrono::duration<float> nextBlasterFireTimeSeconds = std::chrono::duration<float>(0.0f);
	std::chrono::duration<float> nextSecondarySpawnTimeSeconds = std::chrono::duration<float>(0.0f);
	std::chrono::duration<float> shieldCooldownSeconds = std::chrono::duration<float>(0.0f);
	std::chrono::duration<float> shieldDownSoundCooldownSeconds = std::chrono::duration<float>(0.0f);

	// Omni interpolate state
	std::chrono::duration<float> animationTimeSeconds = std::chrono::duration<float>(0.0f);
	uint16_t uiOmniFlags = 0;

	// Spaceship timers
	std::chrono::duration<float> nextBlasterSpawnTimeSeconds = std::chrono::duration<float>(0.0f);

	// Navigation delay (omni transfers only)
	std::chrono::duration<float> navigationDelaySeconds = std::chrono::duration<float>(60.0f);

	// Missile timers
	std::chrono::duration<float> deltaRotationDelaySeconds = std::chrono::duration<float>(0.0f);
	std::chrono::duration<float> timeSeconds = std::chrono::duration<float>(0.0f);
	std::chrono::duration<float> nextJitterSeconds = std::chrono::duration<float>(0.0f);

	// Live turn rate (spaceship and missile transfers)
	float fDeltaRotation = 0.0f;
	float fDeltaRotationMaximum = 0.0f;
	float fPitch = 0.0f;

	// Global ID (omni transfers only)
	engine::GlobalId globalId {};

	// Fleet wanted coord (omni transfers only)
	engine::GridCoord fleetWantedCoordinate {};

	// Pending countdown ticks (omni transfers only)
	uint8_t uiPendingFleetWantedCoordinateTicks = 0;
	uint8_t uiPendingWeaponModeTicks = 0;

	// Client GUID (omni transfers only, not serialized over network)
	uint64_t uiClientGuidHigh = 0;
	uint64_t uiClientGuidLow = 0;
};

using StatusChangeData = std::variant<
	SpawnOmniData,             // kSpawnOmni
	TransferData,              // kTransfer* (all 4 types)
	DestroyOmniData,           // kDestroyOmni
	UpdateOmniData,            // kUpdateOmni
	UpdateFleetData    // kUpdateFleet
>;

inline StatusChangeData DefaultDataForType(StatusChangeType eType)
{
	switch (eType)
	{
		case StatusChangeType::kSpawnOmni:        return SpawnOmniData {};
		case StatusChangeType::kDestroyOmni:      return DestroyOmniData {};
		case StatusChangeType::kUpdateOmni:        return UpdateOmniData {};
		case StatusChangeType::kUpdateFleet:       return UpdateFleetData {};
		default:                                      return TransferData {};
	}
}

struct StatusChange
{
	bool operator==(const StatusChange&) const = default;

	StatusChangeType eType {};
	StatusChangeData data {};
};

} // namespace game
