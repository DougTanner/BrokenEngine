#include "Pch.h"

#include "Agent/Commands/ServerSimulationFixtures.h"

#if defined(BT_SERVER)

#include "Agent/AgentCommandsServerQueries.h"
#include "Agent/Commands/ReplayFixtures.h"
#include "File/Replay.h"
#include "Frame/Collections/Players/Players.h"
#include "Game.h"
#include "Network/Server/ServerClientManager.h"
#include "Network/Server/ServerSession.h"
#include "Network/Server/ServerTransferManager.h"
#include "Profile/ProfileManager.h"
#include "Ui/WrapperBase.h"

namespace game
{

namespace
{

struct ServerSimulationFixtureState
{
	ServerSession* pSession = nullptr;
	std::unordered_map<engine::GridCoord, std::vector<StatusChange>> pendingAgentStatusChanges;
	std::unordered_map<engine::GridCoord, std::vector<StatusChange>> replayTransferFixtures;
};

ServerSimulationFixtureState sFixture;

void Bind(ServerSession& rSession)
{
	if (sFixture.pSession != &rSession)
	{
		sFixture = {};
		sFixture.pSession = &rSession;
	}
}

bool IsCoordActive(engine::GridCoord coord)
{
	return std::find(gpGame->mActiveCoords.begin(), gpGame->mActiveCoords.end(), coord) != gpGame->mActiveCoords.end();
}

bool AreAdjacent(engine::GridCoord source, engine::GridCoord destination)
{
	const int64_t iDeltaX = static_cast<int64_t>(destination.x) - source.x;
	const int64_t iDeltaY = static_cast<int64_t>(destination.y) - source.y;
	return (iDeltaX != 0 || iDeltaY != 0) && std::abs(iDeltaX) <= 1 && std::abs(iDeltaY) <= 1;
}

void CommandReplayRecord([[maybe_unused]] const nlohmann::json& rParams, [[maybe_unused]] nlohmann::json& rResult)
{
	// SaveLoadReplay/SyncReplayTick are compiled out under !kbDebugInput, so the flag would never be consumed.
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("replay requires kbDebugInput build");
	}
	else
	{
		if (!rParams.contains("start"))
		{
			throw std::runtime_error("replay_record requires bool 'start'");
		}
		if (!rParams.at("start").is_boolean())
		{
			throw std::runtime_error("replay_record requires bool 'start'");
		}
		bool bStart = rParams.at("start").get<bool>();
		// kSaveReplay is a pure toggle in SyncReplayTick (empty writer set starts, non-empty stops). While paused the
		// per-tick loop is skipped, so a set-but-unconsumed flag leaves the effective requested state the inverse of
		// IsRecording() — compute it, not IsRecording() alone. bEffective is the state the sim will settle into once the
		// pending flag (if any) is consumed. If that already matches the request, no change is pending; otherwise flip the
		// toggle: setting a clear flag schedules a transition, clearing a set flag cancels a not-yet-consumed one.
		bool bFlagPending = gpGame->mGameFlags & engine::GameFlags::kSaveReplay;
		bool bEffective = engine::gpReplay->IsRecording() != bFlagPending;
		if (bStart == bEffective)
		{
			rResult["pending"] = false;
		}
		else if (bFlagPending)
		{
			// Cancel a pending transition (e.g. a stop request voids a not-yet-started recording).
			if (!engine::gpReplay->IsRecording())
			{
				sFixture.replayTransferFixtures.clear();
				engine::ReplayFixtures::RecordingStartCancelled(*engine::gpReplay);
			}
			gpGame->mGameFlags.Clear(engine::GameFlags::kSaveReplay);
			rResult["pending"] = false;
		}
		else
		{
			gpGame->mGameFlags.Set(engine::GameFlags::kSaveReplay);
			rResult["pending"] = true;
		}
	}
}

void CommandReplayPlay([[maybe_unused]] const nlohmann::json& rParams, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("replay requires kbDebugInput build");
	}
	else
	{
		// Same semantics as F8 / kClientReplayPlaybackRequest: starts playback, or cancels if already replaying.
		gpGame->mGameFlags.Set(engine::GameFlags::kLoadReplay);
		rResult["pending"] = true;
	}
}

void CommandReplayTransferCapture([[maybe_unused]] const nlohmann::json& rParams, nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("replay_transfer_capture requires kbDebugInput build");
	}
	else
	{
		const engine::ReplayFixtures::TransferCaptureSnapshot captureInfo = engine::ReplayFixtures::CaptureSnapshot(*engine::gpReplay);
		rResult["recordingEventTick"] = captureInfo.iRecordingEventTick;
		rResult["playbackEventTick"] = captureInfo.iPlaybackEventTick;
		rResult["firstWriterInputTick"] = captureInfo.iFirstWriterInputTick;
		rResult["writerInputCount"] = captureInfo.iWriterInputCount;
		rResult["playerCount"] = captureInfo.transferCounts.iPlayerCount;
		rResult["spaceshipCount"] = captureInfo.transferCounts.iSpaceshipCount;
		rResult["blasterCount"] = captureInfo.transferCounts.iBlasterCount;
		rResult["missileCount"] = captureInfo.transferCounts.iMissileCount;
	}
}

void CommandReplayDropRetainedEndFrame([[maybe_unused]] const nlohmann::json& rParams, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("replay requires kbDebugInput build");
	}
	else
	{
		if (!engine::gpReplay->IsRecording())
		{
			throw std::runtime_error("replay_drop_retained_end_frame requires active recording");
		}

		engine::GridCoord coord = CoordFromParam(rParams);
		if (!engine::ReplayFixtures::DropRetainedEndFrame(*engine::gpReplay, coord))
		{
			throw std::runtime_error("replay_drop_retained_end_frame requires a retained terminal frame for 'coord'");
		}

		rResult["coord"] = {coord.x, coord.y};
		rResult["dropped"] = true;
	}
}

void CommandReplayInjectPersistenceFailure([[maybe_unused]] const nlohmann::json& rParams, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("replay requires kbDebugInput build");
	}
	else
	{
		if (!rParams.contains("stage"))
		{
			throw std::runtime_error("replay_inject_persistence_failure requires string 'stage'");
		}
		if (!rParams.at("stage").is_string())
		{
			throw std::runtime_error("replay_inject_persistence_failure requires string 'stage'");
		}

		const std::string stage = rParams.at("stage").get<std::string>();
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

		if ((eFailurePoint == engine::ReplayFixtures::PersistenceFailurePoint::kManifestInvalidation ||
			eFailurePoint == engine::ReplayFixtures::PersistenceFailurePoint::kGrid) == engine::gpReplay->IsRecording())
		{
			throw std::runtime_error(engine::gpReplay->IsRecording() ? "selected stage requires recording to be inactive" : "selected stage requires active recording");
		}

		const bool bRequiresCoord = eFailurePoint == engine::ReplayFixtures::PersistenceFailurePoint::kCoordinateWriter
		                         || eFailurePoint == engine::ReplayFixtures::PersistenceFailurePoint::kFullFramesRecord;
		if (bRequiresCoord != rParams.contains("coord"))
		{
			throw std::runtime_error(bRequiresCoord ? "selected stage requires 'coord'" : "'coord' is only valid for coordinate_writer or fullframes_record");
		}

		engine::GridCoord coord {};
		if (bRequiresCoord)
		{
			coord = CoordFromParam(rParams);
		}
		if (!engine::ReplayFixtures::ArmPersistenceFailure(*engine::gpReplay, eFailurePoint, coord))
		{
			throw std::runtime_error("selected 'coord' has no replay writer with an end frame");
		}

		rResult["stage"] = stage;
		if (bRequiresCoord)
		{
			rResult["coord"] = {coord.x, coord.y};
		}
		rResult["armed"] = true;
	}
}
void CommandReplayTransferFixture(const nlohmann::json& rParams, nlohmann::json& rResult)
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
		const bool bPendingStart = (gpGame->mGameFlags & engine::GameFlags::kPaused)
		                        && (gpGame->mGameFlags & engine::GameFlags::kSaveReplay) && !engine::gpReplay->IsRecording();
		if (!engine::gpReplay->IsRecording() && !bPendingStart)
		{
			throw std::runtime_error("replay_transfer_fixture requires active recording or a paused pending recording start");
		}
		if (!rParams.contains("type"))
		{
			throw std::runtime_error("replay_transfer_fixture requires string 'type'");
		}
		if (!rParams.at("type").is_string())
		{
			throw std::runtime_error("replay_transfer_fixture requires string 'type'");
		}
		bool bPauseAfterWriterInput = false;
		if (rParams.contains("pauseAfterWriterInput"))
		{
			if (!rParams.at("pauseAfterWriterInput").is_boolean())
			{
				throw std::runtime_error("'pauseAfterWriterInput' must be bool");
			}
			bPauseAfterWriterInput = rParams.at("pauseAfterWriterInput").get<bool>();
		}
		if (bPauseAfterWriterInput && engine::ReplayFixtures::IsWriterPauseArmed(*engine::gpReplay))
		{
			throw std::runtime_error("replay_transfer_fixture pauseAfterWriterInput is already armed");
		}

		const std::string type = rParams.at("type").get<std::string>();
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

		const engine::GridCoord source = CoordFromParam(rParams, "source");
		const engine::GridCoord destination = CoordFromParam(rParams, "destination");
		if (!AreAdjacent(source, destination))
		{
			throw std::runtime_error("'source' and 'destination' must be distinct adjacent coords");
		}
		if (!IsCoordActive(source))
		{
			throw std::runtime_error("'source' is not active");
		}
		const auto sourceIt = gpGame->mCoordFrames.find(source);
		if (sourceIt == gpGame->mCoordFrames.end())
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

		XMVECTOR vecPosition = XMVectorSet(static_cast<float>(destination.x) * engine::kfCellWidth, static_cast<float>(destination.y) * engine::kfCellHeight, engine::gBaseHeight.Get(), 1.0f);
		if (eType == StatusChangeType::kTransferBlaster)
		{
			auto destinationIt = gpGame->mCoordFrames.find(destination);
			if (destinationIt == gpGame->mCoordFrames.end())
			{
				throw std::runtime_error("replay transfer fixture destination frame is not ready");
			}

			engine::FrameStaticData& rDestinationStaticData = destinationIt->second.staticData;
			if (rDestinationStaticData.elevationGrid.empty() && !rDestinationStaticData.islands.empty())
			{
				// Heap: Build the one-time derived terrain grid before this command samples it.
				ScopedSuppressAllocationTracking suppress;
				engine::gpIslandTerrain->BuildElevationGrid(rDestinationStaticData.coord, rDestinationStaticData.islands, rDestinationStaticData.elevationGrid);
			}
			XMFLOAT4A f4Area {};
			XMStoreFloat4A(&f4Area, rDestinationStaticData.vecArea);
			// Blasters are destroyed by point-terrain contact, so place this debug fixture in a terrain-clear
			// cell and verify the first fixed-tick movement remains clear too.
			constexpr int64_t kiTerrainGridDim = 20;
			float fPitchX = (f4Area.z - f4Area.x) / static_cast<float>(kiTerrainGridDim);
			float fPitchY = (f4Area.y - f4Area.w) / static_cast<float>(kiTerrainGridDim);
			bool bFoundTerrainClearPosition = false;
			for (int64_t iGridY = 0; iGridY < kiTerrainGridDim && !bFoundTerrainClearPosition; ++iGridY)
			{
				for (int64_t iGridX = 0; iGridX < kiTerrainGridDim; ++iGridX)
				{
					XMVECTOR vecCandidate = XMVectorSet(f4Area.x + (static_cast<float>(iGridX) + 0.5f) * fPitchX, f4Area.w + (static_cast<float>(iGridY) + 0.5f) * fPitchY, engine::gBaseHeight.Get(), 1.0f);
					XMVECTOR vecNextCandidate = XMVectorSet(XMVectorGetX(vecCandidate) + engine::kfDeltaTime, XMVectorGetY(vecCandidate), engine::gBaseHeight.Get(), 1.0f);
					if (engine::gpIslandTerrain->FrameElevation(rDestinationStaticData, vecCandidate) < engine::gBaseHeight.Get()
					 && engine::gpIslandTerrain->FrameElevation(rDestinationStaticData, vecNextCandidate) < engine::gBaseHeight.Get())
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
			.alignment = gpGame->PlayerAlignment(),
			.fHealth = 1.0f,
			.fShield = 1.0f,
			.uiTypeIndex = PlayersInterpolate::suiBlasterTypeIndex,
			.fDeltaRotationMax = eType == StatusChangeType::kTransferMissile ? 2.0f : 0.0f,
			.globalPlayerId = engine::global_id_t {eType == StatusChangeType::kTransferPlayer ? gpGame->GenerateGlobalId() : 0},
			.fleetWantedCoord = destination,
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
		rResult["source"] = {source.x, source.y};
		rResult["destination"] = {destination.x, destination.y};
		rResult["pauseAfterWriterInput"] = bPauseAfterWriterInput;
	}
}

int64_t PlayerUuidFromParam(const nlohmann::json& rChange)
{
	if (!rChange.contains("playerUuid"))
	{
		throw std::runtime_error("'playerUuid' required");
	}
	return rChange.at("playerUuid").get<int64_t>();
}

// Mirrors the wire clamp: non-finite → 60, then clamp to [0,60].
float NavigationDelayFromParam(const nlohmann::json& rChange)
{
	float fDelay = rChange.contains("navigationDelay") ? rChange.at("navigationDelay").get<float>() : 60.0f;
	if (!std::isfinite(fDelay))
	{
		fDelay = 60.0f;
	}
	return std::clamp(fDelay, 0.0f, 60.0f);
}

// Build one injectable StatusChange from a change entry, minting a global id at inject-time for SpawnPlayer
// (pushed into rGlobalIds). Only SpawnPlayer/DestroyPlayer/UpdatePlayer/UpdateFleet accepted; others rejected.
std::pair<engine::GridCoord, StatusChange> BuildInjectedChange(const nlohmann::json& rChange, nlohmann::json& rGlobalIds)
{
	engine::GridCoord coord = CoordFromParam(rChange);
	if (!IsCoordActive(coord))
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
		int64_t iGlobalId = gpGame->GenerateGlobalId();
		bool bIsFlagship = rChange.contains("isFlagship") && rChange.at("isFlagship").get<bool>();
		engine::GridCoord fleetWantedCoord = rChange.contains("fleetWantedCoord") ? CoordFromParam(rChange, "fleetWantedCoord") : coord;
		SpawnPlayerData spawn {.iGlobalId = iGlobalId, .bIsFlagship = bIsFlagship, .fleetWantedCoord = fleetWantedCoord, .uiPendingFleetWantedCoordTicks = 0};
		if (rChange.contains("pos"))
		{
			// The consumer rejects out-of-cell spawns.
			const nlohmann::json& rPos = rChange.at("pos");
			if (!rPos.is_array())
			{
				throw std::runtime_error("'pos' must be an [x,y] array of numbers");
			}
			if (rPos.size() != 2)
			{
				throw std::runtime_error("'pos' must be an [x,y] array of numbers");
			}
			if (!rPos.at(0).is_number())
			{
				throw std::runtime_error("'pos' must be an [x,y] array of numbers");
			}
			if (!rPos.at(1).is_number())
			{
				throw std::runtime_error("'pos' must be an [x,y] array of numbers");
			}
			spawn.fSpawnOffsetX = rPos.at(0).get<float>();
			spawn.fSpawnOffsetY = rPos.at(1).get<float>();
		}
		change.eType = StatusChangeType::kSpawnPlayer;
		change.data = spawn;
		rGlobalIds.push_back(iGlobalId);
	}
	else if (type == "DestroyPlayer")
	{
		change.eType = StatusChangeType::kDestroyPlayer;
		change.data = DestroyPlayerData {.iPlayerUuid = PlayerUuidFromParam(rChange)};
	}
	else if (type == "UpdatePlayer")
	{
		bool bUseMissiles = rChange.contains("useMissiles") && rChange.at("useMissiles").get<bool>();
		change.eType = StatusChangeType::kUpdatePlayer;
		change.data = UpdatePlayerData {.iPlayerUuid = PlayerUuidFromParam(rChange), .bUseMissiles = bUseMissiles, .fNavigationDelay = NavigationDelayFromParam(rChange), .uiPendingWeaponModeTicks = static_cast<uint8_t>(engine::kiTickRate)};
	}
	else if (type == "UpdateFleet")
	{
		bool bIsFlagship = rChange.contains("isFlagship") && rChange.at("isFlagship").get<bool>();
		change.eType = StatusChangeType::kUpdateFleet;
		change.data = UpdateFleetData {.iPlayerUuid = PlayerUuidFromParam(rChange), .bIsFlagship = bIsFlagship, .fleetWantedCoord = CoordFromParam(rChange, "fleetWantedCoord"), .uiPendingFleetWantedCoordTicks = static_cast<uint8_t>(engine::kiTickRate)};
	}
	else
	{
		throw std::runtime_error("'type' must be SpawnPlayer|DestroyPlayer|UpdatePlayer|UpdateFleet");
	}
	return {coord, change};
}

// True while a client sits in mClientsWaitingForSpawn: the spawn-assignment-by-snapshot-diff invariant would
// mis-assign an agent SpawnPlayer landing the same tick to the waiting client — reject injection outright.
bool ClientsWaitingForSpawn()
{
	return !gpServerSession->mpClientManager->mClientsWaitingForSpawn.empty();
}

void CommandInjectStatusChanges(const nlohmann::json& rParams, nlohmann::json& rResult)
{
	if (!rParams.is_object())
	{
		throw std::runtime_error("inject_status_changes params must be an object");
	}
	for (const auto& [rKey, rValue] : rParams.items())
	{
		if (rKey != "changes" && rKey != "navQueryActivation")
		{
			throw std::runtime_error("inject_status_changes unknown parameter '" + rKey + "'");
		}
	}
	if (!rParams.contains("changes"))
	{
		throw std::runtime_error("inject_status_changes requires array 'changes'");
	}
	if (!rParams.at("changes").is_array())
	{
		throw std::runtime_error("inject_status_changes requires array 'changes'");
	}

	bool bArmNavQuery = false;
	if (rParams.contains("navQueryActivation"))
	{
		const nlohmann::json& rActivation = rParams.at("navQueryActivation");
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
		bArmNavQuery = true;
	}

	// Replay playback resimulates from the recorded stream: SyncReplayTick's LoadDifference overwrites mFrameInputs,
	// so an injection would be silently lost (or broadcast-but-not-simulated → desync). Reject rather than mislead.
	if (gpGame->mbReplaying)
	{
		throw std::runtime_error("cannot inject during replay playback");
	}
	if (ClientsWaitingForSpawn())
	{
		throw std::runtime_error("cannot inject while clients are waiting for spawn");
	}
	if (bArmNavQuery)
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
		else if (engine::gpReplay->IsRecording())
		{
			throw std::runtime_error("navQueryActivation requires recording and replay to be inactive");
		}
		else if (gpGame->mbReplaying)
		{
			throw std::runtime_error("navQueryActivation requires recording and replay to be inactive");
		}
	}
	const nlohmann::json& rChanges = rParams.at("changes");

	// Build everything first (may throw) so a malformed entry can't partially apply — nothing is queued until all
	// entries validate. Burned global ids on a mid-build throw are harmless (ids are monotonic; gaps are fine).
	nlohmann::json globalIds = nlohmann::json::array();
	std::vector<std::pair<engine::GridCoord, StatusChange>> built;
	for (const nlohmann::json& rChange : rChanges)
	{
		built.push_back(BuildInjectedChange(rChange, globalIds));
	}

	int64_t iQueuedAtTick = 0;
	int64_t iMinimumSampleTick = 0;
	if (bArmNavQuery)
	{
		if constexpr (kbProfiling)
		{
			// Prepare a complete queue copy before touching the profiler state. A failed allocation leaves both the
			// existing queue and the event arm unchanged.
			std::unordered_map<engine::GridCoord, std::vector<StatusChange>> preparedPendingAgentStatusChanges = sFixture.pendingAgentStatusChanges;
			for (const auto& [rCoord, rChange] : built)
			{
				preparedPendingAgentStatusChanges.try_emplace(rCoord).first->second.push_back(rChange);
			}

			// Prepare every response field that could allocate before arming. The command failure path must not report an
			// error after committing the event and queue transaction.
			rResult["injected"] = std::ssize(built);
			rResult["globalIds"] = std::move(globalIds);
			rResult["deferred"] = static_cast<bool>(gpGame->mGameFlags & engine::GameFlags::kPaused);

			// Drain is the main-thread serialization point. Keep the event arm and queue commit in one critical section
			// so another tick cannot move the floor or observe a partially committed transaction.
			std::lock_guard lock(gpProfileManager->mCpuTimerMutex);
			iQueuedAtTick = gpGame->TickCounter();
			constexpr int64_t kiMinimumSampleTickOffset = engine::kiTickRate + 1;
			if (iQueuedAtTick > std::numeric_limits<int64_t>::max() - kiMinimumSampleTickOffset)
			{
				throw std::runtime_error("navQueryActivation tick range exhausted");
			}
			iMinimumSampleTick = iQueuedAtTick + kiMinimumSampleTickOffset;
			rResult["navQueryActivation"] = {
				{"armed", true},
				{"queuedAtTick", iQueuedAtTick},
				{"minimumSampleTick", iMinimumSampleTick},
			};
			if (!gpProfileManager->ArmRawCpuTimerEventLocked(game::kCpuTimerPostRenderUpdateNavQuery, iMinimumSampleTick))
			{
				throw std::runtime_error("navQueryActivation event is occupied or overrun");
			}
			static_assert(noexcept(preparedPendingAgentStatusChanges.swap(sFixture.pendingAgentStatusChanges)));
			preparedPendingAgentStatusChanges.swap(sFixture.pendingAgentStatusChanges);
		}
	}
	else
	{
		for (const auto& [rCoord, rChange] : built)
		{
			QueueAgentStatusChange(*gpServerSession, rCoord, rChange);
		}
		rResult["injected"] = std::ssize(built);
		rResult["globalIds"] = std::move(globalIds);
		rResult["deferred"] = static_cast<bool>(gpGame->mGameFlags & engine::GameFlags::kPaused);
	}
}

void CommandSpawnPlayers(const nlohmann::json& rParams, nlohmann::json& rResult)
{
	if (gpGame->mbReplaying)
	{
		throw std::runtime_error("cannot inject during replay playback");
	}
	if (ClientsWaitingForSpawn())
	{
		throw std::runtime_error("cannot inject while clients are waiting for spawn");
	}
	engine::GridCoord coord = CoordFromParam(rParams);
	if (!IsCoordActive(coord))
	{
		throw std::runtime_error("'coord' is not active");
	}
	if (!rParams.contains("count"))
	{
		throw std::runtime_error("spawn_players requires 'count'");
	}
	int64_t iCount = rParams.at("count").get<int64_t>();
	// Spawn changes are allocated up front, before any tick runs.
	constexpr int64_t kiMaxSpawnCount = 256; // matches the query window default limit
	if (iCount < 0)
	{
		throw std::runtime_error("'count' must be in [0, 256]");
	}
	if (iCount > kiMaxSpawnCount)
	{
		throw std::runtime_error("'count' must be in [0, 256]");
	}
	bool bIsFlagship = rParams.contains("isFlagship") && rParams.at("isFlagship").get<bool>();

	nlohmann::json globalIds = nlohmann::json::array();
	for (int64_t i = 0; i < iCount; ++i)
	{
		int64_t iGlobalId = gpGame->GenerateGlobalId();
		StatusChange change {.eType = StatusChangeType::kSpawnPlayer, .data = SpawnPlayerData {.iGlobalId = iGlobalId, .bIsFlagship = bIsFlagship, .fleetWantedCoord = coord, .uiPendingFleetWantedCoordTicks = 0}};
		QueueAgentStatusChange(*gpServerSession, coord, change);
		globalIds.push_back(iGlobalId);
	}

	rResult["injected"] = iCount;
	rResult["globalIds"] = std::move(globalIds);
	rResult["deferred"] = static_cast<bool>(gpGame->mGameFlags & engine::GameFlags::kPaused);
}

} // namespace

bool ExecuteServerSimulationFixtureCommand(std::string_view cmd, const nlohmann::json& rParams, nlohmann::json& rResult)
{
	if (cmd != "replay_record" && cmd != "replay_play" && cmd != "replay_transfer_capture"
	 && cmd != "replay_drop_retained_end_frame" && cmd != "replay_inject_persistence_failure"
	 && cmd != "replay_transfer_fixture" && cmd != "inject_status_changes" && cmd != "spawn_players")
	{
		return false;
	}
	Bind(*gpServerSession);
	if (cmd == "replay_record")
	{
		CommandReplayRecord(rParams, rResult);
		return true;
	}
	if (cmd == "replay_play")
	{
		CommandReplayPlay(rParams, rResult);
		return true;
	}
	if (cmd == "replay_transfer_capture")
	{
		CommandReplayTransferCapture(rParams, rResult);
		return true;
	}
	if (cmd == "replay_drop_retained_end_frame")
	{
		CommandReplayDropRetainedEndFrame(rParams, rResult);
		return true;
	}
	if (cmd == "replay_inject_persistence_failure")
	{
		CommandReplayInjectPersistenceFailure(rParams, rResult);
		return true;
	}
	if (cmd == "replay_transfer_fixture")
	{
		CommandReplayTransferFixture(rParams, rResult);
		return true;
	}
	if (cmd == "inject_status_changes")
	{
		CommandInjectStatusChanges(rParams, rResult);
		return true;
	}
	if (cmd == "spawn_players")
	{
		CommandSpawnPlayers(rParams, rResult);
		return true;
	}
	return false;
}

void QueueAgentStatusChange(ServerSession& rSession, engine::GridCoord coord, const StatusChange& rChange)
{
	// Runs at the agent command drain point under AgentCommandServer::Drain's allocation suppression.
	Bind(rSession);
	sFixture.pendingAgentStatusChanges.try_emplace(coord).first->second.push_back(rChange);
}

bool QueueReplayTransferFixture(ServerSession& rSession, engine::GridCoord destination, StatusChange transfer)
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

void DrainPendingAgentStatusChanges(ServerSession& rSession)
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
	if (!rSession.mpClientManager->mClientsWaitingForSpawn.empty())
	{
		return;
	}

	for (auto it = sFixture.pendingAgentStatusChanges.begin(); it != sFixture.pendingAgentStatusChanges.end();)
	{
		const engine::GridCoord& rCoord = it->first;
		auto framesIt = gpGame->mCoordFrames.find(rCoord);
		bool bActive = std::find(gpGame->mActiveCoords.begin(), gpGame->mActiveCoords.end(), rCoord) != gpGame->mActiveCoords.end();
		if (!bActive)
		{
			++it;
			continue;
		}
		if (framesIt == gpGame->mCoordFrames.end())
		{
			++it;
			continue;
		}
		if (framesIt->second.pCurrent == nullptr)
		{
			++it;
			continue;
		}
		std::vector<StatusChange>& rStatusChanges = gpGame->mFrameInputs.try_emplace(rCoord).first->second.statusChanges;
		rStatusChanges.insert(rStatusChanges.end(), it->second.begin(), it->second.end());
		std::stable_sort(rStatusChanges.begin(), rStatusChanges.end(), [](const StatusChange& rLeft, const StatusChange& rRight)
		{
			return rLeft.eType < rRight.eType;
		});
		it = sFixture.pendingAgentStatusChanges.erase(it);
	}
}

void DrainReplayTransferFixtures(ServerSession& rSession, engine::ServerTransferManager& rTransferManager)
{
	if (sFixture.pSession != &rSession)
	{
		return;
	}

	for (auto& [rCoord, rTransfers] : sFixture.replayTransferFixtures)
	{
		auto it = gpGame->mCoordFrames.find(rCoord);
		if (it == gpGame->mCoordFrames.end() || it->second.pNext == nullptr)
		{
			gpGame->CreateFrameAtCoord(rCoord);
			engine::CoordFrames& rFrames = gpGame->mCoordFrames.at(rCoord);
			rFrames.pNext = std::make_unique<Frame>();
			std::swap(rFrames.pCurrent, rFrames.pNext);
		}

		std::vector<StatusChange>& rDestinationTransfers = rTransferManager.mTransfers.try_emplace(rCoord).first->second;
		rDestinationTransfers.insert(rDestinationTransfers.end(), std::make_move_iterator(rTransfers.begin()), std::make_move_iterator(rTransfers.end()));
	}
	sFixture.replayTransferFixtures.clear();
}

void ResetPendingAgentStatusChanges(ServerSession& rSession)
{
	if (sFixture.pSession == &rSession)
	{
		sFixture.pendingAgentStatusChanges.clear();
	}
}

void ResetReplayTransferFixtures(ServerSession& rSession)
{
	if (sFixture.pSession == &rSession)
	{
		sFixture.replayTransferFixtures.clear();
	}
}

void DetachServerSimulationFixtures(ServerSession& rSession)
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
