#include "Network/Client/ClientSession.h"

#include "Agent/Commands/ClientFullStateFixture.h"
#include "Agent/Commands/ClientPacketFaultFixture.h"
#include "Agent/Commands/ClientSubscriptionFixtures.h"
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
	DetachClientPacketFaultFixture(*this);
	DetachClientFullStateFixture(*this);
	DetachClientSubscriptionFixtures(*this);
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
		const ReceivedPlayerEvent* pPlayerEvents = playerEventsArena.mBuffer.Data<ReceivedPlayerEvent>();
		int64_t iPlayerEventCount = playerEventsArena.mBuffer.Count<ReceivedPlayerEvent>();
		for (int64_t i = 0; i < iPlayerEventCount; ++i)
		{
			ApplyPlayerEvent(pPlayerEvents[i]);
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

	// Parse fleet sync from remaining game packets
	try
	{
		std::vector<Fleet> receivedFleets;
		if (ParseFleetSync(mpRuntime->mpClient->mReceivedGamePackets, receivedFleets))
		{
			engine::GridCoord preFleetCoord = gpGame->mClientGridCoord;
			gpGame->mFleetSelection.SyncFleets(std::move(receivedFleets));
			if (gpGame->mClientGridCoord != preFleetCoord)
			{
				UpdateDesiredCoords(SubscriptionChangeReason::kFleetSync);
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
			if (!gpGame->IsClientPlayer(rEvent.globalPlayerId))
			{
				LOG(kNetwork, kVerbose, "PlayerEvent kAssigned NewGlobalPlayerId: {} NewCoord: ({},{}) FocusedGlobalPlayerId: {} FocusedCoord: ({},{})", rEvent.globalPlayerId, rEvent.coord.iX, rEvent.coord.iY, gpGame->ClientPlayerId(), gpGame->mClientGridCoord.iX, gpGame->mClientGridCoord.iY);
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
			// A respawn keeps this ID and can come back in the weapon mode it had before a dropped request, which would never clear that pending toggle.
			if (rEvent.globalPlayerId == gpGame->mFleetSelection.mFocusedMemberGlobalId)
			{
				gpGame->mWeaponModeToggle.Reset();
			}
			UpdateDesiredCoords(SubscriptionChangeReason::kDied);
			break;
	}
}

void ClientSession::UpdatePlayerCoord(engine::GlobalId globalPlayerId, engine::GridCoord coord)
{
	for (int64_t i = 0; i < std::ssize(gpGame->mClientPlayerIds); ++i)
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
	if (gpGame->InMainMenu())
	{
		gpGame->StartGameMusic();
		gpGame->CreateNewFrame(GameFlags::kGame);
		gpGame->mGameFlags.Set(engine::GameFlags::kMainMenu, false);
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
	gpGame->mTimeStep.mTickRemainderNanoseconds = 0ns;
	gpGame->mTimeStep.mRealTime.Reset();
	gpGame->ResetRenderClock();

	// Clear player identity — server will reassign
	gpGame->mClientPlayerIds.clear();
	gpGame->mClientPlayerCoords.clear();
	gpGame->SetClientGridCoord({});
	gpGame->mfPreviousClientArmor = 0.0f;
	gpGame->mVecVisualErrorOffset = {};
	gpGame->mWeaponModeToggle.Reset();

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
	ResetClientPacketFaultFixture(*this);
	mpReconciler->Reset();
	mpDesyncCore->Reset();
}

void ClientSession::OnRuntimeDisconnected()
{
	ResetClientPacketFaultFixture(*this);
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

#endif // BT_CLIENT

} // namespace game
