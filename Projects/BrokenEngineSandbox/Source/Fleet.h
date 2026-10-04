#pragma once

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
inline constexpr int64_t kiMaximumFleetsPerClient = 16;
// Per-fleet member cap — parity with Frame.cpp's kiMaximumFleetSize (16); bounds Fleet::members against a spamming client.
inline constexpr int64_t kiMaximumFleetMembers = 16;

enum class FleetMemberFlags : uint8_t
{
	kIsDead = 1 << 0,
};
using FleetMemberFlags_t = common::Flags<FleetMemberFlags>;

struct FleetMember
{
	engine::GlobalId globalPlayerId {};
	FleetMemberFlags_t flags {};
	engine::GridCoord coordinate {};
	// Server-only automatic respawn countdown in seconds; read only while kIsDead is set, never saved or sent.
	std::chrono::duration<float> respawnTimerSeconds = std::chrono::duration<float>::zero();
};

struct Fleet
{
	FleetGuid guid {};
	std::vector<FleetMember> members;
	// Invalid {} only while the fleet has no members; otherwise names one of them.
	engine::GlobalId flagshipGlobalPlayerId {};
	engine::GridCoord wantedCoordinate {};
	uint8_t uiPendingFleetWantedCoordinateTicks = 0;
	std::chrono::duration<float> navigationDelaySeconds = std::chrono::duration<float>(60.0f);
	std::chrono::duration<float> frameChangeTimerSeconds = std::chrono::duration<float>::zero();
};

} // namespace game
