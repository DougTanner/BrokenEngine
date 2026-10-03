#pragma once

#include "Frame/GridCoord.h"

namespace engine
{

// Cursor read helpers (shared across Network*.cpp files)
inline void ReadBytes(const uint8_t*& rpCursor, std::span<uint8_t> destination)
{
	std::memcpy(destination.data(), rpCursor, destination.size());
	rpCursor += destination.size();
}

inline uint8_t ReadUint8(const uint8_t*& rpCursor)
{
	return *rpCursor++;
}

inline uint16_t ReadUint16(const uint8_t*& rpCursor)
{
	uint16_t uiValue = 0;
	ReadBytes(rpCursor, std::span<uint8_t>(reinterpret_cast<uint8_t*>(&uiValue), sizeof(uint16_t)));
	return uiValue;
}

inline int32_t ReadInt32(const uint8_t*& rpCursor)
{
	int32_t iValue = 0;
	ReadBytes(rpCursor, std::span<uint8_t>(reinterpret_cast<uint8_t*>(&iValue), sizeof(int32_t)));
	return iValue;
}

inline uint32_t ReadUint32(const uint8_t*& rpCursor)
{
	uint32_t uiValue = 0;
	ReadBytes(rpCursor, std::span<uint8_t>(reinterpret_cast<uint8_t*>(&uiValue), sizeof(uint32_t)));
	return uiValue;
}

inline int64_t ReadInt64(const uint8_t*& rpCursor)
{
	int64_t iValue = 0;
	ReadBytes(rpCursor, std::span<uint8_t>(reinterpret_cast<uint8_t*>(&iValue), sizeof(int64_t)));
	return iValue;
}

inline uint64_t ReadUint64(const uint8_t*& rpCursor)
{
	uint64_t uiValue = 0;
	ReadBytes(rpCursor, std::span<uint8_t>(reinterpret_cast<uint8_t*>(&uiValue), sizeof(uint64_t)));
	return uiValue;
}

inline float ReadFloat(const uint8_t*& rpCursor)
{
	float fValue = 0.0f;
	ReadBytes(rpCursor, std::span<uint8_t>(reinterpret_cast<uint8_t*>(&fValue), sizeof(float)));
	return fValue;
}

inline XMVECTOR ReadVector4(const uint8_t*& rpCursor)
{
	XMFLOAT4A f4Value {};
	ReadBytes(rpCursor, std::span<uint8_t>(reinterpret_cast<uint8_t*>(&f4Value), sizeof(XMFLOAT4A)));
	return XMLoadFloat4A(&f4Value);
}

inline GridCoord ReadGridCoord(const uint8_t*& rpCursor)
{
	GridCoord gridCoordinate {};
	gridCoordinate.iX = ReadInt32(rpCursor);
	gridCoordinate.iY = ReadInt32(rpCursor);
	return gridCoordinate;
}

// Bounds-tracking wrapper over the unchecked read helpers above for variable-length payloads:
// callers verify Has()/Remaining() before passing pCursor to the Read* helpers
struct BoundedCursor
{
	const uint8_t* pCursor = nullptr;
	const uint8_t* pEnd = nullptr;

	int64_t Remaining() const
	{
		return pEnd - pCursor;
	}

	bool Has(int64_t iBytes) const
	{
		return Remaining() >= iBytes;
	}
};

// Cursor write helpers (shared across Network*.cpp files)
inline void WriteBytes(uint8_t*& rpCursor, std::span<const uint8_t> data)
{
	std::memcpy(rpCursor, data.data(), data.size());
	rpCursor += data.size();
}

inline void WriteUint8(uint8_t*& rpCursor, uint8_t uiValue)
{
	*rpCursor++ = uiValue;
}

inline void WriteUint16(uint8_t*& rpCursor, uint16_t uiValue)
{
	WriteBytes(rpCursor, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&uiValue), sizeof(uint16_t)));
}

inline void WriteInt32(uint8_t*& rpCursor, int32_t iValue)
{
	WriteBytes(rpCursor, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&iValue), sizeof(int32_t)));
}

inline void WriteUint32(uint8_t*& rpCursor, uint32_t uiValue)
{
	WriteBytes(rpCursor, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&uiValue), sizeof(uint32_t)));
}

inline void WriteInt64(uint8_t*& rpCursor, int64_t iValue)
{
	WriteBytes(rpCursor, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&iValue), sizeof(int64_t)));
}

inline void WriteUint64(uint8_t*& rpCursor, uint64_t uiValue)
{
	WriteBytes(rpCursor, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&uiValue), sizeof(uint64_t)));
}

inline void WriteFloat(uint8_t*& rpCursor, float fValue)
{
	WriteBytes(rpCursor, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&fValue), sizeof(float)));
}

inline void WriteVector4(uint8_t*& rpCursor, XMVECTOR vecValue)
{
	XMFLOAT4A f4Value {};
	XMStoreFloat4A(&f4Value, vecValue);
	WriteBytes(rpCursor, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&f4Value), sizeof(XMFLOAT4A)));
}

inline void WriteGridCoord(uint8_t*& rpCursor, GridCoord gridCoordinate)
{
	WriteInt32(rpCursor, gridCoordinate.iX);
	WriteInt32(rpCursor, gridCoordinate.iY);
}

inline void WriteGridCoord(common::Workbuffer& rWorkbuffer, GridCoord gridCoordinate)
{
	rWorkbuffer.PushBack<int32_t>(gridCoordinate.iX);
	rWorkbuffer.PushBack<int32_t>(gridCoordinate.iY);
}

template <typename T>
inline void PushSimplePacketArgument(common::Workbuffer& rWorkbuffer, const T& rArgument)
{
	if constexpr (std::is_same_v<T, GridCoord>)
	{
		WriteGridCoord(rWorkbuffer, rArgument);
	}
	else
	{
		static_assert(std::is_arithmetic_v<T>, "SendSimplePacket only supports arithmetic types and GridCoord; unwrap enums/ids/flags at the call site");
		rWorkbuffer.PushBack<T>(rArgument);
	}
}

} // namespace engine
