#include "Agent/Commands/ServerFrameEdit.h"

#if defined(BT_SERVER)

#include "Agent/Commands/FrameEdit.h"

#include "Agent/AgentCommandsServerQueries.h"
#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Omnis/Omnis.h"
#include "Frame/Collections/Spaceships/Spaceships.h"
#include "Frame/FrameCollections.h"
#include "Save/GameSaveLoad.h"
#include "Game.h"

namespace game
{

using OmnisFrameColumns = engine::FrameColumnList<FRAME_COLUMN(OmnisInterpolate, pVecPositions), FRAME_COLUMN(OmnisInterpolate, pVecDirections), FRAME_COLUMN(OmnisInterpolate, pDestroyedTimes), FRAME_COLUMN(OmnisInterpolate, pAnimationTimes), FRAME_COLUMN(OmnisInterpolate, pPushers), FRAME_COLUMN(OmnisPostRender, pIds), FRAME_COLUMN(OmnisPostRender, pFlags), FRAME_COLUMN(OmnisPostRender, pAlignments), FRAME_COLUMN(OmnisPostRender, pNextBlasterFireTimes), FRAME_COLUMN(OmnisPostRender, pNextSecondarySpawnTimes), FRAME_COLUMN(OmnisPostRender, pVecVelocities), FRAME_COLUMN(OmnisPostRender, pVecWantedDirections), FRAME_COLUMN(OmnisPostRender, pfArmors), FRAME_COLUMN(OmnisPostRender, pfShields), FRAME_COLUMN(OmnisPostRender, pShieldCooldowns), FRAME_COLUMN(OmnisPostRender, pDestroyedExplosionTimes), FRAME_COLUMN(OmnisPostRender, pShieldDownSoundCooldowns), FRAME_COLUMN(OmnisPostRender, pVecAiDirections), FRAME_COLUMN(OmnisPostRender, pTransferLockTimers), FRAME_COLUMN(OmnisPostRender, pArrivalGracePeriods), FRAME_COLUMN(OmnisPostRender, pCellChangeTimers), FRAME_COLUMN(OmnisPostRender, pVecIslandDestinations), FRAME_COLUMN(OmnisPostRender, pClientGuids), FRAME_COLUMN(OmnisPostRender, pGlobalIds), FRAME_COLUMN(OmnisPostRender, pNavigationDelays), FRAME_COLUMN(OmnisPostRender, pFleetWantedCoordinates), FRAME_COLUMN(OmnisPostRender, puiPendingFleetWantedCoordinateTicks), FRAME_COLUMN(OmnisPostRender, puiPendingWeaponModeTicks)>;
using SpaceshipsFrameColumns = engine::FrameColumnList<FRAME_COLUMN(SpaceshipsInterpolate, pVecPositions), FRAME_COLUMN(SpaceshipsInterpolate, pVecDirections), FRAME_COLUMN(SpaceshipsInterpolate, pDestroyedTimes), FRAME_COLUMN(SpaceshipsInterpolate, puiPushers), FRAME_COLUMN(SpaceshipsInterpolate, puiRegistryIds), FRAME_COLUMN(SpaceshipsInterpolate, pfDeltaRotations), FRAME_COLUMN(SpaceshipsPostRender, pFlags), FRAME_COLUMN(SpaceshipsPostRender, pVecVelocities), FRAME_COLUMN(SpaceshipsPostRender, pVecDamageDirections), FRAME_COLUMN(SpaceshipsPostRender, pfHealths), FRAME_COLUMN(SpaceshipsPostRender, pDestroyedExplosionTimes), FRAME_COLUMN(SpaceshipsPostRender, pNextBlasterSpawnTimes), FRAME_COLUMN(SpaceshipsPostRender, pAlignments), FRAME_COLUMN(SpaceshipsPostRender, pArrivalGracePeriods)>;
using MissilesFrameColumns = engine::FrameColumnList<FRAME_COLUMN(MissilesInterpolate, pVecPositions), FRAME_COLUMN(MissilesInterpolate, pVecDirections), FRAME_COLUMN(MissilesInterpolate, pDestroyedTimes), FRAME_COLUMN(MissilesPostRender, pFlags), FRAME_COLUMN(MissilesPostRender, pVecVelocities), FRAME_COLUMN(MissilesPostRender, pVecExplosionDirections), FRAME_COLUMN(MissilesPostRender, pVecStoredDirections), FRAME_COLUMN(MissilesPostRender, puiRegistryTargets), FRAME_COLUMN(MissilesPostRender, pTimes), FRAME_COLUMN(MissilesPostRender, pDeltaRotationDelays), FRAME_COLUMN(MissilesPostRender, pfDeltaRotations), FRAME_COLUMN(MissilesPostRender, pNextJitter), FRAME_COLUMN(MissilesPostRender, pfDeltaRotationMaximum), FRAME_COLUMN(MissilesPostRender, pfAccelerations), FRAME_COLUMN(MissilesPostRender, pfPitches), FRAME_COLUMN(MissilesPostRender, pfExhaustLengths), FRAME_COLUMN(MissilesPostRender, pAlignments)>;
using BlastersFrameColumns = engine::FrameColumnList<FRAME_COLUMN(BlastersInterpolate, puiTypeIndices), FRAME_COLUMN(BlastersInterpolate, pVecPositions), FRAME_COLUMN(BlastersInterpolate, pVecDirections), FRAME_COLUMN(BlastersPostRender, pFlags), FRAME_COLUMN(BlastersPostRender, pVecVelocities), FRAME_COLUMN(BlastersPostRender, pAlignments)>;

} // namespace game

namespace engine
{

template <>
struct FrameCollection<game::OmnisInterpolate>
{
	static constexpr std::string_view kName = "omnis";
	using Columns = game::OmnisFrameColumns;
};

template <>
struct FrameCollection<game::BlastersInterpolate>
{
	static constexpr std::string_view kName = "blasters";
	using Columns = game::BlastersFrameColumns;
};

template <>
struct FrameCollection<game::MissilesInterpolate>
{
	static constexpr std::string_view kName = "missiles";
	using Columns = game::MissilesFrameColumns;
};

template <>
struct FrameCollection<game::SpaceshipsInterpolate>
{
	static constexpr std::string_view kName = "spaceships";
	using Columns = game::SpaceshipsFrameColumns;
};

} // namespace engine

namespace game
{

// edit_frame and read_frame reach collection pairs only through these two tuples, so a pair added here reaches both.
template <typename FRAME>
static auto FrameCollections(FRAME& rFrame)
{
	return std::pair(std::tuple_cat(rFrame.interpolate.ServerCollections(), std::tie(*rFrame.interpolate.pOmnis), GameInterpolateCollections(rFrame.interpolate)), std::tuple_cat(rFrame.postRender.ServerCollections(), std::tie(*rFrame.postRender.pOmnis), GamePostRenderCollections(rFrame.postRender)));
}

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
		return engine::WriteKeys<false>(engine::FrameValueColumns {}, rFrame.interpolate, rFrame.postRender, 0, engine::WritesFromEdit(rEdit), collection);
	}

	auto [interpolates, postRenders] = FrameCollections(rFrame);
	using Interpolates = decltype(interpolates);
	return [&]<int64_t... INDICES>(std::integer_sequence<int64_t, INDICES...>)
	{
		static constexpr std::array<std::string_view, sizeof...(INDICES)> kNames {engine::FrameCollectionAt<Interpolates, INDICES>::kName...};
		static_assert(engine::AreDistinct(kNames), "FrameCollection names must be distinct; rename the repeated FrameCollection specialization");
		int64_t iEdited = 0;
		bool bKnown = ((collection == kNames[INDICES] && (iEdited = engine::WriteRow(typename engine::FrameCollectionAt<Interpolates, INDICES>::Columns {}, std::get<INDICES>(postRenders).iCount, std::get<INDICES>(interpolates), std::get<INDICES>(postRenders), rEdit, collection), true)) || ...);
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
		engine::RefuseDuringReplay("edit_frame");
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
		uint64_t uiPreviousNextUuid = rLiveFrame.postRender.uiNextUuid;

		// Every write lands in this staged copy; the live frame changes only through the commit below.
		Frame stagedFrame;
		engine::TransferViaStream(rLiveFrame, stagedFrame);

		int64_t iEdited = 0;
		for (const nlohmann::json& rEdit : rParameters.at("edits"))
		{
			iEdited += ApplyEdit(stagedFrame, rEdit);
		}

		int64_t iClientCount = std::ranges::count_if(engine::gpServer->mClients, [](const engine::ClientConnection& rClient)
		{
			return rClient.bHandshakeComplete;
		});
		// The resynchronization frees every client subscription, so the same update's active set drops an omniless
		// cell that is neither pinned nor the origin and would discard the edit. The staged pin admits a pin edit and
		// the live pin admits a release; edits never add or remove rows, so both frames hold the same omni count.
		if (iClientCount > 0 && rLiveFrame.postRender.pOmnis->iCount == 0 && rLiveFrame.postRender.uiPinned == 0 && stagedFrame.postRender.uiPinned == 0 && engine::CoordinateFromParameter(rParameters) != engine::kOriginCoordinate)
		{
			throw std::runtime_error("edit_frame with a handshaken client requires a cell with an omni, a pinned cell, or the origin cell");
		}

		// The save reader does not check the counter: a lower one re-mints a uuid a live row holds.
		uint64_t uiNextUuid = stagedFrame.postRender.uiNextUuid;
		if (uiNextUuid != uiPreviousNextUuid && (uiNextUuid < uiPreviousNextUuid || uiNextUuid > engine::kuiUuidCounterLimit))
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

void CommandReadFrame([[maybe_unused]] const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("read_frame requires kbDebugInput build");
	}
	else
	{
		for (const auto& [rKey, rValue] : rParameters.items())
		{
			if (rKey != "coord" && rKey != "collection" && rKey != "index")
			{
				throw std::runtime_error("read_frame unknown parameter '" + rKey + "'");
			}
		}
		if (!rParameters.contains("collection") || !rParameters.at("collection").is_string())
		{
			throw std::runtime_error("read_frame requires string 'collection'");
		}

		const Frame& rFrame = QueryFrame(rParameters);
		std::string collection = rParameters.at("collection").get<std::string>();
		if (collection == "frame")
		{
			if (rParameters.contains("index"))
			{
				throw std::runtime_error("'frame' read takes only 'coord' and 'collection'");
			}
			rResult["values"] = engine::ReadKeys<false>(engine::FrameValueColumns {}, rFrame.interpolate, rFrame.postRender, 0);
			return;
		}

		auto [interpolates, postRenders] = FrameCollections(rFrame);
		using Interpolates = decltype(interpolates);
		[&]<int64_t... INDICES>(std::integer_sequence<int64_t, INDICES...>)
		{
			static constexpr std::array<std::string_view, sizeof...(INDICES)> kNames {engine::FrameCollectionAt<Interpolates, INDICES>::kName...};
			bool bKnown = ((collection == kNames[INDICES] && (engine::ReadRow(typename engine::FrameCollectionAt<Interpolates, INDICES>::Columns {}, std::get<INDICES>(interpolates), std::get<INDICES>(postRenders), rParameters, collection, rResult), true)) || ...);
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
		}(std::make_integer_sequence<int64_t, static_cast<int64_t>(std::tuple_size_v<Interpolates>)> {});
	}
}

} // namespace game

#endif // BT_SERVER
