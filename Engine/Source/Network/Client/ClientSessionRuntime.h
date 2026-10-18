#pragma once

#if defined(BT_CLIENT)

#include "Frame/GridCoord.h"
#include "Network/Client/Client.h"

namespace engine
{

class NetworkDiscoveryScanner;

enum class ClientSessionStateFlags : uint8_t
{
	kServerDiscovered           = 1 << 0,
	kDiscoveryScanTimedOut      = 1 << 1,
	kNoFreeSlotLogged           = 1 << 2,
	kConnectionWasAccepted      = 1 << 3,
	// Harness-only viewer pin: set by set_client_grid_coord, cleared by release_client_grid_coord or GameBase::ResetSession.
	// Survives server load and replay restart so a recipe can hold one cell subscribed across playback loops.
	kClientGridCoordinatePinned = 1 << 4,
};

enum class StandardMenuState : uint8_t
{
	kClientPresent           = 0x01,
	kDiscoveryScannerPresent = 0x02,
	kServerDiscovered        = 0x04,
	kConnectionAccepted      = 0x08,
	kAutoConnect             = 0x10,
};
using StandardMenuState_t = common::Flags<StandardMenuState>;

class ClientSessionRuntime
{
public:

	ClientSessionRuntime();
	~ClientSessionRuntime();

	void Connect(std::string_view serverAddress, int64_t iPort);
	void ConnectToDiscoveredServer(int64_t iPort);
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
		if (!(mpClient->mStateFlags & Client::ClientStateFlags::kConnected))
		{
			return;
		}

		std::optional<common::LogTickScope> optionalTickScope;
		InitializeLogTickScope(optionalTickScope);

		rLogFunction();
		mpClient->SendSimplePacket(ePacketType, NetworkManager::kiChannelReliable, ENET_PACKET_FLAG_RELIABLE, rArguments...);

		{
			// Send the user command now instead of waiting for the tick-cadence ack flush.
			// Heap: ENet may allocate while flushing outgoing commands
			ScopedSuppressAllocationTracking suppress;
			mpClient->Flush();
		}
	}

	void SetDesiredCoordinates(std::span<const GridCoord> desiredCoordinates, std::string_view reason, int64_t iTick);
	void UpdateDesiredCoordinates(std::string_view reason);
	void SynchronizeSubscriptions();
	void ClearSubscriptionState();

	void ComputeActiveSet();
	void PinClientGridCoordinate(GridCoord coordinate);
	StandardMenuState_t GetStandardMenuState() const;

	int64_t GetConfirmedTick() const;
	int64_t GetClientConfirmedTick() const;
	int64_t GetServerUpdateBufferSize() const;

	std::chrono::nanoseconds EvaluateClock(int64_t iPreReconcileTick);
	// Pass the tick sampled before reconciliation.
	void ApplyClockCorrection(int64_t iPreReconcileTick);
	void ResetClock();
	// The clock reference: -1 exactly when miLatestServerTick < 0, otherwise never below it.
	int64_t EstimatedLatestServerTick() const;

	std::unique_ptr<Client> mpClient;
	std::unique_ptr<NetworkDiscoveryScanner> mpDiscoveryScanner;
	common::Flags<ClientSessionStateFlags> mStateFlags;
	char mpcDiscoveredAddress[16] {};
	int64_t miLatestServerTick = -1;
	// Arrival time of miLatestServerTick, backdated by the estimate's lead over that tick when it arrived.
	std::chrono::steady_clock::time_point mLatestServerTickArrival {};
	int64_t miClockError = 0;
	int64_t miClockOffset = 0;
	int64_t miClockTargetBehind = 0;
	int64_t miCurrentTargetBehind = 0;
	std::vector<GridCoord> mDesiredCoordinates;
	std::vector<GridCoord> mSubscriptionQueue;
	std::unordered_map<GridCoord, std::chrono::steady_clock::time_point> mUnwantedTimestamps;
	GridCoord mpVisibleNeighbors[8] {};
	int64_t miVisibleNeighborCount = 0;

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
