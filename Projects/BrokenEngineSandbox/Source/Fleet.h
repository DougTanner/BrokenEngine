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

// DoS ceiling on per-client fleet count — well above any real use; bounds mFleets against a spamming client.
inline constexpr int64_t kiMaxFleetsPerClient = 16;
// Per-fleet member cap — parity with Frame.cpp's kiMaxFleetSize (16); bounds Fleet::members against a spamming client.
inline constexpr size_t kuiMaxFleetMembers = 16;

enum class FleetMemberFlags : uint8_t
{
	kIsDead = 1 << 0,
};
using FleetMemberFlags_t = common::Flags<FleetMemberFlags>;

struct FleetMember
{
	engine::global_id_t globalPlayerId {};
	FleetMemberFlags_t flags {};
	engine::GridCoord coord {};
	// Server-only automatic respawn countdown in seconds; read only while kIsDead is set, never saved or sent.
	float fRespawnTimer = 0.0f;
};

struct Fleet
{
	FleetGuid guid {};
	std::vector<FleetMember> members;
	// Invalid {} only while the fleet has no members; otherwise names one of them.
	engine::global_id_t flagshipGlobalPlayerId {};
	engine::GridCoord wantedCoord {};
	uint8_t uiPendingFleetWantedCoordTicks = 0;
	float fNavigationDelay = 60.0f;
	float fFrameChangeTimer = 0.0f;
};

} // namespace game
