#pragma once

#include "CollectionMemory.h"

namespace engine
{

struct FramePostRenderBase;

// Paired Interpolate/PostRender lifecycle helpers grow with GrowCapacityWithCopy and remove with
// SwapElement from CollectionMemory.h.

// Increments counts for paired Interpolate/PostRender collections and returns spawn index.
template <typename INTERPOLATE, typename POST_RENDER>
inline int64_t AddElement(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender)
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
template <typename INTERPOLATE, typename POST_RENDER, typename INTERPOLATE_TUPLE, typename POST_RENDER_TUPLE>
bool GrowPairedCollections(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, INTERPOLATE_TUPLE&& interpolateTuple, POST_RENDER_TUPLE&& postRenderTuple)
{
	if (rInterpolate.iCount + 1 <= rInterpolate.iCapacity)
	{
		return false;
	}

	// Deterministic growth is 2 * capacity + 1 because capacity participates in the collection CRC.
	int64_t iNewCapacity = 2 * rInterpolate.iCapacity + 1;

	ASSERT(rInterpolate.iCount == rPostRender.iCount);
	GrowCapacityWithCopy(rInterpolate, iNewCapacity, rInterpolate.iCount, std::forward<INTERPOLATE_TUPLE>(interpolateTuple));
	GrowCapacityWithCopy(rPostRender, iNewCapacity, rPostRender.iCount, std::forward<POST_RENDER_TUPLE>(postRenderTuple));

	return true;
}

// Internal insertion primitive. Invokes generateId exactly once, then records that ID at the new row.
template <typename INTERPOLATE, typename POST_RENDER, typename GENERATE_ID>
std::tuple<int64_t, typename INTERPOLATE::id_t> AddGeneratedIndexableElement(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, GENERATE_ID&& generateId)
{
	// Heap: unordered_map::insert_or_assign may allocate a new bucket or node for the ID-to-index entry.
	// The map must persist across frames for stable ID lookups, so workbuffer and static arrays are not viable.
	ScopedSuppressAllocationTracking suppress;
	int64_t iSpawnIndex = AddElement(rInterpolate, rPostRender);

	typename INTERPOLATE::id_t newId = generateId();
	rInterpolate.idToIndexMap.insert_or_assign(newId, iSpawnIndex);

	return {iSpawnIndex, newId};
}

// Increments counts, generates unique ID, and updates idToIndexMap for indexable collections.
// Returns tuple of (spawnIndex, newId).
// Usage: auto [uiIndex, newId] = AddIndexableElement(rInterpolate, rPostRender, rFramePostRender);
template <typename INTERPOLATE, typename POST_RENDER>
std::tuple<int64_t, typename INTERPOLATE::id_t> AddIndexableElement(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, FramePostRenderBase& rFramePostRender)
{
	using id_t = typename INTERPOLATE::id_t;
	return AddGeneratedIndexableElement(rInterpolate, rPostRender, [&rFramePostRender]()
	{
		return id_t::Generate(rFramePostRender);
	});
}

// Increments counts, generates visual unique ID, and updates idToIndexMap for visual-only collections.
// Uses GenerateVisualUuid() so visual object creation does not perturb the main UUID sequence.
#if defined(BT_CLIENT)
template <typename INTERPOLATE, typename POST_RENDER>
std::tuple<int64_t, typename INTERPOLATE::id_t> AddVisualIndexableElement(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, FramePostRenderBase& rFramePostRender)
{
	using id_t = typename INTERPOLATE::id_t;
	return AddGeneratedIndexableElement(rInterpolate, rPostRender, [&rFramePostRender]()
	{
		return id_t::GenerateVisual(rFramePostRender);
	});
}
#endif // BT_CLIENT

// Increments counts, reuses an existing ID, and updates idToIndexMap for indexable collections.
// Returns tuple of (spawnIndex, existingId).
// Usage: auto [uiIndex, id] = AddIndexableElementWithId(rInterpolate, rPostRender, existingId);
template <typename INTERPOLATE, typename POST_RENDER>
std::tuple<int64_t, typename INTERPOLATE::id_t> AddIndexableElementWithId(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, typename INTERPOLATE::id_t existingId)
{
	return AddGeneratedIndexableElement(rInterpolate, rPostRender, [existingId]()
	{
		return existingId;
	});
}

// Removes element by ID from paired indexable collections using swap-and-pop.
// Handles SwapElement on both collections, idToIndexMap update, and count decrement.
// Requires: POST_RENDER must have puiIds member storing element IDs.
// Usage: RemoveIndexableElement(rInterpolate, rPostRender, id, rInterpolate.Members(), rPostRender.Members());
template <typename INTERPOLATE, typename POST_RENDER, typename INTERPOLATE_TUPLE, typename POST_RENDER_TUPLE>
void RemoveIndexableElement(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, typename INTERPOLATE::id_t id, INTERPOLATE_TUPLE&& interpolateTuple, POST_RENDER_TUPLE&& postRenderTuple)
{
	ASSERT(rInterpolate.iCount > 0);
	int64_t iIndex = rInterpolate.idToIndexMap.at(id);

	if (rInterpolate.iCount - 1 > iIndex) [[likely]]
	{
		typename INTERPOLATE::id_t lastId = rPostRender.puiIds[rInterpolate.iCount - 1];

		SwapElement(rInterpolate, iIndex, std::forward<INTERPOLATE_TUPLE>(interpolateTuple));
		SwapElement(rPostRender, iIndex, std::forward<POST_RENDER_TUPLE>(postRenderTuple));

		rInterpolate.idToIndexMap.insert_or_assign(lastId, iIndex);
	}

	--rInterpolate.iCount;
	--rPostRender.iCount;

	rInterpolate.idToIndexMap.erase(id);
}

// Removes an externally-owned handle from paired collections, then invalidates that handle.
template <typename INTERPOLATE, typename POST_RENDER, typename INTERPOLATE_TUPLE, typename POST_RENDER_TUPLE>
void RemoveIndexableElementAndClearHandle(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, typename INTERPOLATE::id_t& rId, INTERPOLATE_TUPLE&& interpolateTuple, POST_RENDER_TUPLE&& postRenderTuple)
{
	ASSERT(rId.IsValid());
	RemoveIndexableElement(rInterpolate, rPostRender, rId, std::forward<INTERPOLATE_TUPLE>(interpolateTuple), std::forward<POST_RENDER_TUPLE>(postRenderTuple));
	rId = {};
}

// Removes element at index from paired collections using swap-and-pop: the last row moves into index i, so
// the row now at i has not been visited by a forward walk. Iteration state stays the caller's to adjust.
template <typename INTERPOLATE, typename POST_RENDER, typename INTERPOLATE_TUPLE, typename POST_RENDER_TUPLE>
void DestroyElement(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, int64_t i, INTERPOLATE_TUPLE&& interpolateTuple, POST_RENDER_TUPLE&& postRenderTuple)
{
	if (rInterpolate.iCount - 1 > i) [[likely]]
	{
		SwapElement(rInterpolate, i, std::forward<INTERPOLATE_TUPLE>(interpolateTuple));
		SwapElement(rPostRender, i, std::forward<POST_RENDER_TUPLE>(postRenderTuple));
	}
	--rInterpolate.iCount;
	--rPostRender.iCount;
}

} // namespace engine
