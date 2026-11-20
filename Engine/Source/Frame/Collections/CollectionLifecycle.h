#pragma once

#include "CollectionId.h"
#include "CollectionMemory.h"

namespace engine
{

struct FramePostRenderBase;

// Every PostRender's pLifecycleFlags column. The Destroy-phase compaction fold (CompactCollections) removes a
// kTransfer or kDestroy row. kDying marks a row readers skip because it is leaving: a timed death its collection plays
// out before it marks the row, or a row MarkDestroy marked.
enum class LifecycleFlags : uint8_t
{
	kTransfer = 0x01,
	kDestroy  = 0x02,
	kDying    = 0x04,
};
using LifecycleFlags_t = common::Flags<LifecycleFlags>;

// The one way to mark a row kDestroy: kDying comes with it, so every reader that skips a dying row also skips a row
// marked for removal earlier in the tick.
inline void MarkDestroy(LifecycleFlags_t& rLifecycleFlags)
{
	rLifecycleFlags.Set({LifecycleFlags::kDestroy, LifecycleFlags::kDying});
}

template <typename FRAME, typename INTERPOLATE, typename POST_RENDER>
void ReleaseOwnedHandles(FRAME& rFrame, INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, int64_t i);

template <typename INTERPOLATE, typename POST_RENDER>
inline int64_t AddElement(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender)
{
	++rInterpolate.iCount;
	++rPostRender.iCount;
	int64_t iSpawnIndex = rInterpolate.iCount - 1;
	ASSERT(iSpawnIndex < rInterpolate.iCapacity);
	rPostRender.pLifecycleFlags[iSpawnIndex] = {};
	return iSpawnIndex;
}

template <typename INTERPOLATE, typename POST_RENDER, typename INTERPOLATE_TUPLE, typename POST_RENDER_TUPLE>
void GrowPairedCollections(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, INTERPOLATE_TUPLE&& rInterpolateTuple, POST_RENDER_TUPLE&& rPostRenderTuple)
{
	if (rInterpolate.iCount + 1 <= rInterpolate.iCapacity)
	{
		return;
	}

	// Deterministic growth is 2 * capacity + 1 because capacity participates in the collection CRC.
	int64_t iNewCapacity = 2 * rInterpolate.iCapacity + 1;

	ASSERT(rInterpolate.iCount == rPostRender.iCount);
	GrowCapacityWithCopy(rInterpolate, iNewCapacity, rInterpolate.iCount, std::forward<INTERPOLATE_TUPLE>(rInterpolateTuple));
	GrowCapacityWithCopy(rPostRender, iNewCapacity, rPostRender.iCount, std::forward<POST_RENDER_TUPLE>(rPostRenderTuple));
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

// Position of collection type T in a Collections() tuple of references; the tuple size when T is absent.
template <typename T, typename... TS>
consteval int64_t CollectionTupleIndex([[maybe_unused]] std::type_identity<std::tuple<TS...>> typeIdentity)
{
	std::array<bool, sizeof...(TS)> bMatches {std::is_same_v<T, std::remove_reference_t<TS>>...};
	return std::ranges::find(bMatches, true) - bMatches.begin();
}

// Releases an owner's handle if it holds one: marks the named row kDestroy for the Destroy-phase compaction fold,
// releases the rows that row owns in turn, and invalidates the handle. A zero handle is a no-op. Both frame halves
// list each collection pair at the same Collections() index.
template <typename FRAME, typename INTERPOLATE>
void ReleaseIndexableElementAndClearHandle(FRAME& rFrame, Id<INTERPOLATE>& rId)
{
	using InterpolateTuple = decltype(rFrame.interpolate.Collections());
	using PostRenderTuple = decltype(rFrame.postRender.Collections());
	static constexpr int64_t kiIndex = CollectionTupleIndex<INTERPOLATE>(std::type_identity<InterpolateTuple> {});
	static_assert(kiIndex < static_cast<int64_t>(std::tuple_size_v<InterpolateTuple>), "ReleaseIndexableElementAndClearHandle: the handle's collection is not a frame collection");

	if (rId.uuid.iValue == 0)
	{
		return;
	}

	INTERPOLATE& rInterpolate = std::get<static_cast<size_t>(kiIndex)>(rFrame.interpolate.Collections());
	std::tuple_element_t<static_cast<size_t>(kiIndex), PostRenderTuple> rPostRender = std::get<static_cast<size_t>(kiIndex)>(rFrame.postRender.Collections());

	int64_t iIndex = rInterpolate.idToIndexMap.at(rId);
	MarkDestroy(rPostRender.pLifecycleFlags[iIndex]);
	ReleaseOwnedHandles(rFrame, rInterpolate, rPostRender, iIndex);
	rId = {};
}

// Releases every row a nonzero handle in row i's OwnedHandles() columns names; a pair half without the accessor owns
// nothing.
template <typename FRAME, typename INTERPOLATE, typename POST_RENDER>
void ReleaseOwnedHandles([[maybe_unused]] FRAME& rFrame, INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, [[maybe_unused]] int64_t i)
{
	auto OwnedHandleColumns = [](auto& rCollection)
	{
		if constexpr (requires { rCollection.OwnedHandles(); })
		{
			return rCollection.OwnedHandles();
		}
		else
		{
			return std::tie();
		}
	};

	std::apply([&](auto&... rHandleColumns)
	{
		(ForEachMemberPointer(rHandleColumns, [&](auto& rpHandles)
		{
			ReleaseIndexableElementAndClearHandle(rFrame, rpHandles[i]);
		}), ...);
	}, std::tuple_cat(OwnedHandleColumns(rInterpolate), OwnedHandleColumns(rPostRender)));
}

// Removes element at index from paired collections using swap-and-pop: the last row moves into index iIndex, so
// the row now at iIndex has not been visited by a forward walk. Iteration state stays the caller's to adjust. An ID
// map re-points the moved row's ID at iIndex and drops the removed ID, read from the paired PostRender pIds; when the
// removed row is last both IDs are the same and the erase undoes the re-point.
template <typename INTERPOLATE, typename POST_RENDER, typename INTERPOLATE_TUPLE, typename POST_RENDER_TUPLE>
void DestroyElement(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, int64_t iIndex, INTERPOLATE_TUPLE&& rInterpolateTuple, POST_RENDER_TUPLE&& rPostRenderTuple)
{
	if constexpr (HasIdToIndex<INTERPOLATE>)
	{
		rInterpolate.idToIndexMap.insert_or_assign(rPostRender.pIds[rInterpolate.iCount - 1], iIndex);
		rInterpolate.idToIndexMap.erase(rPostRender.pIds[iIndex]);
	}

	if (rInterpolate.iCount - 1 > iIndex) [[likely]]
	{
		SwapElement(rInterpolate, iIndex, std::forward<INTERPOLATE_TUPLE>(rInterpolateTuple));
		SwapElement(rPostRender, iIndex, std::forward<POST_RENDER_TUPLE>(rPostRenderTuple));
	}
	--rInterpolate.iCount;
	--rPostRender.iCount;
}

} // namespace engine
