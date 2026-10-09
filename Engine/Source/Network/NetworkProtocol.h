#pragma once

namespace engine
{

struct NetworkTimeState
{
	bool bFastForward = false;
	int64_t iExpectedUpdateIntervalMicroseconds = 0;
	int64_t iExpectedUpdatesPerSecond = 0;
};

enum class PacketType : uint8_t
{
	kServerCoordinateFullState,      // Per-coord full state (reliable, slot channel)
	kServerCoordinateStaticData,     // Per-coord static data sent once per subscription (reliable, slot channel)
	kServerCoordinateUpdate,         // Per-coord delta update (unreliable, slot channel)
	kServerCoordinateResend,         // Per-coord re-sent frame (unreliable, slot channel)
	kServerDebugFrame,
	kClientDesynchronizationReport,
	kClientAcknowledgmentStream,
	kClientDebugFrameRequest,
	kClientHello,
	kServerConnectionResponse,
	kClientSubscribe,           // Client requests subscription to a GridCoord
	kClientUnsubscribe,         // Client releases a coord slot at the observed epoch
	kClientResynchronizationRequest,       // Client requests full state re-download after desync recovery
	kServerSubscribeAccept,     // Server confirms subscription with assigned slot
	kServerUnsubscribeAck,
	kServerLoadNotification,    // Server loaded a save, clients must reset state
	kServerTimespeedUpdate,     // Server broadcasts the current time scale to all clients
	kGamePacketStart,           // All values >= this are game-layer packets forwarded as raw bytes
};

inline constexpr const char* PacketTypeName(PacketType eType)
{
	switch (eType)
	{
		case PacketType::kServerCoordinateFullState:         return "kServerCoordFullState";
		case PacketType::kServerCoordinateStaticData:        return "kServerCoordStaticData";
		case PacketType::kServerCoordinateUpdate:            return "kServerCoordUpdate";
		case PacketType::kServerCoordinateResend:            return "kServerCoordResend";
		case PacketType::kServerDebugFrame:             return "kServerDebugFrame";
		case PacketType::kClientDesynchronizationReport:           return "kClientDesyncReport";
		case PacketType::kClientAcknowledgmentStream:              return "kClientAckStream";
		case PacketType::kClientDebugFrameRequest:      return "kClientDebugFrameRequest";
		case PacketType::kClientHello:                  return "kClientHello";
		case PacketType::kServerConnectionResponse:     return "kServerConnectionResponse";
		case PacketType::kClientSubscribe:              return "kClientSubscribe";
		case PacketType::kClientUnsubscribe:            return "kClientUnsubscribe";
		case PacketType::kClientResynchronizationRequest:          return "kClientResyncRequest";
		case PacketType::kServerSubscribeAccept:        return "kServerSubscribeAccept";
		case PacketType::kServerUnsubscribeAck:         return "kServerUnsubscribeAck";
		case PacketType::kServerLoadNotification:       return "kServerLoadNotification";
		case PacketType::kServerTimespeedUpdate:        return "kServerTimespeedUpdate";
		case PacketType::kGamePacketStart:              return "kGamePacketStart";
	}
	return "Unknown";
}

inline constexpr int64_t kiProtocolVersion = 21;
inline constexpr int64_t kiSubscribeRejectSlot = 0xFF; // Sentinel slot in kServerSubscribeAccept: server rejected the subscribe (not adjacent / no free slot)
inline constexpr int64_t kiDefaultPort = 27'015;
inline constexpr int64_t kiMaximumResendFrames = 8;
inline constexpr int64_t kiFloorStallLogThreshold = 15;
inline constexpr int64_t kiMaximumBufferedFrames = 256;
inline constexpr int64_t kiClockSnapThreshold = 28; // |clockError| >= this (ticks) hard-snaps miTickCounter to the servo target; see Network.md
// Add 109.375 ms to 3x measured jitter for miCurrentTargetBehind. At 32 Hz this is 3.5 ticks, deliberately
// off-boundary so zero jitter rounds to four ticks rather than five. mSmoothedJitterMicroseconds measures mean absolute
// interarrival deviation; the one-way tail half-width is roughly 1.5x that mean, motivating the 3x factor. The safety
// term is specified in wall-clock microseconds.
inline constexpr int64_t kiJitterSafetyMicroseconds = 109'375;
// Slack between the clock-servo target (EstimatedLatestServerTick() - miCurrentTargetBehind) and the hard sim
// ceiling in GameBase::ClientUpdate. The servo steers toward the bare target so the sim never rests
// against the ceiling; the slack absorbs per-packet arrival jitter and the single-tick targetBehind
// raise step in EvaluateClock (which lowers the ceiling by one tick per call) without stalling the sim.
// Must stay below EvaluateClock's |error| >= 4 aggressive-correction threshold so steady state never
// triggers it.
inline constexpr int64_t kiSimulationCeilingSlackTicks = 3;
// Cap on ClientSessionRuntime::EstimatedLatestServerTick's lead over the latest received server tick. It covers the
// longest measured arrival gap (about 15 ticks at kChina: an 11-packet drop run plus the 100 ms one-way delay spread)
// and stays well below kiClockSnapThreshold, so a real server stop speculates at most this plus
// kiSimulationCeilingSlackTicks past the latest received tick without a clock snap.
inline constexpr int64_t kiMaximumEstimatedServerTickLead = 16;
inline constexpr int64_t kiMaximumPacketSize = 64 * 1'024;
inline constexpr int64_t kiMaximumStatusChangesPerCell = 1'024;

// Tag-distinct 128-bit identifier. TAG only separates instantiations at compile time and contributes no
// storage, so every instantiation shares one layout; see engine::Id in Frame/Collections/CollectionId.h.
template <typename TAG>
struct Guid128
{
	uint64_t uiHigh = 0;
	uint64_t uiLow = 0;

	bool operator==(const Guid128&) const = default;
};

// 128-bit client GUID for persistent identity across save/load
struct ClientGuidTag;
using ClientGuid = Guid128<ClientGuidTag>;

static_assert(sizeof(ClientGuid) == 16, "ClientGuid size changed — ClientGuid.bin and the fleet save owner records are 16 raw bytes");
static_assert(alignof(ClientGuid) == alignof(uint64_t), "ClientGuid alignment changed — PlayersPostRender::pClientGuids column stride shifts");
static_assert(BT_OFFSETOF(ClientGuid, uiHigh) == 0, "ClientGuid::uiHigh offset changed — existing ClientGuid.bin and saves read uiHigh first");
static_assert(BT_OFFSETOF(ClientGuid, uiLow) == 8, "ClientGuid::uiLow offset changed — existing ClientGuid.bin and saves read uiLow second");
static_assert(std::is_trivially_copyable_v<ClientGuid>, "ClientGuid must stay trivially copyable — WriteVersionedFile stamps sizeof only for trivially copyable types");
static_assert(std::is_standard_layout_v<ClientGuid>, "ClientGuid must stay standard-layout — BT_OFFSETOF above is only well-defined for standard-layout types");

} // namespace engine

namespace std
{

template <typename TAG>
struct hash<engine::Guid128<TAG>>
{
	size_t operator()(const engine::Guid128<TAG>& rGuid) const
	{
		return std::hash<uint64_t> {}(rGuid.uiHigh) ^ (std::hash<uint64_t> {}(rGuid.uiLow) << 1);
	}
};

} // namespace std

#include "Network/NetworkMessages.h"

namespace engine
{

// Client-to-server packet limits are declarative and checked once at Server::Receive for engine types below
// kGamePacketStart; game-range types are checked during parsing by GetGamePacketContract. See
// Documents/Architecture/Network.md "Client -> Server Contract".

// The 64 mirrors NetworkManager::kiMaximumEnetCoordinateSlots; a static_assert tying it to the transport
// ceiling lives in Server.cpp because NetworkProtocol.h cannot include NetworkManager.h.
inline constexpr int64_t kiMaximumAcknowledgmentStreamPacketSize = NetworkMessages::ClientAckStreamMessage::GetSize(NetworkMessages::ClientAckStreamMessage::kiMaxSlotCount);

// The per-client packet budget spans both polls of an update. At 32 Hz, four seconds of queued
// acknowledgments contribute about 128 packets; the 256-packet allowance also accommodates requests.
// Budget crossings outside Server::Poll's stall grace count as violations.
inline constexpr int64_t kiMaximumClientPacketsPerTick = 256;

// Global per-client inbound byte budget per update window. >10x legitimate steady state; caps hostile
// parse work at ~2 MiB/s/client.
inline constexpr int64_t kiMaximumClientInboundBytesPerTick = 64 * 1'024;

// Corrupt-data violations accumulate for the connection lifetime; reaching this count disconnects.
inline constexpr int64_t kiCorruptViolationDisconnectCount = 4;

// Outstanding budget or counted per-type rate violations disconnect at this limit.
// The margin accommodates Debug-server key presses exceeding a per-type cap and other request bursts.
inline constexpr int64_t kiRateViolationDisconnectCount = 8;

// One too-fast violation is forgiven per interval, so only a sustained flood accumulates to the limit.
inline constexpr std::chrono::seconds kRateViolationDecayInterval = 60s;

// Sizes include the packet type byte.
struct ClientPacketContract
{
	int64_t iMinimumSize = 0;
	int64_t iMaximumSize = 0;              // 0 = not client-sendable (sentinel)
	int64_t iMaximumPerTick = 0;
	bool bRequiresHandshake = true;
	bool bOverCapCountsViolation = true;
};

// Contract for engine packet types (< kGamePacketStart) only. Game-range types bypass this at
// Server::Receive and are contract-checked at parse via GetGamePacketContract. Default row is the
// not-client-sendable sentinel (server->client and unknown engine types).
inline constexpr ClientPacketContract GetClientPacketContract(PacketType eType)
{
	switch (eType)
	{
		case PacketType::kClientAcknowledgmentStream:         return {.iMinimumSize = NetworkMessages::ClientAckStreamMessage::kiFixedSize, .iMaximumSize = kiMaximumAcknowledgmentStreamPacketSize, .iMaximumPerTick = 128, .bRequiresHandshake = true, .bOverCapCountsViolation = false}; // over-cap is a silent multi-tick-poll burst safety drop
		case PacketType::kClientDesynchronizationReport:      return {.iMinimumSize = NetworkMessages::ClientDesyncReportMessage::kiFixedSize, .iMaximumSize = NetworkMessages::ClientDesyncReportMessage::kiFixedSize, .iMaximumPerTick = 8};
		case PacketType::kClientDebugFrameRequest: return {.iMinimumSize = NetworkMessages::ClientDebugFrameRequestMessage::kiFixedSize, .iMaximumSize = NetworkMessages::ClientDebugFrameRequestMessage::kiFixedSize, .iMaximumPerTick = 8};
		case PacketType::kClientHello:             return {.iMinimumSize = NetworkMessages::ClientHelloMessage::kiMinSize, .iMaximumSize = NetworkMessages::ClientHelloMessage::kiMaxSize, .iMaximumPerTick = 4, .bRequiresHandshake = false};
		case PacketType::kClientSubscribe:         return {.iMinimumSize = NetworkMessages::ClientSubscribeMessage::kiFixedSize, .iMaximumSize = NetworkMessages::ClientSubscribeMessage::kiFixedSize, .iMaximumPerTick = 64};
		case PacketType::kClientUnsubscribe:       return {.iMinimumSize = NetworkMessages::ClientUnsubscribeMessage::kiFixedSize, .iMaximumSize = NetworkMessages::ClientUnsubscribeMessage::kiFixedSize, .iMaximumPerTick = 64};
		case PacketType::kClientResynchronizationRequest:     return {.iMinimumSize = NetworkMessages::ClientResyncRequestMessage::kiFixedSize, .iMaximumSize = NetworkMessages::ClientResyncRequestMessage::kiFixedSize, .iMaximumPerTick = 4};
		default:                                   return {};
	}
}

inline constexpr int64_t kiDiscoveryPort = kiDefaultPort + 1;
inline constexpr int64_t kiDiscoveryMagic = 0x42524B4E; // "BRKN"
inline constexpr std::chrono::milliseconds kDiscoveryScanDuration = 1'500ms;

// Network buffer size for ACK bitfield and snapshot ring buffers (decoupled from physics tick rate)
inline constexpr int64_t kiNetworkBufferSize = 128;

// Per-slot ACK tracking state (shared by client and server)
struct AckState
{
	int64_t iAcknowledgmentFloor = -1;
	uint64_t uiReceivedBitfieldLow = 0;   // bits 0-63
	uint64_t uiReceivedBitfieldHigh = 0;  // bits 64-127
	uint16_t uiEpoch = 0;
};

} // namespace engine
