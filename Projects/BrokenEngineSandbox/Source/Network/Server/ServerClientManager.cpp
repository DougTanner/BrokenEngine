#include "Pch.h"

#include "Network/Server/ServerClientManager.h"

#include "Network/Server/ServerTransferManager.h"

#include "Frame/Collections/Ships/ShipQueries.h"
#include "Frame/HealthDamage.h"
#include "Frame/TerrainUtils.h"
#include "Network/Server/ServerFleetManager.h"
#include "Network/Server/ServerSession.h"
#include "Game.h"

namespace game
{

#if defined(BT_SERVER)

void ServerClientManager::NewClients()
{
	// Heap: processed-client-ID set insert
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

		LogConnectingClientDiagnostic(rClient);

		gpServerSession->mpFleetManager->OnClientConnected(rClient.iClientId, rClient.clientGuid);
		mProcessedClientIds.insert(rClient.iClientId);

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

// The fleet spawn point in meters, local to the origin cell, whose frame is centered on the cell. Chosen outside every
// island rectangle of cell (0,0) by more than kfBattleshipRadius; the nearest is about 20 m away.
constexpr float kfFleetSpawnOffsetX = 120.0f;
constexpr float kfFleetSpawnOffsetY = -80.0f;

// A launching Fighter starts this far ahead of its Battleship's center along the heading, fully inside the hull. By its
// first spawning tick the Battleship's pusher has moved one tick ahead; it must still be behind the Fighter, or it pushes
// the Fighter out the stern.
constexpr float kfFighterLaunchOffset = kfBattleshipRadius - kfFighterRadius;
static_assert(kfFighterLaunchOffset > kfBattleshipMaxSpeed * engine::kfDeltaTime);

void ServerClientManager::SpawnFleetMember(const engine::ClientGuid& rClientGuid, const Fleet& rFleet, FleetMember& rMember)
{
	// The fleet step runs while miTickCounter names the tick being prepared; that tick's post-tick batch creates each
	// queued row.
	int64_t iSpawnTick = gpGame->miTickCounter;
	bool bIsFlagship = rMember.globalId == rFleet.flagshipGlobalId;
	int64_t iTargetFlightPlane = rFleet.targetFlightPlanes.at(std::to_underlying(rMember.eType));

	ShipOrders_t spawnOrders {};
	if (bIsFlagship)
	{
		spawnOrders.Set(ShipOrders::kIsFlagship);
	}
	SetTargetFlightPlane(spawnOrders, iTargetFlightPlane);

	// The owner's GUID rides the spawn, so the row is born owned.
	auto QueueFleetSpawn = [&](engine::GridCoord spawnCoordinate, auto spawnInfo)
	{
		spawnInfo.alignment = gpGame->mFleetAlignment;
		spawnInfo.eController = ShipController::kFleet;
		spawnInfo.orders = spawnOrders;
		spawnInfo.globalId = rMember.globalId;
		spawnInfo.clientGuid = rClientGuid;
		spawnInfo.flagshipGlobalId = rFleet.flagshipGlobalId;
		spawnInfo.fleetWantedCoordinate = rFleet.wantedCoordinate;
		spawnInfo.uiPendingFleetWantedCoordinateTicks = rFleet.uiPendingFleetWantedCoordinateTicks;
		// Playback takes every spawn from the recorded post-tick batches.
		if (!gpGame->mbReplaying)
		{
			gpServerSession->mpTransferManager->QueueSpawn(spawnCoordinate, iSpawnTick, MakeSpawn(spawnInfo));
		}
		LOG(kNetwork, kVerbose, "ServerClientManager::SpawnFleetMember Type: {} Guid: ({},{}) GlobalId: {} Coord: ({},{}) Flagship: {} FlagshipGlobalId: {}", ShipTypeName(rMember.eType), rClientGuid.uiHigh, rClientGuid.uiLow, rMember.globalId.iValue, spawnCoordinate.iX, spawnCoordinate.iY, bIsFlagship, rFleet.flagshipGlobalId.iValue);
		rMember.flags.Set(FleetMemberFlags::kIsDead, false);
		rMember.coordinate = spawnCoordinate;
		gpServerSession->mpFleetManager->SendFleetSyncToClient(rClientGuid);
	};
	auto QueueSpawnPointSpawn = [&](auto spawnInfo)
	{
		spawnInfo.vecPosition = XMVectorSet(kfFleetSpawnOffsetX, kfFleetSpawnOffsetY, engine::gpFlightPlanes->Plane(iTargetFlightPlane).fHeightMeters, 1.0f);
		spawnInfo.vecDirection = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		spawnInfo.arrivalGracePeriod = kArrivalGracePeriod;
		spawnInfo.uiFlightPlane = static_cast<uint8_t>(iTargetFlightPlane);
		QueueFleetSpawn(engine::kOriginCoordinate, spawnInfo);
	};
	switch (rMember.eType)
	{
		case ShipType::kFighter:
		{
			// At most one launch per fleet per kfShipSpawnInterval; a skipped Fighter stays dead and is tried again on the
			// next tick.
			auto nextLaunchIt = mNextFighterLaunchTicks.find(rFleet.guid);
			if (nextLaunchIt != mNextFighterLaunchTicks.end() && nextLaunchIt->second > iSpawnTick)
			{
				return;
			}

			engine::GridCoord flagshipCoordinate = std::ranges::find(rFleet.members, rFleet.flagshipGlobalId, &FleetMember::globalId)->coordinate;
			if (!gpGame->mCells.contains(flagshipCoordinate))
			{
				return;
			}
			const Frame& rBattleshipFrame = *gpGame->mCells.at(flagshipCoordinate).pCurrent;
			std::optional<ShipLocator> optionalBattleship = FindShipByGlobalId(rBattleshipFrame, rFleet.flagshipGlobalId);
			if (!optionalBattleship)
			{
				return;
			}

			const BattleshipsInterpolate& rBattleshipsInterpolate = *rBattleshipFrame.interpolate.pBattleships;
			int64_t iBattleshipRow = optionalBattleship->iRow;
			XMVECTOR vecBattleshipDirection = rBattleshipsInterpolate.pVecDirections[iBattleshipRow];
			XMVECTOR vecLaunchPosition = XMVectorMultiplyAdd(XMVectorReplicate(kfFighterLaunchOffset), vecBattleshipDirection, rBattleshipsInterpolate.pVecPositions[iBattleshipRow]);
			vecLaunchPosition = XMVectorSetW(vecLaunchPosition, 1.0f);
			// QueueSpawn asserts the spawn position is inside the cell.
			if (!common::InsideArea(vecLaunchPosition, engine::LocalCellArea()))
			{
				return;
			}
			// Terrain too close ahead also waits: the launch pose must not trigger the Fighter's own terrain avoidance.
			// With a current turn of 0 that function returns exactly 0 when it does not trigger.
			int64_t iLaunchNavigationPlane = NavigationFlightPlane(XMVectorGetZ(vecLaunchPosition), iTargetFlightPlane);
			if (ComputeTerrainAvoidance(gpGame->mCells.at(flagshipCoordinate).staticData, vecLaunchPosition, vecBattleshipDirection, 0.0f, iLaunchNavigationPlane) != 0.0f)
			{
				return;
			}
			FightersPostRender::SpawnInfo spawnInfo {};
			spawnInfo.vecPosition = vecLaunchPosition;
			spawnInfo.vecDirection = vecBattleshipDirection;
			spawnInfo.vecVelocity = rBattleshipFrame.postRender.pBattleships->pVecVelocities[iBattleshipRow];
			spawnInfo.uiFlightPlane = rBattleshipsInterpolate.puiFlightPlanes[iBattleshipRow];
			spawnOrders.Set(ShipOrders::kSpawning);
			QueueFleetSpawn(flagshipCoordinate, spawnInfo);
			mNextFighterLaunchTicks.insert_or_assign(rFleet.guid, iSpawnTick + static_cast<int64_t>(kfShipSpawnInterval * engine::kiTickRate));
			break;
		}
		case ShipType::kOmni:       QueueSpawnPointSpawn(OmnisPostRender::SpawnInfo {}); break;
		case ShipType::kBattleship: QueueSpawnPointSpawn(BattleshipsPostRender::SpawnInfo {}); break;
		case ShipType::kCount:
			ASSERT(false);
			std::unreachable();
	}
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

	gpServerSession->mClientOwnedEntities.mOwned.erase(iClientId);
}

void ServerClientManager::ResetState()
{
	// Runs after the load rebuilt the registry: a client left with no owned entity is handled again by NewClients.
	std::erase_if(mProcessedClientIds, [](int64_t iClientId)
	{
		auto ownedIt = gpServerSession->mClientOwnedEntities.mOwned.find(iClientId);
		return ownedIt == gpServerSession->mClientOwnedEntities.mOwned.end() || ownedIt->second.empty();
	});
	// The load may move the tick counter backward; the next launch of each fleet may then come at once.
	mNextFighterLaunchTicks.clear();
}

#endif // BT_SERVER

} // namespace game
