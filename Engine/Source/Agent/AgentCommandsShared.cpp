#include "Pch.h"

#include "Agent/AgentCommandsShared.h"

#include "Agent/Commands/CellCoordinateProbe.h"
#include "Agent/Commands/CrashReportHarnessRig.h"

namespace engine
{

static common::LogLevel ParseLogLevel(std::string_view name)
{
	if (name == "Verbose")
	{
		return kVerbose;
	}
	if (name == "Debug")
	{
		return kDebug;
	}
	if (name == "Info")
	{
		return kInfo;
	}
	if (name == "Warning")
	{
		return kWarning;
	}
	if (name == "Error")
	{
		return kError;
	}
	throw std::runtime_error("unknown log level");
}

static const char* LogLevelName(common::LogLevel eLevel)
{
	switch (eLevel)
	{
		case kVerbose: return "Verbose";
		case kDebug: return "Debug";
		case kInfo: return "Info";
		case kWarning: return "Warning";
		case kError: return "Error";
	}
	return "Info";
}

static common::LogCategory ParseLogCategory(std::string_view name)
{
	for (int64_t i = 0; i < common::kiLogCategoryCount; ++i)
	{
		if (name == common::kpcLogCategoryNames[i])
		{
			return static_cast<common::LogCategory>(i);
		}
	}
	throw std::runtime_error("unknown log category");
}

// Collects up to iCount log lines from rpcLines into rLines. When bPattern, the whole buffer is scanned and the
// regex is applied per line before the count limit, so the last iCount *matching* lines are returned (chronological).
template <int64_t LINE_COUNT>
static void CollectLogLines(const char (&rpcLines)[LINE_COUNT][common::kiLogBufferSize], const std::atomic<int64_t>& riWritePosition, int64_t iCount, bool bPattern, const std::regex& rPattern, nlohmann::json& rLines)
{
	_Analysis_assume_(iCount >= 0);
	const char* pcLines[LINE_COUNT] {};
	int64_t iScan = bPattern ? LINE_COUNT : iCount;
	int64_t iFilled = common::LogTail(rpcLines, riWritePosition, pcLines, iScan);
	_Analysis_assume_(iFilled >= 0 && iFilled <= LINE_COUNT);

	const char* pcMatching[LINE_COUNT] {};
	int64_t iMatchCount = 0;
	for (int64_t i = 0; i < iFilled; ++i)
	{
		_Analysis_assume_(pcLines[i] != nullptr);
		if (!bPattern || std::regex_search(pcLines[i], rPattern))
		{
			pcMatching[iMatchCount++] = pcLines[i];
		}
	}

	int64_t iStart = std::max<int64_t>(0, iMatchCount - iCount);
	_Analysis_assume_(iStart >= 0 && iStart <= iMatchCount && iMatchCount <= LINE_COUNT);
	for (int64_t i = iStart; i < iMatchCount; ++i)
	{
		_Analysis_assume_(pcMatching[i] != nullptr);
		std::string_view line(pcMatching[i]);
		if (!line.empty() && line.back() == '\n')
		{
			line.remove_suffix(1);
		}
		rLines.push_back(std::string(line));
	}
}

static void CommandPing([[maybe_unused]] const nlohmann::json& rParameters, nlohmann::json& rResult, int64_t iGameTick)
{
#if defined(BT_CLIENT)
	rResult["build"] = "client";
#else
	rResult["build"] = "server";
#endif
	rResult["tick"] = iGameTick;
}

static void CommandQuit([[maybe_unused]] const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	rResult = nlohmann::json::object();
	engine::gbQuit.store(true, std::memory_order_relaxed);
}

static void CommandGetLogs(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	int64_t iCount = rParameters.contains("count") ? rParameters.at("count").get<int64_t>() : 64;
	if (iCount < 0)
	{
		iCount = 0;
	}

	bool bPattern = rParameters.contains("pattern");
	std::regex pattern;
	if (bPattern)
	{
		try
		{
			pattern.assign(rParameters.at("pattern").get<std::string>(), std::regex::ECMAScript);
		}
		catch (const std::regex_error&)
		{
			throw std::runtime_error("invalid regex pattern");
		}
	}

	nlohmann::json lines = nlohmann::json::array();
	if (rParameters.contains("category"))
	{
		common::LogCategory eCategory = ParseLogCategory(rParameters.at("category").get<std::string>());
		int64_t iCategory = static_cast<int64_t>(eCategory);
		CollectLogLines(common::gpcLogRingLines[iCategory], common::gpiLogRingWritePositions[iCategory], iCount, bPattern, pattern, lines);
	}
	else
	{
		// The wrapping cross-category ring always holds the most recent lines; the crash-dump global buffer freezes after its capacity, so it is unsuitable for a live tail.
		CollectLogLines(common::gpcLogAgentLines, common::giLogAgentWritePosition, iCount, bPattern, pattern, lines);
	}

	rResult["lines"] = std::move(lines);
}

static void CommandSetLogLevel(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!rParameters.contains("level"))
	{
		throw std::runtime_error("set_log_level requires 'level'");
	}
	common::LogLevel eLevel = ParseLogLevel(rParameters.at("level").get<std::string>());

	rResult = nlohmann::json::object();

	// LOG() compile-eliminates any line below a category's compile-time floor (kpeLogLevels), so storing a runtime
	// level below that floor can never actually emit. Clamp to the floor and report the effective level(s) back.
	auto ApplyClamped = [eLevel](common::LogCategory eCategory) -> common::LogLevel
	{
		common::LogLevel eFloor = kpeLogLevels[static_cast<int64_t>(eCategory)];
		common::LogLevel eEffective = eLevel < eFloor ? eFloor : eLevel;
		common::SetLogRuntimeLevel(eCategory, eEffective);
		return eEffective;
	};

	if (rParameters.contains("category"))
	{
		common::LogCategory eCategory = ParseLogCategory(rParameters.at("category").get<std::string>());
		rResult["effective"] = LogLevelName(ApplyClamped(eCategory));
	}
	else
	{
		nlohmann::json effective = nlohmann::json::object();
		for (int64_t i = 0; i < common::kiLogCategoryCount; ++i)
			effective[common::kpcLogCategoryNames[i]] = LogLevelName(ApplyClamped(static_cast<common::LogCategory>(i)));
		rResult["effective"] = std::move(effective);
	}
}

nlohmann::json AgentCoordinateJson(GridCoord coordinate)
{
	return nlohmann::json::array({coordinate.iQ, coordinate.iR});
}

nlohmann::json AgentLocalPositionJson(FXMVECTOR vecLocalPosition)
{
	return nlohmann::json::array({XMVectorGetX(vecLocalPosition), XMVectorGetY(vecLocalPosition), XMVectorGetZ(vecLocalPosition)});
}

int64_t AgentGridCoordinateValue(const nlohmann::json& rValue, std::string_view name)
{
	if (!rValue.is_number_integer())
	{
		throw std::runtime_error(std::format("{} must be an array of 2 integers", name));
	}

	if (rValue.is_number_unsigned())
	{
		if (!std::in_range<int32_t>(rValue.get<uint64_t>()))
		{
			throw std::runtime_error(std::format("{} values must fit in a signed 32-bit integer", name));
		}
		int64_t iValue = rValue.get<int64_t>();
		return iValue;
	}

	int64_t iValue = rValue.get<int64_t>();
	if (!std::in_range<int32_t>(iValue))
	{
		throw std::runtime_error(std::format("{} values must fit in a signed 32-bit integer", name));
	}
	return iValue;
}

GridCoord CoordinateFromParameter(const nlohmann::json& rParameters, std::string_view key)
{
	if (!rParameters.contains(key) || !rParameters.at(std::string(key)).is_array() || rParameters.at(std::string(key)).size() != 2)
	{
		std::string message("'");
		message.append(key);
		message.append("' must be a [q,r] array");
		throw std::runtime_error(message);
	}
	const nlohmann::json& rCoordinate = rParameters.at(std::string(key));
	std::string name = std::format("'{}'", key);
	return GridCoord {.iQ = static_cast<int32_t>(AgentGridCoordinateValue(rCoordinate.at(0), name)), .iR = static_cast<int32_t>(AgentGridCoordinateValue(rCoordinate.at(1), name))};
}

float AgentFloatValue(const nlohmann::json& rValue, std::string_view name)
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

bool ExecuteSharedAgentCommand(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult, int64_t iGameTick)
{
	if (command == "ping")
	{
		CommandPing(rParameters, rResult, iGameTick);
	}
	else if (command == "quit")
	{
		CommandQuit(rParameters, rResult);
	}
	else if (command == "get_logs")
	{
		CommandGetLogs(rParameters, rResult);
	}
	else if (command == "set_log_level")
	{
		CommandSetLogLevel(rParameters, rResult);
	}
	else if (command == "crash_report_harness_rig")
	{
		CommandCrashReportHarnessRig(rParameters, rResult);
	}
	else if (command == "cell_coordinate_probe")
	{
		CommandCellCoordinateProbe(rParameters, rResult);
	}
	else
	{
		return false;
	}
	return true;
}

} // namespace engine
