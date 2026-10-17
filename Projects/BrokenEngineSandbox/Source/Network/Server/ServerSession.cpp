#include "Pch.h"

#include "Network/Server/ServerSession.h"

#include "Network/NetworkCursor.h"

#include "Agent/Commands/ServerSimulationHarnessRigs.h"
#include "Frame/Collections/Omnis/Omnis.h"
#include "Frame/ServerCellStats.h"
#include "Network/Server/ServerClientManager.h"
#include "Network/Server/ServerFleetManager.h"
#include "Network/Server/ServerFleetSerialization.h"
#include "Network/GamePacketType.h"
#include "Game.h"

namespace game
{

#if defined(BT_SERVER)

ServerSession::ServerSession()
: common::Singleton<ServerSession>(gpServerSession, this)
{
	mpFleetManager = std::make_unique<ServerFleetManager>();
	mpClientManager = std::make_unique<ServerClientManager>();
}

ServerSession::~ServerSession()
{
	DetachServerSimulationHarnessRigs(*this);
	mpRuntime.reset();
}

// Reject a wire-supplied navigation delay outside [0, 60] before it enters server-authoritative sim state:
// the delay resets the navigation timers, and a non-finite one would freeze them or fire them every tick.
static float AdmitNavigationDelay(float fDelay)
{
	if (!OmnisPostRender::IsNavigationDelayInRange(fDelay))
	{
		engine::NetworkMessages::ThrowCorruptStream("ServerSession navigation delay");
	}
	return fDelay;
}

static bool ReadBooleanByte(const uint8_t*& pCursor)
{
	int64_t iValue = engine::ReadUint8(pCursor);
	if (iValue > 1)
	{
		engine::NetworkMessages::ThrowCorruptStream("ServerSession Boolean byte");
	}
	return iValue != 0;
}

// The engine drain admits each packet through its contract row first. Every client-sendable game contract row has
// min == max (GamePacketType.h), so admission settles each payload's size exactly and the cases below add no per-case
// size check; only residual value checks a row cannot express — navigation-delay range and Boolean bytes — stay, and
// they throw to drop the packet.
void ServerSession::HandleGamePacket(const engine::DeferredClientPacket& rPacket)
{
	switch (static_cast<GamePacketType>(rPacket.iPacketType))
	{
		case GamePacketType::kClientUpdateOwnedEntityRequest:
		{
			// 8B global ID + 1B bUseMissiles + 4B fNavigationDelay = 13 bytes (type byte already stripped)
			const uint8_t* pCursor = rPacket.payload.data();
			engine::GlobalId globalId {};
			globalId.iValue = engine::ReadInt64(pCursor);
			bool bUseMissiles = ReadBooleanByte(pCursor);
			float fNavigationDelay = AdmitNavigationDelay(engine::ReadFloat(pCursor));
			mPendingUpdateOwnedEntityRequests.push_back({.iClientId = rPacket.iClientId, .globalId = globalId, .bUseMissiles = bUseMissiles, .navigationDelaySeconds = std::chrono::duration<float>(fNavigationDelay)});
			break;
		}
		case GamePacketType::kClientCreateFleetRequest:
		{
			mpFleetManager->mPendingCreateFleetRequests.push_back({.iClientId = rPacket.iClientId});
			break;
		}
		case GamePacketType::kClientDeleteFleetRequest:
		{
			// 16B fleetGuid = 16 bytes (type byte already stripped)
			const uint8_t* pCursor = rPacket.payload.data();
			FleetGuid fleetGuid {};
			fleetGuid.uiHigh = engine::ReadUint64(pCursor);
			fleetGuid.uiLow = engine::ReadUint64(pCursor);
			mpFleetManager->mPendingDeleteFleetRequests.push_back({.iClientId = rPacket.iClientId, .fleetGuid = fleetGuid});
			break;
		}
		case GamePacketType::kClientSpawnIntoFleetRequest:
		{
			// 16B fleetGuid = 16 bytes (type byte already stripped)
			const uint8_t* pCursor = rPacket.payload.data();
			FleetGuid fleetGuid {};
			fleetGuid.uiHigh = engine::ReadUint64(pCursor);
			fleetGuid.uiLow = engine::ReadUint64(pCursor);
			mpFleetManager->mPendingSpawnIntoFleetRequests.push_back({.iClientId = rPacket.iClientId, .fleetGuid = fleetGuid});
			break;
		}
		case GamePacketType::kClientRespawnInFleetRequest:
		{
			// 16B fleetGuid + 8B member globalId = 24 bytes (type byte already stripped)
			const uint8_t* pCursor = rPacket.payload.data();
			FleetGuid fleetGuid {};
			fleetGuid.uiHigh = engine::ReadUint64(pCursor);
			fleetGuid.uiLow = engine::ReadUint64(pCursor);
			engine::GlobalId memberGlobalId {.iValue = engine::ReadInt64(pCursor)};
			mpFleetManager->mPendingRespawnInFleetRequests.push_back({.iClientId = rPacket.iClientId, .fleetGuid = fleetGuid, .memberGlobalId = memberGlobalId});
			break;
		}
		case GamePacketType::kClientFleetNavigationDelay:
		{
			// 16B fleetGuid + 4B delay = 20 bytes (type byte already stripped)
			const uint8_t* pCursor = rPacket.payload.data();
			FleetGuid fleetGuid {};
			fleetGuid.uiHigh = engine::ReadUint64(pCursor);
			fleetGuid.uiLow = engine::ReadUint64(pCursor);
			float fDelay = AdmitNavigationDelay(engine::ReadFloat(pCursor));
			const engine::ClientConnection* pClient = engine::gpServer->FindClient(rPacket.iClientId);
			mpFleetManager->UpdateFleetNavigationDelay(pClient->clientGuid, fleetGuid, std::chrono::duration<float>(fDelay));
			break;
		}
		default:
			break;
	}
}

void ServerSession::BeforeNetworkPoll()
{
	// mfLastDeltaTime still describes the previous update here. A zero-delta update could not consume
	// injected owned-entity requests, so retain them; a positive delta keeps their existing one-update lifetime.
	if (gpGame->mfLastDeltaTime > 0.0f)
	{
		mPendingUpdateOwnedEntityRequests.clear();
	}
	// Fleet request queues drain inside their own Process*Requests, so there is nothing left to clear here.
}

void ServerSession::AfterNetworkPoll()
{
	mpClientManager->NewClients();
	// Ordering contract: Create runs first so requests naming a fleet created in the same poll can
	// resolve its guid. SpawnInto before Respawn also matters: both append to the client manager's
	// spawn queue, and that order is the order spawn status changes enter the frame input, and
	// therefore the simulation and the CRC.
	mpFleetManager->ProcessCreateFleetRequests();
	mpFleetManager->ProcessDeleteFleetRequests();
	mpFleetManager->ProcessSpawnIntoFleetRequests();
	mpFleetManager->ProcessRespawnInFleetRequests();
}

void ServerSession::FinalizeTickClients()
{
	mpFleetManager->DetectDisconnectedOwnedEntityDeaths();

	// Post-swap, so the counters describe the tick that just finished, and once per advancing tick.
	PublishServerEntityCounts();
}

void ServerSession::BuildAdvancingFrameInputs()
{
	std::erase_if(mpClientManager->mClientsWaitingForSpawn, [&](const game::ClientSpawnInformation& rClientSpawnInformation)
	{
		if ((rClientSpawnInformation.fleetGuid.uiHigh == 0 && rClientSpawnInformation.fleetGuid.uiLow == 0))
		{
			return false;
		}

		game::ServerFleetManager::FleetLookupResult result = mpFleetManager->LookupFleetWantedCoord(rClientSpawnInformation.clientGuid, rClientSpawnInformation.fleetGuid, rClientSpawnInformation.memberGlobalId);
		if (result.flags & game::ServerFleetManager::FleetLookupFlags::kFound)
		{
			return false;
		}

		LOG(kNetwork, kWarning, "ServerSession::BuildAdvancingFrameInputs Dropping queued spawn Client: {} FleetGuid: ({},{})", rClientSpawnInformation.iClientId, rClientSpawnInformation.fleetGuid.uiHigh, rClientSpawnInformation.fleetGuid.uiLow);

		// A client left with no owned entity is handled again by the next NewClients, whose fleet connect step restarts
		// its dead members' respawn countdowns; SpawnWaitingClients marks it handled again if it still spawns this update.
		auto ownedIt = mClientOwnedEntities.mOwned.find(rClientSpawnInformation.iClientId);
		if (ownedIt == mClientOwnedEntities.mOwned.end() || ownedIt->second.empty())
		{
			mpClientManager->mProcessedClientIds.erase(rClientSpawnInformation.iClientId);
		}
		return true;
	});

	// Inject weapon mode toggle StatusChanges
	ProcessUpdateOwnedEntityRequests();

	// Tick fleet timers and inject fleet coord updates only when this update advances. Subtracting
	// a zero delta does not make TickFleetTimers inert: an already-expired timer can still fire,
	// consume random state, and queue an update that no frame tick could consume. Deferring the
	// timer and all queued work here applies them in order on the first advancing update.
	mpFleetManager->TickFleetTimers();
	mpFleetManager->ProcessFlagshipUpdates();

	// After ProcessFlagshipUpdates so the flagship update this queues is consumed on the following update and its
	// fleet-RNG draw keeps its position, and before the first tick's PrepareTickStatusChanges drain so a drain that
	// releases agent entries for the origin coordinate sorts this spawn in with them.
	mpClientManager->SpawnWaitingClients();
}

void ServerSession::ProcessUpdateOwnedEntityRequests()
{
	// Heap: pending owned-entity update requests may grow frame status changes
	ScopedSuppressAllocationTracking suppress;

	for (const PendingUpdateOwnedEntityRequest& rRequest : mPendingUpdateOwnedEntityRequests)
	{
		engine::ClientConnection* pClient = engine::gpServer->FindClient(rRequest.iClientId);
		if (pClient == nullptr)
		{
			continue;
		}
		auto ownedIt = mClientOwnedEntities.mOwned.find(pClient->iClientId);
		std::span<const engine::OwnedEntity> ownedEntities = ownedIt != mClientOwnedEntities.mOwned.end() ? std::span<const engine::OwnedEntity>(ownedIt->second) : std::span<const engine::OwnedEntity>();

		engine::GridCoord updateCoord {};
		bool bFound = false;
		for (const engine::OwnedEntity& rOwnedEntity : ownedEntities)
		{
			if (rOwnedEntity.globalId == rRequest.globalId)
			{
				updateCoord = rOwnedEntity.coord;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			continue;
		}

		auto it = game::gpGame->mFrameInputs.find(updateCoord);
		if (it == game::gpGame->mFrameInputs.end())
		{
			continue;
		}

		int64_t iOmniUuid = engine::RegistryUuidByGlobalId(game::Frame::OwnershipLayer((*game::gpGame->mCells.at(updateCoord).pCurrent)), rRequest.globalId).iValue;
		if (iOmniUuid == 0)
		{
			continue;
		}

		int64_t iPendingWeaponModeTicks = engine::kiTickRate;
		game::StatusChange updateChange {.eType = game::StatusChangeType::kUpdateOmni, .data = game::UpdateOmniData {.iOmniUuid = iOmniUuid, .bUseMissiles = rRequest.bUseMissiles, .navigationDelaySeconds = rRequest.navigationDelaySeconds, .uiPendingWeaponModeTicks = static_cast<uint8_t>(iPendingWeaponModeTicks)}};
		it->second.statusChanges.push_back(updateChange);

		LOG(kNetwork, kDebug, "ServerSession::ProcessUpdateOwnedEntityRequests Client: {} GlobalId: {} OmniUuid: {} Coord: ({},{}) Missiles: {} NavDelay: {}", rRequest.iClientId, rRequest.globalId, iOmniUuid, updateCoord.iX, updateCoord.iY, rRequest.bUseMissiles, common::Wb(rRequest.navigationDelaySeconds.count(), 3));
	}
}

void ServerSession::OnOwnedEntityChangedCell(const engine::ClientGuid& rClientGuid, engine::GlobalId globalId, engine::GridCoord destination)
{
	mpFleetManager->OnOwnedEntityTransferred(rClientGuid, globalId, destination);
}

void ServerSession::OnOwnedEntityDied(const engine::ClientGuid& rClientGuid, engine::GlobalId globalId)
{
	mpFleetManager->OnOwnedEntityDeath(rClientGuid, globalId);
}

void ServerSession::OnResetForLoad()
{
	mpFleetManager->mNavigation.mPendingFlagshipUpdates.clear();

	for (const engine::ClientConnection& rClient : engine::gpServer->mClients)
	{
		mpFleetManager->OnResetForLoad(rClient.iClientId, rClient.clientGuid);
	}

	mpClientManager->ResetState();
	mPendingUpdateOwnedEntityRequests.clear();
	// ReadFleetData restores mFleets and OnResetForLoad restores mGuidToClientId; preserve both while clearing pending requests.
	mpFleetManager->ClearPendingRequests();
	// OnResetForLoad queues flagship updates after the initial clear; retain those updates.
}

void ServerSession::WriteFleetData(std::fstream& rFileStream) const
{
	::game::WriteFleetData(rFileStream, mpFleetManager->mFleets, mpFleetManager->mRandomEngine);
}

#endif // BT_SERVER

} // namespace game
