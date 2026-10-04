#pragma once

#include "File/PackChunks.h"
#include "CollectionController.h"
#include "CollectionId.h"
#include "CollectionLifecycle.h"
#include "CollectionMemory.h"

namespace game
{

struct Frame;
struct FrameInput;
struct FrameInterpolate;
struct FramePostRender;

} // namespace game

namespace engine
{

struct FramePostRenderBase;
struct GridCoord;

// Multi-array helpers process member pointers in tuple order.

// Computes ordered-fold CRC of multiple member arrays for deterministic replay validation.
template <typename TUPLE>
common::crc_t MultiCrc(int64_t iCount, const TUPLE& rMembers)
{
	common::crc_t uiChecksum = 0;
	// Zero-capacity member pointers may be null, and Crc(empty span) returns the seeded empty-input hash;
	// skip the fold to preserve the collection's zero checksum.
	if (iCount > 0)
	{
		std::apply([&](const auto&... rMemberPointers)
		{
			(ForEachMemberPointer(rMemberPointers, [&](const auto& rpElementPointer)
			{
				uiChecksum = (uiChecksum ^ common::Crc(std::span<const std::remove_pointer_t<std::remove_reference_t<decltype(rpElementPointer)>>>(rpElementPointer, static_cast<size_t>(iCount)))) * common::kCrcMultiplier;
			}), ...);
		}, rMembers);
	}
	return uiChecksum;
}

// Serializes multiple member arrays to stream in order.
template <typename TUPLE>
void MultiWrite(std::ostream& rStream, int64_t iCount, const TUPLE& rMembers)
{
	std::apply([&](const auto&... rMemberPointers)
	{
		(ForEachMemberPointer(rMemberPointers, [&](const auto& rpElementPointer)
		{
			common::Write(rStream, std::span<const std::remove_pointer_t<std::remove_reference_t<decltype(rpElementPointer)>>>(rpElementPointer, static_cast<size_t>(iCount)));
		}), ...);
	}, rMembers);
}

// Deserializes multiple member arrays from stream (must match write order). Arrays must already be allocated.
template <typename TUPLE>
void MultiRead(std::istream& rStream, int64_t iCount, const TUPLE& rMembers)
{
	std::apply([&](const auto&... rMemberPointers)
	{
		(ForEachMemberPointer(rMemberPointers, [&](const auto& rpElementPointer)
		{
			common::Read(rStream, std::span(rpElementPointer, static_cast<size_t>(iCount)));
		}), ...);
	}, rMembers);
}

template <typename STRUCT>
void ValidateAfterRead(std::istream& rStream, STRUCT& rStruct)
{
	if constexpr (requires { STRUCT::PostRead(rStruct); })
	{
		// A failed MultiRead leaves member storage partial; only validate a complete read.
		if (rStream.good())
		{
			STRUCT::PostRead(rStruct);
		}
	}
}

template <typename STRUCT, typename TUPLE>
void AllocateAndRead(STRUCT& rStruct, std::istream& rStream, TUPLE&& rMembers)
{
	if (rStruct.iCapacity > 0)
	{
		AllocateAndAssign(rStruct, rStruct.iCapacity, rMembers);
	}
	else
	{
		ResetDataToNull(rStruct, rMembers);
	}

	MultiRead(rStream, rStruct.iCount, std::forward<TUPLE>(rMembers));
	ValidateAfterRead(rStream, rStruct);
}

enum class CollectionFlags : uint32_t
{
	kIdToIndex = 0x0001,
};
using CollectionFlags_t = common::Flags<CollectionFlags>;

#if defined(BT_CLIENT)
// Register a CRC for pre-blur (implemented in TextureManager.cpp)
void RegisterLightingTextureCrc(common::crc_t uiCrc);
#endif

// Define TYPE before its collection and register types single-threaded before Dispatch() starts workers.
// sTypes is immutable during parallel frame ticks, so registry reads need no synchronization.
template <typename TYPE>
struct TypeRegistry
{
	using Type = TYPE;
	static inline std::vector<TYPE> sTypes;

	static void RegisterType(uint8_t& ruiIndex, const TYPE& rType)
	{
		ASSERT(ruiIndex == kuiInvalidTypeIndex);
		ASSERT(std::ssize(sTypes) < kuiInvalidTypeIndex);
		ruiIndex = static_cast<uint8_t>(sTypes.size());
		sTypes.push_back(rType);

		if constexpr (requires { rType.uiCrc; })
		{
			if (rType.uiCrc != 0)
			{
#if defined(BT_CLIENT)
				gpFileManager->mpPackChunks->mLoader.RequestChunkLoad(std::span(&rType.uiCrc, 1), LoadPriority::kNormal);
				RegisterLightingTextureCrc(rType.uiCrc);
#endif
			}
		}

		if constexpr (requires { rType.particleCrc; })
		{
			if (rType.particleCrc != 0)
			{
#if defined(BT_CLIENT)
				gpFileManager->mpPackChunks->mLoader.RequestChunkLoad(std::span(&rType.particleCrc, 1), LoadPriority::kNormal);
				RegisterLightingTextureCrc(rType.particleCrc);
#endif
			}
		}
	}

};

template <typename T, common::Flags<CollectionFlags> FLAGS>
struct OptionalIdToIndex
{
};

template <typename T, common::Flags<CollectionFlags> FLAGS>
	requires (FLAGS & CollectionFlags::kIdToIndex)
struct OptionalIdToIndex<T, FLAGS>
{
	using id_t = engine::Id<T>;

	std::unordered_map<id_t, int64_t> idToIndexMap;

	inline void Write(std::ostream& rStream) const
	{
		// Infrequent save/replay writes allocate a temporary key vector for deterministic ordering.
		ScopedSuppressAllocationTracking suppress;
		int64_t iSize = idToIndexMap.size();
		common::Write(rStream, iSize);

		std::vector<id_t> vecKeys = GetSortedKeys();
		for (const id_t& rKey : vecKeys)
		{
			rKey.Write(rStream);
			common::Write(rStream, idToIndexMap.at(rKey));
		}
	}

	// iCount is the collection's already-validated live-row count. The rebuilt map must be a bijection from stream
	// keys onto [0, iCount): equal cardinality, distinct keys, in-range values, and distinct values together prove it,
	// so a hostile stream cannot leave a live row unindexed or alias two rows to one index.
	inline void Read(std::istream& rStream, int64_t iCount)
	{
		// Rebuilding the map allocates buckets and nodes that persist across frames for stable ID lookups.
		ScopedSuppressAllocationTracking suppress;
		int64_t iSize = 0;
		common::Read(rStream, iSize);
		// Trust boundary: a hostile map size would make reserve() an unbounded allocation. Each entry
		// serializes at least an int64 value, so bound the count against the stream's remaining length.
		common::ValidateDeserializedCount(iSize, sizeof(int64_t), rStream, "OptionalIdToIndex::Read");
		if (iSize != iCount)
		{
			throw std::ios_base::failure("OptionalIdToIndex::Read");
		}
		idToIndexMap.clear();
		idToIndexMap.reserve(iSize);

		// Index-distinctness bitmap over [0, iCount): proves the values are a permutation, not merely in range.
		int64_t iWordCount = (iCount + 63) / 64;
		auto seenAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<uint64_t*>(iWordCount * sizeof(uint64_t));
		uint64_t* pSeen = static_cast<uint64_t*>(seenAllocation.mpData);
		std::memset(pSeen, 0, iWordCount * sizeof(uint64_t));

		for (int64_t i = 0; i < iSize; ++i)
		{
			id_t key {};
			int64_t iValue = 0;
			key.Read(rStream);
			common::Read(rStream, iValue);
			if (iValue < 0 || iValue >= iCount)
			{
				throw std::ios_base::failure("OptionalIdToIndex::Read");
			}
			uint64_t uiBit = 1ULL << (iValue & 63);
			if ((pSeen[iValue >> 6] & uiBit) != 0)
			{
				throw std::ios_base::failure("OptionalIdToIndex::Read");
			}
			pSeen[iValue >> 6] |= uiBit;
			// A duplicate stream key must not silently overwrite: it would leave a live row unindexed.
			if (!idToIndexMap.try_emplace(key, iValue).second)
			{
				throw std::ios_base::failure("OptionalIdToIndex::Read");
			}
		}
	}

	inline common::crc_t Crc() const
	{
		common::crc_t uiChecksum = 0;
		uiChecksum = (uiChecksum ^ common::Crc(static_cast<int64_t>(idToIndexMap.size()))) * common::kCrcMultiplier;

		// id_t/Uuid ordering compares only iValue, so sorting raw int64_t keys preserves CRC byte order.
		int64_t iKeyCount = static_cast<int64_t>(idToIndexMap.size());
		auto keysAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<int64_t*>(iKeyCount * sizeof(int64_t));
		int64_t* pKeys = static_cast<int64_t*>(keysAllocation.mpData);
		for (int64_t i = 0; const auto& [rKey, riValue] : idToIndexMap)
		{
			pKeys[i++] = rKey.uuid.iValue;
		}
		std::sort(pKeys, pKeys + iKeyCount);

		for (int64_t j = 0; j < iKeyCount; ++j)
		{
			uiChecksum = (uiChecksum ^ common::Crc(pKeys[j])) * common::kCrcMultiplier;
			uiChecksum = (uiChecksum ^ common::Crc(idToIndexMap.at(id_t {Uuid {pKeys[j]}}))) * common::kCrcMultiplier;
		}

		return uiChecksum;
	}

private:
	// Returns sorted keys for deterministic serialization ordering.
	std::vector<id_t> GetSortedKeys() const
	{
		std::vector<id_t> vecKeys;
		vecKeys.reserve(idToIndexMap.size());
		for (const auto& [rKey, riValue] : idToIndexMap)
		{
			vecKeys.push_back(rKey);
		}
		// id_t/Uuid ordering compares only iValue; this comparator preserves serialized key order.
		std::sort(vecKeys.begin(), vecKeys.end(), [](const id_t& rLeft, const id_t& rRight)
		{
			return rLeft.uuid.iValue < rRight.uuid.iValue;
		});
		return vecKeys;
	}
};

template <typename T, common::Flags<CollectionFlags> FLAGS = {}>
struct Collection : public OptionalIdToIndex<T, FLAGS>
{
	inline bool LogDifferences(const Collection& rOther) const
	{
		bool bEqual = true;
		if constexpr (FLAGS & CollectionFlags::kIdToIndex)
		{
			bEqual &= common::LogDifference<"idToIndexMap.size">(static_cast<int64_t>(this->idToIndexMap.size()), static_cast<int64_t>(rOther.idToIndexMap.size()));
		}
		bEqual &= common::LogDifference<"iCount">(iCount, rOther.iCount);
		bEqual &= common::LogDifference<"iCapacity">(iCapacity, rOther.iCapacity);
		return bEqual;
	}

	inline void Write(std::ostream& rStream) const
	{
		common::Write(rStream, iCount);
		common::Write(rStream, iCapacity);
		if constexpr (FLAGS & CollectionFlags::kIdToIndex)
		{
			static_cast<const OptionalIdToIndex<T, FLAGS>&>(*this).Write(rStream);
		}
	}

	inline void Read(std::istream& rStream)
	{
		common::Read(rStream, iCount);
		common::Read(rStream, iCapacity);
		// Save, replay, and network full-state reads require iCount <= iCapacity <= kiMaxDeserializedCapacity
		// to bound writes and allocation. With member stride unknown, validation uses a minimum stride of 1;
		// AllocateAndAssign enforces the 256 MiB ceiling using the actual member stride.
		common::ValidateDeserializedCountCapacity(iCount, iCapacity, 1, rStream, "Collection::Read");
		if constexpr (FLAGS & CollectionFlags::kIdToIndex)
		{
			// Validates the map is a bijection onto [0, iCount), so exactly one map entry backs each live row.
			static_cast<OptionalIdToIndex<T, FLAGS>&>(*this).Read(rStream, iCount);
		}
	}

	inline common::crc_t Crc() const
	{
		common::crc_t uiChecksum = 0;
		if constexpr (FLAGS & CollectionFlags::kIdToIndex)
		{
			uiChecksum = (uiChecksum ^ static_cast<const OptionalIdToIndex<T, FLAGS>&>(*this).Crc()) * common::kCrcMultiplier;
		}
		uiChecksum = (uiChecksum ^ common::Crc(iCount)) * common::kCrcMultiplier;
		uiChecksum = (uiChecksum ^ common::Crc(iCapacity)) * common::kCrcMultiplier;
		return uiChecksum;
	}

	int64_t iCount = 0;
	int64_t iCapacity = 0;
	// Transient physical stride capacity of the installed pData buffer; may exceed iCapacity after a shrink-reuse
	// deserialize. Derived storage state — excluded from Write/Read/Crc/LogDifferences and the member tuples.
	int64_t iPhysicalLayoutCapacity = 0;
	common::AlignedUniquePtr<std::byte> pData;
};

// Computes complete CRC of collection (metadata + all member arrays) for deterministic replay validation.
template <typename STRUCT, typename TUPLE>
inline common::crc_t CollectionCrc(const STRUCT& rCurrent, const TUPLE& rMembers)
{
	common::crc_t uiChecksum = 0;
	uiChecksum = (uiChecksum ^ rCurrent.Crc()) * common::kCrcMultiplier;
	uiChecksum = (uiChecksum ^ engine::MultiCrc(rCurrent.iCount, rMembers)) * common::kCrcMultiplier;
	return uiChecksum;
}

template <typename T>
concept HasSharedMembers = requires(const T value) { value.SharedMembers(); };

// Server broadcasts use the save-format Members() tuple; clients deserialize SharedMembers().
// Server Members() and SharedMembers() must have identical tuple types for wire/CRC parity.
template <typename STRUCT>
inline constexpr bool kbServerMembersParity = std::is_same_v<
	decltype(std::declval<const STRUCT&>().Members()),
	decltype(std::declval<const STRUCT&>().SharedMembers())>;

template <typename STRUCT>
inline common::crc_t SharedCollectionCrc(const STRUCT& rCurrent)
{
	if constexpr (HasSharedMembers<STRUCT>)
	{
#if defined(BT_SERVER)
		static_assert(kbServerMembersParity<STRUCT>, "Server-build Members() must be identical to SharedMembers() — wire format / CRC parity");
#endif
		return CollectionCrc(rCurrent, rCurrent.SharedMembers());
	}
	else
	{
		return CollectionCrc(rCurrent, rCurrent.Members());
	}
}

// Reads collection from a server-format stream. Allocates full Members() (zero-initialized) so client-only
// pointers are valid, then reads only SharedMembers() from the stream to match what the server wrote.
template <typename STRUCT>
inline std::istream& SharedCollectionRead(std::istream& rStream, STRUCT& rCurrent)
{
	rCurrent.Read(rStream);

	if (rCurrent.iCapacity > 0)
	{
		decltype(rCurrent.Members()) fullMembers = rCurrent.Members();
		AllocateAndAssign(rCurrent, rCurrent.iCapacity, fullMembers);

		// Zero the buffer so client-only fields default to 0 (invalid IDs, null references). Size from the physical-layout
		// capacity: after a shrink-reuse the buffer stays strided for the larger iPhysicalLayoutCapacity while iCapacity
		// holds the smaller stream value, so sizing from the latter would leave the tail rows above iCapacity un-zeroed.
		std::memset(rCurrent.pData.get(), 0, MemberTupleBufferSize(rCurrent.iPhysicalLayoutCapacity, fullMembers));
	}
	else
	{
		ResetDataToNull(rCurrent, rCurrent.Members());
	}

	if constexpr (HasSharedMembers<STRUCT>)
	{
#if defined(BT_SERVER)
		static_assert(kbServerMembersParity<STRUCT>, "Server-build Members() must be identical to SharedMembers() — wire format / CRC parity");
#endif
		// Every shared member requires storage allocated through Members().
		ASSERT(IsMemberTupleSubset(rCurrent.SharedMembers(), rCurrent.Members()));
		MultiRead(rStream, rCurrent.iCount, rCurrent.SharedMembers());
	}
	else
	{
		MultiRead(rStream, rCurrent.iCount, rCurrent.Members());
	}

	ValidateAfterRead(rStream, rCurrent);

	return rStream;
}

// Writes complete collection to stream (metadata + all member arrays) for save file serialization.
template <typename STRUCT, typename TUPLE>
inline std::ostream& CollectionWrite(std::ostream& rStream, const STRUCT& rCurrent, const TUPLE& rMembers)
{
	rCurrent.Write(rStream);
	engine::MultiWrite(rStream, rCurrent.iCount, rMembers);
	return rStream;
}

// Reads complete collection from stream (metadata + all member arrays) to restore from save files.
template <typename STRUCT, typename TUPLE>
inline std::istream& CollectionRead(std::istream& rStream, STRUCT& rCurrent, TUPLE&& rMembers)
{
	rCurrent.Read(rStream);
	engine::AllocateAndRead(rCurrent, rStream, std::forward<TUPLE>(rMembers));
	return rStream;
}

#if defined(BT_CLIENT)
// BeginRender capacity spans all active coordinates; Accessor returns a const collection reference from FrameInterpolate.
template <typename ACCESSOR>
int64_t AccumulateRenderCapacity(const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoords, ACCESSOR Accessor)
{
	int64_t iTotalCapacity = 0;
	for (const GridCoord& rCoord : rActiveCoords)
	{
		auto it = rRenderInterpolates.find(rCoord);
		if (it != rRenderInterpolates.end())
		{
			iTotalCapacity += Accessor(it->second).iCapacity;
		}
	}
	return iTotalCapacity;
}

// Erases entries from a render state map whose IDs are no longer present in any active collection.
// Accessor returns the collection's idToIndexMap from a FrameInterpolate reference.
template <typename MAP_TYPE, typename ACCESSOR>
void EraseStaleRenderState(MAP_TYPE& rRenderStateMap, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoords, ACCESSOR Accessor)
{
	// Heap: unordered_map erase for stale render state entries
	ScopedSuppressAllocationTracking suppress;
	std::erase_if(rRenderStateMap, [&rRenderInterpolates, &rActiveCoords, &Accessor](const typename MAP_TYPE::value_type& rPair)
	{
		for (const GridCoord& rCoord : rActiveCoords)
		{
			auto it = rRenderInterpolates.find(rCoord);
			if (it != rRenderInterpolates.end() && Accessor(it->second).contains(rPair.first))
			{
				return false;
			}
		}
		return true;
	});
}
#endif

} // namespace engine
