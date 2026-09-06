#include "Network/Client/ClientSession.h"

#include "Fleet.h"
#include "Game.h"
#include "Network/GamePacketType.h"
#include "Network/PlayerEvents.h"
#include "Profile/ProfileManager.h"

namespace game
{

#if defined(BT_CLIENT)

const char* ToString(SubscriptionChangeReason eReason)
{
	switch (eReason)
	{
		case SubscriptionChangeReason::kAssigned:       return "kAssigned";
		case SubscriptionChangeReason::kSpawned:        return "kSpawned";
		case SubscriptionChangeReason::kChangedFrame:   return "kChangedFrame";
		case SubscriptionChangeReason::kDied:           return "kDied";
		case SubscriptionChangeReason::kFleetSync:      return "kFleetSync";
		case SubscriptionChangeReason::kPollTick:       return "kPollTick";
		case SubscriptionChangeReason::kFocusNextFleet: return "kFocusNextFleet";
		case SubscriptionChangeReason::kFocusPrevFleet: return "kFocusPrevFleet";
		case SubscriptionChangeReason::kSelectPlayer:   return "kSelectPlayer";
	}
	return "Unknown";
}

ClientSession::ClientSession()
{
	ASSERT(gpClientSession == nullptr);

	gpClientSession = this;
	mpDesyncCore = std::make_unique<engine::ClientDesyncCore>();
	mpReconciler = std::make_unique<ClientReconciler>();
	mpRuntime = std::make_unique<engine::ClientSessionRuntime>(*this);
}

ClientSession::~ClientSession()
{
	mpRuntime.reset();
	if (gpClientSession == this)
	{
		gpClientSession = nullptr;
	}
}

void ClientSession::ProcessReceivedGamePackets()
{

	// Parse and process player events from raw game packets
	try
	{
		common::ScopedWorkbufferArena playerEventsArena = common::gpThreadLocal->mWorkbuffer.Push();
		ParsePlayerEvents(mpRuntime->mpClient->mReceivedGamePackets, playerEventsArena);
		const ReceivedPlayerEvent* pPlayerEvents = playerEventsArena.Data<ReceivedPlayerEvent>();
		int64_t iPlayerEventCount = playerEventsArena.Count<ReceivedPlayerEvent>();
		for (int64_t i = 0; i < iPlayerEventCount; ++i)
		{
			ApplyPlayerEvent(pPlayerEvents[i]);
		}
	}
	catch (const common::CorruptStreamException& rException)
	{
		// Trust boundary: only a reader that decided the server's bytes are impossible throws this type, and a client
		// cannot keep playing against a server it cannot decode — assert so the process ends with a crash report naming
		// the reader. The std::exception catch below is log-and-continue, so an ordinary local failure is never blamed
		// on the peer. ParsePlayerEvents decodes two packet types, so what() carries which one failed.
		LOG(kNetwork, kError, "ClientSession::ProcessReceivedGamePackets dropped corrupt player-event packet: {}", rException.what());
		ASSERT(false);
	}
	catch (const std::exception& rException)
	{
		LOG(kNetwork, kWarning, "ClientSession::ProcessReceivedGamePackets failed processing player events: {}", rException.what());
	}

	// Parse fleet sync from remaining game packets
	try
	{
		std::vector<Fleet> receivedFleets;
		if (ParseFleetSync(mpRuntime->mpClient->mReceivedGamePackets, receivedFleets))
		{
			engine::GridCoord preFleetCoord = gpGame->mClientGridCoord;
			gpGame->SyncFleets(std::move(receivedFleets));
			if (gpGame->mClientGridCoord != preFleetCoord)
			{
				UpdateDesiredCoords(SubscriptionChangeReason::kFleetSync);
			}
		}
	}
	catch (const common::CorruptStreamException& rException)
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
			if (!gpGame->IsClientPlayer(rEvent.globalPlayerId))
			{
				LOG(kNetwork, kVerbose, "PlayerEvent kAssigned NewGlobalPlayerId: {} NewCoord: ({},{}) FocusedGlobalPlayerId: {} FocusedCoord: ({},{})", rEvent.globalPlayerId, rEvent.coord.x, rEvent.coord.y, gpGame->ClientPlayerId(), gpGame->mClientGridCoord.x, gpGame->mClientGridCoord.y);
				gpGame->AddClientPlayer(rEvent.globalPlayerId, rEvent.coord);
			}
			UpdateDesiredCoords(SubscriptionChangeReason::kAssigned);
			break;
		case PlayerEventType::kSpawned:
			UpdatePlayerCoord(rEvent.globalPlayerId, rEvent.coord);
			if (rEvent.globalPlayerId == gpGame->ClientPlayerId())
			{
				gpGame->SetClientGridCoord(rEvent.coord);
			}
			UpdateDesiredCoords(SubscriptionChangeReason::kSpawned);
			break;
		case PlayerEventType::kChangedFrame:
			UpdatePlayerCoord(rEvent.globalPlayerId, rEvent.coord);
			if (rEvent.globalPlayerId == gpGame->ClientPlayerId())
			{
				gpGame->SetClientGridCoord(rEvent.coord);
			}
			UpdateDesiredCoords(SubscriptionChangeReason::kChangedFrame);
			break;
		case PlayerEventType::kDied:
			gpGame->RemoveClientPlayer(rEvent.globalPlayerId);
			UpdateDesiredCoords(SubscriptionChangeReason::kDied);
			break;
	}
}

void ClientSession::UpdatePlayerCoord(engine::global_id_t globalPlayerId, engine::GridCoord coord)
{
	for (int64_t i = 0; i < gpGame->PlayerCount(); ++i)
	{
		if (gpGame->mClientPlayerIds.at(i) == globalPlayerId)
		{
			gpGame->mClientPlayerCoords.at(i) = coord;
			break;
		}
	}
}

void ClientSession::Reconcile()
{
	if (mpDesyncCore->IsStalled())
	{
		return;
	}

	gpProfileManager->CpuStart(engine::kCpuTimerNetworkPollReconcile);
	{
		// Heap: reconciliation deserialization and map operations
		ScopedSuppressAllocationTracking suppress;
		int64_t iCurrentTick = gpGame->TickCounter();
		if (engine::gpClient != nullptr)
		{
			engine::ReconcileDesyncInfo desyncInfo = mpReconciler->Run();
			if (desyncInfo.bDesync)
			{
				mpDesyncCore->OnDesyncDetected(std::move(desyncInfo));
			}
		}
		mpRuntime->ApplyClockCorrection(iCurrentTick);
	}
	gpProfileManager->CpuStop(engine::kCpuTimerNetworkPollReconcile, engine::CpuStopFlags::kSmoothNow);
}

void ClientSession::ConnectToServer(std::string_view serverAddress)
{
	gpGame->mModalMessage[0] = '\0';
	mpRuntime->Connect(serverAddress, engine::kuiDefaultPort, NetworkSessionContract::kiCoordSlots);
}

void ClientSession::OnConnectionRejected(const char* pcReason)
{
	std::snprintf(gpGame->mModalMessage, sizeof(gpGame->mModalMessage), "%s", pcReason);
	gpGame->meUiState = engine::UiState::kModal;
}

void ClientSession::OnConnectionFailed()
{
	std::snprintf(gpGame->mModalMessage, sizeof(gpGame->mModalMessage), "Connection failed");
	gpGame->meUiState = engine::UiState::kModal;
}

void ClientSession::OnConnectionAccepted()
{
	if (gpGame->InMainMenu())
	{
		gpGame->StartGameMusic();
		gpGame->CreateNewFrame(GameFlags::kGame);
		gpGame->mGameFlags.Clear(engine::GameFlags::kMainMenu);
		gpGame->Reset();
		gpGame->meUiState = engine::UiState::kNone;
	}
}

void ClientSession::PollDesyncState()
{
	mpDesyncCore->PollDebugFrameResponse();
	mpDesyncCore->PollDesyncTimeout();
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
	gpGame->SetTickCounter(0);
	gpGame->mTimeStep.ClearAccumulator();
	gpGame->mTimeStep.mRealTime.Reset();
	gpGame->ResetRenderClock();

	// Clear player identity — server will reassign
	gpGame->mClientPlayerIds.clear();
	gpGame->mClientPlayerCoords.clear();
	gpGame->SetClientGridCoord({});
	gpGame->SetPreviousClientArmor(0.0f);
	gpGame->mVecVisualErrorOffset = {};
	gpGame->mWeaponModeToggle.Reset();
	gpGame->mNavigationDelayControl.Reset();

	// Clear fleet state — server will re-sync
	gpGame->mFleetSelection.Clear();

	// Clear local coord frames (stale pre-load data). Reset render-progress fields first
	// so that any entry re-emplaced by a racing packet in the same frame starts clean.
	for (auto& [rCoord, rCoordFrames] : gpGame->mCoordFrames)
	{
		rCoordFrames.ResetClientState();
	}
	gpGame->mCoordFrames.clear();

	// Reset game-owned reconciliation and desync state.
	mpReconciler->Reset();
	mpDesyncCore->Reset();
}

void ClientSession::OnRuntimeDisconnected()
{
	mpReconciler->Reset();
	for (auto& [rCoord, rFrames] : gpGame->mCoordFrames)
	{
		rFrames.ResetClientState();
	}
	mpDesyncCore->Reset();
}

void ClientSession::OnCoordReleased(engine::GridCoord coord)
{
	gpGame->mCoordFrames.erase(coord);
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

void ClientSession::SendRespawnInFleetRequest(const FleetGuid& rFleetGuid, int64_t iMemberIndex)
{
	mpRuntime->SendGameRequest(GamePacketType::kClientRespawnInFleetRequest, [&]
	{
		LOG(kNetwork, kDebug, "ClientSession::SendRespawnInFleetRequest Fleet: ({},{}) Member: {}", rFleetGuid.uiHigh, rFleetGuid.uiLow, iMemberIndex);
	}, rFleetGuid.uiHigh, rFleetGuid.uiLow, iMemberIndex);
}

void ClientSession::SendFleetNavigationDelayRequest(const FleetGuid& rFleetGuid, float fDelay)
{
	mpRuntime->SendGameRequest(GamePacketType::kClientFleetNavigationDelay, [&]
	{
		LOG(kNetwork, kDebug, "ClientSession::SendFleetNavigationDelayRequest Fleet: ({},{}) Delay: {}", rFleetGuid.uiHigh, rFleetGuid.uiLow, common::Wb(fDelay, 3));
	}, rFleetGuid.uiHigh, rFleetGuid.uiLow, fDelay);
}

#endif // BT_CLIENT

} // namespace game
