#include "ShipBehaviours.h"

#include "Frame/Collections/Pushers/Pushers.h"
#include "Frame/CellStaticData.h"
#include "Frame/FlightPlanes.h"
#include "Frame/IslandTerrain.h"
#include "Frame/NavQuery.h"
#if defined(BT_CLIENT)
#include "Graphics/Debug/DebugRender.h"
#endif

#include "Frame/Collections/Ships/ShipHitboxes.h"
#include "Frame/Collections/Ships/ShipQueries.h"
#include "Frame/TerrainUtils.h"
#include "Profile/ProfileRows.h"

namespace game
{

// Terrain collision
constexpr float kfTerrainPushVelocity = 15.0f;
constexpr float kfMaximumPushVelocity = 20.0f;

// A rectangle's A* corners stand this far outside its tested box, so a leg round the box never touches it.
constexpr float kfNavRectangleCornerMargin = 0.5f;
// A rectangle counts for a query within this multiple of both ships' summed radii; a farther one moves before arrival.
constexpr float kfNavRectangleReachScale = 4.0f;

// Return hysteresis
constexpr float kfReturnDistance = 180.0f;
constexpr float kfReturnedDistance = kfReturnDistance - 20.0f;

constexpr float kfTerrainBounceRotation = 8.0f;
constexpr float kfTerrainBounceVelocity = 4.0f;

int64_t HighestFlightPlane(ShipType eType)
{
	return kShipTypeHighestFlightPlanes.at(std::to_underlying(eType));
}

int64_t DefaultFlightPlane(ShipType eType)
{
	return eType == ShipType::kOmni ? 0 : HighestFlightPlane(eType);
}

bool IsFlightPlaneAllowed(ShipType eType, int64_t iFlightPlane)
{
	return iFlightPlane >= 0 && iFlightPlane <= HighestFlightPlane(eType);
}

float EffectiveShipFlightHeight(ShipType eType, ShipController eController, int64_t iFlightPlane)
{
	return std::max(engine::gpFlightPlanes->Plane(iFlightPlane).fHeightMeters, gShipHitboxShapes[std::to_underlying(eType)][std::to_underlying(eController)].fMinimumOriginZ);
}

static std::optional<int64_t> FlightPlaneAtHeight(float fHeight)
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

int64_t XM_CALLCONV NearestFlightPlane(FXMVECTOR vecPosition)
{
	return engine::gpFlightPlanes->NearestPlaneIndex(XMVectorGetZ(vecPosition));
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

bool TakesDeathTerrainContact(float fHeight, int64_t iTargetPlane)
{
	return !SkipsTerrainContact(fHeight)
	    && (engine::gpFlightPlanes->Plane(NavigationFlightPlane(fHeight, iTargetPlane)).flags & engine::FlightPlaneFlags::kTerrainCollision);
}

const engine::NavData& NavigationPlaneData(const engine::CellStaticData& rStaticData, int64_t iClearanceTier, int64_t iNavigationPlane)
{
	static const engine::NavData sEmptyNavData;
	int64_t iGraph = NavigationGraphIndex(iClearanceTier, iNavigationPlane);
	return (iGraph >= 0 && iGraph < std::ssize(rStaticData.navigationData)) ? rStaticData.navigationData.at(static_cast<size_t>(iGraph)) : sEmptyNavData;
}

float StepHeightTowards(float fHeight, float fTargetHeight, float fStep, bool& rbArrived)
{
	float fHeightGap = fTargetHeight - fHeight;
	rbArrived = false;
	if (fHeightGap > fStep)
	{
		return fHeight + fStep;
	}
	if (fHeightGap < -fStep)
	{
		return fHeight - fStep;
	}
	rbArrived = true;
	return fTargetHeight;
}

XMVECTOR XM_CALLCONV ClimbTowardsFlightPlane(const engine::CellStaticData& rStaticData, ShipType eType, ShipController eController, FXMVECTOR vecPosition, int64_t iTargetFlightPlane, float fClimbRate, std::chrono::duration<float> deltaTime, uint8_t& ruiFlightPlane)
{
	// Fixed operation order and the exact effective height on arrival keep client and server bit-identical.
	float fTargetHeight = EffectiveShipFlightHeight(eType, eController, iTargetFlightPlane);
	float fClimbStep = fClimbRate * deltaTime.count();
	float fHeight = XMVectorGetZ(vecPosition);
	if (fTargetHeight < fHeight && engine::NavQueryPointBlocked(vecPosition, NavigationPlaneData(rStaticData, ShipTypeClearanceTier(eType), iTargetFlightPlane)))
	{
		iTargetFlightPlane += 1;
		fTargetHeight = EffectiveShipFlightHeight(eType, eController, iTargetFlightPlane);
	}
	bool bArrived = false;
	fHeight = StepHeightTowards(fHeight, fTargetHeight, fClimbStep, bArrived);
	if (ruiFlightPlane != iTargetFlightPlane)
	{
		ruiFlightPlane = bArrived ? static_cast<uint8_t>(iTargetFlightPlane) : kuiNoFlightPlane;
	}
	return XMVectorSetZ(vecPosition, fHeight);
}

XMVECTOR XM_CALLCONV RaiseAboveTerrain(const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, uint8_t& ruiFlightPlane)
{
	float fMinimumHeight = engine::gpIslandTerrain->MakeCellElevationSampler(rStaticData).Sample(vecPosition) + engine::gpFlightPlanes->Lowest().fHeightMeters;
	if (XMVectorGetZ(vecPosition) >= fMinimumHeight)
	{
		return vecPosition;
	}
	ruiFlightPlane = kuiNoFlightPlane;
	return XMVectorSetZ(vecPosition, fMinimumHeight);
}

XMVECTOR XM_CALLCONV RotateByHeading(FXMVECTOR vecHeadingDirection, FXMVECTOR vecOffset, FXMVECTOR vecBase)
{
	float fCos = XMVectorGetX(vecHeadingDirection);
	float fSin = XMVectorGetY(vecHeadingDirection);
	float fX = XMVectorGetX(vecOffset);
	float fY = XMVectorGetY(vecOffset);
	return XMVectorSet((fCos * fX - fSin * fY) + XMVectorGetX(vecBase), (fSin * fX + fCos * fY) + XMVectorGetY(vecBase), 0.0f, 0.0f);
}

void CountdownPendingOrders(int64_t& riPendingFleetWantedCoordinateTicks)
{
	if (riPendingFleetWantedCoordinateTicks > 0)
	{
		riPendingFleetWantedCoordinateTicks--;
	}
}

// Select tests reach per rectangle per query, so the center and half diagonal are cached.
struct NavRectangleBoard::BoardRectangle
{
	engine::NavRectangle rectangle {};
	XMFLOAT2 f2Center {};
	float fHalfDiagonal = 0.0f;
	float fReach = 0.0f;
	int64_t iLowestPlane = 0;
	int64_t iHighestPlane = 0;
};

// OccupiedPlanes reads raw plane heights, so a Battleship at its effective height reads as between planes; the cap at the
// type's highest plane keeps it on its own.
static std::pair<int64_t, int64_t> CappedOccupiedPlanes(float fHeight, ShipType eType)
{
	auto [iLowestPlane, iHighestPlane] = engine::gpFlightPlanes->OccupiedPlanes(fHeight);
	return {iLowestPlane, std::min(iHighestPlane, HighestFlightPlane(eType))};
}

// The handle test is ApplyPusherPush's, so a rectangle exists exactly while its ship's pusher does.
template <typename SHIP_INTERPOLATE, typename SHIP_POST_RENDER>
static bool IsNavRectangleRow(ShipType eType, const SHIP_INTERPOLATE& rInterpolate, const SHIP_POST_RENDER& rPostRender, int64_t iRow)
{
	return rInterpolate.pPushers[iRow].uuid.iValue != 0 && gShipHitboxShapes[std::to_underlying(eType)][std::to_underlying(rPostRender.pController[iRow])].bNavigationObstacle;
}

static int64_t CountNavRectangles(const Frame& rPreviousFrame)
{
	engine::ScopedCpuProfile scopedCpuProfile(kCpuTimerPostRenderUpdateNavigationRectangles);

	int64_t iCount = 0;
	ForEachShipType(rPreviousFrame, [&](ShipType eType, const auto& rInterpolate, const auto& rPostRender)
	{
		for (int64_t i = 0; i < rPostRender.iCount; ++i)
		{
			if (IsNavRectangleRow(eType, rInterpolate, rPostRender, i))
			{
				++iCount;
			}
		}
	});
	return iCount;
}

// The shape's box runs CCW from its minimum corner, so each corner moves out along both axes before posing.
static void PoseInflatedBox(const engine::CollisionHitbox& rShapeHitbox, float fClearance, XMFLOAT2 f2Position, float fCos, float fSin, std::span<XMFLOAT2> outputVertices)
{
	const XMFLOAT2* pf2Box = rShapeHitbox.pf2BoxVertices;
	XMFLOAT2 pf2InflatedBox[4]
	{
		{pf2Box[0].x - fClearance, pf2Box[0].y - fClearance},
		{pf2Box[1].x + fClearance, pf2Box[1].y - fClearance},
		{pf2Box[2].x + fClearance, pf2Box[2].y + fClearance},
		{pf2Box[3].x - fClearance, pf2Box[3].y + fClearance},
	};
	common::BuildWorldHull(pf2InflatedBox, f2Position, fCos, fSin, outputVertices);
}

NavRectangleBoard::NavRectangleBoard(const Frame& rPreviousFrame, ShipType eSelfType)
: meSelfType(eSelfType)
, miRectangleCount(CountNavRectangles(rPreviousFrame))
, mRectangles(common::gpThreadLocal->mWorkbuffer.PushBuffer<BoardRectangle*>(miRectangleCount * static_cast<int64_t>(sizeof(BoardRectangle))))
, mPicked(common::gpThreadLocal->mWorkbuffer.PushBuffer<engine::NavRectangle*>(miRectangleCount * static_cast<int64_t>(sizeof(engine::NavRectangle))))
{
	engine::ScopedCpuProfile scopedCpuProfile(kCpuTimerPostRenderUpdateNavigationRectangles);

	float fClearance = NavigationClearanceMeters(ShipTypeClearanceTier(eSelfType));
	float fCornerClearance = fClearance + kfNavRectangleCornerMargin;
	// One board serves both of the type's controllers, and Select takes no controller.
	const std::array<ShipHitboxShape, kiShipControllerCount>& rSelfShapes = gShipHitboxShapes[std::to_underlying(eSelfType)];
	float fSelfRadius = std::max(rSelfShapes[0].fRadius, rSelfShapes[1].fRadius);

	int64_t iRectangle = 0;
	ForEachShipType(rPreviousFrame, [&](ShipType eType, const auto& rInterpolate, const auto& rPostRender)
	{
		for (int64_t i = 0; i < rPostRender.iCount; ++i)
		{
			if (!IsNavRectangleRow(eType, rInterpolate, rPostRender, i))
			{
				continue;
			}

			const ShipHitboxShape& rShape = gShipHitboxShapes[std::to_underlying(eType)][std::to_underlying(rPostRender.pController[i])];
			XMVECTOR vecPosition = rInterpolate.pVecPositions[i];
			XMVECTOR vecDirection = rInterpolate.pVecDirections[i];
			XMFLOAT2 f2Position {XMVectorGetX(vecPosition), XMVectorGetY(vecPosition)};
			float fCos = XMVectorGetX(vecDirection);
			float fSin = XMVectorGetY(vecDirection);

			BoardRectangle& rRectangle = mRectangles.mpData[iRectangle++];
			PoseInflatedBox(rShape.hitbox, fClearance, f2Position, fCos, fSin, rRectangle.rectangle.pf2Box);
			PoseInflatedBox(rShape.hitbox, fCornerClearance, f2Position, fCos, fSin, rRectangle.rectangle.pf2Corners);
			// The inflated box's half extents are each at most the ship's radius plus the corner clearance, so the penalty is
			// at least the half perimeter a way round costs and a clear way round always wins.
			rRectangle.rectangle.fCrossingPenalty = 4.0f * (rShape.fRadius + fCornerClearance);

			XMFLOAT2 f2Corner = rRectangle.rectangle.pf2Box[0];
			XMFLOAT2 f2OppositeCorner = rRectangle.rectangle.pf2Box[2];
			float fDiagonalX = f2OppositeCorner.x - f2Corner.x;
			float fDiagonalY = f2OppositeCorner.y - f2Corner.y;
			rRectangle.f2Center = {f2Corner.x + 0.5f * fDiagonalX, f2Corner.y + 0.5f * fDiagonalY};
			rRectangle.fHalfDiagonal = 0.5f * std::sqrt(fDiagonalX * fDiagonalX + fDiagonalY * fDiagonalY);
			rRectangle.fReach = kfNavRectangleReachScale * (fSelfRadius + rShape.fRadius);

			auto [iLowestPlane, iHighestPlane] = CappedOccupiedPlanes(XMVectorGetZ(vecPosition), eType);
			rRectangle.iLowestPlane = iLowestPlane;
			rRectangle.iHighestPlane = iHighestPlane;
		}
	});
}

std::span<const engine::NavRectangle> XM_CALLCONV NavRectangleBoard::Select(FXMVECTOR vecPosition, FXMVECTOR vecDestination)
{
	engine::ScopedCpuProfile scopedCpuProfile(kCpuTimerPostRenderUpdateNavigationRectangles);

	auto [iLowestPlane, iHighestPlane] = CappedOccupiedPlanes(XMVectorGetZ(vecPosition), meSelfType);
	XMFLOAT2 f2Position {XMVectorGetX(vecPosition), XMVectorGetY(vecPosition)};
	XMFLOAT2 f2Destination {XMVectorGetX(vecDestination), XMVectorGetY(vecDestination)};

	int64_t iPickedCount = 0;
	for (int64_t i = 0; i < miRectangleCount; ++i)
	{
		const BoardRectangle& rRectangle = mRectangles.mpData[i];
		if (rRectangle.iLowestPlane > iHighestPlane || rRectangle.iHighestPlane < iLowestPlane)
		{
			continue;
		}

		float fOffsetX = rRectangle.f2Center.x - f2Position.x;
		float fOffsetY = rRectangle.f2Center.y - f2Position.y;
		if (std::sqrt(fOffsetX * fOffsetX + fOffsetY * fOffsetY) - rRectangle.fHalfDiagonal > rRectangle.fReach)
		{
			continue;
		}

		if (common::PointInPolygon(f2Position, rRectangle.rectangle.pf2Box) || common::PointInPolygon(f2Destination, rRectangle.rectangle.pf2Box))
		{
			continue;
		}

		mPicked.mpData[iPickedCount++] = rRectangle.rectangle;
	}
	return {mPicked.mpData, static_cast<size_t>(iPickedCount)};
}

XMVECTOR XM_CALLCONV NextRouteWaypoint(const engine::CellStaticData& rStaticData, int64_t iClearanceTier, FXMVECTOR vecPosition, FXMVECTOR vecDestination, NavRectangleBoard& rNavRectangleBoard, int64_t iTargetFlightPlane, bool& rbFinal)
{
	const engine::NavData& rNavData = NavigationPlaneData(rStaticData, iClearanceTier, NavigationFlightPlane(XMVectorGetZ(vecPosition), iTargetFlightPlane));
	XMVECTOR vecLevelDestination = XMVectorSetZ(vecDestination, XMVectorGetZ(vecPosition));
	XMVECTOR vecWaypoint = XMVectorZero();
	engine::NavQueryDirection(vecPosition, vecLevelDestination, rNavData, rNavRectangleBoard.Select(vecPosition, vecLevelDestination), vecWaypoint);
	// NavQueryDirection snaps its destination with the same polygon snap, so a final waypoint matches it exactly.
	rbFinal = XMVector2Equal(vecWaypoint, engine::NavQuerySnapToNavigable(vecLevelDestination, rNavData));
	return vecWaypoint;
}

XMVECTOR XM_CALLCONV FleeRouteWaypoint(const engine::CellStaticData& rStaticData, int64_t iClearanceTier, FXMVECTOR vecPosition, FXMVECTOR vecThreat, NavRectangleBoard& rNavRectangleBoard, int64_t iTargetFlightPlane)
{
	// The neighbouring cell centre whose horizontal direction from the ship has the largest cosine with the ship's
	// horizontal offset from the threat; an exact tie, a zero offset included, keeps the first entry in table order.
	XMVECTOR vecAwayDirection = XMVector2Normalize(XMVectorSubtract(vecPosition, vecThreat));
	XMVECTOR vecPickedCenter = XMVectorZero();
	float fBestCosine = -std::numeric_limits<float>::max();
	for (const common::HexDirection& rDirection : engine::kCellDirections.pEntries)
	{
		XMVECTOR vecCenter = XMVectorSet(rDirection.f2CenterOffset.x, rDirection.f2CenterOffset.y, 0.0f, 1.0f);
		float fCosine = XMVectorGetX(XMVector2Dot(XMVector2Normalize(XMVectorSubtract(vecCenter, vecPosition)), vecAwayDirection));
		if (fCosine > fBestCosine)
		{
			fBestCosine = fCosine;
			vecPickedCenter = vecCenter;
		}
	}
	bool bFinalWaypoint = false;
	return NextRouteWaypoint(rStaticData, iClearanceTier, vecPosition, vecPickedCenter, rNavRectangleBoard, iTargetFlightPlane, bFinalWaypoint);
}

XMVECTOR XM_CALLCONV ComputeReturnDestination(const engine::CellStaticData& rStaticData, int64_t iClearanceTier, FXMVECTOR vecPosition, bool bEnemyFound, FXMVECTOR vecNearestEnemy, NavRectangleBoard& rNavRectangleBoard, int64_t iTargetFlightPlane, ShipFlags_t& rFlags, bool& rbHasDestination)
{
	std::span<const engine::IslandPlacement> centredIslands = engine::CentredIslands(rStaticData.islands);
	if (centredIslands.empty())
	{
		rFlags.Set(ShipFlags::kReturnToIslandCenter, false);
		rbHasDestination = bEnemyFound;
		return vecNearestEnemy;
	}

	float fBaseHeight = engine::gpFlightPlanes->Lowest().fHeightMeters;
	XMVECTOR vecIslandCenter = XMVectorSet(centredIslands[0].f2WorldPosition.x, centredIslands[0].f2WorldPosition.y, fBaseHeight, 1.0f);
	float fNearestDistanceSquared = std::numeric_limits<float>::max();
	for (const engine::IslandPlacement& rPlacement : centredIslands)
	{
		XMVECTOR vecCandidate = XMVectorSet(rPlacement.f2WorldPosition.x, rPlacement.f2WorldPosition.y, fBaseHeight, 1.0f);
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
	if (bEnemyFound && !(rFlags & ShipFlags::kReturnToIslandCenter))
	{
		return vecNearestEnemy;
	}
	bool bFinalWaypoint = false;
	return NextRouteWaypoint(rStaticData, iClearanceTier, vecPosition, vecIslandCenter, rNavRectangleBoard, iTargetFlightPlane, bFinalWaypoint);
}

bool XM_CALLCONV IsWithinHeadingCone(FXMVECTOR vecDirection, FXMVECTOR vecAiDirection)
{
	static constexpr float kfCosHalfAngle = std::numbers::sqrt2_v<float> * 0.5f;
	return XMVectorGetX(XMVector2Dot(vecDirection, vecAiDirection)) >= kfCosHalfAngle;
}

void XM_CALLCONV AimAiDirection(FXMVECTOR vecOffset, XMVECTOR& rVecAiDirection)
{
	XMVECTOR vecAiDirection = XMVector3Normalize(XMVectorSetZ(vecOffset, 0.0f));
	if (XMVectorGetX(XMVector3LengthSq(vecAiDirection)) > 0.0f)
	{
		rVecAiDirection = vecAiDirection;
	}
}

float RampHeadingTurnRate(float fPreviousDeltaRotation, float fWantedDeltaRotation, std::chrono::duration<float> deltaTime, const HeadingMovementConstants& rConstants)
{
	return common::ClampMagnitude(fPreviousDeltaRotation + common::ClampMagnitude(fWantedDeltaRotation - fPreviousDeltaRotation, rConstants.fTurnAcceleration * deltaTime.count()), rConstants.fMaxTurnRate);
}

// Terrain avoidance (enemy-proximity skip constants; sampling constants in TerrainUtils.cpp)
constexpr float kfAvoidTerrainEnemyAngle = 0.4f;
constexpr float kfAvoidTerrainEnemyDistance = 40.0f;

// Reads the previous frame's ships, so the tuple order never hands it an enemy row this tick has not updated yet.
template <ShipType TYPE, typename SHIP_INTERPOLATE, typename SHIP_POST_RENDER>
static void AvoidTerrainRows(const Frame& rFrame, const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, const HeadingMovementConstants& rConstants, SHIP_INTERPOLATE& rCurrentInterpolate, const SHIP_POST_RENDER& rCurrentPostRender, const SHIP_INTERPOLATE& rPreviousInterpolate)
{
	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		if (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) [[unlikely]]
		{
			continue;
		}

		if (rCurrentPostRender.pArrivalGracePeriods[i] > std::chrono::duration<float>::zero())
		{
			continue;
		}

		if (rCurrentPostRender.pController[i] == ShipController::kFleet && !IsWithinHeadingCone(rCurrentInterpolate.pVecDirections[i], rCurrentPostRender.pVecAiDirections[i]))
		{
			continue;
		}

		XMVECTOR vecNearestEnemy = XMVectorZero();
		if (FindNearestEnemyShip(rPreviousFrame, rCurrentInterpolate.pVecPositions[i], rCurrentPostRender.pAlignments[i], vecNearestEnemy))
		{
			XMVECTOR vecToEnemy = XMVectorSubtract(vecNearestEnemy, rCurrentInterpolate.pVecPositions[i]);
			float fDistanceToEnemy = XMVectorGetX(XMVector3Length(vecToEnemy));
			if (fDistanceToEnemy < kfAvoidTerrainEnemyDistance)
			{
				XMVECTOR vecToEnemyNormal = XMVector3Normalize(vecToEnemy);
				float fAngleToEnemy = XMVectorGetX(XMVector3AngleBetweenNormals(rCurrentInterpolate.pVecDirections[i], vecToEnemyNormal));
				if (fAngleToEnemy < kfAvoidTerrainEnemyAngle && HasLineOfSight(rStaticData, ShipTypeClearanceTier(TYPE), rCurrentInterpolate.puiFlightPlanes[i], rCurrentInterpolate.pVecPositions[i], vecNearestEnemy))
				{
					continue;
				}
			}
		}

		int64_t iNavigationPlane = NavigationFlightPlane(XMVectorGetZ(rCurrentInterpolate.pVecPositions[i]), GetTargetFlightPlane(rCurrentPostRender.pOrders[i]));
		float fAvoidanceDeltaRotation = ComputeTerrainAvoidance(rStaticData, rCurrentInterpolate.pVecPositions[i], rCurrentInterpolate.pVecDirections[i], rCurrentInterpolate.pfDeltaRotations[i], iNavigationPlane);

		// Rows still align with the previous frame here: Update calls this before Transfer, Destroy, and Spawn.
		rCurrentInterpolate.pfDeltaRotations[i] = RampHeadingTurnRate(rPreviousInterpolate.pfDeltaRotations[i], fAvoidanceDeltaRotation, std::chrono::duration<float>(rFrame.interpolate.fDeltaTime), rConstants);
	}
}

template <>
void AvoidTerrain<ShipType::kFighter>(Frame& rFrame, const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData)
{
	AvoidTerrainRows<ShipType::kFighter>(rFrame, rPreviousFrame, rStaticData, kFighterMovement, *rFrame.interpolate.pFighters, *rFrame.postRender.pFighters, *rPreviousFrame.interpolate.pFighters);
}

template <>
void AvoidTerrain<ShipType::kBattleship>(Frame& rFrame, const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData)
{
	AvoidTerrainRows<ShipType::kBattleship>(rFrame, rPreviousFrame, rStaticData, kBattleshipMovement, *rFrame.interpolate.pBattleships, *rFrame.postRender.pBattleships, *rPreviousFrame.interpolate.pBattleships);
}

// The wanted turn rate toward vecWantedNormal: 0 within the capture angle, otherwise the rate the turn acceleration can still stop
// from by the capture angle, so the ramp starts slowing early, up to the max turn rate.
float XM_CALLCONV SteerHeading(FXMVECTOR vecDirection, FXMVECTOR vecWantedNormal, const HeadingMovementConstants& rConstants)
{
	float fAngle = XMVectorGetX(XMVector2AngleBetweenNormals(vecDirection, vecWantedNormal));
	if (fAngle <= rConstants.fCaptureAngle)
	{
		return 0.0f;
	}
	float fTurnRate = std::min(rConstants.fMaxTurnRate, std::sqrt(2.0f * rConstants.fTurnAcceleration * (fAngle - rConstants.fCaptureAngle)));
	return XMVectorGetZ(XMVector3Cross(vecDirection, vecWantedNormal)) > 0.0f ? fTurnRate : -fTurnRate;
}

XMVECTOR XM_CALLCONV CaptureHeading(FXMVECTOR vecDirection, FXMVECTOR vecWantedNormal, std::chrono::duration<float> deltaTime, const HeadingMovementConstants& rConstants)
{
	if (XMVectorGetX(XMVector2AngleBetweenNormals(vecDirection, vecWantedNormal)) <= rConstants.fCaptureAngle)
	{
		return common::RotateTowardsPercent(vecDirection, vecWantedNormal, common::ExponentialInterpolant(rConstants.fCaptureRate, deltaTime.count()));
	}
	return vecDirection;
}

void XM_CALLCONV ApplyAcceleratedMovement(int64_t iNavigationDirection, FXMVECTOR vecAiDirection, std::chrono::duration<float> deltaTime, float fAccelerationMultiplier, float fDecayMultiplier, const AcceleratedMovementConstants& rConstants, XMVECTOR& rVecVelocity)
{
	float fAcceleration = (iNavigationDirection == 7 ? rConstants.fCatchUpAcceleration : rConstants.fAcceleration) * fAccelerationMultiplier;
	float fMaximumSpeed = iNavigationDirection == 7 ? rConstants.fCatchUpMaximumSpeed : rConstants.fMaximumSpeed;
	rVecVelocity = engine::ApplyMovement(rVecVelocity, vecAiDirection, deltaTime.count(), fAcceleration, rConstants.fDrag * fDecayMultiplier, fMaximumSpeed);
}

void XM_CALLCONV ApplyHeadingMovement(FXMVECTOR vecDirection, ShipFlags_t flags, bool bAllowAcceleration, std::chrono::duration<float> deltaTime, const HeadingMovementConstants& rConstants, XMVECTOR& rVecVelocity)
{
	float fAcceleration = !bAllowAcceleration ? 0.0f
	                    : flags & ShipFlags::kReturnToIslandCenter ? rConstants.fReturnAcceleration
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

// The clearance tier's graph of the plane a ship at fHeight navigates. Callers skip terrain contact on a plane without
// kTerrainCollision, and kFlightPlanes clears that flag only on its highest plane, so the navigated plane carries it.
static const engine::NavData& TerrainContactNavData(const engine::CellStaticData& rStaticData, int64_t iClearanceTier, float fHeight, int64_t iTargetFlightPlane)
{
	int64_t iNavigationPlane = NavigationFlightPlane(fHeight, iTargetFlightPlane);
	ASSERT(engine::gpFlightPlanes->Plane(iNavigationPlane).flags & engine::FlightPlaneFlags::kTerrainCollision);
	return NavigationPlaneData(rStaticData, iClearanceTier, iNavigationPlane);
}

void XM_CALLCONV ResolveTerrainContact(const engine::CellStaticData& rStaticData, ShipLocator self, FXMVECTOR vecPreviousPosition, XMVECTOR& rVecPosition, float fRadius, XMVECTOR& rVecVelocity, int64_t iTargetFlightPlane)
{
	// Hard contact at the sweep end's height; ApplyTerrainPush is the softer band in front of it.
	float fHeight = XMVectorGetZ(rVecPosition);
	engine::DiscTerrainResult result = engine::ResolveDiscAgainstTerrain(rStaticData, TerrainContactNavData(rStaticData, ShipTypeClearanceTier(self.eType), fHeight, iTargetFlightPlane), vecPreviousPosition, rVecPosition, fRadius, fHeight, true);
	if (result.flags & engine::DiscTerrainFlags::kStartOverlap) [[unlikely]]
	{
		LOG(kDefault, kWarning, "{} terrain start overlap Cell: ({},{}) Index: {} Start: {} Moved: {}", ShipTypeName(self.eType), rStaticData.coordinate.iQ, rStaticData.coordinate.iR, self.iRow, common::WbV2(vecPreviousPosition, 1), common::Wb(result.fStartOverlapDistance, 1));
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
	engine::DiscTerrainResult result = engine::ResolveDiscAgainstTerrain(rStaticData, TerrainContactNavData(rStaticData, ShipTypeClearanceTier(self.eType), fHeight, iTargetFlightPlane), vecPreviousPosition, rVecPosition, fRadius, fHeight, false);
	if (result.flags & engine::DiscTerrainFlags::kStartOverlap) [[unlikely]]
	{
		LOG(kDefault, kWarning, "{} terrain start overlap Cell: ({},{}) Index: {} Start: {} Moved: {}", ShipTypeName(self.eType), rStaticData.coordinate.iQ, rStaticData.coordinate.iR, self.iRow, common::WbV2(vecPreviousPosition, 1), common::Wb(result.fStartOverlapDistance, 1));
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

void XM_CALLCONV ApplyPusherPush(const Frame& rFrame, engine::pusher_t pusher, float fMaximumPushVelocity, XMVECTOR& rVecVelocity, float& rfPositionCorrectionX)
{
	if (!(pusher.uuid.iValue != 0))
	{
		return;
	}

	XMVECTOR vecPush = engine::PushersInterpolate::ApplyPush(rFrame.interpolate, pusher, rfPositionCorrectionX);
	float fPushLength = XMVectorGetX(XMVector3Length(vecPush));
	if (fPushLength > 0.0f)
	{
		XMVECTOR vecPushDirection = XMVectorDivide(vecPush, XMVectorReplicate(fPushLength));
		rVecVelocity = engine::ApplyClampedPush(rVecVelocity, vecPushDirection, fPushLength, fMaximumPushVelocity);
	}
}

bool XM_CALLCONV HasLineOfSight(const engine::CellStaticData& rStaticData, int64_t iClearanceTier, uint8_t uiFlightPlane, FXMVECTOR vecShooterPosition, FXMVECTOR vecTargetPosition)
{
	const engine::NavData& rNavData = NavigationPlaneData(rStaticData, iClearanceTier, uiFlightPlane);
	return !engine::NavQuerySegmentBlocked(engine::NavQuerySnapToNavigable(vecShooterPosition, rNavData), engine::NavQuerySnapToNavigable(vecTargetPosition, rNavData), rNavData);
}

bool XM_CALLCONV HasShipLineOfSight(const Frame& rFrame, const engine::CellStaticData& rStaticData, int64_t iClearanceTier, uint8_t uiFlightPlane, FXMVECTOR vecShooterPosition, FXMVECTOR vecTargetPosition)
{
	if (!HasLineOfSight(rStaticData, iClearanceTier, uiFlightPlane, vecShooterPosition, vecTargetPosition))
	{
		return false;
	}

	XMFLOAT2 f2Shooter {XMVectorGetX(vecShooterPosition), XMVectorGetY(vecShooterPosition)};
	XMFLOAT2 f2Target {XMVectorGetX(vecTargetPosition), XMVectorGetY(vecTargetPosition)};
	// A zero Z range at Z 0 makes the prism clip 2D.
	XMFLOAT3A f3Start {f2Shooter.x, f2Shooter.y, 0.0f};
	XMFLOAT3A f3Delta {f2Target.x - f2Shooter.x, f2Target.y - f2Shooter.y, 0.0f};

	const BattleshipsInterpolate& rInterpolate = *rFrame.interpolate.pBattleships;
	const BattleshipsPostRender& rPostRender = *rFrame.postRender.pBattleships;
	for (int64_t i = 0; i < rPostRender.iCount; ++i)
	{
		if (!IsNavRectangleRow(ShipType::kBattleship, rInterpolate, rPostRender, i))
		{
			continue;
		}

		XMVECTOR vecPosition = rInterpolate.pVecPositions[i];
		auto [iLowestPlane, iHighestPlane] = CappedOccupiedPlanes(XMVectorGetZ(vecPosition), ShipType::kBattleship);
		if (uiFlightPlane < iLowestPlane || uiFlightPlane > iHighestPlane)
		{
			continue;
		}

		const ShipHitboxShape& rShape = gShipHitboxShapes[std::to_underlying(ShipType::kBattleship)][std::to_underlying(rPostRender.pController[i])];
		XMVECTOR vecDirection = rInterpolate.pVecDirections[i];
		XMFLOAT2 f2Position {XMVectorGetX(vecPosition), XMVectorGetY(vecPosition)};
		float fCos = XMVectorGetX(vecDirection);
		float fSin = XMVectorGetY(vecDirection);
		XMFLOAT2 pf2Box[4] {};
		common::BuildWorldHull(rShape.hitbox.pf2BoxVertices, f2Position, fCos, fSin, pf2Box);
		if (common::PointInPolygon(f2Shooter, pf2Box) || common::PointInPolygon(f2Target, pf2Box))
		{
			continue;
		}

		float fEnter = 0.0f;
		float fExit = 1.0f;
		if (common::ClipSegmentToPrism(f3Start, f3Delta, pf2Box, 0.0f, 0.0f, fEnter, fExit))
		{
			return false;
		}
	}
	return true;
}

bool XM_CALLCONV IsSegmentBlocked(const engine::NavData& rNavData, std::span<const engine::NavRectangle> rectangles, FXMVECTOR vecStart, FXMVECTOR vecEnd)
{
	if (engine::NavQuerySegmentBlocked(vecStart, vecEnd, rNavData))
	{
		return true;
	}

	// A zero Z range at Z 0 makes the prism clip 2D.
	XMFLOAT3A f3Start {XMVectorGetX(vecStart), XMVectorGetY(vecStart), 0.0f};
	XMFLOAT3A f3Delta {XMVectorGetX(vecEnd) - f3Start.x, XMVectorGetY(vecEnd) - f3Start.y, 0.0f};
	for (const engine::NavRectangle& rRectangle : rectangles)
	{
		float fEnter = 0.0f;
		float fExit = 1.0f;
		if (common::ClipSegmentToPrism(f3Start, f3Delta, rRectangle.pf2Box, 0.0f, 0.0f, fEnter, fExit))
		{
			return true;
		}
	}
	return false;
}

void RegenerateShield(std::chrono::duration<float> deltaTime, std::chrono::duration<float> shieldCooldown, float fMaximumShield, float fRegeneration, float& rfShield)
{
	if (shieldCooldown.count() <= 0.0f)
	{
		rfShield = std::min(rfShield + deltaTime.count() * fRegeneration, fMaximumShield);
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
	if (iNavigationDirection == 7)
	{
		std::optional<ShipLocator> optionalFlagship = FindShipByGlobalId(rRenderFrame, flagshipGlobalId);
		if (optionalFlagship)
		{
			vecFlagshipPosition = ShipPosition(rFrameInterpolate, *optionalFlagship);
			bFlagshipFound = true;
		}
	}

	if (XMVectorGetW(vecDebugWaypoint) > 0.0f && (iNavigationDirection != 7 || bFlagshipFound))
	{
		// In mode 7 (flagship follow), use the flagship's interpolated position as the waypoint
		XMVECTOR vecWaypoint = (iNavigationDirection == 7) ? vecFlagshipPosition : vecDebugWaypoint;
		XMFLOAT3A f3NavigationStart {};
		XMFLOAT3A f3NavigationEnd {};
		XMStoreFloat3A(&f3NavigationStart, engine::Rebase(rBasis, vecPosition));
		// The waypoint line end and circle draw at the ship's own height, whatever height navigation stored, so a ship
		// on a higher plane shows no vertical drop.
		XMStoreFloat3A(&f3NavigationEnd, engine::Rebase(rBasis, XMVectorSetZ(vecWaypoint, XMVectorGetZ(vecPosition))));
		engine::DebugRender::Line(f3NavigationStart, f3NavigationEnd, {0.0f, 1.0f, 0.0f, 1.0f});
		engine::DebugRender::Circle(f3NavigationEnd, fRadius * 0.5f, {0.0f, 1.0f, 0.0f, 1.0f});
	}

	if (iNavigationDirection == 7)
	{
		if (bFlagshipFound)
		{
			XMFLOAT3A f3Destination {};
			XMStoreFloat3A(&f3Destination, engine::Rebase(rBasis, vecFlagshipPosition));
			engine::DebugRender::Circle(f3Destination, fRadius * 2.0f, {0.0f, 1.0f, 0.0f, 1.0f});
		}
	}
	else if (XMVectorGetW(vecIslandDestination) > 0.0f)
	{
		XMFLOAT3A f3Destination {};
		XMStoreFloat3A(&f3Destination, engine::Rebase(rBasis, XMVectorSetZ(vecIslandDestination, XMVectorGetZ(vecPosition))));
		engine::DebugRender::Circle(f3Destination, fRadius * 2.0f, {0.0f, 1.0f, 0.0f, 1.0f});
	}
}
#endif // BT_CLIENT

} // namespace game
