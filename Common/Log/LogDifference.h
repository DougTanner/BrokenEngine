#pragma once

namespace common
{

inline thread_local const char* gpLogDifferenceContext = "";

struct ScopedLogDifferenceContext
{
	const char* pcPrevious = nullptr;

	ScopedLogDifferenceContext(const char* pcContext)
	: pcPrevious(gpLogDifferenceContext)
	{
		gpLogDifferenceContext = pcContext;
	}

	~ScopedLogDifferenceContext()
	{
		gpLogDifferenceContext = pcPrevious;
	}

	ScopedLogDifferenceContext(const ScopedLogDifferenceContext&) = delete;
	ScopedLogDifferenceContext& operator=(const ScopedLogDifferenceContext&) = delete;
};

inline constexpr int64_t kiLogDifferencePrecision = 3;

template <typename T>
using LogDifferenceValue = std::conditional_t<std::is_same_v<T, float> || std::is_same_v<T, double>, Wb,
	std::conditional_t<std::is_same_v<T, XMFLOAT2>, WbV2,
	std::conditional_t<std::is_same_v<T, XMFLOAT3>, WbV3,
	std::conditional_t<std::is_same_v<T, XMFLOAT4> || std::is_same_v<T, XMFLOAT4A> || std::is_same_v<T, XMVECTOR>, WbV4, std::decay_t<const T&>>>>>;

// Keeps float-bearing diffs on the allocation-free Wb/WbV* formatters instead of the default heap-allocating std::format path.
template <typename T>
inline LogDifferenceValue<T> WrapLogDifferenceValue(const T& rValue)
{
	if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>)
	{
		return Wb(static_cast<float>(rValue), kiLogDifferencePrecision);
	}
	else if constexpr (std::is_same_v<T, XMFLOAT2>)
	{
		return WbV2(XMLoadFloat2(&rValue), kiLogDifferencePrecision);
	}
	else if constexpr (std::is_same_v<T, XMFLOAT3>)
	{
		return WbV3(XMLoadFloat3(&rValue), kiLogDifferencePrecision);
	}
	else if constexpr (std::is_same_v<T, XMFLOAT4>)
	{
		return WbV4(XMLoadFloat4(&rValue), kiLogDifferencePrecision);
	}
	else if constexpr (std::is_same_v<T, XMFLOAT4A>)
	{
		return WbV4(XMLoadFloat4A(&rValue), kiLogDifferencePrecision);
	}
	else if constexpr (std::is_same_v<T, XMVECTOR>)
	{
		return WbV4(rValue, kiLogDifferencePrecision);
	}
	else
	{
		return rValue;
	}
}

// Non-indexed version for scalar frame fields
template <FixedString NAME, typename T>
inline bool LogDifference(const T& rOne, const T& rTwo)
{
	bool bEqual = false;
	if constexpr (std::is_same_v<T, XMFLOAT2> || std::is_same_v<T, XMFLOAT3> || std::is_same_v<T, XMFLOAT4> || std::is_same_v<T, XMFLOAT4A> || std::is_same_v<T, XMVECTOR>)
	{
		bEqual = std::memcmp(&rOne, &rTwo, sizeof(T)) == 0;
	}
	else
	{
		bEqual = rOne == rTwo;
	}

	if (!bEqual) [[unlikely]]
	{
		LOG(kNetwork, kError, "LogDifferences {} {} Client: {} Server: {}", gpLogDifferenceContext, static_cast<const char*>(NAME), WrapLogDifferenceValue(rOne), WrapLogDifferenceValue(rTwo));
	}

	return bEqual;
}

// Indexed version for collection element fields (pVecPositions[i], etc.)
template <FixedString NAME, typename T>
inline bool LogDifference(int64_t iIndex, const T& rOne, const T& rTwo)
{
	bool bEqual = false;
	if constexpr (std::is_same_v<T, XMFLOAT2> || std::is_same_v<T, XMFLOAT3> || std::is_same_v<T, XMFLOAT4> || std::is_same_v<T, XMFLOAT4A> || std::is_same_v<T, XMVECTOR>)
	{
		bEqual = std::memcmp(&rOne, &rTwo, sizeof(T)) == 0;
	}
	else
	{
		bEqual = rOne == rTwo;
	}

	if (!bEqual) [[unlikely]]
	{
		LOG(kNetwork, kError, "LogDifferences {} {}[{}] Client: {} Server: {}", gpLogDifferenceContext, static_cast<const char*>(NAME), iIndex, WrapLogDifferenceValue(rOne), WrapLogDifferenceValue(rTwo));
	}

	return bEqual;
}

} // namespace common
