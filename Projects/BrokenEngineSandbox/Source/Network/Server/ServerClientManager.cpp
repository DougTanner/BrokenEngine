#include "Pch.h"

#include "Network/Server/ServerClientManager.h"

#include "Frame/IslandChainPlacement.h"
#include "Network/Server/ServerTransferManager.h"

#include "Frame/Collections/Ships/ShipHitboxes.h"
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
// lies inside the hexagon inset by the largest ship pusher radius (so a spawn there passes IsInsideCell) and more than
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
				if (!common::IsInsideHex(XMLoadFloat2A(&f2Candidate), engine::kCellDirections, gfLargestShipPusherRadius))
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
// point an earlier spawn of this TickRespawnTimers pass took exceeds the largest ship pusher radius; none when every
// candidate is crowded. A pusher acts only on rows within its own radius, so that leaves the new ship outside every
// pusher, and every row outside the new ship's pusher, at its spawn. A pure function of its inputs, so replay playback,
// which reruns the fleet step, reaches the recording's spawn-or-wait decision.
static std::optional<XMFLOAT2> FindFleetSpawnPoint(const std::vector<XMFLOAT2>& rCandidates, const Frame& rOriginFrame, std::span<const XMFLOAT2> takenSpawnPoints)
{
	for (const XMFLOAT2& rCandidate : rCandidates)
	{
		XMVECTOR vecCandidate = XMLoadFloat2(&rCandidate);
		auto IsClear = [&](FXMVECTOR vecOther)
		{
			return XMVectorGetX(XMVector2LengthSq(XMVectorSubtract(vecCandidate, vecOther))) > gfLargestShipPusherRadius * gfLargestShipPusherRadius;
		};
		bool bClear = std::ranges::all_of(takenSpawnPoints, [&](const XMFLOAT2& rTakenSpawnPoint)
		{
			return IsClear(XMLoadFloat2(&rTakenSpawnPoint));
		});
		// Every row counts, whatever its controller, plane, or lifecycle: horizontal clearance from every row also clears
		// each pusher's 3D range, with no plane or lifecycle test.
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

bool ServerClientManager::SpawnFleetMember(const engine::ClientGuid& rClientGuid, const Fleet& rFleet, FleetMember& rMember, int64_t& riNextFighterLaunchBay, common::ScopedWorkbufferArena& rTakenSpawnPoints)
{
	// The fleet step runs while miTickCounter names the tick being prepared; that tick's post-tick batch creates each
	// queued row.
	int64_t iSpawnTick = gpGame->miTickCounter;
	bool bIsFlagship = rMember.globalId == rFleet.flagshipGlobalId;
	int64_t iTargetFlightPlane = DefaultFlightPlane(rMember.eType);

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
			return false;
		}
		mbFleetSpawnBlocked = false;
		rTakenSpawnPoints.mBuffer.PushBack(*optionalSpawnPoint);
		spawnInfo.vecPosition = XMVectorSet(optionalSpawnPoint->x, optionalSpawnPoint->y, engine::gpFlightPlanes->Lowest().fHeightMeters, 1.0f);
		spawnInfo.vecDirection = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		spawnInfo.arrivalGracePeriod = kArrivalGracePeriod;
		spawnInfo.uiFlightPlane = 0;
		QueueFleetSpawn(engine::kOriginCoordinate, spawnInfo);
		return true;
	};
	switch (rMember.eType)
	{
		case ShipType::kFighter:
		{
			engine::GridCoord flagshipCoordinate = std::ranges::find(rFleet.members, rFleet.flagshipGlobalId, &FleetMember::globalId)->coordinate;
			if (!gpGame->mCells.contains(flagshipCoordinate))
			{
				return false;
			}
			const Frame& rBattleshipFrame = *gpGame->mCells.at(flagshipCoordinate).pCurrent;
			std::optional<ShipLocator> optionalBattleship = FindShipByGlobalId(rBattleshipFrame, rFleet.flagshipGlobalId);
			if (!optionalBattleship)
			{
				return false;
			}

			const BattleshipsInterpolate& rBattleshipsInterpolate = *rBattleshipFrame.interpolate.pBattleships;
			int64_t iBattleshipRow = optionalBattleship->iRow;
			std::span<const common::PackedModelHardpoint> bays = gShipHitboxShapes[std::to_underlying(ShipType::kBattleship)][std::to_underlying(rBattleshipFrame.postRender.pBattleships->pController[iBattleshipRow])].fighterLaunchBays;

			XMVECTOR vecBattleshipDirection = rBattleshipsInterpolate.pVecDirections[iBattleshipRow];
			XMVECTOR vecBattleshipPosition = rBattleshipsInterpolate.pVecPositions[iBattleshipRow];
			float fCos = XMVectorGetX(vecBattleshipDirection);
			float fSin = XMVectorGetY(vecBattleshipDirection);
			while (riNextFighterLaunchBay < std::ssize(bays))
			{
				const common::PackedModelHardpoint& rBay = bays[static_cast<size_t>(riNextFighterLaunchBay++)];
				XMVECTOR vecLaunchPosition = XMVectorSetW(XMVectorSetZ(RotateByHeading(vecBattleshipDirection, XMLoadFloat3(&rBay.f3Position), vecBattleshipPosition), XMVectorGetZ(vecBattleshipPosition)), 1.0f);
				// QueueSpawn asserts the spawn position is inside the cell.
				if (!engine::IsInsideCell(vecLaunchPosition))
				{
					continue;
				}
				XMVECTOR vecAlignedDirection = XMVector2Normalize(XMVectorSetZ(XMLoadFloat3(&rBay.f3Direction), 0.0f));
				XMVECTOR vecLaunchDirection = common::TransformDirection(vecAlignedDirection, XMMATRIX(fCos, fSin, 0.0f, 0.0f, -fSin, fCos, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));
				// Terrain admission uses the Fighter's own avoidance at its authored launch heading.
				int64_t iLaunchNavigationPlane = NavigationFlightPlane(XMVectorGetZ(vecLaunchPosition), iTargetFlightPlane);
				if (ComputeTerrainAvoidance(gpGame->mCells.at(flagshipCoordinate).staticData, vecLaunchPosition, vecLaunchDirection, 0.0f, iLaunchNavigationPlane) != 0.0f)
				{
					continue;
				}
				FightersPostRender::SpawnInfo spawnInfo
				{
					.vecPosition = vecLaunchPosition,
					.vecDirection = vecLaunchDirection,
					.vecVelocity = rBattleshipFrame.postRender.pBattleships->pVecVelocities[iBattleshipRow],
					.uiFlightPlane = rBattleshipsInterpolate.puiFlightPlanes[iBattleshipRow],
				};
				QueueFleetSpawn(flagshipCoordinate, spawnInfo);
				return true;
			}
			return false;
		}
		case ShipType::kOmni:       return QueueSpawnPointSpawn(OmnisPostRender::SpawnInfo {});
		case ShipType::kBattleship: return QueueSpawnPointSpawn(BattleshipsPostRender::SpawnInfo {});
		default:
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
