#include "Network/Client/ClientSession.h"

#include "Agent/Commands/ClientFullStateHarnessRig.h"
#include "Agent/Commands/ClientPacketFaultHarnessRig.h"
#include "Agent/Commands/ClientSubscriptionHarnessRigs.h"
#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Spaceships/Spaceships.h"
#include "Network/GamePacketType.h"
#include "Network/PlayerEvents.h"
#include "Profile/ProfileManager.h"
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
		case SubscriptionChangeReason::kSelectPlayer:   return "kSelectPlayer";
	}
	return "Unknown";
}

ClientSession::ClientSession()
{
	ASSERT(gpClientSession == nullptr);

	gpClientSession = this;
	mpDesynchronizationCore = std::make_unique<engine::ClientDesyncCore>();
	mpReconciler = std::make_unique<ClientReconciler>();
	mpRuntime = std::make_unique<engine::ClientSessionRuntime>(*this);
}

ClientSession::~ClientSession()
{
	ResetClientPacketFaultHarnessRig(*this);
	DetachClientFullStateHarnessRig(*this);
	DetachClientSubscriptionHarnessRigs(*this);
	mpRuntime.reset();
	if (gpClientSession == this)
	{
		gpClientSession = nullptr;
	}
}

void ClientSession::ProcessReceivedGamePackets()
{

	try
	{
		common::ScopedWorkbufferArena playerEventsArena = common::gpThreadLocal->mWorkbuffer.Push();
		ParsePlayerEvents(mpRuntime->mpClient->mReceivedGamePackets, playerEventsArena);
		const ReceivedPlayerEvent* pPlayerEvents = playerEventsArena.mBuffer.Data<ReceivedPlayerEvent>();
		int64_t iPlayerEventCount = playerEventsArena.mBuffer.Count<ReceivedPlayerEvent>();
		for (const ReceivedPlayerEvent& rPlayerEvent : std::span<const ReceivedPlayerEvent>(pPlayerEvents, static_cast<size_t>(iPlayerEventCount)))
		{
			ApplyPlayerEvent(rPlayerEvent);
		}
	}
	catch (const std::ios_base::failure& rException)
	{
		// The client trusts its server, so a shared reader throwing this type means server bytes the client cannot
		// decode, and it cannot keep playing against them — assert so the process ends with a crash report naming the
		// reader. The std::exception catch below is log-and-continue, so an ordinary local failure is never blamed on
		// the peer.
		LOG(kNetwork, kError, "ClientSession::ProcessReceivedGamePackets dropped corrupt player-event packet: {}", rException.what());
		ASSERT(false);
	}
	catch (const std::exception& rException)
	{
		LOG(kNetwork, kWarning, "ClientSession::ProcessReceivedGamePackets failed processing player events: {}", rException.what());
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
				UpdateDesiredCoordinates(SubscriptionChangeReason::kFleetSynchronization);
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

void ClientSession::ApplyPlayerEvent(const ReceivedPlayerEvent& rEvent)
{
	switch (rEvent.eType)
	{
		case PlayerEventType::kAssigned:
			if (!((rEvent.globalPlayerId.iValue != 0) && std::ranges::contains(gpGame->mClientPlayerIdentifiers, rEvent.globalPlayerId)))
			{
				LOG(kNetwork, kVerbose, "PlayerEvent kAssigned NewGlobalPlayerId: {} NewCoord: ({},{}) FocusedGlobalPlayerId: {} FocusedCoord: ({},{})", rEvent.globalPlayerId, rEvent.coordinate.iX, rEvent.coordinate.iY, gpGame->ClientPlayerIdentifier(), gpGame->mClientGridCoordinate.iX, gpGame->mClientGridCoordinate.iY);
				gpGame->AddClientPlayer(rEvent.globalPlayerId, rEvent.coordinate);
			}
			UpdateDesiredCoordinates(SubscriptionChangeReason::kAssigned);
			break;
		case PlayerEventType::kSpawned:
			UpdatePlayerCoordinate(rEvent.globalPlayerId, rEvent.coordinate);
			if (rEvent.globalPlayerId == gpGame->ClientPlayerIdentifier())
			{
				gpGame->SetClientGridCoordinate(rEvent.coordinate);
			}
			UpdateDesiredCoordinates(SubscriptionChangeReason::kSpawned);
			break;
		case PlayerEventType::kChangedCell:
			UpdatePlayerCoordinate(rEvent.globalPlayerId, rEvent.coordinate);
			if (rEvent.globalPlayerId == gpGame->ClientPlayerIdentifier())
			{
				gpGame->SetClientGridCoordinate(rEvent.coordinate);
			}
			UpdateDesiredCoordinates(SubscriptionChangeReason::kChangedCell);
			break;
		case PlayerEventType::kDied:
			gpGame->RemoveClientPlayer(rEvent.globalPlayerId);
			// A respawn keeps this ID and can come back in the weapon mode it had before a dropped request, which would never clear that pending toggle.
			if (rEvent.globalPlayerId == gpGame->mFleetSelection.mFocusedMemberGlobalId)
			{
				gpGame->mWeaponModeToggle.Reset();
			}
			UpdateDesiredCoordinates(SubscriptionChangeReason::kDied);
			break;
	}
}

void ClientSession::UpdatePlayerCoordinate(engine::GlobalId globalPlayerId, engine::GridCoord coordinate)
{
	for (int64_t i = 0; i < std::ssize(gpGame->mClientPlayerIdentifiers); ++i)
	{
		if (gpGame->mClientPlayerIdentifiers.at(i) == globalPlayerId)
		{
			gpGame->mClientPlayerCoordinates.at(i) = coordinate;
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
		gpGame->CreateNewFrame(GameFlags::kGame);
		gpGame->mGameFlags.Set(engine::GameFlags::kMainMenu, false);
		gpGame->Reset();
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
	gpGame->ChangeFrame(GameFlags::kMainMenu);
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
	static constexpr int64_t kiTickCounter = 0;
	static_assert(kiTickCounter >= 0);
	gpGame->miTickCounter = kiTickCounter;
	gpGame->mTimeStep.mTickRemainderNanoseconds = 0ns;
	gpGame->mTimeStep.mRealTime.Reset();
	gpGame->ResetRenderClock();

	// Clear player identity — server will reassign
	gpGame->mClientPlayerIdentifiers.clear();
	gpGame->mClientPlayerCoordinates.clear();
	gpGame->SetClientGridCoordinate({});
	gpGame->mfPreviousClientArmor = 0.0f;
	gpGame->mVecVisualErrorOffset = {};
	gpGame->mWeaponModeToggle.Reset();

	// Clear fleet state — server will re-sync
	gpGame->mFleetSelection.Clear();

	// Discard cells from the previous server state.
	for (auto& [rCoordinate, rCell] : gpGame->mCells)
	{
		rCell.ResetClientState();
	}
	gpGame->mCells.clear();

	ResetClientPacketFaultHarnessRig(*this);
	mpReconciler->Reset();
	mpDesynchronizationCore->Reset();
}

void ClientSession::OnRuntimeDisconnected()
{
	ResetClientPacketFaultHarnessRig(*this);
	mpReconciler->Reset();
	for (auto& [rCoordinate, rCell] : gpGame->mCells)
	{
		rCell.ResetClientState();
	}
	mpDesynchronizationCore->Reset();
}

void ClientSession::SendUpdatePlayerRequest(int64_t iGlobalPlayerId, bool bUseMissiles, float fNavigationDelay)
{
	mpRuntime->SendGameRequest(GamePacketType::kClientUpdatePlayerRequest, [&]
	{
		LOG(kNetwork, kVerbose, "ClientSession::SendUpdatePlayerRequest GlobalPlayer: {} Missiles: {} NavDelay: {}", iGlobalPlayerId, bUseMissiles, common::Wb(fNavigationDelay, 3));
	}, iGlobalPlayerId, static_cast<uint8_t>(bUseMissiles ? 1 : 0), fNavigationDelay);
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

void ClientSession::SendRespawnInFleetRequest(const FleetGuid& rFleetGuid, engine::GlobalId memberGlobalPlayerId)
{
	mpRuntime->SendGameRequest(GamePacketType::kClientRespawnInFleetRequest, [&]
	{
		LOG(kNetwork, kDebug, "ClientSession::SendRespawnInFleetRequest Fleet: ({},{}) Member: {}", rFleetGuid.uiHigh, rFleetGuid.uiLow, memberGlobalPlayerId.iValue);
	}, rFleetGuid.uiHigh, rFleetGuid.uiLow, memberGlobalPlayerId.iValue);
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
	MissilesInterpolate::ClientInitAll(rReceived);
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

void ClientSession::UpdateDesiredCoordinates(SubscriptionChangeReason eReason)
{
	std::optional<common::LogTickScope> optionalTickScope;
	if (common::gpThreadLocal->miLogTickCounter < 0)
	{
		optionalTickScope.emplace(gpGame->miTickCounter);
	}

	static constexpr int64_t kiMaximumDesiredCoordinates = 9;
	engine::GridCoord desiredCoordinates[kiMaximumDesiredCoordinates] {};
	int64_t iDesiredCount = 0;
	auto PushCoordinate = [&](engine::GridCoord coordinate)
	{
		ASSERT(iDesiredCount < kiMaximumDesiredCoordinates);
		desiredCoordinates[iDesiredCount++] = coordinate;
	};

	if ((gpGame->ClientPlayerIdentifier().iValue != 0))
	{
		PushCoordinate(gpGame->mClientGridCoordinate);
		for (const engine::GridCoord& rCoordinate : std::span<const engine::GridCoord>(gpGame->mVisibleNeighbors, static_cast<size_t>(gpGame->miVisibleNeighborCount)))
		{
			PushCoordinate(rCoordinate);
		}
	}
	else
	{
		PushCoordinate(engine::kOriginCoordinate);
	}

	mpRuntime->SetDesiredCoordinates(std::span<const engine::GridCoord>(desiredCoordinates, static_cast<size_t>(iDesiredCount)), ToString(eReason), gpGame->miTickCounter);
}

#endif // BT_CLIENT

} // namespace game
