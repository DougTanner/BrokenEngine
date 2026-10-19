#pragma once

#include "Frame/Collections/CollectionId.h"
#include "Frame/Alignments.h"
#include "Frame/FrameUtils.h"
#include "Network/NetworkCursor.h"

namespace engine
{

template <typename T> inline constexpr bool kbIsFlags = false;
template <typename ENUM> inline constexpr bool kbIsFlags<common::Flags<ENUM>> = true;

template <typename VALUE>
void WriteStatusChangeField(uint8_t*& rpCursor, const VALUE& rValue)
{
	if constexpr (std::is_same_v<VALUE, XMVECTOR>)
	{
		WriteVector4(rpCursor, rValue);
	}
	else if constexpr (std::is_same_v<VALUE, float>)
	{
		WriteFloat(rpCursor, rValue);
	}
	else if constexpr (std::is_same_v<VALUE, std::chrono::duration<float>>)
	{
		WriteFloat(rpCursor, rValue.count());
	}
	else if constexpr (std::is_same_v<VALUE, bool>)
	{
		WriteUint8(rpCursor, rValue ? 1 : 0);
	}
	else if constexpr (std::is_same_v<VALUE, uint8_t>)
	{
		WriteUint8(rpCursor, rValue);
	}
	else if constexpr (std::is_same_v<VALUE, int64_t>)
	{
		WriteInt64(rpCursor, rValue);
	}
	else if constexpr (std::is_same_v<VALUE, GridCoord>)
	{
		WriteGridCoord(rpCursor, rValue);
	}
	else if constexpr (std::is_same_v<VALUE, AlignmentIdentifier>)
	{
		WriteUint32(rpCursor, rValue.uiValue);
	}
	else if constexpr (std::is_same_v<VALUE, GlobalId>)
	{
		WriteInt64(rpCursor, rValue.iValue);
	}
	else if constexpr (kbIsFlags<VALUE>)
	{
		std::underlying_type_t<decltype(rValue.meFlags)> uiValue = std::to_underlying(rValue.meFlags);
		WriteBytes(rpCursor, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&uiValue), sizeof(uiValue)));
	}
	else if constexpr (std::is_enum_v<VALUE>)
	{
		std::underlying_type_t<VALUE> uiValue = std::to_underlying(rValue);
		WriteBytes(rpCursor, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&uiValue), sizeof(uiValue)));
	}
	else
	{
		static_assert(false, "status-change payload has no wire form for this member type");
	}
}

template <typename VALUE>
void ReadStatusChangeField(const uint8_t*& rpCursor, VALUE& rValue)
{
	if constexpr (std::is_same_v<VALUE, XMVECTOR>)
	{
		rValue = ReadVector4(rpCursor);
	}
	else if constexpr (std::is_same_v<VALUE, float>)
	{
		rValue = ReadFloat(rpCursor);
	}
	else if constexpr (std::is_same_v<VALUE, std::chrono::duration<float>>)
	{
		rValue = std::chrono::duration<float>(ReadFloat(rpCursor));
	}
	else if constexpr (std::is_same_v<VALUE, bool>)
	{
		rValue = ReadUint8(rpCursor) != 0;
	}
	else if constexpr (std::is_same_v<VALUE, uint8_t>)
	{
		rValue = ReadUint8(rpCursor);
	}
	else if constexpr (std::is_same_v<VALUE, int64_t>)
	{
		rValue = ReadInt64(rpCursor);
	}
	else if constexpr (std::is_same_v<VALUE, GridCoord>)
	{
		rValue = ReadGridCoord(rpCursor);
	}
	else if constexpr (std::is_same_v<VALUE, AlignmentIdentifier>)
	{
		rValue.uiValue = ReadUint32(rpCursor);
	}
	else if constexpr (std::is_same_v<VALUE, GlobalId>)
	{
		rValue.iValue = ReadInt64(rpCursor);
	}
	else if constexpr (kbIsFlags<VALUE>)
	{
		using Enum = decltype(rValue.meFlags);
		std::underlying_type_t<Enum> uiValue = 0;
		ReadBytes(rpCursor, std::span<uint8_t>(reinterpret_cast<uint8_t*>(&uiValue), sizeof(uiValue)));
		rValue.meFlags = static_cast<Enum>(uiValue);
	}
	else if constexpr (std::is_enum_v<VALUE>)
	{
		std::underlying_type_t<VALUE> uiValue = 0;
		ReadBytes(rpCursor, std::span<uint8_t>(reinterpret_cast<uint8_t*>(&uiValue), sizeof(uiValue)));
		rValue = static_cast<VALUE>(uiValue);
	}
	else
	{
		static_assert(false, "status-change payload has no wire form for this member type");
	}
}

// A member not in the Fields() table never crosses the wire and keeps its default when the payload is read.
template <typename DATA>
void WriteStatusChangeFields(uint8_t*& rpCursor, const DATA& rData)
{
	[&]<typename... COLUMNS>(FrameColumnList<COLUMNS...>)
	{
		(WriteStatusChangeField(rpCursor, rData.*COLUMNS::kpMember), ...);
	}(DATA::Fields());
}

template <typename DATA>
void ReadStatusChangeFields(const uint8_t*& rpCursor, DATA& rData)
{
	[&]<typename... COLUMNS>(FrameColumnList<COLUMNS...>)
	{
		(ReadStatusChangeField(rpCursor, rData.*COLUMNS::kpMember), ...);
	}(DATA::Fields());
}

} // namespace engine
