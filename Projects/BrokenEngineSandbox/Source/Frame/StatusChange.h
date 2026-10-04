#pragma once

// Direct include, not the Engine.h aggregation: the game PCH pulls this header in ahead of Engine.h.
#include "Network/NetworkProtocol.h" // ClientGuid

namespace game
{

// Underlying values are serialized wire/save bytes — enumerators are append-only; never reorder or insert.
enum class StatusChangeType : uint8_t
{
	kSpawnPlayer,
	kTransferPlayer,
	kTransferSpaceship,
	kTransferBlaster,
	kTransferMissile,
	kDestroyPlayer,
	kUpdatePlayer,
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
	return eType >= StatusChangeType::kTransferPlayer && eType <= StatusChangeType::kTransferMissile;
}

static_assert(static_cast<uint8_t>(StatusChangeType::kTransferSpaceship) == static_cast<uint8_t>(StatusChangeType::kTransferPlayer) + 1 && static_cast<uint8_t>(StatusChangeType::kTransferBlaster) == static_cast<uint8_t>(StatusChangeType::kTransferPlayer) + 2 && static_cast<uint8_t>(StatusChangeType::kTransferMissile) == static_cast<uint8_t>(StatusChangeType::kTransferPlayer) + 3, "IsTransferType assumes kTransferPlayer..kTransferMissile stay contiguous; inserting an enumerator between them breaks the range check.");

inline const char* StatusChangeTypeName(StatusChangeType eType)
{
	switch (eType)
	{
		case StatusChangeType::kSpawnPlayer:       return "SpawnPlayer";
		case StatusChangeType::kTransferPlayer:    return "TransferPlayer";
		case StatusChangeType::kTransferSpaceship: return "TransferSpaceship";
		case StatusChangeType::kTransferBlaster:   return "TransferBlaster";
		case StatusChangeType::kTransferMissile:   return "TransferMissile";
		case StatusChangeType::kDestroyPlayer:     return "DestroyPlayer";
		case StatusChangeType::kUpdatePlayer:      return "UpdatePlayer";
		case StatusChangeType::kUpdateFleet:       return "UpdateFleet";
		case StatusChangeType::kCount:             break;
	}
	return "Unknown";
}

struct SpawnPlayerData
{
	int64_t iGlobalId = 0;
	bool bIsFlagship = false;
	engine::GridCoord fleetWantedCoordinate {};
	uint8_t uiPendingFleetWantedCoordinateTicks = 0;
	// Spawn point in meters from the target cell's center.
	float fSpawnOffsetX = 45.0f;
	float fSpawnOffsetY = -12.0f;
	// Owning client GUID, so a requested player row is born owned (not serialized over network, like the transfer payload's GUID).
	engine::ClientGuid clientGuid {};
	bool operator==(const SpawnPlayerData&) const = default;
};

struct DestroyPlayerData
{
	int64_t iPlayerUuid = 0;
	bool operator==(const DestroyPlayerData&) const = default;
};

struct UpdatePlayerData
{
	int64_t iPlayerUuid = 0;
	bool bUseMissiles = false;
	std::chrono::duration<float> navigationDelaySeconds = std::chrono::duration<float>(60.0f);
	uint8_t uiPendingWeaponModeTicks = 0;
	bool operator==(const UpdatePlayerData&) const = default;
};

struct UpdateFleetData
{
	int64_t iPlayerUuid = 0;
	bool bIsFlagship = false;
	engine::GridCoord fleetWantedCoordinate {};
	uint8_t uiPendingFleetWantedCoordinateTicks = 0;
	bool operator==(const UpdateFleetData&) const = default;
};

struct TransferData
{
	auto SharedMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.vecPosition, rSelf.vecDirection, rSelf.vecVelocity, rSelf.alignment, rSelf.fHealth, rSelf.fShield, rSelf.uiTypeIndex, rSelf.fAcceleration, rSelf.nextBlasterFireTimeSeconds, rSelf.nextSecondarySpawnTimeSeconds, rSelf.shieldCooldownSeconds, rSelf.shieldDownSoundCooldownSeconds, rSelf.animationTimeSeconds, rSelf.uiPlayerFlags, rSelf.nextBlasterSpawnTimeSeconds, rSelf.navigationDelaySeconds, rSelf.deltaRotationDelaySeconds, rSelf.timeSeconds, rSelf.nextJitterSeconds, rSelf.fDeltaRotation, rSelf.fDeltaRotationMaximum, rSelf.fPitch, rSelf.globalPlayerId, rSelf.fleetWantedCoordinate, rSelf.uiPendingFleetWantedCoordinateTicks, rSelf.uiPendingWeaponModeTicks);
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

	// Player timers
	std::chrono::duration<float> nextBlasterFireTimeSeconds = std::chrono::duration<float>(0.0f);
	std::chrono::duration<float> nextSecondarySpawnTimeSeconds = std::chrono::duration<float>(0.0f);
	std::chrono::duration<float> shieldCooldownSeconds = std::chrono::duration<float>(0.0f);
	std::chrono::duration<float> shieldDownSoundCooldownSeconds = std::chrono::duration<float>(0.0f);

	// Player interpolate state
	std::chrono::duration<float> animationTimeSeconds = std::chrono::duration<float>(0.0f);
	uint16_t uiPlayerFlags = 0;

	// Spaceship timers
	std::chrono::duration<float> nextBlasterSpawnTimeSeconds = std::chrono::duration<float>(0.0f);

	// Navigation delay (player transfers only)
	std::chrono::duration<float> navigationDelaySeconds = std::chrono::duration<float>(60.0f);

	// Missile timers
	std::chrono::duration<float> deltaRotationDelaySeconds = std::chrono::duration<float>(0.0f);
	std::chrono::duration<float> timeSeconds = std::chrono::duration<float>(0.0f);
	std::chrono::duration<float> nextJitterSeconds = std::chrono::duration<float>(0.0f);

	// Live turn rate (spaceship and missile transfers)
	float fDeltaRotation = 0.0f;
	float fDeltaRotationMaximum = 0.0f;
	float fPitch = 0.0f;

	// Global player ID (player transfers only)
	engine::GlobalId globalPlayerId {};

	// Fleet wanted coord (player transfers only)
	engine::GridCoord fleetWantedCoordinate {};

	// Pending countdown ticks (player transfers only)
	uint8_t uiPendingFleetWantedCoordinateTicks = 0;
	uint8_t uiPendingWeaponModeTicks = 0;

	// Client GUID (player transfers only, not serialized over network)
	uint64_t uiClientGuidHigh = 0;
	uint64_t uiClientGuidLow = 0;
};

using StatusChangeData = std::variant<
	SpawnPlayerData,           // kSpawnPlayer
	TransferData,              // kTransfer* (all 4 types)
	DestroyPlayerData,         // kDestroyPlayer
	UpdatePlayerData,          // kUpdatePlayer
	UpdateFleetData    // kUpdateFleet
>;

inline StatusChangeData DefaultDataForType(StatusChangeType eType)
{
	switch (eType)
	{
		case StatusChangeType::kSpawnPlayer:      return SpawnPlayerData {};
		case StatusChangeType::kDestroyPlayer:    return DestroyPlayerData {};
		case StatusChangeType::kUpdatePlayer:      return UpdatePlayerData {};
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
