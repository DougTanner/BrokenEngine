#pragma once

#if defined(BT_CLIENT)

#include "Frame/CellStaticData.h"
#include "Frame/GridCoord.h"
#include "Network/Client/ClientSubscriptions.h"
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
	CellStaticData staticData;
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

	Client(const char* pcServerAddress, int64_t iPort, int64_t iCoordinateSlotCount, const ClientGuid& rGuid, GuidAssignedCallback pGuidAssignedCallback);
	~Client();

	template <typename TTYPE, typename... TARGS>
	void SendSimplePacket(TTYPE eType, int64_t iChannel, uint32_t uiPacketFlags, const TARGS&... rArguments)
	{
		static_assert(std::is_enum_v<TTYPE>, "SendSimplePacket type tag must be an enum (engine::PacketType or game::GamePacketType)");

		if (!(mStateFlags & ClientStateFlags::kConnected) || mpServerPeer == nullptr)
		{
			return;
		}

		NetworkManager::SendSimplePacket(mpServerPeer, eType, iChannel, uiPacketFlags, rArguments...);
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
	std::vector<std::pair<int64_t, std::vector<uint8_t>>> mReceivedGamePackets;
	std::vector<std::vector<ReceivedCoordUpdate>> mReceivedCoordinateUpdates;
	std::vector<ReceivedCoordFullState> mReceivedFullStates;
	std::vector<ReceivedStaticData> mReceivedStaticData;
	std::unique_ptr<ReceivedDebugFrame> mpReceivedDebugFrame;
	ClientSubscriptions mSubscriptions;
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
	std::optional<int64_t> DrainLoadNotification();
	void ResetAllSlots();
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
	void TrackReceivedTick(int64_t iSlot, int64_t iTick);

	ENetHost* mpHost = nullptr;

	// Reused status-change decode scratch (same reused-buffer + exact-size-assign pattern as ServerBufferedFrames::mCompressionBuffer):
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
	int64_t miCommittedLoadGeneration = 0;

private:
	std::optional<int64_t> miPendingLoadGeneration;
};

inline Client* gpClient = nullptr;

} // namespace engine

#endif // BT_CLIENT
