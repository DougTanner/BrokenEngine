#pragma once

// Direct includes, not the Engine.h aggregation: the game PCH pulls this header in ahead of Engine.h.
#include "Frame/Collections/Explosions/Explosions.h"
#include "Network/NetworkProtocol.h" // ClientGuid

#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Omnis/Omnis.h"
#include "Frame/Collections/Spaceships/Spaceships.h"

namespace game
{

// Underlying values are serialized wire, replay, and save bytes — enumerators are append-only; never reorder or insert.
enum class StatusChangeType : uint8_t
{
	// Creations decided outside the tick and cross-cell arrivals: the post-tick batch, applied to the destination right
	// after its tick; never in server tick input.
	kSpawnOmni,
	kSpawnSpaceship,
	kSpawnBlaster,
	kSpawnMissile,
	kSpawnExplosion,
	kTransferOmni,
	kTransferSpaceship,
	kTransferBlaster,
	kTransferMissile,
	// Tick input.
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

inline constexpr bool IsSpawnType(StatusChangeType eType)
{
	return eType >= StatusChangeType::kSpawnOmni && eType <= StatusChangeType::kSpawnExplosion;
}

inline constexpr bool IsTransferType(StatusChangeType eType)
{
	return eType >= StatusChangeType::kTransferOmni && eType <= StatusChangeType::kTransferMissile;
}

// The post-tick batch: every spawn and transfer tag.
inline constexpr bool IsPostDispatchType(StatusChangeType eType)
{
	return eType >= StatusChangeType::kSpawnOmni && eType <= StatusChangeType::kTransferMissile;
}

static_assert(std::to_underlying(StatusChangeType::kSpawnExplosion) == std::to_underlying(StatusChangeType::kSpawnOmni) + 4 && std::to_underlying(StatusChangeType::kTransferOmni) == std::to_underlying(StatusChangeType::kSpawnExplosion) + 1 && std::to_underlying(StatusChangeType::kTransferMissile) == std::to_underlying(StatusChangeType::kTransferOmni) + 3, "IsSpawnType, IsTransferType, and IsPostDispatchType assume kSpawnOmni..kSpawnExplosion and kTransferOmni..kTransferMissile stay contiguous and adjacent; inserting an enumerator inside them breaks the range checks.");

// The transfer type whose rows carry the owned-entity identity Frame::OwnershipLayer binds.
inline constexpr StatusChangeType kOwnedEntityTransferType = StatusChangeType::kTransferOmni;

inline const char* StatusChangeTypeName(StatusChangeType eType)
{
	switch (eType)
	{
		case StatusChangeType::kSpawnOmni:         return "SpawnOmni";
		case StatusChangeType::kSpawnSpaceship:    return "SpawnSpaceship";
		case StatusChangeType::kSpawnBlaster:      return "SpawnBlaster";
		case StatusChangeType::kSpawnMissile:      return "SpawnMissile";
		case StatusChangeType::kSpawnExplosion:    return "SpawnExplosion";
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

struct DestroyOmniData
{
	int64_t iOmniUuid = 0;

	static constexpr auto Fields()
	{
		return engine::FrameColumnList<FRAME_COLUMN(DestroyOmniData, iOmniUuid)> {};
	}
};

struct UpdateOmniData
{
	int64_t iOmniUuid = 0;
	bool bUseMissiles = false;
	std::chrono::duration<float> navigationDelaySeconds = std::chrono::duration<float>(60.0f);
	uint8_t uiPendingWeaponModeTicks = 0;

	static constexpr auto Fields()
	{
		return engine::FrameColumnList<FRAME_COLUMN(UpdateOmniData, iOmniUuid), FRAME_COLUMN(UpdateOmniData, bUseMissiles), FRAME_COLUMN(UpdateOmniData, navigationDelaySeconds), FRAME_COLUMN(UpdateOmniData, uiPendingWeaponModeTicks)> {};
	}
};

struct UpdateFleetData
{
	int64_t iOmniUuid = 0;
	bool bIsFlagship = false;
	engine::GridCoord fleetWantedCoordinate {};
	uint8_t uiPendingFleetWantedCoordinateTicks = 0;

	static constexpr auto Fields()
	{
		return engine::FrameColumnList<FRAME_COLUMN(UpdateFleetData, iOmniUuid), FRAME_COLUMN(UpdateFleetData, bIsFlagship), FRAME_COLUMN(UpdateFleetData, fleetWantedCoordinate), FRAME_COLUMN(UpdateFleetData, uiPendingFleetWantedCoordinateTicks)> {};
	}
};

using StatusChangeData = std::variant<
	OmnisPostRender::SpawnInfo,
	SpaceshipsPostRender::SpawnInfo,
	BlastersPostRender::SpawnInfo,
	MissilesPostRender::SpawnInfo,
	engine::ExplosionsPostRender::SpawnInfo,
	DestroyOmniData,
	UpdateOmniData,
	UpdateFleetData>;

// A spawn payload's collection and tags. Pushers have no SpawnInfo and no specialization, so they cannot be
// queued, parsed, or applied from outside their owner's Spawn.
template <typename SPAWN_INFO>
struct SpawnTraits {};

template <>
struct SpawnTraits<OmnisPostRender::SpawnInfo>
{
	using Collection = OmnisPostRender;
	static constexpr StatusChangeType kSpawn = StatusChangeType::kSpawnOmni;
	static constexpr StatusChangeType kTransfer = StatusChangeType::kTransferOmni;
};

template <>
struct SpawnTraits<SpaceshipsPostRender::SpawnInfo>
{
	using Collection = SpaceshipsPostRender;
	static constexpr StatusChangeType kSpawn = StatusChangeType::kSpawnSpaceship;
	static constexpr StatusChangeType kTransfer = StatusChangeType::kTransferSpaceship;
};

template <>
struct SpawnTraits<BlastersPostRender::SpawnInfo>
{
	using Collection = BlastersPostRender;
	static constexpr StatusChangeType kSpawn = StatusChangeType::kSpawnBlaster;
	static constexpr StatusChangeType kTransfer = StatusChangeType::kTransferBlaster;
};

template <>
struct SpawnTraits<MissilesPostRender::SpawnInfo>
{
	using Collection = MissilesPostRender;
	static constexpr StatusChangeType kSpawn = StatusChangeType::kSpawnMissile;
	static constexpr StatusChangeType kTransfer = StatusChangeType::kTransferMissile;
};

template <>
struct SpawnTraits<engine::ExplosionsPostRender::SpawnInfo>
{
	using Collection = engine::ExplosionsPostRender;
	static constexpr StatusChangeType kSpawn = StatusChangeType::kSpawnExplosion;
};

static_assert(std::is_trivially_copyable_v<OmnisPostRender::SpawnInfo> && std::is_trivially_copyable_v<SpaceshipsPostRender::SpawnInfo> && std::is_trivially_copyable_v<BlastersPostRender::SpawnInfo> && std::is_trivially_copyable_v<MissilesPostRender::SpawnInfo> && std::is_trivially_copyable_v<engine::ExplosionsPostRender::SpawnInfo>, "Every SpawnInfo is a status-change payload the replay difference stream writes as a raw image.");

inline StatusChangeData DefaultDataForType(StatusChangeType eType)
{
	switch (eType)
	{
		case StatusChangeType::kSpawnOmni:
		case StatusChangeType::kTransferOmni:      return OmnisPostRender::SpawnInfo {};
		case StatusChangeType::kSpawnSpaceship:
		case StatusChangeType::kTransferSpaceship: return SpaceshipsPostRender::SpawnInfo {};
		case StatusChangeType::kSpawnBlaster:
		case StatusChangeType::kTransferBlaster:   return BlastersPostRender::SpawnInfo {};
		case StatusChangeType::kSpawnMissile:
		case StatusChangeType::kTransferMissile:   return MissilesPostRender::SpawnInfo {};
		case StatusChangeType::kSpawnExplosion:    return engine::ExplosionsPostRender::SpawnInfo {};
		case StatusChangeType::kDestroyOmni:       return DestroyOmniData {};
		case StatusChangeType::kUpdateOmni:        return UpdateOmniData {};
		case StatusChangeType::kUpdateFleet:       return UpdateFleetData {};
		case StatusChangeType::kCount:             break;
	}
	ASSERT(false);
	std::unreachable();
}

struct StatusChange
{
	StatusChangeType eType {};
	StatusChangeData data {};
};

template <typename SPAWN_INFO>
StatusChange MakeSpawn(const SPAWN_INFO& rInfo)
{
	return {.eType = SpawnTraits<SPAWN_INFO>::kSpawn, .data = rInfo};
}

template <typename SPAWN_INFO>
StatusChange MakeTransfer(const SPAWN_INFO& rInfo)
{
	return {.eType = SpawnTraits<SPAWN_INFO>::kTransfer, .data = rInfo};
}

// Payload accessors over spawn and transfer tags; the engine transfer and reconcile code calls them by name.
XMVECTOR SpawnPosition(const StatusChange& rChange);
// The position PrepareTransferRequest rebases into the destination cell.
XMVECTOR& SpawnPositionRef(StatusChange& rChange);
// common::ValidateVector on the payload's position, direction, and velocity, where it has them.
void ValidateSpawnVectors(const StatusChange& rChange);
// The kSpawnOmni or kTransferOmni payload's owned-entity identity.
engine::GlobalId OwnedSpawnGlobalId(const StatusChange& rChange);
engine::ClientGuid OwnedSpawnClientGuid(const StatusChange& rChange);

// Adoptability check the replay stream reader and the harness apply, because client hydration looks the Blaster and
// Explosion types up with a checked index.
bool IsAdoptableStatusChange(const StatusChange& rChange);

// The one application point of the post-tick batch: server harvest, replay playback, and the client post-tick loop.
// coordinate is the cell rFrame belongs to; an explosion takes it and rFrame's tick time.
void ApplySpawnStatusChange(Frame& __restrict rFrame, const StatusChange& rChange, engine::GridCoord coordinate);

} // namespace game
