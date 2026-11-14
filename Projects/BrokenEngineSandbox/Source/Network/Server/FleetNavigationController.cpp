#include "Pch.h"

#include "Network/Server/FleetNavigationController.h"

#include "Frame/NavQuery.h"

#include "Frame/Collections/Ships/ShipBehaviours.h"
#include "Frame/Collections/Ships/ShipQueries.h"
#include "Frame/TerrainUtils.h"
#include "Network/Server/ServerFleetSerialization.h"
#include "Game.h"

namespace game
{

#if defined(BT_SERVER)

// The fleet step runs ahead of this tick's dispatch, so a cell just loaded or activated has not built its navigation data.
// It builds here, on the main thread with no tick running: the answer reaches the fleet checksum, and playback can hold an
// unbuilt cell where the recording had a built one.
static bool FlagshipReachesEdge(const Frame& rFlagshipFrame, ShipLocator flagship, const engine::CellStaticData& rStaticData, int64_t iDirection)
{
	if (!(rStaticData.flags & engine::CellStaticDataFlags::kNavigationDataBuilt))
	{
		// Heap: BuildCellNavigationData grows the per-graph navigationData vector and each graph's vertex, polygon, and visibility-edge vectors, and
		// this path runs on the main thread inside the armed main loop
		ScopedSuppressAllocationTracking suppress;
		engine::BuildCellNavigationData(rStaticData.navigationData, rStaticData.islands);
		rStaticData.flags.Set(engine::CellStaticDataFlags::kNavigationDataBuilt);
	}

	XMVECTOR vecPosition = ShipPosition(rFlagshipFrame.interpolate, flagship);
	int64_t iTargetFlightPlane = VisitShip(rFlagshipFrame, flagship, []([[maybe_unused]] const auto& rInterpolate, const auto& rPostRender, int64_t iRow)
	{
		return GetTargetFlightPlane(rPostRender.pOrders[iRow]);
	});
	int64_t iNavigationPlane = NavigationFlightPlane(XMVectorGetZ(vecPosition), iTargetFlightPlane);
	return engine::NavQueryEdgeReachable(vecPosition, iDirection, NavigationPlaneData(rStaticData, ShipTypeClearanceTier(flagship.eType), iNavigationPlane));
}

// Directions are collected in index order so a draw index maps to one direction; the origin is never a candidate.
template <typename TKEEP>
static int64_t ReachableNeighbourDirections(const Frame& rFlagshipFrame, ShipLocator flagship, engine::GridCoord coordinate, int64_t (&rpiDirections)[common::kiHexDirectionCount], const TKEEP& rKeep)
{
	int64_t iCount = 0;
	for (int64_t i = 0; i < common::kiHexDirectionCount; ++i)
	{
		const common::HexDirection& rDirection = engine::kCellDirections.pEntries[i];
		engine::GridCoord neighbour {};
		if (engine::TryAddGridCoordinate(coordinate, rDirection.iDeltaQ, rDirection.iDeltaR, neighbour)
		 && !(neighbour == engine::kOriginCoordinate) && rKeep(neighbour)
		 && FlagshipReachesEdge(rFlagshipFrame, flagship, gpGame->mCells.at(coordinate).staticData, i))
		{
			rpiDirections[iCount++] = i;
		}
	}
	return iCount;
}

// Owners walk in GUID order because each fleet's direction draw consumes the one fleet RNG, and replay playback must
// draw in the recording's order.
void FleetNavigationController::TickFleetTimers(std::unordered_map<engine::ClientGuid, std::vector<Fleet>>& rFleets, common::RandomEngine& rRandom)
{
	common::ScopedWorkbufferArena ownersArena = common::gpThreadLocal->mWorkbuffer.Push();
	for (const engine::ClientGuid& rGuid : SortedGuidKeys(rFleets, ownersArena))
	{
		for (Fleet& rFleet : rFleets.at(rGuid))
		{
			const FleetMember& rFlagship = *std::ranges::find(rFleet.members, rFleet.flagshipGlobalId, &FleetMember::globalId);
			if (rFlagship.flags & FleetMemberFlags::kIsDead)
			{
				continue;
			}

			// Drain cellChangeTimerSeconds only while the flagship is at wantedCoordinate, so the cycle is transit plus
			// navigationDelaySeconds of idle time. Drain there even in cross-cell mode; an expired timer fires when that mode
			// ends instead of freezing. The fleet step runs once per tick, so each pass drains one tick.
			if (rFlagship.coordinate == rFleet.wantedCoordinate)
			{
				rFleet.cellChangeTimerSeconds -= std::chrono::duration<float>(engine::kfDeltaTime);
			}

			if (rFleet.cellChangeTimerSeconds.count() > 0.0f)
			{
				continue;
			}

			// A cross-cell move between ticks can move the flagship after the timer last drained.
			if (!(rFlagship.coordinate == rFleet.wantedCoordinate))
			{
				continue;
			}
			const Frame& rFlagshipFrame = *gpGame->mCells.at(rFlagship.coordinate).pCurrent;
			ShipLocator flagship = FindShipByGlobalId(rFlagshipFrame, rFlagship.globalId).value();
			int64_t iFlagshipNavigationDirection = VisitShip(rFlagshipFrame, flagship, []([[maybe_unused]] const auto& rInterpolate, const auto& rPostRender, int64_t iRow)
			{
				return GetNavigationDirection(rPostRender.pFlags[iRow]);
			});

			if (iFlagshipNavigationDirection >= 0 && iFlagshipNavigationDirection < common::kiHexDirectionCount)
			{
				continue;
			}

			// Every pick is an edge neighbour of the flagship's cell and no candidate is the origin, so no pick moves the
			// flagship into it.
			engine::GridCoord coordinate = rFlagship.coordinate;
			FleetBehaviour ePickBehaviour = rFleet.eBehaviour;
			int64_t piDirections[common::kiHexDirectionCount] {};
			int64_t iCount = 0;
			if (rFleet.eBehaviour == FleetBehaviour::kExplore)
			{
				int64_t iDistance = engine::GridDistance(engine::kOriginCoordinate, coordinate);
				iCount = ReachableNeighbourDirections(rFlagshipFrame, flagship, coordinate, piDirections, [iDistance](engine::GridCoord neighbour)
				{
					return engine::GridDistance(engine::kOriginCoordinate, neighbour) > iDistance;
				});
				if (iCount == 0)
				{
					ePickBehaviour = FleetBehaviour::kRandomWalk;
				}
			}
			else if (rFleet.eBehaviour == FleetBehaviour::kStayInCell)
			{
				if (coordinate == rFleet.holdCoordinate)
				{
					rFleet.cellChangeTimerSeconds = rFleet.navigationDelaySeconds;
					continue;
				}
				// Where the origin is the only strictly closer neighbour, each equal-distance neighbour has a strictly closer
				// one that is not the origin, so the equal-distance step carries the flagship around the origin.
				engine::GridCoord holdCoordinate = rFleet.holdCoordinate;
				int64_t iDistance = engine::GridDistance(holdCoordinate, coordinate);
				iCount = ReachableNeighbourDirections(rFlagshipFrame, flagship, coordinate, piDirections, [holdCoordinate, iDistance](engine::GridCoord neighbour)
				{
					return engine::GridDistance(holdCoordinate, neighbour) < iDistance;
				});
				if (iCount == 0)
				{
					iCount = ReachableNeighbourDirections(rFlagshipFrame, flagship, coordinate, piDirections, [holdCoordinate, iDistance](engine::GridCoord neighbour)
					{
						return engine::GridDistance(holdCoordinate, neighbour) == iDistance;
					});
				}
			}

			// The random walk draws over every direction but the origin's, so away from the origin it draws over all six in
			// order, and the refusal below tests the drawn direction. An Explore or Stay in Cell candidate already passed
			// both refusal tests.
			if (ePickBehaviour == FleetBehaviour::kRandomWalk)
			{
				for (int64_t i = 0; i < common::kiHexDirectionCount; ++i)
				{
					const common::HexDirection& rDirection = engine::kCellDirections.pEntries[i];
					engine::GridCoord neighbour {};
					if (engine::TryAddGridCoordinate(coordinate, rDirection.iDeltaQ, rDirection.iDeltaR, neighbour) && neighbour == engine::kOriginCoordinate)
					{
						continue;
					}
					piDirections[iCount++] = i;
				}
			}
			int64_t iDirection = iCount > 0 ? piDirections[common::Random(iCount - 1, rRandom)] : -1;

			engine::GridCoord destination {};
			if (iDirection < 0
			 || !engine::TryAddGridCoordinate(coordinate, engine::kCellDirections.pEntries[iDirection].iDeltaQ, engine::kCellDirections.pEntries[iDirection].iDeltaR, destination)
			 || !FlagshipReachesEdge(rFlagshipFrame, flagship, gpGame->mCells.at(coordinate).staticData, iDirection)) [[unlikely]]
			{
				// A flagship at a numeric coordinate edge has no neighbour that way, rather than wrapping to the far side of
				// the grid, and one with no reachable open stretch of that edge is sealed from it, so this move never happens;
				// a Stay in Cell flagship with no candidate has no move at all. wantedCoordinate already equals the flagship
				// coord, so resetting the timer alone spends a full navigationDelaySeconds before the next draw instead of
				// redrawing a direction every tick.
				rFleet.cellChangeTimerSeconds = rFleet.navigationDelaySeconds;
				LOG(kNetwork, kDebug, "FleetNavigationController::TickFleetTimers refused move FleetGuid: ({},{}) Coord: ({},{}) Direction: {}", rFleet.guid.uiHigh, rFleet.guid.uiLow, rFlagship.coordinate.iQ, rFlagship.coordinate.iR, iDirection);
				continue;
			}
			int64_t iPendingTicks = engine::kiTickRate;
			rFleet.wantedCoordinate = destination;
			rFleet.uiPendingFleetWantedCoordinateTicks = static_cast<uint8_t>(iPendingTicks);
			rFleet.cellChangeTimerSeconds = rFleet.navigationDelaySeconds;

			// Playback takes every frame change from the recording.
			if (gpGame->mbReplaying)
			{
				continue;
			}

			int64_t iMembersUpdated = 0;
			for (const FleetMember& rMember : rFleet.members)
			{
				if (rMember.flags & FleetMemberFlags::kIsDead)
				{
					continue;
				}

				engine::GridCoord memberCoordinate = rMember.coordinate;

				ShipUuid shipUuid = ShipUuidByGlobalId(*gpGame->mCells.at(memberCoordinate).pCurrent, rMember.globalId);
				ASSERT(shipUuid.iUuid != 0);
				gpGame->mFrameInputs.at(memberCoordinate).statusChanges.push_back(
				{
					.eType = StatusChangeType::kUpdateFleet,
					.data = UpdateFleetData
					{
						.eType = shipUuid.eType,
						.iUuid = shipUuid.iUuid,
						.fleetWantedCoordinate = destination,
						.uiPendingFleetWantedCoordinateTicks = static_cast<uint8_t>(iPendingTicks),
					},
				});
				++iMembersUpdated;
			}
			LOG(kNetwork, kDebug, "FleetNavigationController::TickFleetTimers Guid: ({},{}) FleetGuid: ({},{}) Behaviour: {} Direction: {} WantedCoord: ({},{}) MembersUpdated: {}", rGuid.uiHigh, rGuid.uiLow, rFleet.guid.uiHigh, rFleet.guid.uiLow, kpcFleetBehaviourNames[std::to_underlying(ePickBehaviour)], iDirection, destination.iQ, destination.iR, iMembersUpdated);
		}
	}
}

#endif // BT_SERVER

} // namespace game
