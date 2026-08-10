#pragma once

#include "CollectionMemory.h"

namespace engine
{

struct FramePostRenderBase;

// ============================================================================
// INDEXABLE COLLECTION HELPERS
// ============================================================================
// Add / remove lifecycle for paired Interpolate/PostRender collections. Relies on
// GrowCapacityWithCopy / SwapElement from CollectionMemory.h (included above).

// Increments counts for paired Interpolate/PostRender collections and returns spawn index.
template <typename TInterpolate, typename TPostRender>
inline int64_t AddElement(TInterpolate& rInterpolate, TPostRender& rPostRender)
{
	++rInterpolate.iCount;
	++rPostRender.iCount;
	int64_t iSpawnIndex = rInterpolate.iCount - 1;
	ASSERT(iSpawnIndex < rInterpolate.iCapacity);
	return iSpawnIndex;
}

// Grows paired Interpolate/PostRender collections if capacity is insufficient for spawning.
// Returns true if growth occurred, false otherwise.
// Usage: GrowPairedCollections(rInterpolate, rPostRender, rInterpolate.Members(), rPostRender.Members());
template <typename TInterpolate, typename TPostRender, typename TInterpolateTuple, typename TPostRenderTuple>
bool GrowPairedCollections(TInterpolate& rInterpolate, TPostRender& rPostRender, TInterpolateTuple&& interpolateTuple, TPostRenderTuple&& postRenderTuple)
{
	if (rInterpolate.iCount + 1 <= rInterpolate.iCapacity)
	{
		return false;
	}

	// Deterministic growth is 2 * capacity + 1 because capacity participates in the collection CRC.
	int64_t iNewCapacity = 2 * rInterpolate.iCapacity + 1;

	ASSERT(rInterpolate.iCount == rPostRender.iCount);
	GrowCapacityWithCopy(rInterpolate, iNewCapacity, rInterpolate.iCount, std::forward<TInterpolateTuple>(interpolateTuple));
	GrowCapacityWithCopy(rPostRender, iNewCapacity, rPostRender.iCount, std::forward<TPostRenderTuple>(postRenderTuple));

	return true;
}

// Internal insertion primitive. Invokes generateId exactly once, then records that ID at the new row.
template <typename TInterpolate, typename TPostRender, typename TGenerateId>
std::tuple<int64_t, typename TInterpolate::id_t> AddGeneratedIndexableElement(TInterpolate& rInterpolate, TPostRender& rPostRender, TGenerateId&& generateId)
{
	// Heap: unordered_map::insert_or_assign may allocate a new bucket or node for the ID-to-index entry.
	// The map must persist across frames for stable ID lookups, so workbuffer and static arrays are not viable.
	ScopedSuppressAllocationTracking suppress;
	int64_t iSpawnIndex = AddElement(rInterpolate, rPostRender);

	typename TInterpolate::id_t newId = generateId();
	rInterpolate.idToIndexMap.insert_or_assign(newId, iSpawnIndex);

	return {iSpawnIndex, newId};
}

// Increments counts, generates unique ID, and updates idToIndexMap for indexable collections.
// Returns tuple of (spawnIndex, newId).
// Usage: auto [uiIndex, newId] = AddIndexableElement(rInterpolate, rPostRender, rFramePostRender);
template <typename TInterpolate, typename TPostRender>
std::tuple<int64_t, typename TInterpolate::id_t> AddIndexableElement(TInterpolate& rInterpolate, TPostRender& rPostRender, FramePostRenderBase& rFramePostRender)
{
	using id_t = typename TInterpolate::id_t;
	return AddGeneratedIndexableElement(rInterpolate, rPostRender, [&rFramePostRender]()
	{
		return id_t::Generate(rFramePostRender);
	});
}

// Increments counts, generates visual unique ID, and updates idToIndexMap for visual-only collections.
// Uses GenerateVisualUuid() so visual object creation does not perturb the main UUID sequence.
#if defined(BT_CLIENT)
template <typename TInterpolate, typename TPostRender>
std::tuple<int64_t, typename TInterpolate::id_t> AddVisualIndexableElement(TInterpolate& rInterpolate, TPostRender& rPostRender, FramePostRenderBase& rFramePostRender)
{
	using id_t = typename TInterpolate::id_t;
	return AddGeneratedIndexableElement(rInterpolate, rPostRender, [&rFramePostRender]()
	{
		return id_t::GenerateVisual(rFramePostRender);
	});
}
#endif // BT_CLIENT

// Increments counts, reuses an existing ID, and updates idToIndexMap for indexable collections.
// Returns tuple of (spawnIndex, existingId).
// Usage: auto [uiIndex, id] = AddIndexableElementWithId(rInterpolate, rPostRender, existingId);
template <typename TInterpolate, typename TPostRender>
std::tuple<int64_t, typename TInterpolate::id_t> AddIndexableElementWithId(TInterpolate& rInterpolate, TPostRender& rPostRender, typename TInterpolate::id_t existingId)
{
	return AddGeneratedIndexableElement(rInterpolate, rPostRender, [existingId]()
	{
		return existingId;
	});
}

// Removes element by ID from paired indexable collections using swap-and-pop.
// Handles SwapElement on both collections, idToIndexMap update, and count decrement.
// Requires: TPostRender must have puiIds member storing element IDs.
// Usage: RemoveIndexableElement(rInterpolate, rPostRender, id, rInterpolate.Members(), rPostRender.Members());
template <typename TInterpolate, typename TPostRender, typename TInterpolateTuple, typename TPostRenderTuple>
void RemoveIndexableElement(TInterpolate& rInterpolate, TPostRender& rPostRender, typename TInterpolate::id_t id, TInterpolateTuple&& interpolateTuple, TPostRenderTuple&& postRenderTuple)
{
	ASSERT(rInterpolate.iCount > 0);
	int64_t iIndex = rInterpolate.idToIndexMap.at(id);

	if (rInterpolate.iCount - 1 > iIndex) [[likely]]
	{
		typename TInterpolate::id_t lastId = rPostRender.puiIds[rInterpolate.iCount - 1];

		SwapElement(rInterpolate, iIndex, std::forward<TInterpolateTuple>(interpolateTuple));
		SwapElement(rPostRender, iIndex, std::forward<TPostRenderTuple>(postRenderTuple));

		rInterpolate.idToIndexMap.insert_or_assign(lastId, iIndex);
	}

	--rInterpolate.iCount;
	--rPostRender.iCount;

	rInterpolate.idToIndexMap.erase(id);
}

// Removes an externally-owned handle from paired collections, then invalidates that handle.
template <typename TInterpolate, typename TPostRender, typename TInterpolateTuple, typename TPostRenderTuple>
void RemoveIndexableElementAndClearHandle(TInterpolate& rInterpolate, TPostRender& rPostRender, typename TInterpolate::id_t& rId, TInterpolateTuple&& interpolateTuple, TPostRenderTuple&& postRenderTuple)
{
	ASSERT(rId.IsValid());
	RemoveIndexableElement(rInterpolate, rPostRender, rId, std::forward<TInterpolateTuple>(interpolateTuple), std::forward<TPostRenderTuple>(postRenderTuple));
	rId = {};
}

// Removes element at index from paired collections using swap-and-pop.
// Handles SwapElement on both collections, count decrement, and loop index adjustment.
template <typename TInterpolate, typename TPostRender, typename TInterpolateTuple, typename TPostRenderTuple>
void DestroyElement(TInterpolate& rInterpolate, TPostRender& rPostRender, int64_t& i, TInterpolateTuple&& interpolateTuple, TPostRenderTuple&& postRenderTuple)
{
	if (rInterpolate.iCount - 1 > i) [[likely]]
	{
		SwapElement(rInterpolate, i, std::forward<TInterpolateTuple>(interpolateTuple));
		SwapElement(rPostRender, i, std::forward<TPostRenderTuple>(postRenderTuple));
		--i;
	}
	--rInterpolate.iCount;
	--rPostRender.iCount;
}

} // namespace engine
