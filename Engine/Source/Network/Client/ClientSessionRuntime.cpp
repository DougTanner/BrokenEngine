#include "Pch.h"

#include "Network/Client/ClientSessionRuntime.h"

#if defined(BT_CLIENT)

#include "Network/Client/Client.h"
#include "Network/NetworkDiscoveryScanner.h"

#include "Game.h"
#include "Network/Client/ClientSession.h"
#include "Profile/ProfileManager.h"

namespace engine
{

namespace
{

// The on-disk header version belongs to this file wrapper rather than Guid128. Version 2 uses the shared version+size
// convention, and version 1 files reset once.
struct ClientGuidFile
{
	static constexpr int64_t kiVersion = 2;

	ClientGuid guid {};
};

static_assert(sizeof(ClientGuidFile) == sizeof(ClientGuid), "ClientGuidFile must stay a pure 16-byte body — existing ClientGuid.bin files carry no extra field");
static_assert(alignof(ClientGuidFile) == alignof(ClientGuid), "ClientGuidFile alignment diverged from ClientGuid");
static_assert(BT_OFFSETOF(ClientGuidFile, guid) == 0, "ClientGuidFile::guid must start at offset 0 — the file body is the bare GUID");

ClientGuid LoadClientGuidFromDisk()
{
	// Heap: filesystem path and file stream operations for GUID persistence
	ScopedSuppressAllocationTracking suppress;
	ClientGuidFile loadedFile {};
	if (ReadVersionedFile({FileFlags::kAppDataDirectory, FileFlags::kRead}, std::filesystem::path("ClientGuid.bin"), loadedFile) && !loadedFile.guid.IsEmpty())
	{
		LOG(kNetwork, kInfo, "ClientSessionRuntime loaded GUID from disk: {} {}", loadedFile.guid.uiHigh, loadedFile.guid.uiLow);
		return loadedFile.guid;
	}
	return {};
}

void PersistClientGuidToDisk(const ClientGuid& rGuid)
{
	// Heap: filesystem path and file stream operations for GUID persistence
	ScopedSuppressAllocationTracking suppress;
	const ClientGuidFile guidFile {rGuid};
	if (!WriteVersionedFile({FileFlags::kAppDataDirectory, FileFlags::kWrite}, std::filesystem::path("ClientGuid.bin"), guidFile))
	{
		LOG(kNetwork, kError, "Failed to persist ClientGuid.bin (next session will re-handshake as a new client)");
	}
}

// Lowering miCurrentTargetBehind waits out one mSmoothedJitterUs window's worth of sim ticks, so in
// loss-free arrival the average has mostly turned over before the lower target is adopted. Measured in
// sim ticks, not EvaluateClock calls: jitter samples arrive one per coord update, so they land at tick
// cadence while EvaluateClock runs at render cadence. Packet loss yields fewer than one sample per tick,
// so the window is an approximation, not a guarantee that every higher-jitter sample has aged out.
constexpr int64_t kiLowerTargetBehindStreakTicks = decltype(Client::mSmoothedJitterUs)::kiCapacity;

bool IsSlotActive(const ClientCoordSlot& rSlot)
{
	return rSlot.eState != CoordSubscriptionState::kUnsubscribed && rSlot.eState != CoordSubscriptionState::kUnsubscribing;
}

bool ContainsCoordinate(const GridCoord* pCoordinates, int64_t iCount, GridCoord coordinate)
{
	for (int64_t i = 0; i < iCount; ++i)
	{
		if (pCoordinates[i] == coordinate)
		{
			return true;
		}
	}
	return false;
}

} // namespace

ClientSessionRuntime::ClientSessionRuntime(game::ClientSession& rSession)
:	mrSession(rSession)
{
}

ClientSessionRuntime::~ClientSessionRuntime() = default;

int64_t ClientSessionRuntime::CurrentGameTick() const
{
	return game::gpGame->TickCounter();
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
	mStateFlags.Clear(ClientSessionStateFlags::kServerDiscovered);
	mStateFlags.Clear(ClientSessionStateFlags::kDiscoveryScanTimedOut);
	mStateFlags.Clear(ClientSessionStateFlags::kNoFreeSlotLogged);
}

void ClientSessionRuntime::Connect(std::string_view serverAddress, uint16_t uiPort, int64_t iCoordSlots)
{
	// Heap: address copy, GUID file I/O, and Client/ENet construction
	ScopedSuppressAllocationTracking suppress;
	ResetForConnect();
	std::string serverAddressString(serverAddress);
	ClientGuid clientGuid = LoadClientGuidFromDisk();
	mpClient = std::make_unique<Client>(serverAddressString.c_str(), uiPort, iCoordSlots, clientGuid, &PersistClientGuidToDisk);
}

void ClientSessionRuntime::ConnectToDiscoveredServer(uint16_t uiPort, int64_t iCoordSlots)
{
	mStateFlags.Clear(ClientSessionStateFlags::kServerDiscovered);
	Connect(mcDiscoveredAddress, uiPort, iCoordSlots);
}

void ClientSessionRuntime::Disconnect()
{
	// Heap: Client/ENet and discovery teardown may allocate during cleanup
	ScopedSuppressAllocationTracking suppress;
	mpClient.reset();
	mpDiscoveryScanner.reset();
	ResetClock();
	ClearSubscriptionState();
	mStateFlags.Clear(ClientSessionStateFlags::kServerDiscovered);
	mStateFlags.Clear(ClientSessionStateFlags::kDiscoveryScanTimedOut);
	mStateFlags.Clear(ClientSessionStateFlags::kNoFreeSlotLogged);
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
	if (mpDiscoveryScanner->IsFound())
	{
		std::snprintf(mcDiscoveredAddress, sizeof(mcDiscoveredAddress), "%s", mpDiscoveryScanner->GetFoundAddress());
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
	if (std::shared_ptr<ClientStaleUpdateFixtureState> pState = mpClient->mStaleUpdateFixture.lock(); pState != nullptr)
	{
		pState->packet.clear();
		pState->flags.Set(ClientStaleUpdateFixtureFlags::kReset);
		mpClient->mStaleUpdateFixture.reset();
	}
	ResetClock();
	mpClient->mSmoothedJitterUs.Reset();
	mpClient->mStateFlags.Clear(Client::ClientStateFlags::kHasLastUpdateArrival);
	mpClient->mStateFlags.Set(Client::ClientStateFlags::kSkipNextJitterInterval);
	ClearSubscriptionState();
	mpClient->ResetAllSlots();
	mpClient->mReceivedFullStates.clear();
	mpClient->mReceivedStaticData.clear();
	for (std::vector<ReceivedCoordUpdate>& rSlotUpdates : mpClient->mReceivedCoordUpdates)
	{
		rSlotUpdates.clear();
	}

	// Coord-channel packets still delayed by the network simulation belong to pre-load subscriptions;
	// the control channel is left alone because it carries the load notification and handshake traffic
	if constexpr (keNetworkSimulation != engine::NetworkSimulationLevel::kDisabled)
	{
		for (int64_t i = 0; i < std::ssize(mpClient->mCoordSlots); ++i)
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
	mrSession.PollDesyncState();
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

	if (mpClient->DrainLoadNotification())
	{
		ResetForServerLoad();
		mrSession.OnServerLoad();
	}
	std::shared_ptr<ClientStaleUpdateFixtureState> pDeliveredFixture;
	if (std::shared_ptr<ClientStaleUpdateFixtureState> pState = mpClient->mStaleUpdateFixture.lock(); pState != nullptr)
	{
		if (++pState->iCapturePolls > kiNetworkBufferSize)
		{
			pState->packet.clear();
			pState->flags.Set(ClientStaleUpdateFixtureFlags::kBoundExpired);
			mpClient->mStaleUpdateFixture.reset();
		}
		else if ((pState->flags & ClientStaleUpdateFixtureFlags::kCaptured)
			&& pState->iCapturePolls > pState->iCapturedAtPoll + 1
			&& pState->uiSlotIndex < mpClient->mCoordSlots.size())
		{
			ClientCoordSlot& rSlot = mpClient->mCoordSlots.at(pState->uiSlotIndex);
			auto coordIt = game::gpGame->mCoordFrames.find(pState->coord);
			if (rSlot.eState == CoordSubscriptionState::kActive && rSlot.coord == pState->coord
				&& rSlot.ackState.uiEpoch == pState->uiEpoch && rSlot.ackState.iAckFloor >= pState->iTick
				&& coordIt != game::gpGame->mCoordFrames.end() && coordIt->second.iConfirmedTick >= pState->iTick)
			{
				pState->iAckFloorBefore = rSlot.ackState.iAckFloor;
				pState->iConfirmedBefore = coordIt->second.iConfirmedTick;
				std::vector<uint8_t> packet = std::move(pState->packet);
				pState->packet.clear();
				mpClient->mStaleUpdateFixture.reset();
				mpClient->Receive(packet);
				pState->iAckFloorAfter = rSlot.ackState.iAckFloor;
				pDeliveredFixture = std::move(pState);
			}
		}
	}
	mrSession.ProcessReceivedGamePackets();
	mrSession.ApplyReceivedStaticData();
	ApplyReceivedFullStates();
	ApplyReceivedUpdates();
	if (pDeliveredFixture != nullptr)
	{
		auto coordIt = game::gpGame->mCoordFrames.find(pDeliveredFixture->coord);
		pDeliveredFixture->iConfirmedAfter = coordIt != game::gpGame->mCoordFrames.end() ? coordIt->second.iConfirmedTick : -1;
		if (coordIt != game::gpGame->mCoordFrames.end() && coordIt->second.serverUpdates.contains(pDeliveredFixture->iTick))
		{
			pDeliveredFixture->flags.Set(ClientStaleUpdateFixtureFlags::kRetainedAfterDrain);
		}
		if (mpClient->mStateFlags & Client::ClientStateFlags::kConnected)
		{
			pDeliveredFixture->flags.Set(ClientStaleUpdateFixtureFlags::kConnectedAfterDrain);
		}
		pDeliveredFixture->flags.Set(ClientStaleUpdateFixtureFlags::kComplete);
	}

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
		GridCoord coord = rFullState.coord;
		int64_t iTick = rFullState.iTick;

		CoordFrames& rCoordFrames = game::gpGame->mCoordFrames.try_emplace(coord).first->second;

		const game::Frame* pRingTail = nullptr;
		if (rCoordFrames.iSnapshotCount > 0)
		{
			int64_t iTailPhysical = SnapshotIndex(rCoordFrames.iSnapshotHead, rCoordFrames.iSnapshotCount - 1);
			pRingTail = rCoordFrames.snapshots[iTailPhysical].get();
		}
		mrSession.HydrateReceivedFullState(*rFullState.pFrame, pRingTail);

		if (rCoordFrames.iConfirmedTick < 0)
		{
			// Only advance tick counter during initial setup (no other coords have confirmed data yet)
			bool bInitialSetup = (GetConfirmedTick() < 0);

			// Move the received full state into the ring as the confirmed frame.
			rCoordFrames.iSnapshotHead = 0;
			rCoordFrames.snapshots[0] = std::move(rFullState.pFrame);
			rCoordFrames.snapshots[0]->postRender.sharedCrc = rCoordFrames.snapshots[0]->Crcs();
			rCoordFrames.iSnapshotCount = 1;
			rCoordFrames.iConfirmedTick = iTick;
			rCoordFrames.iLastFullStateTick = iTick;
			rCoordFrames.iConfirmedOffset = 0;

			float fFullStateTime = rCoordFrames.snapshots[0]->interpolate.fCurrentTime;

			// Set frame counter from first received full state only (not from subsequent neighbor subscriptions).
			// Offset sim tick back by the jitter-safety floor so sim starts BEHIND latestServerTick, matching
			// the steady-state target computed by ComputeClockCorrectionNs. Avoids a ~150 ms freeze while the
			// ceiling clamp waits for latest to catch up and drains the spurious +targetBehind error.
			// Clamp the offset at iTick so a fresh post-load server (iTick < jitter-safety floor) does
			// not produce a negative sim tick.
			if (bInitialSetup && game::gpGame->TickCounter() < iTick)
			{
				static constexpr int64_t kiTickTimeMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(engine::kTickNs).count();
				static constexpr int64_t kiInitialTargetBehind = (engine::kiJitterSafetyUs + kiTickTimeMicroseconds - 1) / kiTickTimeMicroseconds;
				const int64_t iAppliedBehind = std::min<int64_t>(kiInitialTargetBehind, iTick);
				game::gpGame->SetTickCounter(iTick - iAppliedBehind);
				game::gpGame->SetCurrentTime(fFullStateTime - static_cast<float>(iAppliedBehind) * engine::kfDeltaTime);
				game::gpGame->ResetRenderClock();
			}
		}
		else
		{
			// Reject stale full states: tick must be after confirmed tick
			if (iTick <= rCoordFrames.iConfirmedTick)
			{
				LOG(kNetwork, kVerbose, "ApplyReceivedFullStates Rejected stale full state Coord: ({},{}) FullStateTick: {} ConfirmedTick: {}", coord.x, coord.y, iTick, rCoordFrames.iConfirmedTick);
				continue;
			}

			// Coord already has confirmed state: store as pending for reconcile injection
			rCoordFrames.pendingFullState = CoordFrames::PendingFullState {
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
	const std::vector<ClientCoordSlot>& rCoordSlots = mpClient->mCoordSlots;
	std::vector<std::vector<ReceivedCoordUpdate>>& rAllUpdates = mpClient->mReceivedCoordUpdates;

	for (int64_t iSlot = 0; iSlot < std::ssize(rCoordSlots); ++iSlot)
	{
		std::vector<ReceivedCoordUpdate>& rSlotUpdates = rAllUpdates.at(iSlot);
		if (rSlotUpdates.empty())
		{
			continue;
		}

		const ClientCoordSlot& rSlot = rCoordSlots.at(iSlot);
		if (rSlot.eState != CoordSubscriptionState::kActive)
		{
			rSlotUpdates.clear();
			continue;
		}

		GridCoord coord = rSlot.coord;
		CoordFrames& rCoordFrames = game::gpGame->mCoordFrames.at(coord);

		for (ReceivedCoordUpdate& rUpdate : rSlotUpdates)
		{
			if (rUpdate.iTick <= rCoordFrames.iConfirmedTick)
			{
				continue;
			}

			miLatestServerTick = std::max(miLatestServerTick, rUpdate.iTick);

			if (static_cast<int64_t>(rCoordFrames.serverUpdates.size()) >= engine::kiMaxBufferedFrames)
			{
				LOG(kNetwork, kWarning, "ClientSession::ApplyReceivedUpdates Buffer full, requesting full-state resync Coord: ({},{}) Size: {} Tick: {}", coord.x, coord.y, rCoordFrames.serverUpdates.size(), rUpdate.iTick);

				// The engine already acked these ticks, so a dropped update would never be resent: abandon the
				// whole drain and take authoritative state instead. Returning here sends exactly one request even
				// when several coords are over budget, and the reset plus the discard below empties every
				// serverUpdates map, so this branch cannot arm again for at least kiMaxBufferedFrames ticks.
				mpClient->SendResyncRequest();
				mrSession.ResetCoordStatesForResync();
				for (std::vector<ReceivedCoordUpdate>& rDrainedUpdates : rAllUpdates)
				{
					rDrainedUpdates.clear();
				}
				return false;
			}

			bool bInserted = rCoordFrames.serverUpdates.try_emplace(rUpdate.iTick, CoordFrames::CoordServerUpdate {
				.sharedCrc = rUpdate.sharedCrc,
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
	for (const auto& [rCoord, rCoordFrames] : game::gpGame->mCoordFrames)
	{
		if (rCoordFrames.iConfirmedTick >= 0 && (iMinimumTick < 0 || rCoordFrames.iConfirmedTick < iMinimumTick))
		{
			iMinimumTick = rCoordFrames.iConfirmedTick;
		}
	}
	return iMinimumTick;
}

int64_t ClientSessionRuntime::GetClientConfirmedTick() const
{
	auto it = game::gpGame->mCoordFrames.find(game::gpGame->mClientGridCoord);
	if (it == game::gpGame->mCoordFrames.end() || it->second.iConfirmedTick < 0)
	{
		return -1;
	}
	return it->second.iConfirmedTick;
}

int64_t ClientSessionRuntime::GetServerUpdateBufferSize() const
{
	int64_t iTotal = 0;
	for (const auto& [rCoord, rCoordFrames] : game::gpGame->mCoordFrames)
	{
		if (rCoordFrames.iConfirmedTick >= 0)
		{
			iTotal += static_cast<int64_t>(rCoordFrames.serverUpdates.size());
		}
	}
	return iTotal;
}

void ClientSessionRuntime::FlushOutgoing()
{
	mpClient->Flush();
}

void ClientSessionRuntime::SendAckAndFlush()
{
	gpProfileManager->CpuStart(kCpuTimerNetworkSend);
	if (mpClient->SendAck())
	{
		mpClient->Flush();
	}
	gpProfileManager->CpuStop(kCpuTimerNetworkSend, CpuStopFlags::kSmoothNow);
}

void ClientSessionRuntime::SetDesiredCoords(const GridCoord* pDesiredCoords, int64_t iDesiredCount, std::string_view reason, int64_t iTick)
{
	bool bChanged = iDesiredCount != std::ssize(mDesiredCoords);
	for (int64_t i = 0; !bChanged && i < iDesiredCount; ++i)
	{
		bChanged = pDesiredCoords[i] != mDesiredCoords.at(i);
	}
	if (!bChanged)
	{
		return;
	}

	// Heap: sticky-coordinate map insertion and desired-coordinate vector assignment
	ScopedSuppressAllocationTracking suppress;
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	for (const GridCoord& rCoord : mDesiredCoords)
	{
		if (!ContainsCoordinate(pDesiredCoords, iDesiredCount, rCoord))
		{
			mUnwantedTimestamps.try_emplace(rCoord, now);
		}
	}
	for (int64_t i = 0; i < iDesiredCount; ++i)
	{
		mUnwantedTimestamps.erase(pDesiredCoords[i]);
	}
	mDesiredCoords.clear();
	if (iDesiredCount > 0)
	{
		mDesiredCoords.assign(pDesiredCoords, pDesiredCoords + iDesiredCount);
	}
	LOG(kNetwork, kVerbose, "Desired subscriptions changed Reason: {} Count: {} Tick: {}", reason, iDesiredCount, iTick);
}

void ClientSessionRuntime::SynchronizeSubscriptions()
{
	if (mpClient == nullptr)
	{
		return;
	}
	// Heap: subscription queue growth and ENet subscription sends
	ScopedSuppressAllocationTracking suppress;
	common::ScopedWorkbufferArena desiredArena = common::gpThreadLocal->mWorkbuffer.Push();
	for (const GridCoord& rCoord : mDesiredCoords)
	{
		desiredArena.PushBack(rCoord);
	}
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	std::erase_if(mUnwantedTimestamps, [&](const auto& rPair)
	{
		if (now - rPair.second >= kStickySubscriptionDuration)
		{
			return true;
		}
		desiredArena.PushBack(rPair.first);
		return false;
	});
	const GridCoord* pDesiredCoords = desiredArena.Data<GridCoord>();
	int64_t iDesiredCount = desiredArena.Count<GridCoord>();
	UnsubscribeStaleCoords(pDesiredCoords, iDesiredCount);
	mpClient->RecoverTimedOutSubscriptions();
	BuildSubscriptionQueue(pDesiredCoords, iDesiredCount);
	TrySubscribeNext();
}

void ClientSessionRuntime::UnsubscribeStaleCoords(const GridCoord* pDesiredCoords, int64_t iDesiredCount)
{
	const std::vector<ClientCoordSlot>& rSlots = mpClient->mCoordSlots;
	for (int64_t i = 0; i < std::ssize(rSlots); ++i)
	{
		if (!IsSlotActive(rSlots.at(i)) || ContainsCoordinate(pDesiredCoords, iDesiredCount, rSlots.at(i).coord))
		{
			continue;
		}
		GridCoord coord = rSlots.at(i).coord;
		if (rSlots.at(i).eState == CoordSubscriptionState::kSubscribing)
		{
			mpClient->CancelSubscription(i);
			mrSession.OnCoordReleased(coord);
		}
		else
		{
			mpClient->SendUnsubscribe(i);
			if (rSlots.at(i).eState == CoordSubscriptionState::kUnsubscribing)
			{
				mrSession.OnCoordReleased(coord);
			}
		}
	}
}

void ClientSessionRuntime::BuildSubscriptionQueue(const GridCoord* pDesiredCoords, int64_t iDesiredCount)
{
	mSubscriptionQueue.clear();
	const std::vector<ClientCoordSlot>& rSlots = mpClient->mCoordSlots;
	for (int64_t i = 0; i < iDesiredCount; ++i)
	{
		const GridCoord& rCoord = pDesiredCoords[i];
		bool bActive = std::ranges::any_of(rSlots, [&](const ClientCoordSlot& rSlot)
		{
			return IsSlotActive(rSlot) && rSlot.coord == rCoord;
		});
		if (!bActive)
		{
			mSubscriptionQueue.push_back(rCoord);
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
				LOG(kNetwork, kWarning, "ClientSessionRuntime no free subscription slot Pending: {}", mSubscriptionQueue.size());
			}
			return;
		}
		mSubscriptionQueue.erase(mSubscriptionQueue.begin());
	}
	mStateFlags.Clear(ClientSessionStateFlags::kNoFreeSlotLogged);
}

void ClientSessionRuntime::ClearSubscriptionState()
{
	mDesiredCoords.clear();
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
	bool bHasActiveSlot = std::ranges::any_of(mpClient->mCoordSlots, [](const ClientCoordSlot& rSlot)
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
	int64_t iJitterMicroseconds = mpClient->mSmoothedJitterUs.Get();
	int64_t iTickMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(game::NetworkSessionContract::kTickDuration).count();
	int64_t iComputedTargetBehind = (3 * iJitterMicroseconds + kiJitterSafetyUs + iTickMicroseconds - 1) / iTickMicroseconds;
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
	std::chrono::nanoseconds clockCorrectionNs = EvaluateClock(iPreReconcileTick);
	gpProfileManager->SetClockCorrection(miClockOffset, miClockTargetBehind, miClockError);

	if (miLatestServerTick >= 0 && std::abs(miClockError) >= kiClockSnapThreshold)
	{
		// Snap tick counter to recover from extreme clock error. Sim runs BEHIND latestServerTick.
		// Clamp at 0 so a fresh post-load server (latestServerTick < currentTargetBehind)
		// doesn't drive the client tick negative.
		int64_t iSnapTick = std::max<int64_t>(0, miLatestServerTick - miCurrentTargetBehind);
		LOG(kNetwork, kWarning, "ClientSessionRuntime::ApplyClockCorrection Clock snap OldTick: {} NewTick: {} LatestServerTick: {} TargetBehind: {}", iPreReconcileTick, iSnapTick, miLatestServerTick, miCurrentTargetBehind);
		game::gpGame->SetTickCounter(iSnapTick);
		game::gpGame->mTimeStep.ClearAccumulator();
		game::gpGame->ResetRenderClock();
		miClockError = 0;
	}
	else
	{
		game::gpGame->mTimeStep.mTickRemainderNs = std::max(0ns, game::gpGame->mTimeStep.mTickRemainderNs + clockCorrectionNs);
	}
}

} // namespace engine

#endif // BT_CLIENT
