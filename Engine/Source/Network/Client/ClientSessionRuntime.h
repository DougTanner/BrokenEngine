#pragma once

#if defined(BT_CLIENT)

#include "Frame/GridCoord.h"
#include "Network/Client/Client.h"

namespace game
{

class ClientSession;

} // namespace game

namespace engine
{

class NetworkDiscoveryScanner;

enum class ClientSessionStateFlags : uint8_t
{
	kServerDiscovered      = 1 << 0,
	kDiscoveryScanTimedOut = 1 << 1,
	kNoFreeSlotLogged      = 1 << 2,
};

class ClientSessionRuntime
{
public:

	explicit ClientSessionRuntime(game::ClientSession& rSession);
	~ClientSessionRuntime();

	void Connect(std::string_view serverAddress, uint16_t uiPort, int64_t iCoordinateSlots);
	void ConnectToDiscoveredServer(uint16_t uiPort, int64_t iCoordinateSlots);
	void Disconnect();
	void StartDiscovery();
	void PollDiscovery();
	void PollAndDrain(const NetworkTimeState& rTimeState);

	template <typename TPACKETTYPE, typename TLOGFUNCTION, typename... TARGUMENTS>
	void SendGameRequest(TPACKETTYPE ePacketType, const TLOGFUNCTION& rLogFunction, const TARGUMENTS&... rArguments)
	{
		if (mpClient == nullptr)
		{
			return;
		}
		if (!(mpClient->mStateFlags & Client::ClientStateFlags::kConnected) || mpClient->mpServerPeer == nullptr)
		{
			return;
		}

		std::optional<common::LogTickScope> optionalTickScope;
		InitializeLogTickScope(optionalTickScope);

		rLogFunction();
		mpClient->SendSimplePacket(ePacketType, NetworkManager::kuiChannelReliable, ENET_PACKET_FLAG_RELIABLE, rArguments...);

		{
			// Send the user command now instead of waiting for the tick-cadence ack flush.
			// Heap: ENet may allocate while flushing outgoing commands
			ScopedSuppressAllocationTracking suppress;
			mpClient->Flush();
		}
	}

	void SetDesiredCoordinates(std::span<const GridCoord> desiredCoordinates, std::string_view reason, int64_t iTick);
	void SynchronizeSubscriptions();
	void ClearSubscriptionState();

	int64_t GetConfirmedTick() const;
	int64_t GetClientConfirmedTick() const;
	int64_t GetServerUpdateBufferSize() const;

	std::chrono::nanoseconds EvaluateClock(int64_t iPreReconcileTick);
	// Pass the tick sampled before reconciliation.
	void ApplyClockCorrection(int64_t iPreReconcileTick);
	void ResetClock();

	game::ClientSession& mrSession;
	std::unique_ptr<Client> mpClient;
	std::unique_ptr<NetworkDiscoveryScanner> mpDiscoveryScanner;
	common::Flags<ClientSessionStateFlags> mStateFlags;
	char mcDiscoveredAddress[16] {};
	int64_t miLatestServerTick = -1;
	int64_t miClockError = 0;
	int64_t miClockOffset = 0;
	int64_t miClockTargetBehind = 0;
	int64_t miCurrentTargetBehind = 0;
	std::vector<GridCoord> mDesiredCoordinates;
	std::vector<GridCoord> mSubscriptionQueue;
	std::unordered_map<GridCoord, std::chrono::steady_clock::time_point> mUnwantedTimestamps;

private:

	void ResetForConnect();
	void ResetForServerLoad();
	void UnsubscribeStaleCoordinates(std::span<const GridCoord> desiredCoordinates);
	void BuildSubscriptionQueue(std::span<const GridCoord> desiredCoordinates);
	void TrySubscribeNext();
	void SendAckAndFlush();
	void ApplyReceivedFullStates();
	bool ApplyReceivedUpdates();
	void InitializeLogTickScope(std::optional<common::LogTickScope>& rOptionalTickScope) const;

	int64_t miLastLoggedClockTargetBehind = -1;
	int64_t miLastPeriodicClockLogTick = -1;
	int64_t miLastClockErrorLogTick = -1;
	// Sim tick the current "computed target is lower" streak started on; -1 when no streak is active.
	int64_t miLowerTargetBehindStreakStartTick = -1;
	// Sim tick the previous streak evaluation observed; -1 when no tick has been observed yet.
	int64_t miLastEvaluateClockTick = -1;
	static constexpr std::chrono::seconds kStickySubscriptionDuration = 2s;
};

} // namespace engine

#endif // BT_CLIENT
