#pragma once

#include "Frame/Collections/CollectionId.h"
#include "Network/NetworkProtocol.h" // ClientGuid

namespace engine
{

struct RegistryEntryTag;
using registry_id_t = Id<RegistryEntryTag>;

// Main-thread, post-tick identity and liveness. Held by value by its caller: a span over a builder's temporary
// layer would dangle the moment the builder returned, so there is no multi-layer view type.
struct RegistryOwnershipLayer
{
	const GlobalId* pGlobalIds = nullptr; // Required
	ClientGuid* pClientGuids = nullptr;      // Required
	int64_t iCount = 0; // The caller's choice of layer is the filter.
};

} // namespace engine
