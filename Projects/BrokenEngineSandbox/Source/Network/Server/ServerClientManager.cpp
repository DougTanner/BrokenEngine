#include "Pch.h"

#include "Network/Server/ServerClientManager.h"

#include "Network/Server/ServerTransferManager.h"

#include "Frame/Collections/Ships/ShipQueries.h"
#include "Frame/HealthDamage.h"
#include "Network/Server/ServerFleetManager.h"
#include "Network/Server/ServerSession.h"
#include "Game.h"

namespace game
{

#if defined(BT_SERVER)

void ServerClientManager::QueueSpawnForClient(int64_t iClientId, const engine::ClientGuid& rClientGuid, const FleetGuid& rFleetGuid, engine::GlobalId memberGlobalId, ShipType eType)
{
	// Dedup only the queue push on the full spawn identity so a client spamming spawn-into/respawn queues at most one spawn
	// per (fleet guid, member). Skip duplicates without reordering — request order is the order spawns are queued for the
	// post-tick batch, which the simulation and the CRC depend on.
	bool bAlreadyQueued = std::ranges::any_of(mClientsWaitingForSpawn, [&](const ClientSpawnInformation& rInformation)
	{
		return rInformation.iClientId == iClientId && rInformation.fleetGuid == rFleetGuid && rInformation.memberGlobalId == memberGlobalId;
	});
	if (!bAlreadyQueued)
	{
		mClientsWaitingForSpawn.push_back({.iClientId = iClientId, .clientGuid = rClientGuid, .fleetGuid = rFleetGuid, .memberGlobalId = memberGlobalId, .eType = eType});
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

		if (mProcessedClientIds.contains(rClient.iClientId))
		{
			continue;
		}

		if (std::ranges::contains(mClientsWaitingForSpawn, rClient.iClientId, &ClientSpawnInformation::iClientId))
		{
			continue;
		}

		LogConnectingClientDiagnostic(rClient);

		// ServerSessionBase::RebuildOwnedEntities runs before this each poll, so the fleet refresh sees the client's surviving owned entities.
		gpServerSession->mpFleetManager->OnClientConnected(rClient.iClientId, rClient.clientGuid);
		mProcessedClientIds.insert(rClient.iClientId);

		// Client connects with zero owned entities — spawns happen via fleet creation requests
		auto ownedIt = gpServerSession->mClientOwnedEntities.mOwned.find(rClient.iClientId);
		if (ownedIt == gpServerSession->mClientOwnedEntities.mOwned.end() || ownedIt->second.empty())
		{
			LOG(kNetwork, kVerbose, "ServerClientManager::NewClients Client: {} connected with no owned entities", rClient.iClientId);
		}
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
		const Frame& rFrame = *rCell.pCurrent;
		int64_t iOwnedCount = Frame::OwnedEntityCount(rFrame);
		if (iOwnedCount == 0)
		{
			continue;
		}
		LOG(kNetwork, kInfo, "  Coord: ({},{}) OwnedCount: {}", rCoordinate.iX, rCoordinate.iY, iOwnedCount);
		ForEachShipType(rFrame, [&](ShipType eType, [[maybe_unused]] const auto& rInterpolate, const auto& rPostRender)
		{
			for (int64_t i = 0; i < rPostRender.iCount; ++i)
			{
				if (rPostRender.pController[i] != ShipController::kFleet)
				{
					continue;
				}
				LOG(kNetwork, kInfo, "    Type: {} Global: {} Guid: ({},{}) Match: {}", ShipTypeName(eType), rPostRender.pGlobalIds[i].iValue, rPostRender.pClientGuids[i].uiHigh, rPostRender.pClientGuids[i].uiLow, rPostRender.pClientGuids[i] == rClient.clientGuid);
			}
		});
	}
}

// The fleet spawn point in meters, local to the origin cell, whose frame is centered on the cell.
constexpr float kfFleetSpawnOffsetX = 45.0f;
constexpr float kfFleetSpawnOffsetY = -12.0f;

void ServerClientManager::SpawnWaitingClients()
{
	// Heap: spawn-queue push_back and processed-client set inserts
	ScopedSuppressAllocationTracking suppress;

	for (const ClientSpawnInformation& rClientSpawnInformation : mClientsWaitingForSpawn)
	{
		// A respawned fleet member keeps its global ID; only a new ship mints one.
		engine::GlobalId globalId = (rClientSpawnInformation.memberGlobalId.iValue != 0)
			? rClientSpawnInformation.memberGlobalId
			: engine::GlobalId {.iValue = gpGame->miNextGlobalId++};

		bool bIsFlagship = false;
		engine::GlobalId flagshipGlobalId {};
		engine::GridCoord spawnFleetWantedCoordinate {};
		int64_t iSpawnPendingFleetTicks = 0;
		int64_t iTargetFlightPlane = 0;
		if ((rClientSpawnInformation.fleetGuid.uiHigh != 0 || rClientSpawnInformation.fleetGuid.uiLow != 0))
		{
			ServerFleetManager::FleetLookupResult result = gpServerSession->mpFleetManager->LookupFleetWantedCoord(rClientSpawnInformation.clientGuid, rClientSpawnInformation.fleetGuid, rClientSpawnInformation.memberGlobalId);
			bIsFlagship = result.flags & ServerFleetManager::FleetLookupFlags::kIsFlagship;
			// A flagship spawn follows itself; an empty fleet's first ship gets its ID only above, so the lookup cannot name it.
			flagshipGlobalId = bIsFlagship ? globalId : result.flagshipGlobalId;
			spawnFleetWantedCoordinate = result.fleetWantedCoord;
			iSpawnPendingFleetTicks = result.iPendingFleetWantedCoordTicks;
			iTargetFlightPlane = result.targetFlightPlanes.at(std::to_underlying(rClientSpawnInformation.eType));
		}

		ShipOrders_t spawnOrders {};
		if (bIsFlagship)
		{
			spawnOrders.Set(ShipOrders::kIsFlagship);
		}
		SetTargetFlightPlane(spawnOrders, iTargetFlightPlane);

		// miTickCounter + 1 is the next tick; its post-tick batch creates the row. The requesting client's GUID rides the
		// spawn, so that row is born owned.
		auto QueueFleetSpawn = [&](auto spawnInfo)
		{
			spawnInfo.vecPosition = XMVectorSet(kfFleetSpawnOffsetX, kfFleetSpawnOffsetY, engine::gpFlightPlanes->Plane(iTargetFlightPlane).fHeightMeters, 1.0f);
			spawnInfo.vecDirection = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
			spawnInfo.alignment = gpGame->mFleetAlignment;
			spawnInfo.eController = ShipController::kFleet;
			spawnInfo.orders = spawnOrders;
			spawnInfo.arrivalGracePeriod = kArrivalGracePeriod;
			spawnInfo.globalId = globalId;
			spawnInfo.clientGuid = rClientSpawnInformation.clientGuid;
			spawnInfo.flagshipGlobalId = flagshipGlobalId;
			spawnInfo.fleetWantedCoordinate = spawnFleetWantedCoordinate;
			spawnInfo.uiPendingFleetWantedCoordinateTicks = static_cast<uint8_t>(iSpawnPendingFleetTicks);
			gpServerSession->mpTransferManager->QueueSpawn(engine::kOriginCoordinate, gpGame->miTickCounter + 1, MakeSpawn(spawnInfo));
		};
		switch (rClientSpawnInformation.eType)
		{
			case ShipType::kFighter:    QueueFleetSpawn(FightersPostRender::SpawnInfo {}); break;
			case ShipType::kOmni:       QueueFleetSpawn(OmnisPostRender::SpawnInfo {}); break;
			case ShipType::kBattleship: QueueFleetSpawn(BattleshipsPostRender::SpawnInfo {}); break;
			case ShipType::kCount:
				ASSERT(false);
				std::unreachable();
		}
		LOG(kNetwork, kVerbose, "ServerClientManager::SpawnWaitingClients Type: {} Client: {} GlobalId: {} Coord: ({},{}) Flagship: {} FlagshipGlobalId: {}", ShipTypeName(rClientSpawnInformation.eType), rClientSpawnInformation.iClientId, globalId.iValue, engine::kOriginCoordinate.iX, engine::kOriginCoordinate.iY, bIsFlagship, flagshipGlobalId.iValue);

		// A client spawning this update is not handled again by NewClients after another of its queued spawns drops.
		mProcessedClientIds.insert(rClientSpawnInformation.iClientId);

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
	mProcessedClientIds.erase(iClientId);

	gpServerSession->mpFleetManager->OnClientDisconnected(iClientId, rClientGuid);

	std::erase_if(mClientsWaitingForSpawn, [&](const ClientSpawnInformation& rInformation)
	{
		return rInformation.iClientId == iClientId;
	});

	gpServerSession->mClientOwnedEntities.mOwned.erase(iClientId);
}

void ServerClientManager::ResetState()
{
	mClientsWaitingForSpawn.clear();
	// Runs after the load rebuilt the registry: a client left with no owned entity is handled again by NewClients.
	std::erase_if(mProcessedClientIds, [](int64_t iClientId)
	{
		auto ownedIt = gpServerSession->mClientOwnedEntities.mOwned.find(iClientId);
		return ownedIt == gpServerSession->mClientOwnedEntities.mOwned.end() || ownedIt->second.empty();
	});
}

#endif // BT_SERVER

} // namespace game
