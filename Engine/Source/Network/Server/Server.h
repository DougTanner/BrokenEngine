#pragma once

#if defined(BT_SERVER)

#include "Frame/GridCoord.h"
#include "Network/Server/ServerBufferedFrames.h"
#include "Network/Server/ServerTypes.h"
#include "Network/NetworkCursor.h"

namespace engine
{

class ServerSessionRuntime;

} // namespace engine

namespace game
{

struct StatusChange;

} // namespace game

namespace engine
{

struct ReceivedGamePacket
{
	int64_t iClientId = 0;
	int64_t iPacketType = 0;
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
		// ServerBufferedFrames::ApplyAckStream's bounded window closes on it.
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
		int64_t iEpoch = rSlot.ack.uiEpoch;
		rSlot = {};
		rSlot.ack.uiEpoch = static_cast<uint16_t>(iEpoch);
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

	explicit Server(int64_t iPort);
	~Server();

	ClientConnection* FindClient(int64_t iClientId);
	const ClientConnection* FindClient(int64_t iClientId) const;
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
	void RecordContractViolation(int64_t iClientId, ContractViolationKind eKind, std::string_view reason, int64_t iPacketType, int64_t iSize);

	// Wire dispatch entry point for one received packet. Public so a harness rig can inject a
	// deliberately malformed packet through the real admission, dispatch, and violation path.
	void Receive(std::span<const uint8_t> packetData, ENetPeer* pPeer);

private:
	friend class ServerBufferedFrames;
	friend class ServerSessionRuntime;
	void Poll(const NetworkTimeState& rTimeState, ServerPollMode ePollMode);
	void Flush();

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

	void RemoveClient(int64_t iClientId);

	ENetHost* mpHost = nullptr;
	int64_t miNextClientId = 1;

	// Budget stall grace (see Server::Poll): gate 2 records no violation before mBudgetGraceDeadline.
	std::chrono::steady_clock::time_point mPreviousPollEnd {};
	std::chrono::steady_clock::time_point mBudgetGraceDeadline {};
	int64_t miLoadGeneration = 0;

	ServerBufferedFrames mBufferedFrames = ServerBufferedFrames(*this);

	// Network simulation delay queue
	std::deque<DelayedPacket> mDelayedPackets;
	NetworkSimulationState mNetworkSimulationState;
};

inline Server* gpServer = nullptr;

} // namespace engine

#endif // BT_SERVER
