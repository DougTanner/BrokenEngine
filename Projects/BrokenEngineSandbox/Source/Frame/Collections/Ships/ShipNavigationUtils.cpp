#include "ShipNavigationUtils.h"

#include "Frame/CellStaticData.h"
#include "Frame/IslandTerrain.h"
#include "Frame/NavQuery.h"

#include "Frame/Collections/Ships/ShipBehaviours.h"
#include "Frame/Collections/Ships/ShipQueries.h"
#include "Frame/TerrainUtils.h"

namespace game
{

// Flagship follow
constexpr float kfFlagshipFollowDistanceSquared = 50.0f * 50.0f;
constexpr float kfFlagshipCloseDistanceSquared = 12.5f * 12.5f;

// Cadence recomputations are staggered by persisted global ID.
constexpr int64_t kiNavigationRecomputeInterval = 16;

constexpr float kfNavigationLookahead = 8.0f;

// A stored patrol point lies this far inside the cell, so an Omni overshooting it stays in the cell: the 10 m arrival
// radius plus kfOmniMaximumSpeed over kfOmniDrag times the lowest decay multiplier is 57.6 m. Assumes the next tick's
// pick succeeds.
constexpr float kfOmniPatrolCellInset = 60.0f;

// Returns the index into centredIslands, the same index in the cell's full list, for the next navigation waypoint:
// 0 -> largest-area centred island, 1 -> smallest-area, 2+ -> the supplied random pick.
// Largest/smallest scan centredIslands deterministically (no RNG, first-found tie-break);
// area uses the same anisotropic quad footprint the destination point is generated from.
static int64_t SelectIslandPlacement(std::span<const engine::IslandPlacement> centredIslands, int64_t iWaypointIndex, int64_t iRandomPick)
{
	if (iWaypointIndex >= 2)
	{
		return iRandomPick;
	}

	bool bWantLargest = (iWaypointIndex == 0);
	int64_t iSelected = 0;
	float fSelectedArea = -1.0f;
	for (const auto& [j, rPlacement] : std::views::enumerate(centredIslands))
	{
		const engine::IslandTemplate& rTemplate = engine::gpIslandTerrain->mIslands.at(rPlacement.islandCrc);
		float fArea = rTemplate.fQuadFootprintX * rTemplate.fQuadFootprintY;
		if (fSelectedArea < 0.0f || (bWantLargest ? (fArea > fSelectedArea) : (fArea < fSelectedArea)))
		{
			fSelectedArea = fArea;
			iSelected = j;
		}
	}
	return iSelected;
}

// A point uniform in the fRadius disc around vecCenter in x/y, at the base height: two draws, the angle first.
static XMVECTOR XM_CALLCONV OmniPatrolDiscPoint(FXMVECTOR vecCenter, float fRadius, common::RandomEngine& rRandomEngine)
{
	float fAngle = common::Random<XM_2PI>(rRandomEngine);
	float fDistance = fRadius * std::sqrt(common::Random<1.0f>(rRandomEngine));
	common::SinCos rotation = common::DeterministicSinCos(fAngle);
	return XMVectorSetZ(XMVectorAdd(vecCenter, XMVectorSet(rotation.fCos * fDistance, rotation.fSin * fDistance, 0.0f, 0.0f)), kfBaseHeightMeters);
}

// Patrol Together: a squadron's leader is its lowest-global-ID non-dying, non-healing Omni in the previous frame whose
// orders carry kPatrolCell and kPatrolTogether, the squadron named by the flagship and kSquadron1. True, with the
// leader's destination, when that leader is another row.
static bool FindPatrolTogetherLeaderDestination(const Frame& rPreviousFrame, engine::GlobalId flagshipGlobalId, ShipOrders_t orders, engine::GlobalId globalId, XMVECTOR& rVecLeaderDestination)
{
	const OmnisPostRender& rOmnis = *rPreviousFrame.postRender.pOmnis;
	int64_t iLeader = -1;
	for (int64_t i = 0; i < rOmnis.iCount; ++i)
	{
		ShipOrders_t rowOrders = rOmnis.pOrders[i];
		if (rOmnis.pFlagshipGlobalIds[i] != flagshipGlobalId || (rowOrders & ShipOrders::kSquadron1) != (orders & ShipOrders::kSquadron1))
		{
			continue;
		}
		if (!(rowOrders & ShipOrders::kPatrolCell) || !(rowOrders & ShipOrders::kPatrolTogether))
		{
			continue;
		}
		if (rOmnis.pLifecycleFlags[i] & engine::LifecycleFlags::kDying)
		{
			continue;
		}
		if (rOmnis.pFlags[i] & ShipFlags::kHealing)
		{
			continue;
		}
		if (iLeader < 0 || rOmnis.pGlobalIds[i].iValue < rOmnis.pGlobalIds[iLeader].iValue)
		{
			iLeader = i;
		}
	}
	if (iLeader < 0)
	{
		return false;
	}
	if (rOmnis.pGlobalIds[iLeader] == globalId)
	{
		return false;
	}
	rVecLeaderDestination = rOmnis.pVecIslandDestinations[iLeader];
	return true;
}

// A non-flagship in its fleet's wanted cell follows its own flagship, resolved by global ID on the previous frame. A
// flagship in another cell, or none, leaves the follow state unchanged.
static void XM_CALLCONV FollowFlagship(const Frame& rPreviousFrame, FXMVECTOR vecPosition, engine::GlobalId flagshipGlobalId, int64_t& riNavigationDirection, XMVECTOR& rVecIslandDestination)
{
	std::optional<ShipLocator> optionalFlagship = FindShipByGlobalId(rPreviousFrame, flagshipGlobalId);
	if (!optionalFlagship)
	{
		return;
	}

	XMVECTOR vecFlagshipPosition = ShipPosition(rPreviousFrame.interpolate, *optionalFlagship);
	// Horizontal, so a raised wingman can still close in on its flagship.
	float fDistanceSquared = XMVectorGetX(XMVector2LengthSq(XMVectorSubtract(vecPosition, vecFlagshipPosition)));

	if (fDistanceSquared > kfFlagshipFollowDistanceSquared)
	{
		riNavigationDirection = 7;
		rVecIslandDestination = XMVectorSetW(vecFlagshipPosition, 1.0f);
	}
	else if (riNavigationDirection == 7)
	{
		rVecIslandDestination = XMVectorSetW(vecFlagshipPosition, 1.0f);
		if (fDistanceSquared < kfFlagshipCloseDistanceSquared)
		{
			riNavigationDirection = 6;
			rVecIslandDestination = XMVectorZero();
		}
	}
}

static void XM_CALLCONV UpdateFleetAndFlagshipNavigation(const Frame& __restrict rPreviousFrame, ShipLocator self, engine::GlobalId globalId, FXMVECTOR vecPosition, const engine::CellStaticData& rStaticData, engine::GridCoord fleetWantedCoordinate, int64_t iPendingFleetWantedCoordinateTicks, ShipOrders_t orders, engine::GlobalId flagshipGlobalId, std::chrono::duration<float> deltaTime, int64_t& riNavigationDirection, XMVECTOR& rVecIslandDestination, std::chrono::duration<float>& rCellChangeTimer)
{
	// Fleet navigation: navigate toward fleet's wanted coord after countdown expires
	if (!(fleetWantedCoordinate == rStaticData.coordinate) && iPendingFleetWantedCoordinateTicks == 0)
	{
		// Two coords anywhere in the signed-int32 identity range can separate by more than int32 holds, so each converts to int64 ahead of the subtraction.
		int64_t iDeltaQ = static_cast<int64_t>(fleetWantedCoordinate.iQ) - static_cast<int64_t>(rStaticData.coordinate.iQ);
		int64_t iDeltaR = static_cast<int64_t>(fleetWantedCoordinate.iR) - static_cast<int64_t>(rStaticData.coordinate.iR);

		// The most direct neighbour: the largest integer cube-coordinate dot product with the delta, which is proportional
		// to the dot product of the center offsets; an exact tie goes to the first entry in table order. A pure function
		// of the two coordinates, so fleets sharing an origin and destination share a route.
		int64_t iPickedDirection = 0;
		int64_t iBestDot = std::numeric_limits<int64_t>::min();
		for (int64_t k = 0; k < common::kiHexDirectionCount; ++k)
		{
			const common::HexDirection& rDirection = engine::kCellDirections.pEntries[k];
			int64_t iDot = iDeltaQ * rDirection.iDeltaQ + (iDeltaQ + iDeltaR) * (rDirection.iDeltaQ + rDirection.iDeltaR) + iDeltaR * rDirection.iDeltaR;
			if (iDot > iBestDot)
			{
				iBestDot = iDot;
				iPickedDirection = k;
			}
		}

		if (iPickedDirection != riNavigationDirection)
		{
			riNavigationDirection = iPickedDirection;
			rVecIslandDestination = XMVectorZero();
			LOG(kNavData, kVerbose, "{} {} GlobalId: {} fleet override NavDir: {} WantedCoord: ({},{}) CellCoord: ({},{})", ShipTypeName(self.eType), self.iRow, globalId, riNavigationDirection, fleetWantedCoordinate.iQ, fleetWantedCoordinate.iR, rStaticData.coordinate.iQ, rStaticData.coordinate.iR);
		}
	}

	if (!(orders & ShipOrders::kIsFlagship) && (fleetWantedCoordinate == rStaticData.coordinate))
	{
		FollowFlagship(rPreviousFrame, vecPosition, flagshipGlobalId, riNavigationDirection, rVecIslandDestination);
	}

	// Cell change timer: cycle back to island destination when roaming
	if (riNavigationDirection == -1)
	{
		rCellChangeTimer -= deltaTime;
		if (rCellChangeTimer.count() <= 0.0f)
		{
			riNavigationDirection = 6;
		}
	}
}

static bool ShouldRecomputeNavigation(const Frame& rFrame, engine::GlobalId globalId, FXMVECTOR vecPosition, FXMVECTOR vecAiDirection, int64_t iEntryNavigationDirection, int64_t iNavigationDirection, const engine::NavData& rNavData)
{
	// Cadence, mode changes, and blocked position/lookahead probes request pathfinding; otherwise cached steering is
	// reused without NavQueryDirection.
	bool bRecompute = (iNavigationDirection != iEntryNavigationDirection)
	               || (((rFrame.interpolate.iTick + globalId.iValue) % kiNavigationRecomputeInterval) == 0);

	// The throttle carries the shared world-space bearing for up to kiNavigationRecomputeInterval ticks. Recompute when
	// either the kfNavigationLookahead point or current position is inside a nav polygon: lookahead alone misses ships
	// already stranded inside. kNavigationGraphSpecs uses the base flight height minus the ship's clearance-tier radius and
	// kfPushMargin, for an Omni (tier 0) the height where its ApplyTerrainPush starts; the nav polygon inflates that
	// contour, leaving a no-nav band with zero terrain push. vecAiDirection, vecPosition, and server-built, wire-shipped
	// navigationData match on client/server, and this check draws no RNG. Roam mode -1 is exempt because ComputeAiSteering
	// re-steers every tick.
	if (!bRecompute && iNavigationDirection >= 0)
	{
		XMVECTOR vecLookahead = XMVectorMultiplyAdd(XMVectorReplicate(kfNavigationLookahead), vecAiDirection, vecPosition);
		if (engine::NavQueryPointBlocked(vecLookahead, rNavData) || engine::NavQueryPointBlocked(vecPosition, rNavData))
		{
			bRecompute = true;
		}
	}

	return bRecompute;
}

static void XM_CALLCONV RecomputeNavigationPath([[maybe_unused]] Frame& rFrame, [[maybe_unused]] ShipLocator self, FXMVECTOR vecPosition, FXMVECTOR vecDestination, const engine::NavData& rNavData, NavRectangleBoard& rNavRectangleBoard, XMVECTOR& rVecAiDirection)
{
	XMVECTOR vecDebugWaypoint = XMVectorZero();
	// A raised ship can sit above the destination's height; at the ship's own Z the query delta, and so the direction,
	// stays horizontal.
	XMVECTOR vecNavigationDirection = engine::NavQueryDirection(vecPosition, XMVectorSetZ(vecDestination, XMVectorGetZ(vecPosition)), rNavData, rNavRectangleBoard.Select(vecPosition, vecDestination), vecDebugWaypoint);
	if (XMVectorGetX(XMVector3LengthSq(vecNavigationDirection)) > 0.001f)
	{
		rVecAiDirection = vecNavigationDirection;
	}
	else
	{
		AimAiDirection(XMVectorSubtract(vecDestination, vecPosition), rVecAiDirection);
	}
#if defined(BT_CLIENT)
	if constexpr (kbDebugRender)
	{
		VisitShip(rFrame, self, [&]([[maybe_unused]] const auto& rInterpolate, auto& rPostRender, int64_t iRow)
		{
			rPostRender.pVecDebugNavigationWaypoints[iRow] = vecDebugWaypoint;
		});
	}
#endif // BT_CLIENT
}

void XM_CALLCONV ComputeNavigation(Frame& rFrame, const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, ShipLocator self, FXMVECTOR vecPosition, NavRectangleBoard& rNavRectangleBoard, engine::GridCoord fleetWantedCoordinate, int64_t iPendingFleetWantedCoordinateTicks, ShipOrders_t orders, int64_t& riNavigationDirection, int64_t& riNavigationWaypointIndex, XMVECTOR& rVecAiDirection, XMVECTOR& rVecIslandDestination, std::chrono::duration<float>& rCellChangeTimer)
{
	engine::GlobalId globalId = VisitShip(rFrame, self, []([[maybe_unused]] const auto& rInterpolate, const auto& rPostRender, int64_t iRow)
	{
		return rPostRender.pGlobalIds[iRow];
	});
	// Only Spawn writes pFlagshipGlobalIds, so the previous frame's value is the row's current one.
	engine::GlobalId flagshipGlobalId = VisitShip(rPreviousFrame, self, []([[maybe_unused]] const auto& rInterpolate, const auto& rPostRender, int64_t iRow)
	{
		return rPostRender.pFlagshipGlobalIds[iRow];
	});
	std::chrono::duration<float> deltaTime(rFrame.interpolate.fDeltaTime);

	// Cell center: every cell's local frame is centered on the origin.
	// W=1.0 keeps this a proper position — every downstream (cellCenter - vecPosition) and neighbour offset add stays W-clean,
	// so normalize fallbacks don't leak W into the AI direction and on into velocity.
	XMVECTOR vecCellCenter = XMVectorSet(0.0f, 0.0f, kfBaseHeightMeters, 1.0f);

	// Throttle bookkeeping: capture mode before the fleet/flagship/timer blocks can change it; a mode change forces an
	// immediate pathfind below (see bRecompute).
	int64_t iEntryNavigationDirection = riNavigationDirection;

	const engine::NavData& rNavData = NavigationTierData(rStaticData, ShipTypeClearanceTier(self.eType));

	// A fleet Omni with its flagship in the cell patrols in mode 6, around the flagship (Escort) or across the cell (Patrol
	// Cell), preempting fleet navigation as the in-cell fleet tactic does; running the fleet override first would reset
	// the mode and destination every tick, and without the forced mode FollowFlagship would pull it back to the flagship.
	std::optional<ShipLocator> optionalPatrolFlagship;
	if (self.eType == ShipType::kOmni)
	{
		optionalPatrolFlagship = FindShipByGlobalId(rPreviousFrame, flagshipGlobalId);
	}
	if (optionalPatrolFlagship)
	{
		riNavigationDirection = 6;
	}
	else
	{
		UpdateFleetAndFlagshipNavigation(rPreviousFrame, self, globalId, vecPosition, rStaticData, fleetWantedCoordinate, iPendingFleetWantedCoordinateTicks, orders, flagshipGlobalId, deltaTime, riNavigationDirection, rVecIslandDestination, rCellChangeTimer);
	}

	bool bRecompute = ShouldRecomputeNavigation(rFrame, globalId, vecPosition, rVecAiDirection, iEntryNavigationDirection, riNavigationDirection, rNavData);

	if (riNavigationDirection == 7)
	{
		// A missing flagship leaves FollowFlagship without a target; the fleet flagship is its Battleship, which never
		// dies, so a follower stalls until that row is back in this cell or the fleet override moves it on.
		// Agent-injected ships have no Fleet updates. Mode 7 follows without arrival or missing-flagship fallback and
		// mirrors mode 6's three entry draws (island pick, footprint X/Y) each tick with centred islands; a fleet Omni
		// never enters mode 7. Every Random call advances once regardless of bound.
		std::span<const engine::IslandPlacement> centredIslands = engine::CentredIslands(rStaticData.islands);
		if (!centredIslands.empty())
		{
			common::Random(std::ssize(centredIslands) - 1, rFrame.postRender.randomEngine);
			const engine::IslandPlacement& rRandomPlacement = rStaticData.islands.at(static_cast<size_t>(self.iRow % std::ssize(centredIslands)));
			const engine::IslandTemplate& rRandomTemplate = engine::gpIslandTerrain->mIslands.at(rRandomPlacement.islandCrc);
			common::Random(rRandomTemplate.fQuadFootprintX, rFrame.postRender.randomEngine);
			common::Random(rRandomTemplate.fQuadFootprintY, rFrame.postRender.randomEngine);
		}

		if (bRecompute)
		{
			RecomputeNavigationPath(rFrame, self, vecPosition, rVecIslandDestination, rNavData, rNavRectangleBoard, rVecAiDirection);
		}
	}
	else if (riNavigationDirection == 6)
	{
		// OmnisPostRender::Update passes a healing row's orders without Patrol Cell, so it escorts and is neither leader nor
		// follower.
		bool bPatrolCell = optionalPatrolFlagship && (orders & ShipOrders::kPatrolCell);
		XMVECTOR vecLeaderDestination = XMVectorZero();
		bool bPatrolFollower = bPatrolCell && (orders & ShipOrders::kPatrolTogether) && FindPatrolTogetherLeaderDestination(rPreviousFrame, flagshipGlobalId, orders, globalId, vecLeaderDestination);

		// Navigate to island destination
		if (bPatrolFollower)
		{
			// A Patrol Together follower draws nothing: it takes its leader's previous-frame destination whenever that is
			// set and differs, and skips the blocked-destination snap and the arrival reset, since a snapped or cleared copy
			// would differ from the leader's and be recopied and repathed every tick. With no destination of its own and none
			// from the leader it keeps its cached steering, as a dropped pick does.
			if (XMVectorGetW(vecLeaderDestination) != 0.0f && !XMVector4Equal(vecLeaderDestination, rVecIslandDestination))
			{
				rVecIslandDestination = vecLeaderDestination;
				bRecompute = true;
			}
			else if (XMVectorGetW(rVecIslandDestination) == 0.0f)
			{
				bRecompute = false;
			}
		}
		else if (XMVectorGetW(rVecIslandDestination) == 0.0f)
		{
			// A patrolling fleet Omni draws a point per attempt: in Escort two draws in the kfOmniPatrolRadius disc around
			// its flagship; in Patrol Cell the island pick then two footprint draws on a random centred island, or with
			// none two draws in the disc around the cell center reaching the hexagon's corners. Any other ship's waypoints
			// pick the largest, smallest, then random centred island by quad-footprint area, drawing the pick before two
			// footprint draws; with no centred island they target the cell center. A patrol point outside the cell inset,
			// or an island point or snap past the cell edge (a footprint can pass it), is dropped and the next tick
			// redraws; a dropped largest or smallest pick still advances, and the tick keeps its cached steering since a
			// recompute would aim at the zero destination.
			std::span<const engine::IslandPlacement> centredIslands = engine::CentredIslands(rStaticData.islands);
			if (optionalPatrolFlagship)
			{
				XMVECTOR vecPatrolPoint = XMVectorZero();
				if (!bPatrolCell)
				{
					vecPatrolPoint = OmniPatrolDiscPoint(ShipPosition(rPreviousFrame.interpolate, *optionalPatrolFlagship), kfOmniPatrolRadius, rFrame.postRender.randomEngine);
				}
				else if (centredIslands.empty())
				{
					vecPatrolPoint = OmniPatrolDiscPoint(vecCellCenter, engine::kfCellHalfWidth, rFrame.postRender.randomEngine);
				}
				else
				{
					int64_t iPlacement = common::Random(std::ssize(centredIslands) - 1, rFrame.postRender.randomEngine);
					const engine::IslandPlacement& rPlacement = rStaticData.islands.at(static_cast<size_t>(iPlacement));
					const engine::IslandTemplate& rTemplate = engine::gpIslandTerrain->mIslands.at(rPlacement.islandCrc);
					float fX = rPlacement.f2WorldPosition.x - 0.5f * rTemplate.fQuadFootprintX + common::Random(rTemplate.fQuadFootprintX, rFrame.postRender.randomEngine);
					float fY = rPlacement.f2WorldPosition.y - 0.5f * rTemplate.fQuadFootprintY + common::Random(rTemplate.fQuadFootprintY, rFrame.postRender.randomEngine);
					vecPatrolPoint = XMVectorSet(fX, fY, kfBaseHeightMeters, 1.0f);
				}
				vecPatrolPoint = engine::NavQuerySnapToNavigable(vecPatrolPoint, rNavData);
				if (common::IsInsideHex(vecPatrolPoint, engine::kCellDirections, kfOmniPatrolCellInset))
				{
					rVecIslandDestination = vecPatrolPoint;
					bRecompute = true;
				}
				else
				{
					bRecompute = false;
				}
			}
			else if (centredIslands.empty())
			{
				rVecIslandDestination = vecCellCenter;
				bRecompute = true;
			}
			else
			{
				int64_t iRandomPick = common::Random(std::ssize(centredIslands) - 1, rFrame.postRender.randomEngine);
				int64_t iPlacement = SelectIslandPlacement(centredIslands, riNavigationWaypointIndex, iRandomPick);
				if (riNavigationWaypointIndex < 2)
				{
					++riNavigationWaypointIndex;
				}
				const engine::IslandPlacement& rPlacement = rStaticData.islands.at(static_cast<size_t>(iPlacement));
				const engine::IslandTemplate& rTemplate = engine::gpIslandTerrain->mIslands.at(rPlacement.islandCrc);
				float fIslandMinimumX = rPlacement.f2WorldPosition.x - 0.5f * rTemplate.fQuadFootprintX;
				float fIslandMinimumY = rPlacement.f2WorldPosition.y - 0.5f * rTemplate.fQuadFootprintY;

				float fX = fIslandMinimumX + common::Random(rTemplate.fQuadFootprintX, rFrame.postRender.randomEngine);
				float fY = fIslandMinimumY + common::Random(rTemplate.fQuadFootprintY, rFrame.postRender.randomEngine);
				rVecIslandDestination = XMVectorSet(fX, fY, kfBaseHeightMeters, 1.0f);

				// Snap to navigable area if inside an obstacle
				rVecIslandDestination = engine::NavQuerySnapToNavigable(rVecIslandDestination, rNavData);
				if (engine::IsInsideCell(rVecIslandDestination))
				{
					LOG(kNavData, kVerbose, "{} {} NavSwitch: island dest generated pos={} dest={}", ShipTypeName(self.eType), self.iRow, common::WbV2(vecPosition, 1), common::WbV2(rVecIslandDestination, 1));
					bRecompute = true; // new destination -> path immediately
				}
				else
				{
					rVecIslandDestination = XMVectorZero();
					bRecompute = false;
				}
			}
		}
		else if (engine::NavQueryPointBlocked(rVecIslandDestination, rNavData))
		{
			// A stored destination inside this tier's navigation polygons would stop the path at the polygon edge, so the
			// arrival test would pass only when that edge lies within its radius of the destination.
			// NavQueryDirection already snaps its destination to this same point, so the snap needs no immediate recompute.
			// A polygon can pass the cell edge; a snap past it is dropped as a fresh pick's is, and the next tick draws again.
			rVecIslandDestination = engine::NavQuerySnapToNavigable(rVecIslandDestination, rNavData);
			if (!engine::IsInsideCell(rVecIslandDestination))
			{
				rVecIslandDestination = XMVectorZero();
				bRecompute = false;
			}
		}

		if (bRecompute)
		{
			RecomputeNavigationPath(rFrame, self, vecPosition, rVecIslandDestination, rNavData, rNavRectangleBoard, rVecAiDirection);
		}

		// Arrival check (unconditional — independent of the pathfind throttle). Zeroing the destination
		// makes the next tick's mode-6 entry block immediately pick the next island in the
		// largest -> smallest -> random sequence (no idle between islands). Staying in mode 6 keeps the
		// tour progressing; the fleet still leaves the cell when the server cell-change timer fires.
		// Horizontal, so a ship above the destination's height can still arrive.
		float fDistanceSquared = XMVectorGetX(XMVector2LengthSq(XMVectorSubtract(vecPosition, rVecIslandDestination)));
		if (!bPatrolFollower && fDistanceSquared < 100.0f)
		{
			LOG(kNavData, kVerbose, "{} {} NavSwitch: arrived at island dest pos={} dest={}", ShipTypeName(self.eType), self.iRow, common::WbV2(vecPosition, 1), common::WbV2(rVecIslandDestination, 1));
			rVecIslandDestination = XMVectorZero();
		}
	}
	else if (riNavigationDirection >= 0)
	{
		// Navigate toward neighboring cell center
		rVecIslandDestination = XMVectorZero();
		XMVECTOR vecDestination = vecCellCenter;
		if (riNavigationDirection < common::kiHexDirectionCount)
		{
			vecDestination = XMVectorAdd(vecCellCenter, XMLoadFloat2(&engine::kCellDirections.pEntries[riNavigationDirection].f2CenterOffset));
		}

		if (bRecompute)
		{
			RecomputeNavigationPath(rFrame, self, vecPosition, vecDestination, rNavData, rNavRectangleBoard, rVecAiDirection);
		}
	}
	else
	{
		rVecIslandDestination = XMVectorZero();
		auto [vecNewAiDirection] = ComputeAiSteering(rStaticData, vecPosition, rVecAiDirection, vecCellCenter, deltaTime.count(), self.iRow % 2 == 0);
		rVecAiDirection = vecNewAiDirection;
#if defined(BT_CLIENT)
		if constexpr (kbDebugRender)
		{
			VisitShip(rFrame, self, []([[maybe_unused]] const auto& rInterpolate, auto& rPostRender, int64_t iRow)
			{
				rPostRender.pVecDebugNavigationWaypoints[iRow] = XMVectorZero();
			});
		}
#endif // BT_CLIENT
	}
}

} // namespace game
