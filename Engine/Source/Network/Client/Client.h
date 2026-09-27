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

// Per-coord received update (single coord, not multi-coord)
struct ReceivedCoordUpdate
{
	int64_t iTick = 0;
	common::crc_t sharedCrc = 0;
	// Heap: ENet packet data, variable per frame
	std::vector<game::StatusChange> statusChanges;
};

// Per-coord received full state
struct ReceivedCoordFullState
{
	int64_t iTick = 0;
	GridCoord coord {};
	std::unique_ptr<game::Frame> pFrame;
};

// Per-coord received static data (sent once per subscription)
struct ReceivedStaticData
{
	GridCoord coord {};
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
	GridCoord coord {};
	CoordSubscriptionState eState = CoordSubscriptionState::kUnsubscribed;
	AckState ackState;
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
	GridCoord coord {};
	std::chrono::steady_clock::time_point startTime {};
	common::Flags<SubscribeRequestFlags> flags;
};

// Subscribe requests in send order. Per coord: zero or more cancelled records, then at most one live record.
// The server answers every subscribe exactly once on the reliable channel, which is FIFO, so each accept or
// reject consumes the coord's oldest record. Full state and static data only consult records.
class SubscribeRequests
{
public:
	void Add(GridCoord coord);
	void Cancel(GridCoord coord);
	// Warns once per live record left unanswered past the timeout; the record stays live until its answer
	void WarnTimedOut(std::chrono::steady_clock::time_point now);
	// Removes the coord's oldest record; true only if that record was live
	bool TakeAnswer(GridCoord coord);
	bool IsLive(GridCoord coord) const;
	std::span<const SubscribeRequest> Records() const;
	void Clear();

private:
	std::vector<SubscribeRequest> mRecords;
};

struct ReceivedDebugFrame
{
	int64_t iTick = 0;
	GridCoord coord {};
	std::unique_ptr<game::Frame> pFrame;
};

class Client
{
public:

	using GuidAssignedCallback = void (*)(const ClientGuid&);

	Client(const char* pServerAddress, uint16_t uiPort, int64_t iCoordSlots, const ClientGuid& rGuid, GuidAssignedCallback pfnGuidAssigned);
	~Client();

	template <typename TTYPE, typename... TARGS>
	void SendSimplePacket(TTYPE eType, uint8_t uiChannel, uint32_t uiPacketFlags, const TARGS&... args)
	{
		static_assert(std::is_enum_v<TTYPE>, "SendSimplePacket type tag must be an enum (engine::PacketType or game::GamePacketType)");

		if (!(mStateFlags & ClientStateFlags::kConnected) || mpServerPeer == nullptr)
		{
			return;
		}

		NetworkManager::SendSimplePacket(mpServerPeer, eType, uiChannel, uiPacketFlags, args...);
	}

	void SendDesyncReport(int64_t iTick, GridCoord coord, common::crc_t expected, common::crc_t actual);
	void SendDebugFrameRequest(int64_t iTick, GridCoord coord);
	void SendResyncRequest();
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
		kDesyncDebugMode          = 1 << 4,
		kLoadNotificationReceived = 1 << 5,
		kSkipNextJitterInterval   = 1 << 6,
		kServerDebugInput         = 1 << 7, // the accepting server takes debug-control requests (its kbDebugInput)
	};

	ENetPeer* mpServerPeer = nullptr;
	common::Flags<ClientStateFlags> mStateFlags;
	char mpcRejectionReason[256] = {};

	// Heap: raw game packet buffer grows on assign/player-state packets
	std::vector<std::pair<uint8_t, std::vector<uint8_t>>> mReceivedGamePackets;
	std::vector<std::vector<ReceivedCoordUpdate>> mReceivedCoordUpdates;
	std::vector<ReceivedCoordFullState> mReceivedFullStates;
	std::vector<ReceivedStaticData> mReceivedStaticData;
	std::unique_ptr<ReceivedDebugFrame> mpReceivedDebugFrame;
	std::vector<ClientCoordSlot> mCoordSlots;
	SubscribeRequests mSubscribeRequests;
	common::Smoothed<int64_t> mSmoothedPipelineRttUs;
	common::InTheLastSecond mBytesInPerSecond;
	common::InTheLastSecond mBytesOutPerSecond;
	common::InTheLastSecond mFramesReceived;
	common::Smoothed<int64_t> mSmoothedJitterUs;
	ClientGuid mClientGuid {};
	NetworkTimeState mTimeState {};

private:
	friend class ClientSessionRuntime;
	void Poll(const NetworkTimeState& rTimeState);
	bool SendAck();
	bool SendSubscribe(GridCoord coord);
	void SendUnsubscribe(int64_t iSlot);
	void Flush();
	std::optional<uint8_t> DrainLoadNotification();
	void RecoverTimedOutSubscriptions();
	void ResetAllSlots();
	void FreeSlot(int64_t iSlot);
	void DispatchIncoming(ENetEvent& rEvent, bool bFastForward);
	void Receive(ENetEvent& rEvent);
	void ServerCoordFullState(std::span<const uint8_t> packetData);
	void ServerCoordStaticData(std::span<const uint8_t> packetData);
	void ServerCoordUpdateOrResend(std::span<const uint8_t> packetData, bool bProcessRtt);
	void ServerDebugFrame(std::span<const uint8_t> packetData);
	void ServerConnectionResponse(std::span<const uint8_t> packetData);
	void ServerSubscribeAccept(std::span<const uint8_t> packetData);
	void ServerUnsubscribeAck(std::span<const uint8_t> packetData);
	void ServerLoadNotification(std::span<const uint8_t> packetData);
	void ServerTimespeedUpdate(std::span<const uint8_t> packetData);
	void SendHello();

	enum class FullStateFlags : uint8_t
	{
		kAdoptCoord    = 1 << 0, // Full state arrived before SubscribeAccept for a live request; adopt the coord
		kRejectAsGhost = 1 << 1, // No live request or coord mismatch; send epoch-qualified unsubscribe + log
		kCommit        = 1 << 2, // Caller proceeds to push fullState + activate slot
	};
	using FullStateFlags_t = common::Flags<FullStateFlags>;
	FullStateFlags_t ClassifyFullState(uint8_t uiSlotIndex, uint16_t uiEpoch, GridCoord coord);

	enum class CoordUpdateFlags : uint8_t
	{
		kCommit    = 1 << 0, // Caller proceeds to decompress + push update
		kTrackTick = 1 << 1, // Call TrackReceivedTick (set only on kActive; skipped on kWaitingFullState)
	};
	using CoordUpdateFlags_t = common::Flags<CoordUpdateFlags>;
	CoordUpdateFlags_t ClassifyCoordUpdate(uint8_t uiSlotIndex, uint16_t uiEpoch);

	enum class SubscribeAcceptFlags : uint8_t
	{
		kHealEpoch   = 1 << 0, // Active slot, same coord: update epoch (late accept after re-subscribe)
		kCommitInit  = 1 << 1, // Initialize slot to kWaitingFullState
		kRejectGhost = 1 << 2, // State mismatch: send unsubscribe + logs
	};
	using SubscribeAcceptFlags_t = common::Flags<SubscribeAcceptFlags>;
	SubscribeAcceptFlags_t ClassifySubscribeAccept(uint8_t uiSlotIndex, uint16_t uiEpoch, GridCoord coord);

	bool IsStaleRetainedEpoch(int64_t iSlot, uint16_t uiEpoch, GridCoord coord) const;
	void TrackReceivedTick(int64_t iSlot, int64_t iTick);

	ENetHost* mpHost = nullptr;

	// Reused status-change decode scratch (same reused-buffer + exact-size-assign pattern as Server::mCompressionBuffer):
	// decompress into this 1024-cap buffer, then assign() the exact count into each ReceivedCoordUpdate so buffered updates carry no slack
	std::vector<game::StatusChange> mStatusChangeScratch;

	// Pipeline RTT (timestamp echo)
	int64_t miLastEchoedTimestampNs = 0;
	int64_t miHelloSendTimeNs = 0;

	// Bandwidth tracking (host-level cumulative counters)
	uint32_t muiPrevReceivedData = 0;
	uint32_t muiPrevSentData = 0;

	// Interarrival jitter tracking
	std::chrono::steady_clock::time_point mLastUpdateArrival {};

	// Tick-rate-locked ack cadence: last wall-clock ack send time; SendAck throttles to one sim-tick
	// interval so packet rate is decoupled from render framerate (interval derived from kiTickRate).
	std::chrono::steady_clock::time_point mLastAckSendTime {};

	GuidAssignedCallback mpfnGuidAssigned = nullptr;

	// Network simulation delay queue
	std::deque<DelayedPacket> mDelayedPackets;
	NetworkSimulationState mNetworkSimState;
public:
	uint8_t muiCommittedLoadGeneration = 0;

private:
	std::optional<uint8_t> muiPendingLoadGeneration;
};

inline Client* gpClient = nullptr;

} // namespace engine

#endif // BT_CLIENT
