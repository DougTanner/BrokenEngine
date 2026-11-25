#include "Network/Client/ClientSession.h"

#if defined(BT_CLIENT)

#include "Frame/Collections/Battleships/Battleships.h"
#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Fighters/Fighters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Network/Client/ClientReconciler.h"
#include "Network/GamePacketType.h"
#include "Fleet.h"
#include "Game.h"

namespace game
{

// Wall clock: the poll keeps its rate when the simulation is paused or time-scaled.
constexpr std::chrono::steady_clock::duration kMapFleetMarkersRequestInterval = 1s;

const char* ToString(SubscriptionChangeReason eReason)
{
	switch (eReason)
	{
		case SubscriptionChangeReason::kAssigned:       return "kAssigned";
		case SubscriptionChangeReason::kSpawned:        return "kSpawned";
		case SubscriptionChangeReason::kChangedCell:    return "kChangedCell";
		case SubscriptionChangeReason::kDied:           return "kDied";
		case SubscriptionChangeReason::kFleetSynchronization:      return "kFleetSync";
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

// Each list replaces the whole previous one, so the last one received wins.
static void ParseMapFleetMarkers(std::vector<std::pair<int64_t, std::vector<uint8_t>>>& rRawPackets, std::vector<GameMessages::MapFleetMarker>& rOutputMarkers)
{
	for (auto it = rRawPackets.begin(); it != rRawPackets.end(); )
	{
		if (static_cast<GamePacketType>(it->first) != GamePacketType::kServerMapFleetMarkers)
		{
			++it;
			continue;
		}

		GameMessages::MapFleetMarkersMessage::ReadPayload(it->second, rOutputMarkers);
		LOG(kNetwork, kDebug, "ClientSession received map fleet markers Markers: {}", std::ssize(rOutputMarkers));
		for (const GameMessages::MapFleetMarker& rMarker : rOutputMarkers)
		{
			LOG(kNetwork, kDebug, "ClientSession received map fleet marker Cell: ({},{}) Own: {}", rMarker.coordinate.iQ, rMarker.coordinate.iR, rMarker.uiFlags & GameMessages::MapFleetMarkersMessage::kuiOwnFlag);
		}

		it = rRawPackets.erase(it);
	}
}

void ClientSession::ProcessReceivedGamePackets()
{
	GamePacketType eParsingType = GamePacketType::kServerFleetSync;
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

		eParsingType = GamePacketType::kServerMapFleetMarkers;
		ParseMapFleetMarkers(mpRuntime->mpClient->mReceivedGamePackets, gpGame->mMapFleetMarkers);
	}
	catch (const std::exception& rException)
	{
		LOG(kNetwork, kWarning, "ClientSession::ProcessReceivedGamePackets failed processing packet (type {}): {}", static_cast<uint8_t>(eParsingType), rException.what());
	}

	// The poll is timed on the client, so the server keeps no map state; a closed map clears the time so the next open
	// requests at once.
	if (!gpGame->MapOpen())
	{
		mOptionalMapFleetMarkersRequestTime.reset();
		return;
	}
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	if (mOptionalMapFleetMarkersRequestTime && now - *mOptionalMapFleetMarkersRequestTime < kMapFleetMarkersRequestInterval)
	{
		return;
	}
	mOptionalMapFleetMarkersRequestTime = now;
	mpRuntime->SendGameRequest(GamePacketType::kClientMapFleetMarkersRequest, []
	{
		LOG(kNetwork, kDebug, "ClientSession::ProcessReceivedGamePackets map fleet markers request");
	}, GameMessages::MapFleetMarkersRequestMessage {});
}

// Applying an owned-entity event to the client owned-entity list is game-agnostic apart from two steps, the
// mFleetSelection pending-focus step and the client-cell move for the focused entity (ClientOwnedEntityIdentifier).
void ClientSession::ApplyOwnedEntityEvent(const engine::ReceivedOwnedEntityEvent& rEvent)
{
	if (rEvent.eState == engine::OwnedEntityState::kDied)
	{
		gpGame->RemoveClientOwnedEntity(rEvent.globalId);
		mpRuntime->UpdateDesiredCoordinates(ToString(SubscriptionChangeReason::kDied));
		return;
	}

	// Spawned and ChangedCell each carry the assignment.
	auto it = std::ranges::find(gpGame->mClientOwnedEntities, rEvent.globalId, &engine::OwnedEntity::globalId);
	if (it == gpGame->mClientOwnedEntities.end())
	{
		LOG(kNetwork, kVerbose, "OwnedEntityEvent kAssigned NewGlobalId: {} NewCoord: ({},{}) FocusedGlobalId: {} FocusedCoord: ({},{})", rEvent.globalId, rEvent.coordinate.iQ, rEvent.coordinate.iR, gpGame->ClientOwnedEntityIdentifier(), gpGame->mClientGridCoordinate.iQ, gpGame->mClientGridCoordinate.iR);
		gpGame->AddClientOwnedEntity(rEvent.globalId, rEvent.coordinate);
	}
	else
	{
		it->coord = rEvent.coordinate;
	}
	mpRuntime->UpdateDesiredCoordinates(ToString(SubscriptionChangeReason::kAssigned));

	// Not at the fleet sync that clears the member's kIsDead: that sync shares this event's reliable flush, and the client
	// applies owned-entity events before game packets, so this event arrives first.
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

void ClientSession::OnServerLoad()
{
	LOG(kDefault, kDebug, "ClientSession::OnServerLoad");

	// Clear owned-entity identity — server will reassign
	gpGame->mClientOwnedEntities.clear();
	gpGame->SetClientGridCoordinate({});
	gpGame->mfPreviousClientArmor = 0.0f;
	engine::gpCamera->mVecVisualErrorOffset = {};

	// Clear fleet state — server will re-sync
	gpGame->mFleetSelection.Clear();
}

void ClientSession::SendCreateFleetRequest()
{
	mpRuntime->SendGameRequest(GamePacketType::kClientCreateFleetRequest, []
	{
		LOG(kNetwork, kDebug, "ClientSession::SendCreateFleetRequest");
	}, GameMessages::CreateFleetRequestMessage {});
}

void ClientSession::SendSpawnIntoFleetRequest(const FleetGuid& rFleetGuid, ShipType eType)
{
	mpRuntime->SendGameRequest(GamePacketType::kClientSpawnIntoFleetRequest, [&]
	{
		LOG(kNetwork, kDebug, "ClientSession::SendSpawnIntoFleetRequest Fleet: ({},{}) Type: {}", rFleetGuid.uiHigh, rFleetGuid.uiLow, ShipTypeName(eType));
	}, GameMessages::SpawnIntoFleetRequestMessage {.fleetGuid = rFleetGuid, .uiShipType = std::to_underlying(eType)});
}

void ClientSession::SendDestroyFleetMemberRequest(engine::GlobalId globalId)
{
	mpRuntime->SendGameRequest(GamePacketType::kClientDestroyFleetMemberRequest, [&]
	{
		LOG(kNetwork, kDebug, "ClientSession::SendDestroyFleetMemberRequest Member: {}", globalId.iValue);
	}, GameMessages::DestroyFleetMemberRequestMessage {.globalId = globalId});
}

void ClientSession::SendSquadronOrdersRequest(engine::GlobalId globalId, ShipOrders_t orders)
{
	mpRuntime->SendGameRequest(GamePacketType::kClientSquadronOrdersRequest, [&]
	{
		LOG(kNetwork, kDebug, "ClientSession::SendSquadronOrdersRequest Member: {} Orders: {}", globalId.iValue, std::to_underlying(orders.meFlags));
	}, GameMessages::SquadronOrdersRequestMessage {.globalId = globalId, .uiOrders = std::to_underlying(orders.meFlags)});
}

void ClientSession::SendFleetNavigationRequest(const FleetGuid& rFleetGuid, float fDelay, FleetBehaviour eBehaviour, engine::GridCoord holdCoordinate)
{
	mpRuntime->SendGameRequest(GamePacketType::kClientFleetNavigation, [&]
	{
		LOG(kNetwork, kDebug, "ClientSession::SendFleetNavigationRequest Fleet: ({},{}) Delay: {} Behaviour: {} Hold: ({},{})", rFleetGuid.uiHigh, rFleetGuid.uiLow, common::Wb(fDelay, 3), kpcFleetBehaviourNames[std::to_underlying(eBehaviour)], holdCoordinate.iQ, holdCoordinate.iR);
	}, GameMessages::FleetNavigationRequestMessage {.fleetGuid = rFleetGuid, .fNavigationDelay = fDelay, .uiBehaviour = std::to_underlying(eBehaviour), .iHoldQ = holdCoordinate.iQ, .iHoldR = holdCoordinate.iR});
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

} // namespace game

#endif // BT_CLIENT
