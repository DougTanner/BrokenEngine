#include "Agent/AgentCommands.h"

#if defined(BT_SERVER)

#include "Agent/Commands/ServerFaultFixtures.h"
#include "Agent/AgentCommandsServerQueries.h"
#include "Agent/Commands/ServerSimulationFixtures.h"
#include "File/Replay.h"
#include "Game.h"
#include "Network/Server/ServerFleetManager.h"
#include "Network/Server/ServerSession.h"
#include "Profile/ProfileManager.h"

namespace game
{

namespace
{

// UTF-8 filesystem path -> UTF-8 std::string for JSON result echoing (no heap-narrow-conversion surprises).
std::string PathToUtf8(const std::filesystem::path& rPath)
{
	std::u8string u8String = rPath.u8string();
	return std::string(reinterpret_cast<const char*>(u8String.c_str()), u8String.size());
}

bool IsWindowsReservedDeviceBasename(std::string_view utf8)
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

// Trust boundary: the agent-supplied save/load filename lands in the user's appdata directory. Reject anything
// but a bare filename (no path separators, no "..") and Windows reserved device basenames.
std::filesystem::path BareFilenameParam(const nlohmann::json& rValue)
{
	std::string utf8 = rValue.get<std::string>(); // throws on a non-string
	if (utf8.empty())
	{
		throw std::runtime_error("'file' must be a non-empty bare filename");
	}
	if (utf8.find('\0') != std::string::npos)
	{
		throw std::runtime_error("'file' must not contain an embedded NUL");
	}
	if (utf8.find('/') != std::string::npos || utf8.find('\\') != std::string::npos || utf8.find(':') != std::string::npos || utf8.find("..") != std::string::npos)
	{
		throw std::runtime_error("'file' must be a bare filename (no path separators, drive/stream ':', or '..')");
	}
	if (IsWindowsReservedDeviceBasename(utf8))
	{
		throw std::runtime_error("'file' must not use a reserved Windows device name");
	}
	return std::filesystem::path(reinterpret_cast<const char8_t*>(utf8.c_str()));
}

void CommandStatus([[maybe_unused]] const nlohmann::json& rParams, nlohmann::json& rResult)
{
	rResult["tick"] = gpGame->TickCounter();
	rResult["paused"] = gpGame->mGameFlags & engine::GameFlags::kPaused;
	rResult["recording"] = engine::gpReplay->IsRecording();
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

	nlohmann::json activeCoords = nlohmann::json::array();
	for (const engine::GridCoord& rCoord : gpGame->mActiveCoords)
	{
		activeCoords.push_back({rCoord.x, rCoord.y});
	}
	rResult["activeCoords"] = std::move(activeCoords);

	rResult["nextGlobalId"] = gpGame->NextGlobalId();
	rResult["pendingFlagshipUpdateCount"] = std::ssize(gpServerSession->mpFleetManager->mNavigation.mPendingFlagshipUpdates);
}

void CommandPause(const nlohmann::json& rParams, nlohmann::json& rResult)
{
	if (!rParams.contains("paused") || !rParams.at("paused").is_boolean())
	{
		throw std::runtime_error("pause requires bool 'paused'");
	}
	bool bPaused = rParams.at("paused").get<bool>();
	gpGame->mGameFlags.Set(engine::GameFlags::kPaused, bPaused); // mirrors kClientPauseRequest
	rResult["paused"] = bPaused;
}

void CommandTimescale(const nlohmann::json& rParams, nlohmann::json& rResult)
{
	if (!rParams.contains("faster") || !rParams.at("faster").is_boolean())
	{
		throw std::runtime_error("timescale requires bool 'faster'");
	}
	bool bFaster = rParams.at("faster").get<bool>();
	gpServerSession->StepTimescale(bFaster); // shared with kClientTimespeedRequest — steps + broadcasts to clients
	rResult["numerator"] = gpGame->mTimeStep.miTimeMultiply;
	rResult["denominator"] = gpGame->mTimeStep.miTimeDivide;
}

void CommandSave(const nlohmann::json& rParams, nlohmann::json& rResult)
{
	std::filesystem::path file = rParams.contains("file") ? BareFilenameParam(rParams.at("file")) : gpGame->QuicksaveFile();
	if (!gpGame->mGameSaveLoad.ServerSave(file))
	{
		throw std::runtime_error("save failed to write '" + PathToUtf8(file) + "'");
	}
	rResult["file"] = PathToUtf8(file);
}

void CommandLoad(const nlohmann::json& rParams, nlohmann::json& rResult)
{
	if (rParams.contains("pauseAfterLoad") && !rParams.at("pauseAfterLoad").is_boolean())
	{
		throw std::runtime_error("load requires bool 'pauseAfterLoad'");
	}
	bool bPauseAfterLoad = rParams.value("pauseAfterLoad", false);
	std::filesystem::path file = rParams.contains("file") ? BareFilenameParam(rParams.at("file")) : gpGame->QuicksaveFile();

	bool bResetToFresh = false;
	if (!gpGame->mGameSaveLoad.ServerLoad(file))
	{
		// Corrupt/truncated save: engine::ReadGridSave already left a clean-slate grid, but ServerLoad's success tail never ran.
		// Fall back to a fresh game exactly like kClientLoadRequest rather than ticking a torn grid.
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

void CommandReset([[maybe_unused]] const nlohmann::json& rParams, nlohmann::json& rResult)
{
	gpGame->mGameSaveLoad.ServerReset();
	rResult = nlohmann::json::object();
}

void CommandQueryProfile(const nlohmann::json& rParams, nlohmann::json& rResult)
{
	if (!rParams.is_object())
	{
		throw std::runtime_error("query_profile params must be an object");
	}

	bool bAcknowledgementRequested = false;
	uint64_t uiAcknowledgementSequence = 0;
	for (const auto& [rKey, rValue] : rParams.items())
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
		uiAcknowledgementSequence = rValue.get<uint64_t>();
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
				bActivationEventAcknowledged = gpProfileManager->AcknowledgeRawCpuTimerEvent(game::kCpuTimerPostRenderUpdateNavQuery, uiAcknowledgementSequence);
			}
		}

		for (int64_t i = 0; i < gpProfileManager->GetCpuTimerCount(); ++i)
		{
			engine::CpuTimer& rTimer = gpProfileManager->GetCpuTimer(i);
			nlohmann::json timer;
			timer["index"] = i;
			timer["name"] = std::string(gpProfileManager->GetCpuTimerName(i));
			timer["currentUs"] = rTimer.smoothedMicroseconds.Current();
			timer["averageUs"] = rTimer.smoothedMicroseconds.Average();
			timer["maxUs"] = rTimer.smoothedMicroseconds.Max();
			timer["allocations"] = rTimer.smoothedAllocations.Current();
			timer["threads"] = rTimer.iThreads;
			if constexpr (kbProfiling)
			{
				if (i == game::kCpuTimerPostRenderUpdateNavQuery)
				{
					engine::RawCpuTimerRecord rawRecord = gpProfileManager->GetRawCpuTimer(i);
					timer["sampleSequence"] = rawRecord.uiSampleSequence;
					timer["sampleUs"] = rawRecord.iSampleUs;
					timer["queryCount"] = rawRecord.iInvocationCount;
					timer["aStarCount"] = rawRecord.iAuxiliaryCount;

					engine::RawCpuTimerEventRecord eventRecord = gpProfileManager->GetRawCpuTimerEvent(i);
					const bool bEventAvailable = eventRecord.flags & engine::RawCpuTimerEventFlags::kAvailable;
					const bool bEventOverrun = eventRecord.flags & engine::RawCpuTimerEventFlags::kOverrun;
					timer["activationEvent"] = {
						{"available", bEventAvailable},
						{"eventSequence", eventRecord.uiEventSequence},
						{"sampleSequence", eventRecord.uiSampleSequence},
						{"sampleTick", eventRecord.iSampleTick},
						{"sampleUs", eventRecord.iSampleUs},
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
	for (int64_t i = 0; i < gpProfileManager->GetCpuCounterCount(); ++i)
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

} // namespace

// Shared agent helpers validate external parameters at the trust boundary.

// Parse a [x,y] JSON array into a GridCoord; .get<int32_t>() throws on a non-number.
engine::GridCoord CoordFromParam(const nlohmann::json& rParams, const char* pcKey)
{
	if (!rParams.contains(pcKey) || !rParams.at(pcKey).is_array() || rParams.at(pcKey).size() != 2)
	{
		throw std::runtime_error(std::string("'") + pcKey + "' must be a [x,y] array");
	}
	const nlohmann::json& rCoord = rParams.at(pcKey);
	return engine::GridCoord {rCoord.at(0).get<int32_t>(), rCoord.at(1).get<int32_t>()};
}

bool ExecuteAgentCommandServer(std::string_view cmd, const nlohmann::json& rParams, nlohmann::json& rResult)
{
	if (cmd == "status")
	{
		CommandStatus(rParams, rResult);
		return true;
	}
	if (cmd == "game_packet_fault_fixture")
	{
		CommandGamePacketFaultFixture(rParams, rResult);
		return true;
	}
	if (cmd == "engine_packet_fault_fixture")
	{
		CommandEnginePacketFaultFixture(rParams, rResult);
		return true;
	}
	if (cmd == "server_pre_handshake_ack_fixture")
	{
		CommandServerPreHandshakeAckFixture(rParams, rResult);
		return true;
	}
	if (cmd == "pause")
	{
		CommandPause(rParams, rResult);
		return true;
	}
	if (cmd == "timescale")
	{
		CommandTimescale(rParams, rResult);
		return true;
	}
	if (cmd == "save")
	{
		CommandSave(rParams, rResult);
		return true;
	}
	if (cmd == "load")
	{
		CommandLoad(rParams, rResult);
		return true;
	}
	if (cmd == "reset")
	{
		CommandReset(rParams, rResult);
		return true;
	}
	if (ExecuteServerSimulationFixtureCommand(cmd, rParams, rResult))
	{
		return true;
	}
	if (cmd == "query_frame")
	{
		CommandQueryFrame(rParams, rResult);
		return true;
	}
	if (cmd == "query_players")
	{
		CommandQueryPlayers(rParams, rResult);
		return true;
	}
	if (cmd == "query_collection")
	{
		CommandQueryCollection(rParams, rResult);
		return true;
	}
	if (cmd == "query_profile")
	{
		CommandQueryProfile(rParams, rResult);
		return true;
	}
	return false;
}

} // namespace game

#endif // defined(BT_SERVER)
