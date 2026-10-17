#include "Pch.h"

#include "Network/Client/ClientSessionBase.h"

#if defined(BT_CLIENT)

#include "Network/Client/ClientSessionRuntime.h"

#include "Network/Client/ClientReconciler.h"
#include "Network/Client/ClientSession.h"
#include "Game.h"

namespace engine
{

ClientSessionBase::ClientSessionBase()
{
	mpDesynchronizationCore = std::make_unique<ClientDesyncCore>();
	mpRuntime = std::make_unique<ClientSessionRuntime>();
}

ClientSessionBase::~ClientSessionBase() = default;

void ClientSessionBase::Reconcile()
{
	if (mpDesynchronizationCore->IsStalled())
	{
		return;
	}

	gpProfileManager->CpuStart(kCpuTimerNetworkPollReconcile);
	{
		// Heap: reconciliation deserialization and map operations
		ScopedSuppressAllocationTracking suppress;
		int64_t iCurrentTick = game::gpGame->miTickCounter;
		if (gpClient != nullptr)
		{
			ReconcileDesyncInfo desynchronizationInformation = game::gpClientSession->mpReconciler->Run();
			if (desynchronizationInformation.bDesync)
			{
				mpDesynchronizationCore->OnDesyncDetected(std::move(desynchronizationInformation));
			}
		}
		mpRuntime->ApplyClockCorrection(iCurrentTick);
	}
	gpProfileManager->CpuStop(kCpuTimerNetworkPollReconcile, CpuStopFlags::kSmoothNow);
}

void ClientSessionBase::ApplyReceivedStaticData()
{
	// Heap: try_emplace may insert new Cell, NavData vectors moved into staticData
	ScopedSuppressAllocationTracking suppress;

	std::vector<ReceivedStaticData>& rStaticDataList = mpRuntime->mpClient->mReceivedStaticData;
	for (ReceivedStaticData& rReceived : rStaticDataList)
	{
		Cell& rCell = game::gpGame->mCells.try_emplace(rReceived.coordinate).first->second;
		rCell.staticData = std::move(rReceived.staticData);
		rCell.staticData.coordinate = rReceived.coordinate;

		// Subscription-driven island texture loading. AcquireTextureSlot is idempotent; duplicate
		// CRCs across placements short-circuit on the hot path. Slot mint + chunk-load request
		// happens here so the data is in-flight before UpdateActiveIslands references the slot.
		for (const IslandPlacement& rPlacement : rCell.staticData.islands)
		{
			gpIslandTerrainResidency->AcquireTextureSlot(rPlacement.islandCrc);
		}
	}
}

void ClientSessionBase::ResetCoordinateStatesForResynchronization()
{
	for ([[maybe_unused]] auto& [rCoordinate, rCell] : game::gpGame->mCells)
	{
		rCell.ResetClientState();
	}

	game::gpClientSession->mpReconciler->Reset();
	mpRuntime->mUnwantedTimestamps.clear();
}

void ClientSessionBase::PollDesynchronizationState()
{
	mpDesynchronizationCore->PollDebugFrameResponse();
	mpDesynchronizationCore->PollDesyncTimeout();
}

void ClientSessionBase::OnRuntimeDisconnected()
{
	game::gpClientSession->mpReconciler->Reset();
	for ([[maybe_unused]] auto& [rCoordinate, rCell] : game::gpGame->mCells)
	{
		rCell.ResetClientState();
	}
	mpDesynchronizationCore->Reset();
}

void ClientSessionBase::ResetSessionForServerLoad()
{
	// Reset tick counter and time step — server tick resets to the saved value
	game::gpGame->miTickCounter = 0;
	game::gpGame->mTimeStep.mTickRemainderNanoseconds = 0ns;
	game::gpGame->mTimeStep.mRealTime.Reset();
	game::gpGame->ResetRenderClock();

	// Discard cells from the previous server state.
	for ([[maybe_unused]] auto& [rCoordinate, rCell] : game::gpGame->mCells)
	{
		rCell.ResetClientState();
	}
	game::gpGame->mCells.clear();

	game::gpClientSession->mpReconciler->Reset();
	mpDesynchronizationCore->Reset();

	game::gpClientSession->OnServerLoad();
}

void ClientSessionBase::ApplyOwnedEntityEvents()
{
	try
	{
		for (const ReceivedOwnedEntityEvent& rEvent : mpRuntime->mpClient->mReceivedOwnedEntityEvents)
		{
			game::gpClientSession->ApplyOwnedEntityEvent(rEvent);
		}
	}
	catch (const std::exception& rException)
	{
		LOG(kNetwork, kWarning, "ClientSessionBase::ApplyOwnedEntityEvents failed processing owned-entity events: {}", rException.what());
	}
}

} // namespace engine

#endif // BT_CLIENT
