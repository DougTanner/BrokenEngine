#pragma once

#include "Frame/Collections/CollectionId.h"
#include "Network/NetworkProtocol.h" // ClientGuid

namespace engine
{

struct RegistryEntryTag;
using registry_id_t = Id<RegistryEntryTag>;

// One source collection's published columns. Bound by value for the length of a single query window.
struct RegistrySourceLayer
{
	const registry_id_t* pIds = nullptr;           // Required; ids stay unique across every bound layer
	const XMVECTOR* pVecCurrentPositions = nullptr;  // Required
	const XMVECTOR* pVecPreviousPositions = nullptr; // Required
	std::span<const int64_t> rows {};                // Eligible rows: strictly ascending, in range
	int64_t iSourceCount = 0;
};

struct RegistryResult
{
	registry_id_t id {};
	XMVECTOR vecCurrentPosition {};
	XMVECTOR vecPreviousPosition {};
};

#if defined(BT_DEBUG)
void ValidateRegistrySourceLayers(std::span<const RegistrySourceLayer> sourceLayers);
#endif

// id must be nonzero.
[[nodiscard]] bool ResolveRegistryHandle(std::span<const RegistrySourceLayer> sourceLayers, registry_id_t id, RegistryResult& rResult);

// Binds a foreign-typed id column as bytes. Viewing an Id<T> array through a const Uuid* or a
// const registry_id_t* is undefined behavior — the specializations are not similar types — so the ownership
// layer erases the element type here and reads each element back through Uuid.
template <typename T>
[[nodiscard]] const std::byte* RegistryIdBytes(const Id<T>* pIds)
{
	static_assert(sizeof(Id<T>) == sizeof(Uuid));
	static_assert(std::is_trivially_copyable_v<Id<T>>);
	return reinterpret_cast<const std::byte*>(pIds);
}

// Main-thread, post-tick identity and liveness. Held by value by its caller: a span over a builder's temporary
// layer would dangle the moment the builder returned, so there is no multi-layer view type.
struct RegistryOwnershipLayer
{
	const std::byte* pIdBytes = nullptr;     // Required; element stride sizeof(Uuid), bind via RegistryIdBytes
	const GlobalId* pGlobalIds = nullptr; // Optional; a layer without it matches no global id
	ClientGuid* pClientGuids = nullptr;      // Optional; sole write channel, CRC-excluded columns only
	int64_t iCount = 0; // The caller's choice of layer is the filter.
};

bool AssignRegistryClientGuid(const RegistryOwnershipLayer& rLayer, GlobalId globalId, const ClientGuid& rGuid);

[[nodiscard]] Uuid RegistryUuidByGlobalId(const RegistryOwnershipLayer& rLayer, GlobalId globalId);

} // namespace engine
