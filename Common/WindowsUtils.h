#pragma once

namespace common
{

std::string LastErrorString();

std::string HresultToString(HRESULT hresult);

// Converts the file time to the user's local timezone and formats it as yyyy-MM-dd and h:mm tt.
std::tuple<std::string, std::string> FileTimeString(const std::filesystem::file_time_type& rFileTime);

struct ExecutableResult
{
	std::string mOutput;
	int64_t miExitCode = 0;
};

// DataPacker uses this for external tools; CreateProcessW mutates rCommandLine, and captured
// stdout/stderr are combined in mOutput.
ExecutableResult RunExecutable(const std::filesystem::path& rExecutableFile, std::wstring& rCommandLine);

// Gaea.Swarm.exe requires real console handles, so the child runs synchronously with
// CREATE_NEW_CONSOLE; output is not captured and mOutput remains empty. CreateProcessW mutates
// rCommandLine, so it must stay mutable.
ExecutableResult RunExecutableInNewConsole(const std::filesystem::path& rExecutableFile, std::wstring& rCommandLine);

// Falls back to 1 when std::thread::hardware_concurrency() reports 0.
int64_t LogicalCoreCount();

// Counts RelationProcessorCore entries, grows the query buffer on ERROR_INSUFFICIENT_BUFFER, and falls
// back to LogicalCoreCount() when the API fails or finds no cores.
int64_t HardwareCoreCount();

} // namespace common
