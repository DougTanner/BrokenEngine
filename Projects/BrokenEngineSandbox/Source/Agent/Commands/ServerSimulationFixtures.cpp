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

namespace game
{

struct ServerSimulationFixtureState
{
	const ServerSession* pSession = nullptr;
	std::unordered_map<engine::GridCoord, std::vector<StatusChange>> pendingAgentStatusChanges;
	std::unordered_map<engine::GridCoord, std::vector<StatusChange>> replayTransferFixtures;
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
	return std::find(gpGame->mActiveCoordinates.begin(), gpGame->mActiveCoordinates.end(), coordinate) != gpGame->mActiveCoordinates.end();
}

static bool AreAdjacent(engine::GridCoord source, engine::GridCoord destination)
{
	int64_t iDeltaX = static_cast<int64_t>(destination.iX) - source.iX;
	int64_t iDeltaY = static_cast<int64_t>(destination.iY) - source.iY;
	return (iDeltaX != 0 || iDeltaY != 0) && std::abs(iDeltaX) <= 1 && std::abs(iDeltaY) <= 1;
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

		rResult["stage"] = stage;
		if (bRequiresCoordinate)
		{
			rResult["coord"] = {coordinate.iX, coordinate.iY};
		}
		rResult["armed"] = true;
	}
}
static void CommandReplayTransferFixture(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("replay_transfer_fixture requires kbDebugInput build");
	}
	else
	{
		if (gpGame->mbReplaying)
		{
			throw std::runtime_error("cannot queue replay transfer fixture during replay playback");
		}
		bool bPendingStart = (gpGame->mGameFlags & engine::GameFlags::kPaused) && (gpGame->mGameFlags & engine::GameFlags::kSaveReplay)
		                  && engine::gpReplay->mReplayWriters.empty();
		if (engine::gpReplay->mReplayWriters.empty() && !bPendingStart)
		{
			throw std::runtime_error("replay_transfer_fixture requires active recording or a paused pending recording start");
		}
		if (!rParameters.contains("type"))
		{
			throw std::runtime_error("replay_transfer_fixture requires string 'type'");
		}
		if (!rParameters.at("type").is_string())
		{
			throw std::runtime_error("replay_transfer_fixture requires string 'type'");
		}
		bool bPauseAfterWriterInput = false;
		if (rParameters.contains("pauseAfterWriterInput"))
		{
			if (!rParameters.at("pauseAfterWriterInput").is_boolean())
			{
				throw std::runtime_error("'pauseAfterWriterInput' must be bool");
			}
			bPauseAfterWriterInput = rParameters.at("pauseAfterWriterInput").get<bool>();
		}
		if (bPauseAfterWriterInput && engine::ReplayFixtures::IsWriterPauseArmed(*engine::gpReplay))
		{
			throw std::runtime_error("replay_transfer_fixture pauseAfterWriterInput is already armed");
		}

		std::string type = rParameters.at("type").get<std::string>();
		StatusChangeType eType {};
		if (type == "player")
		{
			eType = StatusChangeType::kTransferPlayer;
		}
		else if (type == "spaceship")
		{
			eType = StatusChangeType::kTransferSpaceship;
		}
		else if (type == "blaster")
		{
			eType = StatusChangeType::kTransferBlaster;
		}
		else if (type == "missile")
		{
			eType = StatusChangeType::kTransferMissile;
		}
		else
		{
			throw std::runtime_error("'type' must be player|spaceship|blaster|missile");
		}

		engine::GridCoord source = CoordinateFromParameter(rParameters, "source");
		engine::GridCoord destination = CoordinateFromParameter(rParameters, "destination");
		if (!AreAdjacent(source, destination))
		{
			throw std::runtime_error("'source' and 'destination' must be distinct adjacent coords");
		}
		if (!IsCoordinateActive(source))
		{
			throw std::runtime_error("'source' is not active");
		}
		auto sourceIt = gpGame->mCoordinateFrames.find(source);
		if (sourceIt == gpGame->mCoordinateFrames.end())
		{
			throw std::runtime_error("'source' frame is not ready");
		}
		if (sourceIt->second.pCurrent == nullptr)
		{
			throw std::runtime_error("'source' frame is not ready");
		}
		if (sourceIt->second.pNext == nullptr)
		{
			throw std::runtime_error("'source' frame is not ready");
		}

		// Transfer payloads are destination-local, so the default arrival point is the destination cell's center.
		XMVECTOR vecPosition = XMVectorSet(0.0f, 0.0f, engine::gBaseHeight.mfCurrent, 1.0f);
		if (eType == StatusChangeType::kTransferBlaster)
		{
			auto destinationIt = gpGame->mCoordinateFrames.find(destination);
			if (destinationIt == gpGame->mCoordinateFrames.end())
			{
				throw std::runtime_error("replay transfer fixture destination frame is not ready");
			}

			engine::FrameStaticData& rDestinationStaticData = destinationIt->second.staticData;
			if (rDestinationStaticData.elevationGrid.empty() && !rDestinationStaticData.islands.empty())
			{
				// Heap: Build the one-time derived terrain grid before this command samples it.
				ScopedSuppressAllocationTracking suppress;
				engine::gpIslandTerrain->BuildElevationGrid(rDestinationStaticData.islands, rDestinationStaticData.elevationGrid);
			}
			XMFLOAT4A f4Area {};
			XMStoreFloat4A(&f4Area, engine::LocalFrameArea());
			// Blasters are destroyed by point-terrain contact, so place this debug fixture in a terrain-clear
			// cell and verify the first fixed-tick movement remains clear too.
			static constexpr int64_t kiTerrainGridDimension = 20;
			float fPitchX = (f4Area.z - f4Area.x) / static_cast<float>(kiTerrainGridDimension);
			float fPitchY = (f4Area.y - f4Area.w) / static_cast<float>(kiTerrainGridDimension);
			bool bFoundTerrainClearPosition = false;
			for (int64_t i = 0; i < kiTerrainGridDimension && !bFoundTerrainClearPosition; ++i)
			{
				for (int64_t j = 0; j < kiTerrainGridDimension; ++j)
				{
					XMVECTOR vecCandidate = XMVectorSet(f4Area.x + (static_cast<float>(j) + 0.5f) * fPitchX, f4Area.w + (static_cast<float>(i) + 0.5f) * fPitchY, engine::gBaseHeight.mfCurrent, 1.0f);
					XMVECTOR vecNextCandidate = XMVectorSet(XMVectorGetX(vecCandidate) + engine::kfDeltaTime, XMVectorGetY(vecCandidate), engine::gBaseHeight.mfCurrent, 1.0f);
					if (engine::gpIslandTerrain->MakeFrameElevationSampler(rDestinationStaticData).Sample(vecCandidate) < engine::gBaseHeight.mfCurrent
					 && engine::gpIslandTerrain->MakeFrameElevationSampler(rDestinationStaticData).Sample(vecNextCandidate) < engine::gBaseHeight.mfCurrent)
					{
						vecPosition = vecCandidate;
						bFoundTerrainClearPosition = true;
						break;
					}
				}
			}
			if (!bFoundTerrainClearPosition)
			{
				throw std::runtime_error("replay transfer fixture destination has no terrain-clear Blaster position");
			}
		}

		TransferData data
		{
			.vecPosition = vecPosition,
			.vecDirection = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
			.vecVelocity = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
			.alignment = gpGame->mPlayerAlignment,
			.fHealth = 1.0f,
			.fShield = 1.0f,
			.uiTypeIndex = static_cast<uint8_t>(PlayersInterpolate::siBlasterTypeIndex),
			.fDeltaRotationMaximum = eType == StatusChangeType::kTransferMissile ? 2.0f : 0.0f,
			.globalPlayerId = engine::GlobalId {.iValue = eType == StatusChangeType::kTransferPlayer ? gpGame->miNextGlobalId++ : 0},
			.fleetWantedCoordinate = destination,
		};
		StatusChange transfer {.eType = eType, .data = std::move(data)};
		if (!QueueReplayTransferFixture(*gpServerSession, destination, std::move(transfer)))
		{
			throw std::runtime_error("replay transfer fixture destination is not live for this type");
		}
		if (bPauseAfterWriterInput)
		{
			engine::ReplayFixtures::ArmPauseAfterNextWriterInput(*engine::gpReplay, bPendingStart);
		}

		rResult["type"] = type;
		rResult["source"] = {source.iX, source.iY};
		rResult["destination"] = {destination.iX, destination.iY};
		rResult["pauseAfterWriterInput"] = bPauseAfterWriterInput;
	}
}

static int64_t PlayerUuidFromParameter(const nlohmann::json& rChange)
{
	if (!rChange.contains("playerUuid"))
	{
		throw std::runtime_error("'playerUuid' required");
	}
	return rChange.at("playerUuid").get<int64_t>();
}

// Rejects a delay the wire would reject, so an injected player never holds an out-of-range delay.
static std::chrono::duration<float> NavigationDelayFromParameter(const nlohmann::json& rChange)
{
	std::chrono::duration<float> navigationDelaySeconds(rChange.contains("navigationDelay") ? rChange.at("navigationDelay").get<float>() : 60.0f);
	if (!PlayersPostRender::IsNavigationDelayInRange(navigationDelaySeconds.count()))
	{
		throw std::runtime_error("'navigationDelay' must be finite and within [0,60]");
	}
	return navigationDelaySeconds;
}

// Only SpawnPlayer, DestroyPlayer, UpdatePlayer and UpdateFleet are injectable.
// SpawnPlayer global ids are minted at injection time and appended to rGlobalIds.
static std::pair<engine::GridCoord, StatusChange> BuildInjectedChange(const nlohmann::json& rChange, nlohmann::json& rGlobalIds)
{
	engine::GridCoord coordinate = CoordinateFromParameter(rChange);
	if (!IsCoordinateActive(coordinate))
	{
		throw std::runtime_error("change 'coord' is not active");
	}
	if (!rChange.contains("type"))
	{
		throw std::runtime_error("each change requires string 'type'");
	}
	if (!rChange.at("type").is_string())
	{
		throw std::runtime_error("each change requires string 'type'");
	}
	std::string type = rChange.at("type").get<std::string>();

	StatusChange change;
	if (type == "SpawnPlayer")
	{
		bool bIsFlagship = rChange.contains("isFlagship") && rChange.at("isFlagship").get<bool>();
		engine::GridCoord fleetWantedCoordinate = rChange.contains("fleetWantedCoord") ? CoordinateFromParameter(rChange, "fleetWantedCoord") : coordinate;
		int64_t iGlobalId = gpGame->miNextGlobalId++;
		SpawnPlayerData spawn {.iGlobalId = iGlobalId, .bIsFlagship = bIsFlagship, .fleetWantedCoordinate = fleetWantedCoordinate, .uiPendingFleetWantedCoordinateTicks = 0};
		if (rChange.contains("pos"))
		{
			// The consumer rejects out-of-cell spawns.
			const nlohmann::json& rPosition = rChange.at("pos");
			if (!rPosition.is_array())
			{
				throw std::runtime_error("'pos' must be an [x,y] array of numbers");
			}
			if (rPosition.size() != 2)
			{
				throw std::runtime_error("'pos' must be an [x,y] array of numbers");
			}
			if (!rPosition.at(0).is_number())
			{
				throw std::runtime_error("'pos' must be an [x,y] array of numbers");
			}
			if (!rPosition.at(1).is_number())
			{
				throw std::runtime_error("'pos' must be an [x,y] array of numbers");
			}
			spawn.fSpawnOffsetX = rPosition.at(0).get<float>();
			spawn.fSpawnOffsetY = rPosition.at(1).get<float>();
		}
		change.eType = StatusChangeType::kSpawnPlayer;
		change.data = spawn;
		rGlobalIds.push_back(iGlobalId);
	}
	else if (type == "DestroyPlayer")
	{
		change.eType = StatusChangeType::kDestroyPlayer;
		change.data = DestroyPlayerData {.iPlayerUuid = PlayerUuidFromParameter(rChange)};
	}
	else if (type == "UpdatePlayer")
	{
		bool bUseMissiles = rChange.contains("useMissiles") && rChange.at("useMissiles").get<bool>();
		change.eType = StatusChangeType::kUpdatePlayer;
		change.data = UpdatePlayerData {.iPlayerUuid = PlayerUuidFromParameter(rChange), .bUseMissiles = bUseMissiles, .navigationDelaySeconds = NavigationDelayFromParameter(rChange), .uiPendingWeaponModeTicks = static_cast<uint8_t>(engine::kiTickRate)};
	}
	else if (type == "UpdateFleet")
	{
		bool bIsFlagship = rChange.contains("isFlagship") && rChange.at("isFlagship").get<bool>();
		change.eType = StatusChangeType::kUpdateFleet;
		change.data = UpdateFleetData {.iPlayerUuid = PlayerUuidFromParameter(rChange), .bIsFlagship = bIsFlagship, .fleetWantedCoordinate = CoordinateFromParameter(rChange, "fleetWantedCoord"), .uiPendingFleetWantedCoordinateTicks = static_cast<uint8_t>(engine::kiTickRate)};
	}
	else
	{
		throw std::runtime_error("'type' must be SpawnPlayer|DestroyPlayer|UpdatePlayer|UpdateFleet");
	}
	return {coordinate, change};
}

static void CommandInjectStatusChanges(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!rParameters.is_object())
	{
		throw std::runtime_error("inject_status_changes params must be an object");
	}
	for (const auto& [rKey, rValue] : rParameters.items())
	{
		if (rKey != "changes" && rKey != "navQueryActivation")
		{
			throw std::runtime_error("inject_status_changes unknown parameter '" + rKey + "'");
		}
	}
	if (!rParameters.contains("changes"))
	{
		throw std::runtime_error("inject_status_changes requires array 'changes'");
	}
	if (!rParameters.at("changes").is_array())
	{
		throw std::runtime_error("inject_status_changes requires array 'changes'");
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
	const nlohmann::json& rChanges = rParameters.at("changes");

	// Queue the batch only after every entry validates; failed builds can leave gaps in the monotonic global ids.
	nlohmann::json globalIds = nlohmann::json::array();
	std::vector<std::pair<engine::GridCoord, StatusChange>> built;
	built.reserve(rChanges.size());
	for (const nlohmann::json& rChange : rChanges)
	{
		built.push_back(BuildInjectedChange(rChange, globalIds));
	}

	int64_t iQueuedAtTick = 0;
	int64_t iMinimumSampleTick = 0;
	if (bArmNavigationQuery)
	{
		if constexpr (kbProfiling)
		{
			// Prepare a complete queue copy before touching the profiler state. A failed allocation leaves both the
			// existing queue and the event arm unchanged.
			std::unordered_map<engine::GridCoord, std::vector<StatusChange>> preparedPendingAgentStatusChanges = sFixture.pendingAgentStatusChanges;
			for (const auto& [rCoordinate, rChange] : built)
			{
				preparedPendingAgentStatusChanges.try_emplace(rCoordinate).first->second.push_back(rChange);
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
			static_assert(noexcept(preparedPendingAgentStatusChanges.swap(sFixture.pendingAgentStatusChanges)));
			preparedPendingAgentStatusChanges.swap(sFixture.pendingAgentStatusChanges);
		}
	}
	else
	{
		for (const auto& [rCoordinate, rChange] : built)
		{
			QueueAgentStatusChange(*gpServerSession, rCoordinate, rChange);
		}
		rResult["injected"] = std::ssize(built);
		rResult["globalIds"] = std::move(globalIds);
		rResult["deferred"] = static_cast<bool>(gpGame->mGameFlags & engine::GameFlags::kPaused);
	}
}

static void CommandSpawnPlayers(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (gpGame->mbReplaying)
	{
		throw std::runtime_error("cannot inject during replay playback");
	}
	engine::GridCoord coordinate = CoordinateFromParameter(rParameters);
	if (!IsCoordinateActive(coordinate))
	{
		throw std::runtime_error("'coord' is not active");
	}
	if (!rParameters.contains("count"))
	{
		throw std::runtime_error("spawn_players requires 'count'");
	}
	int64_t iCount = rParameters.at("count").get<int64_t>();
	// Spawn changes are allocated up front, before any tick runs.
	static constexpr int64_t kiMaximumSpawnCount = 256; // matches the query window default limit
	if (iCount < 0)
	{
		throw std::runtime_error("'count' must be in [0, 256]");
	}
	if (iCount > kiMaximumSpawnCount)
	{
		throw std::runtime_error("'count' must be in [0, 256]");
	}
	bool bIsFlagship = rParameters.contains("isFlagship") && rParameters.at("isFlagship").get<bool>();

	nlohmann::json globalIds = nlohmann::json::array();
	for (int64_t i = 0; i < iCount; ++i)
	{
		int64_t iGlobalId = gpGame->miNextGlobalId++;
		StatusChange change {.eType = StatusChangeType::kSpawnPlayer, .data = SpawnPlayerData {.iGlobalId = iGlobalId, .bIsFlagship = bIsFlagship, .fleetWantedCoordinate = coordinate, .uiPendingFleetWantedCoordinateTicks = 0}};
		QueueAgentStatusChange(*gpServerSession, coordinate, change);
		globalIds.push_back(iGlobalId);
	}

	rResult["injected"] = iCount;
	rResult["globalIds"] = std::move(globalIds);
	rResult["deferred"] = static_cast<bool>(gpGame->mGameFlags & engine::GameFlags::kPaused);
}

// Seeds one synthetic Player arrival near the requested edge and lets the unchanged transfer pipeline carry it
// back out: SpawnTransfer's post-arrival lock skips navigation and acceleration, so the arrival coasts at the
// velocity chosen here until PostCollision sees it leave the cell.
static void CommandInjectOutwardTransfer(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("inject_outward_transfer requires kbDebugInput build");
	}
	else
	{
		if (gpGame->mbReplaying)
		{
			throw std::runtime_error("cannot inject during replay playback");
		}
		if (!rParameters.is_object())
		{
			throw std::runtime_error("inject_outward_transfer requires exactly {\"coord\":[x,y],\"delta\":[dx,dy]}");
		}
		if (rParameters.size() != 2)
		{
			throw std::runtime_error("inject_outward_transfer requires exactly {\"coord\":[x,y],\"delta\":[dx,dy]}");
		}

		engine::GridCoord coordinate = CoordinateFromParameter(rParameters);
		if (!rParameters.contains("delta"))
		{
			throw std::runtime_error("inject_outward_transfer requires exactly {\"coord\":[x,y],\"delta\":[dx,dy]}");
		}
		const nlohmann::json& rDelta = rParameters.at("delta");
		if (!rDelta.is_array())
		{
			throw std::runtime_error("'delta' must be a [dx,dy] array");
		}
		if (rDelta.size() != 2)
		{
			throw std::runtime_error("'delta' must be a [dx,dy] array");
		}
		if (!rDelta.at(0).is_number_integer())
		{
			throw std::runtime_error("'delta' components must be integers in [-1,1] and not both zero");
		}
		if (!rDelta.at(1).is_number_integer())
		{
			throw std::runtime_error("'delta' components must be integers in [-1,1] and not both zero");
		}
		int64_t iDeltaX = rDelta.at(0).get<int64_t>();
		int64_t iDeltaY = rDelta.at(1).get<int64_t>();
		if (std::abs(iDeltaX) > 1)
		{
			throw std::runtime_error("'delta' components must be integers in [-1,1] and not both zero");
		}
		if (std::abs(iDeltaY) > 1)
		{
			throw std::runtime_error("'delta' components must be integers in [-1,1] and not both zero");
		}
		if (iDeltaX == 0 && iDeltaY == 0)
		{
			throw std::runtime_error("'delta' components must be integers in [-1,1] and not both zero");
		}

		XMFLOAT4A f4Area {};
		XMStoreFloat4A(&f4Area, engine::LocalFrameArea());
		// 16 ticks of coast: half of SpawnTransfer's one-second transfer lock, long enough for the harness to
		// observe the seeded cell while it is active and still finished inside the lock. The extra half step
		// keeps the crossing off an exact multiple of the step, so the departing position overshoots the edge
		// instead of landing on it, and the cell-width subtraction PrepareTransferRequest applies leaves the
		// destination-local position strictly inside the destination cell.
		static constexpr float kfCoastMargin = (16.0f - 0.5f) * kfPlayerMaximumSpeed * engine::kfDeltaTime;
		// Midpoint of the requested edge: the half-extent pulled inward on each non-zero axis, the cell centre
		// on a zero-delta one.
		float fPositionX = (iDeltaX > 0) ? f4Area.z - kfCoastMargin : (iDeltaX < 0) ? f4Area.x + kfCoastMargin : (f4Area.x + f4Area.z) * 0.5f;
		float fPositionY = (iDeltaY > 0) ? f4Area.y - kfCoastMargin : (iDeltaY < 0) ? f4Area.w + kfCoastMargin : (f4Area.y + f4Area.w) * 0.5f;
		XMVECTOR vecVelocity = XMVectorSet(static_cast<float>(iDeltaX) * kfPlayerMaximumSpeed, static_cast<float>(iDeltaY) * kfPlayerMaximumSpeed, 0.0f, 0.0f);

		int64_t iGlobalId = gpGame->miNextGlobalId++;
		TransferData data
		{
			.vecPosition = XMVectorSet(fPositionX, fPositionY, engine::gBaseHeight.mfCurrent, 1.0f),
			.vecDirection = XMVector3Normalize(vecVelocity),
			.vecVelocity = vecVelocity,
			.alignment = gpGame->mPlayerAlignment,
			// A zero-armor arrival is flagged exploding on its first tick instead of transferring.
			.fHealth = 1.0f,
			.fShield = 1.0f,
			.globalPlayerId = engine::GlobalId {.iValue = iGlobalId},
			// Seeded cell: no fleet override fights the coast once the transfer lock expires.
			.fleetWantedCoordinate = coordinate,
		};
		StatusChange transfer {.eType = StatusChangeType::kTransferPlayer, .data = std::move(data)};
		if (!QueueReplayTransferFixture(*gpServerSession, coordinate, std::move(transfer)))
		{
			throw std::runtime_error("inject_outward_transfer could not queue the arrival");
		}

		rResult["globalId"] = iGlobalId;
		rResult["deferred"] = static_cast<bool>(gpGame->mGameFlags & engine::GameFlags::kPaused);
	}
}

bool ExecuteServerSimulationFixtureCommand(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (command != "replay_record" && command != "replay_play" && command != "replay_transfer_capture" && command != "replay_drop_retained_end_frame" && command != "replay_inject_persistence_failure" && command != "replay_transfer_fixture" && command != "inject_status_changes" && command != "spawn_players" && command != "inject_outward_transfer")
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
	if (command == "replay_transfer_fixture")
	{
		CommandReplayTransferFixture(rParameters, rResult);
		return true;
	}
	if (command == "inject_status_changes")
	{
		CommandInjectStatusChanges(rParameters, rResult);
		return true;
	}
	if (command == "spawn_players")
	{
		CommandSpawnPlayers(rParameters, rResult);
		return true;
	}
	if (command == "inject_outward_transfer")
	{
		CommandInjectOutwardTransfer(rParameters, rResult);
		return true;
	}
	return false;
}

void QueueAgentStatusChange(const ServerSession& rSession, engine::GridCoord coordinate, const StatusChange& rChange)
{
	// Runs at the agent command drain point under AgentCommandServer::Drain's allocation suppression.
	Bind(rSession);
	sFixture.pendingAgentStatusChanges.try_emplace(coordinate).first->second.push_back(rChange);
}

bool QueueReplayTransferFixture(const ServerSession& rSession, engine::GridCoord destination, StatusChange transfer)
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
	if (transfer.eType != StatusChangeType::kTransferPlayer && !rSession.mpTransferManager->IsDestinationLive(destination))
	{
		return false;
	}

	Bind(rSession);
	sFixture.replayTransferFixtures.try_emplace(destination).first->second.push_back(std::move(transfer));
	return true;
}

void DrainPendingAgentStatusChanges(const ServerSession& rSession)
{
	if (sFixture.pSession != &rSession)
	{
		return;
	}
	if (gpGame->mfLastDeltaTime <= 0.0f)
	{
		return;
	}
	if (gpGame->mbReplaying)
	{
		return;
	}

	for (auto it = sFixture.pendingAgentStatusChanges.begin(); it != sFixture.pendingAgentStatusChanges.end();)
	{
		const engine::GridCoord& rCoordinate = it->first;
		auto framesIt = gpGame->mCoordinateFrames.find(rCoordinate);
		bool bActive = std::find(gpGame->mActiveCoordinates.begin(), gpGame->mActiveCoordinates.end(), rCoordinate) != gpGame->mActiveCoordinates.end();
		if (!bActive)
		{
			++it;
			continue;
		}
		if (framesIt == gpGame->mCoordinateFrames.end())
		{
			++it;
			continue;
		}
		if (framesIt->second.pCurrent == nullptr)
		{
			++it;
			continue;
		}
		std::vector<StatusChange>& rStatusChanges = gpGame->mFrameInputs.try_emplace(rCoordinate).first->second.statusChanges;
		rStatusChanges.insert(rStatusChanges.end(), it->second.begin(), it->second.end());
		std::stable_sort(rStatusChanges.begin(), rStatusChanges.end(), [](const StatusChange& rLeft, const StatusChange& rRight)
		{
			return rLeft.eType < rRight.eType;
		});
		it = sFixture.pendingAgentStatusChanges.erase(it);
	}
}

void DrainReplayTransferFixtures(const ServerSession& rSession, engine::ServerTransferManager& rTransferManager)
{
	if (sFixture.pSession != &rSession)
	{
		return;
	}

	for (const auto& [rCoordinate, rTransfers] : sFixture.replayTransferFixtures)
	{
		rTransferManager.PrepareReplayTransfers(rCoordinate, rTransfers);
	}
	sFixture.replayTransferFixtures.clear();
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
