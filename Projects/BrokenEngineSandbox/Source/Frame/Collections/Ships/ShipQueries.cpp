#include "ShipQueries.h"

#include "Frame/Collections/Missiles/Missiles.h"

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

bool XM_CALLCONV FindNearestEnemyShip(const Frame& rFrame, FXMVECTOR vecFrom, engine::AlignmentIdentifier alignment, std::optional<int64_t> optionalFlightPlane, XMVECTOR& rVecResult)
{
	float fClosestDistanceSquared = std::numeric_limits<float>::max();
	bool bFound = false;
	ForEachEnemyShip(rFrame.interpolate, rFrame.postRender, alignment, [&](ShipLocator locator, FXMVECTOR vecPosition, [[maybe_unused]] FXMVECTOR vecVelocity)
	{
		if (optionalFlightPlane && ShipFlightPlane(rFrame.interpolate, locator) != *optionalFlightPlane)
		{
			return;
		}
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

bool XM_CALLCONV FindNearestEnemyShipInArc(const Frame& rFrame, FXMVECTOR vecFrom, FXMVECTOR vecAxisNormal, float fHalfAngle, engine::AlignmentIdentifier alignment, int64_t iFlightPlane, XMVECTOR& rVecResult)
{
	float fClosestDistanceSquared = std::numeric_limits<float>::max();
	bool bFound = false;
	ForEachEnemyShip(rFrame.interpolate, rFrame.postRender, alignment, [&](ShipLocator locator, FXMVECTOR vecPosition, [[maybe_unused]] FXMVECTOR vecVelocity)
	{
		if (!FrameInterpolate::IsVisible(vecPosition, vecFrom))
		{
			return;
		}
		if (ShipFlightPlane(rFrame.interpolate, locator) != iFlightPlane)
		{
			return;
		}
		XMVECTOR vecToCandidate = XMVectorSetZ(XMVectorSubtract(vecPosition, vecFrom), 0.0f);
		float fAngle = XMVectorGetX(XMVector3AngleBetweenNormals(vecAxisNormal, XMVector3Normalize(vecToCandidate)));
		if (!(fAngle <= fHalfAngle))
		{
			return;
		}
		float fDistanceSquared = XMVectorGetX(XMVector3LengthSq(vecToCandidate));
		if (fDistanceSquared < fClosestDistanceSquared)
		{
			fClosestDistanceSquared = fDistanceSquared;
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

int64_t ShipFlightPlane(const FrameInterpolate& rInterpolate, ShipLocator locator)
{
	switch (locator.eType)
	{
		case ShipType::kFighter:    return rInterpolate.pFighters->puiFlightPlanes[locator.iRow];
		case ShipType::kOmni:       return rInterpolate.pOmnis->puiFlightPlanes[locator.iRow];
		case ShipType::kBattleship: return rInterpolate.pBattleships->puiFlightPlanes[locator.iRow];
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

RegistryWindow BuildShipRegistryWindow(const Frame& rFrame, const Frame* pPreviousFrame, const MissilesPostRender& rSubscribers)
{
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	const Frame& rGraceFrame = pPreviousFrame != nullptr ? *pPreviousFrame : rFrame;

	auto IsEligible = [](const auto& rInterpolate, const auto& rGracePostRender, int64_t i)
	{
		return (rInterpolate.pRegistryIds[i].uuid.iValue != 0) && rGracePostRender.pArrivalGracePeriods[i].count() <= 0.0f;
	};

	static constexpr int64_t kiPlaneCount = std::ssize(kFlightPlanes);
	int64_t iSubscriberCount = rSubscribers.iCount;
	int64_t iAscendingCount = iSubscriberCount;
	std::array<int64_t, kiPlaneCount> eligibleCounts {};
	ForEachShipType(rFrame.interpolate, rGraceFrame.postRender, [&]([[maybe_unused]] ShipType eType, const auto& rInterpolate, const auto& rGracePostRender)
	{
		iAscendingCount = std::max(iAscendingCount, rInterpolate.iCount);
		for (int64_t i = 0; i < rInterpolate.iCount; ++i)
		{
			uint8_t uiPlane = rInterpolate.puiFlightPlanes[i];
			if (uiPlane != kuiNoFlightPlane && IsEligible(rInterpolate, rGracePostRender, i))
			{
				++eligibleCounts[uiPlane];
			}
		}
	});

	// Reserve all window storage together. PushBuffer may grow the workbuffer, so no pointer is published until
	// the combined reservation has returned. The source rows, every plane's registry scratch, padding, and every
	// plane's layer descriptors then remain in one allocation for the contexts' lifetime. Each scratch block starts
	// 16-byte aligned because its int64_t rows follow the previous block's uint16_t subscriber counts.
	static constexpr int64_t kiLayerCount = std::to_underlying(ShipType::kCount);
	int64_t iAscendingBytes = iAscendingCount * static_cast<int64_t>(sizeof(int64_t));
	std::array<int64_t, kiPlaneCount> scratchOffsets {};
	int64_t iOffset = iAscendingBytes;
	for (int64_t i = 0; i < kiPlaneCount; ++i)
	{
		scratchOffsets[i] = common::RoundUp(iOffset, 16i64);
		iOffset = scratchOffsets[i] + engine::RegistryScratchBytes(eligibleCounts[i]);
	}
	int64_t iLayerOffset = common::RoundUp(iOffset, 16i64);
	int64_t iTotalBytes = iLayerOffset + kiPlaneCount * kiLayerCount * static_cast<int64_t>(sizeof(engine::RegistrySourceLayer));
	auto pBuffer = rWorkbuffer.PushBuffer<std::byte*>(iTotalBytes);
	std::byte* pBufferBytes = pBuffer.mpData;
	int64_t* pAscendingRows = reinterpret_cast<int64_t*>(pBufferBytes);
	// Plane i's layer descriptors start at pLayers + i * kiLayerCount.
	engine::RegistrySourceLayer* pLayers = reinterpret_cast<engine::RegistrySourceLayer*>(pBufferBytes + iLayerOffset);

	std::iota(pAscendingRows, pAscendingRows + iAscendingCount, 0i64);

	// Each plane's registry block stores that plane's caller-filled eligible rows, layer after layer, before derived
	// subscriber counts; ascending row order within each layer resolves exact ranking ties deterministically.
	std::array<int64_t*, kiPlaneCount> eligibleRows {};
	std::array<int64_t, kiPlaneCount> filledCounts {};
	for (int64_t i = 0; i < kiPlaneCount; ++i)
	{
		eligibleRows[i] = reinterpret_cast<int64_t*>(pBufferBytes + scratchOffsets[i]);
	}
	ForEachShipType(rFrame.interpolate, rGraceFrame.postRender, [&](ShipType eType, const auto& rInterpolate, const auto& rGracePostRender)
	{
		std::array<int64_t, kiPlaneCount> layerFirstRows = filledCounts;
		for (int64_t i = 0; i < rInterpolate.iCount; ++i)
		{
			uint8_t uiPlane = rInterpolate.puiFlightPlanes[i];
			if (uiPlane != kuiNoFlightPlane && IsEligible(rInterpolate, rGracePostRender, i))
			{
				eligibleRows[uiPlane][filledCounts[uiPlane]++] = i;
			}
		}

		for (int64_t i = 0; i < kiPlaneCount; ++i)
		{
			pLayers[i * kiLayerCount + std::to_underlying(eType)] =
			{
				.pIds = rInterpolate.pRegistryIds,
				.pVecCurrentPositions = rInterpolate.pVecPositions,
				.rows = std::span<const int64_t>(eligibleRows[i] + layerFirstRows[i], static_cast<size_t>(filledCounts[i] - layerFirstRows[i])),
				.iSourceCount = rInterpolate.iCount,
			};
		}
	});
	ForEachShipType(rFrame, [&](ShipType eType, [[maybe_unused]] const auto& rInterpolate, const auto& rPostRender)
	{
		for (int64_t i = 0; i < kiPlaneCount; ++i)
		{
			pLayers[i * kiLayerCount + std::to_underlying(eType)].pAlignments = rPostRender.pAlignments;
		}
	});
	// Previous rows still align with the current ones: no row is added or removed before the Update phase ends.
	if (pPreviousFrame != nullptr)
	{
		ForEachShipType(*pPreviousFrame, [&](ShipType eType, const auto& rPreviousInterpolate, [[maybe_unused]] const auto& rPreviousPostRender)
		{
			for (int64_t i = 0; i < kiPlaneCount; ++i)
			{
				pLayers[i * kiLayerCount + std::to_underlying(eType)].pVecPreviousPositions = rPreviousInterpolate.pVecPositions;
			}
		});
	}

	engine::RegistrySubscriptionLayer subscriptionLayer
	{
		.pTargets = rSubscribers.puiRegistryTargets,
		.rows = std::span<const int64_t>(pAscendingRows, static_cast<size_t>(iSubscriberCount)),
		.iSourceCount = iSubscriberCount,
	};

	RegistryWindow window {.buffer = std::move(pBuffer)};
	for (int64_t i = 0; i < kiPlaneCount; ++i)
	{
		std::span<const engine::RegistrySourceLayer> sourceLayers(pLayers + i * kiLayerCount, static_cast<size_t>(kiLayerCount));
		std::span<std::byte> scratch(pBufferBytes + scratchOffsets[i], static_cast<size_t>(engine::RegistryScratchBytes(eligibleCounts[i])));
		window.contexts[i] = engine::BuildRegistryQueryContext(rFrame.postRender.alignments, sourceLayers, std::span<const engine::RegistrySubscriptionLayer>(&subscriptionLayer, 1), scratch);
	}
	return window;
}

} // namespace game
