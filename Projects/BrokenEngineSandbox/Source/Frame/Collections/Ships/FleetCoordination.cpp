#include "FleetCoordination.h"

#include "Frame/Collections/Ships/ShipQueries.h"
#include "Frame/TerrainUtils.h"

namespace game
{

constexpr float kfWeaponsFreeDistanceSquared = 100.0f * 100.0f;
constexpr float kfEscortDistance = 50.0f;
// A slot then moves at about 90% of the Fighters' steady speed on the escort circle, so a Fighter behind its slot can catch up.
constexpr int64_t kiEscortLapTicks = 480;
constexpr float kfVeerOffStartDistanceSquared = 20.0f * 20.0f;
constexpr float kfVeerOffEndDistanceSquared = 150.0f * 150.0f;

// XMVector2Normalize divides all four lanes by the x/y length, so every horizontal offset zeroes Z before it; a zero x/y
// offset normalizes to zero.
static XMVECTOR XM_CALLCONV HorizontalNormal(FXMVECTOR vecOffset)
{
	return XMVector2Normalize(XMVectorSetZ(vecOffset, 0.0f));
}

static float XM_CALLCONV HorizontalDistanceSquared(FXMVECTOR vecFrom, FXMVECTOR vecTo)
{
	return XMVectorGetX(XMVector2LengthSq(XMVectorSubtract(vecTo, vecFrom)));
}

// Flies directly away from the veer-off point on that point's plane; a ship exactly over it gets its own position.
static ShipTactic XM_CALLCONV VeerOffTactic(FXMVECTOR vecPosition, FXMVECTOR vecVeerOffPoint)
{
	return
	{
		.vecDestination = XMVectorAdd(vecPosition, HorizontalNormal(XMVectorSubtract(vecPosition, vecVeerOffPoint))),
		.iTargetFlightPlane = NearestFlightPlane(vecVeerOffPoint),
		.bHasDestination = true,
	};
}

// Continues a veer-off in progress until kfVeerOffEndDistanceSquared from its point, then clears it. Returns false,
// leaving rTactic unchanged, when not veering off.
static bool XM_CALLCONV ContinueVeerOff(FXMVECTOR vecPosition, FXMVECTOR vecVeerOffPoint, ShipFlags_t& rFlags, ShipTactic& rTactic)
{
	if (rFlags & ShipFlags::kVeerOff)
	{
		if (HorizontalDistanceSquared(vecPosition, vecVeerOffPoint) < kfVeerOffEndDistanceSquared)
		{
			rTactic = VeerOffTactic(vecPosition, vecVeerOffPoint);
			return true;
		}
		rFlags.Set(ShipFlags::kVeerOff, false);
	}
	return false;
}

static ShipTactic XM_CALLCONV AttackTactic(FXMVECTOR vecEnemyPosition)
{
	return
	{
		.vecDestination = vecEnemyPosition,
		.iTargetFlightPlane = NearestFlightPlane(vecEnemyPosition),
		.bHasDestination = true,
		.bAttacking = true,
	};
}

// Starts a veer-off from an enemy within kfVeerOffStartDistanceSquared in x/y, else attacks it on its nearest plane.
static ShipTactic XM_CALLCONV AttackOrStartVeerOff(FXMVECTOR vecPosition, FXMVECTOR vecEnemyPosition, ShipFlags_t& rFlags, XMVECTOR& rVecVeerOffPoint)
{
	if (HorizontalDistanceSquared(vecPosition, vecEnemyPosition) <= kfVeerOffStartDistanceSquared)
	{
		rFlags.Set(ShipFlags::kVeerOff);
		rVecVeerOffPoint = vecEnemyPosition;
		return VeerOffTactic(vecPosition, rVecVeerOffPoint);
	}

	return AttackTactic(vecEnemyPosition);
}

// For a target hidden from the ship: the next route waypoint toward it on its nearest plane.
static ShipTactic XM_CALLCONV RouteTactic(const engine::CellStaticData& rStaticData, int64_t iClearanceTier, FXMVECTOR vecPosition, FXMVECTOR vecTargetPosition, NavRectangleBoard& rNavRectangleBoard)
{
	int64_t iTargetFlightPlane = NearestFlightPlane(vecTargetPosition);
	bool bFinalWaypoint = false;
	return
	{
		.vecDestination = NextRouteWaypoint(rStaticData, iClearanceTier, vecPosition, vecTargetPosition, rNavRectangleBoard, iTargetFlightPlane, bFinalWaypoint),
		.iTargetFlightPlane = iTargetFlightPlane,
		.bHasDestination = true,
	};
}

// The fleet's non-dying Fighters in the previous frame share the escort circle evenly in global-ID order, clockwise from
// slot 0 due East, on a clock that turns clockwise once per kiEscortLapTicks.
static XMVECTOR XM_CALLCONV EscortSlotPoint(const Frame& rPreviousFrame, const FleetView& rView, engine::GlobalId globalId)
{
	int64_t iCount = 0;
	int64_t iSlot = 0;
	const FightersPostRender& rFighters = *rPreviousFrame.postRender.pFighters;
	for (int64_t i = 0; i < rFighters.iCount; ++i)
	{
		if (rFighters.pFlagshipGlobalIds[i] != rView.flagshipGlobalId)
		{
			continue;
		}

		if (rFighters.pLifecycleFlags[i] & engine::LifecycleFlags::kDying)
		{
			continue;
		}
		++iCount;
		if (rFighters.pGlobalIds[i].iValue < globalId.iValue)
		{
			++iSlot;
		}
	}

	// The escorting Fighter counts itself, so iCount >= 1. The integer phase keeps the angle in [0, 2*pi) and exact over
	// any run length.
	int64_t iLapTick = rPreviousFrame.interpolate.iTick % kiEscortLapTicks;
	int64_t iPeriod = kiEscortLapTicks * iCount;
	int64_t iPhase = (iLapTick * iCount + iSlot * kiEscortLapTicks) % iPeriod;
	common::SinCos rotation = common::DeterministicSinCos(XM_2PI * static_cast<float>(iPhase) / static_cast<float>(iPeriod));
	return XMVectorAdd(rView.vecFlagshipPosition, XMVectorSet(rotation.fCos * kfEscortDistance, -rotation.fSin * kfEscortDistance, 0.0f, 0.0f));
}

FleetBoard::FleetBoard(const Frame& rPreviousFrame)
: mViews(common::gpThreadLocal->mWorkbuffer.PushBuffer<FleetView*>(rPreviousFrame.postRender.pBattleships->iCount * static_cast<int64_t>(sizeof(FleetView))))
{
	const BattleshipsInterpolate& rInterpolate = *rPreviousFrame.interpolate.pBattleships;
	const BattleshipsPostRender& rPostRender = *rPreviousFrame.postRender.pBattleships;
	for (int64_t i = 0; i < rPostRender.iCount; ++i)
	{
		if (!(rPostRender.pOrders[i] & ShipOrders::kIsFlagship))
		{
			continue;
		}

		XMVECTOR vecFlagshipPosition = rInterpolate.pVecPositions[i];
		FleetStance eStance = FleetStance::kEscort;
		ForEachEnemyShip(rPreviousFrame.interpolate, rPreviousFrame.postRender, rPostRender.pAlignments[i], [&]([[maybe_unused]] ShipLocator locator, FXMVECTOR vecPosition, [[maybe_unused]] FXMVECTOR vecVelocity)
		{
			if (HorizontalDistanceSquared(vecFlagshipPosition, vecPosition) <= kfWeaponsFreeDistanceSquared)
			{
				eStance = FleetStance::kWeaponsFree;
			}
		});
		mViews.mpData[miViewCount++] = FleetView
		{
			.vecFlagshipPosition = vecFlagshipPosition,
			.flagshipGlobalId = rPostRender.pGlobalIds[i],
			.eStance = eStance,
		};
	}
}

FleetView FleetBoard::Find(engine::GlobalId flagshipGlobalId) const
{
	for (int64_t i = 0; i < miViewCount; ++i)
	{
		if (mViews.mpData[i].flagshipGlobalId == flagshipGlobalId)
		{
			return mViews.mpData[i];
		}
	}
	return {};
}

ShipTactic XM_CALLCONV ChooseFleetTactic(ShipType eType, const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, const FleetView& rView, engine::GlobalId globalId, FXMVECTOR vecPosition, NavRectangleBoard& rNavRectangleBoard, uint8_t uiFlightPlane, engine::AlignmentIdentifier alignment, ShipFlags_t& rFlags, XMVECTOR& rVecVeerOffPoint, engine::registry_id_t& rRegistryTarget)
{
	int64_t iHighestFlightPlane = HighestFlightPlane(eType);
	if (rView.flagshipGlobalId.iValue == 0)
	{
		rFlags.Set(ShipFlags::kVeerOff, false);
		rRegistryTarget = {};
		return {.iTargetFlightPlane = DefaultFlightPlane(eType)};
	}

	if (rView.eStance == FleetStance::kWeaponsFree)
	{
		ShipTactic tactic {};
		if (eType == ShipType::kFighter && ContinueVeerOff(vecPosition, rVecVeerOffPoint, rFlags, tactic))
		{
			// Attack ignores enemies above the type's highest plane, so only a veer-off point loaded from a save can lie above it.
			tactic.iTargetFlightPlane = std::min(tactic.iTargetFlightPlane, iHighestFlightPlane);
			return tactic;
		}

		XMVECTOR vecTargetPosition = XMVectorZero();
		bool bInLineOfSight = false;
		if (FindAttackTarget(rPreviousFrame, rStaticData, vecPosition, uiFlightPlane, rView.vecFlagshipPosition, true, alignment, iHighestFlightPlane, rRegistryTarget, vecTargetPosition, bInLineOfSight))
		{
			// The pick tests sight on kiFiringClearanceTier; flying at the target also needs sight on the type's own tier,
			// which is the pick's when the two match.
			int64_t iClearanceTier = ShipTypeClearanceTier(eType);
			if (!bInLineOfSight || (iClearanceTier != kiFiringClearanceTier && !HasLineOfSight(rStaticData, iClearanceTier, uiFlightPlane, vecPosition, vecTargetPosition)))
			{
				return RouteTactic(rStaticData, iClearanceTier, vecPosition, vecTargetPosition, rNavRectangleBoard);
			}
			if (eType == ShipType::kFighter)
			{
				return AttackOrStartVeerOff(vecPosition, vecTargetPosition, rFlags, rVecVeerOffPoint);
			}
			return AttackTactic(vecTargetPosition);
		}
	}

	rFlags.Set(ShipFlags::kVeerOff, false);
	if (eType == ShipType::kOmni)
	{
		return {.iTargetFlightPlane = DefaultFlightPlane(eType)};
	}
	return
	{
		.vecDestination = EscortSlotPoint(rPreviousFrame, rView, globalId),
		.iTargetFlightPlane = DefaultFlightPlane(eType),
		.bHasDestination = true,
	};
}

bool XM_CALLCONV ChooseAttackTactic(const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, int64_t iClearanceTier, FXMVECTOR vecPosition, NavRectangleBoard& rNavRectangleBoard, uint8_t uiFlightPlane, engine::AlignmentIdentifier alignment, int64_t iHighestFlightPlane, bool bTargetFlagship, ShipFlags_t& rFlags, XMVECTOR& rVecVeerOffPoint, engine::registry_id_t& rRegistryTarget, ShipTactic& rTactic)
{
	if (ContinueVeerOff(vecPosition, rVecVeerOffPoint, rFlags, rTactic))
	{
		return true;
	}

	XMVECTOR vecTargetPosition = XMVectorZero();
	bool bInLineOfSight = false;
	if (bTargetFlagship && FindNearestEnemyFlagship(rPreviousFrame, vecPosition, alignment, rRegistryTarget, vecTargetPosition))
	{
		bInLineOfSight = HasShipLineOfSight(rPreviousFrame, rStaticData, iClearanceTier, uiFlightPlane, vecPosition, vecTargetPosition);
	}
	else if (!FindAttackTarget(rPreviousFrame, rStaticData, vecPosition, uiFlightPlane, vecPosition, true, alignment, iHighestFlightPlane, rRegistryTarget, vecTargetPosition, bInLineOfSight))
	{
		return false;
	}
	else if (bInLineOfSight)
	{
		// The pick tests sight on kiFiringClearanceTier; flying at the target also needs sight on iClearanceTier.
		bInLineOfSight = HasLineOfSight(rStaticData, iClearanceTier, uiFlightPlane, vecPosition, vecTargetPosition);
	}

	rTactic = bInLineOfSight ? AttackOrStartVeerOff(vecPosition, vecTargetPosition, rFlags, rVecVeerOffPoint) : RouteTactic(rStaticData, iClearanceTier, vecPosition, vecTargetPosition, rNavRectangleBoard);
	return true;
}

} // namespace game
