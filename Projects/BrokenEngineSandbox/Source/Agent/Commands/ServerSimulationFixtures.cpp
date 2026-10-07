#include "Pch.h"

#include "Agent/Commands/ServerSimulationFixtures.h"

#if defined(BT_SERVER)

#include "Agent/Commands/ReplayFixtures.h"
#include "File/Replay.h"
#include "Network/Server/ServerTransferManager.h"
#include "Ui/WrapperBase.h"

#include "Agent/AgentCommandsServerQueries.h"
#include "Frame/Collections/Players/Players.h"
#include "Network/Server/ServerSession.h"
#include "Profile/ProfileManager.h"
#include "Game.h"
#include "SpawnTransfer.h"

namespace game
{

// iTick is the tick the change is released on, or the first later tick its coordinate can take it.
struct ScheduledStatusChange
{
	int64_t iTick = 0;
	StatusChange change;
};

struct ServerSimulationFixtureState
{
	const ServerSession* pSession = nullptr;
	std::unordered_map<engine::GridCoord, std::vector<ScheduledStatusChange>> pendingAgentStatusChanges;
	std::unordered_map<engine::GridCoord, std::vector<ScheduledStatusChange>> replayTransferFixtures;
};

static ServerSimulationFixtureState sFixture;

static void Bind(const ServerSession& rSession)
{
	if (sFixture.pSession != &rSession)
	{
		sFixture = {};
		sFixture.pSession = &rSession;
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
		// Paused ticks skip SyncReplayTick, which consumes kSaveReplay as a start/stop toggle.
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
				sFixture.replayTransferFixtures.clear();
				engine::ReplayFixtures::Reset(*engine::gpReplay);
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
		engine::ReplayFixtures::TransferCaptureSnapshot captureSnapshot = engine::ReplayFixtures::CaptureSnapshot(*engine::gpReplay);
		rResult["firstWriterInputTick"] = captureSnapshot.iFirstWriterInputTick;
		rResult["writerInputCount"] = captureSnapshot.iWriterInputCount;

		nlohmann::json events = nlohmann::json::array();
		for (const engine::ReplayFixtures::TransferCaptureEvent& rEvent : captureSnapshot.events)
		{
			nlohmann::json eventJson;
			eventJson["recordingEventTick"] = rEvent.iRecordingEventTick;
			eventJson["playbackEventTick"] = rEvent.iPlaybackEventTick;
			eventJson["playerCount"] = rEvent.transferCounts.iPlayerCount;
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
		if (!engine::ReplayFixtures::DropRetainedEndFrame(*engine::gpReplay, coordinate))
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
		engine::ReplayFixtures::PersistenceFailurePoint eFailurePoint = engine::ReplayFixtures::PersistenceFailurePoint::kNone;
		if (stage == "invalidation")
		{
			eFailurePoint = engine::ReplayFixtures::PersistenceFailurePoint::kManifestInvalidation;
		}
		else if (stage == "grid")
		{
			eFailurePoint = engine::ReplayFixtures::PersistenceFailurePoint::kGrid;
		}
		else if (stage == "transfer_capture")
		{
			eFailurePoint = engine::ReplayFixtures::PersistenceFailurePoint::kTransferCapture;
		}
		else if (stage == "coordinate_writer")
		{
			eFailurePoint = engine::ReplayFixtures::PersistenceFailurePoint::kCoordinateWriter;
		}
		else if (stage == "fullframes_record")
		{
			eFailurePoint = engine::ReplayFixtures::PersistenceFailurePoint::kFullFramesRecord;
		}
		else if (stage == "metadata")
		{
			eFailurePoint = engine::ReplayFixtures::PersistenceFailurePoint::kMetadata;
		}
		else if (stage == "inventory")
		{
			eFailurePoint = engine::ReplayFixtures::PersistenceFailurePoint::kInventory;
		}
		else if (stage == "final_manifest")
		{
			eFailurePoint = engine::ReplayFixtures::PersistenceFailurePoint::kFinalManifest;
		}
		else
		{
			throw std::runtime_error("'stage' must be invalidation|grid|transfer_capture|coordinate_writer|fullframes_record|metadata|inventory|final_manifest");
		}

		if ((eFailurePoint == engine::ReplayFixtures::PersistenceFailurePoint::kManifestInvalidation || eFailurePoint == engine::ReplayFixtures::PersistenceFailurePoint::kGrid) == (!engine::gpReplay->mReplayWriters.empty()))
		{
			throw std::runtime_error((!engine::gpReplay->mReplayWriters.empty()) ? "selected stage requires recording to be inactive" : "selected stage requires active recording");
		}

		bool bRequiresCoordinate = eFailurePoint == engine::ReplayFixtures::PersistenceFailurePoint::kCoordinateWriter
		                        || eFailurePoint == engine::ReplayFixtures::PersistenceFailurePoint::kFullFramesRecord;
		if (bRequiresCoordinate != rParameters.contains("coord"))
		{
			throw std::runtime_error(bRequiresCoordinate ? "selected stage requires 'coord'" : "'coord' is only valid for coordinate_writer or fullframes_record");
		}

		engine::GridCoord coordinate {};
		if (bRequiresCoordinate)
		{
			coordinate = CoordinateFromParameter(rParameters);
		}
		if (!engine::ReplayFixtures::ArmPersistenceFailure(*engine::gpReplay, eFailurePoint, coordinate))
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

template <auto MEMBER, common::FixedString NAME, bool REQUIRED = false>
struct InjectedField
{
	static constexpr auto kpMember = MEMBER;
	static constexpr bool kbIsRequired = REQUIRED;
	static constexpr std::string_view kName = NAME.data;
};

// The JSON key is the payload member's own token.
#define INJECTED_FIELD(OWNER, MEMBER, ...) InjectedField<&OWNER::MEMBER, #MEMBER __VA_OPT__(,) __VA_ARGS__>

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

template <typename FIELD, typename DATA_TYPE>
static void ParseInjectedField(const nlohmann::json& rEntry, DATA_TYPE& rData)
{
	static constexpr std::string_view kName = FIELD::kName;
	bool bPresent = rEntry.contains(kName);
	if (!bPresent && FIELD::kbIsRequired)
	{
		throw std::runtime_error(std::format("'{}' required", kName));
	}
	if (!bPresent)
	{
		return;
	}

	using Member = std::remove_cvref_t<decltype(rData.*FIELD::kpMember)>;
	Member& rMember = rData.*FIELD::kpMember;
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
		// The caller supplies local XY only; the W lane follows the position/direction invariant.
		static constexpr bool kbIsPosition = FIELD::kpMember == &TransferData::vecPosition;
		rMember = XMVectorSet(FiniteFloatFromValue(rValue.at(0), kName), FiniteFloatFromValue(rValue.at(1), kName), kbIsPosition ? engine::gBaseHeight.mfCurrent : 0.0f, kbIsPosition ? 1.0f : 0.0f);
	}
	else
	{
		static_assert(false, "inject_payload has no JSON parse for this payload member type");
	}
}

// FIELDS lists the payload members a kind accepts; every other entry key except coord, type, and tick is rejected.
template <typename... FIELDS, typename DATA_TYPE>
static void ParseInjectedFields(const nlohmann::json& rEntry, DATA_TYPE& rData)
{
	for (const auto& [rKey, rValue] : rEntry.items())
	{
		if (rKey != "coord" && rKey != "type" && rKey != "tick" && ((rKey != FIELDS::kName) && ...))
		{
			throw std::runtime_error(std::format("inject_payload entry has unknown field '{}' for its type", rKey));
		}
	}
	(ParseInjectedField<FIELDS>(rEntry, rData), ...);
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
		if (type == StatusChangeTypeName(static_cast<StatusChangeType>(i)))
		{
			return static_cast<StatusChangeType>(i);
		}
	}
	throw std::runtime_error("'type' must be SpawnPlayer|TransferPlayer|TransferSpaceship|TransferBlaster|TransferMissile|DestroyPlayer|UpdatePlayer|UpdateFleet");
}

static void ValidateInjectedPosition(FXMVECTOR vecPosition)
{
	if (!common::InsideArea(vecPosition, engine::LocalCellArea()))
	{
		throw std::runtime_error("inject_payload position must lie inside the cell");
	}
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

	if (IsTransferType(eType))
	{
		if constexpr (!kbDebugInput)
		{
			throw std::runtime_error("transfer entries require kbDebugInput build");
		}
		if (eType != StatusChangeType::kTransferPlayer && !gpServerSession->mpTransferManager->IsDestinationLive(coordinate))
		{
			throw std::runtime_error("transfer entry 'coord' is not live for this type");
		}

		TransferData& rData = std::get<TransferData>(scheduled.change.data);
		// Transfer payloads are destination-local, so the default arrival point is the cell centre.
		rData.vecPosition = XMVectorSet(0.0f, 0.0f, engine::gBaseHeight.mfCurrent, 1.0f);
		rData.vecDirection = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		rData.vecVelocity = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		rData.alignment = gpGame->mPlayerAlignment;
		rData.fHealth = 1.0f;
		rData.fShield = 1.0f;
		rData.uiTypeIndex = static_cast<uint8_t>(PlayersInterpolate::siBlasterTypeIndex);
		rData.fDeltaRotationMaximum = eType == StatusChangeType::kTransferMissile ? 2.0f : 0.0f;
		rData.fleetWantedCoordinate = coordinate;
		switch (eType)
		{
			case StatusChangeType::kTransferPlayer:
				ParseInjectedFields<
					INJECTED_FIELD(TransferData, vecPosition),
					INJECTED_FIELD(TransferData, vecDirection),
					INJECTED_FIELD(TransferData, vecVelocity),
					INJECTED_FIELD(TransferData, fHealth),
					INJECTED_FIELD(TransferData, fShield),
					INJECTED_FIELD(TransferData, nextBlasterFireTimeSeconds),
					INJECTED_FIELD(TransferData, nextSecondarySpawnTimeSeconds),
					INJECTED_FIELD(TransferData, shieldCooldownSeconds),
					INJECTED_FIELD(TransferData, shieldDownSoundCooldownSeconds),
					INJECTED_FIELD(TransferData, animationTimeSeconds),
					INJECTED_FIELD(TransferData, navigationDelaySeconds),
					INJECTED_FIELD(TransferData, fleetWantedCoordinate)>(rEntry, rData);
				break;
			case StatusChangeType::kTransferSpaceship:
				ParseInjectedFields<
					INJECTED_FIELD(TransferData, vecPosition),
					INJECTED_FIELD(TransferData, vecDirection),
					INJECTED_FIELD(TransferData, vecVelocity),
					INJECTED_FIELD(TransferData, fHealth),
					INJECTED_FIELD(TransferData, nextBlasterSpawnTimeSeconds),
					INJECTED_FIELD(TransferData, fDeltaRotation)>(rEntry, rData);
				break;
			case StatusChangeType::kTransferBlaster:
				ParseInjectedFields<
					INJECTED_FIELD(TransferData, vecPosition),
					INJECTED_FIELD(TransferData, vecVelocity),
					INJECTED_FIELD(TransferData, uiTypeIndex)>(rEntry, rData);
				break;
			default:
				ParseInjectedFields<
					INJECTED_FIELD(TransferData, vecPosition),
					INJECTED_FIELD(TransferData, vecDirection),
					INJECTED_FIELD(TransferData, vecVelocity),
					INJECTED_FIELD(TransferData, fAcceleration),
					INJECTED_FIELD(TransferData, deltaRotationDelaySeconds),
					INJECTED_FIELD(TransferData, timeSeconds),
					INJECTED_FIELD(TransferData, nextJitterSeconds),
					INJECTED_FIELD(TransferData, fDeltaRotation),
					INJECTED_FIELD(TransferData, fDeltaRotationMaximum),
					INJECTED_FIELD(TransferData, fPitch)>(rEntry, rData);
				break;
		}

		// An out-of-cell arrival asserts at PlayersPostRender::Spawn and is silently dropped by the other transfer types'
		// Spawn, so reject it here.
		ValidateInjectedPosition(rData.vecPosition);
		// An arriving player coasts under its transfer lock. A velocity that crosses a whole cell in one tick outruns the
		// one-cell transfer delta and asserts at Spawn, so cap it at the speed navigation never exceeds.
		if (eType == StatusChangeType::kTransferPlayer && !(XMVectorGetX(XMVector2LengthSq(rData.vecVelocity)) <= kfPlayerMaximumSpeed * kfPlayerMaximumSpeed))
		{
			throw std::runtime_error(std::format("TransferPlayer '{}' must not exceed the maximum player speed", INJECTED_FIELD(TransferData, vecVelocity)::kName));
		}
		if (!PlayersPostRender::IsBlasterFireTimeInRange(rData.nextBlasterFireTimeSeconds.count()))
		{
			throw std::runtime_error(std::format("'{}' is out of range", INJECTED_FIELD(TransferData, nextBlasterFireTimeSeconds)::kName));
		}
		if (!PlayersPostRender::IsNavigationDelayInRange(rData.navigationDelaySeconds.count()))
		{
			throw std::runtime_error(std::format("'{}' must be within [0,60]", INJECTED_FIELD(TransferData, navigationDelaySeconds)::kName));
		}
		if (!IsAdoptableStatusChange(scheduled.change))
		{
			throw std::runtime_error(std::format("'{}' is not a registered Blaster type", INJECTED_FIELD(TransferData, uiTypeIndex)::kName));
		}
	}
	else
	{
		if (!IsCoordinateActive(coordinate))
		{
			throw std::runtime_error("status-change entry 'coord' is not active");
		}

		switch (eType)
		{
			case StatusChangeType::kSpawnPlayer:
			{
				SpawnPlayerData& rData = std::get<SpawnPlayerData>(scheduled.change.data);
				rData.fleetWantedCoordinate = coordinate;
				ParseInjectedFields<
					INJECTED_FIELD(SpawnPlayerData, fSpawnOffsetX),
					INJECTED_FIELD(SpawnPlayerData, fSpawnOffsetY),
					INJECTED_FIELD(SpawnPlayerData, bIsFlagship),
					INJECTED_FIELD(SpawnPlayerData, fleetWantedCoordinate)>(rEntry, rData);
				// Spawn silently skips an out-of-cell offset, so reject it here.
				ValidateInjectedPosition(XMVectorSet(rData.fSpawnOffsetX, rData.fSpawnOffsetY, engine::gBaseHeight.mfCurrent, 1.0f));
				break;
			}
			case StatusChangeType::kDestroyPlayer:
				ParseInjectedFields<INJECTED_FIELD(DestroyPlayerData, iPlayerUuid, true)>(rEntry, std::get<DestroyPlayerData>(scheduled.change.data));
				break;
			case StatusChangeType::kUpdatePlayer:
			{
				UpdatePlayerData& rData = std::get<UpdatePlayerData>(scheduled.change.data);
				rData.uiPendingWeaponModeTicks = static_cast<uint8_t>(engine::kiTickRate);
				ParseInjectedFields<
					INJECTED_FIELD(UpdatePlayerData, iPlayerUuid, true),
					INJECTED_FIELD(UpdatePlayerData, bUseMissiles),
					INJECTED_FIELD(UpdatePlayerData, navigationDelaySeconds)>(rEntry, rData);
				// Rejects a delay the wire would reject, so an injected player never holds an out-of-range delay.
				if (!PlayersPostRender::IsNavigationDelayInRange(rData.navigationDelaySeconds.count()))
				{
					throw std::runtime_error(std::format("'{}' must be within [0,60]", INJECTED_FIELD(UpdatePlayerData, navigationDelaySeconds)::kName));
				}
				break;
			}
			default:
			{
				UpdateFleetData& rData = std::get<UpdateFleetData>(scheduled.change.data);
				rData.uiPendingFleetWantedCoordinateTicks = static_cast<uint8_t>(engine::kiTickRate);
				ParseInjectedFields<
					INJECTED_FIELD(UpdateFleetData, iPlayerUuid, true),
					INJECTED_FIELD(UpdateFleetData, fleetWantedCoordinate, true),
					INJECTED_FIELD(UpdateFleetData, bIsFlagship)>(rEntry, rData);
				break;
			}
		}
	}
	return {coordinate, std::move(scheduled)};
}

static void CommandInjectPayload(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!rParameters.is_object())
	{
		throw std::runtime_error("inject_payload params must be an object");
	}
	for (const auto& [rKey, rValue] : rParameters.items())
	{
		if (rKey != "entries" && rKey != "navQueryActivation" && rKey != "pauseAfterWriterInput")
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

	bool bArmNavigationQuery = false;
	if (rParameters.contains("navQueryActivation"))
	{
		const nlohmann::json& rActivation = rParameters.at("navQueryActivation");
		if (!rActivation.is_object())
		{
			throw std::runtime_error("'navQueryActivation' must be an object");
		}
		for (const auto& [rKey, rValue] : rActivation.items())
		{
			if (rKey != "arm")
			{
				throw std::runtime_error("navQueryActivation unknown parameter '" + rKey + "'");
			}
		}
		if (!rActivation.contains("arm"))
		{
			throw std::runtime_error("navQueryActivation requires boolean 'arm': true");
		}
		if (!rActivation.at("arm").is_boolean())
		{
			throw std::runtime_error("navQueryActivation requires boolean 'arm': true");
		}
		if (!rActivation.at("arm").get<bool>())
		{
			throw std::runtime_error("navQueryActivation requires boolean 'arm': true");
		}
		bArmNavigationQuery = true;
	}

	// Replay playback loads recorded mFrameInputs through SyncReplayTick's LoadDifference; live injection is incompatible with simulation and broadcasts.
	if (gpGame->mbReplaying)
	{
		throw std::runtime_error("cannot inject during replay playback");
	}
	if (bArmNavigationQuery)
	{
		if constexpr (!kbProfiling)
		{
			throw std::runtime_error("navQueryActivation requires a profiling server");
		}
		else if (gpGame->mGameFlags & engine::GameFlags::kPaused)
		{
			throw std::runtime_error("navQueryActivation requires an unpaused normal server state");
		}
		else if (gpGame->mGameFlags & engine::GameFlags::kSaveReplay)
		{
			throw std::runtime_error("navQueryActivation requires an unpaused normal server state");
		}
		else if (gpGame->mGameFlags & engine::GameFlags::kLoadReplay)
		{
			throw std::runtime_error("navQueryActivation requires an unpaused normal server state");
		}
		else if (gpGame->mTimeStep.miTimeMultiply != 1)
		{
			throw std::runtime_error("navQueryActivation requires timescale 1/1");
		}
		else if (gpGame->mTimeStep.miTimeDivide != 1)
		{
			throw std::runtime_error("navQueryActivation requires timescale 1/1");
		}
		else if ((!engine::gpReplay->mReplayWriters.empty()))
		{
			throw std::runtime_error("navQueryActivation requires recording and replay to be inactive");
		}
		else if (gpGame->mbReplaying)
		{
			throw std::runtime_error("navQueryActivation requires recording and replay to be inactive");
		}
	}
	const nlohmann::json& rEntries = rParameters.at("entries");

	// Validate the whole batch before any queue mutation or global id is minted.
	std::vector<std::pair<engine::GridCoord, ScheduledStatusChange>> built;
	built.reserve(rEntries.size());
	bool bHasTransfer = false;
	bool bHasTick = false;
	for (const nlohmann::json& rEntry : rEntries)
	{
		const std::pair<engine::GridCoord, ScheduledStatusChange>& rBuiltEntry = built.emplace_back(BuildInjectedEntry(rEntry));
		bHasTransfer = bHasTransfer || IsTransferType(rBuiltEntry.second.change.eType);
		bHasTick = bHasTick || rEntry.contains("tick");
	}
	// The arm's prepared queue swap covers only the status queue.
	if (bArmNavigationQuery && bHasTransfer)
	{
		throw std::runtime_error("navQueryActivation cannot be combined with transfer entries");
	}
	// The arm's sample floor and the writer-input pause both assume the batch enters the next tick.
	if (bArmNavigationQuery && bHasTick)
	{
		throw std::runtime_error("navQueryActivation cannot be combined with entry 'tick'");
	}
	bool bPendingStart = (gpGame->mGameFlags & engine::GameFlags::kPaused) && (gpGame->mGameFlags & engine::GameFlags::kSaveReplay)
	                  && engine::gpReplay->mReplayWriters.empty();
	if (bPauseAfterWriterInput)
	{
		if (!bHasTransfer)
		{
			throw std::runtime_error("pauseAfterWriterInput requires a transfer entry");
		}
		if (engine::gpReplay->mReplayWriters.empty() && !bPendingStart)
		{
			throw std::runtime_error("pauseAfterWriterInput requires active recording or a paused pending recording start");
		}
		if (engine::ReplayFixtures::IsWriterPauseArmed(*engine::gpReplay))
		{
			throw std::runtime_error("pauseAfterWriterInput is already armed");
		}
		if (bHasTick)
		{
			throw std::runtime_error("pauseAfterWriterInput cannot be combined with entry 'tick'");
		}
	}

	// Ids are reserved here and committed to miNextGlobalId only with the queue, so a rejected arm consumes none.
	int64_t iNextGlobalId = gpGame->miNextGlobalId;
	nlohmann::json globalIds = nlohmann::json::array();
	for (std::pair<engine::GridCoord, ScheduledStatusChange>& rBuiltEntry : built)
	{
		ScheduledStatusChange& rScheduled = rBuiltEntry.second;
		if (rScheduled.change.eType == StatusChangeType::kSpawnPlayer)
		{
			int64_t iGlobalId = iNextGlobalId++;
			std::get<SpawnPlayerData>(rScheduled.change.data).iGlobalId = iGlobalId;
			globalIds.push_back(iGlobalId);
		}
		else if (rScheduled.change.eType == StatusChangeType::kTransferPlayer)
		{
			int64_t iGlobalId = iNextGlobalId++;
			std::get<TransferData>(rScheduled.change.data).globalPlayerId = engine::GlobalId {.iValue = iGlobalId};
			globalIds.push_back(iGlobalId);
		}
	}
	if (bPauseAfterWriterInputPresent)
	{
		rResult["pauseAfterWriterInput"] = bPauseAfterWriterInput;
	}

	int64_t iQueuedAtTick = 0;
	int64_t iMinimumSampleTick = 0;
	if (bArmNavigationQuery)
	{
		if constexpr (kbProfiling)
		{
			// Prepare a complete queue copy before touching the profiler state. A failed allocation leaves both the
			// existing queue and the event arm unchanged.
			decltype(sFixture.pendingAgentStatusChanges) preparedPendingAgentStatusChanges = sFixture.pendingAgentStatusChanges;
			for (const std::pair<engine::GridCoord, ScheduledStatusChange>& rBuiltEntry : built)
			{
				preparedPendingAgentStatusChanges.try_emplace(rBuiltEntry.first).first->second.push_back(rBuiltEntry.second);
			}

			// Prepare every response field that could allocate before arming. The command failure path must not report an
			// error after committing the event and queue transaction.
			rResult["injected"] = std::ssize(built);
			rResult["globalIds"] = std::move(globalIds);
			rResult["deferred"] = static_cast<bool>(gpGame->mGameFlags & engine::GameFlags::kPaused);

			// Drain is the main-thread serialization point. Keep the event arm and queue commit in one critical section
			// so another tick cannot move the floor or observe a partially committed transaction.
			std::lock_guard lock(gpProfileManager->mCpuTimerMutex);
			iQueuedAtTick = gpGame->miTickCounter;
			static constexpr int64_t kiMinimumSampleTickOffset = engine::kiTickRate + 1;
			if (iQueuedAtTick > std::numeric_limits<int64_t>::max() - kiMinimumSampleTickOffset)
			{
				throw std::runtime_error("navQueryActivation tick range exhausted");
			}
			iMinimumSampleTick = iQueuedAtTick + kiMinimumSampleTickOffset;
			rResult["navQueryActivation"] =
			{
				{"armed", true},
				{"queuedAtTick", iQueuedAtTick},
				{"minimumSampleTick", iMinimumSampleTick},
			};
			if (!gpProfileManager->ArmRawCpuTimerEventLocked(game::kCpuTimerPostRenderUpdateNavigationQuery, iMinimumSampleTick))
			{
				throw std::runtime_error("navQueryActivation event is occupied or overrun");
			}
			gpGame->miNextGlobalId = iNextGlobalId;
			static_assert(noexcept(preparedPendingAgentStatusChanges.swap(sFixture.pendingAgentStatusChanges)));
			preparedPendingAgentStatusChanges.swap(sFixture.pendingAgentStatusChanges);
		}
	}
	else
	{
		gpGame->miNextGlobalId = iNextGlobalId;
		for (const std::pair<engine::GridCoord, ScheduledStatusChange>& rBuiltEntry : built)
		{
			const auto& [rCoordinate, rScheduled] = rBuiltEntry;
			if (IsTransferType(rScheduled.change.eType))
			{
				VERIFY_SUCCESS(QueueReplayTransferFixture(*gpServerSession, rCoordinate, rScheduled.iTick, rScheduled.change));
			}
			else
			{
				QueueAgentStatusChange(*gpServerSession, rCoordinate, rScheduled.iTick, rScheduled.change);
			}
		}
		if (bPauseAfterWriterInput)
		{
			engine::ReplayFixtures::ArmPauseAfterNextWriterInput(*engine::gpReplay, bPendingStart);
		}
		rResult["injected"] = std::ssize(built);
		rResult["globalIds"] = std::move(globalIds);
		rResult["deferred"] = static_cast<bool>(gpGame->mGameFlags & engine::GameFlags::kPaused);
	}
}

bool ExecuteServerSimulationFixtureCommand(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult)
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
	sFixture.pendingAgentStatusChanges.try_emplace(coordinate).first->second.push_back({.iTick = iTick, .change = rChange});
}

bool QueueReplayTransferFixture(const ServerSession& rSession, engine::GridCoord destination, int64_t iTick, StatusChange transfer)
{
	if constexpr (!kbDebugInput)
	{
		return false;
	}

	if (!IsTransferType(transfer.eType))
	{
		return false;
	}
	if (!std::holds_alternative<TransferData>(transfer.data))
	{
		return false;
	}

	Bind(rSession);
	sFixture.replayTransferFixtures.try_emplace(destination).first->second.push_back({.iTick = iTick, .change = std::move(transfer)});
	return true;
}

void DrainPendingAgentStatusChanges(const ServerSession& rSession)
{
	if (sFixture.pSession != &rSession)
	{
		return;
	}

	int64_t iTick = gpGame->miTickCounter;
	auto IsReleased = [iTick](const ScheduledStatusChange& rScheduled)
	{
		return rScheduled.iTick <= iTick;
	};
	for (auto it = sFixture.pendingAgentStatusChanges.begin(); it != sFixture.pendingAgentStatusChanges.end();)
	{
		const engine::GridCoord& rCoordinate = it->first;
		auto cellIt = gpGame->mCells.find(rCoordinate);
		bool bActive = std::ranges::contains(gpGame->mActiveCoordinates, rCoordinate);
		if (!bActive)
		{
			++it;
			continue;
		}
		if (cellIt == gpGame->mCells.end())
		{
			++it;
			continue;
		}
		if (cellIt->second.pCurrent == nullptr)
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
		it = rScheduledChanges.empty() ? sFixture.pendingAgentStatusChanges.erase(it) : std::next(it);
	}
}

void DrainReplayTransferFixtures(const ServerSession& rSession, engine::ServerTransferManager& rTransferManager)
{
	if (sFixture.pSession != &rSession)
	{
		return;
	}

	int64_t iTick = gpGame->miTickCounter;
	auto IsReleased = [iTick](const ScheduledStatusChange& rScheduled)
	{
		return rScheduled.iTick <= iTick;
	};
	for (auto it = sFixture.replayTransferFixtures.begin(); it != sFixture.replayTransferFixtures.end();)
	{
		const engine::GridCoord& rCoordinate = it->first;
		std::vector<ScheduledStatusChange>& rScheduledTransfers = it->second;
		for (const ScheduledStatusChange& rScheduled : rScheduledTransfers)
		{
			if (!IsReleased(rScheduled))
			{
				continue;
			}
			// Mirrors CollectTransfers' organic drop: ApplyPreparedTransfers breaks on a non-player transfer to a non-live destination.
			if (rScheduled.change.eType != StatusChangeType::kTransferPlayer && !rTransferManager.IsDestinationLive(rCoordinate))
			{
				LOG(kNetwork, kWarning, "Dropping injected transfer to non-live Frame Tick: {} Dest: ({},{}) Type: {}", iTick, rCoordinate.iX, rCoordinate.iY, StatusChangeTypeName(rScheduled.change.eType));
				continue;
			}
			// One call per entry: PrepareReplayTransfers appends, and a call with no entry would still create the destination.
			rTransferManager.PrepareReplayTransfers(rCoordinate, std::span(&rScheduled.change, 1));
		}
		std::erase_if(rScheduledTransfers, IsReleased);
		it = rScheduledTransfers.empty() ? sFixture.replayTransferFixtures.erase(it) : std::next(it);
	}
}

void ResetPendingAgentStatusChanges(const ServerSession& rSession)
{
	if (sFixture.pSession == &rSession)
	{
		sFixture.pendingAgentStatusChanges.clear();
	}
}

void ResetReplayTransferFixtures(const ServerSession& rSession)
{
	if (sFixture.pSession == &rSession)
	{
		sFixture.replayTransferFixtures.clear();
	}
}

int64_t CountPendingAgentStatusChanges(const ServerSession& rSession)
{
	if (sFixture.pSession != &rSession)
	{
		return 0;
	}

	int64_t iCount = 0;
	for (const auto& [rCoordinate, rChanges] : sFixture.pendingAgentStatusChanges)
	{
		iCount += std::ssize(rChanges);
	}
	return iCount;
}

int64_t CountReplayTransferFixtures(const ServerSession& rSession)
{
	if (sFixture.pSession != &rSession)
	{
		return 0;
	}

	int64_t iCount = 0;
	for (const auto& [rCoordinate, rTransfers] : sFixture.replayTransferFixtures)
	{
		iCount += std::ssize(rTransfers);
	}
	return iCount;
}

void DetachServerSimulationFixtures(const ServerSession& rSession)
{
	if (sFixture.pSession == &rSession)
	{
		sFixture = {};
	}
}

void CountCapturedReplayTransfers(std::span<const StatusChange> transfers, ReplayTransferCaptureCounts& rCounts)
{
	for (const StatusChange& rTransfer : transfers)
	{
		switch (rTransfer.eType)
		{
			case StatusChangeType::kTransferPlayer: ++rCounts.iPlayerCount; break;
			case StatusChangeType::kTransferSpaceship: ++rCounts.iSpaceshipCount; break;
			case StatusChangeType::kTransferBlaster: ++rCounts.iBlasterCount; break;
			case StatusChangeType::kTransferMissile: ++rCounts.iMissileCount; break;
			default: DEBUG_BREAK(); break;
		}
	}
}

} // namespace game

#endif // BT_SERVER
