#pragma once

namespace engine
{

struct FramePostRenderBase;

// Stable entity identity across transfers and reconnects
// Assigned by server at first spawn, carried in TransferData
struct GlobalId
{
	int64_t iValue = 0;
	bool operator==(const GlobalId&) const = default;
};

// Global unique identifier with counter stored in FramePostRenderBase
// 0 = invalid/uninitialized, counter starts at 1
struct Uuid
{
	int64_t iValue = 0;

	constexpr Uuid() = default;
	constexpr explicit Uuid(int64_t iInitialValue) : iValue(iInitialValue)
	{
	}

	static Uuid Generate(FramePostRenderBase& rFramePostRender);
#if defined(BT_CLIENT)
	static Uuid GenerateVisual(FramePostRenderBase& rFramePostRender);
#endif

	constexpr bool operator==(const Uuid& rOther) const = default;
	constexpr std::strong_ordering operator<=>(const Uuid& rOther) const = default;

	void Write(std::ostream& rStream) const
	{
		common::Write(rStream, iValue);
	}
	void Read(std::istream& rStream)
	{
		common::Read(rStream, iValue);
	}
};

template <typename T>
struct Id
{
	Uuid uuid {};

	constexpr Id() = default;
	constexpr explicit Id(Uuid uuidValue)
	: uuid(uuidValue)
	{
	}

	static Id Generate(FramePostRenderBase& rFramePostRender)
	{
		return Id {Uuid::Generate(rFramePostRender)};
	}

#if defined(BT_CLIENT)
	static Id GenerateVisual(FramePostRenderBase& rFramePostRender)
	{
		return Id {Uuid::GenerateVisual(rFramePostRender)};
	}
#endif

	constexpr bool operator==(const Id& rOther) const = default;
	constexpr std::strong_ordering operator<=>(const Id& rOther) const = default;

	void Write(std::ostream& rStream) const
	{
		uuid.Write(rStream);
	}
	void Read(std::istream& rStream)
	{
		uuid.Read(rStream);
	}
};

} // namespace engine

namespace std
{

template <>
struct hash<engine::Uuid>
{
	std::size_t operator()(const engine::Uuid& rId) const noexcept
	{
		return std::hash<int64_t> {}(rId.iValue);
	}
};

template <typename T>
struct hash<engine::Id<T>>
{
	std::size_t operator()(const engine::Id<T>& rId) const noexcept
	{
		return std::hash<engine::Uuid> {}(rId.uuid);
	}
};

// Zero denotes an absent global identity.
template <>
struct formatter<engine::GlobalId> : std::formatter<std::string_view>
{
	template <typename CONTEXT>
	typename CONTEXT::iterator format(const engine::GlobalId id, CONTEXT& rContext) const
	{
		if (id.iValue == 0)
		{
			return std::formatter<std::string_view>::format("(none)", rContext);
		}

		char pcBuffer[24];
		char* pWrite = std::to_chars(pcBuffer, pcBuffer + sizeof(pcBuffer), id.iValue).ptr;
		return std::formatter<std::string_view>::format(std::string_view(pcBuffer, pWrite - pcBuffer), rContext);
	}
};

} // namespace std
