#pragma once

#include "Math/MathUtils.h"

namespace common
{

// Grow-only storage whose base address never moves: one address-space reservation is taken on first use and every
// later growth commits more pages inside that same reservation, so every pointer, span, view, or RAII handle into
// the storage stays valid across a grow. The reservation is a hard ceiling — exceeding it asserts rather than
// relocating — and it charges no committed memory until a page is written, so a generous reserve is cheap.
// The constructor makes no OS call, which is what lets a thread_local instance be constructed before the process
// allocator is initialized.
template <typename T>
class StableVector
{
public:

	// iReservedCount is the element ceiling; the reservation covers that many elements' bytes rounded up to the
	// 64 KiB allocation granularity, so the base address is 64 KiB aligned and every element inside keeps T's alignment.
	explicit StableVector(int64_t iReservedCount)
	: miReservedBytes(common::RoundUp<int64_t, 64 * 1024>(iReservedCount * static_cast<int64_t>(sizeof(T))))
	{
	}

	~StableVector()
	{
		if (mpData == nullptr)
		{
			return;
		}

		std::destroy_n(mpData, miCount);
		VirtualFree(mpData, 0, MEM_RELEASE);
	}

	// Non-copyable/non-movable: the stable base address is the point of the type, and outstanding pointers into it
	// could not follow a copy or a move.
	StableVector(const StableVector&) = delete;
	StableVector& operator=(const StableVector&) = delete;
	StableVector(StableVector&&) = delete;
	StableVector& operator=(StableVector&&) = delete;

	// Grows only. The existing bytes are never touched, which is why outstanding pointers survive.
	void Resize(int64_t iCount)
	{
		ASSERT(iCount >= miCount);
		if (iCount == miCount)
		{
			return;
		}

		if (mpData == nullptr)
		{
			// VirtualAlloc is an OS trust boundary. Growth has no fallback and no source to re-read, so a failure
			// asserts instead of failing soft: there is nowhere else the elements could live.
			mpData = static_cast<T*>(VirtualAlloc(nullptr, static_cast<SIZE_T>(miReservedBytes), MEM_RESERVE, PAGE_READWRITE));
			ASSERT(mpData != nullptr);
		}

		int64_t iNeededBytes = iCount * static_cast<int64_t>(sizeof(T));
		// The reservation cannot be extended in place, so overrunning it is an under-sized reserve constant.
		ASSERT(iNeededBytes <= miReservedBytes);
		// Committing a range that is already partly committed is a no-op on the committed pages and leaves their
		// contents alone, so the whole prefix is committed in one call with no per-page bookkeeping.
		void* pCommitted = VirtualAlloc(mpData, static_cast<SIZE_T>(iNeededBytes), MEM_COMMIT, PAGE_READWRITE);
		ASSERT(pCommitted != nullptr);

		std::uninitialized_value_construct_n(mpData + miCount, iCount - miCount);
		miCount = iCount;
	}

	int64_t Size() const { return miCount; }
	T* Data() { return mpData; }
	const T* Data() const { return mpData; }

	T& operator[](int64_t iIndex)
	{
		ASSERT(iIndex >= 0 && iIndex < miCount);
		return mpData[iIndex];
	}

	const T& operator[](int64_t iIndex) const
	{
		ASSERT(iIndex >= 0 && iIndex < miCount);
		return mpData[iIndex];
	}

private:

	int64_t miReservedBytes = 0;
	int64_t miCount = 0;
	T* mpData = nullptr;
};

} // namespace common
