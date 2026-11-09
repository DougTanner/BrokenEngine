#include "FrameRegistry.h"

namespace engine
{

struct EligibleRowReference
{
	const RegistrySourceLayer* pLayer = nullptr;
	int64_t iRow = 0;
};

static EligibleRowReference FindEligibleRow(std::span<const RegistrySourceLayer> sourceLayers, registry_id_t id)
{
	for (const RegistrySourceLayer& rLayer : sourceLayers)
	{
		for (int64_t iRow : rLayer.rows)
		{
			if (rLayer.pIds[iRow] == id)
			{
				return {.pLayer = &rLayer, .iRow = iRow};
			}
		}
	}

	return {};
}

static RegistryResult MakeResult(const RegistrySourceLayer& rLayer, int64_t iRow)
{
	RegistryResult result {};
	result.id = rLayer.pIds[iRow];
	result.vecCurrentPosition = rLayer.pVecCurrentPositions[iRow];
	result.vecPreviousPosition = rLayer.pVecPreviousPositions[iRow];
	return result;
}

static Uuid OwnershipUuid(const RegistryOwnershipLayer& rLayer, int64_t iIndex)
{
	Uuid uuid {};
	std::memcpy(&uuid, rLayer.pIdBytes + iIndex * sizeof(Uuid), sizeof(Uuid));
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

void ValidateRegistrySourceLayers(std::span<const RegistrySourceLayer> sourceLayers)
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
			registry_id_t idA = rLayerA.pIds[iRowA];
			ASSERT((idA.uuid.iValue != 0));

			int64_t iIndexB = 0;
			for (const RegistrySourceLayer& rLayerB : sourceLayers)
			{
				for (int64_t iRowB : rLayerB.rows)
				{
					ASSERT(iIndexB == iIndexA || rLayerB.pIds[iRowB] != idA);
					++iIndexB;
				}
			}

			++iIndexA;
		}
	}
}

#endif // BT_DEBUG

bool ResolveRegistryHandle(std::span<const RegistrySourceLayer> sourceLayers, registry_id_t id, RegistryResult& rResult)
{
	EligibleRowReference found = FindEligibleRow(sourceLayers, id);
	if (found.pLayer == nullptr)
	{
		return false;
	}

	rResult = MakeResult(*found.pLayer, found.iRow);
	return true;
}

bool AssignRegistryClientGuid(const RegistryOwnershipLayer& rLayer, GlobalId globalId, const ClientGuid& rGuid)
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

Uuid RegistryUuidByGlobalId(const RegistryOwnershipLayer& rLayer, GlobalId globalId)
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
