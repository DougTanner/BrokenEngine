#include "Network/Client/ClientSession.h"

#include "Frame/Collections/Battleships/Battleships.h"
#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Fighters/Fighters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Network/Client/ClientReconciler.h"
#include "Network/GamePacketType.h"
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
	mpReconciler = std::make_unique<ClientReconciler>();
}

ClientSession::~ClientSession()
{
	mpRuntime.reset();
}

static bool ParseFleetSynchronization(std::vector<std::pair<int64_t, std::vector<uint8_t>>>& rRawPackets, std::vector<Fleet>& rOutputFleets)
{
	// A sync, including a zero-fleet sync, commits into rOutputFleets but leaves it empty when
	// fleetCount == 0, so emptiness cannot tell the caller "applied" from "nothing arrived" — report it explicitly
	bool bApplied = false;
	for (auto it = rRawPackets.begin(); it != rRawPackets.end(); )
	{
		GamePacketType eType = static_cast<GamePacketType>(it->first);
		if (eType != GamePacketType::kServerFleetSync)
		{
			++it;
			continue;
		}

		// Each sync replaces the whole list, so parse into a fresh vector: fields the payload does not carry
		// start at their defaults rather than inheriting an earlier sync's values
		std::vector<Fleet> parsedFleets;
		GameMessages::FleetSyncMessage::ReadPayload(it->second, parsedFleets);
		rOutputFleets = std::move(parsedFleets);
		bApplied = true;

		it = rRawPackets.erase(it);
	}

	return bApplied;
}

void ClientSession::ProcessReceivedGamePackets()
{
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
		// The client trusts its server, so a shared reader throwing this type means server bytes the client cannot
		// decode, and it cannot keep playing against them — assert so the process ends with a crash report naming the
		// reader. The std::exception catch below is log-and-continue, so an ordinary local failure is never blamed on
		// the peer.
		LOG(kNetwork, kError, "ClientSession::ProcessReceivedGamePackets dropped corrupt packet (type {}): {}", static_cast<uint8_t>(GamePacketType::kServerFleetSync), rException.what());
		ASSERT(false);
	}
	catch (const std::exception& rException)
	{
		LOG(kNetwork, kWarning, "ClientSession::ProcessReceivedGamePackets failed processing fleet sync: {}", rException.what());
	}

}

void ClientSession::ApplyOwnedEntityEvent(const engine::ReceivedOwnedEntityEvent& rEvent)
{
	if (rEvent.eState == engine::OwnedEntityState::kDied)
	{
		gpGame->RemoveClientOwnedEntity(rEvent.globalId);
		// A respawn keeps this ID and can come back in the weapon mode it had before a dropped request, which would never clear that pending toggle.
		if (rEvent.globalId == gpGame->mFleetSelection.mFocusedMemberGlobalId)
		{
			gpGame->mWeaponModeToggle.Reset();
		}
		mpRuntime->UpdateDesiredCoordinates(ToString(SubscriptionChangeReason::kDied));
		return;
	}

	// Spawned and ChangedCell each carry the assignment.
	if (!((rEvent.globalId.iValue != 0) && std::ranges::contains(gpGame->mClientOwnedEntityIdentifiers, rEvent.globalId)))
	{
		LOG(kNetwork, kVerbose, "OwnedEntityEvent kAssigned NewGlobalId: {} NewCoord: ({},{}) FocusedGlobalId: {} FocusedCoord: ({},{})", rEvent.globalId, rEvent.coordinate.iQ, rEvent.coordinate.iR, gpGame->ClientOwnedEntityIdentifier(), gpGame->mClientGridCoordinate.iQ, gpGame->mClientGridCoordinate.iR);
		gpGame->AddClientOwnedEntity(rEvent.globalId, rEvent.coordinate);
	}
	mpRuntime->UpdateDesiredCoordinates(ToString(SubscriptionChangeReason::kAssigned));

	UpdateOwnedEntityCoordinate(rEvent.globalId, rEvent.coordinate);
	// Not at the fleet sync that clears the member's kIsDead: the server sends it while the spawn is still queued, before
	// the row exists.
	if (rEvent.eState == engine::OwnedEntityState::kSpawned && rEvent.globalId == gpGame->mFleetSelection.mPendingFocusMemberGlobalId)
	{
		gpGame->mFleetSelection.SelectOwnedEntityInFleet(rEvent.globalId);
	}
	if (rEvent.globalId == gpGame->ClientOwnedEntityIdentifier())
	{
		gpGame->SetClientGridCoordinate(rEvent.coordinate);
	}
	mpRuntime->UpdateDesiredCoordinates(ToString(rEvent.eState == engine::OwnedEntityState::kSpawned ? SubscriptionChangeReason::kSpawned : SubscriptionChangeReason::kChangedCell));
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

void ClientSession::ConnectToServer(std::string_view serverAddress)
{
	gpGame->mpcModalMessage[0] = '\0';
	mpRuntime->Connect(serverAddress, engine::kiDefaultPort);
}

void ClientSession::OnConnectionRejected(std::string_view reason)
{
	std::snprintf(gpGame->mpcModalMessage, sizeof(gpGame->mpcModalMessage), "%.*s", static_cast<int>(reason.size()), reason.data());
	gpGame->meUiState = engine::UiState::kModal;
}

void ClientSession::OnConnectionFailed()
{
	std::snprintf(gpGame->mpcModalMessage, sizeof(gpGame->mpcModalMessage), "Connection failed");
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

void ClientSession::OnConnectionLost()
{
	gpGame->ChangeFrameToMainMenu();
	if (gpGame->mpcModalMessage[0] == '\0')
	{
		std::snprintf(gpGame->mpcModalMessage, sizeof(gpGame->mpcModalMessage), "Connection lost");
	}
	gpGame->meUiState = engine::UiState::kModal;
}

void ClientSession::OnServerLoad()
{
	LOG(kDefault, kDebug, "ClientSession::OnServerLoad");

	// Clear owned-entity identity — server will reassign
	gpGame->mClientOwnedEntityIdentifiers.clear();
	gpGame->mClientOwnedEntityCoordinates.clear();
	gpGame->SetClientGridCoordinate({});
	gpGame->mfPreviousClientArmor = 0.0f;
	engine::gpCamera->mVecVisualErrorOffset = {};
	gpGame->mWeaponModeToggle.Reset();

	// Clear fleet state — server will re-sync
	gpGame->mFleetSelection.Clear();
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

void ClientSession::SendSpawnIntoFleetRequest(const FleetGuid& rFleetGuid, ShipType eType)
{
	mpRuntime->SendGameRequest(GamePacketType::kClientSpawnIntoFleetRequest, [&]
	{
		LOG(kNetwork, kDebug, "ClientSession::SendSpawnIntoFleetRequest Fleet: ({},{}) Type: {}", rFleetGuid.uiHigh, rFleetGuid.uiLow, ShipTypeName(eType));
	}, rFleetGuid.uiHigh, rFleetGuid.uiLow, std::to_underlying(eType));
}

void ClientSession::SendFleetNavigationDelayRequest(const FleetGuid& rFleetGuid, float fDelay)
{
	mpRuntime->SendGameRequest(GamePacketType::kClientFleetNavigationDelay, [&]
	{
		LOG(kNetwork, kDebug, "ClientSession::SendFleetNavigationDelayRequest Fleet: ({},{}) Delay: {}", rFleetGuid.uiHigh, rFleetGuid.uiLow, common::Wb(fDelay, 3));
	}, rFleetGuid.uiHigh, rFleetGuid.uiLow, fDelay);
}

void ClientSession::HydrateReceivedFullState(Frame& rReceived, const Frame* pRingTail)
{
	BlastersInterpolate::ClientInitializeAll(rReceived);
	MissilesInterpolate::ClientInitializeAll(rReceived);
	FightersInterpolate::ClientInitializeAll(rReceived);
	BattleshipsInterpolate::ClientInitializeAll(rReceived);

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

#endif // BT_CLIENT

} // namespace game
