#include "Pch.h"

#include "Network/Server/ServerClientManager.h"

#include "Frame/IslandChainPlacement.h"
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

// The fleet spawn search walks square rings of this pitch around the origin cell's center.
constexpr float kfFleetSpawnSearchStepMeters = 10.0f;

// Every spawning Omni or Battleship is treated as a Battleship, the largest ship, so one list serves both. A candidate
// lies inside the hexagon inset by kfBattleshipPusherRadius (so a spawn there passes IsInsideCell) and more than
// kfBattleshipRadius outside every footprint rectangle of the origin's generated islands. Ring k holds the lattice
// points k steps out, walked south to north and each row west to east; the rings stop at the first one lying wholly
// past the circumradius.
static std::vector<XMFLOAT2> ComputeFleetSpawnCandidates()
{
	std::vector<engine::IslandPlacement> placements;
	engine::GenerateIslandChain(engine::kOriginCoordinate, placements);

	std::vector<XMFLOAT2> candidates;
	for (int64_t k = 0; static_cast<float>(k) * kfFleetSpawnSearchStepMeters <= engine::kfCellHalfWidth; ++k)
	{
		for (int64_t j = -k; j <= k; ++j)
		{
			// The ring's south and north rows run whole; every row between holds only the ring's two end points.
			int64_t iStep = (std::abs(j) == k) ? 1 : 2 * k;
			for (int64_t i = -k; i <= k; i += iStep)
			{
				XMFLOAT2A f2Candidate(static_cast<float>(i) * kfFleetSpawnSearchStepMeters, static_cast<float>(j) * kfFleetSpawnSearchStepMeters);
				if (!common::IsInsideHex(XMLoadFloat2A(&f2Candidate), engine::kCellDirections, kfBattleshipPusherRadius))
				{
					continue;
				}
				bool bClearOfIslands = std::ranges::all_of(placements, [&](const engine::IslandPlacement& rPlacement)
				{
					// Negated rotation maps cell-local positions into the island's frame, as the elevation splat does.
					const engine::IslandTemplate& rTemplate = engine::gpIslandTerrain->mIslands.at(rPlacement.islandCrc);
					common::SinCos rotation = common::DeterministicSinCos(-rPlacement.fRotation);
					float fDeltaX = f2Candidate.x - rPlacement.f2WorldPosition.x;
					float fDeltaY = f2Candidate.y - rPlacement.f2WorldPosition.y;
					float fLocalX = fDeltaX * rotation.fCos - fDeltaY * rotation.fSin;
					float fLocalY = fDeltaX * rotation.fSin + fDeltaY * rotation.fCos;
					return std::abs(fLocalX) > 0.5f * rTemplate.fQuadFootprintX + kfBattleshipRadius || std::abs(fLocalY) > 0.5f * rTemplate.fQuadFootprintY + kfBattleshipRadius;
				});
				if (bClearOfIslands)
				{
					candidates.push_back(f2Candidate);
				}
			}
		}
	}
	return candidates;
}

// The first candidate whose horizontal distance from every ship row of the origin's committed frame and from every
// point an earlier spawn of this TickRespawnTimers pass took exceeds two Battleship pusher radii, which no other pair
// of pusher radii exceeds, so no pusher acts on the new ship at its spawn; none when every candidate is crowded.
// A pure function of its inputs, so replay playback, which reruns the fleet step, reaches the recording's
// spawn-or-wait decision.
static std::optional<XMFLOAT2> FindFleetSpawnPoint(const std::vector<XMFLOAT2>& rCandidates, const Frame& rOriginFrame, std::span<const XMFLOAT2> takenSpawnPoints)
{
	static constexpr float kfClearance = 2.0f * kfBattleshipPusherRadius;
	for (const XMFLOAT2& rCandidate : rCandidates)
	{
		XMVECTOR vecCandidate = XMLoadFloat2(&rCandidate);
		auto IsClear = [&](FXMVECTOR vecOther)
		{
			return XMVectorGetX(XMVector2LengthSq(XMVectorSubtract(vecCandidate, vecOther))) > kfClearance * kfClearance;
		};
		bool bClear = std::ranges::all_of(takenSpawnPoints, [&](const XMFLOAT2& rTakenSpawnPoint)
		{
			return IsClear(XMLoadFloat2(&rTakenSpawnPoint));
		});
		// Every row counts, whatever its controller, plane, or lifecycle: pushers ignore planes, and a dying row is still there.
		ForEachShipType(rOriginFrame, [&]([[maybe_unused]] ShipType eType, const auto& rInterpolate, [[maybe_unused]] const auto& rPostRender)
		{
			for (int64_t i = 0; i < rInterpolate.iCount && bClear; ++i)
			{
				bClear = IsClear(rInterpolate.pVecPositions[i]);
			}
		});
		if (bClear)
		{
			return rCandidate;
		}
	}
	return std::nullopt;
}

ServerClientManager::ServerClientManager()
: mFleetSpawnCandidates(ComputeFleetSpawnCandidates())
{
	// The list depends only on the build's generator and island pack, so an empty one fails every server start.
	ASSERT(!mFleetSpawnCandidates.empty());
	LOG(kNetwork, kInfo, "ServerClientManager fleet spawn candidates Count: {} First: ({},{})", std::ssize(mFleetSpawnCandidates), common::Wb(mFleetSpawnCandidates.front().x, 1), common::Wb(mFleetSpawnCandidates.front().y, 1));
}

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
		LOG(kNetwork, kInfo, "  Coord: ({},{}) OwnedCount: {}", rCoordinate.iQ, rCoordinate.iR, iOwnedCount);
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

// A launching Fighter starts this far ahead of its Battleship's center along the heading, fully inside the hull. By its
// first spawning tick the Battleship's pusher has moved one tick ahead; it must still be behind the Fighter, or it pushes
// the Fighter out the stern.
constexpr float kfFighterLaunchOffset = kfBattleshipRadius - kfFighterRadius;
static_assert(kfFighterLaunchOffset > kfBattleshipMaxSpeed * engine::kfDeltaTime);

void ServerClientManager::SpawnFleetMember(const engine::ClientGuid& rClientGuid, const Fleet& rFleet, FleetMember& rMember, common::ScopedWorkbufferArena& rTakenSpawnPoints)
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
		LOG(kNetwork, kVerbose, "ServerClientManager::SpawnFleetMember Type: {} Guid: ({},{}) GlobalId: {} Coord: ({},{}) Flagship: {} FlagshipGlobalId: {}", ShipTypeName(rMember.eType), rClientGuid.uiHigh, rClientGuid.uiLow, rMember.globalId.iValue, spawnCoordinate.iQ, spawnCoordinate.iR, bIsFlagship, rFleet.flagshipGlobalId.iValue);
		rMember.flags.Set(FleetMemberFlags::kIsDead, false);
		rMember.coordinate = spawnCoordinate;
		gpServerSession->mpFleetManager->SendFleetSyncToClient(rClientGuid);
	};
	auto QueueSpawnPointSpawn = [&](auto spawnInfo)
	{
		std::optional<XMFLOAT2> optionalSpawnPoint = FindFleetSpawnPoint(mFleetSpawnCandidates, *gpGame->mCells.at(engine::kOriginCoordinate).pCurrent, rTakenSpawnPoints.mBuffer.Span<XMFLOAT2>());
		if (!optionalSpawnPoint)
		{
			// The member stays dead, so the next tick's respawn pass searches again.
			if (!mbFleetSpawnBlocked)
			{
				LOG(kNetwork, kWarning, "ServerClientManager::SpawnFleetMember no clear fleet spawn point in the origin cell, retrying each tick Type: {} Guid: ({},{}) GlobalId: {}", ShipTypeName(rMember.eType), rClientGuid.uiHigh, rClientGuid.uiLow, rMember.globalId.iValue);
				mbFleetSpawnBlocked = true;
			}
			return;
		}
		mbFleetSpawnBlocked = false;
		rTakenSpawnPoints.mBuffer.PushBack(*optionalSpawnPoint);
		spawnInfo.vecPosition = XMVectorSet(optionalSpawnPoint->x, optionalSpawnPoint->y, engine::gpFlightPlanes->Plane(iTargetFlightPlane).fHeightMeters, 1.0f);
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
			if (!engine::IsInsideCell(vecLaunchPosition))
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
