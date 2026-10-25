#pragma once

#if defined(BT_SERVER)

#include "Network/StatusChangeFields.h"

#include "Frame/Frame.h"

// edit_frame and read_frame column machinery. It names the game frame types, so it stays out of Engine.h and the
// game frame-edit commands include it directly.
namespace engine
{

// MakeUuid keeps only the low 48 bits of the counter, so a written counter at or below this still leaves 2^47 mints
// before one wraps and aliases an earlier uuid.
inline constexpr uint64_t kuiUuidCounterLimit = 1ui64 << 47;

template <typename T> inline constexpr bool kbIsIdentity = false;
template <typename T> inline constexpr bool kbIsIdentity<Id<T>> = true;

template <typename MEMBER_POINTER> struct MemberOwner;
template <typename OWNER, typename MEMBER> struct MemberOwner<MEMBER OWNER::*>
{
	using Type = OWNER;
};

using ExplosionsFrameColumns = FrameColumnList<FRAME_COLUMN(ExplosionsInterpolate, puiTypeIndices), FRAME_COLUMN(ExplosionsInterpolate, pStartTimes), FRAME_COLUMN(ExplosionsInterpolate, pVecPositions), FRAME_COLUMN(ExplosionsInterpolate, pVecDirections), FRAME_COLUMN(ExplosionsInterpolate, pfTimePercents), FRAME_COLUMN(ExplosionsInterpolate, piTrailCounts), FRAME_COLUMN(ExplosionsInterpolate, pTrailTimes), FRAME_COLUMN(ExplosionsPostRender, pLifecycleFlags)>;
using PushersFrameColumns = FrameColumnList<FRAME_COLUMN(PushersInterpolate, pVecPositions), FRAME_COLUMN(PushersInterpolate, pfRadii), FRAME_COLUMN(PushersInterpolate, pfIntensities), FRAME_COLUMN(PushersInterpolate, pfPowers), FRAME_COLUMN(PushersInterpolate, pFlags), FRAME_COLUMN(PushersPostRender, pIds), FRAME_COLUMN(PushersPostRender, pLifecycleFlags)>;

template <typename... FIRST, typename... SECOND, typename... THIRD, typename... FOURTH>
FrameColumnList<FIRST..., SECOND..., THIRD..., FOURTH...> ConcatenateColumns(FrameColumnList<FIRST...>, FrameColumnList<SECOND...>, FrameColumnList<THIRD...>, FrameColumnList<FOURTH...>);

using FrameValueColumns = decltype(ConcatenateColumns(FrameInterpolateBase::Values(), game::FrameInterpolate::Values(), FramePostRenderBase::Values(), game::FramePostRender::Values()));

inline constexpr std::array<std::string_view, 2> kRefusedFrameValues {"iTick", "fDeltaTime"};

consteval bool IsRefusedFrameValue(std::string_view name)
{
	return std::ranges::contains(kRefusedFrameValues, name);
}

template <typename... COLUMNS>
consteval bool ContainsRefusedFrameValues(FrameColumnList<COLUMNS...>)
{
	return std::ranges::all_of(kRefusedFrameValues, [](std::string_view name)
	{
		return ((name == COLUMNS::kName) || ...);
	});
}

// The primary template stays undefined, so a server collection pair added to a frame tuple fails the build until
// its specialization names it for edit_frame.
template <typename INTERPOLATE>
struct FrameCollection;

template <>
struct FrameCollection<ExplosionsInterpolate>
{
	static constexpr std::string_view kName = "explosions";
	using Columns = ExplosionsFrameColumns;
};

template <>
struct FrameCollection<PushersInterpolate>
{
	static constexpr std::string_view kName = "pushers";
	using Columns = PushersFrameColumns;
};

template <typename COLUMN>
using ColumnReference = decltype(std::declval<typename MemberOwner<std::remove_cv_t<decltype(COLUMN::kpMember)>>::Type&>().*COLUMN::kpMember);

template <typename INTERPOLATE, typename POST_RENDER, typename... COLUMNS>
consteval bool MatchesServerMembers(FrameColumnList<COLUMNS...>)
{
	return std::is_same_v<std::tuple<ColumnReference<COLUMNS>...>, decltype(std::tuple_cat(std::declval<INTERPOLATE&>().Members(), std::declval<POST_RENDER&>().Members()))>;
}

template <size_t COUNT>
consteval bool AreDistinct(std::array<std::string_view, COUNT> names)
{
	std::ranges::sort(names);
	return std::ranges::adjacent_find(names) == names.end();
}

template <typename... COLUMNS>
consteval bool HasDistinctNames(FrameColumnList<COLUMNS...>)
{
	return AreDistinct(std::array<std::string_view, sizeof...(COLUMNS)> {COLUMNS::kName...});
}

static_assert(HasDistinctNames(FrameValueColumns {}), "The frame Values() lists repeat a name; rename the value");
static_assert(ContainsRefusedFrameValues(FrameValueColumns {}), "kRefusedFrameValues names a value missing from the frame Values() lists; update kRefusedFrameValues");

template <typename INTEGER>
INTEGER IntegerFromValue(const nlohmann::json& rValue, std::string_view name)
{
	if (rValue.is_number_unsigned())
	{
		uint64_t uiValue = rValue.get<uint64_t>();
		if (std::in_range<INTEGER>(uiValue))
		{
			return static_cast<INTEGER>(uiValue);
		}
	}
	else if (rValue.is_number_integer())
	{
		int64_t iValue = rValue.get<int64_t>();
		if (std::in_range<INTEGER>(iValue))
		{
			return static_cast<INTEGER>(iValue);
		}
	}
	throw std::runtime_error(std::format("'{}' must be an integer in [{},{}]", name, std::numeric_limits<INTEGER>::min(), std::numeric_limits<INTEGER>::max()));
}

template <typename ELEMENT>
void ReadElement(const nlohmann::json& rWrites, std::string_view name, ELEMENT& rElement)
{
	const nlohmann::json& rValue = rWrites.at(std::string(name));
	if constexpr (kbIsIdentity<ELEMENT>)
	{
		throw std::runtime_error(std::format("'{}' is an identity or handle column and cannot be edited", name));
	}
	else if constexpr (std::is_same_v<ELEMENT, float>)
	{
		rElement = AgentFloatValue(rValue, name);
	}
	else if constexpr (std::is_same_v<ELEMENT, std::chrono::duration<float>>)
	{
		rElement = std::chrono::duration<float>(AgentFloatValue(rValue, name));
	}
	else if constexpr (std::is_same_v<ELEMENT, SimulationTime>)
	{
		rElement = SimulationTime(std::chrono::nanoseconds(IntegerFromValue<int64_t>(rValue, name)));
	}
	else if constexpr (std::is_same_v<ELEMENT, int32_t> || std::is_same_v<ELEMENT, uint8_t> || std::is_same_v<ELEMENT, uint16_t> || std::is_same_v<ELEMENT, uint64_t>)
	{
		rElement = IntegerFromValue<ELEMENT>(rValue, name);
	}
	else if constexpr (std::is_same_v<ELEMENT, XMVECTOR>)
	{
		if (!rValue.is_array() || rValue.size() != 3)
		{
			throw std::runtime_error(std::format("'{}' must be an [x,y,z] array of finite numbers", name));
		}
		rElement = XMVectorSet(AgentFloatValue(rValue.at(0), name), AgentFloatValue(rValue.at(1), name), AgentFloatValue(rValue.at(2), name), XMVectorGetW(rElement));
	}
	else if constexpr (kbIsFlags<ELEMENT>)
	{
		using Enum = decltype(ELEMENT::meFlags);
		rElement = ELEMENT(static_cast<Enum>(IntegerFromValue<std::underlying_type_t<Enum>>(rValue, name)));
	}
	else if constexpr (std::is_enum_v<ELEMENT>)
	{
		// The enum's own namespace supplies EnumFromToken and EnumToken, found by argument-dependent lookup.
		EnumFromToken(rValue, name, rElement);
	}
	else if constexpr (std::is_same_v<ELEMENT, AlignmentIdentifier>)
	{
		rElement.uiValue = IntegerFromValue<uint32_t>(rValue, name);
	}
	else if constexpr (std::is_same_v<ELEMENT, GlobalId>)
	{
		rElement.iValue = IntegerFromValue<int64_t>(rValue, name);
	}
	else if constexpr (std::is_same_v<ELEMENT, ClientGuid>)
	{
		if (!rValue.is_array() || rValue.size() != 2)
		{
			throw std::runtime_error(std::format("'{}' must be a [high,low] array of integers", name));
		}
		rElement.uiHigh = IntegerFromValue<uint64_t>(rValue.at(0), name);
		rElement.uiLow = IntegerFromValue<uint64_t>(rValue.at(1), name);
	}
	else if constexpr (std::is_same_v<ELEMENT, GridCoord>)
	{
		rElement = CoordinateFromParameter(rWrites, name);
	}
	else if constexpr (std::is_same_v<ELEMENT, common::RandomEngine>)
	{
		if (!rValue.is_number_unsigned() || rValue.get<uint64_t>() == 0)
		{
			throw std::runtime_error(std::format("'{}' must be a nonzero integer within uint64_t", name));
		}
		rElement.SetSerializedState(rValue.get<uint64_t>());
	}
	else if constexpr (std::is_same_v<ELEMENT, Alignments>)
	{
		auto IsAlignmentPair = [](const nlohmann::json& rPair)
		{
			return rPair.is_object() && rPair.size() == 3 && rPair.contains("a") && rPair.contains("b") && rPair.contains("flags");
		};
		if (!rValue.is_array() || !std::all_of(rValue.begin(), rValue.end(), IsAlignmentPair))
		{
			throw std::runtime_error(std::format("'{}' must be an array of {{a,b,flags}} objects", name));
		}
		// Rebuilding through AddAlignment keeps the table sorted with no repeated key, as CanCollide's search requires.
		rElement.alignmentPairs.clear();
		for (const nlohmann::json& rPair : rValue)
		{
			rElement.AddAlignment(AlignmentIdentifier(IntegerFromValue<uint32_t>(rPair.at("a"), name)), AlignmentIdentifier(IntegerFromValue<uint32_t>(rPair.at("b"), name)), IntegerFromValue<uint8_t>(rPair.at("flags"), name));
		}
	}
	else
	{
		static_assert(false, "edit_frame has no JSON value form for this column type");
	}
}

// IS_COLUMN selects row iIndex of a collection column; otherwise COLUMN is a frame value and iIndex is unused.
template <bool IS_COLUMN, typename COLUMN, typename INTERPOLATE, typename POST_RENDER>
void WriteMember(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, int64_t iIndex, const nlohmann::json& rWrites)
{
	static constexpr std::string_view kName = COLUMN::kName;
	if constexpr (!IS_COLUMN && IsRefusedFrameValue(kName))
	{
		throw std::runtime_error(std::format("'{}' is a clock value and cannot be edited", kName));
	}
	else
	{
		using Owner = typename MemberOwner<std::remove_cv_t<decltype(COLUMN::kpMember)>>::Type;
		Owner& rOwner = [&]() -> Owner&
		{
			if constexpr (std::is_base_of_v<Owner, INTERPOLATE>)
			{
				return rInterpolate;
			}
			else
			{
				static_assert(std::is_base_of_v<Owner, POST_RENDER>, "edit_frame column owner must be its pair's Interpolate or PostRender type or a base of one");
				return rPostRender;
			}
		}();
		using Member = std::remove_reference_t<decltype(rOwner.*COLUMN::kpMember)>;
		Member& rMember = rOwner.*COLUMN::kpMember;
		if constexpr (!IS_COLUMN)
		{
			ReadElement(rWrites, kName, rMember);
		}
		else if constexpr (std::is_array_v<Member>)
		{
			static_assert(std::is_same_v<std::remove_pointer_t<std::remove_extent_t<Member>>, std::chrono::duration<float>>, "edit_frame reads array columns of std::chrono::duration<float> only");
			const nlohmann::json& rValue = rWrites.at(std::string(kName));
			if (!rValue.is_array() || std::ssize(rValue) != std::ssize(rMember))
			{
				throw std::runtime_error(std::format("'{}' must be an array of {} finite numbers", kName, std::ssize(rMember)));
			}
			for (int64_t j = 0; const nlohmann::json& rTime : rValue)
			{
				rMember[j++][iIndex] = std::chrono::duration<float>(AgentFloatValue(rTime, kName));
			}
		}
		else
		{
			ReadElement(rWrites, kName, rMember[iIndex]);
		}
	}
}

template <bool IS_COLUMN, typename INTERPOLATE, typename POST_RENDER, typename... COLUMNS>
int64_t WriteKeys(FrameColumnList<COLUMNS...>, INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, int64_t iIndex, const nlohmann::json& rWrites, std::string_view collection)
{
	for (const auto& [rKey, rValue] : rWrites.items())
	{
		bool bKnown = ((rKey == COLUMNS::kName && (WriteMember<IS_COLUMN, COLUMNS>(rInterpolate, rPostRender, iIndex, rWrites), true)) || ...);
		if (!bKnown)
		{
			throw std::runtime_error(std::format("'{}' is not an editable {} member", rKey, collection));
		}
	}
	return std::ssize(rWrites);
}

inline const nlohmann::json& WritesFromEdit(const nlohmann::json& rEdit)
{
	if (!rEdit.contains("writes") || !rEdit.at("writes").is_object() || rEdit.at("writes").empty())
	{
		throw std::runtime_error("each edit requires non-empty object 'writes'");
	}
	return rEdit.at("writes");
}

template <typename COLUMNS, typename INTERPOLATE, typename POST_RENDER>
int64_t WriteRow(COLUMNS columns, int64_t iCount, INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, const nlohmann::json& rEdit, std::string_view collection)
{
	static_assert(MatchesServerMembers<INTERPOLATE, POST_RENDER>(COLUMNS {}), "FrameCollection column list must list its pair's server Members() in order; update the column list its FrameCollection specialization names");
	static_assert(HasDistinctNames(COLUMNS {}), "FrameCollection column list repeats a name; update the column list its FrameCollection specialization names");
	for (const auto& [rKey, rValue] : rEdit.items())
	{
		if (rKey != "collection" && rKey != "index" && rKey != "writes")
		{
			throw std::runtime_error(std::format("'{}' edit takes only 'collection', 'index', and 'writes'", collection));
		}
	}
	if (!rEdit.contains("index") || !rEdit.at("index").is_number_integer())
	{
		throw std::runtime_error(std::format("'{}' edit requires integer 'index' in [0,{})", collection, iCount));
	}
	int64_t iIndex = IntegerFromValue<int64_t>(rEdit.at("index"), "index");
	if (iIndex < 0 || iIndex >= iCount)
	{
		throw std::runtime_error(std::format("'{}' edit requires integer 'index' in [0,{})", collection, iCount));
	}
	return WriteKeys<true>(columns, rInterpolate, rPostRender, iIndex, WritesFromEdit(rEdit), collection);
}

template <typename COLLECTIONS, int64_t INDEX>
using FrameCollectionAt = FrameCollection<std::remove_cvref_t<std::tuple_element_t<INDEX, COLLECTIONS>>>;

// The inverse of ReadElement's value forms, plus the read-only identity and int64_t types.
template <typename ELEMENT>
void WriteElement(nlohmann::json& rValue, const ELEMENT& rElement)
{
	if constexpr (kbIsIdentity<ELEMENT>)
	{
		rValue = rElement.uuid.iValue;
	}
	else if constexpr (std::is_same_v<ELEMENT, float> || std::is_same_v<ELEMENT, int32_t> || std::is_same_v<ELEMENT, int64_t> || std::is_same_v<ELEMENT, uint8_t> || std::is_same_v<ELEMENT, uint16_t> || std::is_same_v<ELEMENT, uint64_t>)
	{
		rValue = rElement;
	}
	else if constexpr (std::is_same_v<ELEMENT, std::chrono::duration<float>>)
	{
		rValue = rElement.count();
	}
	else if constexpr (std::is_same_v<ELEMENT, SimulationTime>)
	{
		rValue = rElement.time_since_epoch().count();
	}
	else if constexpr (std::is_same_v<ELEMENT, XMVECTOR>)
	{
		rValue = nlohmann::json::array({XMVectorGetX(rElement), XMVectorGetY(rElement), XMVectorGetZ(rElement)});
	}
	else if constexpr (kbIsFlags<ELEMENT>)
	{
		rValue = std::to_underlying(rElement.meFlags);
	}
	else if constexpr (std::is_enum_v<ELEMENT>)
	{
		rValue = EnumToken(rElement);
	}
	else if constexpr (std::is_same_v<ELEMENT, AlignmentIdentifier>)
	{
		rValue = rElement.uiValue;
	}
	else if constexpr (std::is_same_v<ELEMENT, GlobalId>)
	{
		rValue = rElement.iValue;
	}
	else if constexpr (std::is_same_v<ELEMENT, ClientGuid>)
	{
		rValue = nlohmann::json::array({rElement.uiHigh, rElement.uiLow});
	}
	else if constexpr (std::is_same_v<ELEMENT, GridCoord>)
	{
		rValue = nlohmann::json::array({rElement.iQ, rElement.iR});
	}
	else if constexpr (std::is_same_v<ELEMENT, common::RandomEngine>)
	{
		rValue = rElement.State();
	}
	else if constexpr (std::is_same_v<ELEMENT, Alignments>)
	{
		// The key holds the smaller identifier in its high half.
		rValue = nlohmann::json::array();
		for (const AlignmentPair& rPair : rElement.alignmentPairs)
		{
			rValue.push_back({{"a", rPair.uiKey >> 32}, {"b", rPair.uiKey & 0xFFFF'FFFFui64}, {"flags", rPair.uiFlags}});
		}
	}
	else
	{
		static_assert(false, "read_frame has no JSON value form for this column type");
	}
}

// IS_COLUMN selects row iIndex of a collection column; otherwise COLUMN is a frame value and iIndex is unused.
template <bool IS_COLUMN, typename COLUMN, typename INTERPOLATE, typename POST_RENDER>
void ReadMember(const INTERPOLATE& rInterpolate, const POST_RENDER& rPostRender, int64_t iIndex, nlohmann::json& rValues)
{
	using Owner = typename MemberOwner<std::remove_cv_t<decltype(COLUMN::kpMember)>>::Type;
	const Owner& rOwner = [&]() -> const Owner&
	{
		if constexpr (std::is_base_of_v<Owner, INTERPOLATE>)
		{
			return rInterpolate;
		}
		else
		{
			static_assert(std::is_base_of_v<Owner, POST_RENDER>, "read_frame column owner must be its pair's Interpolate or PostRender type or a base of one");
			return rPostRender;
		}
	}();
	using Member = std::remove_reference_t<decltype(rOwner.*COLUMN::kpMember)>;
	Member& rMember = rOwner.*COLUMN::kpMember;
	nlohmann::json& rValue = rValues[std::string(COLUMN::kName)];
	if constexpr (!IS_COLUMN)
	{
		WriteElement(rValue, rMember);
	}
	else if constexpr (std::is_array_v<Member>)
	{
		rValue = nlohmann::json::array();
		for (const std::chrono::duration<float>* pElements : rMember)
		{
			WriteElement(rValue.emplace_back(), pElements[iIndex]);
		}
	}
	else
	{
		WriteElement(rValue, rMember[iIndex]);
	}
}

template <bool IS_COLUMN, typename INTERPOLATE, typename POST_RENDER, typename... COLUMNS>
nlohmann::json ReadKeys(FrameColumnList<COLUMNS...>, const INTERPOLATE& rInterpolate, const POST_RENDER& rPostRender, int64_t iIndex)
{
	nlohmann::json values = nlohmann::json::object();
	(ReadMember<IS_COLUMN, COLUMNS>(rInterpolate, rPostRender, iIndex, values), ...);
	return values;
}

template <typename COLUMNS, typename INTERPOLATE, typename POST_RENDER>
void ReadRow(COLUMNS columns, const INTERPOLATE& rInterpolate, const POST_RENDER& rPostRender, const nlohmann::json& rParameters, std::string_view collection, nlohmann::json& rResult)
{
	int64_t iCount = rPostRender.iCount;
	if (!rParameters.contains("index") || !rParameters.at("index").is_number_integer())
	{
		throw std::runtime_error(std::format("'{}' read requires integer 'index' in [0,{})", collection, iCount));
	}
	int64_t iIndex = IntegerFromValue<int64_t>(rParameters.at("index"), "index");
	if (iIndex < 0 || iIndex >= iCount)
	{
		throw std::runtime_error(std::format("'{}' read requires integer 'index' in [0,{})", collection, iCount));
	}
	rResult["count"] = iCount;
	rResult["index"] = iIndex;
	rResult["values"] = ReadKeys<true>(columns, rInterpolate, rPostRender, iIndex);
}

} // namespace engine

#endif // defined(BT_SERVER)
