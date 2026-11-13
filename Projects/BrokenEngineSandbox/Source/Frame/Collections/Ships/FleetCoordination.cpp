#include "FleetCoordination.h"

#include "Frame/Collections/Ships/ShipQueries.h"
#include "Frame/TerrainUtils.h"

namespace game
{

constexpr float kfWeaponsFreeDistanceSquared = 100.0f * 100.0f;
constexpr float kfEscortDistance = 50.0f;
constexpr int64_t kiEscortLapTicks = 480;
// An escorting Fighter aims at the point its slot reaches this many ticks later, so it closes on the slot's path.
constexpr int64_t kiEscortLeadTicks = 30;
// The maximum speed whose steady speed under the Fighter's chase acceleration and drag matches the slot's arc speed:
// steady speed = a / (drag + a / cap), solved for cap.
constexpr float kfEscortSlotSpeed = XM_2PI * kfEscortDistance * static_cast<float>(engine::kiTickRate) / static_cast<float>(kiEscortLapTicks);
constexpr float kfEscortBaseMaximumSpeed = kFighterMovement.fChaseAcceleration / (kFighterMovement.fChaseAcceleration / kfEscortSlotSpeed - kFighterMovement.fDrag);
// Maximum speed added per metre the Fighter trails its slot along the slot's path (1/s); ahead of it, removed.
constexpr float kfEscortGapGain = 20.0f;
// ApplyMovement divides by the maximum speed, and 0 means the ship type's own.
constexpr float kfEscortMinimumMaximumSpeed = 1.0f;
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

// Routes away from the veer-off point on that point's plane.
static ShipTactic XM_CALLCONV VeerOffTactic(const engine::CellStaticData& rStaticData, int64_t iClearanceTier, FXMVECTOR vecPosition, FXMVECTOR vecVeerOffPoint, NavRectangleBoard& rNavRectangleBoard)
{
	int64_t iTargetFlightPlane = NearestFlightPlane(vecVeerOffPoint);
	return
	{
		.vecDestination = FleeRouteWaypoint(rStaticData, iClearanceTier, vecPosition, vecVeerOffPoint, rNavRectangleBoard, iTargetFlightPlane),
		.iTargetFlightPlane = iTargetFlightPlane,
		.bHasDestination = true,
	};
}

// Continues a veer-off in progress until kfVeerOffEndDistanceSquared from its point, then clears it. Returns false,
// leaving rTactic unchanged, when not veering off.
static bool XM_CALLCONV ContinueVeerOff(const engine::CellStaticData& rStaticData, int64_t iClearanceTier, FXMVECTOR vecPosition, FXMVECTOR vecVeerOffPoint, NavRectangleBoard& rNavRectangleBoard, ShipFlags_t& rFlags, ShipTactic& rTactic)
{
	if (rFlags & ShipFlags::kVeerOff)
	{
		if (HorizontalDistanceSquared(vecPosition, vecVeerOffPoint) < kfVeerOffEndDistanceSquared)
		{
			rTactic = VeerOffTactic(rStaticData, iClearanceTier, vecPosition, vecVeerOffPoint, rNavRectangleBoard);
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
static ShipTactic XM_CALLCONV AttackOrStartVeerOff(const engine::CellStaticData& rStaticData, int64_t iClearanceTier, FXMVECTOR vecPosition, FXMVECTOR vecEnemyPosition, NavRectangleBoard& rNavRectangleBoard, ShipFlags_t& rFlags, XMVECTOR& rVecVeerOffPoint)
{
	if (HorizontalDistanceSquared(vecPosition, vecEnemyPosition) <= kfVeerOffStartDistanceSquared)
	{
		rFlags.Set(ShipFlags::kVeerOff);
		rVecVeerOffPoint = vecEnemyPosition;
		return VeerOffTactic(rStaticData, iClearanceTier, vecPosition, rVecVeerOffPoint, rNavRectangleBoard);
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
// slot 0 due East, on a clock that turns clockwise once per kiEscortLapTicks; iTickOffset reads the clock that many ticks
// after the previous frame's.
static XMVECTOR XM_CALLCONV EscortSlotPoint(const Frame& rPreviousFrame, const FleetView& rView, engine::GlobalId globalId, int64_t iTickOffset)
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
	int64_t iLapTick = (rPreviousFrame.interpolate.iTick + iTickOffset) % kiEscortLapTicks;
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

ShipTactic XM_CALLCONV ChooseFleetTactic(ShipType eType, const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, const FleetView& rView, engine::GlobalId globalId, FXMVECTOR vecPosition, NavRectangleBoard& rNavRectangleBoard, uint8_t uiFlightPlane, engine::AlignmentIdentifier alignment, engine::registry_id_t& rRegistryTarget)
{
	int64_t iHighestFlightPlane = HighestFlightPlane(eType);
	if (rView.flagshipGlobalId.iValue == 0)
	{
		rRegistryTarget = {};
		return {.iTargetFlightPlane = DefaultFlightPlane(eType)};
	}

	if (rView.eStance == FleetStance::kWeaponsFree)
	{
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
			return AttackTactic(vecTargetPosition);
		}
	}

	if (eType == ShipType::kOmni)
	{
		return {.iTargetFlightPlane = DefaultFlightPlane(eType)};
	}
	XMVECTOR vecSlot = EscortSlotPoint(rPreviousFrame, rView, globalId, 0);
	XMVECTOR vecLead = EscortSlotPoint(rPreviousFrame, rView, globalId, kiEscortLeadTicks);
	// Positive while the Fighter trails its slot along the slot's path.
	float fGap = XMVectorGetX(XMVector2Dot(XMVectorSubtract(vecSlot, vecPosition), HorizontalNormal(XMVectorSubtract(vecLead, vecSlot))));
	return
	{
		.vecDestination = vecLead,
		.iTargetFlightPlane = DefaultFlightPlane(eType),
		.fMaximumSpeed = std::max(kfEscortBaseMaximumSpeed + kfEscortGapGain * fGap, kfEscortMinimumMaximumSpeed),
		.bHasDestination = true,
	};
}

ShipTactic XM_CALLCONV ChooseFighterFleetTactic(const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, const FleetView& rView, engine::GlobalId globalId, FXMVECTOR vecPosition, NavRectangleBoard& rNavRectangleBoard, uint8_t uiFlightPlane, engine::AlignmentIdentifier alignment, ShipFlags_t& rFlags, XMVECTOR& rVecVeerOffPoint, engine::registry_id_t& rRegistryTarget)
{
	int64_t iClearanceTier = ShipTypeClearanceTier(ShipType::kFighter);
	if (rView.flagshipGlobalId.iValue != 0 && rView.eStance == FleetStance::kWeaponsFree)
	{
		ShipTactic tactic {};
		if (ContinueVeerOff(rStaticData, iClearanceTier, vecPosition, rVecVeerOffPoint, rNavRectangleBoard, rFlags, tactic))
		{
			return tactic;
		}
	}

	ShipTactic tactic = ChooseFleetTactic(ShipType::kFighter, rPreviousFrame, rStaticData, rView, globalId, vecPosition, rNavRectangleBoard, uiFlightPlane, alignment, rRegistryTarget);
	if (tactic.bAttacking)
	{
		return AttackOrStartVeerOff(rStaticData, iClearanceTier, vecPosition, tactic.vecDestination, rNavRectangleBoard, rFlags, rVecVeerOffPoint);
	}
	rFlags.Set(ShipFlags::kVeerOff, false);
	return tactic;
}

bool XM_CALLCONV ChooseAttackTactic(const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, int64_t iClearanceTier, FXMVECTOR vecPosition, NavRectangleBoard& rNavRectangleBoard, uint8_t uiFlightPlane, engine::AlignmentIdentifier alignment, int64_t iHighestFlightPlane, bool bTargetFlagship, ShipFlags_t& rFlags, XMVECTOR& rVecVeerOffPoint, engine::registry_id_t& rRegistryTarget, ShipTactic& rTactic)
{
	if (ContinueVeerOff(rStaticData, iClearanceTier, vecPosition, rVecVeerOffPoint, rNavRectangleBoard, rFlags, rTactic))
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

	rTactic = bInLineOfSight ? AttackOrStartVeerOff(rStaticData, iClearanceTier, vecPosition, vecTargetPosition, rNavRectangleBoard, rFlags, rVecVeerOffPoint) : RouteTactic(rStaticData, iClearanceTier, vecPosition, vecTargetPosition, rNavRectangleBoard);
	return true;
}

} // namespace game
