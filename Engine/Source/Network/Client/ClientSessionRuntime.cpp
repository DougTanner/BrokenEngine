#include "Pch.h"

#include "Network/Client/ClientSessionRuntime.h"

#if defined(BT_CLIENT)

#include "Agent/Commands/ClientNetworkFixtures.h"
#include "Network/Client/Client.h"
#include "Network/NetworkDiscoveryScanner.h"

#include "Network/Client/ClientSession.h"
#include "Profile/ProfileManager.h"
#include "Game.h"

namespace engine
{

// The file wrapper owns the on-disk version; the version-and-size header precedes the bare GUID body.
struct ClientGuidFile
{
	static constexpr int64_t kiVersion = 2;

	ClientGuid guid {};
};

static_assert(sizeof(ClientGuidFile) == sizeof(ClientGuid), "ClientGuidFile must stay a pure 16-byte body — existing ClientGuid.bin files carry no extra field");
static_assert(alignof(ClientGuidFile) == alignof(ClientGuid), "ClientGuidFile alignment diverged from ClientGuid");
static_assert(BT_OFFSETOF(ClientGuidFile, guid) == 0, "ClientGuidFile::guid must start at offset 0 — the file body is the bare GUID");

static ClientGuid LoadClientGuidFromDisk()
{
	// Heap: filesystem path and file stream operations for GUID persistence
	ScopedSuppressAllocationTracking suppress;
	ClientGuidFile loadedFile {};
	if (ReadVersionedFile({FileFlags::kAppDataDirectory, FileFlags::kRead}, std::filesystem::path("ClientGuid.bin"), loadedFile) && (loadedFile.guid.uiHigh != 0 || loadedFile.guid.uiLow != 0))
	{
		LOG(kNetwork, kInfo, "ClientSessionRuntime loaded GUID from disk: {} {}", loadedFile.guid.uiHigh, loadedFile.guid.uiLow);
		return loadedFile.guid;
	}
	return {};
}

static void PersistClientGuidToDisk(const ClientGuid& rGuid)
{
	// Heap: filesystem path and file stream operations for GUID persistence
	ScopedSuppressAllocationTracking suppress;
	ClientGuidFile guidFile {.guid = rGuid};
	if (!WriteVersionedFile({FileFlags::kAppDataDirectory, FileFlags::kWrite}, std::filesystem::path("ClientGuid.bin"), guidFile))
	{
		LOG(kNetwork, kError, "Failed to persist ClientGuid.bin (next session will re-handshake as a new client)");
	}
}

// Lowering miCurrentTargetBehind waits out one mSmoothedJitterMicroseconds window's worth of sim ticks, so in
// loss-free arrival the average has mostly turned over before the lower target is adopted. Measured in
// sim ticks, not EvaluateClock calls: jitter samples arrive one per coord update, so they land at tick
// cadence while EvaluateClock runs at render cadence. Packet loss yields fewer than one sample per tick,
// so the window is an approximation, not a guarantee that every higher-jitter sample has aged out.
constexpr int64_t kiLowerTargetBehindStreakTicks = decltype(Client::mSmoothedJitterMicroseconds)::kiCapacity;

static bool IsSlotActive(const ClientCoordSlot& rSlot)
{
	return rSlot.eState != CoordSubscriptionState::kUnsubscribed && rSlot.eState != CoordSubscriptionState::kUnsubscribing;
}

static bool ContainsCoordinate(std::span<const GridCoord> coordinates, GridCoord coordinate)
{
	for (const GridCoord& rCoordinate : coordinates)
	{
		if (rCoordinate == coordinate)
		{
			return true;
		}
	}
	return false;
}

static ClientNetworkFixtures::CoordUpdateState QueryFixtureCoordinateUpdateState(GridCoord coordinate, int64_t iTick)
{
	auto it = game::gpGame->mCoordinateFrames.find(coordinate);
	if (it == game::gpGame->mCoordinateFrames.end())
	{
		return {};
	}
	ClientNetworkFixtures::CoordUpdateState state {.iConfirmedTick = it->second.iConfirmedTick};
	state.flags.Set(ClientNetworkFixtures::CoordUpdateFlags::kPresent);
	state.flags.Set(ClientNetworkFixtures::CoordUpdateFlags::kUpdateRetained, it->second.serverUpdates.contains(iTick));
	return state;
}

ClientSessionRuntime::ClientSessionRuntime(game::ClientSession& rSession)
:	mrSession(rSession)
{
}

ClientSessionRuntime::~ClientSessionRuntime() = default;

void ClientSessionRuntime::InitializeLogTickScope(std::optional<common::LogTickScope>& rOptionalTickScope) const
{
	if (common::gpThreadLocal->miLogTickCounter < 0)
	{
		rOptionalTickScope.emplace(game::gpGame->miTickCounter);
	}
}

void ClientSessionRuntime::ResetClock()
{
	miLatestServerTick = -1;
	miClockError = 0;
	miClockOffset = 0;
	miClockTargetBehind = 0;
	miCurrentTargetBehind = 0;
	miLowerTargetBehindStreakStartTick = -1;
	miLastEvaluateClockTick = -1;
	miLastLoggedClockTargetBehind = -1;
	miLastPeriodicClockLogTick = -1;
	miLastClockErrorLogTick = -1;
}

void ClientSessionRuntime::ResetForConnect()
{
	ResetClock();
	ClearSubscriptionState();
	mpDiscoveryScanner.reset();
	mStateFlags.Set(ClientSessionStateFlags::kServerDiscovered, false);
	mStateFlags.Set(ClientSessionStateFlags::kDiscoveryScanTimedOut, false);
	mStateFlags.Set(ClientSessionStateFlags::kNoFreeSlotLogged, false);
}

void ClientSessionRuntime::Connect(std::string_view serverAddress, uint16_t uiPort, int64_t iCoordinateSlots)
{
	// Heap: address copy, GUID file I/O, and Client/ENet construction
	ScopedSuppressAllocationTracking suppress;
	ResetForConnect();
	std::string serverAddressString(serverAddress);
	ClientGuid clientGuid = LoadClientGuidFromDisk();
	mpClient = std::make_unique<Client>(serverAddressString.c_str(), uiPort, iCoordinateSlots, clientGuid, &PersistClientGuidToDisk);
	if (mpClient->mpHost == nullptr)
	{
		mrSession.OnConnectionFailed();
		Disconnect();
	}
}

void ClientSessionRuntime::ConnectToDiscoveredServer(uint16_t uiPort, int64_t iCoordinateSlots)
{
	mStateFlags.Set(ClientSessionStateFlags::kServerDiscovered, false);
	Connect(mcDiscoveredAddress, uiPort, iCoordinateSlots);
}

void ClientSessionRuntime::Disconnect()
{
	// Heap: Client/ENet and discovery teardown may allocate during cleanup
	ScopedSuppressAllocationTracking suppress;
	ClientNetworkFixtures::Detach();
	mpClient.reset();
	mpDiscoveryScanner.reset();
	ResetClock();
	ClearSubscriptionState();
	mStateFlags.Set(ClientSessionStateFlags::kServerDiscovered, false);
	mStateFlags.Set(ClientSessionStateFlags::kDiscoveryScanTimedOut, false);
	mStateFlags.Set(ClientSessionStateFlags::kNoFreeSlotLogged, false);
	mrSession.OnRuntimeDisconnected();
}

void ClientSessionRuntime::StartDiscovery()
{
	// Heap: discovery scanner and UDP socket construction
	ScopedSuppressAllocationTracking suppress;
	mpDiscoveryScanner = std::make_unique<NetworkDiscoveryScanner>();
	mpDiscoveryScanner->StartScan();
}

void ClientSessionRuntime::PollDiscovery()
{
	if (mpDiscoveryScanner == nullptr)
	{
		return;
	}
	mpDiscoveryScanner->Poll();
	if ((mpDiscoveryScanner->mFlags & NetworkDiscoveryScanner::DiscoveryScannerFlags::kFound))
	{
		std::snprintf(mcDiscoveredAddress, sizeof(mcDiscoveredAddress), "%s", mpDiscoveryScanner->mpcFoundAddress);
		mpDiscoveryScanner.reset();
		mStateFlags.Set(ClientSessionStateFlags::kServerDiscovered);
	}
	else if (!mpDiscoveryScanner->IsScanning())
	{
		mStateFlags.Set(ClientSessionStateFlags::kDiscoveryScanTimedOut);
		mpDiscoveryScanner.reset();
		StartDiscovery();
	}
}

void ClientSessionRuntime::ResetForServerLoad()
{
	ResetClock();
	mpClient->mSmoothedJitterMicroseconds = common::Smoothed<int64_t>();
	mpClient->mStateFlags.Set(Client::ClientStateFlags::kHasLastUpdateArrival, false);
	mpClient->mStateFlags.Set(Client::ClientStateFlags::kSkipNextJitterInterval);
	ClearSubscriptionState();
	mpClient->ResetAllSlots();
	mpClient->mReceivedFullStates.clear();
	mpClient->mReceivedStaticData.clear();
	for (std::vector<ReceivedCoordUpdate>& rSlotUpdates : mpClient->mReceivedCoordinateUpdates)
	{
		rSlotUpdates.clear();
	}

	// Coord-channel packets still delayed by the network simulation belong to pre-load subscriptions;
	// the control channel is left alone because it carries the load notification and handshake traffic
	if constexpr (keNetworkSimulation != engine::NetworkSimulationLevel::kDisabled)
	{
		for (int64_t i = 0; i < std::ssize(mpClient->mSubscriptions.mCoordinateSlots); ++i)
		{
			NetworkSimulation::PurgeDelayedForSlot(mpClient->mDelayedPackets, i);
		}
	}
}

void ClientSessionRuntime::PollAndDrain(const NetworkTimeState& rTimeState)
{
	ASSERT(common::gpMultithreading->IsMainThread());
	PollDiscovery();
	if (mpClient == nullptr)
	{
		return;
	}

	mpClient->Poll(rTimeState);
	if (!(mpClient->mStateFlags & Client::ClientStateFlags::kConnectionAccepted))
	{
		if (const char* pcRejection = mpClient->mpcRejectionReason[0] != '\0' ? mpClient->mpcRejectionReason : nullptr; pcRejection != nullptr)
		{
			mrSession.OnConnectionRejected(pcRejection);
			Disconnect();
		}
		else if (mpClient->mStateFlags & Client::ClientStateFlags::kDisconnectedEvent)
		{
			mrSession.OnConnectionFailed();
			Disconnect();
		}
		else
		{
			SendAckAndFlush();
		}
		return;
	}

	mrSession.OnConnectionAccepted();
	mrSession.PollDesynchronizationState();
	if (mpClient == nullptr)
	{
		return;
	}
	if (mpClient->mStateFlags & Client::ClientStateFlags::kDisconnectedEvent)
	{
		mrSession.OnConnectionLost();
		if (mpClient != nullptr)
		{
			Disconnect();
		}
		return;
	}
	if (std::optional<uint8_t> uiLoadGeneration = mpClient->DrainLoadNotification(); uiLoadGeneration.has_value())
	{
		mpClient->muiCommittedLoadGeneration = *uiLoadGeneration;
		ResetForServerLoad();
		mrSession.OnServerLoad();
	}
	std::shared_ptr<ClientNetworkFixtures::StaleUpdateState> pDeliveredFixture =
		ClientNetworkFixtures::PollBeforeDrain(*mpClient, &QueryFixtureCoordinateUpdateState);
	mrSession.ProcessReceivedGamePackets();
	mrSession.ApplyReceivedStaticData();
	ApplyReceivedFullStates();
	ApplyReceivedUpdates();
	ClientNetworkFixtures::PollAfterDrain(*mpClient, pDeliveredFixture, &QueryFixtureCoordinateUpdateState);

	SendAckAndFlush();
}

void ClientSessionRuntime::ApplyReceivedFullStates()
{
	// Heap: try_emplace may insert new CoordFrames; full state is moved directly into snapshot ring slot 0
	ScopedSuppressAllocationTracking suppress;

	std::vector<ReceivedCoordFullState>& rFullStates = mpClient->mReceivedFullStates;
	if (rFullStates.empty())
	{
		return;
	}

	for (ReceivedCoordFullState& rFullState : rFullStates)
	{
		GridCoord coordinate = rFullState.coordinate;
		int64_t iTick = rFullState.iTick;

		CoordFrames& rCoordinateFrames = game::gpGame->mCoordinateFrames.try_emplace(coordinate).first->second;

		const game::Frame* pRingTail = nullptr;
		if (rCoordinateFrames.iSnapshotCount > 0)
		{
			int64_t iTailPhysical = SnapshotIndex(rCoordinateFrames.iSnapshotHead, rCoordinateFrames.iSnapshotCount - 1);
			pRingTail = rCoordinateFrames.snapshots[iTailPhysical].get();
		}
		mrSession.HydrateReceivedFullState(*rFullState.pFrame, pRingTail);

		if (rCoordinateFrames.iConfirmedTick < 0)
		{
			bool bInitialSetup = (GetConfirmedTick() < 0);

			rCoordinateFrames.iSnapshotHead = 0;
			rCoordinateFrames.snapshots[0] = std::move(rFullState.pFrame);
			rCoordinateFrames.snapshots[0]->postRender.uiSharedCrc = rCoordinateFrames.snapshots[0]->Crc();
			rCoordinateFrames.iSnapshotCount = 1;
			rCoordinateFrames.iConfirmedTick = iTick;
			rCoordinateFrames.iLastFullStateTick = iTick;
			rCoordinateFrames.iConfirmedOffset = 0;

			float fFullStateTime = rCoordinateFrames.snapshots[0]->interpolate.fCurrentTime;

			// Only the first full state sets the game clock. Start behind the server by the jitter-safety floor,
			// matching EvaluateClock, to avoid a ~150 ms ceiling stall and an initial target-behind error.
			// Clamp the offset to iTick so a freshly loaded server cannot produce a negative simulation tick.
			if (bInitialSetup && game::gpGame->miTickCounter < iTick)
			{
				// iTickWallNanoseconds is one tick's wall duration at the current time scale, so dividing the
				// wall-clock nanosecond numerator by it keeps the tick count correct as the time scale changes.
				int64_t iTickWallNanoseconds = game::gpGame->mTimeStep.SimulationToWall(engine::kTickNanoseconds).count();
				int64_t iInitialTargetBehind = (engine::kiJitterSafetyMicroseconds * 1'000 + iTickWallNanoseconds - 1) / iTickWallNanoseconds;
				int64_t iAppliedBehind = std::min<int64_t>(iInitialTargetBehind, iTick);
				int64_t iTickCounter = iTick - iAppliedBehind;
				ASSERT(iTickCounter >= 0);
				game::gpGame->miTickCounter = iTickCounter;
				game::gpGame->mfCurrentTime = fFullStateTime - static_cast<float>(iAppliedBehind) * engine::kfDeltaTime;
				game::gpGame->ResetRenderClock();
			}
		}
		else
		{
			if (iTick <= rCoordinateFrames.iConfirmedTick)
			{
				LOG(kNetwork, kVerbose, "ApplyReceivedFullStates Rejected stale full state Coord: ({},{}) FullStateTick: {} ConfirmedTick: {}", coordinate.iX, coordinate.iY, iTick, rCoordinateFrames.iConfirmedTick);
				continue;
			}

			// Coord already has confirmed state: store as pending for reconcile injection
			rCoordinateFrames.pendingFullState = CoordFrames::PendingFullState
			{
				.iTick = iTick,
				.pFrame = std::move(rFullState.pFrame),
			};
		}
	}
}

bool ClientSessionRuntime::ApplyReceivedUpdates()
{
	// Heap: map insertion for per-frame server updates
	ScopedSuppressAllocationTracking suppress;

	bool bHasNewData = false;
	const std::vector<ClientCoordSlot>& rCoordinateSlots = mpClient->mSubscriptions.mCoordinateSlots;
	std::vector<std::vector<ReceivedCoordUpdate>>& rAllUpdates = mpClient->mReceivedCoordinateUpdates;

	for (int64_t i = 0; i < std::ssize(rCoordinateSlots); ++i)
	{
		std::vector<ReceivedCoordUpdate>& rSlotUpdates = rAllUpdates.at(i);
		if (rSlotUpdates.empty())
		{
			continue;
		}

		const ClientCoordSlot& rSlot = rCoordinateSlots.at(i);
		if (rSlot.eState != CoordSubscriptionState::kActive)
		{
			rSlotUpdates.clear();
			continue;
		}

		GridCoord coordinate = rSlot.coordinate;
		CoordFrames& rCoordinateFrames = game::gpGame->mCoordinateFrames.at(coordinate);

		for (ReceivedCoordUpdate& rUpdate : rSlotUpdates)
		{
			if (rUpdate.iTick <= rCoordinateFrames.iConfirmedTick)
			{
				continue;
			}

			miLatestServerTick = std::max(miLatestServerTick, rUpdate.iTick);

			if (static_cast<int64_t>(rCoordinateFrames.serverUpdates.size()) >= engine::kiMaximumBufferedFrames)
			{
				LOG(kNetwork, kWarning, "ClientSession::ApplyReceivedUpdates Buffer full, requesting full-state resync Coord: ({},{}) Size: {} Tick: {}", coordinate.iX, coordinate.iY, rCoordinateFrames.serverUpdates.size(), rUpdate.iTick);

				// The engine already acked these ticks, so a dropped update would never be resent: abandon the
				// whole drain and take authoritative state instead. Returning here sends exactly one request even
				// when several coords are over budget, and the reset plus the discard below empties every
				// serverUpdates map, so this branch cannot arm again for at least kiMaximumBufferedFrames ticks.
				mpClient->SendResynchronizationRequest();
				mrSession.ResetCoordinateStatesForResynchronization();
				for (std::vector<ReceivedCoordUpdate>& rDrainedUpdates : rAllUpdates)
				{
					rDrainedUpdates.clear();
				}
				return false;
			}

			bool bInserted = rCoordinateFrames.serverUpdates.try_emplace(rUpdate.iTick, CoordFrames::CoordServerUpdate
			{
				.uiSharedCrc = rUpdate.uiSharedCrc,
				.statusChanges = std::move(rUpdate.statusChanges),
			}).second;
			if (bInserted)
			{
				bHasNewData = true;
			}
		}

		rSlotUpdates.clear();
	}

	return bHasNewData;
}

int64_t ClientSessionRuntime::GetConfirmedTick() const
{
	int64_t iMinimumTick = -1;
	for (const auto& [rCoordinate, rCoordinateFrames] : game::gpGame->mCoordinateFrames)
	{
		if (rCoordinateFrames.iConfirmedTick >= 0 && (iMinimumTick < 0 || rCoordinateFrames.iConfirmedTick < iMinimumTick))
		{
			iMinimumTick = rCoordinateFrames.iConfirmedTick;
		}
	}
	return iMinimumTick;
}

int64_t ClientSessionRuntime::GetClientConfirmedTick() const
{
	auto it = game::gpGame->mCoordinateFrames.find(game::gpGame->mClientGridCoordinate);
	if (it == game::gpGame->mCoordinateFrames.end())
	{
		return -1;
	}
	if (it->second.iConfirmedTick < 0)
	{
		return -1;
	}
	return it->second.iConfirmedTick;
}

int64_t ClientSessionRuntime::GetServerUpdateBufferSize() const
{
	int64_t iTotal = 0;
	for (const auto& [rCoordinate, rCoordinateFrames] : game::gpGame->mCoordinateFrames)
	{
		if (rCoordinateFrames.iConfirmedTick >= 0)
		{
			iTotal += static_cast<int64_t>(rCoordinateFrames.serverUpdates.size());
		}
	}
	return iTotal;
}

void ClientSessionRuntime::SendAckAndFlush()
{
	gpProfileManager->CpuStart(kCpuTimerNetworkSend);
	if (mpClient->SendAcknowledgement())
	{
		mpClient->Flush();
	}
	gpProfileManager->CpuStop(kCpuTimerNetworkSend, CpuStopFlags::kSmoothNow);
}

void ClientSessionRuntime::SetDesiredCoordinates(std::span<const GridCoord> desiredCoordinates, std::string_view reason, int64_t iTick)
{
	bool bChanged = std::ssize(desiredCoordinates) != std::ssize(mDesiredCoordinates);
	for (int64_t i = 0; !bChanged && i < std::ssize(desiredCoordinates); ++i)
	{
		bChanged = desiredCoordinates[i] != mDesiredCoordinates.at(i);
	}
	if (!bChanged)
	{
		return;
	}

	// Heap: sticky-coordinate map insertion and desired-coordinate vector assignment
	ScopedSuppressAllocationTracking suppress;
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	for (const GridCoord& rCoordinate : mDesiredCoordinates)
	{
		if (!ContainsCoordinate(desiredCoordinates, rCoordinate))
		{
			mUnwantedTimestamps.try_emplace(rCoordinate, now);
		}
	}
	for (int64_t i = 0; i < std::ssize(desiredCoordinates); ++i)
	{
		mUnwantedTimestamps.erase(desiredCoordinates[i]);
	}
	mDesiredCoordinates.clear();
	if (!desiredCoordinates.empty())
	{
		mDesiredCoordinates.assign(desiredCoordinates.begin(), desiredCoordinates.end());
	}
	LOG(kNetwork, kVerbose, "Desired subscriptions changed Reason: {} Count: {} Tick: {}", reason, std::ssize(desiredCoordinates), iTick);
}

void ClientSessionRuntime::SynchronizeSubscriptions()
{
	if (mpClient == nullptr)
	{
		return;
	}
	if (!(mpClient->mStateFlags & Client::ClientStateFlags::kConnectionAccepted))
	{
		return;
	}
	// Heap: subscription queue growth and ENet subscription sends
	ScopedSuppressAllocationTracking suppress;
	common::ScopedWorkbufferArena desiredArena = common::gpThreadLocal->mWorkbuffer.Push();
	for (const GridCoord& rCoordinate : mDesiredCoordinates)
	{
		desiredArena.mBuffer.PushBack(rCoordinate);
	}
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	std::erase_if(mUnwantedTimestamps, [&](const std::pair<const GridCoord, std::chrono::steady_clock::time_point>& rPair)
	{
		if (now - rPair.second >= kStickySubscriptionDuration)
		{
			return true;
		}
		desiredArena.mBuffer.PushBack(rPair.first);
		return false;
	});
	const GridCoord* pDesiredCoordinates = desiredArena.mBuffer.Data<GridCoord>();
	int64_t iDesiredCount = desiredArena.mBuffer.Count<GridCoord>();
	UnsubscribeStaleCoordinates(std::span<const GridCoord>(pDesiredCoordinates, iDesiredCount));
	mpClient->mSubscriptions.RecoverTimedOutSubscriptions();
	BuildSubscriptionQueue(std::span<const GridCoord>(pDesiredCoordinates, iDesiredCount));
	TrySubscribeNext();
}

void ClientSessionRuntime::UnsubscribeStaleCoordinates(std::span<const GridCoord> desiredCoordinates)
{
	const std::vector<ClientCoordSlot>& rSlots = mpClient->mSubscriptions.mCoordinateSlots;
	for (int64_t i = 0; i < std::ssize(rSlots); ++i)
	{
		if (!IsSlotActive(rSlots.at(i)))
		{
			continue;
		}
		if (ContainsCoordinate(desiredCoordinates, rSlots.at(i).coordinate))
		{
			continue;
		}
		GridCoord coordinate = rSlots.at(i).coordinate;
		mpClient->SendUnsubscribe(i);
		if (rSlots.at(i).eState == CoordSubscriptionState::kUnsubscribing)
		{
			game::gpGame->mCoordinateFrames.erase(coordinate);
		}
	}

	for (const SubscribeRequest& rRecord : mpClient->mSubscriptions.mSubscribeRequests.mRecords)
	{
		if (!(rRecord.flags & SubscribeRequestFlags::kCancelled) &&!ContainsCoordinate(desiredCoordinates, rRecord.coordinate))
		{
			mpClient->mSubscriptions.mSubscribeRequests.Cancel(rRecord.coordinate);
			game::gpGame->mCoordinateFrames.erase(rRecord.coordinate);
		}
	}
}

void ClientSessionRuntime::BuildSubscriptionQueue(std::span<const GridCoord> desiredCoordinates)
{
	mSubscriptionQueue.clear();
	const std::vector<ClientCoordSlot>& rSlots = mpClient->mSubscriptions.mCoordinateSlots;
	for (const GridCoord& rCoordinate : desiredCoordinates)
	{
		bool bActive = std::ranges::any_of(rSlots, [&](const ClientCoordSlot& rSlot)
		{
			return IsSlotActive(rSlot) && rSlot.coordinate == rCoordinate;
		});
		if (!bActive && !mpClient->mSubscriptions.mSubscribeRequests.IsLive(rCoordinate))
		{
			mSubscriptionQueue.push_back(rCoordinate);
		}
	}
}

void ClientSessionRuntime::TrySubscribeNext()
{
	if (!(mpClient->mStateFlags & Client::ClientStateFlags::kConnected))
	{
		return;
	}
	while (!mSubscriptionQueue.empty())
	{
		if (!mpClient->SendSubscribe(mSubscriptionQueue.front()))
		{
			if (!(mStateFlags & ClientSessionStateFlags::kNoFreeSlotLogged))
			{
				mStateFlags.Set(ClientSessionStateFlags::kNoFreeSlotLogged);
				LOG(kNetwork, kWarning, "ClientSessionRuntime no free subscription slot Pending: {}", std::ssize(mSubscriptionQueue));
			}
			return;
		}
		mSubscriptionQueue.erase(mSubscriptionQueue.begin());
	}
	mStateFlags.Set(ClientSessionStateFlags::kNoFreeSlotLogged, false);
}

void ClientSessionRuntime::ClearSubscriptionState()
{
	mDesiredCoordinates.clear();
	mUnwantedTimestamps.clear();
	mSubscriptionQueue.clear();
}

std::chrono::nanoseconds ClientSessionRuntime::EvaluateClock(int64_t iPreReconcileTick)
{
	if (miLatestServerTick < 0 || mpClient == nullptr)
	{
		miLowerTargetBehindStreakStartTick = -1;
		return 0ns;
	}
	bool bHasActiveSlot = std::ranges::any_of(mpClient->mSubscriptions.mCoordinateSlots, [](const ClientCoordSlot& rSlot)
	{
		return rSlot.eState == CoordSubscriptionState::kActive;
	});
	if (!bHasActiveSlot)
	{
		miLatestServerTick = -1;
		miLowerTargetBehindStreakStartTick = -1;
		return 0ns;
	}
	// A streak only means something across continuously observed ticks, so any discontinuity restarts it:
	// a clock snap in either direction, and the tick span skipped while an interval had no clock at all.
	bool bTickDiscontinuity = miLastEvaluateClockTick < 0 || iPreReconcileTick < miLastEvaluateClockTick || iPreReconcileTick - miLastEvaluateClockTick >= kiClockSnapThreshold;
	miLastEvaluateClockTick = iPreReconcileTick;
	if (bTickDiscontinuity)
	{
		miLowerTargetBehindStreakStartTick = -1;
	}
	int64_t iJitterMicroseconds = mpClient->mSmoothedJitterMicroseconds.mSmoothedValue;
	// iTickWallNanoseconds is one tick's wall duration at the current time scale, so dividing the
	// wall-clock nanosecond numerator by it keeps the tick count correct as the time scale changes.
	int64_t iTickWallNanoseconds = game::gpGame->mTimeStep.SimulationToWall(engine::kTickNanoseconds).count();
	int64_t iComputedTargetBehind = ((3 * iJitterMicroseconds + kiJitterSafetyMicroseconds) * 1'000 + iTickWallNanoseconds - 1) / iTickWallNanoseconds;
	if (miCurrentTargetBehind == 0)
	{
		miCurrentTargetBehind = iComputedTargetBehind;
		miLowerTargetBehindStreakStartTick = -1;
	}
	else if (iComputedTargetBehind > miCurrentTargetBehind)
	{
		// One tick per call: raising the target lowers the sim ceiling by the same amount, so a multi-tick
		// raise can drop the ceiling below the sim and stall it.
		++miCurrentTargetBehind;
		miLowerTargetBehindStreakStartTick = -1;
	}
	else if (iComputedTargetBehind < miCurrentTargetBehind)
	{
		if (miLowerTargetBehindStreakStartTick < 0)
		{
			miLowerTargetBehindStreakStartTick = iPreReconcileTick;
		}
		else if (iPreReconcileTick - miLowerTargetBehindStreakStartTick >= kiLowerTargetBehindStreakTicks)
		{
			miCurrentTargetBehind = iComputedTargetBehind;
			miLowerTargetBehindStreakStartTick = -1;
		}
	}
	else
	{
		miLowerTargetBehindStreakStartTick = -1;
	}
	miClockTargetBehind = miCurrentTargetBehind;
	miClockOffset = iPreReconcileTick - miLatestServerTick;
	miClockError = iPreReconcileTick - (miLatestServerTick - miCurrentTargetBehind);
	bool bPeriodic = iPreReconcileTick % (kiTickRate * 32) == 0 && iPreReconcileTick != miLastPeriodicClockLogTick;
	bool bError = std::abs(miClockError) >= 4 && iPreReconcileTick != miLastClockErrorLogTick;
	if (miCurrentTargetBehind != miLastLoggedClockTargetBehind || bPeriodic || bError)
	{
		LOG(kNetwork, kVerbose, "ClockSync TargetBehind: {} Error: {} Offset: {} JitterUs: {} LatestServer: {} SimTick: {}", miCurrentTargetBehind, miClockError, miClockOffset, iJitterMicroseconds, miLatestServerTick, iPreReconcileTick);
		miLastLoggedClockTargetBehind = miCurrentTargetBehind;
		if (bPeriodic)
		{
			miLastPeriodicClockLogTick = iPreReconcileTick;
		}
		if (bError)
		{
			miLastClockErrorLogTick = iPreReconcileTick;
		}
	}
	if (std::abs(miClockError) < 4)
	{
		miLastClockErrorLogTick = -1;
	}
	int64_t iSteps = std::clamp(miClockError, -4LL, 4LL);
	int64_t iDivisor = std::abs(miClockError) >= 4 ? 8 : 64;
	return std::chrono::nanoseconds(-iSteps * game::NetworkSessionContract::kTickDuration.count() / iDivisor);
}

void ClientSessionRuntime::ApplyClockCorrection(int64_t iPreReconcileTick)
{
	std::chrono::nanoseconds clockCorrectionNanoseconds = EvaluateClock(iPreReconcileTick);
	gpProfileManager->SetClockCorrection(miClockOffset, miClockTargetBehind, miClockError);

	if (miLatestServerTick >= 0 && std::abs(miClockError) >= kiClockSnapThreshold)
	{
		// Snap tick counter to recover from extreme clock error. Sim runs BEHIND latestServerTick.
		// Clamp at 0 so a fresh post-load server (latestServerTick < currentTargetBehind)
		// doesn't drive the client tick negative.
		int64_t iSnapTick = std::max<int64_t>(0, miLatestServerTick - miCurrentTargetBehind);
		LOG(kNetwork, kWarning, "ClientSessionRuntime::ApplyClockCorrection Clock snap OldTick: {} NewTick: {} LatestServerTick: {} TargetBehind: {}", iPreReconcileTick, iSnapTick, miLatestServerTick, miCurrentTargetBehind);
		int64_t iTickCounter = iSnapTick;
		ASSERT(iTickCounter >= 0);
		game::gpGame->miTickCounter = iTickCounter;
		game::gpGame->mTimeStep.mTickRemainderNanoseconds = 0ns;
		game::gpGame->ResetRenderClock();
		miClockError = 0;
	}
	else
	{
		game::gpGame->mTimeStep.mTickRemainderNanoseconds = std::max(0ns, game::gpGame->mTimeStep.mTickRemainderNanoseconds + clockCorrectionNanoseconds);
	}
}

} // namespace engine

#endif // BT_CLIENT
