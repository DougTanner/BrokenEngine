#pragma once

#include "Frame/Alignments.h"
#include "Frame/Collections/CollectionId.h"
#include "Network/NetworkProtocol.h" // ClientGuid

namespace engine
{

struct RegistryEntryTag;
using registry_id_t = id_t<RegistryEntryTag>;

// One source collection's published columns. Bound by value for the length of a single query window.
struct RegistrySourceLayer
{
	const registry_id_t* puiIds = nullptr;           // Required; ids stay unique across every bound layer
	const XMVECTOR* pVecCurrentPositions = nullptr;  // Required
	const XMVECTOR* pVecPreviousPositions = nullptr; // Optional; results fall back to the current position
	const alignment_t* pAlignments = nullptr;        // Optional; both sides bound enables Alignments::CanCollide
	std::span<const int64_t> rows {};                // Eligible rows: strictly ascending, in range
	int64_t iSourceCount = 0;
};

// One consumer collection's target column, read to derive how many consumers already hold each source.
struct RegistrySubscriptionLayer
{
	const registry_id_t* puiTargets = nullptr;
	std::span<const int64_t> rows {};
	int64_t iSourceCount = 0;
};

struct RegistryResult
{
	registry_id_t id {};
	XMVECTOR vecCurrentPosition {};
	XMVECTOR vecPreviousPosition {};
};

// One acquisition submission. puiTargets and the origin/direction/alignment columns are indexed by consumer
// row; rows selects the consumer rows to acquire for and results carries one entry per rows entry.
struct RegistryBatch
{
	registry_id_t* puiTargets = nullptr;
	const XMVECTOR* pVecOrigins = nullptr;
	const XMVECTOR* pVecDirections = nullptr;
	const alignment_t* pAlignments = nullptr;
	std::span<const int64_t> rows {};
	std::span<RegistryResult> results {};
	int64_t iSourceCount = 0;
};

struct RegistryQueryContext
{
	std::span<const RegistrySourceLayer> sourceLayers {};
	std::span<uint16_t> subscriberCounts {};
	const Alignments* pAlignments = nullptr;
};

// Size of the registry-internal scratch block for one query window's eligible rows, laid out as the row indices the
// caller writes and binds into RegistrySourceLayer::rows, followed by the subscriber counts
// BuildRegistryQueryContext derives. It cannot size RegistryBatch::rows or RegistryBatch::results, which scale
// with the caller's consumer count rather than with the source layers.
[[nodiscard]] constexpr int64_t RegistryScratchBytes(int64_t iEligibleRows)
{
	return iEligibleRows * static_cast<int64_t>(sizeof(int64_t) + sizeof(uint16_t));
}

// scratch is the whole RegistryScratchBytes block, including the row prefix the caller already filled.
[[nodiscard]] RegistryQueryContext BuildRegistryQueryContext(const Alignments& rAlignments, std::span<const RegistrySourceLayer> sourceLayers, std::span<const RegistrySubscriptionLayer> subscriptionLayers, std::span<std::byte> scratch);

// Fixed policy: fewest subscribers, then smallest angle, ties by layer order then row order.
void AcquireRegistryTargets(RegistryQueryContext& rContext, const RegistryBatch& rBatch, float fRadius);

[[nodiscard]] bool ResolveRegistryHandle(const RegistryQueryContext& rContext, registry_id_t id, RegistryResult& rResult);

void ReleaseRegistryTarget(RegistryQueryContext& rContext, registry_id_t& rId);

// Binds a foreign-typed id column as bytes. Viewing an id_t<T> array through a const uuid_t* or a
// const registry_id_t* is undefined behavior — the specializations are not similar types — so the ownership
// layer erases the element type here and reads each element back through uuid_t.
template <typename T>
[[nodiscard]] const std::byte* RegistryIdBytes(const id_t<T>* puiIds)
{
	static_assert(sizeof(id_t<T>) == sizeof(uuid_t));
	static_assert(std::is_trivially_copyable_v<id_t<T>>);
	return reinterpret_cast<const std::byte*>(puiIds);
}

// Main-thread, post-tick identity and liveness. Held by value by its caller: a span over a builder's temporary
// layer would dangle the moment the builder returned, so there is no multi-layer view type.
struct RegistryOwnershipLayer
{
	const std::byte* pIdBytes = nullptr;     // Required; element stride sizeof(uuid_t), bind via RegistryIdBytes
	const global_id_t* pGlobalIds = nullptr; // Optional; a layer without it matches no global id
	ClientGuid* pClientGuids = nullptr;      // Optional; sole write channel, CRC-excluded columns only
	int64_t iCount = 0;
};

// Counts every bound row; the caller's choice of layer is the filter.
[[nodiscard]] int64_t CountRegistryRows(const RegistryOwnershipLayer& rLayer);

// The registry's only write.
bool AssignRegistryClientGuid(const RegistryOwnershipLayer& rLayer, global_id_t globalId, const ClientGuid& rGuid);

[[nodiscard]] uuid_t RegistryUuidByGlobalId(const RegistryOwnershipLayer& rLayer, global_id_t globalId);

} // namespace engine
