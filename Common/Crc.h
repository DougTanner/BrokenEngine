#pragma once

namespace common
{

using crc_t = uint64_t;

// Compile-time string wrapper for use as non-type template parameter (C++20 NTTP)
// Enables passing string literals directly as template arguments
// Template parameter: N - Size of the string including null terminator
template<size_t N>
struct FixedString
{
	char data[N] {};

	constexpr FixedString(const char (&str)[N])
	{
		for (size_t i = 0; i < N; ++i)
		{
			data[i] = str[i];
		}
	}

	constexpr operator const char*() const { return data; }
};

// Generates decimal suffixes for compile-time CRC arrays; input must be nonnegative.
constexpr std::string IntToString(int64_t i)
{
	std::string string;

	do
	{
		int64_t iDigit = i % 10;
		i = i / 10;
		string.push_back(static_cast<char>(iDigit) + '0');
	}
	while (i > 0);

	std::reverse(string.begin(), string.end());
	return string;
}

// Single definition of the hash core's magic constants, shared by the runtime constexpr Crc and the
// consteval CrcConsteval wrapper so the two evaluation paths can never diverge.
inline constexpr crc_t kCrcSeed = 0xabcdef123456789a;
inline constexpr crc_t kCrcMultiplier = 0x123456789abcdef1;
// Changing these constants or the mixing fold below alters every persisted/replicated CRC — bump the
// save/replay version gate game::Frame::kiVersion in the same change. That gate is the only signal
// separating "data desynced" from "checksum algorithm changed" (skip it and straddling replays false-desync).

// Custom 64-bit hash used for asset identification and lookup; it is not standard CRC32/64.
constexpr crc_t Crc(std::string_view pData)
{
	crc_t crc = kCrcSeed;
	// Fold each byte as unsigned char so values >= 0x80 zero-extend deterministically regardless of
	// char signedness (signed char sign-extends on the XOR, changing the hash cross-toolchain).
	// Raw-pointer index loop with an 8-wide manual unroll: preserves the exact per-byte fold order and
	// ops while shedding the Debug range-for iterator overhead. static_cast (not reinterpret_cast) keeps
	// the char->unsigned char conversion a value cast, so the fold stays constexpr-evaluable.
	const char* pBytes = pData.data();
	int64_t iSize = static_cast<int64_t>(pData.size());
	int64_t i = 0;
	for (; i + 8 <= iSize; i += 8)
	{
		crc = (crc ^ static_cast<unsigned char>(pBytes[i + 0])) * kCrcMultiplier;
		crc = (crc ^ static_cast<unsigned char>(pBytes[i + 1])) * kCrcMultiplier;
		crc = (crc ^ static_cast<unsigned char>(pBytes[i + 2])) * kCrcMultiplier;
		crc = (crc ^ static_cast<unsigned char>(pBytes[i + 3])) * kCrcMultiplier;
		crc = (crc ^ static_cast<unsigned char>(pBytes[i + 4])) * kCrcMultiplier;
		crc = (crc ^ static_cast<unsigned char>(pBytes[i + 5])) * kCrcMultiplier;
		crc = (crc ^ static_cast<unsigned char>(pBytes[i + 6])) * kCrcMultiplier;
		crc = (crc ^ static_cast<unsigned char>(pBytes[i + 7])) * kCrcMultiplier;
	}
	for (; i < iSize; ++i)
	{
		crc = (crc ^ static_cast<unsigned char>(pBytes[i])) * kCrcMultiplier;
	}
	return crc;
}

#pragma warning(suppress: 26497) // consteval is stricter than constexpr
consteval crc_t CrcConsteval(std::string_view pData)
{
	// Forward to the constexpr Crc: a single mixing-loop definition that consteval still forces to
	// evaluate at compile time. Keeps the two paths byte-identical by construction.
	return Crc(pData);
}
static_assert(Crc("test") == CrcConsteval("test"), "CRC functions must produce identical results");

// Hashes native object representations, including padding and float bit patterns; use layout-stable,
// padding-free or deterministically-zeroed data.
template<typename T>
inline crc_t Crc(const T* pValues, int64_t iCount)
{
	static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
	return Crc(std::string_view(reinterpret_cast<const char*>(pValues), iCount * sizeof(T)));
}

// Concept to exclude string-like types from template Crc
// Prevents ambiguous overload resolution by excluding types convertible to string_view
template<typename T>
concept NotStringLike = !std::is_convertible_v<T, std::string_view>;

// String-like types use the string_view overload, and XMVECTOR requires its dedicated overload with no
// generic fallback. Hashing includes native padding and float bit patterns; pass value-initialized,
// padding-free or deterministically-zeroed objects.
template <typename T> requires NotStringLike<T>
	&& (!std::is_pointer_v<std::remove_cvref_t<T>>)
	&& (!std::is_same_v<std::remove_cvref_t<T>, XMVECTOR>)
inline crc_t XM_CALLCONV Crc(const T& rIn)
{
	static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable to hash by byte reinterpretation");
	return Crc(std::string_view(reinterpret_cast<const char*>(&rIn), sizeof(rIn)));
}

// XMVECTOR overload - stores to XMFLOAT4 for consistent hashing
inline crc_t XM_CALLCONV Crc(FXMVECTOR vecIn)
{
	XMFLOAT4 f4Temp;
	XMStoreFloat4(&f4Temp, vecIn);
	return Crc(f4Temp);
}

// Compile-time generation of CRC hash arrays with sequential numbering
// Generates array of CRCs for strings like "prefix0suffix", "prefix1suffix", etc.
// Used for creating lookup tables of related asset names
// Template parameter: SIZE - Number of elements in the array
template<int64_t SIZE>
struct ConstexprCrcArray
{
	int64_t miCount = SIZE;
	crc_t mArray[SIZE];

	consteval ConstexprCrcArray(const char* pcPrefix, const char* pcSuffix)
	{
		for (int64_t i = 0; i < SIZE; ++i)
		{
			mArray[i] = CrcConsteval(std::string(pcPrefix) + IntToString(i) + std::string(pcSuffix));
		}
	}

	crc_t operator[](int64_t i) const
	{
		return mArray[i];
	}
};

} // namespace common
