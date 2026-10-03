#pragma once

#if defined(BT_SERVER)

#include "Frame/GridCoord.h"
#include "Network/Server/ServerTypes.h"
#include "Network/NetworkCursor.h"

namespace engine
{

struct FrameStaticData;
class ServerSessionRuntime;

} // namespace engine

namespace game
{

struct Frame;
struct StatusChange;

} // namespace game

namespace engine
{

struct ReceivedGamePacket
{
	int64_t iClientId = 0;
	uint8_t uiPacketType = 0;
	// Heap: raw game packet payload (type byte stripped)
	std::vector<uint8_t> payload;
};

struct ClientConnection
{
	struct SlotState
	{
		ClientCoordSubscription subscription {};
		AckState ack {};
		int64_t iPreviousResendCount = 0;
		int64_t iResendLogCooldown = 0;
		// Tick of the last full state sent on this slot, until the client's re-baselined ACK floor arrives or
		// Server::ClientAcknowledgementStream's bounded window closes on it.
		// Server-local: the client derives the same tick from the full state itself, so this stays off the wire.
		int64_t iPendingFullStateTick = -1;
		bool bHoldUpdatesUntilFullStateAck = false;
	};

	ENetPeer* pPeer = nullptr;
	int64_t iClientId = 0;
	bool bHandshakeComplete = false;
	std::vector<GridCoord> authorizedCoordinates;
	ClientGuid clientGuid {};

	// Slot-based coord subscriptions with independent ACK and resend tracking
	std::vector<SlotState> slots;

	// Pipeline RTT: echoed back to client in update packets
	int64_t iClientTimestampNanoseconds = 0;

	// Delta-only floor advance logging: consecutive zero-advance ACK count
	int64_t iConsecutiveZeroAdvanceAcks = 0;
	bool bFloorStalled = false;
	int64_t iPeakConsecutiveStallAcks = 0;

	// Independent wall-clock limits for expensive desync diagnostics
	std::chrono::steady_clock::time_point desynchronizationReportDeadline {};
	std::chrono::steady_clock::time_point debugFrameRequestDeadline {};

	// Client->server contract enforcement (see NetworkProtocol.h / Server::RecordContractViolation)
	int64_t iCorruptViolations = 0;             // lifetime, never reset
	int64_t iRateViolations = 0;                // too-fast strikes still outstanding after decay
	std::chrono::steady_clock::time_point rateViolationDecayStart {}; // start of the rate decay interval in progress
	int64_t iTickPacketCount = 0;              // reset per update window (Server::Poll, kUpdateStart only)
	int64_t iTickByteCount = 0;                 // reset per update window (Server::Poll, kUpdateStart only)
	uint16_t uiTickTypeCounts[256] {}; // per-type count this update window, indexed by raw type byte; reset in Poll (kUpdateStart only)

	int64_t FindSlotForCoordinate(GridCoord coordinate) const
	{
		for (int64_t i = 0; i < std::ssize(slots); ++i)
		{
			if ((slots.at(i).subscription.flags & SubscriptionFlags::kActive) && slots.at(i).subscription.coordinate == coordinate)
			{
				return i;
			}
		}
		return -1;
	}

	int64_t AllocateSlot(int64_t iMaxSlots)
	{
		int64_t iLimit = std::min(iMaxSlots, std::ssize(slots));
		for (int64_t i = 0; i < iLimit; ++i)
		{
			if (!(slots.at(i).subscription.flags & SubscriptionFlags::kActive))
			{
				return i;
			}
		}
		return -1;
	}

	void FreeSlot(int64_t iSlot)
	{
		SlotState& rSlot = slots.at(iSlot);
		// Reset ACK state but preserve epoch (incremented on next allocation)
		uint16_t uiEpoch = rSlot.ack.uiEpoch;
		rSlot = {};
		rSlot.ack.uiEpoch = uiEpoch;
	}

};

// Per-coord ring buffer entry for re-send support
struct PerCoordBufferedFrame
{
	int64_t iTick = 0;
	common::crc_t uiSharedCrc = 0;
	// Heap: variable-size compressed status change data per frame
	std::vector<uint8_t> compressedData;
};

struct BufferedFullFrame
{
	int64_t iTick = 0;
	// Heap: serialized frame data per grid coordinate for debug frame requests
	std::unordered_map<GridCoord, std::string> serializedFrames;
};

// Server main thread only. Set mpTarget to the destination before writing; appending retains its
// capacity across serializations.
class StringAppendStreamBuf : public std::streambuf
{
public:

	std::string* mpTarget = nullptr;

protected:

	int_type overflow(int_type iChar) override
	{
		if (iChar != traits_type::eof())
		{
			mpTarget->push_back(static_cast<char>(iChar));
		}
		return traits_type::not_eof(iChar);
	}

	std::streamsize xsputn(const char_type* pData, std::streamsize iCount) override
	{
		mpTarget->append(pData, static_cast<size_t>(iCount));
		return iCount;
	}
};

// Which of an update's two Server::Poll calls is running. The tick-boundary poll continues the admission
// budget window the update-start poll opened, so a hostile client gets one budget window per update.
enum class ServerPollMode : uint8_t
{
	kUpdateStart,
	kTickBoundary,
};

// Which violation count a client->server contract violation charges: corrupt data, or messages sent too fast.
enum class ContractViolationKind : uint8_t
{
	kCorrupt,
	kRate,
};

class Server
{
public:

	explicit Server(uint16_t uiPort);
	~Server();

	ClientConnection* FindClient(int64_t iClientId);
	const ClientConnection* FindClient(int64_t iClientId) const;
	void SendCoordinateFullState(int64_t iClientId, int64_t iSlot, int64_t iTick, GridCoord coordinate, const game::Frame* pFrame);
	void SendCoordinateStaticData(int64_t iClientId, int64_t iSlot, GridCoord coordinate, const FrameStaticData& rStaticData);
	void AdvanceLoadGeneration();
	void BroadcastLoadNotification();
	// Consume-once: sends only when TimeStep recorded an applied time-scale change since the last call.
	void BroadcastTimespeedIfChanged();

	std::vector<ClientConnection> mClients;
	std::vector<PendingDisconnect> mPendingDisconnects;
	std::vector<PendingNewSubscription> mPendingNewSubscriptions;
	std::vector<int64_t> mPendingResynchronizationClientIds;
	std::vector<ReceivedGamePacket> mReceivedGamePackets;

	bool AdmitGamePacket(const ReceivedGamePacket& rPacket, const ClientPacketContract& rContract);

	// Records a client->server contract violation against eKind's count; escalates to disconnect at
	// kiCorruptViolationDisconnectCount corrupt or kiRateViolationDisconnectCount outstanding rate violations.
	// Callers MUST NOT touch their ClientConnection* afterward -- the client may have been removed.
	void RecordContractViolation(int64_t iClientId, ContractViolationKind eKind, std::string_view reason, uint8_t uiPacketType, int64_t iSize);

	// Wire dispatch entry point for one received packet. Public so a harness fixture can inject a
	// deliberately malformed packet through the real admission, dispatch, and violation path.
	void Receive(std::span<const uint8_t> packetData, ENetPeer* pPeer);

private:
	friend class ServerSessionRuntime;
	void Poll(const NetworkTimeState& rTimeState, ServerPollMode ePollMode);
	void BufferFrame(int64_t iTick, std::span<const std::pair<GridCoord, GridUpdateData>> gridUpdates);
	void BufferFullFrame(int64_t iTick, std::span<const std::pair<GridCoord, const game::Frame*>> frames);
	void SendUpdate(ClientConnection& rClient, int64_t iTick);
	void SendResends(ClientConnection& rClient, int64_t iTick);
	void Flush();
	void ClearBufferedFrames();

	void Connect(const ENetEvent& rEvent);
	void Disconnect(const ENetEvent& rEvent);
	void DispatchIncoming(ENetEvent& rEvent, bool bFastForward);

	ClientConnection* FindHandshakenClient(int64_t iClientId);

	void ClientAcknowledgementStream(std::span<const uint8_t> packetData, int64_t iClientId);
	void ClientDesynchronizationReport(std::span<const uint8_t> packetData, int64_t iClientId);
	void ClientDebugFrameRequest(std::span<const uint8_t> packetData, ENetPeer* pPeer, int64_t iClientId);
	void ClientHello(std::span<const uint8_t> packetData, ENetPeer* pPeer, int64_t iClientId);
	void RejectHello(ENetPeer* pPeer, int64_t iClientId, const char* pcMessage);
	void ClientSubscribe(std::span<const uint8_t> packetData, int64_t iClientId);
	void ClientUnsubscribe(std::span<const uint8_t> packetData, int64_t iClientId);
	void ClientResynchronizationRequest(std::span<const uint8_t> packetData, int64_t iClientId);
	void SendConnectionResponse(ENetPeer* pPeer, bool bAccepted, const char* pcMessage, const ClientGuid* pGuid);
	void SendSubscribeAccept(const ClientConnection& rClient, int64_t iSlot, GridCoord coordinate);
	void SendTimespeedUpdate(ENetPeer* pPeer, int64_t iMultiply, int64_t iDivide);

	void WriteBufferedFramePacket(common::Workbuffer& rWorkbuffer, PacketType eType, int64_t iSlot, uint16_t uiEpoch, const PerCoordBufferedFrame& rBuffered, int64_t iTimestampNanoseconds);
	const PerCoordBufferedFrame* FindBufferedFrame(GridCoord coordinate, int64_t iTick) const;
	int CompressToBuffer(std::span<const char> data);
	void RemoveClient(int64_t iClientId);

	void UpdateResendLogState(ClientConnection& rClient, int64_t iSlot, int64_t iSlotResendCount, GridCoord coordinate);

	ENetHost* mpHost = nullptr;
	int64_t miNextClientId = 1;

	// Budget stall grace (see Server::Poll): gate 2 records no violation before mBudgetGraceDeadline.
	std::chrono::steady_clock::time_point mPreviousPollEnd {};
	std::chrono::steady_clock::time_point mBudgetGraceDeadline {};
	uint8_t muiLoadGeneration = 0;

	// Per-coord ring buffers for re-sends
	std::unordered_map<GridCoord, std::deque<PerCoordBufferedFrame>> mPerCoordinateBufferedFrames;
	int64_t miLatestBufferedTick = -1;

	// Ring buffer for debug frame requests
	std::deque<BufferedFullFrame> mBufferedFullFrames;

	// Compression scratch buffer (reused across BufferFrame calls)
	std::vector<uint8_t> mCompressionBuffer;

	// Server main thread only. mFrameStream writes to mFrameStreamBuffer.mpTarget: recycled full-frame pool
	// entries or mSendScratch for transient coordinate sends.
	StringAppendStreamBuf mFrameStreamBuffer;
	std::ostream mFrameStream = std::ostream(&mFrameStreamBuffer);
	std::string mSendScratch;
	// Heap: recycled per-coord buffers for the full-frame ring, reused across BufferFullFrame calls
	std::vector<std::string> mFullFramePool;

	// Network simulation delay queue
	std::deque<DelayedPacket> mDelayedPackets;
	NetworkSimulationState mNetworkSimulationState;
};

inline Server* gpServer = nullptr;

} // namespace engine

#endif // BT_SERVER
