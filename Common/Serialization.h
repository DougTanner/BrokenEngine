#pragma once

namespace common
{

// Element-count limit for deserialized SOA capacity; byte size has a separate ceiling below.
inline constexpr int64_t kiMaxDeserializedCapacity = 1 << 24; // 16,777,216 elements

// Byte ceiling for deserialized SOA capacity. Its validator uses the per-element stride to cap each reservation at 256 MiB.
inline constexpr int64_t kiMaxDeserializedBytes = 256 << 20; // 268,435,456 bytes

// Bytes between the stream's current get position and its end. Requires a seekable stream — every
// save/replay file stream and the network istringstream are. Restores the get position.
inline int64_t StreamBytesRemaining(std::istream& rStream)
{
	std::streampos positionCurrent = rStream.tellg();
	rStream.seekg(0, std::ios::end);
	std::streampos positionEnd = rStream.tellg();
	rStream.seekg(positionCurrent);
	return static_cast<int64_t>(positionEnd - positionCurrent);
}

// Trust-boundary guard for a deserialized element count whose elements are read sequentially.
// iElementBytes is the minimum bytes one element consumes in the stream (>= 1); the count is bounded
// against the stream's remaining length so a hostile value cannot drive a large vector resize or loop.
// Divides (never multiplies) against the remaining length so the bound itself cannot overflow.
inline void ValidateDeserializedCount(int64_t iCount, int64_t iElementBytes, std::istream& rStream, std::string_view reader)
{
	if (iCount < 0)
	{
		throw std::ios_base::failure(std::string(reader));
	}

	if (iElementBytes <= 0)
	{
		throw std::ios_base::failure(std::string(reader));
	}

	if (iCount > StreamBytesRemaining(rStream) / iElementBytes)
	{
		throw std::ios_base::failure(std::string(reader));
	}
}

// As ValidateDeserializedCount, plus an SOA capacity (allocation size) and the count <= capacity
// invariant. iCount elements are read from the stream (so it is stream-length bounded); iCapacity
// sizes the buffer, so it is bounded by the absolute ceiling rather than the stream length.
inline void ValidateDeserializedCountCapacity(int64_t iCount, int64_t iCapacity, int64_t iElementBytes, std::istream& rStream, std::string_view reader)
{
	// The byte-ceiling clause divides (never multiplies) so the bound cannot overflow; iElementBytes <= 0 is
	// left to ValidateDeserializedCount below, which throws on it.
	if (iCount < 0 || iCapacity < 0 || iCount > iCapacity)
	{
		throw std::ios_base::failure(std::string(reader));
	}

	if (iCapacity > kiMaxDeserializedCapacity
	 || (iElementBytes > 0 && iCapacity > kiMaxDeserializedBytes / iElementBytes))
	{
		throw std::ios_base::failure(std::string(reader));
	}
	ValidateDeserializedCount(iCount, iElementBytes, rStream, reader);
}

template<typename T>
int64_t VectorByteSize(const std::vector<T>& rVector)
{
	return std::ssize(rVector) * sizeof(T);
}

// XMVECTOR requires its dedicated overload with no generic raw-byte fallback. Reads native x64 object
// representations (sizeof and endianness), including padding and float bytes; pass padding-free or
// zeroed POD types.
template<typename T> requires (!std::is_same_v<std::remove_cvref_t<T>, XMVECTOR>)
inline void Read(std::istream& rStream, T& rValue)
{
	static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
	rStream.read(reinterpret_cast<char*>(&rValue), sizeof(T));
}

template<typename T>
inline void Read(std::istream& rStream, std::span<T> values)
{
	static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
	rStream.read(reinterpret_cast<char*>(values.data()), values.size_bytes());
}

template<typename T>
inline void Read(std::istream& rStream, std::vector<T>& rVector)
{
	static_assert(std::is_trivially_copyable_v<T>, "Vector element type must be trivially copyable");
	rStream.read(reinterpret_cast<char*>(rVector.data()), VectorByteSize(rVector));
}

// XMVECTOR requires its dedicated overload with no generic raw-byte fallback. Writes native x64 object
// representations (sizeof and endianness), including padding and float bytes; pass padding-free or
// zeroed POD types.
template<typename T> requires (!std::is_same_v<std::remove_cvref_t<T>, XMVECTOR>)
inline void Write(std::ostream& rStream, const T& rValue)
{
	static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
	rStream.write(reinterpret_cast<const char*>(&rValue), sizeof(T));
}

template<typename T>
inline void Write(std::ostream& rStream, std::span<const T> values)
{
	static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
	rStream.write(reinterpret_cast<const char*>(values.data()), values.size_bytes());
}

template<typename T>
inline void Write(std::ostream& rStream, const std::vector<T>& rVector)
{
	static_assert(std::is_trivially_copyable_v<T>, "Vector element type must be trivially copyable");
	rStream.write(reinterpret_cast<const char*>(rVector.data()), VectorByteSize(rVector));
}

// XMVECTOR overloads - store/load via XMFLOAT4 for consistent serialization
inline void XM_CALLCONV Write(std::ostream& rStream, FXMVECTOR vecValue)
{
	XMFLOAT4 f4Temp;
	XMStoreFloat4(&f4Temp, vecValue);
	Write(rStream, f4Temp);
}

inline void Read(std::istream& rStream, XMVECTOR& rVecValue)
{
	XMFLOAT4 f4Temp;
	Read(rStream, f4Temp);
	rVecValue = XMLoadFloat4(&f4Temp);
}

} // namespace common
