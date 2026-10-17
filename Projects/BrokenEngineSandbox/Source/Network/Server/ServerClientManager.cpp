#include "Pch.h"

#include "Network/Server/ServerClientManager.h"

#include "Network/Server/ServerTransferManager.h"

#include "Frame/Collections/Omnis/Omnis.h"
#include "Network/Server/ServerFleetManager.h"
#include "Network/Server/ServerSession.h"
#include "Network/OwnedEntityEvents.h"
#include "Game.h"

namespace game
{

#if defined(BT_SERVER)

void ServerClientManager::QueueSpawnForClient(int64_t iClientId, const engine::ClientGuid& rClientGuid, const FleetGuid& rFleetGuid, engine::GlobalId memberGlobalId)
{
	// A queued spawn revives the client: clear its dead/processed state unconditionally.
	mDeadClientIds.erase(iClientId);
	mProcessedClientIds.erase(iClientId);

	// Dedup only the queue push on the full spawn identity so a client spamming spawn-into/respawn queues at most one spawn
	// per (fleet guid, member). Skip duplicates without reordering — request order is the order spawn status changes enter
	// the frame input, and therefore the simulation and the CRC.
	bool bAlreadyQueued = std::ranges::any_of(mClientsWaitingForSpawn, [&](const ClientSpawnInformation& rInformation)
	{
		return rInformation.iClientId == iClientId && rInformation.fleetGuid == rFleetGuid && rInformation.memberGlobalId == memberGlobalId;
	});
	if (!bAlreadyQueued)
	{
		mClientsWaitingForSpawn.push_back({.iClientId = iClientId, .clientGuid = rClientGuid, .fleetGuid = rFleetGuid, .memberGlobalId = memberGlobalId});
	}
}

void ServerClientManager::NewClients()
{
	// Heap: vector push_back for waiting clients
	ScopedSuppressAllocationTracking suppress;

	std::vector<engine::ClientConnection>& rClients = engine::gpServer->mClients;
	for (const engine::ClientConnection& rClient : rClients)
	{
		if (!rClient.bHandshakeComplete)
		{
			continue;
		}

		auto ownedIt = gpServerSession->mClientOwnedEntities.mOwned.find(rClient.iClientId);
		if (ownedIt != gpServerSession->mClientOwnedEntities.mOwned.end() && !ownedIt->second.empty())
		{
			continue;
		}

		if (mDeadClientIds.contains(rClient.iClientId))
		{
			continue;
		}

		if (mProcessedClientIds.contains(rClient.iClientId))
		{
			continue;
		}

		if (std::ranges::contains(mClientsWaitingForSpawn, rClient.iClientId, &ClientSpawnInformation::iClientId))
		{
			continue;
		}

		LogConnectingClientDiagnostic(rClient);

		int64_t iRelinkedOwnedEntityCount = gpServerSession->RelinkFromFrames(rClient.iClientId, rClient.clientGuid, ServerSession::RelinkContext::kConnect);
		gpServerSession->mpFleetManager->OnClientConnected(rClient.iClientId, rClient.clientGuid);
		if (iRelinkedOwnedEntityCount > 0)
		{
			continue;
		}

		// Client connects with zero owned entities — spawns happen via fleet creation requests
		mProcessedClientIds.insert(rClient.iClientId);
		LOG(kNetwork, kVerbose, "ServerClientManager::NewClients Client: {} connected with no owned entities", rClient.iClientId);
	}
}

void ServerClientManager::LogConnectingClientDiagnostic(const engine::ClientConnection& rClient)
{
	LOG(kNetwork, kInfo, "ServerClientManager::NewClients Connecting Client: {} Guid: ({},{}) Empty: {}", rClient.iClientId, rClient.clientGuid.uiHigh, rClient.clientGuid.uiLow, (rClient.clientGuid.uiHigh == 0 && rClient.clientGuid.uiLow == 0));
	LOG(kNetwork, kInfo, "  FleetGuids: {}", gpServerSession->mpFleetManager->mFleets.size());
	for (const auto& [rExistingGuid, rExistingFleets] : gpServerSession->mpFleetManager->mFleets)
	{
		LOG(kNetwork, kInfo, "    Guid: ({},{}) FleetCount: {} Match: {}", rExistingGuid.uiHigh, rExistingGuid.uiLow, std::ssize(rExistingFleets), rExistingGuid == rClient.clientGuid);
	}
	for (const auto& [rCoordinate, rCell] : gpGame->mCells)
	{
		const OmnisPostRender& rOmnis = *rCell.pCurrent->postRender.pOmnis;
		if (rOmnis.iCount == 0)
		{
			continue;
		}
		LOG(kNetwork, kInfo, "  Coord: ({},{}) OmniCount: {}", rCoordinate.iX, rCoordinate.iY, rOmnis.iCount);
		for (int64_t i = 0; i < rOmnis.iCount; ++i)
		{
			LOG(kNetwork, kInfo, "    Global: {} Guid: ({},{}) Match: {}", rOmnis.pGlobalIds[i].iValue, rOmnis.pClientGuids[i].uiHigh, rOmnis.pClientGuids[i].uiLow, rOmnis.pClientGuids[i] == rClient.clientGuid);
		}
	}
}

void ServerClientManager::SpawnWaitingClients()
{
	// Heap: vector push_back for status changes and owned-entity records
	ScopedSuppressAllocationTracking suppress;

	for (const ClientSpawnInformation& rClientSpawnInformation : mClientsWaitingForSpawn)
	{
		// A respawned fleet member keeps its global ID; only a new ship mints one.
		engine::GlobalId globalId = (rClientSpawnInformation.memberGlobalId.iValue != 0)
			? rClientSpawnInformation.memberGlobalId
			: engine::GlobalId {.iValue = gpGame->miNextGlobalId++};

		bool bIsFlagship = false;
		engine::GridCoord spawnFleetWantedCoordinate {};
		int64_t iSpawnPendingFleetTicks = 0;
		if ((rClientSpawnInformation.fleetGuid.uiHigh != 0 || rClientSpawnInformation.fleetGuid.uiLow != 0))
		{
			ServerFleetManager::FleetLookupResult result = gpServerSession->mpFleetManager->LookupFleetWantedCoord(rClientSpawnInformation.clientGuid, rClientSpawnInformation.fleetGuid, rClientSpawnInformation.memberGlobalId);
			bIsFlagship = result.flags & ServerFleetManager::FleetLookupFlags::kIsFlagship;
			spawnFleetWantedCoordinate = result.fleetWantedCoord;
			iSpawnPendingFleetTicks = result.iPendingFleetWantedCoordTicks;
		}

		// The requesting client's GUID rides the status change, so the row this tick creates is born owned.
		StatusChange spawnChange {.eType = StatusChangeType::kSpawnOmni, .data = SpawnOmniData {.iGlobalId = globalId.iValue, .bIsFlagship = bIsFlagship, .fleetWantedCoordinate = spawnFleetWantedCoordinate, .uiPendingFleetWantedCoordinateTicks = static_cast<uint8_t>(iSpawnPendingFleetTicks), .clientGuid = rClientSpawnInformation.clientGuid}};
		gpGame->mFrameInputs.try_emplace(engine::kOriginCoordinate).first->second.statusChanges.push_back(spawnChange);
		LOG(kNetwork, kVerbose, "ServerClientManager::SpawnWaitingClients::kSpawnOmni Client: {} GlobalId: {} Coord: ({},{}) Flagship: {}", rClientSpawnInformation.iClientId, globalId.iValue, engine::kOriginCoordinate.iX, engine::kOriginCoordinate.iY, bIsFlagship);

		// Assignment is keyed on this spawn's id, so it completes here rather than waiting for the row to exist.
		// Client handles subscriptions — no full state sent here.
		gpServerSession->SendAssignOwnedEntity(rClientSpawnInformation.iClientId, globalId, engine::kOriginCoordinate);
		gpServerSession->SendOwnedEntityState(rClientSpawnInformation.iClientId, OwnedEntityStateWireType::kSpawned, globalId.iValue, engine::kOriginCoordinate);
		gpServerSession->mClientOwnedEntities.Add(rClientSpawnInformation.iClientId, globalId, engine::kOriginCoordinate);

		gpServerSession->mpFleetManager->OnOwnedEntitySpawned(rClientSpawnInformation.iClientId, rClientSpawnInformation.clientGuid, rClientSpawnInformation, globalId);
	}

	mClientsWaitingForSpawn.clear();
}

void ServerClientManager::OnClientDisconnected(int64_t iClientId, const engine::ClientGuid& rClientGuid)
{
	// Heap: registry and fleet-manager bookkeeping
	ScopedSuppressAllocationTracking suppress;

	LOG(kNetwork, kVerbose, "ServerClientManager::OnClientDisconnected Client: {} OwnedEntities: {}", iClientId, [&]()
	{
		auto ownedIt = gpServerSession->mClientOwnedEntities.mOwned.find(iClientId);
		return ownedIt != gpServerSession->mClientOwnedEntities.mOwned.end() ? std::ssize(ownedIt->second) : 0i64;
	}());
	mDeadClientIds.erase(iClientId);
	mProcessedClientIds.erase(iClientId);

	gpServerSession->mpFleetManager->OnClientDisconnected(iClientId, rClientGuid);

	std::erase_if(mClientsWaitingForSpawn, [&](const ClientSpawnInformation& rInformation)
	{
		return rInformation.iClientId == iClientId;
	});

	gpServerSession->mClientOwnedEntities.mOwned.erase(iClientId);
}

void ServerClientManager::DetectOwnedEntityDeaths()
{
	std::vector<engine::ClientConnection>& rClients = engine::gpServer->mClients;
	for (const engine::ClientConnection& rClient : rClients)
	{
		auto ownedIt = gpServerSession->mClientOwnedEntities.mOwned.find(rClient.iClientId);
		std::span<const engine::OwnedEntity> ownedEntities = ownedIt != gpServerSession->mClientOwnedEntities.mOwned.end() ? std::span<const engine::OwnedEntity>(ownedIt->second) : std::span<const engine::OwnedEntity>();

		if (ownedEntities.empty())
		{
			continue;
		}

		if (mDeadClientIds.contains(rClient.iClientId))
		{
			continue;
		}

		// Skip clients mid-transfer (subscription update pending from HarvestTransfers)
		if (std::ranges::contains(gpServerSession->mPendingSubscriptionUpdates, rClient.iClientId, &game::SubscriptionUpdate::iClientId))
		{
			continue;
		}

		// Heap: per-frame death scan may erase registry entries and authorized coords
		ScopedSuppressAllocationTracking suppress;

		// Check each owned entity for death (reverse iterate for safe removal)
		for (int64_t i = std::ssize(ownedEntities) - 1; i >= 0; --i)
		{
			const engine::OwnedEntity& rOwnedEntity = ownedEntities[i];
			engine::GlobalId globalId = rOwnedEntity.globalId;
			engine::GridCoord coordinate = rOwnedEntity.coord;

			if (!gpGame->mCells.contains(coordinate))
			{
				continue;
			}

			const OmnisPostRender& rOmnis = *(*gpGame->mCells.at(coordinate).pCurrent).postRender.pOmnis;
			bool bFound = false;
			for (const engine::GlobalId& rGlobalId : std::span<const engine::GlobalId>(rOmnis.pGlobalIds, static_cast<size_t>(rOmnis.iCount)))
			{
				if (rGlobalId == globalId)
				{
					bFound = true;
					break;
				}
			}

			if (!bFound)
			{
				gpServerSession->SendOwnedEntityState(rClient.iClientId, OwnedEntityStateWireType::kDied, globalId.iValue, coordinate);
				LOG(kNetwork, kVerbose, "ServerClientManager::DetectOwnedEntityDeaths Client: {} GlobalId: {} Coord: ({},{})", rClient.iClientId, globalId, coordinate.iX, coordinate.iY);
				gpServerSession->mClientOwnedEntities.RemoveAt(rClient.iClientId, i);

				gpServerSession->mpFleetManager->OnOwnedEntityDeath(rClient.clientGuid, globalId);
			}
		}

		// Mark client as dead only when ALL owned entities are dead
		ownedIt = gpServerSession->mClientOwnedEntities.mOwned.find(rClient.iClientId);
		if (ownedIt == gpServerSession->mClientOwnedEntities.mOwned.end() || ownedIt->second.empty())
		{
			mDeadClientIds.insert(rClient.iClientId);
		}
	}
}

void ServerClientManager::ResetState()
{
	mClientsWaitingForSpawn.clear();
	mDeadClientIds.clear();
	mProcessedClientIds.clear();
}

#endif // BT_SERVER

} // namespace game
