#include "Pch.h"

#include "Network/Server/ServerClientManager.h"

#include "Network/Server/ServerTransferManager.h"

#include "Frame/Collections/Players/Players.h"
#include "Network/Server/ServerFleetManager.h"
#include "Network/Server/ServerSession.h"
#include "Network/PlayerEvents.h"
#include "Game.h"

namespace game
{

#if defined(BT_SERVER)

void ServerClientManager::QueueSpawnForClient(int64_t iClientId, const engine::ClientGuid& rClientGuid, const FleetGuid& rFleetGuid, engine::GlobalId memberGlobalPlayerId)
{
	// A queued spawn revives the client: clear its dead/processed state unconditionally.
	mDeadClientIds.erase(iClientId);
	mProcessedClientIds.erase(iClientId);

	// Dedup only the queue push on the full spawn identity so a client spamming spawn-into/respawn queues at most one spawn
	// per (fleet guid, member). Skip duplicates without reordering — request order is the order spawn status changes enter
	// the frame input, and therefore the simulation and the CRC.
	bool bAlreadyQueued = std::ranges::any_of(mClientsWaitingForSpawn, [&](const ClientSpawnInformation& rInformation)
	{
		return rInformation.iClientId == iClientId && rInformation.fleetGuid == rFleetGuid && rInformation.memberGlobalPlayerId == memberGlobalPlayerId;
	});
	if (!bAlreadyQueued)
	{
		mClientsWaitingForSpawn.push_back({.iClientId = iClientId, .clientGuid = rClientGuid, .fleetGuid = rFleetGuid, .memberGlobalPlayerId = memberGlobalPlayerId});
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

		auto ownedIt = gpServerSession->mClientPlayers.mOwned.find(rClient.iClientId);
		if (ownedIt != gpServerSession->mClientPlayers.mOwned.end() && !ownedIt->second.empty())
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

		int64_t iRelinkedPlayerCount = gpServerSession->RelinkFromFrames(rClient.iClientId, rClient.clientGuid, ServerSession::RelinkContext::kConnect);
		gpServerSession->mpFleetManager->OnClientConnected(rClient.iClientId, rClient.clientGuid);
		if (iRelinkedPlayerCount > 0)
		{
			continue;
		}

		// Client connects with zero players — spawns happen via fleet creation requests
		mProcessedClientIds.insert(rClient.iClientId);
		LOG(kNetwork, kVerbose, "ServerClientManager::NewClients Client: {} connected with no players", rClient.iClientId);
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
	for (const auto& [rCoordinate, rFrames] : gpGame->mCoordinateFrames)
	{
		const PlayersPostRender& rPlayers = *rFrames.pCurrent->postRender.pPlayers;
		if (rPlayers.iCount == 0)
		{
			continue;
		}
		LOG(kNetwork, kInfo, "  Coord: ({},{}) PlayerCount: {}", rCoordinate.iX, rCoordinate.iY, rPlayers.iCount);
		for (int64_t i = 0; i < rPlayers.iCount; ++i)
		{
			LOG(kNetwork, kInfo, "    Global: {} Guid: ({},{}) Match: {}", rPlayers.pGlobalPlayerIds[i].iValue, rPlayers.pClientGuids[i].uiHigh, rPlayers.pClientGuids[i].uiLow, rPlayers.pClientGuids[i] == rClient.clientGuid);
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
		engine::GlobalId globalPlayerId = (rClientSpawnInformation.memberGlobalPlayerId.iValue != 0)
			? rClientSpawnInformation.memberGlobalPlayerId
			: engine::GlobalId {.iValue = gpGame->miNextGlobalId++};

		bool bIsFlagship = false;
		engine::GridCoord spawnFleetWantedCoordinate {};
		uint8_t uiSpawnPendingFleetTicks = 0;
		if ((rClientSpawnInformation.fleetGuid.uiHigh != 0 || rClientSpawnInformation.fleetGuid.uiLow != 0))
		{
			ServerFleetManager::FleetLookupResult result = gpServerSession->mpFleetManager->LookupFleetWantedCoord(rClientSpawnInformation.clientGuid, rClientSpawnInformation.fleetGuid, rClientSpawnInformation.memberGlobalPlayerId);
			bIsFlagship = result.flags & ServerFleetManager::FleetLookupFlags::kIsFlagship;
			spawnFleetWantedCoordinate = result.fleetWantedCoord;
			uiSpawnPendingFleetTicks = result.uiPendingFleetWantedCoordTicks;
		}

		// The requesting client's GUID rides the status change, so the row this tick creates is born owned.
		StatusChange spawnChange {.eType = StatusChangeType::kSpawnPlayer, .data = SpawnPlayerData {.iGlobalId = globalPlayerId.iValue, .bIsFlagship = bIsFlagship, .fleetWantedCoordinate = spawnFleetWantedCoordinate, .uiPendingFleetWantedCoordinateTicks = uiSpawnPendingFleetTicks, .clientGuid = rClientSpawnInformation.clientGuid}};
		gpGame->mFrameInputs.try_emplace(engine::kOriginCoordinate).first->second.statusChanges.push_back(spawnChange);
		LOG(kNetwork, kVerbose, "ServerClientManager::SpawnWaitingClients::kSpawnPlayer Client: {} GlobalId: {} Coord: ({},{}) Flagship: {}", rClientSpawnInformation.iClientId, globalPlayerId.iValue, engine::kOriginCoordinate.iX, engine::kOriginCoordinate.iY, bIsFlagship);

		// Assignment is keyed on this spawn's id, so it completes here rather than waiting for the row to exist.
		// Client handles subscriptions — no full state sent here.
		gpServerSession->SendAssignPlayer(rClientSpawnInformation.iClientId, globalPlayerId, engine::kOriginCoordinate);
		gpServerSession->SendPlayerState(rClientSpawnInformation.iClientId, PlayerStateWireType::kSpawned, globalPlayerId.iValue, engine::kOriginCoordinate);
		gpServerSession->mClientPlayers.Add(rClientSpawnInformation.iClientId, globalPlayerId, engine::kOriginCoordinate);

		gpServerSession->mpFleetManager->OnPlayerSpawned(rClientSpawnInformation.iClientId, rClientSpawnInformation.clientGuid, rClientSpawnInformation, globalPlayerId);
	}

	mClientsWaitingForSpawn.clear();
}

void ServerClientManager::Disconnects()
{
	// Heap: drain disconnect events; registry and fleet-manager bookkeeping
	ScopedSuppressAllocationTracking suppress;

	for (const engine::PendingDisconnect& rDisconnect : gpServerSession->mpRuntime->mpServer->mPendingDisconnects)
	{
		LOG(kNetwork, kVerbose, "ServerClientManager::Disconnects Client: {} Players: {}", rDisconnect.iClientId, [&]()
		{
			auto ownedIt = gpServerSession->mClientPlayers.mOwned.find(rDisconnect.iClientId);
			return ownedIt != gpServerSession->mClientPlayers.mOwned.end() ? std::ssize(ownedIt->second) : 0i64;
		}());
		mDeadClientIds.erase(rDisconnect.iClientId);
		mProcessedClientIds.erase(rDisconnect.iClientId);

		gpServerSession->mpFleetManager->OnClientDisconnected(rDisconnect.clientGuid);

		std::erase_if(mClientsWaitingForSpawn, [&](const ClientSpawnInformation& rInformation)
		{
			return rInformation.iClientId == rDisconnect.iClientId;
		});

		gpServerSession->mClientPlayers.mOwned.erase(rDisconnect.iClientId);
	}
}

void ServerClientManager::DetectPlayerDeaths()
{
	std::vector<engine::ClientConnection>& rClients = engine::gpServer->mClients;
	for (const engine::ClientConnection& rClient : rClients)
	{
		auto ownedIt = gpServerSession->mClientPlayers.mOwned.find(rClient.iClientId);
		std::span<const engine::OwnedEntity> ownedPlayers = ownedIt != gpServerSession->mClientPlayers.mOwned.end() ? std::span<const engine::OwnedEntity>(ownedIt->second) : std::span<const engine::OwnedEntity>();

		if (ownedPlayers.empty())
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

		// Check each owned player for death (reverse iterate for safe removal)
		for (int64_t i = std::ssize(ownedPlayers) - 1; i >= 0; --i)
		{
			const engine::OwnedEntity& rOwnedPlayer = ownedPlayers[i];
			engine::GlobalId globalId = rOwnedPlayer.globalId;
			engine::GridCoord coordinate = rOwnedPlayer.coord;

			if (!gpGame->mCoordinateFrames.contains(coordinate))
			{
				continue;
			}

			const PlayersPostRender& rPlayers = *(*gpGame->mCoordinateFrames.at(coordinate).pCurrent).postRender.pPlayers;
			bool bFound = false;
			for (const engine::GlobalId& rGlobalPlayerId : std::span<const engine::GlobalId>(rPlayers.pGlobalPlayerIds, static_cast<size_t>(rPlayers.iCount)))
			{
				if (rGlobalPlayerId == globalId)
				{
					bFound = true;
					break;
				}
			}

			if (!bFound)
			{
				gpServerSession->SendPlayerState(rClient.iClientId, PlayerStateWireType::kDied, globalId.iValue, coordinate);
				LOG(kNetwork, kVerbose, "ServerClientManager::DetectPlayerDeaths Client: {} GlobalPlayer: {} Coord: ({},{})", rClient.iClientId, globalId, coordinate.iX, coordinate.iY);
				gpServerSession->mClientPlayers.RemoveAt(rClient.iClientId, i);

				gpServerSession->mpFleetManager->OnPlayerDeath(rClient.clientGuid, globalId);
			}
		}

		// Mark client as dead only when ALL owned players are dead
		ownedIt = gpServerSession->mClientPlayers.mOwned.find(rClient.iClientId);
		if (ownedIt == gpServerSession->mClientPlayers.mOwned.end() || ownedIt->second.empty())
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
