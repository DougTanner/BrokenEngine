#include "Pch.h"

#include "Network/Server/ServerSessionRuntime.h"

#if defined(BT_SERVER)

#include "File/Replay.h"
#include "Network/Server/Server.h"
#include "Network/Server/ServerBroadcaster.h"
#include "Network/NetworkDiscoveryResponder.h"

#include "Network/Server/ServerSession.h"
#include "Game.h"

namespace engine
{

ServerSessionRuntime::ServerSessionRuntime(int64_t iPort)
{
	mpDiscoveryResponder = std::make_unique<NetworkDiscoveryResponder>();
	timeBeginPeriod(1);
	mHandleTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
	try
	{
		mpServer = std::make_unique<Server>(iPort);
	}
	catch (...)
	{
		CloseHandle(mHandleTimer);
		timeEndPeriod(1);
		throw;
	}
}

ServerSessionRuntime::~ServerSessionRuntime()
{
	mpServer.reset();
	CloseHandle(mHandleTimer);
	timeEndPeriod(1);
}

void ServerSessionRuntime::ResetOnLastClientLeave()
{
	// Runtime-owned reset after each post-poll sample: clear network-driven pause and timescale when the
	// engine client set transitions from non-empty to empty. Sampling after the game's AfterNetworkPoll returns catches
	// every removal path, while an already-empty server retains its commanded pause and timescale.
	bool bHasClients = !mpServer->mClients.empty();
	if (mbHadClients && !bHasClients)
	{
		game::gpGame->mGameFlags.Set(engine::GameFlags::kPaused, false);
		if (game::gpGame->mTimeStep.miTimeMultiply != 1 || game::gpGame->mTimeStep.miTimeDivide != 1)
		{
			game::gpGame->mTimeStep.SetTimeScale(1, 1);
		}
	}
	mbHadClients = bHasClients;
}

void ServerSessionRuntime::Poll(const NetworkTimeState& rTimeState)
{
	ASSERT(common::gpMultithreading->IsMainThread());
	// Heap: ENet/discovery polling and game request queues may grow
	ScopedSuppressAllocationTracking suppress;
	game::gpServerSession->BeforeNetworkPoll();
	mpServer->Poll(rTimeState, ServerPollMode::kUpdateStart);
	mpDiscoveryResponder->Poll();
	game::gpServerSession->DispatchDeferredClientPackets();
	game::gpServerSession->RebuildOwnedEntities();
	game::gpServerSession->AfterNetworkPoll();
	ResetOnLastClientLeave();
}

// Second poll of the update, run after WaitForTick so commands that arrived during the wait enter the
// imminent tick instead of the next one. BeforeNetworkPoll is deliberately omitted: it clears the previous
// update's pending request queues, so running it here would drop requests the update-start poll queued.
// Discovery is polled once per update by Poll above; the admission budget window opened there continues.
void ServerSessionRuntime::PollTickBoundary(const NetworkTimeState& rTimeState)
{
	ASSERT(common::gpMultithreading->IsMainThread());
	// Heap: ENet polling and game request queues may grow
	ScopedSuppressAllocationTracking suppress;
	mpServer->Poll(rTimeState, ServerPollMode::kTickBoundary);
	game::gpServerSession->DispatchDeferredClientPackets();
	game::gpServerSession->RebuildOwnedEntities();
	game::gpServerSession->AfterNetworkPoll();
	ResetOnLastClientLeave();
}

void ServerSessionRuntime::WaitForTick(TimeStep& rTimeStep)
{
	std::chrono::nanoseconds tickNanoseconds = rTimeStep.SimulationToWall(kTickNanoseconds);
	std::chrono::nanoseconds tickRemainderWallNanoseconds = rTimeStep.SimulationToWall(rTimeStep.mTickRemainderNanoseconds);
	static constexpr std::chrono::nanoseconds kSpinMarginNanoseconds = 500'000ns;
	std::chrono::nanoseconds remainingNanoseconds = tickNanoseconds - tickRemainderWallNanoseconds - rTimeStep.mRealTime.GetDeltaNs();
	std::chrono::nanoseconds sleepNanoseconds = remainingNanoseconds - kSpinMarginNanoseconds;
	if (sleepNanoseconds > 0ns)
	{
		LARGE_INTEGER dueTime {.QuadPart = -(sleepNanoseconds.count() / 100),};
		SetWaitableTimerEx(mHandleTimer, &dueTime, 0, nullptr, nullptr, nullptr, 0);
		WaitForSingleObject(mHandleTimer, INFINITE);
	}
	while (rTimeStep.mRealTime.GetDeltaNs() + tickRemainderWallNanoseconds < tickNanoseconds)
	{
		YieldProcessor();
	}

	std::chrono::nanoseconds marginNanoseconds = tickNanoseconds / 64;
	std::chrono::nanoseconds remainderNanoseconds = rTimeStep.mRealTime.GetDeltaNs() + tickRemainderWallNanoseconds - tickNanoseconds;
	static int64_t siTotalTicks = 0;
	static int64_t siOvershootTicks = 0;
	++siTotalTicks;
	if ((remainderNanoseconds < 0ns || remainderNanoseconds > marginNanoseconds)) [[unlikely]]
	{
		++siOvershootTicks;
		if constexpr (kbProfilingFrameSpike)
		{
			LOG(kNetwork, kVerbose, "ServerSessionRuntime::WaitForTick Remainder: {}ns Overshoot: {}/{} = {}%", remainderNanoseconds.count(), siOvershootTicks, siTotalTicks, siOvershootTicks * 100 / siTotalTicks);
		}
	}
}

void ServerSessionRuntime::PreparePausedSubscriptions()
{
	for (const engine::PendingNewSubscription& rSubscription : mpServer->mPendingNewSubscriptions)
	{
		auto it = game::gpGame->mCells.find(rSubscription.coordinate);
		if (it == game::gpGame->mCells.end())
		{
			continue;
		}
		engine::CellStaticData& rStaticData = it->second.staticData;
		if (!rStaticData.bNavigationDataBuilt)
		{
			// Heap: BuildCellNavigationData grows the per-plane navigationData vector and each plane's vertex, polygon, and visibility-edge vectors, and
			// this path runs on the main thread inside the armed main loop
			ScopedSuppressAllocationTracking suppress;
			engine::BuildCellNavigationData(rStaticData.navigationData, rStaticData.islands);
			rStaticData.bNavigationDataBuilt = true;
		}
	}
}

void ServerSessionRuntime::HandleResyncRequests()
{
	std::vector<int64_t>& rResyncClientIds = mpServer->mPendingResynchronizationClientIds;
	if (rResyncClientIds.empty())
	{
		return;
	}

	// Heap: per-resync per-slot SendCoordinateFullState allocates serialization buffers
	ScopedSuppressAllocationTracking suppress;

	for (int64_t iClientId : rResyncClientIds)
	{
		engine::ClientConnection* pClient = engine::gpServer->FindClient(iClientId);
		if (pClient == nullptr)
		{
			continue;
		}

		LOG(kNetwork, kWarning, "ServerSessionRuntime::HandleResyncRequests Client: {}", iClientId);

		for (int64_t i = 0; i < std::ssize(pClient->slots); ++i)
		{
			if (!(pClient->slots.at(i).subscription.flags & engine::SubscriptionFlags::kActive))
			{
				continue;
			}

			engine::GridCoord coord = pClient->slots.at(i).subscription.coordinate;

			// A slot whose new subscription is still queued gets its full state from that entry, after its static
			// data; a resync full state sent first would activate the client slot, which then drops the static data
			bool bNewSubscriptionQueued = std::ranges::any_of(mpServer->mPendingNewSubscriptions, [iClientId, i, coord](const engine::PendingNewSubscription& rPending)
			{
				return rPending.iClientId == iClientId && rPending.iSlot == i && rPending.coordinate == coord;
			});
			if (bNewSubscriptionQueued)
			{
				continue;
			}

			auto it = game::gpGame->mCells.find(coord);
			if (it == game::gpGame->mCells.end())
			{
				continue;
			}

			mpServer->mBufferedFrames.SendCoordinateFullState(iClientId, i, game::gpGame->miTickCounter, coord, it->second.pCurrent.get());
		}
	}

	// Persist-until-served: Server::Poll leaves this runtime-owned queue intact. Clear it after servicing so a
	// request received during a paused or otherwise zero-tick update (iFullTicks == 0) survives across polls until
	// ServerSessionRuntime::CompleteTick or the current zero-tick ServerSessionRuntime::CompleteUpdate services it.
	rResyncClientIds.clear();
}

// Runs inside ENet calls (polls, peer disconnects, host teardown), so it only marks the entry holding the packet.
// ENet sets ENET_PACKET_FLAG_SENT only when the acknowledgment of the packet's last unacknowledged fragment frees it;
// a reset, a timeout, or enet_host_destroy frees it without the flag.
static void OnStaticDataPacketFreed(ENetPacket* pPacket)
{
	for (engine::PendingNewSubscription& rSubscription : engine::gpServer->mPendingNewSubscriptions)
	{
		if (rSubscription.pStaticDataPacket == pPacket)
		{
			rSubscription.pStaticDataPacket = nullptr;
			rSubscription.bStaticDataDelivered = (pPacket->flags & ENET_PACKET_FLAG_SENT) != 0;
			return;
		}
	}
}

void ServerSessionRuntime::SendNewSubscriptionFullStates()
{
	// Persist-until-served: Server::Poll leaves this runtime-owned queue intact, and an entry leaves it only once its
	// full state is sent, its static-data send fails, or its client or slot is gone. Static data always carries
	// navigation data, so an entry waits for a frame whose navigation data is built: a replay reader activated this tick
	// builds on its first dispatch next tick, and a paused or otherwise zero-tick update builds in CompleteUpdate
	// through PreparePausedSubscriptions. The full state waits for ENet's delivery confirmation of the static data and
	// carries the tick counter current when it is sent, so a multi-poll static-data transfer does not leave it stale.
	// A client is served one entry at a time, so only an erased or restarted entry's static data can still be in flight
	// ahead of a full state.
	std::vector<engine::PendingNewSubscription>& rPendingSubscriptions = mpServer->mPendingNewSubscriptions;
	for (int64_t i = 0; i < std::ssize(rPendingSubscriptions);)
	{
		engine::PendingNewSubscription& rSubscription = rPendingSubscriptions.at(i);
		const engine::ClientConnection* pClient = engine::gpServer->FindClient(rSubscription.iClientId);
		bool bSlotStillValid = (pClient != nullptr
		                     && (pClient->slots.at(rSubscription.iSlot).subscription.flags & engine::SubscriptionFlags::kActive)
		                     && pClient->slots.at(rSubscription.iSlot).subscription.coordinate == rSubscription.coordinate);
		if (!bSlotStillValid)
		{
			rPendingSubscriptions.erase(rPendingSubscriptions.begin() + i);
			continue;
		}

		auto it = game::gpGame->mCells.find(rSubscription.coordinate);
		if (it == game::gpGame->mCells.end() || !it->second.staticData.bNavigationDataBuilt)
		{
			++i;
			continue;
		}

		if (rSubscription.bStaticDataDelivered)
		{
			mpServer->mBufferedFrames.SendCoordinateFullState(rSubscription.iClientId, rSubscription.iSlot, game::gpGame->miTickCounter, rSubscription.coordinate, it->second.pCurrent.get());
			rPendingSubscriptions.erase(rPendingSubscriptions.begin() + i);
			continue;
		}

		// Includes this entry, so an entry whose static data is in flight waits for it
		bool bClientBeingServed = std::ranges::any_of(rPendingSubscriptions, [&rSubscription](const engine::PendingNewSubscription& rOther)
		{
			return rOther.iClientId == rSubscription.iClientId && (rOther.pStaticDataPacket != nullptr || rOther.bStaticDataDelivered);
		});
		if (!bClientBeingServed)
		{
			rSubscription.pStaticDataPacket = mpServer->mBufferedFrames.SendCoordinateStaticData(*pClient, rSubscription.iSlot, rSubscription.coordinate, it->second.staticData, &OnStaticDataPacketFreed);
			if (rSubscription.pStaticDataPacket == nullptr)
			{
				rPendingSubscriptions.erase(rPendingSubscriptions.begin() + i);
				continue;
			}
		}
		++i;
	}
}

void ServerSessionRuntime::CompleteTick(int64_t iTick)
{
	// Heap: client/resync/subscription bookkeeping and ENet sends outside publication construction
	ScopedSuppressAllocationTracking suppress;
	HandleResyncRequests();
	game::gpServerSession->RebuildOwnedEntities();
	game::gpServerSession->FinalizeTickClients();
	{
		ScopedResumeAllocationTracking resume;
		common::ScopedWorkbufferArena publicationArena = common::gpThreadLocal->mWorkbuffer.Push();
		game::gpServerSession->mpBroadcaster->BuildTickPublication(iTick, *this, publicationArena);
	}
	game::gpServerSession->mpBroadcaster->mBroadcastStatusChanges.clear();
	SendNewSubscriptionFullStates();
	mpServer->Flush();
}

void ServerSessionRuntime::CompleteUpdate(int64_t iFullTicks, int64_t iTick)
{
	// Heap: resend or zero-tick subscription serialization and ENet sends
	ScopedSuppressAllocationTracking suppress;
	if (iFullTicks > 0)
	{
		for (ClientConnection& rClient : mpServer->mClients)
		{
			mpServer->mBufferedFrames.SendResends(rClient, iTick);
		}
		return;
	}
	PreparePausedSubscriptions();
	HandleResyncRequests();
	SendNewSubscriptionFullStates();
	mpServer->Flush();
}

void ServerSessionRuntime::ResetTransportForLoad()
{
	mpServer->mBufferedFrames.ClearBufferedFrames();
	mpServer->mPendingNewSubscriptions.clear();
	mpServer->mPendingResynchronizationClientIds.clear();
	mpServer->Flush();
}

void ServerSessionRuntime::PublishTick(int64_t iTick, std::span<const std::pair<GridCoord, GridUpdateData>> gridUpdates, std::span<const std::pair<GridCoord, const game::Frame*>> fullFrames)
{
	mpServer->mBufferedFrames.BufferFrame(iTick, gridUpdates);
	if (!fullFrames.empty())
	{
		mpServer->mBufferedFrames.BufferFullFrame(iTick, fullFrames);
	}
	for (ClientConnection& rClient : mpServer->mClients)
	{
		mpServer->mBufferedFrames.SendUpdate(rClient, iTick);
	}
}

void ServerSessionRuntime::AddSubscribedCoords()
{
	const std::vector<engine::ClientConnection>& rClients = mpServer->mClients;
	for (const engine::ClientConnection& rClient : rClients)
	{
		for (const engine::ClientConnection::SlotState& rSlot : rClient.slots)
		{
			if (rSlot.subscription.flags & engine::SubscriptionFlags::kActive)
			{
				engine::GridCoord coord = rSlot.subscription.coordinate;
				if (!std::ranges::contains(game::gpGame->mActiveCoordinates, coord))
				{
					game::gpGame->mActiveCoordinates.push_back(coord);
				}
			}
		}
	}
}

void ServerSessionRuntime::SyncActiveCells()
{
	for (const engine::GridCoord& rCoord : game::gpGame->mActiveCoordinates)
	{
		if (!game::gpGame->mCells.contains(rCoord))
		{
			game::gpGame->CreateCellAtCoordinate(rCoord);
		}
	}

	// Delete cells outside the active set, handing a recording writer its final complete current frame first.
	for (auto it = game::gpGame->mCells.begin(); it != game::gpGame->mCells.end();)
	{
		if (std::ranges::contains(game::gpGame->mActiveCoordinates, it->first))
		{
			++it;
			continue;
		}

		engine::gpReplay->RetireCoordinate(it->first, std::move(it->second.pCurrent));
		it = game::gpGame->mCells.erase(it);
	}
}

void ServerSessionRuntime::ComputeActiveSet()
{
	// Heap: mActiveCoordinates vector clear/push_back may allocate. Persists as Game member across frame updates
	ScopedSuppressAllocationTracking suppress;

	game::gpGame->mActiveCoordinates.clear();
	AddSubscribedCoords();
	game::gpServerSession->AddOwnedEntityCoordinates();

	if (!std::ranges::contains(game::gpGame->mActiveCoordinates, engine::kOriginCoordinate))
	{
		game::gpGame->mActiveCoordinates.push_back(engine::kOriginCoordinate);
	}

	SyncActiveCells();
}

} // namespace engine

#endif // BT_SERVER
