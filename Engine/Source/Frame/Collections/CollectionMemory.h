#pragma once

namespace engine
{

// ForEachMemberPointer visits array pointer elements in index order and scalar pointers once; order
// determines CRC and layout. Callbacks receive pointer references for assign/reset/swap, preserving
// constness. Derive ElementType with remove_pointer_t<remove_reference_t<decltype(rElementPointer)>>:
// reversing the removals leaves a pointer type and gives pointer-sized storage.

template <typename MEMBER, typename FN>
constexpr void ForEachMemberPointer(MEMBER& rMember, FN&& rFunction)
{
	if constexpr (std::is_array_v<MEMBER>)
	{
		for (std::remove_extent_t<MEMBER>& rElementPointer : rMember)
		{
			rFunction(rElementPointer);
		}
	}
	else
	{
		rFunction(rMember);
	}
}

// Computes 64-byte-rounded storage for array or scalar member pointers at iCapacity.

template <typename T>
constexpr int64_t CalculateBufferSize(int64_t iCapacity, const T& rMember)
{
	ASSERT(iCapacity >= 0);

	int64_t iBufferSize = 0;
	ForEachMemberPointer(rMember, [&](auto& rElementPointer)
	{
		using ElementType = std::remove_pointer_t<std::remove_reference_t<decltype(rElementPointer)>>;
		iBufferSize += common::RoundUp<int64_t, 64>(iCapacity * sizeof(ElementType));
	});
	return iBufferSize;
}

// Raw summed byte size of one member tuple laid out at iCapacity. Unclamped: allocation callers apply their own
// max(size, 1) so a zero-member layout still backs a positive capacity, while the deserialize zero-fill wants the
// exact physical-layout sum.
template <typename TUPLE>
int64_t MemberTupleBufferSize(int64_t iCapacity, const TUPLE& rMembers)
{
	int64_t iBufferSize = 0;
	std::apply([&](const auto&... rMemberPointers)
	{
		((iBufferSize += CalculateBufferSize(iCapacity, rMemberPointers)), ...);
	}, rMembers);
	return iBufferSize;
}


template <typename T>
void AssignAligned(T& rMember, int64_t iCapacity, std::byte*& rpCurrent)
{
	ForEachMemberPointer(rMember, [&](auto& rElementPointer)
	{
		using ElementPtrType = std::remove_reference_t<decltype(rElementPointer)>;
		using ElementType = std::remove_pointer_t<ElementPtrType>;
		rpCurrent = reinterpret_cast<std::byte*>(common::RoundUp<uintptr_t, 64>(reinterpret_cast<uintptr_t>(rpCurrent)));
		rElementPointer = reinterpret_cast<ElementPtrType>(rpCurrent);
		rpCurrent += iCapacity * sizeof(ElementType);
	});
}

template <typename T>
void AssignAndCopyAligned(T& rMember, int64_t iCapacity, int64_t iCount, std::byte*& rpCurrent)
{
	ForEachMemberPointer(rMember, [&](auto& rElementPointer)
	{
		using ElementPtrType = std::remove_reference_t<decltype(rElementPointer)>;
		using ElementType = std::remove_pointer_t<ElementPtrType>;
		rpCurrent = reinterpret_cast<std::byte*>(common::RoundUp<uintptr_t, 64>(reinterpret_cast<uintptr_t>(rpCurrent)));

		if (rElementPointer != nullptr)
		{
			std::memcpy(rpCurrent, rElementPointer, iCount * sizeof(ElementType));
		}

		rElementPointer = reinterpret_cast<ElementPtrType>(rpCurrent);
		rpCurrent += iCapacity * sizeof(ElementType);
	});
}


template <typename STRUCT, typename TUPLE>
void ResetDataToNull(STRUCT& rStruct, TUPLE&& rMembers)
{
	rStruct.pData.reset();
	rStruct.iCapacity = 0;
	rStruct.iPhysicalLayoutCapacity = 0;

	std::apply([&](auto&... rMemberPointers)
	{
		(ForEachMemberPointer(rMemberPointers, [](auto& rElementPointer)
		{
			rElementPointer = nullptr;
		}), ...);
	}, std::forward<TUPLE>(rMembers));
}

// iPhysicalLayoutCapacity records the installed buffer's stride capacity and survives shrink reuse.
// Deserialization can already have overwritten iCapacity, so buffer reuse is bounded by the physical capacity.
template <typename STRUCT, typename TUPLE>
void AllocateAndAssign(STRUCT& rStruct, int64_t iCapacity, TUPLE&& rMembers)
{
	if (rStruct.iPhysicalLayoutCapacity >= iCapacity && rStruct.pData != nullptr)
	{
		return;
	}

	// Heap: MakeAligned allocates the SOA data buffer, which must persist across frames and can be arbitrarily
	// large depending on entity count. Workbuffer is temporary (lost on Pop) and can't hold cross-frame state.
	ScopedSuppressAllocationTracking suppress;
	int64_t iBufferSize = std::max<int64_t>(MemberTupleBufferSize(iCapacity, rMembers), 1);
	if (iBufferSize > common::kiMaxDeserializedBytes)
	{
		ResetDataToNull(rStruct, rMembers);
		throw std::ios_base::failure("AllocateAndAssign");
	}

	common::AlignedUniquePtr<std::byte> pNewData = common::MakeAligned<std::byte>(iBufferSize);
	if (pNewData == nullptr)
	{
		ResetDataToNull(rStruct, rMembers);
		throw std::ios_base::failure("AllocateAndAssign");
	}

	// Publish capacity and physical layout only after allocation succeeds.
	rStruct.iCapacity = iCapacity;
	rStruct.iPhysicalLayoutCapacity = iCapacity;
	rStruct.pData = std::move(pNewData);

	std::byte* pCurrent = rStruct.pData.get();
	std::apply([&](auto&... rMemberPointers)
	{
		(AssignAligned(rMemberPointers, iCapacity, pCurrent), ...);
	}, std::forward<TUPLE>(rMembers));
}

template <typename T>
concept HasIdToIndex = requires(T& rStruct)
{
	rStruct.idToIndexMap;
};

template <typename T>
concept HasPersistentMembers = requires(const T& rStruct)
{
	rStruct.PersistentMembers();
};

// True when every entry of rSubsetMembers refers to one of rFullMembers' member arrays. Compared by
// address — element types repeat across members, so a type-level check cannot express containment.
template <typename SUBSET_TUPLE, typename FULL_TUPLE>
inline bool IsMemberTupleSubset(const SUBSET_TUPLE& rSubsetMembers, const FULL_TUPLE& rFullMembers)
{
	return std::apply([&](const auto&... rSubsetMemberPointers)
	{
		return ([&](const auto& rSubsetMemberPointer)
		{
			return std::apply([&](const auto&... rFullMemberPointers)
			{
				// Compare via uintptr_t: clang rejects static_cast<const void*> on &(T* __restrict) as casting away __restrict
				return ((reinterpret_cast<uintptr_t>(&rSubsetMemberPointer) == reinterpret_cast<uintptr_t>(&rFullMemberPointers)) || ...);
			}, rFullMembers);
		}(rSubsetMemberPointers) && ...);
	}, rSubsetMembers);
}

// Called during AllocateAndCopy before Update; a previous frame without backing storage resets current pointers and capacities.
template <typename STRUCT, typename TUPLE>
void Allocate(STRUCT& rCurrent, const STRUCT& rPrevious, TUPLE&& rMembers)
{
	// Heap: MakeAligned for the SOA buffer and unordered_map copy for idToIndexMap. Both persist across frames
	// with sizes that vary at runtime based on entity count, so neither workbuffer nor static arrays work.
	ScopedSuppressAllocationTracking suppress;
	rCurrent.iCount = rPrevious.iCount;

	if constexpr (HasIdToIndex<STRUCT>)
	{
		rCurrent.idToIndexMap = rPrevious.idToIndexMap;
	}

	if (rPrevious.pData == nullptr)
	{
		ResetDataToNull(rCurrent, std::forward<TUPLE>(rMembers));
		return;
	}

	int64_t iCapacity = rPrevious.iCapacity;

	// Capacity-only guard: a null pData always implies zero capacity (ResetDataToNull zeroes both together,
	// and every allocating path sets both together), so a matching nonzero capacity guarantees pData is non-null.
	// The equal-capacity else path reuses rCurrent's existing buffer as-is, preserving its iPhysicalLayoutCapacity.
	if (rCurrent.iCapacity != iCapacity)
	{
		int64_t iBufferSize = std::max<int64_t>(MemberTupleBufferSize(iCapacity, rMembers), 1);

		common::AlignedUniquePtr<std::byte> pNewData = common::MakeAligned<std::byte>(iBufferSize);
		if (pNewData == nullptr)
		{
			throw std::bad_alloc();
		}

		rCurrent.iCapacity = iCapacity;
		rCurrent.iPhysicalLayoutCapacity = iCapacity;
		rCurrent.pData = std::move(pNewData);

		std::byte* pCurrent = rCurrent.pData.get();
		std::apply([&](auto&... rMemberPointers)
		{
			(AssignAligned(rMemberPointers, iCapacity, pCurrent), ...);
		}, std::forward<TUPLE>(rMembers));

		ASSERT(rCurrent.iCount <= rCurrent.iCapacity);
	}
}

template <typename CURRENT_POINTER, typename PREVIOUS_POINTER>
void CopyMemberPointerRows(int64_t iCount, CURRENT_POINTER& rCurrentPointer, const PREVIOUS_POINTER& rPreviousPointer)
{
	using CurrentPointer = std::remove_reference_t<decltype(rCurrentPointer)>;
	using PreviousPointer = std::remove_reference_t<decltype(rPreviousPointer)>;
	static_assert(std::is_pointer_v<CurrentPointer> && std::is_pointer_v<PreviousPointer>, "Collection members must be pointers");

	using CurrentElement = std::remove_pointer_t<CurrentPointer>;
	using PreviousElement = std::remove_pointer_t<PreviousPointer>;
	static_assert(std::is_same_v<CurrentElement, PreviousElement>, "Corresponding collection member element types must match");

	std::memcpy(rCurrentPointer, rPreviousPointer, iCount * sizeof(CurrentElement));
}

// C arrays of member pointers are copied in index order.
template <typename CURRENT_MEMBER, typename PREVIOUS_MEMBER>
void CopyMemberEntryRows(int64_t iCount, CURRENT_MEMBER& rCurrentMember, const PREVIOUS_MEMBER& rPreviousMember)
{
	static constexpr bool kbCurrentIsArray = std::is_array_v<CURRENT_MEMBER>;
	static constexpr bool kbPreviousIsArray = std::is_array_v<PREVIOUS_MEMBER>;
	static_assert(kbCurrentIsArray == kbPreviousIsArray, "Corresponding collection member shapes must match");

	if constexpr (kbCurrentIsArray && kbPreviousIsArray)
	{
		static constexpr size_t kuiCurrentExtent = std::extent_v<CURRENT_MEMBER>;
		static constexpr size_t kuiPreviousExtent = std::extent_v<PREVIOUS_MEMBER>;
		static_assert(kuiCurrentExtent == kuiPreviousExtent, "Corresponding collection member array extents must match");
		for (size_t i = 0; i < kuiCurrentExtent; ++i)
		{
			CopyMemberPointerRows(iCount, rCurrentMember[i], rPreviousMember[i]);
		}
	}
	else
	{
		CopyMemberPointerRows(iCount, rCurrentMember, rPreviousMember);
	}
}

template <typename CURRENT_TUPLE, typename PREVIOUS_TUPLE, size_t... INDICES>
void CopyMemberRows(int64_t iCount, CURRENT_TUPLE&& rCurrentMembers, PREVIOUS_TUPLE&& rPreviousMembers, std::index_sequence<INDICES...>)
{
	(CopyMemberEntryRows(iCount, std::get<INDICES>(rCurrentMembers), std::get<INDICES>(rPreviousMembers)), ...);
}

// Copies corresponding member arrays in stable tuple order, and array entries in stable index order.
template <typename CURRENT_TUPLE, typename PREVIOUS_TUPLE>
void CopyMemberRows(int64_t iCount, CURRENT_TUPLE&& rCurrentMembers, PREVIOUS_TUPLE&& rPreviousMembers)
{
	using CurrentTuple = std::remove_reference_t<CURRENT_TUPLE>;
	using PreviousTuple = std::remove_reference_t<PREVIOUS_TUPLE>;
	static constexpr size_t kuiCurrentSize = std::tuple_size_v<CurrentTuple>;
	static constexpr size_t kuiPreviousSize = std::tuple_size_v<PreviousTuple>;
	static_assert(kuiCurrentSize == kuiPreviousSize, "Corresponding collection member tuples must have matching arity");

	if constexpr (kuiCurrentSize == kuiPreviousSize)
	{
		if (iCount > 0)
		{
			CopyMemberRows(iCount, std::forward<CURRENT_TUPLE>(rCurrentMembers), std::forward<PREVIOUS_TUPLE>(rPreviousMembers), std::make_index_sequence<kuiCurrentSize> {});
		}
	}
}

// Allocates the exact previous-frame capacity, then copies either PersistentMembers() or all Members().
template <typename STRUCT>
void AllocateAndCopyMembers(STRUCT& rCurrent, const STRUCT& rPrevious)
{
	Allocate(rCurrent, rPrevious, rCurrent.Members());

	if constexpr (HasPersistentMembers<STRUCT>)
	{
		ASSERT(IsMemberTupleSubset(rCurrent.PersistentMembers(), rCurrent.Members()));
		CopyMemberRows(rCurrent.iCount, rCurrent.PersistentMembers(), rPrevious.PersistentMembers());
	}
	else
	{
		CopyMemberRows(rCurrent.iCount, rCurrent.Members(), rPrevious.Members());
	}
}

// Allocates and copies the ID array from previous frame. Used by PostRender collections
// whose only persistent member is pIds.
template <typename POST_RENDER>
void AllocateAndCopyIds(POST_RENDER& rCurrent, const POST_RENDER& rPrevious)
{
	Allocate(rCurrent, rPrevious, rCurrent.Members());

	if (rCurrent.iCount > 0)
	{
		std::memcpy(rCurrent.pIds, rPrevious.pIds, rCurrent.iCount * sizeof(rCurrent.pIds[0]));
	}
}

template <typename STRUCT, typename TUPLE>
void GrowCapacityWithCopy(STRUCT& rStruct, int64_t iNewCapacity, int64_t iCurrentCount, TUPLE&& rMembers)
{
	// Heap: MakeAligned for a larger SOA buffer that replaces the old one. The buffer persists across frames
	// and grows with entity count, so workbuffer (lost on Pop) and static arrays (fixed size) don't work.
	ScopedSuppressAllocationTracking suppress;
	int64_t iBufferSize = std::max<int64_t>(MemberTupleBufferSize(iNewCapacity, rMembers), 1);

	common::AlignedUniquePtr<std::byte> pNewData = common::MakeAligned<std::byte>(iBufferSize);
	if (pNewData == nullptr)
	{
		throw std::bad_alloc();
	}

	std::byte* pCurrent = pNewData.get();
	std::apply([&](auto&... rMemberPointers)
	{
		(AssignAndCopyAligned(rMemberPointers, iNewCapacity, iCurrentCount, pCurrent), ...);
	}, std::forward<TUPLE>(rMembers));
	rStruct.pData = std::move(pNewData);
	rStruct.iCapacity = iNewCapacity;
	rStruct.iPhysicalLayoutCapacity = iNewCapacity;
}

// Overwrite row i with the last row for O(1) unordered removal.
// The caller must check bounds, decrement the count, and recheck i.
template <typename STRUCT, typename TUPLE>
void SwapElement(STRUCT& rStruct, int64_t i, TUPLE&& rMembers)
{
	std::apply([&](auto&... rMemberPointers)
	{
		(ForEachMemberPointer(rMemberPointers, [&](auto& rElementPointer)
		{
			rElementPointer[i] = rElementPointer[rStruct.iCount - 1];
		}), ...);
	}, std::forward<TUPLE>(rMembers));
}

// Value-initializes every Members() entry at one row before collection-specific defaults are applied.
template <typename TUPLE>
void ZeroMemberRow(int64_t i, TUPLE&& rMembers)
{
	std::apply([&](auto&... rMemberPointers)
	{
		(ForEachMemberPointer(rMemberPointers, [&](auto& rElementPointer)
		{
			rElementPointer[i] = {};
		}), ...);
	}, std::forward<TUPLE>(rMembers));
}

} // namespace engine
