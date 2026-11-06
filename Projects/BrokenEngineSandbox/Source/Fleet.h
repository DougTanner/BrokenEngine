#pragma once

#include "Frame/Collections/Ships/ShipBehaviours.h"

namespace game
{

// Server-minted random 128-bit fleet identifier; survives disconnect/reconnect, save/load, and full client restart.
struct FleetGuidTag;
using FleetGuid = engine::Guid128<FleetGuidTag>;

static_assert(!std::is_same_v<FleetGuid, engine::ClientGuid>, "FleetGuid and ClientGuid must stay tag-distinct — a shared tag silently permits passing a client identifier where a fleet identifier belongs");
static_assert(sizeof(FleetGuid) == 16, "FleetGuid size changed — ClientState.bin and the fleet save records are 16 raw bytes");
static_assert(alignof(FleetGuid) == alignof(uint64_t), "FleetGuid alignment changed — ClientStateSettings padding shifts");
static_assert(BT_OFFSETOF(FleetGuid, uiHigh) == 0, "FleetGuid::uiHigh offset changed — existing saves read uiHigh first");
static_assert(BT_OFFSETOF(FleetGuid, uiLow) == 8, "FleetGuid::uiLow offset changed — existing saves read uiLow second");
static_assert(std::is_trivially_copyable_v<FleetGuid>, "FleetGuid must stay trivially copyable — it is the leading member of the trivially copyable ClientStateSettings POD");
static_assert(std::is_standard_layout_v<FleetGuid>, "FleetGuid must stay standard-layout — BT_OFFSETOF above is only well-defined for standard-layout types");

// Bounds per-client fleet storage against request spam.
inline constexpr int64_t kiMaximumFleetsPerClient = 4;
inline constexpr int64_t kiMaximumFleetSquadronsPerType = 2;
// Members one squadron holds, indexed by ShipType.
inline constexpr int64_t kpiFleetSquadronCapacities[] {9, 2, 1};
static_assert(std::ssize(kpiFleetSquadronCapacities) == std::to_underlying(ShipType::kCount), "kpiFleetSquadronCapacities needs one entry per ShipType");
// The save read's allocation bound; FindOpenFleetSquadron is the limit test.
inline constexpr int64_t kiMaximumFleetMembers = kiMaximumFleetSquadronsPerType * std::ranges::fold_left(kpiFleetSquadronCapacities, 0i64, std::plus {});

enum class FleetMemberFlags : uint8_t
{
	kIsDead = 1 << 0,
};
using FleetMemberFlags_t = common::Flags<FleetMemberFlags>;

struct FleetMember
{
	engine::GlobalId globalId {};
	// Fixed when the member is added; every spawn spawns this type.
	ShipType eType {};
	FleetMemberFlags_t flags {};
	// Fixed when the member is added. Numbered per type, so eType and uiSquadron together name a squadron within the
	// fleet, which exists exactly while a member names it. Stored because list position cannot supply it: the list is
	// in add order and a removal closes its hole.
	uint8_t uiSquadron = 0;
	engine::GridCoord coordinate {};
	// Server-only automatic respawn countdown in seconds; TickRespawnTimers reads it only while kIsDead is set. Replay
	// fleet records carry every member's value; grid saves and the wire do not.
	std::chrono::duration<float> respawnTimerSeconds = std::chrono::duration<float>::zero();
};

struct Fleet
{
	FleetGuid guid {};
	std::vector<FleetMember> members;
	// Names the fleet's Battleship member from the fleet's creation.
	engine::GlobalId flagshipGlobalId {};
	engine::GridCoord wantedCoordinate {};
	uint8_t uiPendingFleetWantedCoordinateTicks = 0;
	std::chrono::duration<float> navigationDelaySeconds = std::chrono::duration<float>(60.0f);
	std::chrono::duration<float> cellChangeTimerSeconds = std::chrono::duration<float>::zero();
};

inline int64_t CountFleetSquadronMembers(const Fleet& rFleet, ShipType eType, uint8_t uiSquadron)
{
	return std::ranges::count_if(rFleet.members, [eType, uiSquadron](const FleetMember& rMember)
	{
		return rMember.eType == eType && rMember.uiSquadron == uiSquadron;
	});
}

// Never a valid squadron number.
inline constexpr uint8_t kuiNoOpenFleetSquadron = static_cast<uint8_t>(kiMaximumFleetSquadronsPerType);

// The squadron a new member of eType joins: the lowest non-empty one with room, else the lowest empty number, else
// kuiNoOpenFleetSquadron, the one limit test on fleet size. A fleet's Battleship never joins this way.
inline uint8_t FindOpenFleetSquadron(const Fleet& rFleet, ShipType eType)
{
	uint8_t uiLowestEmptySquadron = kuiNoOpenFleetSquadron;
	for (uint8_t i = 0; i < kiMaximumFleetSquadronsPerType; ++i)
	{
		int64_t iMemberCount = CountFleetSquadronMembers(rFleet, eType, i);
		if (iMemberCount > 0 && iMemberCount < kpiFleetSquadronCapacities[std::to_underlying(eType)])
		{
			return i;
		}
		if (iMemberCount == 0 && uiLowestEmptySquadron == kuiNoOpenFleetSquadron)
		{
			uiLowestEmptySquadron = i;
		}
	}
	return uiLowestEmptySquadron;
}

} // namespace game
