#include "ShipBehaviours.h"

#include "Frame/Collections/Pushers/Pushers.h"
#include "Frame/CellStaticData.h"
#include "Frame/FlightPlanes.h"
#include "Frame/IslandTerrain.h"
#include "Frame/NavQuery.h"
#if defined(BT_CLIENT)
#include "Graphics/Debug/DebugRender.h"
#endif

#include "Frame/Collections/Ships/ShipQueries.h"
#include "Frame/TerrainUtils.h"

namespace game
{

// Terrain collision
constexpr float kfTerrainPushVelocity = 15.0f;
constexpr float kfMaximumPushVelocity = 20.0f;

// Flagship follow
constexpr float kfFlagshipFollowDistanceSquared = 50.0f * 50.0f;
constexpr float kfFlagshipCloseDistanceSquared = 12.5f * 12.5f;

// Cadence recomputations are staggered by persisted global ID.
constexpr int64_t kiNavigationRecomputeInterval = 16;

constexpr float kfNavigationLookahead = 8.0f;

// Flee/return hysteresis
constexpr float kfFleeStartDistance = 15.0f;
constexpr float kfFleeEndDistance = 25.0f;
constexpr float kfReturnDistance = 180.0f;
constexpr float kfReturnedDistance = kfReturnDistance - 20.0f;

constexpr float kfTerrainBounceRotation = 8.0f;
constexpr float kfTerrainBounceVelocity = 4.0f;

constexpr float kfHealthRegeneration = 0.1f;
constexpr float kfHealthRegenerationDistance = 60.0f;

int64_t HighestFlightPlane(ShipType eType)
{
	switch (eType)
	{
		case ShipType::kFighter:    return kiFighterHighestFlightPlane;
		case ShipType::kOmni:       return kiOmniHighestFlightPlane;
		case ShipType::kBattleship: return kiBattleshipHighestFlightPlane;
		case ShipType::kCount:      break;
	}
	ASSERT(false);
	std::unreachable();
}

bool IsFlightPlaneAllowed(ShipType eType, int64_t iFlightPlane)
{
	return iFlightPlane >= 0 && iFlightPlane <= HighestFlightPlane(eType);
}

std::optional<int64_t> FlightPlaneAtHeight(float fHeight)
{
	for (int64_t i = 0; i < engine::gpFlightPlanes->Count(); ++i)
	{
		if (engine::gpFlightPlanes->Plane(i).fHeightMeters == fHeight)
		{
			return i;
		}
	}
	return std::nullopt;
}

int64_t NavigationFlightPlane(float fHeight, int64_t iTargetPlane)
{
	// Heights ascend, and a ship's Z never drops below the lowest plane, so plane 0 is the floor.
	int64_t iBelow = 0;
	for (int64_t i = 1; i < engine::gpFlightPlanes->Count() && engine::gpFlightPlanes->Plane(i).fHeightMeters <= fHeight; ++i)
	{
		iBelow = i;
	}
	return std::min(iBelow, iTargetPlane);
}

bool SkipsTerrainContact(float fHeight)
{
	std::optional<int64_t> optionalFlightPlane = FlightPlaneAtHeight(fHeight);
	return optionalFlightPlane && !(engine::gpFlightPlanes->Plane(*optionalFlightPlane).flags & engine::FlightPlaneFlags::kTerrainCollision);
}

XMVECTOR XM_CALLCONV ClimbTowardsFlightPlane(FXMVECTOR vecPosition, int64_t iTargetFlightPlane, float fClimbRate, std::chrono::duration<float> deltaTime)
{
	// Fixed operation order and the table's own height on arrival keep client and server bit-identical, which
	// FlightPlaneAtHeight's exact equality relies on.
	float fTargetHeight = engine::gpFlightPlanes->Plane(iTargetFlightPlane).fHeightMeters;
	float fClimbStep = fClimbRate * deltaTime.count();
	float fHeight = XMVectorGetZ(vecPosition);
	float fHeightGap = fTargetHeight - fHeight;
	if (fHeightGap > fClimbStep)
	{
		fHeight += fClimbStep;
	}
	else if (fHeightGap < -fClimbStep)
	{
		fHeight -= fClimbStep;
	}
	else
	{
		fHeight = fTargetHeight;
	}
	return XMVectorSetZ(vecPosition, fHeight);
}

void CountdownPendingOrders(int64_t& riPendingFleetWantedCoordinateTicks)
{
	if (riPendingFleetWantedCoordinateTicks > 0)
	{
		riPendingFleetWantedCoordinateTicks--;
	}
}

void CountdownPendingOrders(ShipOrders_t& rOrders, int64_t& riPendingFleetWantedCoordinateTicks, int64_t& riPendingWeaponModeTicks)
{
	CountdownPendingOrders(riPendingFleetWantedCoordinateTicks);
	if (riPendingWeaponModeTicks > 0)
	{
		riPendingWeaponModeTicks--;
		if (riPendingWeaponModeTicks == 0)
		{
			if (rOrders & ShipOrders::kPendingUseMissiles)
			{
				rOrders.Set(ShipOrders::kUseMissiles);
			}
			else
			{
				rOrders.Set(ShipOrders::kUseMissiles, false);
			}
		}
	}
}

// Returns the index into rStaticData.islands for the next navigation waypoint:
// 0 -> largest-area island in this frame, 1 -> smallest-area, 2+ -> the supplied random pick.
// Largest/smallest scan the frame's placements deterministically (no RNG, first-found tie-break);
// area uses the same anisotropic quad footprint the destination point is generated from.
static int64_t SelectIslandPlacement(const engine::CellStaticData& rStaticData, int64_t iWaypointIndex, int64_t iRandomPick)
{
	if (iWaypointIndex >= 2)
	{
		return iRandomPick;
	}

	bool bWantLargest = (iWaypointIndex == 0);
	int64_t iSelected = 0;
	float fSelectedArea = -1.0f;
	for (int64_t j = 0; j < std::ssize(rStaticData.islands); ++j)
	{
		const engine::IslandTemplate& rTemplate = engine::gpIslandTerrain->mIslands.at(rStaticData.islands.at(static_cast<size_t>(j)).islandCrc);
		float fArea = rTemplate.fQuadFootprintX * rTemplate.fQuadFootprintY;
		if (fSelectedArea < 0.0f || (bWantLargest ? (fArea > fSelectedArea) : (fArea < fSelectedArea)))
		{
			fSelectedArea = fArea;
			iSelected = j;
		}
	}
	return iSelected;
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
	// Horizontal, so a wingman on another flight plane than its flagship can still close in.
	float fDistanceSquared = XMVectorGetX(XMVector2LengthSq(XMVectorSubtract(vecPosition, vecFlagshipPosition)));

	if (fDistanceSquared > kfFlagshipFollowDistanceSquared)
	{
		riNavigationDirection = 5;
		rVecIslandDestination = XMVectorSetW(vecFlagshipPosition, 1.0f);
	}
	else if (riNavigationDirection == 5)
	{
		rVecIslandDestination = XMVectorSetW(vecFlagshipPosition, 1.0f);
		if (fDistanceSquared < kfFlagshipCloseDistanceSquared)
		{
			riNavigationDirection = 4;
			rVecIslandDestination = XMVectorZero();
		}
	}
}

static void XM_CALLCONV UpdateFleetAndFlagshipNavigation(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, ShipLocator self, engine::GlobalId globalId, FXMVECTOR vecPosition, const engine::CellStaticData& rStaticData, engine::GridCoord fleetWantedCoordinate, int64_t iPendingFleetWantedCoordinateTicks, ShipOrders_t orders, engine::GlobalId flagshipGlobalId, std::chrono::duration<float> deltaTime, int64_t& riNavigationDirection, XMVECTOR& rVecIslandDestination, std::chrono::duration<float>& rCellChangeTimer)
{
	// Fleet navigation: navigate toward fleet's wanted coord after countdown expires
	if (!(fleetWantedCoordinate == rStaticData.coordinate) && iPendingFleetWantedCoordinateTicks == 0)
	{
		// Two coords anywhere in the signed-int32 identity range can separate by more than int32 holds, so each converts to int64 ahead of the subtraction.
		int64_t iDeltaX = static_cast<int64_t>(fleetWantedCoordinate.iX) - static_cast<int64_t>(rStaticData.coordinate.iX);
		int64_t iDeltaY = static_cast<int64_t>(fleetWantedCoordinate.iY) - static_cast<int64_t>(rStaticData.coordinate.iY);

		// Check if already heading in a valid direction toward wanted coord
		bool bAlreadyValid = false;
		if (riNavigationDirection >= 0 && riNavigationDirection <= 3)
		{
			switch (riNavigationDirection)
			{
				case 0: bAlreadyValid = iDeltaY > 0; break;
				case 1: bAlreadyValid = iDeltaY < 0; break;
				case 2: bAlreadyValid = iDeltaX > 0; break;
				case 3: bAlreadyValid = iDeltaX < 0; break;
				default: break;
			}
		}

		if (!bAlreadyValid)
		{
			int64_t iRandom = static_cast<int64_t>(common::Random(3i64, rFrame.postRender.randomEngine));
			if (iDeltaX != 0 && iDeltaY != 0)
			{
				riNavigationDirection = (iRandom < 2)
					? (iDeltaY > 0 ? 0 : 1)
					: (iDeltaX > 0 ? 2 : 3);
			}
			else if (iDeltaY != 0)
			{
				riNavigationDirection = iDeltaY > 0 ? 0 : 1;
			}
			else
			{
				riNavigationDirection = iDeltaX > 0 ? 2 : 3;
			}
			rVecIslandDestination = XMVectorZero();
			LOG(kNavData, kVerbose, "{} {} GlobalId: {} fleet override NavDir: {} WantedCoord: ({},{}) CellCoord: ({},{})", ShipTypeName(self.eType), self.iRow, globalId, riNavigationDirection, fleetWantedCoordinate.iX, fleetWantedCoordinate.iY, rStaticData.coordinate.iX, rStaticData.coordinate.iY);
		}
	}

	if (!(orders & ShipOrders::kIsFlagship) && (fleetWantedCoordinate == rStaticData.coordinate))
	{
		FollowFlagship(rPreviousFrame, vecPosition, flagshipGlobalId, riNavigationDirection, rVecIslandDestination);
	}

	if ((orders & ShipOrders::kIsFlagship) && riNavigationDirection == 5 && (fleetWantedCoordinate == rStaticData.coordinate))
	{
		riNavigationDirection = 4;
		rVecIslandDestination = XMVectorZero();
	}

	// Cell change timer: cycle back to island destination when roaming
	if (riNavigationDirection == -1)
	{
		rCellChangeTimer -= deltaTime;
		if (rCellChangeTimer.count() <= 0.0f)
		{
			riNavigationDirection = 4;
		}
	}
}

static bool ShouldRecomputeNavigation(const Frame& rFrame, engine::GlobalId globalId, FXMVECTOR vecPosition, FXMVECTOR vecArtificialIntelligenceDirection, int64_t iEntryNavigationDirection, bool bReseededDirection, int64_t iNavigationDirection, const engine::NavData& rNavData)
{
	// Cadence, mode changes, direction reseeding, and blocked position/lookahead probes request pathfinding; otherwise
	// cached steering is reused without NavQueryDirection.
	bool bRecompute = bReseededDirection || (iNavigationDirection != iEntryNavigationDirection)
	               || (((rFrame.interpolate.iTick + globalId.iValue) % kiNavigationRecomputeInterval) == 0);

	// The throttle carries the shared world-space bearing for up to kiNavigationRecomputeInterval ticks. Recompute when
	// either the kfNavigationLookahead point or current position is inside a nav polygon: lookahead alone misses ships
	// already stranded inside. NavigationThresholdElevation, like the Omni ApplyTerrainPush on the navigated flight plane,
	// uses that plane's height minus kfOmniRadius and kfPushMargin; the nav polygon inflates that contour, leaving a no-nav band
	// with zero terrain push. rVecArtificialIntelligenceDirection, vecPosition, and server-built, wire-shipped navigationData match on
	// client/server, and this check draws no RNG. Roam mode -1 is exempt because ComputeArtificialIntelligenceSteering re-steers
	// every tick.
	if (!bRecompute && iNavigationDirection >= 0 && XMVectorGetX(XMVector3LengthSq(vecArtificialIntelligenceDirection)) > 0.001f)
	{
		XMVECTOR vecLookahead = XMVectorMultiplyAdd(XMVectorReplicate(kfNavigationLookahead), vecArtificialIntelligenceDirection, vecPosition);
		if (engine::NavQueryPointBlocked(vecLookahead, rNavData) || engine::NavQueryPointBlocked(vecPosition, rNavData))
		{
			bRecompute = true;
		}
	}

	return bRecompute;
}

static void XM_CALLCONV RecomputeNavigationPath([[maybe_unused]] Frame& rFrame, [[maybe_unused]] ShipLocator self, FXMVECTOR vecPosition, FXMVECTOR vecDestination, const engine::NavData& rNavData, XMVECTOR& rVecArtificialIntelligenceDirection)
{
	XMVECTOR vecDebugWaypoint = XMVectorZero();
	// Destinations can sit on another plane's height; at the ship's own Z the query delta, and so the direction, stays
	// horizontal.
	XMVECTOR vecNavigationDirection = engine::NavQueryDirection(vecPosition, XMVectorSetZ(vecDestination, XMVectorGetZ(vecPosition)), rNavData, vecDebugWaypoint);
	if (XMVectorGetX(XMVector3LengthSq(vecNavigationDirection)) > 0.001f)
	{
		rVecArtificialIntelligenceDirection = vecNavigationDirection;
	}
	else
	{
		rVecArtificialIntelligenceDirection = XMVector3Normalize(XMVectorSetZ(XMVectorSubtract(vecDestination, vecPosition), 0.0f));
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

// A client cell whose static data has not arrived holds no graphs; it navigates with an empty graph.
static const engine::NavData& NavigationPlaneData(const engine::CellStaticData& rStaticData, int64_t iNavigationPlane)
{
	static const engine::NavData sEmptyNavData;
	return (iNavigationPlane < std::ssize(rStaticData.navigationData)) ? rStaticData.navigationData.at(static_cast<size_t>(iNavigationPlane)) : sEmptyNavData;
}

void XM_CALLCONV ComputeNavigation(Frame& rFrame, const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, ShipLocator self, FXMVECTOR vecPosition, FXMVECTOR vecCellCenter, engine::GridCoord fleetWantedCoordinate, int64_t iPendingFleetWantedCoordinateTicks, ShipOrders_t orders, engine::GlobalId flagshipGlobalId, std::chrono::duration<float> deltaTime, int64_t& riNavigationDirection, int64_t& riNavigationWaypointIndex, XMVECTOR& rVecArtificialIntelligenceDirection, XMVECTOR& rVecIslandDestination, std::chrono::duration<float>& rCellChangeTimer)
{
	engine::GlobalId globalId = VisitShip(rFrame, self, []([[maybe_unused]] const auto& rInterpolate, const auto& rPostRender, int64_t iRow)
	{
		return rPostRender.pGlobalIds[iRow];
	});

	// Throttle bookkeeping: capture mode before the fleet/flagship/timer blocks can change it, and note
	// when the direction is re-seeded. Both force an immediate pathfind below (see bRecompute).
	int64_t iEntryNavigationDirection = riNavigationDirection;
	bool bReseededDirection = false;

	// Initialize direction if zero (first spawn or after reset)
	if (XMVectorGetX(XMVector3LengthSq(rVecArtificialIntelligenceDirection)) < 0.001f)
	{
		float fAngle = common::Random<XM_2PI>(rFrame.postRender.randomEngine);
		rVecArtificialIntelligenceDirection = XMVector4Transform(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMMatrixRotationZ(fAngle));
		bReseededDirection = true;
	}

	int64_t iNavigationPlane = NavigationFlightPlane(XMVectorGetZ(vecPosition), GetTargetFlightPlane(orders));
	const engine::NavData& rNavData = NavigationPlaneData(rStaticData, iNavigationPlane);

	UpdateFleetAndFlagshipNavigation(rFrame, rPreviousFrame, self, globalId, vecPosition, rStaticData, fleetWantedCoordinate, iPendingFleetWantedCoordinateTicks, orders, flagshipGlobalId, deltaTime, riNavigationDirection, rVecIslandDestination, rCellChangeTimer);

	bool bRecompute = ShouldRecomputeNavigation(rFrame, globalId, vecPosition, rVecArtificialIntelligenceDirection, iEntryNavigationDirection, bReseededDirection, riNavigationDirection, rNavData);

	if (riNavigationDirection == 5)
	{
		// A missing flagship leaves FollowFlagship without a target. OnOwnedEntityDeath shifts the flagship after removal;
		// ProcessFlagshipUpdates queues kUpdateFleet, whose Update handler writes kIsFlagship and the flagship global ID.
		// Followers normally stall one tick before replacement or fleet override. A same-cell promoted flagship
		// recovers on D+2 via mode 4, clearing its stale destination without advancing the rally timer.
		// Agent-injected ships have no Fleet updates. Mode 5 follows without arrival or missing-flagship fallback.
		// It consumes three draws (island pick, footprint X/Y) each tick with islands; mode 4 draws only when
		// selecting a destination. Both skip draws without islands. Every Random call advances once regardless of bound.
		if (!rStaticData.islands.empty())
		{
			common::Random(std::ssize(rStaticData.islands) - 1, rFrame.postRender.randomEngine);
			const engine::IslandPlacement& rRandomPlacement = rStaticData.islands.at(static_cast<size_t>(self.iRow % std::ssize(rStaticData.islands)));
			const engine::IslandTemplate& rRandomTemplate = engine::gpIslandTerrain->mIslands.at(rRandomPlacement.islandCrc);
			common::Random(rRandomTemplate.fQuadFootprintX, rFrame.postRender.randomEngine);
			common::Random(rRandomTemplate.fQuadFootprintY, rFrame.postRender.randomEngine);
		}

		if (bRecompute)
		{
			RecomputeNavigationPath(rFrame, self, vecPosition, rVecIslandDestination, rNavData, rVecArtificialIntelligenceDirection);
		}
	}
	else if (riNavigationDirection == 4)
	{
		// Navigate to island destination
		if (XMVectorGetW(rVecIslandDestination) == 0.0f)
		{
			// Waypoints select the largest, smallest, then random island by quad-footprint area.
			// All waypoint indices consume the island-pick draw before the two footprint draws, keeping each
			// destination selection at three draws. The sampled point lies in the selected island's quad-footprint
			// AABB; island-free cells skip draws and target the cell center.
			if (rStaticData.islands.empty())
			{
				rVecIslandDestination = vecCellCenter;
				bRecompute = true;
			}
			else
			{
				int64_t iRandomPick = common::Random(std::ssize(rStaticData.islands) - 1, rFrame.postRender.randomEngine);
				int64_t iPlacement = SelectIslandPlacement(rStaticData, riNavigationWaypointIndex, iRandomPick);
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
				rVecIslandDestination = XMVectorSet(fX, fY, engine::gpFlightPlanes->Lowest().fHeightMeters, 1.0f);

				// Snap to navigable area if inside an obstacle
				rVecIslandDestination = engine::NavQuerySnapToNavigable(rVecIslandDestination, rNavData);
				LOG(kNavData, kVerbose, "{} {} NavSwitch: island dest generated pos={} dest={}", ShipTypeName(self.eType), self.iRow, common::WbV2(vecPosition, 1), common::WbV2(rVecIslandDestination, 1));
				bRecompute = true; // new destination -> path immediately
			}
		}
		else if (engine::NavQueryPointBlocked(rVecIslandDestination, rNavData))
		{
			// A destination snapped on a higher plane's graph can lie inside this plane's polygons once the navigation
			// plane drops; left there, the path stops at the polygon edge, so the arrival test passes only when that edge lies
			// within its radius of the destination.
			// NavQueryDirection already snaps its destination to this same point, so the snap needs no immediate recompute.
			rVecIslandDestination = engine::NavQuerySnapToNavigable(rVecIslandDestination, rNavData);
		}

		if (bRecompute)
		{
			RecomputeNavigationPath(rFrame, self, vecPosition, rVecIslandDestination, rNavData, rVecArtificialIntelligenceDirection);
		}

		// Arrival check (unconditional — independent of the pathfind throttle). Zeroing the destination
		// makes the next tick's mode-4 entry block immediately pick the next island in the
		// largest -> smallest -> random sequence (no idle between islands). Staying in mode 4 keeps the
		// tour progressing; the fleet still leaves the cell when the server cell-change timer fires.
		// Horizontal, so a ship above the destination's height can still arrive.
		float fDistanceSquared = XMVectorGetX(XMVector2LengthSq(XMVectorSubtract(vecPosition, rVecIslandDestination)));
		if (fDistanceSquared < 100.0f)
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
		switch (riNavigationDirection)
		{
			case 0:
				vecDestination = XMVectorAdd(vecCellCenter, XMVectorSet(0.0f, engine::kfCellHeight, 0.0f, 0.0f));
				break;
			case 1:
				vecDestination = XMVectorAdd(vecCellCenter, XMVectorSet(0.0f, -engine::kfCellHeight, 0.0f, 0.0f));
				break;
			case 2:
				vecDestination = XMVectorAdd(vecCellCenter, XMVectorSet(engine::kfCellWidth, 0.0f, 0.0f, 0.0f));
				break;
			case 3:
				vecDestination = XMVectorAdd(vecCellCenter, XMVectorSet(-engine::kfCellWidth, 0.0f, 0.0f, 0.0f));
				break;
			default:
				break;
		}

		if (bRecompute)
		{
			RecomputeNavigationPath(rFrame, self, vecPosition, vecDestination, rNavData, rVecArtificialIntelligenceDirection);
		}
	}
	else
	{
		rVecIslandDestination = XMVectorZero();
		auto [vecNewArtificialIntelligenceDirection] = ComputeArtificialIntelligenceSteering(rStaticData, vecPosition, rVecArtificialIntelligenceDirection, vecCellCenter, deltaTime.count(), self.iRow % 2 == 0, iNavigationPlane);
		rVecArtificialIntelligenceDirection = vecNewArtificialIntelligenceDirection;
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

XMVECTOR XM_CALLCONV ComputeFleeOrReturnDestination(std::span<const XMFLOAT4> islandCandidates, FXMVECTOR vecPosition, bool bEnemyFound, FXMVECTOR vecNearestEnemy, ShipFlags_t& rFlags, bool& rbHasDestination)
{
	XMVECTOR vecToEnemy = bEnemyFound ? XMVectorSubtract(vecNearestEnemy, vecPosition) : XMVectorZero();
	float fEnemyDistance = bEnemyFound ? XMVectorGetX(XMVector3Length(vecToEnemy)) : kfFleeEndDistance + 1.0f;
	if (fEnemyDistance < kfFleeStartDistance)
	{
		rFlags.Set(ShipFlags::kFlee);
	}
	else if (fEnemyDistance > kfFleeEndDistance)
	{
		rFlags.Set(ShipFlags::kFlee, false);
	}

	if (islandCandidates.empty())
	{
		rFlags.Set(ShipFlags::kReturnToIslandCenter, false);
		rbHasDestination = bEnemyFound;
		return vecNearestEnemy;
	}

	// Return-to-island steering selects the nearest center from candidates precomputed once per coord in Update.
	// The tick-invariant candidates use the placement's x/y, the lowest flight plane's height, and w=1 for every ship.
	XMVECTOR vecIslandCenter = XMLoadFloat4(&islandCandidates[0]);
	float fNearestDistanceSquared = std::numeric_limits<float>::max();
	for (const XMFLOAT4& rf4Candidate : islandCandidates)
	{
		XMVECTOR vecCandidate = XMLoadFloat4(&rf4Candidate);
		float fDistanceSquared = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(vecCandidate, vecPosition)));
		if (fDistanceSquared < fNearestDistanceSquared)
		{
			fNearestDistanceSquared = fDistanceSquared;
			vecIslandCenter = vecCandidate;
		}
	}
	float fDistanceFromIslandCenter = common::Distance(vecPosition, vecIslandCenter);
	if (fDistanceFromIslandCenter > kfReturnDistance)
	{
		rFlags.Set(ShipFlags::kReturnToIslandCenter);
	}
	else if (fDistanceFromIslandCenter < kfReturnedDistance)
	{
		rFlags.Set(ShipFlags::kReturnToIslandCenter, false);
	}

	rbHasDestination = true;
	XMVECTOR vecDestination = bEnemyFound ? vecNearestEnemy : vecIslandCenter;
	if (rFlags & ShipFlags::kReturnToIslandCenter)
	{
		vecDestination = vecIslandCenter;
	}
	return vecDestination;
}

void XM_CALLCONV SteerHeadingTowards(FXMVECTOR vecPosition, FXMVECTOR vecDirection, FXMVECTOR vecDestination, bool bHasDestination, float fTurnRate, float fSmoothing, float fDecay, std::chrono::duration<float> deltaTime, float& rfDeltaRotation)
{
	if (bHasDestination)
	{
		XMVECTOR vecToDestinationNormal = XMVector3Normalize(XMVectorSubtract(vecDestination, vecPosition));
		float fDirectionDestinationCrossZ = XMVectorGetZ(XMVector3Cross(vecDirection, vecToDestinationNormal));
		float fWantedDeltaRotation = fDirectionDestinationCrossZ > 0.0f ? fTurnRate : -fTurnRate;
		rfDeltaRotation = std::lerp(fWantedDeltaRotation, rfDeltaRotation, common::ExponentialDecay(fSmoothing, deltaTime.count()));
	}
	rfDeltaRotation = common::ExponentialDecay(fDecay, deltaTime.count()) * rfDeltaRotation;
}

void XM_CALLCONV ApplyAcceleratedMovement(int64_t iNavigationDirection, FXMVECTOR vecAiDirection, std::chrono::duration<float> deltaTime, float fAccelerationMultiplier, float fDecayMultiplier, const AcceleratedMovementConstants& rConstants, XMVECTOR& rVecVelocity)
{
	float fAcceleration = (iNavigationDirection == 5 ? rConstants.fCatchUpAcceleration : rConstants.fAcceleration) * fAccelerationMultiplier;
	float fMaximumSpeed = iNavigationDirection == 5 ? rConstants.fCatchUpMaximumSpeed : rConstants.fMaximumSpeed;
	rVecVelocity = engine::ApplyMovement(rVecVelocity, vecAiDirection, deltaTime.count(), fAcceleration, rConstants.fDrag * fDecayMultiplier, fMaximumSpeed);
}

void XM_CALLCONV ApplyHeadingMovement(FXMVECTOR vecDirection, ShipFlags_t flags, std::chrono::duration<float> deltaTime, const HeadingMovementConstants& rConstants, XMVECTOR& rVecVelocity)
{
	float fAcceleration = flags & ShipFlags::kReturnToIslandCenter ? rConstants.fReturnAcceleration
	                    : flags & ShipFlags::kFlee ? rConstants.fFleeAcceleration
	                    : rConstants.fChaseAcceleration;
	rVecVelocity = engine::ApplyMovement<true>(rVecVelocity, vecDirection, deltaTime.count(), fAcceleration, rConstants.fDrag, rConstants.fMaximumSpeed, rConstants.fVelocityToDirection);
}

void XM_CALLCONV ApplyTerrainPush(const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, float fRadius, float fHeight, float fPushMargin, XMVECTOR& rVecVelocity)
{
	// Terrain collision - add velocity away from terrain, gentle at first then ramping up
	float fElevation = engine::gpIslandTerrain->MakeCellElevationSampler(rStaticData).Sample(vecPosition);
	float fPushHeight = fHeight - fRadius - fPushMargin;
	if (fElevation >= fPushHeight) [[unlikely]]
	{
		XMVECTOR vecTerrainNormal = XMVector3Normalize(XMVectorSetZ(engine::gpIslandTerrain->CellNormal(rStaticData, vecPosition), 0.0f));
		float fPenetration = fElevation - fPushHeight;
		float fPushStrength = fPenetration * fPenetration * kfTerrainPushVelocity;
		rVecVelocity = engine::ApplyClampedPush(rVecVelocity, vecTerrainNormal, fPushStrength, kfMaximumPushVelocity);
	}
}

// The graph of the plane a ship at fHeight navigates. Callers skip terrain contact on a plane without
// kTerrainCollision, and kFlightPlanes clears that flag only on its highest plane, so the navigated plane carries it.
static const engine::NavData& TerrainContactNavData(const engine::CellStaticData& rStaticData, float fHeight, int64_t iTargetFlightPlane)
{
	int64_t iNavigationPlane = NavigationFlightPlane(fHeight, iTargetFlightPlane);
	ASSERT(engine::gpFlightPlanes->Plane(iNavigationPlane).flags & engine::FlightPlaneFlags::kTerrainCollision);
	return NavigationPlaneData(rStaticData, iNavigationPlane);
}

void XM_CALLCONV ResolveTerrainContact(const engine::CellStaticData& rStaticData, ShipLocator self, FXMVECTOR vecPreviousPosition, XMVECTOR& rVecPosition, float fRadius, XMVECTOR& rVecVelocity, int64_t iTargetFlightPlane)
{
	// Hard contact at the sweep end's height; ApplyTerrainPush is the softer band in front of it.
	float fHeight = XMVectorGetZ(rVecPosition);
	engine::DiscTerrainResult result = engine::ResolveDiscAgainstTerrain(rStaticData, TerrainContactNavData(rStaticData, fHeight, iTargetFlightPlane), vecPreviousPosition, rVecPosition, fRadius, fHeight, true);
	if (result.flags & engine::DiscTerrainFlags::kStartOverlap) [[unlikely]]
	{
		LOG(kDefault, kWarning, "{} terrain start overlap Cell: ({},{}) Index: {} Start: {} Moved: {}", ShipTypeName(self.eType), rStaticData.coordinate.iX, rStaticData.coordinate.iY, self.iRow, common::WbV2(vecPreviousPosition, 1), common::Wb(result.fStartOverlapDistance, 1));
	}

	rVecPosition = result.vecPosition;
	if (result.flags & engine::DiscTerrainFlags::kContact) [[unlikely]]
	{
		float fVelocityIntoNormal = std::min(0.0f, XMVectorGetX(XMVector3Dot(rVecVelocity, result.vecNormal)));
		rVecVelocity = XMVectorSubtract(rVecVelocity, XMVectorScale(result.vecNormal, fVelocityIntoNormal));
	}
}

void XM_CALLCONV ApplyTerrainBounce(const engine::CellStaticData& rStaticData, ShipLocator self, FXMVECTOR vecPreviousPosition, XMVECTOR& rVecPosition, FXMVECTOR vecDirection, float fRadius, std::chrono::duration<float> deltaTime, float& rfDeltaRotation, XMVECTOR& rVecVelocity, int64_t iTargetFlightPlane)
{
	float fHeight = XMVectorGetZ(rVecPosition);
	engine::DiscTerrainResult result = engine::ResolveDiscAgainstTerrain(rStaticData, TerrainContactNavData(rStaticData, fHeight, iTargetFlightPlane), vecPreviousPosition, rVecPosition, fRadius, fHeight, false);
	if (result.flags & engine::DiscTerrainFlags::kStartOverlap) [[unlikely]]
	{
		LOG(kDefault, kWarning, "{} terrain start overlap Cell: ({},{}) Index: {} Start: {} Moved: {}", ShipTypeName(self.eType), rStaticData.coordinate.iX, rStaticData.coordinate.iY, self.iRow, common::WbV2(vecPreviousPosition, 1), common::Wb(result.fStartOverlapDistance, 1));
	}

	// The resolved position carries W=1.0 — this write bypasses the main integration clamp.
	rVecPosition = result.vecPosition;
	if (result.flags & engine::DiscTerrainFlags::kContact) [[unlikely]]
	{
		float fDirectionTerrainCrossZ = XMVectorGetZ(XMVector3Cross(vecDirection, result.vecNormal));
		rfDeltaRotation = fDirectionTerrainCrossZ > 0.0f ? kfTerrainBounceRotation : -kfTerrainBounceRotation;

		// Velocity W stays exactly 0 because ResolveDiscAgainstTerrain returns the contact normal with W=0.
		rVecVelocity = XMVector3Reflect(rVecVelocity, result.vecNormal);
		rVecVelocity = XMVectorMultiplyAdd(XMVectorReplicate(deltaTime.count() * kfTerrainBounceVelocity), result.vecNormal, rVecVelocity);
	}
}

void XM_CALLCONV ApplyPusherPush(const Frame& rFrame, FXMVECTOR vecPosition, engine::pusher_t pusher, float fMaximumPushVelocity, XMVECTOR& rVecVelocity)
{
	XMVECTOR vecPush = engine::PushersInterpolate::ApplyPush(rFrame.interpolate, vecPosition, pusher);
	float fPushLength = XMVectorGetX(XMVector3Length(vecPush));
	if (fPushLength > 0.0f)
	{
		XMVECTOR vecPushDirection = XMVectorDivide(vecPush, XMVectorReplicate(fPushLength));
		rVecVelocity = engine::ApplyClampedPush(rVecVelocity, vecPushDirection, fPushLength, fMaximumPushVelocity);
	}
}

void XM_CALLCONV AcquireTarget(const Frame& rPreviousFrame, FXMVECTOR vecPosition, engine::AlignmentIdentifier alignment, float fBlasterSpeed, float fBlasterTargetRange, float fMissileTargetRange, ShipFlags_t& rFlags, bool& rbLookTargetFound, XMVECTOR& rVecLookPosition)
{
	bool bTargetInRange = false;
	bool bTargetForLook = false;
	XMVECTOR vecTargetInRangePosition = XMVectorZero();
	XMVECTOR vecTargetInRangeVelocity = XMVectorZero();
	XMVECTOR vecTargetForLookPosition = XMVectorZero();
	float fBestInRange = fMissileTargetRange;
	float fBestForLook = std::numeric_limits<float>::max();
	// Firing and look targets require a visible enemy past arrival grace.
	ForEachEnemyShip(rPreviousFrame.interpolate, rPreviousFrame.postRender, alignment, [&]([[maybe_unused]] ShipLocator locator, FXMVECTOR vecTargetPosition, FXMVECTOR vecTargetVelocity)
	{
		if (!FrameInterpolate::IsVisible(vecPosition, vecTargetPosition))
		{
			return;
		}
		float fDistance = common::Distance(vecPosition, vecTargetPosition);
		if (fDistance < fBestInRange)
		{
			fBestInRange = fDistance;
			vecTargetInRangePosition = vecTargetPosition;
			vecTargetInRangeVelocity = vecTargetVelocity;
			bTargetInRange = true;
		}
		if (fDistance < fBestForLook)
		{
			fBestForLook = fDistance;
			vecTargetForLookPosition = vecTargetPosition;
			bTargetForLook = true;
		}
	});

	rbLookTargetFound = false;
	rVecLookPosition = XMVectorZero();
	if (bTargetInRange)
	{
		rVecLookPosition = common::ComputeLeadPosition(vecPosition, vecTargetInRangePosition, vecTargetInRangeVelocity, fBlasterSpeed);
		rbLookTargetFound = true;
	}
	else if (bTargetForLook)
	{
		rVecLookPosition = vecTargetForLookPosition;
		rbLookTargetFound = true;
	}

	// Weapon spawn timers rate-limit continuous fire; blasters require the shorter target range.
	if (bTargetInRange)
	{
		rFlags.Set(ShipFlags::kFireMissile);
		if (common::Distance(vecPosition, vecTargetInRangePosition) <= fBlasterTargetRange)
		{
			rFlags.Set(ShipFlags::kFireBlaster);
		}
	}
}

void RegenerateShield(std::chrono::duration<float> deltaTime, std::chrono::duration<float> shieldCooldown, float fMaximumShield, float fRegeneration, float& rfShield)
{
	if (shieldCooldown.count() <= 0.0f)
	{
		rfShield = std::min(rfShield + deltaTime.count() * fRegeneration, fMaximumShield);
	}
}

void XM_CALLCONV RegenerateHealth(FXMVECTOR vecPosition, bool bEnemyFound, FXMVECTOR vecNearestEnemy, std::chrono::duration<float> deltaTime, float fMaximumHealth, float& rfHealth)
{
	if (bEnemyFound && common::Distance(vecPosition, vecNearestEnemy) > kfHealthRegenerationDistance) [[unlikely]]
	{
		rfHealth = std::min(rfHealth + deltaTime.count() * kfHealthRegeneration, fMaximumHealth);
	}
}

#if defined(BT_CLIENT)
void XM_CALLCONV DebugRenderNavigation(const FrameInterpolate& rFrameInterpolate, const Frame& rRenderFrame, FXMVECTOR vecPosition, FXMVECTOR vecDebugWaypoint, FXMVECTOR vecIslandDestination, ShipFlags_t flags, engine::GlobalId flagshipGlobalId, float fRadius)
{
	// Every position here is local to the rendered cell, so each one converts into the camera cell's frame where it is
	// handed to the debug renderer.
	const engine::RenderBasis& rBasis = rFrameInterpolate.renderBasis;
	int64_t iNavigationDirection = GetNavigationDirection(flags);
	bool bFlagshipFound = false;
	XMVECTOR vecFlagshipPosition = XMVectorZero();
	if (iNavigationDirection == 5)
	{
		std::optional<ShipLocator> optionalFlagship = FindShipByGlobalId(rRenderFrame, flagshipGlobalId);
		if (optionalFlagship)
		{
			vecFlagshipPosition = ShipPosition(rFrameInterpolate, *optionalFlagship);
			bFlagshipFound = true;
		}
	}

	if (XMVectorGetW(vecDebugWaypoint) > 0.0f && (iNavigationDirection != 5 || bFlagshipFound))
	{
		// In mode 5 (flagship follow), use the flagship's interpolated position as the waypoint
		XMVECTOR vecWaypoint = (iNavigationDirection == 5) ? vecFlagshipPosition : vecDebugWaypoint;
		XMFLOAT3A f3NavigationStart {};
		XMFLOAT3A f3NavigationEnd {};
		XMStoreFloat3A(&f3NavigationStart, engine::Rebase(rBasis, vecPosition));
		XMStoreFloat3A(&f3NavigationEnd, engine::Rebase(rBasis, XMVectorSetZ(vecWaypoint, engine::gpFlightPlanes->Lowest().fHeightMeters)));
		engine::DebugRender::Line(f3NavigationStart, f3NavigationEnd, {0.0f, 1.0f, 0.0f, 1.0f});
		engine::DebugRender::Circle(f3NavigationEnd, fRadius * 0.5f, {0.0f, 1.0f, 0.0f, 1.0f});
	}

	if (iNavigationDirection == 5)
	{
		if (bFlagshipFound)
		{
			XMFLOAT3A f3Destination {};
			XMStoreFloat3A(&f3Destination, engine::Rebase(rBasis, XMVectorSetZ(vecFlagshipPosition, engine::gpFlightPlanes->Lowest().fHeightMeters)));
			engine::DebugRender::Circle(f3Destination, fRadius * 2.0f, {0.0f, 1.0f, 0.0f, 1.0f});
		}
	}
	else if (XMVectorGetW(vecIslandDestination) > 0.0f)
	{
		XMFLOAT3A f3Destination {};
		XMStoreFloat3A(&f3Destination, engine::Rebase(rBasis, XMVectorSetZ(vecIslandDestination, engine::gpFlightPlanes->Lowest().fHeightMeters)));
		engine::DebugRender::Circle(f3Destination, fRadius * 2.0f, {0.0f, 1.0f, 0.0f, 1.0f});
	}
}
#endif // BT_CLIENT

} // namespace game
