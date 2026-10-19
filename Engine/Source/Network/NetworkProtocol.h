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
	kClientCrashReport,         // Client sends the leading bytes of its crash report while crashing, or on its next accepted connection while that report is unsent
	kClientChat,                // Client sends one chat message to the server
	kServerChat,                // Server relays a validated chat message to every handshaken client
	kServerOwnedEntityState,    // Server reports one owned entity spawned, changed cell, or died
	kClientSaveRequest,         // Client requests server quicksave (debug only)
	kClientLoadRequest,         // Client requests server quickload (debug only)
	kClientResetRequest,        // Client requests server reset (debug only)
	kClientReplayRecordRequest, // Client requests server replay record start/stop (debug only)
	kClientReplayPlaybackRequest, // Client requests server replay playback (debug only)
	kClientPauseRequest,        // Client requests server pause/unpause (debug only)
	kClientTimespeedRequest,    // Client requests timescale change (debug only)
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
		case PacketType::kClientCrashReport:            return "kClientCrashReport";
		case PacketType::kClientChat:                   return "kClientChat";
		case PacketType::kServerChat:                   return "kServerChat";
		case PacketType::kServerOwnedEntityState:       return "kServerOwnedEntityState";
		case PacketType::kClientSaveRequest:            return "kClientSaveRequest";
		case PacketType::kClientLoadRequest:            return "kClientLoadRequest";
		case PacketType::kClientResetRequest:           return "kClientResetRequest";
		case PacketType::kClientReplayRecordRequest:    return "kClientReplayRecordRequest";
		case PacketType::kClientReplayPlaybackRequest:  return "kClientReplayPlaybackRequest";
		case PacketType::kClientPauseRequest:           return "kClientPauseRequest";
		case PacketType::kClientTimespeedRequest:       return "kClientTimespeedRequest";
		case PacketType::kGamePacketStart:              return "kGamePacketStart";
	}
	return "Unknown";
}

inline constexpr int64_t kiProtocolVersion = 33;
inline constexpr int64_t kiSubscribeRejectSlot = 0xFF; // Sentinel slot in kServerSubscribeAccept: server rejected the subscribe (not adjacent / no free slot)
inline constexpr int64_t kiDefaultPort = 27'015;
inline constexpr int64_t kiCoordinateSlots = 16; // 9 visible subscriptions + 6 sticky + 1 spare
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
// The report's header, exception text, and callstack lead the file; the log dump after them can reach tens of MiB.
inline constexpr int64_t kiMaximumCrashReportSize = 1'024 * 1'024;
// Initial per-cell status-change batch capacity; server and client buffers grow past it with a debug break, which means raise it.
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

static_assert(sizeof(ClientGuid) == 16, "ClientGuid size changed — ClientGuid.bin and the game save owner records are 16 raw bytes");
static_assert(alignof(ClientGuid) == alignof(uint64_t), "ClientGuid alignment changed — the game owned-entity client-GUID column stride shifts");
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
		case PacketType::kClientCrashReport:       return {.iMinimumSize = NetworkMessages::ClientCrashReportMessage::kiMinSize, .iMaximumSize = NetworkMessages::ClientCrashReportMessage::kiMaxSize, .iMaximumPerTick = 1};
		case PacketType::kClientChat:              return {.iMinimumSize = NetworkMessages::ClientChatMessage::kiMinSize, .iMaximumSize = NetworkMessages::ClientChatMessage::kiMaxSize, .iMaximumPerTick = 8};

		// With kbDebugInput false, debug-control requests return the non-sendable sentinel, preventing handshaken clients from resetting, pausing, or changing server speed.
		case PacketType::kClientSaveRequest:
		case PacketType::kClientLoadRequest:
		case PacketType::kClientResetRequest:
		case PacketType::kClientReplayRecordRequest:
		case PacketType::kClientReplayPlaybackRequest:
			if constexpr (kbDebugInput)
			{
				return {.iMinimumSize = NetworkMessages::kiPacketTypeSize, .iMaximumSize = NetworkMessages::kiPacketTypeSize, .iMaximumPerTick = 2};
			}
			else
			{
				return {};
			}
		case PacketType::kClientPauseRequest:
			if constexpr (kbDebugInput)
			{
				return {.iMinimumSize = NetworkMessages::ClientPauseRequestMessage::kiFixedSize, .iMaximumSize = NetworkMessages::ClientPauseRequestMessage::kiFixedSize, .iMaximumPerTick = 4};
			}
			else
			{
				return {};
			}
		case PacketType::kClientTimespeedRequest:
			if constexpr (kbDebugInput)
			{
				return {.iMinimumSize = NetworkMessages::ClientTimespeedRequestMessage::kiFixedSize, .iMaximumSize = NetworkMessages::ClientTimespeedRequestMessage::kiFixedSize, .iMaximumPerTick = 8};
			}
			else
			{
				return {};
			}

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
