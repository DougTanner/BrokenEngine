#include "Agent/Commands/ServerFrameEdit.h"

#if defined(BT_SERVER)

#include "File/Replay.h"

#include "Agent/AgentCommandsServerQueries.h"
#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Players/Players.h"
#include "Frame/Collections/Spaceships/Spaceships.h"
#include "Frame/FrameCollections.h"
#include "Save/GameSaveLoad.h"
#include "Game.h"

namespace game
{

// MakeUuid keeps only the low 48 bits of the counter, so a written counter at or below this still leaves 2^47 mints
// before one wraps and aliases an earlier uuid.
constexpr uint64_t kuiUuidCounterLimit = 1ui64 << 47;

template <typename T> constexpr bool kbIsIdentity = false;
template <typename T> constexpr bool kbIsIdentity<engine::Id<T>> = true;

template <typename T> constexpr bool kbIsFlags = false;
template <typename ENUM> constexpr bool kbIsFlags<common::Flags<ENUM>> = true;

template <typename MEMBER_POINTER> struct MemberOwner;
template <typename OWNER, typename MEMBER> struct MemberOwner<MEMBER OWNER::*>
{
	using Type = OWNER;
};

using PlayersFrameColumns = engine::FrameColumnList<FRAME_COLUMN(PlayersInterpolate, pVecPositions), FRAME_COLUMN(PlayersInterpolate, pVecDirections), FRAME_COLUMN(PlayersInterpolate, pfDestroyedTimes), FRAME_COLUMN(PlayersInterpolate, pfAnimationTimes), FRAME_COLUMN(PlayersInterpolate, pPushers), FRAME_COLUMN(PlayersPostRender, pIds), FRAME_COLUMN(PlayersPostRender, pFlags), FRAME_COLUMN(PlayersPostRender, pAlignments), FRAME_COLUMN(PlayersPostRender, pfNextBlasterFireTimes), FRAME_COLUMN(PlayersPostRender, pfNextSecondarySpawnTimes), FRAME_COLUMN(PlayersPostRender, pVecVelocities), FRAME_COLUMN(PlayersPostRender, pVecWantedDirections), FRAME_COLUMN(PlayersPostRender, pfArmors), FRAME_COLUMN(PlayersPostRender, pfShields), FRAME_COLUMN(PlayersPostRender, pfShieldCooldowns), FRAME_COLUMN(PlayersPostRender, pfDestroyedExplosionTimes), FRAME_COLUMN(PlayersPostRender, pfShieldDownSoundCooldowns), FRAME_COLUMN(PlayersPostRender, pVecAiDirections), FRAME_COLUMN(PlayersPostRender, pfTransferLockTimers), FRAME_COLUMN(PlayersPostRender, pfArrivalGracePeriods), FRAME_COLUMN(PlayersPostRender, pfFrameChangeTimers), FRAME_COLUMN(PlayersPostRender, pVecIslandDestinations), FRAME_COLUMN(PlayersPostRender, pClientGuids), FRAME_COLUMN(PlayersPostRender, pGlobalPlayerIds), FRAME_COLUMN(PlayersPostRender, pfNavigationDelays), FRAME_COLUMN(PlayersPostRender, pFleetWantedCoordinates), FRAME_COLUMN(PlayersPostRender, puiPendingFleetWantedCoordinateTicks), FRAME_COLUMN(PlayersPostRender, puiPendingWeaponModeTicks)>;
using SpaceshipsFrameColumns = engine::FrameColumnList<FRAME_COLUMN(SpaceshipsInterpolate, pVecPositions), FRAME_COLUMN(SpaceshipsInterpolate, pVecDirections), FRAME_COLUMN(SpaceshipsInterpolate, pfDestroyedTimes), FRAME_COLUMN(SpaceshipsInterpolate, puiPushers), FRAME_COLUMN(SpaceshipsInterpolate, puiRegistryIds), FRAME_COLUMN(SpaceshipsInterpolate, pfDeltaRotations), FRAME_COLUMN(SpaceshipsPostRender, pFlags), FRAME_COLUMN(SpaceshipsPostRender, pVecVelocities), FRAME_COLUMN(SpaceshipsPostRender, pVecDamageDirections), FRAME_COLUMN(SpaceshipsPostRender, pfHealths), FRAME_COLUMN(SpaceshipsPostRender, pfDestroyedExplosionTimes), FRAME_COLUMN(SpaceshipsPostRender, pfNextBlasterSpawnTimes), FRAME_COLUMN(SpaceshipsPostRender, pAlignments), FRAME_COLUMN(SpaceshipsPostRender, pfArrivalGracePeriods)>;
using MissilesFrameColumns = engine::FrameColumnList<FRAME_COLUMN(MissilesInterpolate, pVecPositions), FRAME_COLUMN(MissilesInterpolate, pVecDirections), FRAME_COLUMN(MissilesInterpolate, pfDestroyedTimes), FRAME_COLUMN(MissilesPostRender, pFlags), FRAME_COLUMN(MissilesPostRender, pVecVelocities), FRAME_COLUMN(MissilesPostRender, pVecExplosionDirections), FRAME_COLUMN(MissilesPostRender, pVecStoredDirections), FRAME_COLUMN(MissilesPostRender, puiRegistryTargets), FRAME_COLUMN(MissilesPostRender, pfTimes), FRAME_COLUMN(MissilesPostRender, pfDeltaRotationDelays), FRAME_COLUMN(MissilesPostRender, pfDeltaRotations), FRAME_COLUMN(MissilesPostRender, pfNextJitter), FRAME_COLUMN(MissilesPostRender, pfDeltaRotationMaximum), FRAME_COLUMN(MissilesPostRender, pfAccelerations), FRAME_COLUMN(MissilesPostRender, pfPitches), FRAME_COLUMN(MissilesPostRender, pfExhaustLengths), FRAME_COLUMN(MissilesPostRender, pAlignments)>;
using BlastersFrameColumns = engine::FrameColumnList<FRAME_COLUMN(BlastersInterpolate, puiTypeIndices), FRAME_COLUMN(BlastersInterpolate, pVecPositions), FRAME_COLUMN(BlastersInterpolate, pVecDirections), FRAME_COLUMN(BlastersPostRender, pFlags), FRAME_COLUMN(BlastersPostRender, pVecVelocities), FRAME_COLUMN(BlastersPostRender, pAlignments)>;
using ExplosionsFrameColumns = engine::FrameColumnList<FRAME_COLUMN(engine::ExplosionsInterpolate, puiTypeIndices), FRAME_COLUMN(engine::ExplosionsInterpolate, pFlags), FRAME_COLUMN(engine::ExplosionsInterpolate, pfStartTimes), FRAME_COLUMN(engine::ExplosionsInterpolate, pVecPositions), FRAME_COLUMN(engine::ExplosionsInterpolate, pVecDirections), FRAME_COLUMN(engine::ExplosionsInterpolate, pfTimePercents), FRAME_COLUMN(engine::ExplosionsInterpolate, piTrailCounts), FRAME_COLUMN(engine::ExplosionsInterpolate, pfTrailTimes)>;
using PushersFrameColumns = engine::FrameColumnList<FRAME_COLUMN(engine::PushersInterpolate, pVecPositions), FRAME_COLUMN(engine::PushersInterpolate, pfRadii), FRAME_COLUMN(engine::PushersInterpolate, pfIntensities), FRAME_COLUMN(engine::PushersInterpolate, pfPowers), FRAME_COLUMN(engine::PushersInterpolate, pFlags), FRAME_COLUMN(engine::PushersPostRender, pIds)>;

template <typename... FIRST, typename... SECOND, typename... THIRD, typename... FOURTH>
static engine::FrameColumnList<FIRST..., SECOND..., THIRD..., FOURTH...> ConcatenateColumns(engine::FrameColumnList<FIRST...>, engine::FrameColumnList<SECOND...>, engine::FrameColumnList<THIRD...>, engine::FrameColumnList<FOURTH...>);

using FrameValueColumns = decltype(ConcatenateColumns(engine::FrameInterpolateBase::Values(), FrameInterpolate::Values(), engine::FramePostRenderBase::Values(), FramePostRender::Values()));

constexpr std::array<std::string_view, 3> kRefusedFrameValues {"iTick", "fCurrentTime", "fDeltaTime"};

static consteval bool IsRefusedFrameValue(std::string_view name)
{
	return std::ranges::contains(kRefusedFrameValues, name);
}

template <typename... COLUMNS>
static consteval bool ContainsRefusedFrameValues(engine::FrameColumnList<COLUMNS...>)
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
struct FrameCollection<engine::ExplosionsInterpolate>
{
	static constexpr std::string_view kName = "explosions";
	using Columns = ExplosionsFrameColumns;
};

template <>
struct FrameCollection<engine::PushersInterpolate>
{
	static constexpr std::string_view kName = "pushers";
	using Columns = PushersFrameColumns;
};

template <>
struct FrameCollection<PlayersInterpolate>
{
	static constexpr std::string_view kName = "players";
	using Columns = PlayersFrameColumns;
};

template <>
struct FrameCollection<BlastersInterpolate>
{
	static constexpr std::string_view kName = "blasters";
	using Columns = BlastersFrameColumns;
};

template <>
struct FrameCollection<MissilesInterpolate>
{
	static constexpr std::string_view kName = "missiles";
	using Columns = MissilesFrameColumns;
};

template <>
struct FrameCollection<SpaceshipsInterpolate>
{
	static constexpr std::string_view kName = "spaceships";
	using Columns = SpaceshipsFrameColumns;
};

template <typename COLUMN>
using ColumnReference = decltype(std::declval<typename MemberOwner<std::remove_cv_t<decltype(COLUMN::kpMember)>>::Type&>().*COLUMN::kpMember);

template <typename INTERPOLATE, typename POST_RENDER, typename... COLUMNS>
static consteval bool MatchesServerMembers(engine::FrameColumnList<COLUMNS...>)
{
	return std::is_same_v<std::tuple<ColumnReference<COLUMNS>...>, decltype(std::tuple_cat(std::declval<INTERPOLATE&>().Members(), std::declval<POST_RENDER&>().Members()))>;
}

template <size_t COUNT>
static consteval bool AreDistinct(std::array<std::string_view, COUNT> names)
{
	std::ranges::sort(names);
	return std::ranges::adjacent_find(names) == names.end();
}

template <typename... COLUMNS>
static consteval bool HasDistinctNames(engine::FrameColumnList<COLUMNS...>)
{
	return AreDistinct(std::array<std::string_view, sizeof...(COLUMNS)> {COLUMNS::kName...});
}

static_assert(HasDistinctNames(FrameValueColumns {}), "The frame Values() lists repeat a name; rename the value");
static_assert(ContainsRefusedFrameValues(FrameValueColumns {}), "kRefusedFrameValues names a value missing from the frame Values() lists; update kRefusedFrameValues");

static float FloatFromValue(const nlohmann::json& rValue, std::string_view name)
{
	// The transport parse already rejects a literal that overflows double, so only the narrowing to float can leave
	// the finite range.
	if (rValue.is_number())
	{
		double fValue = rValue.get<double>();
		if (std::abs(fValue) <= std::numeric_limits<float>::max())
		{
			return static_cast<float>(fValue);
		}
	}
	throw std::runtime_error(std::format("'{}' must be a finite number within the float range", name));
}

template <typename INTEGER>
static INTEGER IntegerFromValue(const nlohmann::json& rValue, std::string_view name)
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
static void ReadElement(const nlohmann::json& rWrites, std::string_view name, ELEMENT& rElement)
{
	const nlohmann::json& rValue = rWrites.at(std::string(name));
	if constexpr (kbIsIdentity<ELEMENT>)
	{
		throw std::runtime_error(std::format("'{}' is an identity or handle column and cannot be edited", name));
	}
	else if constexpr (std::is_same_v<ELEMENT, float>)
	{
		rElement = FloatFromValue(rValue, name);
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
		rElement = XMVectorSet(FloatFromValue(rValue.at(0), name), FloatFromValue(rValue.at(1), name), FloatFromValue(rValue.at(2), name), XMVectorGetW(rElement));
	}
	else if constexpr (kbIsFlags<ELEMENT>)
	{
		using Enum = decltype(ELEMENT::meFlags);
		rElement = ELEMENT(static_cast<Enum>(IntegerFromValue<std::underlying_type_t<Enum>>(rValue, name)));
	}
	else if constexpr (std::is_same_v<ELEMENT, engine::AlignmentIdentifier>)
	{
		rElement.uiValue = IntegerFromValue<uint32_t>(rValue, name);
	}
	else if constexpr (std::is_same_v<ELEMENT, engine::GlobalId>)
	{
		rElement.iValue = IntegerFromValue<int64_t>(rValue, name);
	}
	else if constexpr (std::is_same_v<ELEMENT, engine::ClientGuid>)
	{
		if (!rValue.is_array() || rValue.size() != 2)
		{
			throw std::runtime_error(std::format("'{}' must be a [high,low] array of integers", name));
		}
		rElement.uiHigh = IntegerFromValue<uint64_t>(rValue.at(0), name);
		rElement.uiLow = IntegerFromValue<uint64_t>(rValue.at(1), name);
	}
	else if constexpr (std::is_same_v<ELEMENT, engine::GridCoord>)
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
	else if constexpr (std::is_same_v<ELEMENT, engine::Alignments>)
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
			rElement.AddAlignment(engine::AlignmentIdentifier(IntegerFromValue<uint32_t>(rPair.at("a"), name)), engine::AlignmentIdentifier(IntegerFromValue<uint32_t>(rPair.at("b"), name)), IntegerFromValue<uint8_t>(rPair.at("flags"), name));
		}
	}
	else
	{
		static_assert(false, "edit_frame has no JSON value form for this column type");
	}
}

// IS_COLUMN selects row iIndex of a collection column; otherwise COLUMN is a frame value and iIndex is unused.
template <bool IS_COLUMN, typename COLUMN, typename INTERPOLATE, typename POST_RENDER>
static void WriteMember(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, int64_t iIndex, const nlohmann::json& rWrites)
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
			static_assert(std::is_same_v<std::remove_pointer_t<std::remove_extent_t<Member>>, float>, "edit_frame reads array columns of float only");
			const nlohmann::json& rValue = rWrites.at(std::string(kName));
			if (!rValue.is_array() || std::ssize(rValue) != std::ssize(rMember))
			{
				throw std::runtime_error(std::format("'{}' must be an array of {} finite numbers", kName, std::ssize(rMember)));
			}
			for (int64_t j = 0; const nlohmann::json& rTime : rValue)
			{
				rMember[j++][iIndex] = FloatFromValue(rTime, kName);
			}
		}
		else
		{
			ReadElement(rWrites, kName, rMember[iIndex]);
		}
	}
}

template <bool IS_COLUMN, typename INTERPOLATE, typename POST_RENDER, typename... COLUMNS>
static int64_t WriteKeys(engine::FrameColumnList<COLUMNS...>, INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, int64_t iIndex, const nlohmann::json& rWrites, std::string_view collection)
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

static const nlohmann::json& WritesFromEdit(const nlohmann::json& rEdit)
{
	if (!rEdit.contains("writes") || !rEdit.at("writes").is_object() || rEdit.at("writes").empty())
	{
		throw std::runtime_error("each edit requires non-empty object 'writes'");
	}
	return rEdit.at("writes");
}

template <typename COLUMNS, typename INTERPOLATE, typename POST_RENDER>
static int64_t WriteRow(COLUMNS columns, int64_t iCount, INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, const nlohmann::json& rEdit, std::string_view collection)
{
	static_assert(MatchesServerMembers<INTERPOLATE, POST_RENDER>(COLUMNS {}), "FrameCollection column list must list its pair's server Members() in order; update the list in this file");
	static_assert(HasDistinctNames(COLUMNS {}), "FrameCollection column list repeats a name; update the list in this file");
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
using FrameCollectionAt = FrameCollection<std::remove_reference_t<std::tuple_element_t<INDEX, COLLECTIONS>>>;

static int64_t ApplyEdit(Frame& rFrame, const nlohmann::json& rEdit)
{
	if (!rEdit.is_object() || !rEdit.contains("collection") || !rEdit.at("collection").is_string())
	{
		throw std::runtime_error("each edit must be an object with string 'collection'");
	}
	std::string collection = rEdit.at("collection").get<std::string>();
	if (collection == "frame")
	{
		for (const auto& [rKey, rValue] : rEdit.items())
		{
			if (rKey != "collection" && rKey != "writes")
			{
				throw std::runtime_error("'frame' edit takes only 'collection' and 'writes'");
			}
		}
		return WriteKeys<false>(FrameValueColumns {}, rFrame.interpolate, rFrame.postRender, 0, WritesFromEdit(rEdit), collection);
	}

	auto interpolates = std::tuple_cat(rFrame.interpolate.ServerCollections(), std::tie(*rFrame.interpolate.pPlayers), GameInterpolateCollections(rFrame.interpolate));
	auto postRenders = std::tuple_cat(rFrame.postRender.ServerCollections(), std::tie(*rFrame.postRender.pPlayers), GamePostRenderCollections(rFrame.postRender));
	using Interpolates = decltype(interpolates);
	return [&]<int64_t... INDICES>(std::integer_sequence<int64_t, INDICES...>)
	{
		static constexpr std::array<std::string_view, sizeof...(INDICES)> kNames {FrameCollectionAt<Interpolates, INDICES>::kName...};
		static_assert(AreDistinct(kNames), "FrameCollection names must be distinct; rename the repeated specialization in this file");
		int64_t iEdited = 0;
		bool bKnown = ((collection == kNames[INDICES] && (iEdited = WriteRow(typename FrameCollectionAt<Interpolates, INDICES>::Columns {}, std::get<INDICES>(postRenders).iCount, std::get<INDICES>(interpolates), std::get<INDICES>(postRenders), rEdit, collection), true)) || ...);
		if (!bKnown)
		{
			std::string names;
			for (std::string_view name : kNames)
			{
				names += name;
				names += '|';
			}
			throw std::runtime_error(std::format("'collection' must be {}frame", names));
		}
		return iEdited;
	}(std::make_integer_sequence<int64_t, static_cast<int64_t>(std::tuple_size_v<Interpolates>)> {});
}

void CommandEditFrame([[maybe_unused]] const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("edit_frame requires kbDebugInput build");
	}
	else
	{
		// No replay records the edit, so a recording or playback would diverge from the live frame.
		if (!engine::gpReplay->mReplayWriters.empty() || gpGame->mbReplaying || (gpGame->mGameFlags & engine::GameFlags::kSaveReplay) || (gpGame->mGameFlags & engine::GameFlags::kLoadReplay))
		{
			throw std::runtime_error("edit_frame is refused while recording, during playback, or with a replay_record or replay_play transition pending");
		}
		if (!rParameters.is_object())
		{
			throw std::runtime_error("edit_frame params must be an object");
		}
		for (const auto& [rKey, rValue] : rParameters.items())
		{
			if (rKey != "coord" && rKey != "edits")
			{
				throw std::runtime_error("edit_frame unknown parameter '" + rKey + "'");
			}
		}
		if (!rParameters.contains("edits") || !rParameters.at("edits").is_array() || rParameters.at("edits").empty())
		{
			throw std::runtime_error("edit_frame requires non-empty array 'edits'");
		}

		Frame& rLiveFrame = QueryFrame(rParameters);
		int64_t iClientCount = std::ranges::count_if(engine::gpServer->mClients, [](const engine::ClientConnection& rClient)
		{
			return rClient.bHandshakeComplete;
		});
		// The resynchronization frees every client subscription, so the same update's active set drops a playerless
		// cell other than the origin and would discard the edit.
		if (iClientCount > 0 && rLiveFrame.postRender.pPlayers->iCount == 0 && CoordinateFromParameter(rParameters) != engine::kOriginCoordinate)
		{
			throw std::runtime_error("edit_frame with a handshaken client requires a cell with a player or the origin cell");
		}
		uint64_t uiPreviousNextUuid = rLiveFrame.postRender.uiNextUuid;

		// Every write lands in this staged copy; the live frame changes only through the commit below.
		Frame stagedFrame;
		engine::TransferViaStream(rLiveFrame, stagedFrame);

		int64_t iEdited = 0;
		for (const nlohmann::json& rEdit : rParameters.at("edits"))
		{
			iEdited += ApplyEdit(stagedFrame, rEdit);
		}

		// The save reader does not check the counter: a lower one re-mints a uuid a live row holds.
		uint64_t uiNextUuid = stagedFrame.postRender.uiNextUuid;
		if (uiNextUuid != uiPreviousNextUuid && (uiNextUuid < uiPreviousNextUuid || uiNextUuid > kuiUuidCounterLimit))
		{
			throw std::runtime_error(std::format("'{}' must stay within [current, 2^47]", FRAME_COLUMN(engine::FramePostRenderBase, uiNextUuid)::kName));
		}

		// operator>> runs the save reader's checks on its own local frame and move-assigns into the live frame only
		// after all of them pass, so a rejected batch leaves the live frame and its uiSharedCrc untouched.
		engine::TransferViaStream(stagedFrame, rLiveFrame);
		// uiSharedCrc is not serialized, so the reader leaves it 0.
		rLiveFrame.postRender.uiSharedCrc = rLiveFrame.Crc();

		// Without a handshaken client there is nothing to resynchronize; skipping it keeps the load-generation budget
		// and the queued inject_payload entries.
		if (iClientCount > 0)
		{
			OnStateReplaced();
		}
		rResult["edited"] = iEdited;
		rResult["resynchronizedClients"] = iClientCount;
	}
}

} // namespace game

#endif // BT_SERVER
