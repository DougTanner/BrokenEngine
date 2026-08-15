#include "Pch.h"

#include "Network/Server/ServerSessionRuntime.h"

#if defined(BT_SERVER)

#include "Network/NetworkDiscoveryResponder.h"
#include "Network/Server/Server.h"

#include "Game.h"
#include "Network/Server/ServerBroadcaster.h"
#include "Network/Server/ServerSession.h"

namespace engine
{

ServerSessionRuntime::ServerSessionRuntime(game::ServerSession& rSession, uint16_t uiPort)
:	mrSession(rSession)
{
	mpDiscoveryResponder = std::make_unique<NetworkDiscoveryResponder>();
	timeBeginPeriod(1);
	mTimerHandle = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
	try
	{
		mpServer = std::make_unique<Server>(uiPort, *this);
	}
	catch (...)
	{
		CloseHandle(mTimerHandle);
		timeEndPeriod(1);
		throw;
	}
}

ServerSessionRuntime::~ServerSessionRuntime()
{
	mpServer.reset();
	CloseHandle(mTimerHandle);
	timeEndPeriod(1);
}

void ServerSessionRuntime::ResetOnLastClientLeave()
{
	// Runtime-owned reset after each post-poll sample: clear network-driven pause and timescale when the
	// engine client set transitions from non-empty to empty. Sampling after the complete game hook catches
	// every removal path, while an already-empty server retains its commanded pause and timescale.
	bool bHasClients = !mpServer->mClients.empty();
	if (mbHadClients && !bHasClients)
	{
		game::gpGame->mGameFlags.Clear(engine::GameFlags::kPaused);
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
	mrSession.BeforeNetworkPoll();
	mpServer->Poll(rTimeState, ServerPollMode::kUpdateStart);
	mpDiscoveryResponder->Poll();
	mrSession.AfterNetworkPoll();
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
	mrSession.AfterNetworkPoll();
	ResetOnLastClientLeave();
}

void ServerSessionRuntime::WaitForTick(TimeStep& rTimeStep)
{
	std::chrono::nanoseconds tickNanoseconds = rTimeStep.SimToWall(game::NetworkSessionContract::kTickDuration);
	static constexpr std::chrono::nanoseconds kSpinMarginNanoseconds = 500'000ns;
	std::chrono::nanoseconds remainingNanoseconds = tickNanoseconds - rTimeStep.mTickRemainderNs - rTimeStep.mRealTime.GetDeltaNs();
	std::chrono::nanoseconds sleepNanoseconds = remainingNanoseconds - kSpinMarginNanoseconds;
	if (sleepNanoseconds > 0ns)
	{
		LARGE_INTEGER dueTime {.QuadPart = -(sleepNanoseconds.count() / 100),};
		SetWaitableTimerEx(mTimerHandle, &dueTime, 0, nullptr, nullptr, nullptr, 0);
		WaitForSingleObject(mTimerHandle, INFINITE);
	}
	while (rTimeStep.mRealTime.GetDeltaNs() + rTimeStep.mTickRemainderNs < tickNanoseconds)
	{
		YieldProcessor();
	}

	std::chrono::nanoseconds marginNanoseconds = tickNanoseconds / 64;
	std::chrono::nanoseconds remainderNanoseconds = rTimeStep.mRealTime.GetDeltaNs() + rTimeStep.mTickRemainderNs - tickNanoseconds;
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
	for (const engine::PendingNewSubscription& rSub : mpServer->mPendingNewSubscriptions)
	{
		auto it = game::gpGame->mCoordFrames.find(rSub.coord);
		if (it == game::gpGame->mCoordFrames.end())
		{
			continue;
		}
		engine::FrameStaticData& rStaticData = it->second.staticData;
		if (!rStaticData.bNavDataBuilt && !rStaticData.islands.empty())
		{
			// Heap: BuildCellNavData grows the navData vertex, polygon, and visibility-edge vectors, and
			// this path runs on the main thread inside the armed main loop
			ScopedSuppressAllocationTracking suppress;
			engine::BuildCellNavData(rStaticData.navData, rStaticData.islands);
			rStaticData.bNavDataBuilt = true;
		}
	}
}

void ServerSessionRuntime::HandleResyncRequests()
{
	std::vector<int64_t>& rResyncClientIds = mpServer->mPendingResyncClientIds;
	if (rResyncClientIds.empty())
	{
		return;
	}

	// Heap: per-resync per-slot SendCoordFullState allocates serialization buffers
	ScopedSuppressAllocationTracking suppress;

	for (int64_t iClientId : rResyncClientIds)
	{
		engine::ClientConnection* pClient = engine::gpServer->FindClient(iClientId);
		if (pClient == nullptr)
		{
			continue;
		}

		LOG(kNetwork, kWarning, "ServerSessionRuntime::HandleResyncRequests Client: {}", iClientId);

		for (int64_t iSlot = 0; iSlot < std::ssize(pClient->slots); ++iSlot)
		{
			if (!(pClient->slots.at(iSlot).subscription.flags & engine::SubscriptionFlags::kActive))
			{
				continue;
			}

			engine::GridCoord coord = pClient->slots.at(iSlot).subscription.coord;
			auto frameIt = game::gpGame->mCoordFrames.find(coord);
			if (frameIt == game::gpGame->mCoordFrames.end())
			{
				continue;
			}

			mpServer->SendCoordFullState(iClientId, iSlot, game::gpGame->TickCounter(), coord, frameIt->second.pCurrent.get());
		}
	}

	// Persist-until-served: Server::Poll leaves this runtime-owned queue intact. Clear it after servicing so a
	// request received during a paused or otherwise zero-tick update (iFullTicks == 0) survives across polls until
	// ServerSessionRuntime::CompleteTick or the current zero-tick ServerSessionRuntime::CompleteUpdate services it.
	rResyncClientIds.clear();
}

void ServerSessionRuntime::SendNewSubscriptionFullStates()
{
	std::vector<engine::PendingNewSubscription>& rNewSubscriptions = mpServer->mPendingNewSubscriptions;
	for (const engine::PendingNewSubscription& rSubscription : rNewSubscriptions)
	{
		const engine::ClientConnection* pClient = engine::gpServer->FindClient(rSubscription.iClientId);
		bool bSlotStillValid = (pClient != nullptr
			&& rSubscription.iSlot < std::ssize(pClient->slots)
			&& (pClient->slots.at(rSubscription.iSlot).subscription.flags & engine::SubscriptionFlags::kActive)
			&& pClient->slots.at(rSubscription.iSlot).subscription.coord == rSubscription.coord);
		if (!bSlotStillValid)
		{
			continue;
		}

		auto it = game::gpGame->mCoordFrames.find(rSubscription.coord);
		if (it != game::gpGame->mCoordFrames.end())
		{
			mpServer->SendCoordStaticData(rSubscription.iClientId, rSubscription.iSlot, rSubscription.coord, it->second.staticData);
			mpServer->SendCoordFullState(rSubscription.iClientId, rSubscription.iSlot, game::gpGame->TickCounter(), rSubscription.coord, it->second.pCurrent.get());
		}
	}

	// Persist-until-served: Server::Poll leaves this runtime-owned queue intact, so clear it here once serviced.
	// A subscription accepted during a paused or otherwise zero-tick update (iFullTicks == 0) stays queued across
	// polls until ServerSessionRuntime services it — either after the next tick in CompleteTick or during the
	// current zero-tick update in CompleteUpdate — and its static data and full state are sent.
	rNewSubscriptions.clear();
}

void ServerSessionRuntime::CompleteTick(int64_t iTick)
{
	// Heap: client/resync/subscription bookkeeping and ENet sends outside publication construction
	ScopedSuppressAllocationTracking suppress;
	HandleResyncRequests();
	mrSession.FinalizeTickClients();
	{
		ScopedResumeAllocationTracking resume;
		common::ScopedWorkbufferArena publicationArena = common::gpThreadLocal->mWorkbuffer.Push();
		mrSession.mpBroadcaster->BuildTickPublication(iTick, *this, publicationArena);
	}
	mrSession.mpBroadcaster->ClearBroadcastStatusChanges();
	mrSession.SubscriptionUpdates();
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
			mpServer->SendResends(rClient, iTick);
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
	mpServer->ClearBufferedFrames();
	mpServer->mPendingNewSubscriptions.clear();
	mpServer->mPendingResyncClientIds.clear();
	mpServer->Flush();
}

void ServerSessionRuntime::PublishTick(int64_t iTick, const std::pair<GridCoord, GridUpdateData>* pGridUpdates, int64_t iGridUpdateCount, const std::pair<GridCoord, const game::Frame*>* pFullFrames, int64_t iFullFrameCount)
{
	mpServer->BufferFrame(iTick, pGridUpdates, iGridUpdateCount);
	if (iFullFrameCount > 0)
	{
		mpServer->BufferFullFrame(iTick, pFullFrames, iFullFrameCount);
	}
	for (ClientConnection& rClient : mpServer->mClients)
	{
		mpServer->SendUpdate(rClient, iTick);
	}
}

} // namespace engine

#endif // BT_SERVER
