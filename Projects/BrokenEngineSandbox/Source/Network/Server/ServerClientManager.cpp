#include "Pch.h"

#include "Network/Server/ServerClientManager.h"

#include "Frame/Collections/Players/Players.h"
#include "Game.h"
#include "Network/PlayerEvents.h"
#include "Network/Server/ServerFleetManager.h"
#include "Network/Server/ServerSession.h"
#include "Network/Server/ServerTransferManager.h"

namespace game
{

#if defined(BT_SERVER)

void ServerClientManager::QueueSpawnForClient(int64_t iClientId, const engine::ClientGuid& rClientGuid, const FleetGuid& rFleetGuid, int64_t iMemberIndex)
{
	// A queued spawn revives the client: clear its dead/processed state unconditionally.
	mDeadClientIds.erase(iClientId);
	mProcessedClientIds.erase(iClientId);

	// Dedup only the queue push on the full spawn identity so a client spamming spawn-into/respawn queues at most one spawn
	// per (fleet guid, member). Skip duplicates without reordering — request order is the order spawn status changes enter
	// the frame input, and therefore the simulation and the CRC.
	bool bAlreadyQueued = std::ranges::any_of(mClientsWaitingForSpawn, [&](const ClientSpawnInfo& rInfo)
	{
		return rInfo.iClientId == iClientId && rInfo.fleetGuid == rFleetGuid && rInfo.iMemberIndex == iMemberIndex;
	});
	if (!bAlreadyQueued)
	{
		mClientsWaitingForSpawn.push_back({iClientId, rClientGuid, rFleetGuid, iMemberIndex});
	}
}

void ServerClientManager::NewClients()
{
	// Heap: vector push_back for waiting clients
	ScopedSuppressAllocationTracking suppress;

	std::vector<engine::ClientConnection>& rClients = engine::gpServer->mClients;
	for (engine::ClientConnection& rClient : rClients)
	{
		if (!rClient.bHandshakeComplete)
		{
			continue;
		}

		if (!gpServerSession->mClientPlayers.Owned(rClient.iClientId).empty())
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

		if (std::ranges::contains(mClientsWaitingForSpawn, rClient.iClientId, &ClientSpawnInfo::iClientId))
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
	// Diagnostic: dump connecting GUID, server-side fleet roster, and per-coord player GUIDs so we can see whether reconnect should re-link
	LOG(kNetwork, kInfo, "ServerClientManager::NewClients Connecting Client: {} Guid: ({},{}) Empty: {}",
		rClient.iClientId, rClient.clientGuid.uiHigh, rClient.clientGuid.uiLow, rClient.clientGuid.IsEmpty());
	LOG(kNetwork, kInfo, "  FleetGuids: {}", gpServerSession->mpFleetManager->mFleets.size());
	for (const auto& [rExistingGuid, rExistingFleets] : gpServerSession->mpFleetManager->mFleets)
	{
		LOG(kNetwork, kInfo, "    Guid: ({},{}) FleetCount: {} Match: {}",
			rExistingGuid.uiHigh, rExistingGuid.uiLow, rExistingFleets.size(), rExistingGuid == rClient.clientGuid);
	}
	for (const auto& [rCoord, rFrames] : gpGame->mCoordFrames)
	{
		const PlayersPostRender& rPlayers = *rFrames.pCurrent->postRender.pPlayers;
		if (rPlayers.iCount == 0)
		{
			continue;
		}
		LOG(kNetwork, kInfo, "  Coord: ({},{}) PlayerCount: {}", rCoord.x, rCoord.y, rPlayers.iCount);
		for (int64_t i = 0; i < rPlayers.iCount; ++i)
		{
			LOG(kNetwork, kInfo, "    Global: {} Guid: ({},{}) Match: {}",
				rPlayers.pGlobalPlayerIds[i].iValue, rPlayers.pClientGuids[i].uiHigh, rPlayers.pClientGuids[i].uiLow,
				rPlayers.pClientGuids[i] == rClient.clientGuid);
		}
	}
}

void ServerClientManager::SpawnWaitingClients()
{
	// Heap: vector push_back for status changes and owned-entity records
	ScopedSuppressAllocationTracking suppress;

	for (const ClientSpawnInfo& rClientSpawnInformation : mClientsWaitingForSpawn)
	{
		engine::global_id_t globalPlayerId {gpGame->GenerateGlobalId()};

		bool bIsFlagship = false;
		engine::GridCoord spawnFleetWantedCoord {};
		uint8_t uiSpawnPendingFleetTicks = 0;
		if (!rClientSpawnInformation.fleetGuid.IsEmpty())
		{
			ServerFleetManager::FleetLookupResult result = gpServerSession->mpFleetManager->LookupFleetWantedCoord(rClientSpawnInformation.clientGuid, rClientSpawnInformation.fleetGuid, rClientSpawnInformation.iMemberIndex);
			bIsFlagship = result.flags & ServerFleetManager::FleetLookupFlags::kIsFlagship;
			spawnFleetWantedCoord = result.fleetWantedCoord;
			uiSpawnPendingFleetTicks = result.uiPendingFleetWantedCoordTicks;
		}

		// The requesting client's GUID rides the status change, so the row this tick creates is born owned.
		StatusChange spawnChange {.eType = StatusChangeType::kSpawnPlayer, .data = SpawnPlayerData{.iGlobalId = globalPlayerId.iValue, .bIsFlagship = bIsFlagship, .fleetWantedCoord = spawnFleetWantedCoord, .uiPendingFleetWantedCoordTicks = uiSpawnPendingFleetTicks, .clientGuid = rClientSpawnInformation.clientGuid}};
		gpGame->mFrameInputs.try_emplace(engine::kOriginCoord).first->second.statusChanges.push_back(spawnChange);
		LOG(kNetwork, kVerbose, "ServerClientManager::SpawnWaitingClients::kSpawnPlayer Client: {} GlobalId: {} Coord: ({},{}) Flagship: {}", rClientSpawnInformation.iClientId, globalPlayerId.iValue, engine::kOriginCoord.x, engine::kOriginCoord.y, bIsFlagship);

		// Assignment is keyed on the id just minted, so it completes here rather than waiting for the row to exist.
		// Client handles subscriptions — no full state sent here.
		gpServerSession->SendAssignPlayer(rClientSpawnInformation.iClientId, globalPlayerId, engine::kOriginCoord);
		gpServerSession->SendPlayerState(rClientSpawnInformation.iClientId, PlayerStateWireType::kSpawned, globalPlayerId.iValue, engine::kOriginCoord);
		gpServerSession->mClientPlayers.Add(rClientSpawnInformation.iClientId, globalPlayerId, engine::kOriginCoord);

		// Associate with fleet if this spawn was fleet-triggered
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
		LOG(kNetwork, kVerbose, "ServerClientManager::Disconnects Client: {} Players: {}", rDisconnect.iClientId, gpServerSession->mClientPlayers.Owned(rDisconnect.iClientId).size());
		mDeadClientIds.erase(rDisconnect.iClientId);
		mProcessedClientIds.erase(rDisconnect.iClientId);

		gpServerSession->mpFleetManager->OnClientDisconnected(rDisconnect.clientGuid);

		// Remove from spawn queue if waiting
		std::erase_if(mClientsWaitingForSpawn, [&](const ClientSpawnInfo& rInfo)
		{
			return rInfo.iClientId == rDisconnect.iClientId;
		});

		gpServerSession->mClientPlayers.Remove(rDisconnect.iClientId);
	}
}

void ServerClientManager::DetectPlayerDeaths()
{
	std::vector<engine::ClientConnection>& rClients = engine::gpServer->mClients;
	for (engine::ClientConnection& rClient : rClients)
	{
		std::span<const engine::OwnedEntity> ownedPlayers = gpServerSession->mClientPlayers.Owned(rClient.iClientId);

		if (ownedPlayers.empty())
		{
			continue;
		}

		if (mDeadClientIds.contains(rClient.iClientId))
		{
			continue;
		}

		// Skip clients mid-transfer (subscription update pending from HarvestTransfers)
		if (gpServerSession->mpTransferManager->HasPendingSubscriptionUpdate(rClient.iClientId))
		{
			continue;
		}

		// Heap: per-frame death scan may erase registry entries and authorized coords
		ScopedSuppressAllocationTracking suppress;

		// Check each owned player for death (reverse iterate for safe removal)
		for (int64_t i = std::ssize(ownedPlayers) - 1; i >= 0; --i)
		{
			const engine::OwnedEntity& rOwnedPlayer = ownedPlayers[i];
			engine::global_id_t globalId = rOwnedPlayer.globalId;
			engine::GridCoord coord = rOwnedPlayer.coord;

			if (!gpGame->mCoordFrames.contains(coord))
			{
				continue;
			}

			// Scan pGlobalPlayerIds to see if the player still exists
			const PlayersPostRender& rPlayers = *gpGame->CurrentFrame(coord).postRender.pPlayers;
			bool bFound = false;
			for (int64_t j = 0; j < rPlayers.iCount; ++j)
			{
				if (rPlayers.pGlobalPlayerIds[j] == globalId)
				{
					bFound = true;
					break;
				}
			}

			if (!bFound)
			{
				gpServerSession->SendPlayerState(rClient.iClientId, PlayerStateWireType::kDied, globalId.iValue, coord);
				LOG(kNetwork, kVerbose, "ServerClientManager::DetectPlayerDeaths Client: {} GlobalPlayer: {} Coord: ({},{})", rClient.iClientId, globalId, coord.x, coord.y);
				gpServerSession->mClientPlayers.RemoveAt(rClient.iClientId, i);

				gpServerSession->mpFleetManager->OnPlayerDeath(rClient.clientGuid, globalId);
			}
		}

		// Mark client as dead only when ALL owned players are dead
		if (gpServerSession->mClientPlayers.Owned(rClient.iClientId).empty())
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
