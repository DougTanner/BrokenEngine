#pragma once

#include "Threading/ThreadLocal.h"
#include "Workbuffer.h"

namespace common
{

// Assembles a brace-wrapped, comma-separated float list ("{1.5, 2.5}") in shortest round-trip form (matching std::format
// "{}") into a caller stack buffer, so the caller can forward the view through std::formatter<std::string_view> and have
// width/precision/fill specs apply.
inline std::string_view FormatVector(std::span<char> buffer, std::initializer_list<float> values)
{
	char* pWrite = buffer.data();
	char* pEnd = buffer.data() + buffer.size();
	auto Put = [&pWrite, pEnd](char c)
	{
		if (pWrite < pEnd)
		{
			*(pWrite++) = c;
		}
	};

	Put('{');
	bool bFirst = true;
	for (float fValue : values)
	{
		if (!bFirst)
		{
			Put(',');
			Put(' ');
		}
		bFirst = false;
		pWrite = std::to_chars(pWrite, pEnd, fValue).ptr;
	}
	Put('}');
	return std::string_view(buffer.data(), pWrite - buffer.data());
}

inline std::string_view FormatSuffixed(std::span<char> buffer, int64_t iValue, std::string_view suffix)
{
	char* pEnd = buffer.data() + buffer.size();
	char* pWrite = std::to_chars(buffer.data(), pEnd, iValue).ptr;
	for (char c : suffix)
	{
		if (pWrite < pEnd)
		{
			*(pWrite++) = c;
		}
	}
	return std::string_view(buffer.data(), pWrite - buffer.data());
}

} // namespace common

template<>
struct std::formatter<std::string> : std::formatter<std::string_view>
{
	template<typename CONTEXT>
	auto format(const std::string& rString, CONTEXT& rContext) const
	{
		return std::formatter<std::string_view>::format(rString, rContext);
	}
};

template<>
struct std::formatter<std::wstring> : std::formatter<std::string_view>
{
	template<typename CONTEXT>
	auto format(const std::wstring& rString, CONTEXT& rContext) const
	{
		// UTF-8-converts into the workbuffer (no heap allocation), so the calling thread needs a live ThreadLocal. The
		// ASSERT's _Analysis_assume_ is load-bearing — without it /analyze flags C6011 (null deref) as an error in the
		// Release code-analysis build.
		ASSERT(common::gpThreadLocal != nullptr);
		common::ScopedWorkbufferArena arena = common::gpThreadLocal->mWorkbuffer.Push();
		arena.mBuffer.Append(rString);
		return std::formatter<std::string_view>::format(arena.mBuffer.View(), rContext);
	}
};

template<>
struct std::formatter<std::filesystem::path> : std::formatter<std::string_view>
{
	template<typename CONTEXT>
	auto format(const std::filesystem::path& rPath, CONTEXT& rContext) const
	{
		// See the std::wstring formatter above: requires a live ThreadLocal, and the ASSERT is required for /analyze.
		ASSERT(common::gpThreadLocal != nullptr);
		common::ScopedWorkbufferArena arena = common::gpThreadLocal->mWorkbuffer.Push();
		arena.mBuffer.Append(rPath.native());
		return std::formatter<std::string_view>::format(arena.mBuffer.View(), rContext);
	}
};

template<>
struct std::formatter<XMFLOAT3> : std::formatter<std::string_view>
{
	template<typename CONTEXT>
	auto format(const XMFLOAT3 f3, CONTEXT& rContext) const
	{
		char pcBuffer[128];
		return std::formatter<std::string_view>::format(common::FormatVector(pcBuffer, {f3.x, f3.y, f3.z}), rContext);
	}
};

template<>
struct std::formatter<XMFLOAT4> : std::formatter<std::string_view>
{
	template<typename CONTEXT>
	auto format(const XMFLOAT4 f4, CONTEXT& rContext) const
	{
		char pcBuffer[128];
		return std::formatter<std::string_view>::format(common::FormatVector(pcBuffer, {f4.x, f4.y, f4.z, f4.w}), rContext);
	}
};

template<>
struct std::formatter<XMFLOAT4A> : std::formatter<std::string_view>
{
	template<typename CONTEXT>
	auto format(const XMFLOAT4A f4, CONTEXT& rContext) const
	{
		char pcBuffer[128];
		return std::formatter<std::string_view>::format(common::FormatVector(pcBuffer, {f4.x, f4.y, f4.z, f4.w}), rContext);
	}
};

template<>
struct std::formatter<XMVECTOR> : std::formatter<std::string_view>
{
	template<typename CONTEXT>
	auto format(const XMVECTOR vec, CONTEXT& rContext) const
	{
		XMFLOAT4A f4 {};
		XMStoreFloat4A(&f4, vec);
		char pcBuffer[128];
		return std::formatter<std::string_view>::format(common::FormatVector(pcBuffer, {f4.x, f4.y, f4.z, f4.w}), rContext);
	}
};

template<>
struct std::formatter<VkResult> : std::formatter<std::string_view>
{
	template<typename CONTEXT>
	auto format(const VkResult vkResult, CONTEXT& rContext) const
	{
		return std::formatter<std::string_view>::format(string_VkResult(vkResult), rContext);
	}
};

template<>
struct std::formatter<VkFilter> : std::formatter<std::string_view>
{
	template<typename CONTEXT>
	auto format(const VkFilter vkFilter, CONTEXT& rContext) const
	{
		return std::formatter<std::string_view>::format(string_VkFilter(vkFilter), rContext);
	}
};

template<>
struct std::formatter<VkSamplerAddressMode> : std::formatter<std::string_view>
{
	template<typename CONTEXT>
	auto format(const VkSamplerAddressMode vkSamplerAddressMode, CONTEXT& rContext) const
	{
		return std::formatter<std::string_view>::format(string_VkSamplerAddressMode(vkSamplerAddressMode), rContext);
	}
};

template<>
struct std::formatter<std::chrono::nanoseconds> : std::formatter<std::string_view>
{
	template<typename CONTEXT>
	auto format(const std::chrono::nanoseconds nanoseconds, CONTEXT& rContext) const
	{
		char pcBuffer[32];
		return std::formatter<std::string_view>::format(common::FormatSuffixed(pcBuffer, nanoseconds.count(), "ns"), rContext);
	}
};

template<>
struct std::formatter<std::chrono::microseconds> : std::formatter<std::string_view>
{
	template<typename CONTEXT>
	auto format(const std::chrono::microseconds microseconds, CONTEXT& rContext) const
	{
		char pcBuffer[32];
		return std::formatter<std::string_view>::format(common::FormatSuffixed(pcBuffer, microseconds.count(), "us"), rContext);
	}
};

template<>
struct std::formatter<std::chrono::milliseconds> : std::formatter<std::string_view>
{
	template<typename CONTEXT>
	auto format(const std::chrono::milliseconds milliseconds, CONTEXT& rContext) const
	{
		char pcBuffer[32];
		return std::formatter<std::string_view>::format(common::FormatSuffixed(pcBuffer, milliseconds.count(), "ms"), rContext);
	}
};

template<>
struct std::formatter<std::chrono::seconds> : std::formatter<std::string_view>
{
	template<typename CONTEXT>
	auto format(const std::chrono::seconds seconds, CONTEXT& rContext) const
	{
		char pcBuffer[32];
		return std::formatter<std::string_view>::format(common::FormatSuffixed(pcBuffer, seconds.count(), "s"), rContext);
	}
};

template<>
struct std::formatter<int8_t> : std::formatter<int>
{
	template<typename CONTEXT>
	auto format(const int8_t iValue, CONTEXT& rContext) const
	{
		return std::formatter<int>::format(static_cast<int>(iValue), rContext);
	}
};

template<>
struct std::formatter<uint8_t> : std::formatter<uint32_t>
{
	template<typename CONTEXT>
	auto format(const uint8_t uiValue, CONTEXT& rContext) const
	{
		return std::formatter<uint32_t>::format(static_cast<uint32_t>(uiValue), rContext);
	}
};

template<>
struct std::formatter<XMFLOAT2> : std::formatter<std::string_view>
{
	template<typename CONTEXT>
	auto format(const XMFLOAT2 f2, CONTEXT& rContext) const
	{
		char pcBuffer[128];
		return std::formatter<std::string_view>::format(common::FormatVector(pcBuffer, {f2.x, f2.y}), rContext);
	}
};

template<typename ENUM_TYPE>
struct std::formatter<common::Flags<ENUM_TYPE>> : std::formatter<std::underlying_type_t<ENUM_TYPE>>
{
	template<typename CONTEXT>
	auto format(const common::Flags<ENUM_TYPE> flags, CONTEXT& rContext) const
	{
		return std::formatter<std::underlying_type_t<ENUM_TYPE>>::format(std::to_underlying(flags.meFlags), rContext);
	}
};

template<>
struct std::formatter<common::RandomEngine> : std::formatter<uint64_t>
{
	template<typename CONTEXT>
	auto format(const common::RandomEngine& rEngine, CONTEXT& rContext) const
	{
		return std::formatter<uint64_t>::format(rEngine.State(), rContext);
	}
};

template <>
struct std::formatter<common::Wb> : std::formatter<std::string_view>
{
	template <typename CONTEXT>
	auto format(const common::Wb& rValue, CONTEXT& rContext) const
	{
		ASSERT(common::gpThreadLocal != nullptr);
		common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
		common::ScopedWorkbufferArena arena = rWorkbuffer.Push();
		arena.mBuffer.AppendFloat(rValue.fValue, rValue.iPrecision);
		return std::formatter<std::string_view>::format(arena.mBuffer.View(), rContext);
	}
};

template <>
struct std::formatter<common::WbV2> : std::formatter<std::string_view>
{
	template <typename CONTEXT>
	auto format(const common::WbV2& rValue, CONTEXT& rContext) const
	{
		ASSERT(common::gpThreadLocal != nullptr);
		common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
		common::ScopedWorkbufferArena arena = rWorkbuffer.Push();
		arena.mBuffer.Append(std::string_view("("));
		arena.mBuffer.AppendFloat(DirectX::XMVectorGetX(rValue.vecValue), rValue.iPrecision);
		arena.mBuffer.Append(std::string_view(","));
		arena.mBuffer.AppendFloat(DirectX::XMVectorGetY(rValue.vecValue), rValue.iPrecision);
		arena.mBuffer.Append(std::string_view(")"));
		return std::formatter<std::string_view>::format(arena.mBuffer.View(), rContext);
	}
};

template <>
struct std::formatter<common::WbV3> : std::formatter<std::string_view>
{
	template <typename CONTEXT>
	auto format(const common::WbV3& rValue, CONTEXT& rContext) const
	{
		ASSERT(common::gpThreadLocal != nullptr);
		common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
		common::ScopedWorkbufferArena arena = rWorkbuffer.Push();
		arena.mBuffer.Append(std::string_view("("));
		arena.mBuffer.AppendFloat(DirectX::XMVectorGetX(rValue.vecValue), rValue.iPrecision);
		arena.mBuffer.Append(std::string_view(","));
		arena.mBuffer.AppendFloat(DirectX::XMVectorGetY(rValue.vecValue), rValue.iPrecision);
		arena.mBuffer.Append(std::string_view(","));
		arena.mBuffer.AppendFloat(DirectX::XMVectorGetZ(rValue.vecValue), rValue.iPrecision);
		arena.mBuffer.Append(std::string_view(")"));
		return std::formatter<std::string_view>::format(arena.mBuffer.View(), rContext);
	}
};

template <>
struct std::formatter<common::WbV4> : std::formatter<std::string_view>
{
	template <typename CONTEXT>
	auto format(const common::WbV4& rValue, CONTEXT& rContext) const
	{
		ASSERT(common::gpThreadLocal != nullptr);
		common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
		common::ScopedWorkbufferArena arena = rWorkbuffer.Push();
		arena.mBuffer.Append(std::string_view("("));
		arena.mBuffer.AppendFloat(DirectX::XMVectorGetX(rValue.vecValue), rValue.iPrecision);
		arena.mBuffer.Append(std::string_view(","));
		arena.mBuffer.AppendFloat(DirectX::XMVectorGetY(rValue.vecValue), rValue.iPrecision);
		arena.mBuffer.Append(std::string_view(","));
		arena.mBuffer.AppendFloat(DirectX::XMVectorGetZ(rValue.vecValue), rValue.iPrecision);
		arena.mBuffer.Append(std::string_view(","));
		arena.mBuffer.AppendFloat(DirectX::XMVectorGetW(rValue.vecValue), rValue.iPrecision);
		arena.mBuffer.Append(std::string_view(")"));
		return std::formatter<std::string_view>::format(arena.mBuffer.View(), rContext);
	}
};
