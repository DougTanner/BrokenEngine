#include "FrameRegistry.h"

namespace engine
{

// One eligible source row located by id. iEligibleIndex is the row's position in the flat scan order across
// every bound layer, which is also its slot in RegistryQueryContext::subscriberCounts.
struct EligibleRowRef
{
	const RegistrySourceLayer* pLayer = nullptr;
	int64_t iRow = 0;
	int64_t iEligibleIndex = 0;
};

static int64_t TotalEligibleRows(std::span<const RegistrySourceLayer> sourceLayers)
{
	int64_t iTotal = 0;
	for (const RegistrySourceLayer& rLayer : sourceLayers)
	{
		iTotal += static_cast<int64_t>(rLayer.rows.size());
	}
	return iTotal;
}

static EligibleRowRef FindEligibleRow(std::span<const RegistrySourceLayer> sourceLayers, registry_id_t id)
{
	if (!id.IsValid())
	{
		return {};
	}

	int64_t iEligibleIndex = 0;
	for (const RegistrySourceLayer& rLayer : sourceLayers)
	{
		for (int64_t iRow : rLayer.rows)
		{
			if (rLayer.puiIds[iRow] == id)
			{
				return {&rLayer, iRow, iEligibleIndex};
			}
			++iEligibleIndex;
		}
	}

	return {};
}

static RegistryResult MakeResult(const RegistrySourceLayer& rLayer, int64_t iRow)
{
	RegistryResult result {};
	result.id = rLayer.puiIds[iRow];
	result.vecCurrentPosition = rLayer.pVecCurrentPositions[iRow];
	result.vecPreviousPosition = rLayer.pVecPreviousPositions != nullptr ? rLayer.pVecPreviousPositions[iRow] : result.vecCurrentPosition;
	return result;
}

static uuid_t OwnershipUuid(const RegistryOwnershipLayer& rLayer, int64_t iIndex)
{
	uuid_t uuid {};
	std::memcpy(&uuid, rLayer.pIdBytes + iIndex * sizeof(uuid_t), sizeof(uuid_t));
	return uuid;
}

#if defined(BT_DEBUG)

static void ValidateRows(std::span<const int64_t> rows, int64_t iSourceCount)
{
	int64_t iPrevious = -1;
	for (int64_t iRow : rows)
	{
		ASSERT(iRow > iPrevious && iRow < iSourceCount);
		iPrevious = iRow;
	}
}

static void ValidateSourceLayers(std::span<const RegistrySourceLayer> sourceLayers)
{
	for (const RegistrySourceLayer& rLayer : sourceLayers)
	{
		ValidateRows(rLayer.rows, rLayer.iSourceCount);
	}

	int64_t iIndexA = 0;
	for (const RegistrySourceLayer& rLayerA : sourceLayers)
	{
		for (int64_t iRowA : rLayerA.rows)
		{
			const registry_id_t idA = rLayerA.puiIds[iRowA];
			ASSERT(idA.IsValid());

			int64_t iIndexB = 0;
			for (const RegistrySourceLayer& rLayerB : sourceLayers)
			{
				for (int64_t iRowB : rLayerB.rows)
				{
					ASSERT(iIndexB == iIndexA || rLayerB.puiIds[iRowB] != idA);
					++iIndexB;
				}
			}

			++iIndexA;
		}
	}
}

#endif // BT_DEBUG

int64_t RegistryScratchBytes(int64_t iEligibleRows)
{
	return iEligibleRows * static_cast<int64_t>(sizeof(int64_t) + sizeof(uint16_t));
}

RegistryQueryContext BuildRegistryQueryContext(const Alignments& rAlignments, std::span<const RegistrySourceLayer> sourceLayers, std::span<const RegistrySubscriptionLayer> subscriptionLayers, std::span<std::byte> scratch)
{
#if defined(BT_DEBUG)
	ValidateSourceLayers(sourceLayers);
	for (const RegistrySubscriptionLayer& rLayer : subscriptionLayers)
	{
		ValidateRows(rLayer.rows, rLayer.iSourceCount);
	}
#endif

	const int64_t iEligibleRows = TotalEligibleRows(sourceLayers);
	ASSERT(static_cast<int64_t>(scratch.size()) >= RegistryScratchBytes(iEligibleRows));

	RegistryQueryContext context {};
	context.sourceLayers = sourceLayers;
	context.pAlignments = &rAlignments;

	if (iEligibleRows > 0)
	{
		// The row indices already bound into RegistrySourceLayer::rows occupy the front of the block.
		uint16_t* puiCounts = reinterpret_cast<uint16_t*>(scratch.data() + iEligibleRows * sizeof(int64_t));
		std::memset(puiCounts, 0, static_cast<size_t>(iEligibleRows) * sizeof(uint16_t));
		context.subscriberCounts = std::span<uint16_t>(puiCounts, static_cast<size_t>(iEligibleRows));
	}

	for (const RegistrySubscriptionLayer& rLayer : subscriptionLayers)
	{
		for (int64_t iRow : rLayer.rows)
		{
			const EligibleRowRef found = FindEligibleRow(sourceLayers, rLayer.puiTargets[iRow]);
			if (found.pLayer != nullptr)
			{
				ASSERT(context.subscriberCounts[found.iEligibleIndex] < std::numeric_limits<uint16_t>::max());
				++context.subscriberCounts[found.iEligibleIndex];
			}
		}
	}

	return context;
}

void AcquireRegistryTargets(RegistryQueryContext& rContext, const RegistryBatch& rBatch, float fRadius)
{
	ASSERT(rBatch.rows.size() == rBatch.results.size());
#if defined(BT_DEBUG)
	ValidateRows(rBatch.rows, rBatch.iSourceCount);
#endif

	const float fRadiusSquared = fRadius * fRadius;

	for (size_t uiEntry = 0; uiEntry < rBatch.rows.size(); ++uiEntry)
	{
		const int64_t iConsumerRow = rBatch.rows[uiEntry];
		const XMVECTOR vecOrigin = rBatch.pVecOrigins[iConsumerRow];
		const XMVECTOR vecDirection = rBatch.pVecDirections[iConsumerRow];
		const alignment_t consumerAlignment = rBatch.pAlignments != nullptr ? rBatch.pAlignments[iConsumerRow] : alignment_t {};

		const RegistrySourceLayer* pBestLayer = nullptr;
		int64_t iBestRow = 0;
		int64_t iBestEligibleIndex = 0;
		uint16_t uiBestSubscribers = std::numeric_limits<uint16_t>::max();
		float fBestAngle = std::numeric_limits<float>::max();

		int64_t iEligibleIndex = 0;
		for (const RegistrySourceLayer& rLayer : rContext.sourceLayers)
		{
			const bool bFilterAlignment = rLayer.pAlignments != nullptr && rBatch.pAlignments != nullptr;
			for (int64_t iRow : rLayer.rows)
			{
				const int64_t iCandidateIndex = iEligibleIndex++;

				if (bFilterAlignment && !rContext.pAlignments->CanCollide(consumerAlignment, rLayer.pAlignments[iRow]))
				{
					continue;
				}

				const XMVECTOR vecToSource = XMVectorSubtract(rLayer.pVecCurrentPositions[iRow], vecOrigin);
				if (XMVectorGetX(XMVector3LengthSq(vecToSource)) > fRadiusSquared)
				{
					continue;
				}

				const float fAngle = std::abs(XMVectorGetX(XMVector3AngleBetweenNormals(vecDirection, XMVector3Normalize(vecToSource))));
				const uint16_t uiSubscribers = rContext.subscriberCounts[iCandidateIndex];

				// Strictly better wins, so an exact tie leaves the earlier layer and row in place.
				if (uiSubscribers < uiBestSubscribers || (uiSubscribers == uiBestSubscribers && fAngle < fBestAngle))
				{
					uiBestSubscribers = uiSubscribers;
					fBestAngle = fAngle;
					pBestLayer = &rLayer;
					iBestRow = iRow;
					iBestEligibleIndex = iCandidateIndex;
				}
			}
		}

		RegistryResult& rResult = rBatch.results[uiEntry];
		rResult = pBestLayer != nullptr ? MakeResult(*pBestLayer, iBestRow) : RegistryResult {};
		if (pBestLayer != nullptr)
		{
			// Later entries in this same batch see the subscription this one just took.
			ASSERT(rContext.subscriberCounts[iBestEligibleIndex] < std::numeric_limits<uint16_t>::max());
			++rContext.subscriberCounts[iBestEligibleIndex];
		}

		rBatch.puiTargets[iConsumerRow] = rResult.id;
	}
}

bool ResolveRegistryHandle(const RegistryQueryContext& rContext, registry_id_t id, RegistryResult& rResult)
{
	const EligibleRowRef found = FindEligibleRow(rContext.sourceLayers, id);
	if (found.pLayer == nullptr)
	{
		return false;
	}

	rResult = MakeResult(*found.pLayer, found.iRow);
	return true;
}

void ReleaseRegistryTarget(RegistryQueryContext& rContext, registry_id_t& rId)
{
	const EligibleRowRef found = FindEligibleRow(rContext.sourceLayers, rId);
	if (found.pLayer != nullptr)
	{
		ASSERT(rContext.subscriberCounts[found.iEligibleIndex] > 0);
		--rContext.subscriberCounts[found.iEligibleIndex];
	}

	rId = {};
}

int64_t CountRegistryRows(const RegistryOwnershipLayer& rLayer)
{
	return rLayer.iCount;
}

bool AssignRegistryClientGuid(const RegistryOwnershipLayer& rLayer, global_id_t globalId, const ClientGuid& rGuid)
{
	ASSERT(common::gpMultithreading->IsMainThread());

	if (rLayer.pGlobalIds == nullptr || rLayer.pClientGuids == nullptr)
	{
		return false;
	}

	for (int64_t i = 0; i < rLayer.iCount; ++i)
	{
		if (rLayer.pGlobalIds[i] == globalId)
		{
			rLayer.pClientGuids[i] = rGuid;
			return true;
		}
	}

	return false;
}

uuid_t RegistryUuidByGlobalId(const RegistryOwnershipLayer& rLayer, global_id_t globalId)
{
	if (rLayer.pGlobalIds == nullptr)
	{
		return {};
	}

	for (int64_t i = 0; i < rLayer.iCount; ++i)
	{
		if (rLayer.pGlobalIds[i] == globalId)
		{
			return OwnershipUuid(rLayer, i);
		}
	}

	return {};
}

} // namespace engine
