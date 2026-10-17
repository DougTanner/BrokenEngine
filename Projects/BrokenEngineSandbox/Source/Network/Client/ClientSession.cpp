#include "Network/Client/ClientSession.h"

#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Spaceships/Spaceships.h"
#include "Network/Client/ClientReconciler.h"
#include "Network/GamePacketType.h"
#include "Network/OwnedEntityEvents.h"
#include "Profile/ProfileRows.h"
#include "Fleet.h"
#include "Game.h"

namespace game
{

#if defined(BT_CLIENT)

const char* ToString(SubscriptionChangeReason eReason)
{
	switch (eReason)
	{
		case SubscriptionChangeReason::kAssigned:       return "kAssigned";
		case SubscriptionChangeReason::kSpawned:        return "kSpawned";
		case SubscriptionChangeReason::kChangedCell:    return "kChangedCell";
		case SubscriptionChangeReason::kDied:           return "kDied";
		case SubscriptionChangeReason::kFleetSynchronization:      return "kFleetSync";
		case SubscriptionChangeReason::kPollTick:       return "kPollTick";
		case SubscriptionChangeReason::kFocusNextFleet: return "kFocusNextFleet";
		case SubscriptionChangeReason::kFocusPreviousFleet: return "kFocusPrevFleet";
		case SubscriptionChangeReason::kSelectOwnedEntity: return "kSelectOwnedEntity";
	}
	return "Unknown";
}

ClientSession::ClientSession()
: common::Singleton<ClientSession>(gpClientSession, this)
{
	mpDesynchronizationCore = std::make_unique<engine::ClientDesyncCore>();
	mpReconciler = std::make_unique<ClientReconciler>();
	mpRuntime = std::make_unique<engine::ClientSessionRuntime>();
}

ClientSession::~ClientSession()
{
	mpRuntime.reset();
}

void ClientSession::ProcessReceivedGamePackets()
{

	try
	{
		common::ScopedWorkbufferArena ownedEntityEventsArena = common::gpThreadLocal->mWorkbuffer.Push();
		ParseOwnedEntityEvents(mpRuntime->mpClient->mReceivedGamePackets, ownedEntityEventsArena);
		const ReceivedOwnedEntityEvent* pOwnedEntityEvents = ownedEntityEventsArena.mBuffer.Data<ReceivedOwnedEntityEvent>();
		int64_t iOwnedEntityEventCount = ownedEntityEventsArena.mBuffer.Count<ReceivedOwnedEntityEvent>();
		for (const ReceivedOwnedEntityEvent& rOwnedEntityEvent : std::span<const ReceivedOwnedEntityEvent>(pOwnedEntityEvents, static_cast<size_t>(iOwnedEntityEventCount)))
		{
			ApplyOwnedEntityEvent(rOwnedEntityEvent);
		}
	}
	catch (const std::ios_base::failure& rException)
	{
		// The client trusts its server, so a shared reader throwing this type means server bytes the client cannot
		// decode, and it cannot keep playing against them — assert so the process ends with a crash report naming the
		// reader. The std::exception catch below is log-and-continue, so an ordinary local failure is never blamed on
		// the peer.
		LOG(kNetwork, kError, "ClientSession::ProcessReceivedGamePackets dropped corrupt owned-entity event packet: {}", rException.what());
		ASSERT(false);
	}
	catch (const std::exception& rException)
	{
		LOG(kNetwork, kWarning, "ClientSession::ProcessReceivedGamePackets failed processing owned-entity events: {}", rException.what());
	}

	try
	{
		std::vector<Fleet> receivedFleets;
		if (ParseFleetSynchronization(mpRuntime->mpClient->mReceivedGamePackets, receivedFleets))
		{
			engine::GridCoord previousFleetCoordinate = gpGame->mClientGridCoordinate;
			gpGame->mFleetSelection.SynchronizeFleets(std::move(receivedFleets));
			if (gpGame->mClientGridCoordinate != previousFleetCoordinate)
			{
				mpRuntime->UpdateDesiredCoordinates(ToString(SubscriptionChangeReason::kFleetSynchronization));
			}
		}
	}
	catch (const std::ios_base::failure& rException)
	{
		LOG(kNetwork, kError, "ClientSession::ProcessReceivedGamePackets dropped corrupt packet (type {}): {}", static_cast<uint8_t>(GamePacketType::kServerFleetSync), rException.what());
		ASSERT(false);
	}
	catch (const std::exception& rException)
	{
		LOG(kNetwork, kWarning, "ClientSession::ProcessReceivedGamePackets failed processing fleet sync: {}", rException.what());
	}

}

void ClientSession::ApplyOwnedEntityEvent(const ReceivedOwnedEntityEvent& rEvent)
{
	switch (rEvent.eType)
	{
		case OwnedEntityEventType::kAssigned:
			if (!((rEvent.globalId.iValue != 0) && std::ranges::contains(gpGame->mClientOwnedEntityIdentifiers, rEvent.globalId)))
			{
				LOG(kNetwork, kVerbose, "OwnedEntityEvent kAssigned NewGlobalId: {} NewCoord: ({},{}) FocusedGlobalId: {} FocusedCoord: ({},{})", rEvent.globalId, rEvent.coordinate.iX, rEvent.coordinate.iY, gpGame->ClientOwnedEntityIdentifier(), gpGame->mClientGridCoordinate.iX, gpGame->mClientGridCoordinate.iY);
				gpGame->AddClientOwnedEntity(rEvent.globalId, rEvent.coordinate);
			}
			mpRuntime->UpdateDesiredCoordinates(ToString(SubscriptionChangeReason::kAssigned));
			break;
		case OwnedEntityEventType::kSpawned:
			UpdateOwnedEntityCoordinate(rEvent.globalId, rEvent.coordinate);
			if (rEvent.globalId == gpGame->ClientOwnedEntityIdentifier())
			{
				gpGame->SetClientGridCoordinate(rEvent.coordinate);
			}
			mpRuntime->UpdateDesiredCoordinates(ToString(SubscriptionChangeReason::kSpawned));
			break;
		case OwnedEntityEventType::kChangedCell:
			UpdateOwnedEntityCoordinate(rEvent.globalId, rEvent.coordinate);
			if (rEvent.globalId == gpGame->ClientOwnedEntityIdentifier())
			{
				gpGame->SetClientGridCoordinate(rEvent.coordinate);
			}
			mpRuntime->UpdateDesiredCoordinates(ToString(SubscriptionChangeReason::kChangedCell));
			break;
		case OwnedEntityEventType::kDied:
			gpGame->RemoveClientOwnedEntity(rEvent.globalId);
			// A respawn keeps this ID and can come back in the weapon mode it had before a dropped request, which would never clear that pending toggle.
			if (rEvent.globalId == gpGame->mFleetSelection.mFocusedMemberGlobalId)
			{
				gpGame->mWeaponModeToggle.Reset();
			}
			mpRuntime->UpdateDesiredCoordinates(ToString(SubscriptionChangeReason::kDied));
			break;
	}
}

void ClientSession::UpdateOwnedEntityCoordinate(engine::GlobalId globalId, engine::GridCoord coordinate)
{
	for (int64_t i = 0; i < std::ssize(gpGame->mClientOwnedEntityIdentifiers); ++i)
	{
		if (gpGame->mClientOwnedEntityIdentifiers.at(i) == globalId)
		{
			gpGame->mClientOwnedEntityCoordinates.at(i) = coordinate;
			break;
		}
	}
}

void ClientSession::Reconcile()
{
	if (mpDesynchronizationCore->IsStalled())
	{
		return;
	}

	gpProfileManager->CpuStart(engine::kCpuTimerNetworkPollReconcile);
	{
		// Heap: reconciliation deserialization and map operations
		ScopedSuppressAllocationTracking suppress;
		int64_t iCurrentTick = gpGame->miTickCounter;
		if (engine::gpClient != nullptr)
		{
			engine::ReconcileDesyncInfo desynchronizationInformation = mpReconciler->Run();
			if (desynchronizationInformation.bDesync)
			{
				mpDesynchronizationCore->OnDesyncDetected(std::move(desynchronizationInformation));
			}
		}
		mpRuntime->ApplyClockCorrection(iCurrentTick);
	}
	gpProfileManager->CpuStop(engine::kCpuTimerNetworkPollReconcile, engine::CpuStopFlags::kSmoothNow);
}

void ClientSession::ConnectToServer(std::string_view serverAddress)
{
	gpGame->mModalMessage[0] = '\0';
	mpRuntime->Connect(serverAddress, engine::kiDefaultPort, NetworkSessionContract::kiCoordinateSlots);
}

void ClientSession::OnConnectionRejected(std::string_view reason)
{
	std::snprintf(gpGame->mModalMessage, sizeof(gpGame->mModalMessage), "%.*s", static_cast<int>(reason.size()), reason.data());
	gpGame->meUiState = engine::UiState::kModal;
}

void ClientSession::OnConnectionFailed()
{
	std::snprintf(gpGame->mModalMessage, sizeof(gpGame->mModalMessage), "Connection failed");
	gpGame->meUiState = engine::UiState::kModal;
}

void ClientSession::OnConnectionAccepted()
{
	if ((gpGame->mGameFlags & engine::GameFlags::kMainMenu))
	{
		gpGame->StartGameMusic();
		gpGame->mGameFlags.Set(engine::GameFlags::kMainMenu, false);
		gpGame->CreateNewFrame();
		gpGame->ResetSession();
		gpGame->meUiState = engine::UiState::kNone;
	}
}

void ClientSession::PollDesynchronizationState()
{
	mpDesynchronizationCore->PollDebugFrameResponse();
	mpDesynchronizationCore->PollDesyncTimeout();
}

void ClientSession::OnConnectionLost()
{
	gpGame->ChangeFrameToMainMenu();
	if (gpGame->mModalMessage[0] == '\0')
	{
		std::snprintf(gpGame->mModalMessage, sizeof(gpGame->mModalMessage), "Connection lost");
	}
	gpGame->meUiState = engine::UiState::kModal;
}

void ClientSession::OnServerLoad()
{
	LOG(kDefault, kDebug, "ClientSession::OnServerLoad");

	// Reset tick counter and time step — server tick resets to the saved value
	gpGame->miTickCounter = 0;
	gpGame->mTimeStep.mTickRemainderNanoseconds = 0ns;
	gpGame->mTimeStep.mRealTime.Reset();
	gpGame->ResetRenderClock();

	// Clear owned-entity identity — server will reassign
	gpGame->mClientOwnedEntityIdentifiers.clear();
	gpGame->mClientOwnedEntityCoordinates.clear();
	gpGame->SetClientGridCoordinate({});
	gpGame->mfPreviousClientArmor = 0.0f;
	engine::gpCamera->mVecVisualErrorOffset = {};
	gpGame->mWeaponModeToggle.Reset();

	// Clear fleet state — server will re-sync
	gpGame->mFleetSelection.Clear();

	// Discard cells from the previous server state.
	for (auto& [rCoordinate, rCell] : gpGame->mCells)
	{
		rCell.ResetClientState();
	}
	gpGame->mCells.clear();

	mpReconciler->Reset();
	mpDesynchronizationCore->Reset();
}

void ClientSession::OnRuntimeDisconnected()
{
	mpReconciler->Reset();
	for (auto& [rCoordinate, rCell] : gpGame->mCells)
	{
		rCell.ResetClientState();
	}
	mpDesynchronizationCore->Reset();
}

void ClientSession::SendUpdateOwnedEntityRequest(int64_t iGlobalId, bool bUseMissiles, float fNavigationDelay)
{
	mpRuntime->SendGameRequest(GamePacketType::kClientUpdateOwnedEntityRequest, [&]
	{
		LOG(kNetwork, kVerbose, "ClientSession::SendUpdateOwnedEntityRequest GlobalId: {} Missiles: {} NavDelay: {}", iGlobalId, bUseMissiles, common::Wb(fNavigationDelay, 3));
	}, iGlobalId, static_cast<uint8_t>(bUseMissiles ? 1 : 0), fNavigationDelay);
}

void ClientSession::SendCreateFleetRequest()
{
	mpRuntime->SendGameRequest(GamePacketType::kClientCreateFleetRequest, []
	{
		LOG(kNetwork, kDebug, "ClientSession::SendCreateFleetRequest");
	});
}

void ClientSession::SendDeleteFleetRequest(const FleetGuid& rFleetGuid)
{
	mpRuntime->SendGameRequest(GamePacketType::kClientDeleteFleetRequest, [&]
	{
		LOG(kNetwork, kDebug, "ClientSession::SendDeleteFleetRequest Fleet: ({},{})", rFleetGuid.uiHigh, rFleetGuid.uiLow);
	}, rFleetGuid.uiHigh, rFleetGuid.uiLow);
}

void ClientSession::SendSpawnIntoFleetRequest(const FleetGuid& rFleetGuid)
{
	mpRuntime->SendGameRequest(GamePacketType::kClientSpawnIntoFleetRequest, [&]
	{
		LOG(kNetwork, kDebug, "ClientSession::SendSpawnIntoFleetRequest Fleet: ({},{})", rFleetGuid.uiHigh, rFleetGuid.uiLow);
	}, rFleetGuid.uiHigh, rFleetGuid.uiLow);
}

void ClientSession::SendRespawnInFleetRequest(const FleetGuid& rFleetGuid, engine::GlobalId memberGlobalId)
{
	mpRuntime->SendGameRequest(GamePacketType::kClientRespawnInFleetRequest, [&]
	{
		LOG(kNetwork, kDebug, "ClientSession::SendRespawnInFleetRequest Fleet: ({},{}) Member: {}", rFleetGuid.uiHigh, rFleetGuid.uiLow, memberGlobalId.iValue);
	}, rFleetGuid.uiHigh, rFleetGuid.uiLow, memberGlobalId.iValue);
}

void ClientSession::SendFleetNavigationDelayRequest(const FleetGuid& rFleetGuid, float fDelay)
{
	mpRuntime->SendGameRequest(GamePacketType::kClientFleetNavigationDelay, [&]
	{
		LOG(kNetwork, kDebug, "ClientSession::SendFleetNavigationDelayRequest Fleet: ({},{}) Delay: {}", rFleetGuid.uiHigh, rFleetGuid.uiLow, common::Wb(fDelay, 3));
	}, rFleetGuid.uiHigh, rFleetGuid.uiLow, fDelay);
}

void ClientSession::ApplyReceivedStaticData()
{
	// Heap: try_emplace may insert new Cell, NavData vectors moved into staticData
	ScopedSuppressAllocationTracking suppress;

	std::vector<engine::ReceivedStaticData>& rStaticDataList = mpRuntime->mpClient->mReceivedStaticData;
	for (engine::ReceivedStaticData& rReceived : rStaticDataList)
	{
		engine::Cell& rCell = gpGame->mCells.try_emplace(rReceived.coordinate).first->second;
		rCell.staticData = std::move(rReceived.staticData);
		rCell.staticData.coordinate = rReceived.coordinate;

		// Subscription-driven island texture loading. AcquireTextureSlot is idempotent; duplicate
		// CRCs across placements short-circuit on the hot path. Slot mint + chunk-load request
		// happens here so the data is in-flight before UpdateActiveIslands references the slot.
		for (const engine::IslandPlacement& rPlacement : rCell.staticData.islands)
		{
			engine::gpIslandTerrainResidency->AcquireTextureSlot(rPlacement.islandCrc);
		}
	}
}

void ClientSession::HydrateReceivedFullState(Frame& rReceived, const Frame* pRingTail)
{
	BlastersInterpolate::ClientInitializeAll(rReceived);
	MissilesInterpolate::ClientInitializeAll(rReceived);
	SpaceshipsInterpolate::ClientInitializeAll(rReceived);

	// Copy smoke trail smoothed positions from the most recent ring frame to preserve
	// rendering continuity across reconciliation.
	if (pRingTail != nullptr)
	{
		const engine::SmokeTrailsInterpolate& rOldSmokeTrails = pRingTail->interpolate.smokeTrails;
		engine::SmokeTrailsInterpolate& rNewSmokeTrails = rReceived.interpolate.smokeTrails;
		int64_t iCopyCount = std::min(rOldSmokeTrails.iCount, rNewSmokeTrails.iCount);
		if (iCopyCount > 0)
		{
			std::memcpy(rNewSmokeTrails.pVecSmoothedPositions, rOldSmokeTrails.pVecSmoothedPositions, iCopyCount * sizeof(XMVECTOR));
		}
	}
}

void ClientSession::ResetCoordinateStatesForResynchronization()
{
	for (auto& [rCoordinate, rCell] : gpGame->mCells)
	{
		rCell.ResetClientState();
	}

	mpReconciler->Reset();
	mpRuntime->mUnwantedTimestamps.clear();
}

#endif // BT_CLIENT

} // namespace game
