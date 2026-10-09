#include "Agent/AgentCommands.h"

#if defined(BT_SERVER)

#include "File/Replay.h"

#include "Agent/Commands/ServerFaultHarnessRigs.h"
#include "Agent/Commands/ServerFrameEdit.h"
#include "Agent/Commands/ServerSimulationHarnessRigs.h"
#include "Agent/AgentCommandsServerQueries.h"
#include "Network/Server/ServerFleetManager.h"
#include "Network/Server/ServerSession.h"
#include "Profile/ProfileManager.h"
#include "Game.h"

namespace game
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

static void CommandStatus([[maybe_unused]] const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	rResult["tick"] = gpGame->miTickCounter;
	rResult["paused"] = gpGame->mGameFlags & engine::GameFlags::kPaused;
	rResult["recording"] = (!engine::gpReplay->mReplayWriters.empty());
	rResult["replaying"] = gpGame->mbReplaying;

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
	for (const engine::GridCoord& rCoordinate : gpGame->mActiveCoordinates)
	{
		activeCoordinates.push_back({rCoordinate.iX, rCoordinate.iY});
	}
	rResult["activeCoords"] = std::move(activeCoordinates);

	rResult["nextGlobalId"] = gpGame->miNextGlobalId;
	rResult["pendingFlagshipUpdateCount"] = std::ssize(gpServerSession->mpFleetManager->mNavigation.mPendingFlagshipUpdates);
	rResult["harvestedTransferTotal"] = gpServerSession->miHarvestedTransferTotal;
	rResult["pendingTransferHarnessRigCount"] = CountReplayTransferHarnessRigs(*gpServerSession);
	rResult["pendingAgentStatusChangeCount"] = CountPendingAgentStatusChanges(*gpServerSession);
}

static void CommandPause(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!rParameters.contains("paused") || !rParameters.at("paused").is_boolean())
	{
		throw std::runtime_error("pause requires bool 'paused'");
	}
	bool bPaused = rParameters.at("paused").get<bool>();
	gpGame->mGameFlags.Set(engine::GameFlags::kPaused, bPaused); // mirrors kClientPauseRequest
	rResult["paused"] = bPaused;
}

static void CommandTimescale(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!rParameters.contains("faster") || !rParameters.at("faster").is_boolean())
	{
		throw std::runtime_error("timescale requires bool 'faster'");
	}
	bool bFaster = rParameters.at("faster").get<bool>();
	gpServerSession->StepTimescale(bFaster); // shared with kClientTimespeedRequest — steps + broadcasts to clients
	rResult["numerator"] = gpGame->mTimeStep.miTimeMultiply;
	rResult["denominator"] = gpGame->mTimeStep.miTimeDivide;
}

static void CommandSave(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	std::filesystem::path file = rParameters.contains("file") ? BareFilenameParameter(rParameters.at("file")) : std::filesystem::path("ServerQuicksave.save");
	if (!gpGame->mGameSaveLoad.ServerSave(file))
	{
		throw std::runtime_error("save failed to write '" + PathToUtf8(file) + "'");
	}
	rResult["file"] = PathToUtf8(file);
}

static void CommandLoad(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (rParameters.contains("pauseAfterLoad") && !rParameters.at("pauseAfterLoad").is_boolean())
	{
		throw std::runtime_error("load requires bool 'pauseAfterLoad'");
	}
	bool bPauseAfterLoad = rParameters.value("pauseAfterLoad", false);
	std::filesystem::path file = rParameters.contains("file") ? BareFilenameParameter(rParameters.at("file")) : std::filesystem::path("ServerQuicksave.save");

	bool bResetToFresh = false;
	if (!gpGame->mGameSaveLoad.ServerLoad(file))
	{
		// A load failure after header validation clears the grid without running ServerLoad's success tail.
		// Rebuild a fresh game as kClientLoadRequest does before simulation resumes.
		gpGame->mGameSaveLoad.ServerReset();
		bResetToFresh = true;
	}
	if (bPauseAfterLoad)
	{
		gpGame->mGameFlags.Set(engine::GameFlags::kPaused);
	}
	rResult["file"] = PathToUtf8(file);
	rResult["resetToFresh"] = bResetToFresh;
	rResult["paused"] = static_cast<bool>(gpGame->mGameFlags & engine::GameFlags::kPaused);
	rResult["pendingFlagshipUpdateCount"] = std::ssize(gpServerSession->mpFleetManager->mNavigation.mPendingFlagshipUpdates);
}

static void CommandReset([[maybe_unused]] const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	gpGame->mGameSaveLoad.ServerReset();
	rResult = nlohmann::json::object();
}

static void CommandQueryProfile(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!rParameters.is_object())
	{
		throw std::runtime_error("query_profile params must be an object");
	}

	bool bAcknowledgementRequested = false;
	int64_t iAcknowledgementSequence = 0;
	for (const auto& [rKey, rValue] : rParameters.items())
	{
		if (rKey != "ackActivationEventSequence")
		{
			throw std::runtime_error("query_profile unknown parameter '" + rKey + "'");
		}
		if (!rValue.is_number_unsigned() || rValue.get<uint64_t>() == 0)
		{
			throw std::runtime_error("'ackActivationEventSequence' must be a nonzero unsigned integer");
		}
		bAcknowledgementRequested = true;
		iAcknowledgementSequence = static_cast<int64_t>(rValue.get<uint64_t>());
	}
	if constexpr (!kbProfiling)
	{
		if (bAcknowledgementRequested)
		{
			throw std::runtime_error("ackActivationEventSequence requires a profiling server");
		}
	}

	nlohmann::json timers = nlohmann::json::array();
	{
		std::lock_guard lock(gpProfileManager->mCpuTimerMutex);
		bool bActivationEventAcknowledged = false;
		if constexpr (kbProfiling)
		{
			if (bAcknowledgementRequested)
			{
				bActivationEventAcknowledged = gpProfileManager->AcknowledgeRawCpuTimerEvent(game::kCpuTimerPostRenderUpdateNavigationQuery, iAcknowledgementSequence);
			}
		}

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

					engine::RawCpuTimerEventRecord eventRecord = gpProfileManager->mpRawCpuTimers[static_cast<size_t>(i)].eventRecord;
					bool bEventAvailable = eventRecord.flags & engine::RawCpuTimerEventFlags::kAvailable;
					bool bEventOverrun = eventRecord.flags & engine::RawCpuTimerEventFlags::kOverrun;
					timer["activationEvent"] = {
						{"available", bEventAvailable},
						{"eventSequence", eventRecord.iEventSequence},
						{"sampleSequence", eventRecord.iSampleSequence},
						{"sampleTick", eventRecord.iSampleTick},
						{"sampleUs", eventRecord.iSampleMicroseconds},
						{"queryCount", eventRecord.iInvocationCount},
						{"aStarCount", eventRecord.iAuxiliaryCount},
						{"qualifying", bEventAvailable && eventRecord.iInvocationCount == 8 && eventRecord.iAuxiliaryCount == 8},
						{"overrun", bEventOverrun},
					};
					if (bAcknowledgementRequested)
					{
						timer["activationEventAcknowledged"] = bActivationEventAcknowledged;
					}
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

// Coordinate elements must be integers representable by int32_t.
engine::GridCoord CoordinateFromParameter(const nlohmann::json& rParameters, std::string_view key)
{
	if (!rParameters.contains(key) || !rParameters.at(std::string(key)).is_array() || rParameters.at(std::string(key)).size() != 2)
	{
		std::string message("'");
		message.append(key);
		message.append("' must be a [x,y] array");
		throw std::runtime_error(message);
	}
	const nlohmann::json& rCoordinate = rParameters.at(std::string(key));
	std::string name = std::format("'{}'", key);
	return engine::GridCoord {.iX = static_cast<int32_t>(engine::AgentGridCoordinateValue(rCoordinate.at(0), name)), .iY = static_cast<int32_t>(engine::AgentGridCoordinateValue(rCoordinate.at(1), name))};
}

bool ExecuteAgentCommandServer(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (command == "status")
	{
		CommandStatus(rParameters, rResult);
		return true;
	}
	if (command == "game_packet_fault_harness_rig")
	{
		CommandGamePacketFaultHarnessRig(rParameters, rResult);
		return true;
	}
	if (command == "engine_packet_fault_harness_rig")
	{
		CommandEnginePacketFaultHarnessRig(rParameters, rResult);
		return true;
	}
	if (command == "server_pre_handshake_ack_harness_rig")
	{
		CommandServerPreHandshakeAcknowledgmentHarnessRig(rParameters, rResult);
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
	if (command == "edit_frame")
	{
		CommandEditFrame(rParameters, rResult);
		return true;
	}
	if (ExecuteServerSimulationHarnessRigCommand(command, rParameters, rResult))
	{
		return true;
	}
	if (command == "query_frame")
	{
		CommandQueryFrame(rParameters, rResult);
		return true;
	}
	if (command == "query_players")
	{
		CommandQueryPlayers(rParameters, rResult);
		return true;
	}
	if (command == "query_collection")
	{
		CommandQueryCollection(rParameters, rResult);
		return true;
	}
	if (command == "query_profile")
	{
		CommandQueryProfile(rParameters, rResult);
		return true;
	}
	return false;
}

} // namespace game

#endif // defined(BT_SERVER)
