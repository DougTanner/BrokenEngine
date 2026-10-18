#include "Pch.h"

#if defined(BT_SERVER)

#include "Agent/AgentCommandsServerGeneric.h"

#include "Agent/Commands/ReplayHarnessRigs.h"
#include "File/Replay.h"
#include "Network/Server/ServerTransferManager.h"

#include "Network/Server/ServerSession.h"
#include "Profile/ProfileRows.h"
#include "Game.h"

namespace engine
{

// JSON path echoes require UTF-8 bytes.
static std::string PathToUtf8(const std::filesystem::path& rPath)
{
	std::u8string u8String = rPath.u8string();
	return std::string(reinterpret_cast<const char*>(u8String.c_str()), u8String.size());
}

static bool IsWindowsReservedDeviceBasename(std::string_view utf8)
{
	std::string basename(utf8.substr(0, utf8.find('.')));
	// Win32 strips trailing spaces and dots from a final path component, so "NUL " or "NUL ." still
	// resolves to the device.
	while (!basename.empty() && (basename.back() == ' ' || basename.back() == '.'))
	{
		basename.pop_back();
	}
	for (char& rCharacter : basename)
	{
		if (rCharacter >= 'a' && rCharacter <= 'z')
		{
			rCharacter = static_cast<char>(rCharacter - ('a' - 'A'));
		}
	}

	if (basename == "CON" || basename == "NUL" || basename == "PRN" || basename == "AUX")
	{
		return true;
	}

	// Win32 also resolves the superscript spellings COM¹/COM²/COM³ and LPT¹/LPT²/LPT³ to the same devices.
	// Those trailing characters are the two UTF-8 bytes 0xC2 0xB9/0xB2/0xB3, so match the bytes directly.
	if (basename.size() == 5 && (basename.starts_with("COM") || basename.starts_with("LPT")) && basename[3] == '\xC2'
	 && (basename[4] == '\xB9' || basename[4] == '\xB2' || basename[4] == '\xB3'))
	{
		return true;
	}

	return basename.size() == 4 && (basename.starts_with("COM") || basename.starts_with("LPT")) && basename[3] >= '1' && basename[3] <= '9';
}

// Save/load filenames must stay within the user's appdata directory and cannot name Windows devices.
static std::filesystem::path BareFilenameParameter(const nlohmann::json& rValue)
{
	std::string utf8 = rValue.get<std::string>();
	if (utf8.empty())
	{
		throw std::runtime_error("'file' must be a non-empty bare filename");
	}
	if (utf8.contains('\0'))
	{
		throw std::runtime_error("'file' must not contain an embedded NUL");
	}
	if (utf8.contains('/') || utf8.contains('\\') || utf8.contains(':') || utf8.contains(".."))
	{
		throw std::runtime_error("'file' must be a bare filename (no path separators, drive/stream ':', or '..')");
	}
	if (IsWindowsReservedDeviceBasename(utf8))
	{
		throw std::runtime_error("'file' must not use a reserved Windows device name");
	}
	std::u8string u8String(utf8.begin(), utf8.end());
	return std::filesystem::path(u8String);
}

void CommandStatus([[maybe_unused]] const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	rResult["tick"] = game::gpGame->miTickCounter;
	rResult["paused"] = game::gpGame->mGameFlags & engine::GameFlags::kPaused;
	rResult["recording"] = (!engine::gpReplay->mReplayWriters.empty());
	rResult["replaying"] = game::gpGame->mbReplaying;

	int64_t iClientCount = 0;
	for (const engine::ClientConnection& rClient : engine::gpServer->mClients)
	{
		if (rClient.bHandshakeComplete)
		{
			++iClientCount;
		}
	}
	rResult["clientCount"] = iClientCount;

	nlohmann::json activeCoordinates = nlohmann::json::array();
	for (const engine::GridCoord& rCoordinate : game::gpGame->mActiveCoordinates)
	{
		activeCoordinates.push_back({rCoordinate.iX, rCoordinate.iY});
	}
	rResult["activeCoords"] = std::move(activeCoordinates);

	rResult["nextGlobalId"] = game::gpGame->miNextGlobalId;
	rResult["harvestedTransferTotal"] = game::gpServerSession->mpTransferManager->miHarvestedTransferTotal;
}

static void CommandPause(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!rParameters.contains("paused") || !rParameters.at("paused").is_boolean())
	{
		throw std::runtime_error("pause requires bool 'paused'");
	}
	bool bPaused = rParameters.at("paused").get<bool>();
	game::gpGame->mGameFlags.Set(engine::GameFlags::kPaused, bPaused); // mirrors kClientPauseRequest
	rResult["paused"] = bPaused;
}

static void CommandTimescale(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!rParameters.contains("faster") || !rParameters.at("faster").is_boolean())
	{
		throw std::runtime_error("timescale requires bool 'faster'");
	}
	bool bFaster = rParameters.at("faster").get<bool>();
	game::gpServerSession->StepTimescale(bFaster); // shared with kClientTimespeedRequest — steps + broadcasts to clients
	rResult["numerator"] = game::gpGame->mTimeStep.miTimeMultiply;
	rResult["denominator"] = game::gpGame->mTimeStep.miTimeDivide;
}

static void CommandSave(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	std::filesystem::path file = rParameters.contains("file") ? BareFilenameParameter(rParameters.at("file")) : std::filesystem::path("ServerQuicksave.save");
	if (!game::gpGame->mServerSaveLoad.ServerSave(file))
	{
		throw std::runtime_error("save failed to write '" + PathToUtf8(file) + "'");
	}
	rResult["file"] = PathToUtf8(file);
}

void CommandLoad(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (rParameters.contains("pauseAfterLoad") && !rParameters.at("pauseAfterLoad").is_boolean())
	{
		throw std::runtime_error("load requires bool 'pauseAfterLoad'");
	}
	bool bPauseAfterLoad = rParameters.value("pauseAfterLoad", false);
	std::filesystem::path file = rParameters.contains("file") ? BareFilenameParameter(rParameters.at("file")) : std::filesystem::path("ServerQuicksave.save");

	bool bResetToFresh = false;
	if (!game::gpGame->mServerSaveLoad.ServerLoad(file))
	{
		// A load failure after header validation clears the grid without running ServerLoad's success tail.
		// Rebuild a fresh game as kClientLoadRequest does before simulation resumes.
		game::gpGame->mServerSaveLoad.ServerReset();
		bResetToFresh = true;
	}
	if (bPauseAfterLoad)
	{
		game::gpGame->mGameFlags.Set(engine::GameFlags::kPaused);
	}
	rResult["file"] = PathToUtf8(file);
	rResult["resetToFresh"] = bResetToFresh;
	rResult["paused"] = static_cast<bool>(game::gpGame->mGameFlags & engine::GameFlags::kPaused);
}

static void CommandReset([[maybe_unused]] const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	game::gpGame->mServerSaveLoad.ServerReset();
	rResult = nlohmann::json::object();
}

static void CommandQueryProfile(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!rParameters.empty())
	{
		throw std::runtime_error("query_profile requires empty params");
	}

	nlohmann::json timers = nlohmann::json::array();
	{
		std::lock_guard lock(gpProfileManager->mCpuTimerMutex);
		for (int64_t i = 0; i < gpProfileManager->miCpuTimerCount; ++i)
		{
			engine::CpuTimer& rTimer = gpProfileManager->GetCpuTimer(i);
			nlohmann::json timer;
			timer["index"] = i;
			timer["name"] = std::string(gpProfileManager->GetCpuTimerName(i));
			timer["currentUs"] = rTimer.smoothedMicroseconds.Current();
			timer["averageUs"] = rTimer.smoothedMicroseconds.Average();
			timer["maxUs"] = rTimer.smoothedMicroseconds.Maximum();
			timer["allocations"] = rTimer.smoothedAllocations.Current();
			timer["threads"] = rTimer.iThreads;
			if constexpr (kbProfiling)
			{
				if (i == game::kCpuTimerPostRenderUpdateNavigationQuery)
				{
					engine::RawCpuTimerRecord rawRecord = gpProfileManager->mpRawCpuTimers[static_cast<size_t>(i)].record;
					timer["sampleSequence"] = rawRecord.iSampleSequence;
					timer["sampleUs"] = rawRecord.iSampleMicroseconds;
					timer["queryCount"] = rawRecord.iInvocationCount;
					timer["aStarCount"] = rawRecord.iAuxiliaryCount;
				}
			}
			timers.push_back(std::move(timer));
		}
	}
	rResult["timers"] = std::move(timers);

	nlohmann::json counters = nlohmann::json::array();
	for (int64_t i = 0; i < gpProfileManager->miCpuCounterCount; ++i)
	{
		engine::CpuCounter& rCounter = gpProfileManager->GetCpuCounter(i);
		nlohmann::json counter;
		counter["index"] = i;
		counter["name"] = std::string(gpProfileManager->GetCpuCounterName(i));
		counter["count"] = rCounter.iCount;
		counters.push_back(std::move(counter));
	}
	rResult["counters"] = std::move(counters);
}

void RefuseDuringReplay(std::string_view command)
{
	if (!engine::gpReplay->mReplayWriters.empty() || game::gpGame->mbReplaying || (game::gpGame->mGameFlags & engine::GameFlags::kSaveReplay) || (game::gpGame->mGameFlags & engine::GameFlags::kLoadReplay))
	{
		throw std::runtime_error(std::format("{} is refused while recording, during playback, or with a replay_record or replay_play transition pending", command));
	}
}

static void CommandCreateCell([[maybe_unused]] const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("create_cell requires kbDebugInput build");
	}
	else
	{
		RefuseDuringReplay("create_cell");
		for ([[maybe_unused]] const auto& [rKey, rValue] : rParameters.items())
		{
			if (rKey != "coord")
			{
				throw std::runtime_error("create_cell unknown parameter '" + rKey + "'");
			}
		}
		engine::GridCoord coordinate = CoordinateFromParameter(rParameters);
		// CreateCellAtCoordinate replaces a present cell's frame, which would discard its state.
		if (game::gpGame->mCells.contains(coordinate))
		{
			throw std::runtime_error("coord already has a cell; pin it with edit_frame");
		}
		game::gpGame->CreateCellAtCoordinate(coordinate);
		game::gpGame->mCells.at(coordinate).pCurrent->postRender.uiPinned = 1;
		rResult["coord"] = nlohmann::json::array({coordinate.iX, coordinate.iY});
	}
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
		bool bFlagPending = game::gpGame->mGameFlags & engine::GameFlags::kSaveReplay;
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
			game::gpGame->mGameFlags.Set(engine::GameFlags::kSaveReplay, false);
			rResult["pending"] = false;
		}
		else
		{
			game::gpGame->mGameFlags.Set(engine::GameFlags::kSaveReplay);
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
		game::gpGame->mGameFlags.Set(engine::GameFlags::kLoadReplay);
		rResult["pending"] = true;
	}
}

ReplayHarnessRigs::TransferCaptureSnapshot CommandReplayTransferCapture([[maybe_unused]] const nlohmann::json& rParameters, nlohmann::json& rResult)
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
			events.push_back(std::move(eventJson));
		}
		rResult["events"] = std::move(events);
		return captureSnapshot;
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

bool ExecuteServerAgentCommand(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (command == "status")
	{
		CommandStatus(rParameters, rResult);
		return true;
	}
	if (command == "pause")
	{
		CommandPause(rParameters, rResult);
		return true;
	}
	if (command == "timescale")
	{
		CommandTimescale(rParameters, rResult);
		return true;
	}
	if (command == "save")
	{
		CommandSave(rParameters, rResult);
		return true;
	}
	if (command == "load")
	{
		CommandLoad(rParameters, rResult);
		return true;
	}
	if (command == "reset")
	{
		CommandReset(rParameters, rResult);
		return true;
	}
	if (command == "create_cell")
	{
		CommandCreateCell(rParameters, rResult);
		return true;
	}
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
		std::ignore = CommandReplayTransferCapture(rParameters, rResult);
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
	if (command == "query_profile")
	{
		CommandQueryProfile(rParameters, rResult);
		return true;
	}
	return false;
}

} // namespace engine

#endif // defined(BT_SERVER)
