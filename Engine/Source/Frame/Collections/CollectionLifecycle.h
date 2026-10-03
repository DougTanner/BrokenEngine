#pragma once

#include "CollectionMemory.h"

namespace engine
{

struct FramePostRenderBase;


template <typename INTERPOLATE, typename POST_RENDER>
inline int64_t AddElement(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender)
{
	++rInterpolate.iCount;
	++rPostRender.iCount;
	int64_t iSpawnIndex = rInterpolate.iCount - 1;
	ASSERT(iSpawnIndex < rInterpolate.iCapacity);
	return iSpawnIndex;
}

template <typename INTERPOLATE, typename POST_RENDER, typename INTERPOLATE_TUPLE, typename POST_RENDER_TUPLE>
bool GrowPairedCollections(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, INTERPOLATE_TUPLE&& rInterpolateTuple, POST_RENDER_TUPLE&& rPostRenderTuple)
{
	if (rInterpolate.iCount + 1 <= rInterpolate.iCapacity)
	{
		return false;
	}

	// Deterministic growth is 2 * capacity + 1 because capacity participates in the collection CRC.
	int64_t iNewCapacity = 2 * rInterpolate.iCapacity + 1;

	ASSERT(rInterpolate.iCount == rPostRender.iCount);
	GrowCapacityWithCopy(rInterpolate, iNewCapacity, rInterpolate.iCount, std::forward<INTERPOLATE_TUPLE>(rInterpolateTuple));
	GrowCapacityWithCopy(rPostRender, iNewCapacity, rPostRender.iCount, std::forward<POST_RENDER_TUPLE>(rPostRenderTuple));

	return true;
}

// Internal insertion primitive. Invokes rGenerateId exactly once, then records that ID at the new row.
template <typename INTERPOLATE, typename POST_RENDER, typename GENERATE_ID>
std::tuple<int64_t, typename INTERPOLATE::id_t> AddGeneratedIndexableElement(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, GENERATE_ID&& rGenerateId)
{
	// Heap: unordered_map::insert_or_assign may allocate a new bucket or node for the ID-to-index entry.
	// The map must persist across frames for stable ID lookups, so workbuffer and static arrays are not viable.
	ScopedSuppressAllocationTracking suppress;
	int64_t iSpawnIndex = AddElement(rInterpolate, rPostRender);

	typename INTERPOLATE::id_t newId = rGenerateId();
	rInterpolate.idToIndexMap.insert_or_assign(newId, iSpawnIndex);

	return {iSpawnIndex, newId};
}

template <typename INTERPOLATE, typename POST_RENDER>
std::tuple<int64_t, typename INTERPOLATE::id_t> AddIndexableElement(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, FramePostRenderBase& rFramePostRender)
{
	using id_t = typename INTERPOLATE::id_t;
	return AddGeneratedIndexableElement(rInterpolate, rPostRender, [&rFramePostRender]()
	{
		return id_t::Generate(rFramePostRender);
	});
}

// Uses MakeUuid(uiNextVisualUuid) so visual object creation does not perturb the main UUID sequence.
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

template <typename INTERPOLATE, typename POST_RENDER>
std::tuple<int64_t, typename INTERPOLATE::id_t> AddIndexableElementWithId(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, typename INTERPOLATE::id_t existingId)
{
	return AddGeneratedIndexableElement(rInterpolate, rPostRender, [existingId]()
	{
		return existingId;
	});
}

// POST_RENDER must provide pIds containing the element IDs.
template <typename INTERPOLATE, typename POST_RENDER, typename INTERPOLATE_TUPLE, typename POST_RENDER_TUPLE>
void RemoveIndexableElement(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, typename INTERPOLATE::id_t id, INTERPOLATE_TUPLE&& rInterpolateTuple, POST_RENDER_TUPLE&& rPostRenderTuple)
{
	ASSERT(rInterpolate.iCount > 0);
	int64_t iIndex = rInterpolate.idToIndexMap.at(id);

	if (rInterpolate.iCount - 1 > iIndex) [[likely]]
	{
		typename INTERPOLATE::id_t lastId = rPostRender.pIds[rInterpolate.iCount - 1];

		SwapElement(rInterpolate, iIndex, std::forward<INTERPOLATE_TUPLE>(rInterpolateTuple));
		SwapElement(rPostRender, iIndex, std::forward<POST_RENDER_TUPLE>(rPostRenderTuple));

		rInterpolate.idToIndexMap.insert_or_assign(lastId, iIndex);
	}

	--rInterpolate.iCount;
	--rPostRender.iCount;

	rInterpolate.idToIndexMap.erase(id);
}

// Removes an externally-owned handle from paired collections, then invalidates that handle.
template <typename INTERPOLATE, typename POST_RENDER, typename INTERPOLATE_TUPLE, typename POST_RENDER_TUPLE>
void RemoveIndexableElementAndClearHandle(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, typename INTERPOLATE::id_t& rId, INTERPOLATE_TUPLE&& rInterpolateTuple, POST_RENDER_TUPLE&& rPostRenderTuple)
{
	ASSERT((rId.uuid.iValue != 0));
	RemoveIndexableElement(rInterpolate, rPostRender, rId, std::forward<INTERPOLATE_TUPLE>(rInterpolateTuple), std::forward<POST_RENDER_TUPLE>(rPostRenderTuple));
	rId = {};
}

// Removes element at index from paired collections using swap-and-pop: the last row moves into index iIndex, so
// the row now at iIndex has not been visited by a forward walk. Iteration state stays the caller's to adjust.
template <typename INTERPOLATE, typename POST_RENDER, typename INTERPOLATE_TUPLE, typename POST_RENDER_TUPLE>
void DestroyElement(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, int64_t iIndex, INTERPOLATE_TUPLE&& rInterpolateTuple, POST_RENDER_TUPLE&& rPostRenderTuple)
{
	if (rInterpolate.iCount - 1 > iIndex) [[likely]]
	{
		SwapElement(rInterpolate, iIndex, std::forward<INTERPOLATE_TUPLE>(rInterpolateTuple));
		SwapElement(rPostRender, iIndex, std::forward<POST_RENDER_TUPLE>(rPostRenderTuple));
	}
	--rInterpolate.iCount;
	--rPostRender.iCount;
}

} // namespace engine
