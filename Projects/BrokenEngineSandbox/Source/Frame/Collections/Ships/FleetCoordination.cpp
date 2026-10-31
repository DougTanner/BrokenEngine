#include "FleetCoordination.h"

#include "Frame/Collections/Ships/ShipQueries.h"

namespace game
{

constexpr float kfWeaponsFreeDistanceSquared = 100.0f * 100.0f;
constexpr float kfEscortDistance = 50.0f;
constexpr float kfOmniEscortDistance = 25.0f;
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

// Skips enemies above iHighestFlightPlane. Ties keep the first row in ForEachShipType order.
static bool XM_CALLCONV FindHorizontallyNearestEnemyShip(const Frame& rPreviousFrame, FXMVECTOR vecFrom, engine::AlignmentIdentifier alignment, int64_t iHighestFlightPlane, XMVECTOR& rVecResult)
{
	bool bFound = false;
	float fNearestDistanceSquared = std::numeric_limits<float>::max();
	ForEachEnemyShip(rPreviousFrame.interpolate, rPreviousFrame.postRender, alignment, [&]([[maybe_unused]] ShipLocator locator, FXMVECTOR vecPosition, [[maybe_unused]] FXMVECTOR vecVelocity)
	{
		if (NearestFlightPlane(vecPosition) > iHighestFlightPlane)
		{
			return;
		}
		float fDistanceSquared = HorizontalDistanceSquared(vecFrom, vecPosition);
		if (fDistanceSquared < fNearestDistanceSquared)
		{
			fNearestDistanceSquared = fDistanceSquared;
			rVecResult = vecPosition;
			bFound = true;
		}
	});
	return bFound;
}

// The kept target's position while rRegistryTarget names an enemy row; otherwise rRegistryTarget becomes the enemy nearest
// the flagship in x/y, ties keeping the first row in ForEachShipType order. Both skip enemies above iHighestFlightPlane.
// Returns false, leaving both outputs unchanged, when the previous frame holds no such enemy.
static bool XM_CALLCONV FindFleetAttackTarget(const Frame& rPreviousFrame, FXMVECTOR vecFlagshipPosition, engine::AlignmentIdentifier alignment, int64_t iHighestFlightPlane, engine::registry_id_t& rRegistryTarget, XMVECTOR& rVecResult)
{
	bool bKept = false;
	bool bFound = false;
	float fNearestDistanceSquared = std::numeric_limits<float>::max();
	engine::registry_id_t nearestRegistryId {};
	XMVECTOR vecNearestPosition = XMVectorZero();
	ForEachEnemyShip(rPreviousFrame.interpolate, rPreviousFrame.postRender, alignment, [&](ShipLocator locator, FXMVECTOR vecPosition, [[maybe_unused]] FXMVECTOR vecVelocity)
	{
		if (NearestFlightPlane(vecPosition) > iHighestFlightPlane)
		{
			return;
		}
		engine::registry_id_t registryId = VisitShip(rPreviousFrame, locator, [](const auto& rInterpolate, [[maybe_unused]] const auto& rPostRender, int64_t iRow)
		{
			return rInterpolate.pRegistryIds[iRow];
		});
		if (rRegistryTarget.uuid.iValue != 0 && registryId == rRegistryTarget)
		{
			bKept = true;
			rVecResult = vecPosition;
		}
		float fDistanceSquared = HorizontalDistanceSquared(vecFlagshipPosition, vecPosition);
		if (fDistanceSquared < fNearestDistanceSquared)
		{
			fNearestDistanceSquared = fDistanceSquared;
			nearestRegistryId = registryId;
			vecNearestPosition = vecPosition;
			bFound = true;
		}
	});
	if (bKept)
	{
		return true;
	}
	if (!bFound)
	{
		return false;
	}
	rRegistryTarget = nearestRegistryId;
	rVecResult = vecNearestPosition;
	return true;
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

// Starts a veer-off from an enemy within kfVeerOffStartDistanceSquared in x/y, else attacks it on its nearest plane.
static ShipTactic XM_CALLCONV AttackOrStartVeerOff(FXMVECTOR vecPosition, FXMVECTOR vecEnemyPosition, ShipFlags_t& rFlags, XMVECTOR& rVecVeerOffPoint)
{
	if (HorizontalDistanceSquared(vecPosition, vecEnemyPosition) <= kfVeerOffStartDistanceSquared)
	{
		rFlags.Set(ShipFlags::kVeerOff);
		rVecVeerOffPoint = vecEnemyPosition;
		return VeerOffTactic(vecPosition, rVecVeerOffPoint);
	}

	return
	{
		.vecDestination = vecEnemyPosition,
		.iTargetFlightPlane = NearestFlightPlane(vecEnemyPosition),
		.bHasDestination = true,
	};
}

// The fleet's non-dying ships of eType in the previous frame share their type's escort circle evenly in global-ID order,
// slot 0 due East. The Fighters' clock turns counter-clockwise once per kiEscortLapTicks; the Omnis' stays fixed.
static XMVECTOR XM_CALLCONV EscortSlotPoint(ShipType eType, const Frame& rPreviousFrame, const FleetView& rView, engine::GlobalId globalId)
{
	int64_t iCount = 0;
	int64_t iSlot = 0;
	auto CountSlots = [&](const auto& rShips)
	{
		for (int64_t i = 0; i < rShips.iCount; ++i)
		{
			if (rShips.pFlagshipGlobalIds[i] != rView.flagshipGlobalId)
			{
				continue;
			}

			if (rShips.pLifecycleFlags[i] & engine::LifecycleFlags::kDying)
			{
				continue;
			}
			++iCount;
			if (rShips.pGlobalIds[i].iValue < globalId.iValue)
			{
				++iSlot;
			}
		}
	};
	if (eType == ShipType::kFighter)
	{
		CountSlots(*rPreviousFrame.postRender.pFighters);
	}
	else
	{
		CountSlots(*rPreviousFrame.postRender.pOmnis);
	}

	// The escorting ship counts itself, so iCount >= 1. The integer phase keeps the angle in [0, 2*pi) and exact over any
	// run length.
	int64_t iLapTick = eType == ShipType::kFighter ? rPreviousFrame.interpolate.iTick % kiEscortLapTicks : 0;
	float fEscortDistance = eType == ShipType::kFighter ? kfEscortDistance : kfOmniEscortDistance;
	int64_t iPeriod = kiEscortLapTicks * iCount;
	int64_t iPhase = (iLapTick * iCount + iSlot * kiEscortLapTicks) % iPeriod;
	common::SinCos rotation = common::DeterministicSinCos(XM_2PI * static_cast<float>(iPhase) / static_cast<float>(iPeriod));
	return XMVectorAdd(rView.vecFlagshipPosition, XMVectorSet(rotation.fCos * fEscortDistance, rotation.fSin * fEscortDistance, 0.0f, 0.0f));
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

ShipTactic XM_CALLCONV ChooseFleetTactic(ShipType eType, const Frame& rPreviousFrame, const FleetView& rView, engine::GlobalId globalId, FXMVECTOR vecPosition, engine::AlignmentIdentifier alignment, ShipFlags_t& rFlags, XMVECTOR& rVecVeerOffPoint, engine::registry_id_t& rRegistryTarget)
{
	int64_t iHighestFlightPlane = HighestFlightPlane(eType);
	if (rView.flagshipGlobalId.iValue == 0)
	{
		rFlags.Set(ShipFlags::kVeerOff, false);
		rRegistryTarget = {};
		return {.iTargetFlightPlane = iHighestFlightPlane};
	}

	if (rView.eStance == FleetStance::kWeaponsFree)
	{
		ShipTactic tactic {};
		if (ContinueVeerOff(vecPosition, rVecVeerOffPoint, rFlags, tactic))
		{
			// Attack ignores enemies above the type's highest plane, so only a veer-off point loaded from a save can lie above it.
			tactic.iTargetFlightPlane = std::min(tactic.iTargetFlightPlane, iHighestFlightPlane);
			return tactic;
		}

		XMVECTOR vecTargetPosition = XMVectorZero();
		if (FindFleetAttackTarget(rPreviousFrame, rView.vecFlagshipPosition, alignment, iHighestFlightPlane, rRegistryTarget, vecTargetPosition))
		{
			return AttackOrStartVeerOff(vecPosition, vecTargetPosition, rFlags, rVecVeerOffPoint);
		}
	}

	rFlags.Set(ShipFlags::kVeerOff, false);
	return
	{
		.vecDestination = EscortSlotPoint(eType, rPreviousFrame, rView, globalId),
		.iTargetFlightPlane = iHighestFlightPlane,
		.bHasDestination = true,
	};
}

bool XM_CALLCONV ChooseAttackTactic(const Frame& rPreviousFrame, FXMVECTOR vecPosition, engine::AlignmentIdentifier alignment, int64_t iHighestFlightPlane, ShipFlags_t& rFlags, XMVECTOR& rVecVeerOffPoint, ShipTactic& rTactic)
{
	if (ContinueVeerOff(vecPosition, rVecVeerOffPoint, rFlags, rTactic))
	{
		return true;
	}

	XMVECTOR vecEnemyPosition = XMVectorZero();
	if (!FindHorizontallyNearestEnemyShip(rPreviousFrame, vecPosition, alignment, iHighestFlightPlane, vecEnemyPosition))
	{
		return false;
	}

	rTactic = AttackOrStartVeerOff(vecPosition, vecEnemyPosition, rFlags, rVecVeerOffPoint);
	return true;
}

} // namespace game
