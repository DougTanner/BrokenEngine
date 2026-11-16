#include "FrameRegistry.h"

namespace engine
{

static Uuid OwnershipUuid(const RegistryOwnershipLayer& rLayer, int64_t iIndex)
{
	Uuid uuid {};
	std::memcpy(&uuid, rLayer.pIdBytes + iIndex * sizeof(Uuid), sizeof(Uuid));
	return uuid;
}

void AssignRegistryClientGuid(const RegistryOwnershipLayer& rLayer, GlobalId globalId, const ClientGuid& rGuid)
{
	ASSERT(common::gpMultithreading->IsMainThread());

	for (int64_t i = 0; i < rLayer.iCount; ++i)
	{
		if (rLayer.pGlobalIds[i] == globalId)
		{
			rLayer.pClientGuids[i] = rGuid;
			return;
		}
	}

	ASSERT(false);
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
