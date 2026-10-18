#include "Pch.h"

#include "Agent/Commands/ServerSimulationHarnessRigs.h"

#if defined(BT_SERVER)

#include "Agent/Commands/ReplayHarnessRigs.h"
#include "File/Replay.h"
#include "Frame/FlightPlanes.h"
#include "Network/Server/ServerTransferManager.h"
#include "Network/StatusChangeFields.h"

#include "Agent/AgentCommandsServerQueries.h"
#include "Frame/Collections/Omnis/Omnis.h"
#include "Network/Server/ServerSession.h"
#include "Game.h"

namespace game
{

// iTick is the tick the change is released on, or the first later tick its coordinate can take it.
struct ScheduledStatusChange
{
	int64_t iTick = 0;
	StatusChange change;
};

struct ServerSimulationHarnessRigState
{
	const ServerSession* pSession = nullptr;
	std::unordered_map<engine::GridCoord, std::vector<ScheduledStatusChange>> pendingAgentStatusChanges;
};

static ServerSimulationHarnessRigState sHarnessRig;

static void Bind(const ServerSession& rSession)
{
	if (sHarnessRig.pSession != &rSession)
	{
		sHarnessRig = {};
		sHarnessRig.pSession = &rSession;
	}
}

static bool IsCoordinateActive(engine::GridCoord coordinate)
{
	return std::ranges::contains(gpGame->mActiveCoordinates, coordinate);
}

static void CommandReplayRecord([[maybe_unused]] const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	// SaveLoadReplay/SyncReplayTick are compiled out under !kbDebugInput, so the flag would never be consumed.
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("replay requires kbDebugInput build");
	}
	else
	{
		if (!rParameters.contains("start"))
		{
			throw std::runtime_error("replay_record requires bool 'start'");
		}
		if (!rParameters.at("start").is_boolean())
		{
			throw std::runtime_error("replay_record requires bool 'start'");
		}
		bool bStart = rParameters.at("start").get<bool>();
		if (bStart && (game::gpGame->mbReplaying || (game::gpGame->mGameFlags & engine::GameFlags::kLoadReplay)))
		{
			throw std::runtime_error("replay_record start rejected: playback is active or pending");
		}
		// SyncReplayTick consumes kSaveReplay as a start request on a dispatched tick; SaveLoadReplay consumes it as a stop request on its next run.
		// A pending flag inverts writer presence to give the requested state; setting it schedules a transition, and clearing it cancels one.
		bool bFlagPending = gpGame->mGameFlags & engine::GameFlags::kSaveReplay;
		bool bEffective = (!engine::gpReplay->mReplayWriters.empty()) != bFlagPending;
		if (bStart == bEffective)
		{
			rResult["pending"] = false;
		}
		else if (bFlagPending)
		{
			// Cancel a pending transition (e.g. a stop request voids a not-yet-started recording).
			if (engine::gpReplay->mReplayWriters.empty())
			{
				engine::ReplayHarnessRigs::Reset(*engine::gpReplay);
			}
			gpGame->mGameFlags.Set(engine::GameFlags::kSaveReplay, false);
			rResult["pending"] = false;
		}
		else
		{
			gpGame->mGameFlags.Set(engine::GameFlags::kSaveReplay);
			rResult["pending"] = true;
		}
	}
}

static void CommandReplayPlay([[maybe_unused]] const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("replay requires kbDebugInput build");
	}
	else
	{
		// Same semantics as F8 / kClientReplayPlaybackRequest: starts playback, or cancels if already replaying.
		if ((!engine::gpReplay->mReplayWriters.empty() || (game::gpGame->mGameFlags & engine::GameFlags::kSaveReplay)))
		{
			throw std::runtime_error("replay_play rejected: active or pending recording");
		}
		gpGame->mGameFlags.Set(engine::GameFlags::kLoadReplay);
		rResult["pending"] = true;
	}
}

static void CommandReplayTransferCapture([[maybe_unused]] const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("replay_transfer_capture requires kbDebugInput build");
	}
	else
	{
		engine::ReplayHarnessRigs::TransferCaptureSnapshot captureSnapshot = engine::ReplayHarnessRigs::CaptureSnapshot(*engine::gpReplay);
		rResult["firstWriterInputTick"] = captureSnapshot.iFirstWriterInputTick;
		rResult["writerInputCount"] = captureSnapshot.iWriterInputCount;

		nlohmann::json events = nlohmann::json::array();
		for (const engine::ReplayHarnessRigs::TransferCaptureEvent& rEvent : captureSnapshot.events)
		{
			nlohmann::json eventJson;
			eventJson["recordingEventTick"] = rEvent.iRecordingEventTick;
			eventJson["playbackEventTick"] = rEvent.iPlaybackEventTick;
			eventJson["omniCount"] = rEvent.transferCounts.iOmniCount;
			eventJson["spaceshipCount"] = rEvent.transferCounts.iSpaceshipCount;
			eventJson["blasterCount"] = rEvent.transferCounts.iBlasterCount;
			eventJson["missileCount"] = rEvent.transferCounts.iMissileCount;
			events.push_back(std::move(eventJson));
		}
		rResult["events"] = std::move(events);
	}
}

static void CommandReplayDropRetainedEndFrame([[maybe_unused]] const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("replay requires kbDebugInput build");
	}
	else
	{
		if (engine::gpReplay->mReplayWriters.empty())
		{
			throw std::runtime_error("replay_drop_retained_end_frame requires active recording");
		}

		engine::GridCoord coordinate = CoordinateFromParameter(rParameters);
		if (!engine::ReplayHarnessRigs::DropRetainedEndFrame(*engine::gpReplay, coordinate))
		{
			throw std::runtime_error("replay_drop_retained_end_frame requires a retained terminal frame for 'coord'");
		}

		rResult["coord"] = {coordinate.iX, coordinate.iY};
		rResult["dropped"] = true;
	}
}

static void CommandReplayInjectPersistenceFailure([[maybe_unused]] const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("replay requires kbDebugInput build");
	}
	else
	{
		if (!rParameters.contains("stage"))
		{
			throw std::runtime_error("replay_inject_persistence_failure requires string 'stage'");
		}
		if (!rParameters.at("stage").is_string())
		{
			throw std::runtime_error("replay_inject_persistence_failure requires string 'stage'");
		}

		std::string stage = rParameters.at("stage").get<std::string>();
		engine::ReplayHarnessRigs::PersistenceFailurePoint eFailurePoint = engine::ReplayHarnessRigs::PersistenceFailurePoint::kNone;
		if (stage == "invalidation")
		{
			eFailurePoint = engine::ReplayHarnessRigs::PersistenceFailurePoint::kManifestInvalidation;
		}
		else if (stage == "grid")
		{
			eFailurePoint = engine::ReplayHarnessRigs::PersistenceFailurePoint::kGrid;
		}
		else if (stage == "transfer_capture")
		{
			eFailurePoint = engine::ReplayHarnessRigs::PersistenceFailurePoint::kTransferCapture;
		}
		else if (stage == "coordinate_writer")
		{
			eFailurePoint = engine::ReplayHarnessRigs::PersistenceFailurePoint::kCoordinateWriter;
		}
		else if (stage == "fullframes_record")
		{
			eFailurePoint = engine::ReplayHarnessRigs::PersistenceFailurePoint::kFullFramesRecord;
		}
		else if (stage == "metadata")
		{
			eFailurePoint = engine::ReplayHarnessRigs::PersistenceFailurePoint::kMetadata;
		}
		else if (stage == "inventory")
		{
			eFailurePoint = engine::ReplayHarnessRigs::PersistenceFailurePoint::kInventory;
		}
		else if (stage == "final_manifest")
		{
			eFailurePoint = engine::ReplayHarnessRigs::PersistenceFailurePoint::kFinalManifest;
		}
		else
		{
			throw std::runtime_error("'stage' must be invalidation|grid|transfer_capture|coordinate_writer|fullframes_record|metadata|inventory|final_manifest");
		}

		if ((eFailurePoint == engine::ReplayHarnessRigs::PersistenceFailurePoint::kManifestInvalidation || eFailurePoint == engine::ReplayHarnessRigs::PersistenceFailurePoint::kGrid) == (!engine::gpReplay->mReplayWriters.empty()))
		{
			throw std::runtime_error((!engine::gpReplay->mReplayWriters.empty()) ? "selected stage requires recording to be inactive" : "selected stage requires active recording");
		}

		bool bRequiresCoordinate = eFailurePoint == engine::ReplayHarnessRigs::PersistenceFailurePoint::kCoordinateWriter
		                        || eFailurePoint == engine::ReplayHarnessRigs::PersistenceFailurePoint::kFullFramesRecord;
		if (bRequiresCoordinate != rParameters.contains("coord"))
		{
			throw std::runtime_error(bRequiresCoordinate ? "selected stage requires 'coord'" : "'coord' is only valid for coordinate_writer or fullframes_record");
		}

		engine::GridCoord coordinate {};
		if (bRequiresCoordinate)
		{
			coordinate = CoordinateFromParameter(rParameters);
		}
		if (!engine::ReplayHarnessRigs::ArmPersistenceFailure(*engine::gpReplay, eFailurePoint, coordinate))
		{
			throw std::runtime_error("selected 'coord' has no replay writer with an end frame");
		}

		rResult["stage"] = std::move(stage);
		if (bRequiresCoordinate)
		{
			rResult["coord"] = {coordinate.iX, coordinate.iY};
		}
		rResult["armed"] = true;
	}
}

static float FiniteFloatFromValue(const nlohmann::json& rValue, std::string_view name)
{
	if (!rValue.is_number())
	{
		throw std::runtime_error(std::format("'{}' must be a finite number", name));
	}
	float fValue = rValue.get<float>();
	if (!std::isfinite(fValue))
	{
		throw std::runtime_error(std::format("'{}' must be a finite number", name));
	}
	return fValue;
}

template <typename COLUMN, typename DATA>
static void ParseStatusChangeField(const nlohmann::json& rEntry, DATA& rData)
{
	static constexpr std::string_view kName = COLUMN::kName;
	if (!rEntry.contains(kName))
	{
		return;
	}

	using Member = std::remove_cvref_t<decltype(rData.*COLUMN::kpMember)>;
	[[maybe_unused]] Member& rMember = rData.*COLUMN::kpMember;
	const nlohmann::json& rValue = rEntry.at(std::string(kName));
	if constexpr (std::is_same_v<Member, bool>)
	{
		if (!rValue.is_boolean())
		{
			throw std::runtime_error(std::format("'{}' must be a boolean", kName));
		}
		rMember = rValue.get<bool>();
	}
	else if constexpr (std::is_same_v<Member, int64_t>)
	{
		if (!rValue.is_number_integer())
		{
			throw std::runtime_error(std::format("'{}' must be an integer", kName));
		}
		rMember = rValue.get<int64_t>();
	}
	else if constexpr (std::is_same_v<Member, uint8_t>)
	{
		if (!rValue.is_number_integer())
		{
			throw std::runtime_error(std::format("'{}' must be an integer in [0,255]", kName));
		}
		int64_t iValue = rValue.get<int64_t>();
		if (!std::in_range<uint8_t>(iValue))
		{
			throw std::runtime_error(std::format("'{}' must be an integer in [0,255]", kName));
		}
		rMember = static_cast<uint8_t>(iValue);
	}
	else if constexpr (std::is_same_v<Member, float>)
	{
		rMember = FiniteFloatFromValue(rValue, kName);
	}
	else if constexpr (std::is_same_v<Member, std::chrono::duration<float>>)
	{
		rMember = std::chrono::duration<float>(FiniteFloatFromValue(rValue, kName));
	}
	else if constexpr (std::is_same_v<Member, engine::GridCoord>)
	{
		rMember = CoordinateFromParameter(rEntry, kName);
	}
	else if constexpr (std::is_same_v<Member, XMVECTOR>)
	{
		if (!rValue.is_array())
		{
			throw std::runtime_error(std::format("'{}' must be an [x,y] array of finite numbers", kName));
		}
		if (rValue.size() != 2)
		{
			throw std::runtime_error(std::format("'{}' must be an [x,y] array of finite numbers", kName));
		}
		// The caller supplies local XY only; the position sits on the lowest flight plane, and the W lane follows the
		// position/direction invariant.
		static constexpr bool kbIsPosition = kName == "vecPosition";
		rMember = XMVectorSet(FiniteFloatFromValue(rValue.at(0), kName), FiniteFloatFromValue(rValue.at(1), kName), kbIsPosition ? engine::gpFlightPlanes->Lowest().fHeightMeters : 0.0f, kbIsPosition ? 1.0f : 0.0f);
	}
	else if constexpr (std::is_same_v<Member, engine::AlignmentIdentifier>)
	{
		if (rValue == "omni")
		{
			rMember = gpGame->mOmniAlignment;
		}
		else if (rValue == "enemy")
		{
			rMember = gpGame->mEnemyAlignment;
		}
		else
		{
			throw std::runtime_error(std::format("'{}' must be \"omni\" or \"enemy\"", kName));
		}
	}
	else if constexpr (engine::kbIsFlags<Member>)
	{
		using Enum = decltype(rMember.meFlags);
		using Underlying = std::underlying_type_t<Enum>;
		if (!rValue.is_number_integer())
		{
			throw std::runtime_error(std::format("'{}' must be an integer in [0,{}]", kName, std::numeric_limits<Underlying>::max()));
		}
		int64_t iValue = rValue.get<int64_t>();
		if (!std::in_range<Underlying>(iValue))
		{
			throw std::runtime_error(std::format("'{}' must be an integer in [0,{}]", kName, std::numeric_limits<Underlying>::max()));
		}
		rMember.meFlags = static_cast<Enum>(iValue);
	}
	else if constexpr (std::is_same_v<Member, engine::GlobalId>)
	{
		throw std::runtime_error(std::format("'{}' is minted by the server", kName));
	}
	else
	{
		static_assert(false, "inject_payload has no JSON parse for this payload member type");
	}
}

// The keys of an entry are the Fields() member tokens of its payload; every other key except coord, type, and tick is
// rejected, and a member whose key is absent keeps its current value.
template <typename DATA>
static void ParseStatusChangeFields(const nlohmann::json& rEntry, DATA& rData)
{
	[&]<typename... COLUMNS>(engine::FrameColumnList<COLUMNS...>)
	{
		for (const auto& [rKey, rValue] : rEntry.items())
		{
			if (rKey != "coord" && rKey != "type" && rKey != "tick" && ((rKey != COLUMNS::kName) && ...))
			{
				throw std::runtime_error(std::format("inject_payload entry has unknown field '{}' for its type", rKey));
			}
		}
		(ParseStatusChangeField<COLUMNS>(rEntry, rData), ...);
	}(DATA::Fields());
}

template <typename COLUMN>
static void RequireEntryKey(const nlohmann::json& rEntry)
{
	if (!rEntry.contains(COLUMN::kName))
	{
		throw std::runtime_error(std::format("'{}' required", COLUMN::kName));
	}
}

static StatusChangeType StatusChangeTypeFromEntry(const nlohmann::json& rEntry)
{
	if (!rEntry.contains("type"))
	{
		throw std::runtime_error("each entry requires string 'type'");
	}
	if (!rEntry.at("type").is_string())
	{
		throw std::runtime_error("each entry requires string 'type'");
	}
	std::string type = rEntry.at("type").get<std::string>();
	for (int64_t i = 0; i < static_cast<int64_t>(StatusChangeType::kCount); ++i)
	{
		StatusChangeType eType = static_cast<StatusChangeType>(i);
		// A transfer is a cell crossing only the simulation produces.
		if (!IsTransferType(eType) && type == StatusChangeTypeName(eType))
		{
			return eType;
		}
	}
	throw std::runtime_error("'type' must be SpawnOmni|SpawnSpaceship|SpawnBlaster|SpawnMissile|SpawnExplosion|DestroyOmni|UpdateOmni|UpdateFleet");
}

static void ValidateInjectedPosition(FXMVECTOR vecPosition)
{
	if (!common::InsideArea(vecPosition, engine::LocalCellArea()))
	{
		throw std::runtime_error("inject_payload position must lie inside the cell");
	}
}

// Seats the spawn defaults, parses the entry over them, and rejects an outside position, which QueueSpawn asserts on.
template <typename SPAWN_INFO>
static SPAWN_INFO& ParseSpawnEntry(const nlohmann::json& rEntry, StatusChange& rChange, [[maybe_unused]] engine::GridCoord coordinate)
{
	SPAWN_INFO& rData = std::get<SPAWN_INFO>(rChange.data);
	rData.vecPosition = XMVectorSet(0.0f, 0.0f, engine::gpFlightPlanes->Lowest().fHeightMeters, 1.0f);
	if constexpr (requires { rData.vecDirection; })
	{
		rData.vecDirection = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	}
	if constexpr (requires { rData.vecVelocity; })
	{
		rData.vecVelocity = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	}
	if constexpr (requires { rData.vecStoredDirection; })
	{
		rData.vecStoredDirection = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	}
	if constexpr (requires { rData.alignment; })
	{
		rData.alignment = gpGame->mOmniAlignment;
	}
	if constexpr (requires { rData.fleetWantedCoordinate; })
	{
		rData.fleetWantedCoordinate = coordinate;
	}
	ParseStatusChangeFields(rEntry, rData);
	ValidateInjectedPosition(rData.vecPosition);
	return rData;
}

// Validates one entry without touching any queue.
static std::pair<engine::GridCoord, ScheduledStatusChange> BuildInjectedEntry(const nlohmann::json& rEntry)
{
	if (!rEntry.is_object())
	{
		throw std::runtime_error("each inject_payload entry must be an object");
	}
	engine::GridCoord coordinate = CoordinateFromParameter(rEntry);
	StatusChangeType eType = StatusChangeTypeFromEntry(rEntry);

	ScheduledStatusChange scheduled {.iTick = gpGame->miTickCounter + 1, .change = {.eType = eType, .data = DefaultDataForType(eType)}};
	if (rEntry.contains("tick"))
	{
		if (!rEntry.at("tick").is_number_integer())
		{
			throw std::runtime_error("'tick' must be an integer");
		}
		scheduled.iTick = rEntry.at("tick").get<int64_t>();
		if (scheduled.iTick <= gpGame->miTickCounter)
		{
			throw std::runtime_error("'tick' must be greater than the current status.tick");
		}
	}

	if (!IsCoordinateActive(coordinate))
	{
		throw std::runtime_error("status-change entry 'coord' is not active");
	}

	switch (eType)
	{
		case StatusChangeType::kSpawnOmni:
		{
			const OmnisPostRender::SpawnInfo& rData = ParseSpawnEntry<OmnisPostRender::SpawnInfo>(rEntry, scheduled.change, coordinate);
			// An omni coasts under a transfer lock the entry may set. A velocity that crosses a whole cell in one tick
			// outruns the one-cell transfer delta and asserts at the destination's Spawn, so cap it at the speed
			// navigation never exceeds.
			if (!(XMVectorGetX(XMVector2LengthSq(rData.vecVelocity)) <= kfOmniMaximumSpeed * kfOmniMaximumSpeed))
			{
				throw std::runtime_error(std::format("SpawnOmni '{}' must not exceed the maximum omni speed", FRAME_COLUMN(OmnisPostRender::SpawnInfo, vecVelocity)::kName));
			}
			if (!OmnisPostRender::IsBlasterFireTimeInRange(rData.nextBlasterFireTime.count()))
			{
				throw std::runtime_error(std::format("'{}' is out of range", FRAME_COLUMN(OmnisPostRender::SpawnInfo, nextBlasterFireTime)::kName));
			}
			if (!OmnisPostRender::IsNavigationDelayInRange(rData.navigationDelay.count()))
			{
				throw std::runtime_error(std::format("'{}' must be within [0,60]", FRAME_COLUMN(OmnisPostRender::SpawnInfo, navigationDelay)::kName));
			}
			break;
		}
		case StatusChangeType::kSpawnSpaceship:
			ParseSpawnEntry<SpaceshipsPostRender::SpawnInfo>(rEntry, scheduled.change, coordinate);
			break;
		case StatusChangeType::kSpawnBlaster:
			ParseSpawnEntry<BlastersPostRender::SpawnInfo>(rEntry, scheduled.change, coordinate);
			if (!IsAdoptableStatusChange(scheduled.change))
			{
				throw std::runtime_error(std::format("'{}' is not a registered Blaster type", FRAME_COLUMN(BlastersPostRender::SpawnInfo, iTypeIndex)::kName));
			}
			break;
		case StatusChangeType::kSpawnMissile:
		{
			MissilesPostRender::SpawnInfo& rData = ParseSpawnEntry<MissilesPostRender::SpawnInfo>(rEntry, scheduled.change, coordinate);
			// Without explicit flags the missile homes on the other alignment.
			if (!rEntry.contains(FRAME_COLUMN(MissilesPostRender::SpawnInfo, flags)::kName))
			{
				rData.flags = (rData.alignment == gpGame->mOmniAlignment) ? MissileFlags::kTargetEnemy : MissileFlags::kTargetOmni;
			}
			break;
		}
		case StatusChangeType::kSpawnExplosion:
		{
			const engine::ExplosionsPostRender::SpawnInfo& rData = ParseSpawnEntry<engine::ExplosionsPostRender::SpawnInfo>(rEntry, scheduled.change, coordinate);
			if (!IsAdoptableStatusChange(scheduled.change))
			{
				throw std::runtime_error(std::format("'{}' is not a registered Explosion type", FRAME_COLUMN(engine::ExplosionsPostRender::SpawnInfo, iTypeIndex)::kName));
			}
			// Spawn caps only the trail count's upper end; the explosion read check of a later save or replay rejects a
			// negative count or a time percent outside [0,1].
			if (rData.iTrailCount < 0)
			{
				throw std::runtime_error(std::format("'{}' must not be negative", FRAME_COLUMN(engine::ExplosionsPostRender::SpawnInfo, iTrailCount)::kName));
			}
			if (!(rData.fTimePercent >= 0.0f && rData.fTimePercent <= 1.0f))
			{
				throw std::runtime_error(std::format("'{}' must be within [0,1]", FRAME_COLUMN(engine::ExplosionsPostRender::SpawnInfo, fTimePercent)::kName));
			}
			break;
		}
		case StatusChangeType::kDestroyOmni:
			RequireEntryKey<FRAME_COLUMN(DestroyOmniData, iOmniUuid)>(rEntry);
			ParseStatusChangeFields(rEntry, std::get<DestroyOmniData>(scheduled.change.data));
			break;
		case StatusChangeType::kUpdateOmni:
		{
			UpdateOmniData& rData = std::get<UpdateOmniData>(scheduled.change.data);
			rData.uiPendingWeaponModeTicks = static_cast<uint8_t>(engine::kiTickRate);
			RequireEntryKey<FRAME_COLUMN(UpdateOmniData, iOmniUuid)>(rEntry);
			ParseStatusChangeFields(rEntry, rData);
			// Rejects a delay the wire would reject, so an injected omni never holds an out-of-range delay.
			if (!OmnisPostRender::IsNavigationDelayInRange(rData.navigationDelaySeconds.count()))
			{
				throw std::runtime_error(std::format("'{}' must be within [0,60]", FRAME_COLUMN(UpdateOmniData, navigationDelaySeconds)::kName));
			}
			break;
		}
		default:
		{
			UpdateFleetData& rData = std::get<UpdateFleetData>(scheduled.change.data);
			rData.uiPendingFleetWantedCoordinateTicks = static_cast<uint8_t>(engine::kiTickRate);
			RequireEntryKey<FRAME_COLUMN(UpdateFleetData, iOmniUuid)>(rEntry);
			RequireEntryKey<FRAME_COLUMN(UpdateFleetData, fleetWantedCoordinate)>(rEntry);
			ParseStatusChangeFields(rEntry, rData);
			break;
		}
	}
	return {coordinate, std::move(scheduled)};
}

static void CommandInjectPayload(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	for (const auto& [rKey, rValue] : rParameters.items())
	{
		if (rKey != "entries" && rKey != "pauseAfterWriterInput")
		{
			throw std::runtime_error("inject_payload unknown parameter '" + rKey + "'");
		}
	}
	if (!rParameters.contains("entries"))
	{
		throw std::runtime_error("inject_payload requires non-empty array 'entries'");
	}
	if (!rParameters.at("entries").is_array())
	{
		throw std::runtime_error("inject_payload requires non-empty array 'entries'");
	}
	if (rParameters.at("entries").empty())
	{
		throw std::runtime_error("inject_payload requires non-empty array 'entries'");
	}

	bool bPauseAfterWriterInputPresent = rParameters.contains("pauseAfterWriterInput");
	bool bPauseAfterWriterInput = false;
	if (bPauseAfterWriterInputPresent)
	{
		if (!rParameters.at("pauseAfterWriterInput").is_boolean())
		{
			throw std::runtime_error("'pauseAfterWriterInput' must be bool");
		}
		if constexpr (!kbDebugInput)
		{
			throw std::runtime_error("'pauseAfterWriterInput' requires kbDebugInput build");
		}
		bPauseAfterWriterInput = rParameters.at("pauseAfterWriterInput").get<bool>();
	}

	// Replay playback loads recorded mFrameInputs through SyncReplayTick's LoadDifference; live injection is incompatible with simulation and broadcasts.
	if (gpGame->mbReplaying)
	{
		throw std::runtime_error("cannot inject during replay playback");
	}
	const nlohmann::json& rEntries = rParameters.at("entries");

	// Validate the whole batch before any queue mutation or global id is minted.
	std::vector<std::pair<engine::GridCoord, ScheduledStatusChange>> built;
	built.reserve(rEntries.size());
	bool bHasSpawn = false;
	bool bHasTick = false;
	for (const nlohmann::json& rEntry : rEntries)
	{
		const std::pair<engine::GridCoord, ScheduledStatusChange>& rBuiltEntry = built.emplace_back(BuildInjectedEntry(rEntry));
		bHasSpawn = bHasSpawn || IsSpawnType(rBuiltEntry.second.change.eType);
		bHasTick = bHasTick || rEntry.contains("tick");
	}
	bool bPendingStart = (gpGame->mGameFlags & engine::GameFlags::kPaused) && (gpGame->mGameFlags & engine::GameFlags::kSaveReplay)
	                  && engine::gpReplay->mReplayWriters.empty();
	if (bPauseAfterWriterInput)
	{
		if (!bHasSpawn)
		{
			throw std::runtime_error("pauseAfterWriterInput requires a spawn entry");
		}
		if (engine::gpReplay->mReplayWriters.empty() && !bPendingStart)
		{
			throw std::runtime_error("pauseAfterWriterInput requires active recording or a paused pending recording start");
		}
		if (engine::ReplayHarnessRigs::IsWriterPauseArmed(*engine::gpReplay))
		{
			throw std::runtime_error("pauseAfterWriterInput is already armed");
		}
		// The writer-input pause assumes the batch enters the next tick.
		if (bHasTick)
		{
			throw std::runtime_error("pauseAfterWriterInput cannot be combined with entry 'tick'");
		}
	}

	// Ids are reserved here and committed to miNextGlobalId only with the queue.
	int64_t iNextGlobalId = gpGame->miNextGlobalId;
	nlohmann::json globalIds = nlohmann::json::array();
	for (std::pair<engine::GridCoord, ScheduledStatusChange>& rBuiltEntry : built)
	{
		ScheduledStatusChange& rScheduled = rBuiltEntry.second;
		if (rScheduled.change.eType == StatusChangeType::kSpawnOmni)
		{
			int64_t iGlobalId = iNextGlobalId++;
			std::get<OmnisPostRender::SpawnInfo>(rScheduled.change.data).globalId = engine::GlobalId {.iValue = iGlobalId};
			globalIds.push_back(iGlobalId);
		}
	}
	if (bPauseAfterWriterInputPresent)
	{
		rResult["pauseAfterWriterInput"] = bPauseAfterWriterInput;
	}

	gpGame->miNextGlobalId = iNextGlobalId;
	for (const std::pair<engine::GridCoord, ScheduledStatusChange>& rBuiltEntry : built)
	{
		const auto& [rCoordinate, rScheduled] = rBuiltEntry;
		// A spawn is applied right after its target tick, with that tick's arrivals; any other entry enters its target
		// tick's input.
		if (IsSpawnType(rScheduled.change.eType))
		{
			gpServerSession->mpTransferManager->QueueSpawn(rCoordinate, rScheduled.iTick, rScheduled.change);
		}
		else
		{
			QueueAgentStatusChange(*gpServerSession, rCoordinate, rScheduled.iTick, rScheduled.change);
		}
	}
	if (bPauseAfterWriterInput)
	{
		engine::ReplayHarnessRigs::ArmPauseAfterNextWriterInput(*engine::gpReplay, bPendingStart);
	}
	rResult["injected"] = std::ssize(built);
	rResult["globalIds"] = std::move(globalIds);
	rResult["deferred"] = static_cast<bool>(gpGame->mGameFlags & engine::GameFlags::kPaused);
}

bool ExecuteServerSimulationHarnessRigCommand(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (command != "replay_record" && command != "replay_play" && command != "replay_transfer_capture" && command != "replay_drop_retained_end_frame" && command != "replay_inject_persistence_failure" && command != "inject_payload")
	{
		return false;
	}
	Bind(*gpServerSession);
	if (command == "replay_record")
	{
		CommandReplayRecord(rParameters, rResult);
		return true;
	}
	if (command == "replay_play")
	{
		CommandReplayPlay(rParameters, rResult);
		return true;
	}
	if (command == "replay_transfer_capture")
	{
		CommandReplayTransferCapture(rParameters, rResult);
		return true;
	}
	if (command == "replay_drop_retained_end_frame")
	{
		CommandReplayDropRetainedEndFrame(rParameters, rResult);
		return true;
	}
	if (command == "replay_inject_persistence_failure")
	{
		CommandReplayInjectPersistenceFailure(rParameters, rResult);
		return true;
	}
	if (command == "inject_payload")
	{
		CommandInjectPayload(rParameters, rResult);
		return true;
	}
	return false;
}

void QueueAgentStatusChange(const ServerSession& rSession, engine::GridCoord coordinate, int64_t iTick, const StatusChange& rChange)
{
	// Runs at the agent command drain point under AgentCommandServer::Drain's allocation suppression.
	Bind(rSession);
	sHarnessRig.pendingAgentStatusChanges.try_emplace(coordinate).first->second.push_back({.iTick = iTick, .change = rChange});
}

void DrainPendingAgentStatusChanges(const ServerSession& rSession)
{
	if (sHarnessRig.pSession != &rSession)
	{
		return;
	}

	int64_t iTick = gpGame->miTickCounter;
	auto IsReleased = [iTick](const ScheduledStatusChange& rScheduled)
	{
		return rScheduled.iTick <= iTick;
	};
	for (auto it = sHarnessRig.pendingAgentStatusChanges.begin(); it != sHarnessRig.pendingAgentStatusChanges.end();)
	{
		const engine::GridCoord& rCoordinate = it->first;
		if (!IsCoordinateActive(rCoordinate))
		{
			++it;
			continue;
		}
		std::vector<ScheduledStatusChange>& rScheduledChanges = it->second;
		if (std::ranges::none_of(rScheduledChanges, IsReleased))
		{
			++it;
			continue;
		}
		std::vector<StatusChange>& rStatusChanges = gpGame->mFrameInputs.try_emplace(rCoordinate).first->second.statusChanges;
		for (const ScheduledStatusChange& rScheduled : rScheduledChanges)
		{
			if (IsReleased(rScheduled))
			{
				rStatusChanges.push_back(rScheduled.change);
			}
		}
		std::stable_sort(rStatusChanges.begin(), rStatusChanges.end(), [](const StatusChange& rLeft, const StatusChange& rRight)
		{
			return rLeft.eType < rRight.eType;
		});
		std::erase_if(rScheduledChanges, IsReleased);
		it = rScheduledChanges.empty() ? sHarnessRig.pendingAgentStatusChanges.erase(it) : std::next(it);
	}
}

void ResetPendingAgentStatusChanges(const ServerSession& rSession)
{
	if (sHarnessRig.pSession == &rSession)
	{
		sHarnessRig.pendingAgentStatusChanges.clear();
	}
}

int64_t CountPendingAgentStatusChanges(const ServerSession& rSession)
{
	if (sHarnessRig.pSession != &rSession)
	{
		return 0;
	}

	int64_t iCount = 0;
	for (const auto& [rCoordinate, rChanges] : sHarnessRig.pendingAgentStatusChanges)
	{
		iCount += std::ssize(rChanges);
	}
	return iCount;
}

void DetachServerSimulationHarnessRigs(const ServerSession& rSession)
{
	if (sHarnessRig.pSession == &rSession)
	{
		sHarnessRig = {};
	}
}

void CountCapturedReplayTransfers(std::span<const StatusChange> transfers, ReplayTransferCaptureCounts& rCounts)
{
	for (const StatusChange& rTransfer : transfers)
	{
		switch (rTransfer.eType)
		{
			case StatusChangeType::kSpawnOmni:
			case StatusChangeType::kTransferOmni: ++rCounts.iOmniCount; break;
			case StatusChangeType::kSpawnSpaceship:
			case StatusChangeType::kTransferSpaceship: ++rCounts.iSpaceshipCount; break;
			case StatusChangeType::kSpawnBlaster:
			case StatusChangeType::kTransferBlaster: ++rCounts.iBlasterCount; break;
			case StatusChangeType::kSpawnMissile:
			case StatusChangeType::kTransferMissile: ++rCounts.iMissileCount; break;
			// The capture counts have no explosion field.
			case StatusChangeType::kSpawnExplosion: break;
			default: DEBUG_BREAK(); break;
		}
	}
}

} // namespace game

#endif // BT_SERVER
