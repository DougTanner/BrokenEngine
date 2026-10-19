#pragma once

// Direct includes, not the Engine.h aggregation: the game PCH pulls this header in ahead of Engine.h.
#include "Frame/Collections/Explosions/Explosions.h"
#include "Network/NetworkProtocol.h" // ClientGuid

#include "Frame/Collections/Battleships/Battleships.h"
#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Fighters/Fighters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Omnis/Omnis.h"
#include "Frame/Collections/Ships/ShipTypes.h"

namespace game
{

// Underlying values are serialized wire, replay, and save bytes: enumerator order is wire order, and any change to the
// set or order is an incompatible wire change that takes the engine::kiProtocolVersion and FrameInput::kiVersion bumps.
enum class StatusChangeType : uint8_t
{
	// Creations decided outside the tick and cross-cell arrivals: the post-tick batch, applied to the destination right
	// after its tick; never in server tick input.
	kSpawnFighter,
	kSpawnOmni,
	kSpawnBattleship,
	kSpawnBlaster,
	kSpawnMissile,
	kSpawnExplosion,
	kTransferFighter,
	kTransferOmni,
	kTransferBattleship,
	kTransferBlaster,
	kTransferMissile,
	// Tick input.
	kDestroyShip,
	kUpdateShip,
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
	return eType >= StatusChangeType::kSpawnFighter && eType <= StatusChangeType::kSpawnExplosion;
}

inline constexpr bool IsTransferType(StatusChangeType eType)
{
	return eType >= StatusChangeType::kTransferFighter && eType <= StatusChangeType::kTransferMissile;
}

// The post-tick batch: every spawn and transfer tag.
inline constexpr bool IsPostDispatchType(StatusChangeType eType)
{
	return eType >= StatusChangeType::kSpawnFighter && eType <= StatusChangeType::kTransferMissile;
}

static_assert(std::to_underlying(StatusChangeType::kSpawnExplosion) == std::to_underlying(StatusChangeType::kSpawnFighter) + 5 && std::to_underlying(StatusChangeType::kTransferFighter) == std::to_underlying(StatusChangeType::kSpawnExplosion) + 1 && std::to_underlying(StatusChangeType::kTransferMissile) == std::to_underlying(StatusChangeType::kTransferFighter) + 4, "IsSpawnType, IsTransferType, and IsPostDispatchType assume kSpawnFighter..kSpawnExplosion and kTransferFighter..kTransferMissile stay contiguous and adjacent; inserting an enumerator inside them breaks the range checks.");

inline const char* StatusChangeTypeName(StatusChangeType eType)
{
	switch (eType)
	{
		case StatusChangeType::kSpawnFighter:        return "SpawnFighter";
		case StatusChangeType::kSpawnOmni:           return "SpawnOmni";
		case StatusChangeType::kSpawnBattleship:     return "SpawnBattleship";
		case StatusChangeType::kSpawnBlaster:        return "SpawnBlaster";
		case StatusChangeType::kSpawnMissile:        return "SpawnMissile";
		case StatusChangeType::kSpawnExplosion:      return "SpawnExplosion";
		case StatusChangeType::kTransferFighter:     return "TransferFighter";
		case StatusChangeType::kTransferOmni:        return "TransferOmni";
		case StatusChangeType::kTransferBattleship:  return "TransferBattleship";
		case StatusChangeType::kTransferBlaster:     return "TransferBlaster";
		case StatusChangeType::kTransferMissile:     return "TransferMissile";
		case StatusChangeType::kDestroyShip:         return "DestroyShip";
		case StatusChangeType::kUpdateShip:          return "UpdateShip";
		case StatusChangeType::kUpdateFleet:         return "UpdateFleet";
		case StatusChangeType::kCount:               break;
	}
	return "Unknown";
}

struct DestroyShipData
{
	ShipType eType {};
	int64_t iUuid = 0;

	static constexpr auto Fields()
	{
		return engine::FrameColumnList<FRAME_COLUMN(DestroyShipData, eType), FRAME_COLUMN(DestroyShipData, iUuid)> {};
	}
};

struct UpdateShipData
{
	ShipType eType {};
	int64_t iUuid = 0;
	bool bUseMissiles = false;
	std::chrono::duration<float> navigationDelaySeconds = std::chrono::duration<float>(60.0f);
	uint8_t uiPendingWeaponModeTicks = 0;

	static constexpr auto Fields()
	{
		return engine::FrameColumnList<FRAME_COLUMN(UpdateShipData, eType), FRAME_COLUMN(UpdateShipData, iUuid), FRAME_COLUMN(UpdateShipData, bUseMissiles), FRAME_COLUMN(UpdateShipData, navigationDelaySeconds), FRAME_COLUMN(UpdateShipData, uiPendingWeaponModeTicks)> {};
	}
};

struct UpdateFleetData
{
	ShipType eType {};
	int64_t iUuid = 0;
	bool bIsFlagship = false;
	engine::GlobalId flagshipGlobalId {};
	engine::GridCoord fleetWantedCoordinate {};
	uint8_t uiPendingFleetWantedCoordinateTicks = 0;

	static constexpr auto Fields()
	{
		return engine::FrameColumnList<FRAME_COLUMN(UpdateFleetData, eType), FRAME_COLUMN(UpdateFleetData, iUuid), FRAME_COLUMN(UpdateFleetData, bIsFlagship), FRAME_COLUMN(UpdateFleetData, flagshipGlobalId), FRAME_COLUMN(UpdateFleetData, fleetWantedCoordinate), FRAME_COLUMN(UpdateFleetData, uiPendingFleetWantedCoordinateTicks)> {};
	}
};

using StatusChangeData = std::variant<
	FightersPostRender::SpawnInfo,
	OmnisPostRender::SpawnInfo,
	BattleshipsPostRender::SpawnInfo,
	BlastersPostRender::SpawnInfo,
	MissilesPostRender::SpawnInfo,
	engine::ExplosionsPostRender::SpawnInfo,
	DestroyShipData,
	UpdateShipData,
	UpdateFleetData>;

// A spawn payload's collection and tags. Pushers have no SpawnInfo and no specialization, so they cannot be
// queued, parsed, or applied from outside their owner's Spawn.
template <typename SPAWN_INFO>
struct SpawnTraits {};

template <>
struct SpawnTraits<FightersPostRender::SpawnInfo>
{
	using Collection = FightersPostRender;
	static constexpr StatusChangeType kSpawn = StatusChangeType::kSpawnFighter;
	static constexpr StatusChangeType kTransfer = StatusChangeType::kTransferFighter;
};

template <>
struct SpawnTraits<OmnisPostRender::SpawnInfo>
{
	using Collection = OmnisPostRender;
	static constexpr StatusChangeType kSpawn = StatusChangeType::kSpawnOmni;
	static constexpr StatusChangeType kTransfer = StatusChangeType::kTransferOmni;
};

template <>
struct SpawnTraits<BattleshipsPostRender::SpawnInfo>
{
	using Collection = BattleshipsPostRender;
	static constexpr StatusChangeType kSpawn = StatusChangeType::kSpawnBattleship;
	static constexpr StatusChangeType kTransfer = StatusChangeType::kTransferBattleship;
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

static_assert(std::is_trivially_copyable_v<FightersPostRender::SpawnInfo> && std::is_trivially_copyable_v<OmnisPostRender::SpawnInfo> && std::is_trivially_copyable_v<BattleshipsPostRender::SpawnInfo> && std::is_trivially_copyable_v<BlastersPostRender::SpawnInfo> && std::is_trivially_copyable_v<MissilesPostRender::SpawnInfo> && std::is_trivially_copyable_v<engine::ExplosionsPostRender::SpawnInfo>, "Every SpawnInfo is a status-change payload the replay difference stream writes as a raw image.");

inline StatusChangeData DefaultDataForType(StatusChangeType eType)
{
	switch (eType)
	{
		case StatusChangeType::kSpawnFighter:
		case StatusChangeType::kTransferFighter:    return FightersPostRender::SpawnInfo {};
		case StatusChangeType::kSpawnOmni:
		case StatusChangeType::kTransferOmni:       return OmnisPostRender::SpawnInfo {};
		case StatusChangeType::kSpawnBattleship:
		case StatusChangeType::kTransferBattleship: return BattleshipsPostRender::SpawnInfo {};
		case StatusChangeType::kSpawnBlaster:
		case StatusChangeType::kTransferBlaster:    return BlastersPostRender::SpawnInfo {};
		case StatusChangeType::kSpawnMissile:
		case StatusChangeType::kTransferMissile:    return MissilesPostRender::SpawnInfo {};
		case StatusChangeType::kSpawnExplosion:     return engine::ExplosionsPostRender::SpawnInfo {};
		case StatusChangeType::kDestroyShip:        return DestroyShipData {};
		case StatusChangeType::kUpdateShip:         return UpdateShipData {};
		case StatusChangeType::kUpdateFleet:        return UpdateFleetData {};
		case StatusChangeType::kCount:              break;
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
// A ship transfer whose payload is fleet-owned: the transfer that carries an owned entity.
bool IsOwnedEntityTransfer(const StatusChange& rChange);
// A ship spawn or transfer payload's owned-entity identity.
engine::GlobalId OwnedSpawnGlobalId(const StatusChange& rChange);
engine::ClientGuid OwnedSpawnClientGuid(const StatusChange& rChange);

// Adoptability check the replay stream reader and the harness apply: client hydration looks the Blaster and Explosion
// types up with a checked index, and a ship controller or ship type outside its enumerators selects no collection.
bool IsAdoptableStatusChange(const StatusChange& rChange);

// The one application point of the post-tick batch: server harvest, replay playback, and the client post-tick loop.
// coordinate is the cell rFrame belongs to; an explosion takes it and rFrame's tick time.
void ApplySpawnStatusChange(Frame& __restrict rFrame, const StatusChange& rChange, engine::GridCoord coordinate);

} // namespace game
