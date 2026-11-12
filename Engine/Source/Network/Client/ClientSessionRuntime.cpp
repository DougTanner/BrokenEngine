#include "Pch.h"

#include "Network/Client/ClientSessionRuntime.h"

#if defined(BT_CLIENT)

#include "Network/Client/Client.h"
#include "Network/NetworkDiscoveryScanner.h"
#include "CrashReport.h"

#include "Network/Client/ClientSession.h"
#include "Profile/ProfileRows.h"
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
	return std::ranges::any_of(coordinates, [coordinate](const GridCoord& rCoordinate)
	{
		return rCoordinate == coordinate;
	});
}

ClientSessionRuntime::ClientSessionRuntime() = default;

ClientSessionRuntime::~ClientSessionRuntime()
{
	if (mTerrainPreparation.valid())
	{
		mTerrainPreparation.wait();
		std::ignore = mTerrainPreparation.get();
	}
}

static bool EqualPlacements(const CellStaticData& rLeft, const CellStaticData& rRight)
{
	return std::ranges::equal(rLeft.islands, rRight.islands, [](const IslandPlacement& rA, const IslandPlacement& rB)
	{
		return rA.islandCrc == rB.islandCrc && std::bit_cast<uint32_t>(rA.f2WorldPosition.x) == std::bit_cast<uint32_t>(rB.f2WorldPosition.x) && std::bit_cast<uint32_t>(rA.f2WorldPosition.y) == std::bit_cast<uint32_t>(rB.f2WorldPosition.y) && std::bit_cast<uint32_t>(rA.fRotation) == std::bit_cast<uint32_t>(rB.fRotation);
	});
}

bool ClientSessionRuntime::IsCurrentIdentity(const ReceivedCellIdentity& rIdentity) const
{
	if (mpClient == nullptr)
	{
		return false;
	}
	if (rIdentity.iLoadGeneration != mpClient->miCommittedLoadGeneration)
	{
		return false;
	}
	const ClientCoordSlot& rSlot = mpClient->mSubscriptions.mCoordinateSlots.at(rIdentity.iSlot);
	if (IsSlotActive(rSlot))
	{
		return rSlot.coordinate == rIdentity.coordinate && rSlot.acknowledgementState.uiEpoch == rIdentity.iEpoch;
	}
	return mpClient->mSubscriptions.mSubscribeRequests.IsLive(rIdentity.coordinate) && !mpClient->mSubscriptions.IsStaleRetainedEpoch(rIdentity.iSlot, rIdentity.iEpoch, rIdentity.coordinate);
}

bool ClientSessionRuntime::HoldsSlotOrRequest(GridCoord coordinate) const
{
	return mpClient != nullptr && (mpClient->mSubscriptions.mSubscribeRequests.IsLive(coordinate) || std::ranges::any_of(mpClient->mSubscriptions.mCoordinateSlots, [coordinate](const ClientCoordSlot& rSlot)
	{
		return IsSlotActive(rSlot) && rSlot.coordinate == coordinate;
	}));
}

ClientSessionRuntime::PendingCell& ClientSessionRuntime::PendingForIdentity(const ReceivedCellIdentity& rIdentity)
{
	PendingCell& rPending = mPendingCells.try_emplace(rIdentity.coordinate).first->second;
	if (rPending.identity.iSlot >= 0 && rPending.identity != rIdentity)
	{
		rPending = {};
	}
	if (rPending.uiPreparation == 0)
	{
		rPending.uiPreparation = ++muiNextPreparation;
	}
	rPending.identity = rIdentity;
	return rPending;
}

void ClientSessionRuntime::StageLocalCell(GridCoord coordinate)
{
	// Heap: pending terrain, frame ownership and procedural placements persist until publication.
	ScopedSuppressAllocationTracking suppress;
	PendingCell& rPending = mPendingCells.try_emplace(coordinate).first->second;
	if (rPending.uiPreparation != 0)
	{
		return;
	}
	rPending.uiPreparation = ++muiNextPreparation;
	rPending.identity.coordinate = coordinate;
	Cell localCell;
	game::gpGame->CreateCellFrame(localCell, coordinate).interpolate.iTick = game::gpGame->miTickCounter;
	rPending.pLocalFrame = std::move(localCell.ppSnapshots[0]);
	rPending.optionalStaticData.emplace();
	rPending.optionalStaticData->coordinate = coordinate;
	GenerateIslandChain(coordinate, rPending.optionalStaticData->islands);
}

void ClientSessionRuntime::ResolveAuthoritativeTerrain(PendingCell& rPending)
{
	if (!rPending.optionalAuthoritativeStaticData.has_value())
	{
		return;
	}
	if (rPending.bTerrainReady && EqualPlacements(*rPending.optionalStaticData, *rPending.optionalAuthoritativeStaticData))
	{
		rPending.optionalStaticData->navigationData = std::move(rPending.optionalAuthoritativeStaticData->navigationData);
		rPending.optionalStaticData->flags.Set(CellStaticDataFlags::kNavigationDataBuilt, rPending.optionalAuthoritativeStaticData->flags & CellStaticDataFlags::kNavigationDataBuilt);
	}
	else
	{
		rPending.optionalStaticData = std::move(rPending.optionalAuthoritativeStaticData);
		rPending.bTerrainReady = false;
		rPending.uiPreparation = ++muiNextPreparation;
	}
	rPending.optionalAuthoritativeStaticData.reset();
}

void ClientSessionRuntime::StageStaticData(ReceivedStaticData received)
{
	// Heap: retained authoritative records outlive Client's transient poll buffers.
	ScopedSuppressAllocationTracking suppress;
	if (!IsCurrentIdentity(received.identity))
	{
		return;
	}
	PendingCell& rPending = PendingForIdentity(received.identity);
	received.staticData.coordinate = received.identity.coordinate;
	for (const IslandPlacement& rPlacement : received.staticData.islands)
	{
		gpIslandTerrainResidency->AcquireTextureSlot(rPlacement.islandCrc);
	}
	auto it = game::gpGame->mCells.find(received.identity.coordinate);
	if (it != game::gpGame->mCells.end() && (it->second.staticData.flags & CellStaticDataFlags::kTerrainPrepared) && EqualPlacements(it->second.staticData, received.staticData))
	{
		it->second.staticData.navigationData = std::move(received.staticData.navigationData);
		it->second.staticData.flags.Set(CellStaticDataFlags::kNavigationDataBuilt, received.staticData.flags & CellStaticDataFlags::kNavigationDataBuilt);
		rPending.bTerrainReady = true;
		return;
	}
	rPending.optionalAuthoritativeStaticData = std::move(received.staticData);
	if (mTerrainPreparation.valid() && mPreparingCoordinate == received.identity.coordinate && muiPreparingIdentity == rPending.uiPreparation)
	{
		return;
	}
	if (!rPending.optionalStaticData.has_value())
	{
		rPending.optionalStaticData = std::move(rPending.optionalAuthoritativeStaticData);
		rPending.optionalAuthoritativeStaticData.reset();
		rPending.bTerrainReady = false;
		rPending.uiPreparation = ++muiNextPreparation;
	}
	else
	{
		ResolveAuthoritativeTerrain(rPending);
	}
}

void ClientSessionRuntime::PollTerrainPreparation()
{
	if (!mTerrainPreparation.valid() || mTerrainPreparation.wait_for(0s) != std::future_status::ready)
	{
		return;
	}
	CellStaticData prepared = mTerrainPreparation.get();
	auto it = mPendingCells.find(mPreparingCoordinate);
	if (it == mPendingCells.end())
	{
		return;
	}
	if (it->second.uiPreparation != muiPreparingIdentity)
	{
		return;
	}
	PendingCell& rPending = it->second;
	if (rPending.identity.iSlot < 0 && mPreparingCoordinate != game::gpGame->mClientGridCoordinate && !ContainsCoordinate(std::span<const GridCoord>(mpVisibleNeighbors, static_cast<size_t>(miVisibleNeighborCount)), mPreparingCoordinate) && !HoldsSlotOrRequest(mPreparingCoordinate))
	{
		mPendingCells.erase(it);
		return;
	}
	if (rPending.identity.iSlot >= 0 && !IsCurrentIdentity(rPending.identity))
	{
		mPendingCells.erase(it);
		return;
	}
	rPending.optionalStaticData = std::move(prepared);
	rPending.bTerrainReady = true;
	ResolveAuthoritativeTerrain(rPending);
}

void ClientSessionRuntime::StartTerrainPreparation()
{
	if (mTerrainPreparation.valid())
	{
		return;
	}
	auto it = mPendingCells.find(game::gpGame->mClientGridCoordinate);
	auto NeedsPreparation = [](const PendingCell& rPending)
	{
		return rPending.optionalStaticData.has_value() && !rPending.bTerrainReady;
	};
	if (it == mPendingCells.end() || !NeedsPreparation(it->second))
	{
		it = std::ranges::find_if(mPendingCells, [&](const auto& rPair)
		{
			return NeedsPreparation(rPair.second);
		});
	}
	if (it == mPendingCells.end())
	{
		return;
	}
	PendingCell& rPending = it->second;
	for (const IslandPlacement& rPlacement : rPending.optionalStaticData->islands)
	{
		gpIslandTerrainResidency->AcquireTextureSlot(rPlacement.islandCrc);
	}
	mPreparingCoordinate = it->first;
	muiPreparingIdentity = rPending.uiPreparation;
	// Heap: async state and the complete owned terrain record outlive this main-thread call.
	ScopedSuppressAllocationTracking suppress;
	mTerrainPreparation = std::async(std::launch::async, common::ThreadLocal::Entry([staticData = std::move(*rPending.optionalStaticData)]() mutable
	{
		if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL))
		{
			throw std::runtime_error("Terrain preparation thread priority failed");
		}
		// Heap: derived terrain vectors belong exclusively to the returned record.
		ScopedSuppressAllocationTracking suppressBuild;
		if (!staticData.islands.empty())
		{
			gpIslandTerrain->BuildElevationGrid(staticData.islands, staticData.elevationGrid);
		}
		staticData.BuildRenderPlacementCache(*gpIslandTerrain);
		staticData.flags.Set(CellStaticDataFlags::kTerrainPrepared);
		return std::move(staticData);
	}, common::kiMinWorkbufferSize));
	rPending.optionalStaticData.reset();
}

void ClientSessionRuntime::PublishPreparedCells()
{
	// Heap: publication moves persistent records, snapshot frames and retained updates into cells.
	ScopedSuppressAllocationTracking suppress;
	for (auto& [rCoordinate, rPending] : mPendingCells)
	{
		if (!rPending.bTerrainReady)
		{
			continue;
		}
		if (rPending.optionalAuthoritativeStaticData.has_value())
		{
			continue;
		}
		bool bAuthoritative = rPending.identity.iSlot >= 0;
		if (bAuthoritative)
		{
			if (!IsCurrentIdentity(rPending.identity))
			{
				continue;
			}
			if (!rPending.optionalFullState.has_value())
			{
				continue;
			}
		}
		if (!bAuthoritative && rPending.pLocalFrame == nullptr)
		{
			continue;
		}
		auto it = game::gpGame->mCells.find(rCoordinate);
		if (bAuthoritative && !(rPending.optionalStaticData.has_value() ? (rPending.optionalStaticData->flags & CellStaticDataFlags::kNavigationDataBuilt) : it != game::gpGame->mCells.end() && (it->second.staticData.flags & CellStaticDataFlags::kNavigationDataBuilt)))
		{
			continue;
		}
		Cell& rCell = game::gpGame->mCells.try_emplace(rCoordinate).first->second;
		if (rPending.optionalStaticData.has_value())
		{
			rCell.staticData = std::move(*rPending.optionalStaticData);
			rPending.optionalStaticData.reset();
		}
		if (bAuthoritative)
		{
			AdoptFullState(*rPending.optionalFullState);
			rPending.optionalFullState.reset();
			rPending.pLocalFrame.reset();
		}
		else
		{
			rCell.iSnapshotHead = 0;
			rCell.iSnapshotCount = 1;
			rCell.ppSnapshots[0] = std::move(rPending.pLocalFrame);
		}
		common::ScopedWorkbufferArena updateArena = common::gpThreadLocal->mWorkbuffer.Push();
		for (const auto& [rTick, rUpdate] : rPending.updates)
		{
			updateArena.mBuffer.PushBack(rTick);
		}
		std::span<int64_t> ticks = updateArena.mBuffer.Span<int64_t>();
		std::ranges::sort(ticks);
		for (int64_t iTick : ticks)
		{
			ReceivedCoordUpdate& rUpdate = rPending.updates.at(iTick);
			if (iTick > rCell.iConfirmedTick)
			{
				rCell.serverUpdates.try_emplace(iTick, Cell::CoordServerUpdate {.uiSharedCrc = rUpdate.uiSharedCrc, .statusChanges = std::move(rUpdate.statusChanges)});
			}
		}
		rPending.updates.clear();
	}
}

void ClientSessionRuntime::ResetPendingFrames()
{
	for (auto& [rCoordinate, rPending] : mPendingCells)
	{
		rPending.optionalFullState.reset();
		rPending.updates.clear();
	}
}

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
	mStateFlags.Set(ClientSessionStateFlags::kConnectionWasAccepted, false);
}

void ClientSessionRuntime::Connect(std::string_view serverAddress, int64_t iPort)
{
	// Heap: address copy, GUID file I/O, and Client/ENet construction
	ScopedSuppressAllocationTracking suppress;
	ResetForConnect();
	std::string serverAddressString(serverAddress);
	ClientGuid clientGuid = LoadClientGuidFromDisk();
	mpClient = std::make_unique<Client>(serverAddressString.c_str(), iPort, clientGuid, &PersistClientGuidToDisk);
	if (mpClient->mpHost == nullptr)
	{
		game::gpClientSession->OnConnectionFailed();
		Disconnect();
	}
}

void ClientSessionRuntime::ConnectToDiscoveredServer(int64_t iPort)
{
	mStateFlags.Set(ClientSessionStateFlags::kServerDiscovered, false);
	Connect(mpcDiscoveredAddress, iPort);
}

void ClientSessionRuntime::Disconnect()
{
	// Heap: Client/ENet and discovery teardown may allocate during cleanup
	ScopedSuppressAllocationTracking suppress;
	mpClient.reset();
	mpDiscoveryScanner.reset();
	ResetClock();
	ClearSubscriptionState();
	mStateFlags.Set(ClientSessionStateFlags::kServerDiscovered, false);
	mStateFlags.Set(ClientSessionStateFlags::kDiscoveryScanTimedOut, false);
	mStateFlags.Set(ClientSessionStateFlags::kNoFreeSlotLogged, false);
	mStateFlags.Set(ClientSessionStateFlags::kConnectionWasAccepted, false);
	game::gpClientSession->OnRuntimeDisconnected();
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
		std::snprintf(mpcDiscoveredAddress, sizeof(mpcDiscoveredAddress), "%s", mpDiscoveryScanner->mpcFoundAddress);
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
	PollPendingCrashReport(mpClient.get());
	PollDiscovery();
	PollTerrainPreparation();
	PublishPreparedCells();
	if (mpClient == nullptr)
	{
		return;
	}

	mpClient->Poll(rTimeState);
	// Client clears kConnectionAccepted on disconnect; the session bit keeps a lost accepted connection on the accepted
	// branch, whose disconnect check routes it to OnConnectionLost rather than OnConnectionFailed.
	if (!(mpClient->mStateFlags & Client::ClientStateFlags::kConnectionAccepted) && !(mStateFlags & ClientSessionStateFlags::kConnectionWasAccepted))
	{
		if (const char* pcRejection = mpClient->mpcRejectionReason[0] != '\0' ? mpClient->mpcRejectionReason : nullptr; pcRejection != nullptr)
		{
			game::gpClientSession->OnConnectionRejected(pcRejection);
			Disconnect();
		}
		else if (mpClient->mStateFlags & Client::ClientStateFlags::kDisconnectedEvent)
		{
			game::gpClientSession->OnConnectionFailed();
			Disconnect();
		}
		else
		{
			SendAckAndFlush();
		}
		return;
	}

	mStateFlags.Set(ClientSessionStateFlags::kConnectionWasAccepted);
	game::gpClientSession->OnConnectionAccepted();
	game::gpClientSession->PollDesynchronizationState();
	if (mpClient == nullptr)
	{
		return;
	}
	if (mpClient->mStateFlags & Client::ClientStateFlags::kDisconnectedEvent)
	{
		game::gpClientSession->OnConnectionLost();
		if (mpClient != nullptr)
		{
			Disconnect();
		}
		return;
	}
	if (std::optional<int64_t> optionalLoadGeneration = mpClient->DrainLoadNotification(); optionalLoadGeneration.has_value())
	{
		mpClient->miCommittedLoadGeneration = *optionalLoadGeneration;
		ResetForServerLoad();
		game::gpClientSession->ResetSessionForServerLoad();
	}
	game::gpClientSession->ApplyOwnedEntityEvents();
	game::gpClientSession->ProcessReceivedGamePackets();
	game::gpClientSession->ApplyReceivedStaticData();
	ApplyReceivedFullStates();
	ApplyReceivedUpdates();
	PublishPreparedCells();

	SendAckAndFlush();
}

void ClientSessionRuntime::ApplyReceivedFullStates()
{
	// Heap: accepted frames remain owned until matching terrain is ready.
	ScopedSuppressAllocationTracking suppress;
	for (ReceivedCoordFullState& rFullState : mpClient->mReceivedFullStates)
	{
		if (!IsCurrentIdentity(rFullState.identity))
		{
			continue;
		}
		PendingCell& rPending = PendingForIdentity(rFullState.identity);
		if (!rPending.optionalFullState.has_value() || rFullState.iTick > rPending.optionalFullState->iTick)
		{
			rPending.optionalFullState = std::move(rFullState);
		}
	}
	mpClient->mReceivedFullStates.clear();
}

void ClientSessionRuntime::AdoptFullState(ReceivedCoordFullState& rFullState)
{
	GridCoord coordinate = rFullState.identity.coordinate;
	int64_t iTick = rFullState.iTick;

	Cell& rCell = game::gpGame->mCells.try_emplace(coordinate).first->second;

	game::gpClientSession->HydrateReceivedFullState(*rFullState.pFrame, rCell.TailFrame());

	if (rCell.iConfirmedTick < 0)
	{
		bool bInitialSetup = (GetConfirmedTick() < 0);

		rCell.iSnapshotHead = 0;
		rCell.ppSnapshots[0] = std::move(rFullState.pFrame);
		rCell.ppSnapshots[0]->postRender.uiSharedCrc = rCell.ppSnapshots[0]->Crc();
		rCell.iSnapshotCount = 1;
		rCell.iConfirmedTick = iTick;
		rCell.iLastFullStateTick = iTick;
		rCell.iConfirmedOffset = 0;

		// A full state carries the server's tick when sent and a paused server sends no delta, so without this
		// seed a load reset or connect while paused leaves no clock reference to bound the simulation ceiling.
		if (miLatestServerTick < 0)
		{
			miLatestServerTick = iTick;
			mLatestServerTickArrival = std::chrono::steady_clock::now();
		}

		// Only the first full state sets the game clock. Start behind the server by the jitter-safety floor,
		// matching EvaluateClock, to avoid a ~150 ms ceiling stall and an initial target-behind error.
		// Clamp the offset to iTick so a freshly loaded server cannot produce a negative simulation tick.
		if (bInitialSetup)
		{
			// iTickWallNanoseconds is one tick's wall duration at the current time scale, so dividing the
			// wall-clock nanosecond numerator by it keeps the tick count correct as the time scale changes.
			int64_t iTickWallNanoseconds = game::gpGame->mTimeStep.SimulationToWall(engine::kTickNanoseconds).count();
			int64_t iInitialTargetBehind = (engine::kiJitterSafetyMicroseconds * 1'000 + iTickWallNanoseconds - 1) / iTickWallNanoseconds;
			int64_t iAppliedBehind = std::min<int64_t>(iInitialTargetBehind, iTick);
			int64_t iTickCounter = iTick - iAppliedBehind;
			ASSERT(iTickCounter >= 0);
			game::gpGame->miTickCounter = iTickCounter;
			game::gpGame->ResetRenderClock();
		}
	}
	else
	{
		if (iTick <= rCell.iConfirmedTick)
		{
			LOG(kNetwork, kVerbose, "ApplyReceivedFullStates Rejected stale full state Coord: ({},{}) FullStateTick: {} ConfirmedTick: {}", coordinate.iQ, coordinate.iR, iTick, rCell.iConfirmedTick);
			return;
		}

		// Coord already has confirmed state: store as pending for reconcile injection
		rCell.optionalPendingFullState = Cell::PendingFullState
		{
			.iTick = iTick,
			.pFrame = std::move(rFullState.pFrame),
		};
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

		for (ReceivedCoordUpdate& rUpdate : rSlotUpdates)
		{
			if (!IsCurrentIdentity(rUpdate.identity))
			{
				continue;
			}
			GridCoord coordinate = rUpdate.identity.coordinate;
			PendingCell& rPending = PendingForIdentity(rUpdate.identity);
			auto it = game::gpGame->mCells.find(coordinate);
			Cell* pCell = it != game::gpGame->mCells.end() && rPending.bTerrainReady && !rPending.optionalAuthoritativeStaticData.has_value() && it->second.iConfirmedTick >= 0 ? &it->second : nullptr;
			int64_t iConfirmedTick = pCell != nullptr ? pCell->iConfirmedTick : rPending.optionalFullState.has_value() ? rPending.optionalFullState->iTick : -1;
			if (rUpdate.iTick <= iConfirmedTick)
			{
				continue;
			}

			if (rUpdate.iTick > miLatestServerTick)
			{
				// An arrival never steps the estimate down: a newer tick that arrives below it (an out-of-order
				// arrival, or the first arrival after a gap) backdates the stamp by the lead so the estimate keeps its
				// value. Each later newer arrival restarts the tick phase, dropping the fraction of a tick elapsed, so
				// the lead eases back; a newer tick at or above the estimate clears it. Sampling now before the
				// estimate keeps a tick boundary between the two clock reads from lowering it.
				std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
				int64_t iLeadTicks = std::max<int64_t>(0, EstimatedLatestServerTick() - rUpdate.iTick);
				miLatestServerTick = rUpdate.iTick;
				mLatestServerTickArrival = now - iLeadTicks * game::gpGame->mTimeStep.SimulationToWall(engine::kTickNanoseconds);
			}

			int64_t iBufferedCount = pCell != nullptr ? std::ssize(pCell->serverUpdates) : std::ssize(rPending.updates);
			bool bDuplicate = pCell != nullptr ? pCell->serverUpdates.contains(rUpdate.iTick) : rPending.updates.contains(rUpdate.iTick);
			if (bDuplicate)
			{
				continue;
			}
			if (iBufferedCount >= engine::kiMaximumBufferedFrames)
			{
				LOG(kNetwork, kWarning, "ClientSession::ApplyReceivedUpdates Buffer full, requesting full-state resync Coord: ({},{}) Size: {} Tick: {}", coordinate.iQ, coordinate.iR, iBufferedCount, rUpdate.iTick);

				// The engine already acked these ticks, so a dropped update would never be resent: abandon the
				// whole drain and take authoritative state instead. Returning here sends exactly one request even
				// when several coords are over budget, and the reset plus the discard below empties every
				// serverUpdates map, so this branch cannot arm again for at least kiMaximumBufferedFrames ticks.
				mpClient->SendResynchronizationRequest();
				game::gpClientSession->ResetCoordinateStatesForResynchronization();
				for (std::vector<ReceivedCoordUpdate>& rDrainedUpdates : rAllUpdates)
				{
					rDrainedUpdates.clear();
				}
				return false;
			}

			if (pCell != nullptr)
			{
				pCell->serverUpdates.try_emplace(rUpdate.iTick, Cell::CoordServerUpdate {.uiSharedCrc = rUpdate.uiSharedCrc, .statusChanges = std::move(rUpdate.statusChanges)});
			}
			else
			{
				rPending.updates.try_emplace(rUpdate.iTick, std::move(rUpdate));
			}
			bHasNewData = true;
		}

		rSlotUpdates.clear();
	}

	return bHasNewData;
}

int64_t ClientSessionRuntime::GetConfirmedTick() const
{
	int64_t iMinimumTick = -1;
	for (const auto& [rCoordinate, rCell] : game::gpGame->mCells)
	{
		if (rCell.iConfirmedTick >= 0 && (iMinimumTick < 0 || rCell.iConfirmedTick < iMinimumTick))
		{
			iMinimumTick = rCell.iConfirmedTick;
		}
	}
	return iMinimumTick;
}

int64_t ClientSessionRuntime::GetClientConfirmedTick() const
{
	auto it = game::gpGame->mCells.find(game::gpGame->mClientGridCoordinate);
	if (it == game::gpGame->mCells.end())
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
	for (const auto& [rCoordinate, rPending] : mPendingCells)
	{
		iTotal += std::ssize(rPending.updates);
	}
	for (const auto& [rCoordinate, rCell] : game::gpGame->mCells)
	{
		if (rCell.iConfirmedTick >= 0)
		{
			iTotal += static_cast<int64_t>(rCell.serverUpdates.size());
		}
	}
	return iTotal;
}

int64_t ClientSessionRuntime::EstimatedLatestServerTick() const
{
	if (miLatestServerTick < 0)
	{
		return -1;
	}
	// Elapsed time is non-negative, so integer division floors to whole scaled ticks.
	int64_t iTickWallNanoseconds = game::gpGame->mTimeStep.SimulationToWall(engine::kTickNanoseconds).count();
	int64_t iElapsedTicks = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - mLatestServerTickArrival).count() / iTickWallNanoseconds;
	return miLatestServerTick + std::min(iElapsedTicks, kiMaximumEstimatedServerTickLead);
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
	if (!bChanged)
	{
		bChanged = !std::ranges::equal(desiredCoordinates, mDesiredCoordinates);
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
	for (const GridCoord& rCoordinate : desiredCoordinates)
	{
		mUnwantedTimestamps.erase(rCoordinate);
	}
	mDesiredCoordinates.assign(desiredCoordinates.begin(), desiredCoordinates.end());
	LOG(kNetwork, kVerbose, "Desired subscriptions changed Reason: {} Count: {} Tick: {}", reason, std::ssize(desiredCoordinates), iTick);
}

void ClientSessionRuntime::UpdateDesiredCoordinates(std::string_view reason)
{
	std::optional<common::LogTickScope> optionalTickScope;
	InitializeLogTickScope(optionalTickScope);

	common::ScopedWorkbufferArena desiredArena = common::gpThreadLocal->mWorkbuffer.Push();
	if (game::gpGame->HasFocusedClientEntity())
	{
		desiredArena.mBuffer.PushBack(game::gpGame->mClientGridCoordinate);
		for (const GridCoord& rCoordinate : std::span<const GridCoord>(mpVisibleNeighbors, static_cast<size_t>(miVisibleNeighborCount)))
		{
			desiredArena.mBuffer.PushBack(rCoordinate);
		}
	}
	else
	{
		desiredArena.mBuffer.PushBack(kOriginCoordinate);
	}

	// During a camera jump the confirmed cells under the previous cell loaded area stay desired until the jump ends.
	// They already hold their slots, so keeping them subscribes no new cell.
	if (gpCamera->mbPreviousCellLoadedAreaValid)
	{
		for (const auto& [rCoordinate, rCell] : game::gpGame->mCells)
		{
			if (rCell.iConfirmedTick >= 0 && !ContainsCoordinate(desiredArena.mBuffer.Span<GridCoord>(), rCoordinate) && common::RectangleOverlapsHex(gpCamera->mf4PreviousCellLoadedArea, CellCenterOffset(gpCamera->mBasisCoordinate, rCoordinate), kCellDirections))
			{
				desiredArena.mBuffer.PushBack(rCoordinate);
			}
		}
	}

	SetDesiredCoordinates(desiredArena.mBuffer.Span<GridCoord>(), reason, game::gpGame->miTickCounter);
}

void ClientSessionRuntime::SynchronizeSubscriptions()
{
	if (mpClient == nullptr)
	{
		return;
	}
	// Same gate as PollAndDrain, so local subscription bookkeeping continues until the disconnect event is handled.
	if (!(mpClient->mStateFlags & Client::ClientStateFlags::kConnectionAccepted) && !(mStateFlags & ClientSessionStateFlags::kConnectionWasAccepted))
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
			game::gpGame->mCells.erase(coordinate);
			mPendingCells.erase(coordinate);
		}
	}

	for (const SubscribeRequest& rRecord : mpClient->mSubscriptions.mSubscribeRequests.mRecords)
	{
		if (!(rRecord.flags & SubscribeRequestFlags::kCancelled) &&!ContainsCoordinate(desiredCoordinates, rRecord.coordinate))
		{
			mpClient->mSubscriptions.mSubscribeRequests.Cancel(rRecord.coordinate);
			game::gpGame->mCells.erase(rRecord.coordinate);
			mPendingCells.erase(rRecord.coordinate);
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
	mPendingCells.clear();
	mDesiredCoordinates.clear();
	mUnwantedTimestamps.clear();
	mSubscriptionQueue.clear();
	miVisibleNeighborCount = 0;
}

void ClientSessionRuntime::ComputeActiveSet()
{
	// Heap: mActiveCoordinates vector clear/push_back may allocate. Persists as Game member across frame updates
	ScopedSuppressAllocationTracking suppress;

	PollTerrainPreparation();
	PublishPreparedCells();

	if (!(game::gpGame->mGameFlags & GameFlags::kMainMenu))
	{
		game::gpGame->mActiveCoordinates.clear();
		for (const auto& [rCoordinate, rCell] : game::gpGame->mCells)
		{
			if ((rCell.staticData.flags & CellStaticDataFlags::kTerrainPrepared) && rCell.iSnapshotCount > 0 && (rCell.iConfirmedTick >= 0 || rCoordinate == game::gpGame->mClientGridCoordinate))
			{
				game::gpGame->mActiveCoordinates.push_back(rCoordinate);
			}
		}

		{
			auto it = game::gpGame->mCells.find(game::gpGame->mClientGridCoordinate);
			if (it == game::gpGame->mCells.end() || it->second.iSnapshotCount == 0)
			{
				game::gpGame->CreateCellAtCoordinate(game::gpGame->mClientGridCoordinate);
			}
			it = game::gpGame->mCells.find(game::gpGame->mClientGridCoordinate);
			if (it != game::gpGame->mCells.end() && it->second.iSnapshotCount > 0 && (it->second.staticData.flags & CellStaticDataFlags::kTerrainPrepared) && !std::ranges::contains(game::gpGame->mActiveCoordinates, game::gpGame->mClientGridCoordinate))
			{
				game::gpGame->mActiveCoordinates.push_back(game::gpGame->mClientGridCoordinate);
			}
		}
		if (!std::ranges::contains(game::gpGame->mActiveCoordinates, game::gpGame->mClientGridCoordinate))
		{
			auto it = game::gpGame->mCells.find(gpCamera->mBasisCoordinate);
			if (it != game::gpGame->mCells.end() && (it->second.staticData.flags & CellStaticDataFlags::kTerrainPrepared) && it->second.iSnapshotCount > 0 && !std::ranges::contains(game::gpGame->mActiveCoordinates, it->first))
			{
				game::gpGame->mActiveCoordinates.push_back(it->first);
			}
		}

		miVisibleNeighborCount = 0;
		if (game::gpGame->HasFocusedClientEntity())
		{
			// The cell loaded area, packed (minX, maxY, maxX, minY): the world framing's footprint, so a framing
			// override (the map) loads no extra cells, centred on the destination during a camera jump.
			const XMFLOAT4& rf4CellLoadedArea = gpCamera->mf4CellLoadedArea;
			// The cell loaded area is in the camera basis cell's frame, which is not always the client cell: it lags a
			// client-cell change and can be another coord entirely while render-camera selection falls back. Each
			// neighbour hexagon is therefore centered at that neighbour's own offset from the camera basis, so both
			// sides of the test are always in the same frame.
			auto EnsureNeighbor = [&](GridCoord neighbor)
			{
				auto it = game::gpGame->mCells.find(neighbor);
				if (it == game::gpGame->mCells.end() || it->second.iSnapshotCount == 0)
				{
					game::gpGame->CreateCellAtCoordinate(neighbor);
				}
				it = game::gpGame->mCells.find(neighbor);
				if (it != game::gpGame->mCells.end() && it->second.iSnapshotCount > 0 && (it->second.staticData.flags & CellStaticDataFlags::kTerrainPrepared) && !std::ranges::contains(game::gpGame->mActiveCoordinates, neighbor))
				{
					game::gpGame->mActiveCoordinates.push_back(neighbor);
				}
			};

			// Edge-neighbour-only clamp: at wide zoom the cell loaded area may extend past the six edge neighbours;
			// only that ring is ever newly subscribed.
			for (const common::HexDirection& rDirection : kCellDirections.pEntries)
			{
				// A cell at the numeric edge of the grid has no neighbour in that direction; skip it rather than wrap
				// to the opposite end.
				GridCoord neighbor {};
				if (!TryAddGridCoordinate(game::gpGame->mClientGridCoordinate, rDirection.iDeltaQ, rDirection.iDeltaR, neighbor))
				{
					continue;
				}
				if (common::RectangleOverlapsHex(rf4CellLoadedArea, CellCenterOffset(gpCamera->mBasisCoordinate, neighbor), kCellDirections))
				{
					mpVisibleNeighbors[miVisibleNeighborCount++] = neighbor;
					EnsureNeighbor(neighbor);
				}
			}
		}

	}
	else
	{
		game::gpGame->mActiveCoordinates.clear();
		game::gpGame->mActiveCoordinates.push_back(game::gpGame->mClientGridCoordinate);
	}

	std::erase_if(mPendingCells, [&](const std::pair<const GridCoord, PendingCell>& rPair)
	{
		const PendingCell& rPending = rPair.second;
		if (rPending.identity.iSlot >= 0)
		{
			return !IsCurrentIdentity(rPending.identity);
		}
		return rPair.first != game::gpGame->mClientGridCoordinate && !ContainsCoordinate(std::span<const GridCoord>(mpVisibleNeighbors, static_cast<size_t>(miVisibleNeighborCount)), rPair.first) && !HoldsSlotOrRequest(rPair.first);
	});
	StartTerrainPreparation();

	// Delete local-only cells outside the active set, preserve network-subscribed cells. A cell whose coordinate holds an
	// active subscription slot or a live subscribe request counts as subscribed even while unconfirmed, as between a
	// resynchronization reset and its full state, or when its static data arrives before its subscribe accept, so it and
	// its static data live as long as that slot or request.
	std::erase_if(game::gpGame->mCells, [&](const std::pair<const GridCoord, Cell>& rPair)
	{
		return !std::ranges::contains(game::gpGame->mActiveCoordinates, rPair.first) && rPair.first != game::gpGame->mClientGridCoordinate && !ContainsCoordinate(std::span<const GridCoord>(mpVisibleNeighbors, static_cast<size_t>(miVisibleNeighborCount)), rPair.first) && rPair.second.iConfirmedTick < 0 && !HoldsSlotOrRequest(rPair.first);
	});

	ASSERT(std::ranges::count_if(game::gpGame->mCells, [&](const std::pair<const GridCoord, Cell>& rPair)
	{
		return rPair.second.iConfirmedTick < 0 && !HoldsSlotOrRequest(rPair.first);
	}) <= 2 + common::kiHexDirectionCount);
}

void ClientSessionRuntime::PinClientGridCoordinate(GridCoord coordinate)
{
	mStateFlags.Set(ClientSessionStateFlags::kClientGridCoordinatePinned, false);
	game::gpGame->SetClientGridCoordinate(coordinate);
	mStateFlags.Set(ClientSessionStateFlags::kClientGridCoordinatePinned);
}

StandardMenuState_t ClientSessionRuntime::GetStandardMenuState() const
{
	StandardMenuState_t state;
	state.Set(StandardMenuState::kClientPresent, mpClient != nullptr);
	state.Set(StandardMenuState::kDiscoveryScannerPresent, mpDiscoveryScanner != nullptr);
	state.Set(StandardMenuState::kServerDiscovered, mStateFlags & ClientSessionStateFlags::kServerDiscovered);
	// Set only while a client exists, so the menu can test this bit on its own.
	state.Set(StandardMenuState::kConnectionAccepted, mpClient != nullptr && (mpClient->mStateFlags & Client::ClientStateFlags::kConnectionAccepted));
	state.Set(StandardMenuState::kAutoConnect, kbAutoConnect);
	return state;
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
	int64_t iEstimatedLatestServerTick = EstimatedLatestServerTick();
	miClockOffset = iPreReconcileTick - iEstimatedLatestServerTick;
	miClockError = iPreReconcileTick - (iEstimatedLatestServerTick - miCurrentTargetBehind);
	bool bPeriodic = iPreReconcileTick % (kiTickRate * 32) == 0 && iPreReconcileTick != miLastPeriodicClockLogTick;
	bool bError = std::abs(miClockError) >= 4 && iPreReconcileTick != miLastClockErrorLogTick;
	if (miCurrentTargetBehind != miLastLoggedClockTargetBehind || bPeriodic || bError)
	{
		LOG(kNetwork, kVerbose, "ClockSync TargetBehind: {} Error: {} Offset: {} JitterUs: {} LatestServer: {} SimTick: {}", miCurrentTargetBehind, miClockError, miClockOffset, iJitterMicroseconds, iEstimatedLatestServerTick, iPreReconcileTick);
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
	int64_t iSteps = std::clamp(miClockError, -4i64, 4i64);
	int64_t iDivisor = std::abs(miClockError) >= 4 ? 8 : 64;
	return std::chrono::nanoseconds(-iSteps * kTickNanoseconds.count() / iDivisor);
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
		int64_t iEstimatedLatestServerTick = EstimatedLatestServerTick();
		int64_t iSnapTick = std::max<int64_t>(0, iEstimatedLatestServerTick - miCurrentTargetBehind);
		LOG(kNetwork, kWarning, "ClientSessionRuntime::ApplyClockCorrection Clock snap OldTick: {} NewTick: {} LatestServerTick: {} TargetBehind: {}", iPreReconcileTick, iSnapTick, iEstimatedLatestServerTick, miCurrentTargetBehind);
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
