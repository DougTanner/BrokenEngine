#pragma once

namespace common
{

// Zero-allocation hex conversion. Writes "0x" + uppercase hex digits + null terminator.
// Returns buffer data pointer for convenience. Fixed-extent span enables compile-time size verification.
template<std::unsigned_integral T, size_t N>
char* ToHex(std::span<char, N> pcBuffer, T uiValue)
{
	static_assert(N >= 2 + sizeof(T) * 2 + 1, "Buffer too small for ToHex output");
	constexpr char kacDigits[] = "0123456789ABCDEF";
	pcBuffer[0] = '0';
	pcBuffer[1] = 'x';
	constexpr int64_t kiDigits = sizeof(T) * 2;
	for (int64_t i = kiDigits - 1; i >= 0; --i)
	{
		pcBuffer[2 + i] = kacDigits[uiValue & 0xF];
		uiValue >>= 4;
	}
	pcBuffer[2 + kiDigits] = '\0';
	return pcBuffer.data();
}

// Converts UTF-16 text to UTF-8 with Win32 WideCharToMultiByte.
std::string ToString(std::wstring_view wideChars);

// Splits rString at each rDelimiter and appends the remaining suffix.
template<typename T>
std::vector<T> Split(const T& rString, const T& rDelimiter)
{
	int64_t iStart = 0;
	int64_t iEnd = 0;
	int64_t iDelimiterLength = rDelimiter.length();
	T token;

	std::vector<T> splits;
	while ((iEnd = rString.find(rDelimiter, iStart)) != T::npos)
	{
		token = rString.substr(iStart, iEnd - iStart);
		iStart = iEnd + iDelimiterLength;
		splits.push_back(token);
	}
	splits.push_back(rString.substr(iStart));
	return splits;
}

// Locale-independent: converts ASCII uppercase letters to lowercase and leaves other bytes unchanged.
std::string ToLower(std::string_view chars);

// Keeps ASCII letters, digits, and underscores independently of locale. The mapping is many-to-one;
// callers must check uniqueness and prefix the result to form a C++ identifier.
std::string PathToCppVariable(std::string_view path);

} // namespace common
