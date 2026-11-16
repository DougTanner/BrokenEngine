#include "ShipQueries.h"

#include "Frame/Collections/Ships/ShipBehaviours.h"

namespace game
{

std::optional<ShipLocator> FindShipByGlobalId(const Frame& rFrame, engine::GlobalId globalId)
{
	std::optional<ShipLocator> optionalLocator;
	if (!(globalId.iValue != 0))
	{
		return optionalLocator;
	}

	ForEachShipType(rFrame, [&](ShipType eType, [[maybe_unused]] const auto& rInterpolate, const auto& rPostRender)
	{
		for (int64_t i = 0; i < rPostRender.iCount && !optionalLocator; ++i)
		{
			if (rPostRender.pGlobalIds[i] == globalId)
			{
				optionalLocator = ShipLocator {.eType = eType, .iRow = i};
			}
		}
	});
	return optionalLocator;
}

bool XM_CALLCONV FindNearestEnemyShip(const Frame& rFrame, FXMVECTOR vecFrom, engine::AlignmentIdentifier alignment, XMVECTOR& rVecResult)
{
	float fClosestDistanceSquared = std::numeric_limits<float>::max();
	bool bFound = false;
	ForEachEnemyShip(rFrame.interpolate, rFrame.postRender, alignment, [&]([[maybe_unused]] ShipLocator locator, FXMVECTOR vecPosition, [[maybe_unused]] FXMVECTOR vecVelocity)
	{
		float fDistanceSquared = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(vecPosition, vecFrom)));
		if (fDistanceSquared < fClosestDistanceSquared)
		{
			fClosestDistanceSquared = fDistanceSquared;
			rVecResult = vecPosition;
			bFound = true;
		}
	});
	return bFound;
}

bool XM_CALLCONV FindAttackTarget(const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, uint8_t uiFlightPlane, FXMVECTOR vecPickFrom, AttackTargetFlags_t flags, engine::AlignmentIdentifier alignment, int64_t iHighestFlightPlane, engine::registry_id_t& rRegistryTarget, XMVECTOR& rVecResult, bool& rbInLineOfSight)
{
	auto PickDistanceSquared = [&](FXMVECTOR vecEnemyPosition)
	{
		XMVECTOR vecOffset = XMVectorSubtract(vecEnemyPosition, vecPickFrom);
		return XMVectorGetX((flags & AttackTargetFlags::kHorizontal) ? XMVector2LengthSq(vecOffset) : XMVector3LengthSq(vecOffset));
	};
	auto RegistryId = [&](ShipLocator locator)
	{
		return VisitShip(rPreviousFrame, locator, [](const auto& rInterpolate, [[maybe_unused]] const auto& rPostRender, int64_t iRow)
		{
			return rInterpolate.pRegistryIds[iRow];
		});
	};

	bool bKept = false;
	bool bKeptAboveHighestPlane = false;
	XMVECTOR vecKeptPosition = XMVectorZero();
	bool bFound = false;
	float fNearestDistanceSquared = std::numeric_limits<float>::max();
	engine::registry_id_t nearestRegistryId {};
	XMVECTOR vecNearestPosition = XMVectorZero();
	ForEachEnemyShip(rPreviousFrame.interpolate, rPreviousFrame.postRender, alignment, [&](ShipLocator locator, FXMVECTOR vecEnemyPosition, [[maybe_unused]] FXMVECTOR vecVelocity)
	{
		engine::registry_id_t registryId = RegistryId(locator);
		bool bIsKept = rRegistryTarget.uuid.iValue != 0 && registryId == rRegistryTarget;
		if (NearestFlightPlane(vecEnemyPosition) > iHighestFlightPlane)
		{
			bKeptAboveHighestPlane = bKeptAboveHighestPlane || bIsKept;
			return;
		}
		if (bIsKept)
		{
			bKept = true;
			vecKeptPosition = vecEnemyPosition;
		}
		float fDistanceSquared = PickDistanceSquared(vecEnemyPosition);
		if (fDistanceSquared < fNearestDistanceSquared)
		{
			fNearestDistanceSquared = fDistanceSquared;
			nearestRegistryId = registryId;
			vecNearestPosition = vecEnemyPosition;
			bFound = true;
		}
	});
	if (!bFound)
	{
		if (bKeptAboveHighestPlane)
		{
			rRegistryTarget = {};
		}
		return false;
	}

	if (flags & AttackTargetFlags::kIgnoreLineOfSight)
	{
		if (bKept)
		{
			rVecResult = vecKeptPosition;
		}
		else
		{
			rRegistryTarget = nearestRegistryId;
			rVecResult = vecNearestPosition;
		}
		rbInLineOfSight = false;
		return true;
	}

	if (bKept && HasShipLineOfSight(rPreviousFrame, rStaticData, kiFiringClearanceTier, uiFlightPlane, vecPosition, vecKeptPosition))
	{
		rVecResult = vecKeptPosition;
		rbInLineOfSight = true;
		return true;
	}

	// Only a candidate nearer than the best one in sight so far takes a sight test.
	bool bVisibleFound = false;
	float fVisibleDistanceSquared = 0.0f;
	engine::registry_id_t visibleRegistryId {};
	XMVECTOR vecVisiblePosition = XMVectorZero();
	ForEachEnemyShip(rPreviousFrame.interpolate, rPreviousFrame.postRender, alignment, [&](ShipLocator locator, FXMVECTOR vecEnemyPosition, [[maybe_unused]] FXMVECTOR vecVelocity)
	{
		if (NearestFlightPlane(vecEnemyPosition) > iHighestFlightPlane)
		{
			return;
		}
		float fDistanceSquared = PickDistanceSquared(vecEnemyPosition);
		if ((!bVisibleFound || fDistanceSquared < fVisibleDistanceSquared) && HasShipLineOfSight(rPreviousFrame, rStaticData, kiFiringClearanceTier, uiFlightPlane, vecPosition, vecEnemyPosition))
		{
			bVisibleFound = true;
			fVisibleDistanceSquared = fDistanceSquared;
			visibleRegistryId = RegistryId(locator);
			vecVisiblePosition = vecEnemyPosition;
		}
	});

	if (bVisibleFound)
	{
		rRegistryTarget = visibleRegistryId;
		rVecResult = vecVisiblePosition;
		rbInLineOfSight = true;
	}
	else if (bKept)
	{
		rVecResult = vecKeptPosition;
		rbInLineOfSight = false;
	}
	else
	{
		rRegistryTarget = nearestRegistryId;
		rVecResult = vecNearestPosition;
		rbInLineOfSight = false;
	}
	return true;
}

void XM_CALLCONV AcquireTarget(const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, engine::AlignmentIdentifier alignment, engine::registry_id_t registryTarget, uint8_t uiFlightPlane, int64_t iHighestFlightPlane, float fBlasterSpeed, float fBlasterTargetRange, ShipFlags_t& rFlags, bool& rbLookTargetFound, XMVECTOR& rVecLookPosition)
{
	bool bTargetInRange = false;
	bool bKeptTargetInRange = false;
	bool bTargetForLook = false;
	XMVECTOR vecTargetInRangePosition = XMVectorZero();
	XMVECTOR vecTargetInRangeVelocity = XMVectorZero();
	XMVECTOR vecTargetForLookPosition = XMVectorZero();
	float fBestInRange = fBlasterTargetRange;
	float fBestForLook = std::numeric_limits<float>::max();
	// Firing and look targets require a visible enemy past arrival grace.
	ForEachEnemyShip(rPreviousFrame.interpolate, rPreviousFrame.postRender, alignment, [&](ShipLocator locator, FXMVECTOR vecTargetPosition, FXMVECTOR vecTargetVelocity)
	{
		if (!FrameInterpolate::IsVisible(vecPosition, vecTargetPosition))
		{
			return;
		}
		int64_t iTargetFlightPlane = NearestFlightPlane(vecTargetPosition);
		if (iTargetFlightPlane > iHighestFlightPlane)
		{
			return;
		}
		bool bKeptTarget = registryTarget.uuid.iValue != 0 && VisitShip(rPreviousFrame, locator, [](const auto& rInterpolate, [[maybe_unused]] const auto& rPostRender, int64_t iRow)
		{
			return rInterpolate.pRegistryIds[iRow];
		}) == registryTarget;
		float fDistance = common::Distance(vecPosition, vecTargetPosition);
		// A kept target within range wins over every nearer candidate.
		bool bInRangeCandidate = iTargetFlightPlane == uiFlightPlane && (bKeptTarget ? fDistance < fBlasterTargetRange : (!bKeptTargetInRange && fDistance < fBestInRange));
		bool bLookCandidate = fDistance < fBestForLook;
		if (!(bInRangeCandidate || bLookCandidate))
		{
			return;
		}
		if (!HasShipLineOfSight(rPreviousFrame, rStaticData, kiFiringClearanceTier, uiFlightPlane, vecPosition, vecTargetPosition))
		{
			return;
		}
		if (bInRangeCandidate)
		{
			fBestInRange = fDistance;
			vecTargetInRangePosition = vecTargetPosition;
			vecTargetInRangeVelocity = vecTargetVelocity;
			bTargetInRange = true;
			bKeptTargetInRange = bKeptTarget;
		}
		if (bLookCandidate)
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

	// The blaster spawn timer rate-limits continuous fire.
	if (bTargetInRange)
	{
		rFlags.Set(ShipFlags::kFireBlaster);
	}
}

static bool IsFlagship(const FramePostRender& rPostRender, ShipLocator locator)
{
	return locator.eType == ShipType::kBattleship && (rPostRender.pBattleships->pOrders[locator.iRow] & ShipOrders::kIsFlagship);
}

bool XM_CALLCONV FindNearestEnemyFlagship(const Frame& rFrame, FXMVECTOR vecFrom, engine::AlignmentIdentifier alignment, engine::registry_id_t& rRegistryId, XMVECTOR& rVecResult)
{
	float fClosestDistanceSquared = std::numeric_limits<float>::max();
	bool bFound = false;
	ForEachEnemyShip(rFrame.interpolate, rFrame.postRender, alignment, [&](ShipLocator locator, FXMVECTOR vecPosition, [[maybe_unused]] FXMVECTOR vecVelocity)
	{
		if (!IsFlagship(rFrame.postRender, locator))
		{
			return;
		}
		float fDistanceSquared = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(vecPosition, vecFrom)));
		if (fDistanceSquared < fClosestDistanceSquared)
		{
			fClosestDistanceSquared = fDistanceSquared;
			rRegistryId = rFrame.interpolate.pBattleships->pRegistryIds[locator.iRow];
			rVecResult = vecPosition;
			bFound = true;
		}
	});
	return bFound;
}

ShipUuid ShipUuidByGlobalId(const Frame& rFrame, engine::GlobalId globalId)
{
	std::optional<ShipLocator> optionalLocator = FindShipByGlobalId(rFrame, globalId);
	if (!optionalLocator)
	{
		return {};
	}
	return
	{
		.eType = optionalLocator->eType,
		.iUuid = VisitShip(rFrame, *optionalLocator, []([[maybe_unused]] const auto& rInterpolate, const auto& rPostRender, int64_t iRow)
		{
			return rPostRender.pIds[iRow].uuid.iValue;
		}),
	};
}

XMVECTOR ShipPosition(const FrameInterpolate& rInterpolate, ShipLocator locator)
{
	switch (locator.eType)
	{
		case ShipType::kFighter:    return rInterpolate.pFighters->pVecPositions[locator.iRow];
		case ShipType::kOmni:       return rInterpolate.pOmnis->pVecPositions[locator.iRow];
		case ShipType::kBattleship: return rInterpolate.pBattleships->pVecPositions[locator.iRow];
		case ShipType::kCount:      break;
	}
	ASSERT(false);
	std::unreachable();
}

float ShipHull(const Frame& rFrame, ShipLocator locator)
{
	return VisitShip(rFrame, locator, []([[maybe_unused]] const auto& rInterpolate, const auto& rPostRender, int64_t iRow)
	{
		if constexpr (requires { rPostRender.pfArmors; })
		{
			return rPostRender.pfArmors[iRow];
		}
		else
		{
			return rPostRender.pfHealths[iRow];
		}
	});
}

bool FindLowestPlaneShipByRegistryId(const Frame& rFrame, const Frame& rPreviousFrame, engine::registry_id_t registryId, XMVECTOR& rVecPreviousPosition)
{
	bool bFound = false;
	ForEachShipType(rFrame.interpolate, rPreviousFrame.postRender, [&](ShipType eType, const auto& rInterpolate, const auto& rGracePostRender)
	{
		for (int64_t i = 0; i < rInterpolate.iCount && !bFound; ++i)
		{
			if (rInterpolate.pRegistryIds[i] == registryId && rGracePostRender.pArrivalGracePeriods[i].count() <= 0.0f && NearestFlightPlane(rInterpolate.pVecPositions[i]) == 0)
			{
				// No row is added or removed before the Update phase ends, so the previous row matches the current one.
				rVecPreviousPosition = ShipPosition(rPreviousFrame.interpolate, ShipLocator {.eType = eType, .iRow = i});
				bFound = true;
			}
		}
	});
	return bFound;
}

} // namespace game
