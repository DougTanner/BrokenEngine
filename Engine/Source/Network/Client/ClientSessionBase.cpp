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
	for (ReceivedStaticData& rReceived : mpRuntime->mpClient->mReceivedStaticData)
	{
		mpRuntime->StageStaticData(std::move(rReceived));
	}
	mpRuntime->mpClient->mReceivedStaticData.clear();
}

void ClientSessionBase::ResetCoordinateStatesForResynchronization()
{
	mpRuntime->ResetPendingFrames();
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

	// Discard cells, the camera tracking, smoke, and particles from the previous server state.
	for ([[maybe_unused]] auto& [rCoordinate, rCell] : game::gpGame->mCells)
	{
		rCell.ResetClientState();
	}
	game::gpGame->mCells.clear();
	gpCamera->DiscardTrackingCaches();
	gbSmokeClear = true;
	gpParticleManager->mbReset = true;

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
