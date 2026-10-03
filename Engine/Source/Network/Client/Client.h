#pragma once

#if defined(BT_CLIENT)

#include "Frame/FrameStaticData.h"
#include "Frame/GridCoord.h"
#include "Network/NetworkCursor.h"

namespace game
{

struct Frame;
struct StatusChange;

} // namespace game

namespace engine
{

class ClientSessionRuntime;

// Per-coordinate received update (single coordinate, not multi-coordinate)
struct ReceivedCoordUpdate
{
	int64_t iTick = 0;
	common::crc_t uiSharedCrc = 0;
	// Heap: ENet packet data, variable per frame
	std::vector<game::StatusChange> statusChanges;
};

struct ReceivedCoordFullState
{
	int64_t iTick = 0;
	GridCoord coordinate {};
	std::unique_ptr<game::Frame> pFrame;
};

// Per-coordinate received static data (sent once per subscription)
struct ReceivedStaticData
{
	GridCoord coordinate {};
	FrameStaticData staticData;
};

enum class CoordSubscriptionState : uint8_t
{
	kUnsubscribed,
	kWaitingFullState,  // Accept received, waiting for full state
	kActive,            // Receiving delta updates
	kUnsubscribing,     // kClientUnsubscribe sent, waiting for ack
};

struct ClientCoordSlot
{
	GridCoord coordinate {};
	CoordSubscriptionState eState = CoordSubscriptionState::kUnsubscribed;
	AckState acknowledgementState;
	std::chrono::steady_clock::time_point transitionStartTime {};
};

enum class SubscribeRequestFlags : uint8_t
{
	kCancelled     = 1 << 0,
	kTimeoutWarned = 1 << 1,
};

// One kClientSubscribe sent and not yet answered by an accept or reject
struct SubscribeRequest
{
	GridCoord coordinate {};
	std::chrono::steady_clock::time_point startTime {};
	common::Flags<SubscribeRequestFlags> flags;
};

// Subscribe requests in send order. Per coordinate: zero or more cancelled records, then at most one live record.
// The server answers every subscribe exactly once on the reliable channel, which is FIFO, so each accept or
// reject consumes the coordinate's oldest record. Full state and static data only consult records.
class SubscribeRequests
{
public:
	void Add(GridCoord coordinate);
	void Cancel(GridCoord coordinate);
	// Warns once per live record left unanswered past the timeout; the record stays live until its answer
	void WarnTimedOut(std::chrono::steady_clock::time_point now);
	// Removes the coordinate's oldest record; true only if that record was live
	bool TakeAnswer(GridCoord coordinate);
	bool IsLive(GridCoord coordinate) const;

	std::vector<SubscribeRequest> mRecords;
};

struct ReceivedDebugFrame
{
	int64_t iTick = 0;
	GridCoord coordinate {};
	std::unique_ptr<game::Frame> pFrame;
};

class Client
{
public:

	using GuidAssignedCallback = void (*)(const ClientGuid&);

	Client(const char* pcServerAddress, uint16_t uiPort, int64_t iCoordinateSlotCount, const ClientGuid& rGuid, GuidAssignedCallback pGuidAssignedCallback);
	~Client();

	template <typename TTYPE, typename... TARGS>
	void SendSimplePacket(TTYPE eType, uint8_t uiChannel, uint32_t uiPacketFlags, const TARGS&... rArguments)
	{
		static_assert(std::is_enum_v<TTYPE>, "SendSimplePacket type tag must be an enum (engine::PacketType or game::GamePacketType)");

		if (!(mStateFlags & ClientStateFlags::kConnected) || mpServerPeer == nullptr)
		{
			return;
		}

		NetworkManager::SendSimplePacket(mpServerPeer, eType, uiChannel, uiPacketFlags, rArguments...);
	}

	void SendDesynchronizationReport(int64_t iTick, GridCoord coordinate, common::crc_t uiExpectedCrc, common::crc_t uiActualCrc);
	void SendDebugFrameRequest(int64_t iTick, GridCoord coordinate);
	void SendResynchronizationRequest();
	void Disconnect();

	// Wire dispatch entry point for one received packet. Public so harness fixtures can exercise the real dispatch,
	// classification, and response paths.
	void Receive(std::span<const uint8_t> packetData);

	enum class ClientStateFlags : uint8_t
	{
		kConnected                = 1 << 0,
		kConnectionAccepted       = 1 << 1,
		kDisconnectedEvent        = 1 << 2,
		kHasLastUpdateArrival     = 1 << 3,
		kDesynchronizationDebugMode          = 1 << 4,
		kLoadNotificationReceived = 1 << 5,
		kSkipNextJitterInterval   = 1 << 6,
		kServerDebugInput         = 1 << 7, // the accepting server takes debug-control requests (its kbDebugInput)
	};

	ENetPeer* mpServerPeer = nullptr;
	common::Flags<ClientStateFlags> mStateFlags;
	char mpcRejectionReason[256] = {};

	// Heap: raw game packet buffer grows on assign/player-state packets
	std::vector<std::pair<uint8_t, std::vector<uint8_t>>> mReceivedGamePackets;
	std::vector<std::vector<ReceivedCoordUpdate>> mReceivedCoordinateUpdates;
	std::vector<ReceivedCoordFullState> mReceivedFullStates;
	std::vector<ReceivedStaticData> mReceivedStaticData;
	std::unique_ptr<ReceivedDebugFrame> mpReceivedDebugFrame;
	std::vector<ClientCoordSlot> mCoordinateSlots;
	SubscribeRequests mSubscribeRequests;
	common::Smoothed<int64_t> mSmoothedPipelineRoundTripTimeMicroseconds;
	common::InTheLastSecond mBytesInPerSecond;
	common::InTheLastSecond mBytesOutPerSecond;
	common::InTheLastSecond mFramesReceived;
	common::Smoothed<int64_t> mSmoothedJitterMicroseconds;
	ClientGuid mClientGuid {};
	NetworkTimeState mTimeState {};

private:
	friend class ClientSessionRuntime;
	void Poll(const NetworkTimeState& rTimeState);
	bool SendAcknowledgement();
	bool SendSubscribe(GridCoord coordinate);
	void SendUnsubscribe(int64_t iSlot);
	void Flush();
	std::optional<uint8_t> DrainLoadNotification();
	void RecoverTimedOutSubscriptions();
	void ResetAllSlots();
	void FreeSlot(int64_t iSlot);
	void DispatchIncoming(ENetEvent& rEvent, bool bFastForward);
	void Receive(ENetEvent& rEvent);
	void ServerCoordinateFullState(std::span<const uint8_t> packetData);
	void ServerCoordinateStaticData(std::span<const uint8_t> packetData);
	void ServerCoordinateUpdateOrResend(std::span<const uint8_t> packetData, bool bProcessRoundTripTime);
	void ServerDebugFrame(std::span<const uint8_t> packetData);
	void ServerConnectionResponse(std::span<const uint8_t> packetData);
	void ServerSubscribeAccept(std::span<const uint8_t> packetData);
	void ServerUnsubscribeAcknowledgement(std::span<const uint8_t> packetData);
	void ServerLoadNotification(std::span<const uint8_t> packetData);
	void ServerTimespeedUpdate(std::span<const uint8_t> packetData);
	void SendHello();

	enum class FullStateFlags : uint8_t
	{
		kAdoptCoordinate    = 1 << 0, // Full state arrived before SubscribeAccept for a live request; adopt the coordinate
		kRejectAsGhost = 1 << 1, // No live request or coordinate mismatch; send epoch-qualified unsubscribe + log
		kCommit        = 1 << 2, // Caller proceeds to push fullState + activate slot
	};
	using FullStateFlags_t = common::Flags<FullStateFlags>;
	FullStateFlags_t ClassifyFullState(uint8_t uiSlotIndex, uint16_t uiEpoch, GridCoord coordinate);

	enum class CoordUpdateFlags : uint8_t
	{
		kCommit    = 1 << 0, // Caller proceeds to decompress + push update
		kTrackTick = 1 << 1, // Call TrackReceivedTick (set only on kActive; skipped on kWaitingFullState)
	};
	using CoordUpdateFlags_t = common::Flags<CoordUpdateFlags>;
	CoordUpdateFlags_t ClassifyCoordinateUpdate(uint8_t uiSlotIndex, uint16_t uiEpoch);

	enum class SubscribeAcceptFlags : uint8_t
	{
		kHealEpoch   = 1 << 0, // Active slot, same coordinate: update epoch (late accept after re-subscribe)
		kCommitInitialization  = 1 << 1, // Initialize slot to kWaitingFullState
		kRejectGhost = 1 << 2, // State mismatch: send unsubscribe + logs
	};
	using SubscribeAcceptFlags_t = common::Flags<SubscribeAcceptFlags>;
	SubscribeAcceptFlags_t ClassifySubscribeAccept(uint8_t uiSlotIndex, uint16_t uiEpoch, GridCoord coordinate);

	bool IsStaleRetainedEpoch(int64_t iSlot, uint16_t uiEpoch, GridCoord coordinate) const;
	void TrackReceivedTick(int64_t iSlot, int64_t iTick);

	ENetHost* mpHost = nullptr;

	// Reused status-change decode scratch (same reused-buffer + exact-size-assign pattern as Server::mCompressionBuffer):
	// decompress into this 1024-cap buffer, then assign() the exact count into each ReceivedCoordUpdate so buffered updates carry no slack
	std::vector<game::StatusChange> mStatusChangeScratch;

	// Pipeline RTT (timestamp echo)
	int64_t miLastEchoedTimestampNanoseconds = 0;
	int64_t miHelloSendTimeNanoseconds = 0;

	// Bandwidth tracking (host-level cumulative counters)
	uint32_t muiPreviousReceivedData = 0;
	uint32_t muiPreviousSentData = 0;

	// Interarrival jitter tracking
	std::chrono::steady_clock::time_point mLastUpdateArrival {};

	// Tick-rate-locked ack cadence: last wall-clock ack send time; SendAcknowledgement throttles to one sim-tick
	// interval so packet rate is decoupled from render framerate (interval derived from kiTickRate).
	std::chrono::steady_clock::time_point mLastAcknowledgementSendTime {};

	GuidAssignedCallback mpGuidAssignedCallback = nullptr;

	std::deque<DelayedPacket> mDelayedPackets;
	NetworkSimulationState mNetworkSimulationState;
public:
	uint8_t muiCommittedLoadGeneration = 0;

private:
	std::optional<uint8_t> muiPendingLoadGeneration;
};

inline Client* gpClient = nullptr;

} // namespace engine

#endif // BT_CLIENT
