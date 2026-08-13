#pragma once

namespace game
{

// Server-minted random 128-bit fleet identifier; survives disconnect/reconnect, save/load, and full client restart.
// A tag-distinct instantiation of engine::Guid128, so a fleet identifier cannot be passed where a client identifier belongs.
struct FleetGuidTag;
using FleetGuid = engine::Guid128<FleetGuidTag>;
using FleetGuidHash = engine::Guid128Hash<FleetGuidTag>;

static_assert(!std::is_same_v<FleetGuid, engine::ClientGuid>, "FleetGuid and ClientGuid must stay tag-distinct — a shared tag silently permits passing a client identifier where a fleet identifier belongs");
static_assert(sizeof(FleetGuid) == 16, "FleetGuid size changed — ClientState.bin and the fleet save records are 16 raw bytes");
static_assert(alignof(FleetGuid) == alignof(uint64_t), "FleetGuid alignment changed — ClientStateSettings padding shifts");
static_assert(BT_OFFSETOF(FleetGuid, uiHigh) == 0, "FleetGuid::uiHigh offset changed — existing saves read uiHigh first");
static_assert(BT_OFFSETOF(FleetGuid, uiLow) == 8, "FleetGuid::uiLow offset changed — existing saves read uiLow second");
static_assert(std::is_trivially_copyable_v<FleetGuid>, "FleetGuid must stay trivially copyable — it is the leading member of the trivially copyable ClientStateSettings POD");
static_assert(std::is_standard_layout_v<FleetGuid>, "FleetGuid must stay standard-layout — BT_OFFSETOF above is only well-defined for standard-layout types");

struct FleetMember
{
	engine::global_id_t globalPlayerId {};
	bool bAlive = true;
	engine::GridCoord coord {};
};

struct Fleet
{
	FleetGuid guid {};
	std::vector<FleetMember> members;
	int64_t iFlagshipIndex = 0;
	engine::GridCoord wantedCoord {};
	uint8_t uiPendingFleetWantedCoordTicks = 0;
	float fNavigationDelay = 60.0f;
	float fFrameChangeTimer = 0.0f;
};

} // namespace game
