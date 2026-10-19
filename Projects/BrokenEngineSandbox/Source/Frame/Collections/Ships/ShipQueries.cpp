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

bool XM_CALLCONV FindNearestEnemyShipInArc(const Frame& rFrame, FXMVECTOR vecFrom, FXMVECTOR vecAxisNormal, float fHalfAngle, engine::AlignmentIdentifier alignment, XMVECTOR& rVecResult)
{
	float fClosestDistanceSquared = std::numeric_limits<float>::max();
	bool bFound = false;
	ForEachEnemyShip(rFrame.interpolate, rFrame.postRender, alignment, [&]([[maybe_unused]] ShipLocator locator, FXMVECTOR vecPosition, [[maybe_unused]] FXMVECTOR vecVelocity)
	{
		if (!FrameInterpolate::IsVisible(vecPosition, vecFrom))
		{
			return;
		}
		XMVECTOR vecToCandidate = XMVectorSubtract(vecPosition, vecFrom);
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

	int64_t iSubscriberCount = rSubscribers.iCount;
	int64_t iAscendingCount = iSubscriberCount;
	int64_t iEligibleCount = 0;
	ForEachShipType(rFrame.interpolate, rGraceFrame.postRender, [&]([[maybe_unused]] ShipType eType, const auto& rInterpolate, const auto& rGracePostRender)
	{
		iAscendingCount = std::max(iAscendingCount, rInterpolate.iCount);
		for (int64_t i = 0; i < rInterpolate.iCount; ++i)
		{
			iEligibleCount += IsEligible(rInterpolate, rGracePostRender, i) ? 1 : 0;
		}
	});

	// Reserve all window storage together. PushBuffer may grow the workbuffer, so no pointer is published until
	// the combined reservation has returned. The source rows, registry scratch, padding, and layer descriptors
	// then remain in one allocation for the context's lifetime.
	static constexpr int64_t kiLayerCount = std::to_underlying(ShipType::kCount);
	int64_t iAscendingBytes = iAscendingCount * static_cast<int64_t>(sizeof(int64_t));
	int64_t iScratchBytes = engine::RegistryScratchBytes(iEligibleCount);
	int64_t iLayerOffset = common::RoundUp(iAscendingBytes + iScratchBytes, 16i64);
	int64_t iTotalBytes = iLayerOffset + kiLayerCount * static_cast<int64_t>(sizeof(engine::RegistrySourceLayer));
	auto pBuffer = rWorkbuffer.PushBuffer<std::byte*>(iTotalBytes);
	std::byte* pBufferBytes = pBuffer.mpData;
	int64_t* pAscendingRows = reinterpret_cast<int64_t*>(pBufferBytes);
	std::byte* pScratch = pBufferBytes + iAscendingBytes;
	engine::RegistrySourceLayer* pLayers = reinterpret_cast<engine::RegistrySourceLayer*>(pBufferBytes + iLayerOffset);

	std::iota(pAscendingRows, pAscendingRows + iAscendingCount, 0i64);

	// The registry block stores caller-filled eligible rows, layer after layer, before derived subscriber counts;
	// ascending row order within each layer resolves exact ranking ties deterministically.
	int64_t* pEligibleRows = reinterpret_cast<int64_t*>(pScratch);
	int64_t iEligibleRow = 0;
	ForEachShipType(rFrame.interpolate, rGraceFrame.postRender, [&](ShipType eType, const auto& rInterpolate, const auto& rGracePostRender)
	{
		int64_t iLayerFirstRow = iEligibleRow;
		for (int64_t i = 0; i < rInterpolate.iCount; ++i)
		{
			if (IsEligible(rInterpolate, rGracePostRender, i))
			{
				pEligibleRows[iEligibleRow++] = i;
			}
		}

		pLayers[std::to_underlying(eType)] =
		{
			.pIds = rInterpolate.pRegistryIds,
			.pVecCurrentPositions = rInterpolate.pVecPositions,
			.rows = std::span<const int64_t>(pEligibleRows + iLayerFirstRow, static_cast<size_t>(iEligibleRow - iLayerFirstRow)),
			.iSourceCount = rInterpolate.iCount,
		};
	});
	ForEachShipType(rFrame, [&](ShipType eType, [[maybe_unused]] const auto& rInterpolate, const auto& rPostRender)
	{
		pLayers[std::to_underlying(eType)].pAlignments = rPostRender.pAlignments;
	});
	// Previous rows still align with the current ones: no row is added or removed before the Update phase ends.
	if (pPreviousFrame != nullptr)
	{
		ForEachShipType(*pPreviousFrame, [&](ShipType eType, const auto& rPreviousInterpolate, [[maybe_unused]] const auto& rPreviousPostRender)
		{
			pLayers[std::to_underlying(eType)].pVecPreviousPositions = rPreviousInterpolate.pVecPositions;
		});
	}
	std::span<const engine::RegistrySourceLayer> sourceLayers(pLayers, static_cast<size_t>(kiLayerCount));

	engine::RegistrySubscriptionLayer subscriptionLayer
	{
		.pTargets = rSubscribers.puiRegistryTargets,
		.rows = std::span<const int64_t>(pAscendingRows, static_cast<size_t>(iSubscriberCount)),
		.iSourceCount = iSubscriberCount,
	};

	return
	{
		.buffer = std::move(pBuffer),
		.context = engine::BuildRegistryQueryContext(rFrame.postRender.alignments, sourceLayers, std::span<const engine::RegistrySubscriptionLayer>(&subscriptionLayer, 1), std::span<std::byte>(pScratch, static_cast<size_t>(iScratchBytes))),
	};
}

} // namespace game
