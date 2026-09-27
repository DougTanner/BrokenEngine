#pragma once

namespace common
{

struct AlignedDeleter
{
	void operator()(void* p) const noexcept
	{
		_aligned_free(p);
	}
};

// Owns 64-byte-aligned SIMD storage from MakeAligned; AlignedDeleter's _aligned_free accepts only
// _aligned_malloc memory.
template<typename T>
using AlignedUniquePtr = std::unique_ptr<T[], AlignedDeleter>;

// Allocates iCount elements with 64-byte alignment; an allocation failure returns an empty pointer without throwing.
template<typename T>
AlignedUniquePtr<T> MakeAligned(int64_t iCount)
{
	static_assert(std::is_trivially_default_constructible_v<T>, "MakeAligned requires a trivially default-constructible type");
	static_assert(std::is_trivially_destructible_v<T>, "MakeAligned requires a trivially destructible type");
	size_t uiBytes = static_cast<size_t>(iCount) * sizeof(T);
	ASSERT(iCount >= 0 && uiBytes / sizeof(T) == static_cast<size_t>(iCount));
	return AlignedUniquePtr<T>(static_cast<T*>(_aligned_malloc(uiBytes, 64)));
}

} // namespace common
