#pragma once

namespace engine
{

// Views into the report text ParseCrashReportHeader was given.
struct CrashReportHeader
{
	std::string_view gameName;
	std::string_view gameVersion;
	std::string_view exceptionLine;
};

void SetCrashReportAppDataDirectory(const wchar_t* pcDirectory);
void ResolveCrashReportPaths();
void RenamePreviousCrashReport();
// Returns true only for the call that wrote the report; every later call returns false at once.
bool HandleException(std::optional<const std::exception*> optionalException = std::nullopt);
void PromptCrashReportDesktopCopy();
// Call only from a catch handler: an attached debugger gets the handled exception back through a bare rethrow.
void HandleProcessMainException(std::optional<const std::exception*> optionalException = std::nullopt);
std::optional<CrashReportHeader> ParseCrashReportHeader(std::string_view report);
const wchar_t* ReportPath();
const wchar_t* PendingReportPath();
void ReadDxDiag();

#if defined(BT_SERVER)

// Posts crash reports as GitHub issues when the app-data folder holds a token. A posting task reads only mToken of this
// object; mToken is declared before the futures, whose destructors wait for every task, so it outlives them all.
class CrashReportPoster : public common::Singleton<CrashReportPoster>
{
public:

	CrashReportPoster();

	CrashReportPoster(const CrashReportPoster&) = delete;
	CrashReportPoster& operator=(const CrashReportPoster&) = delete;

	void PostAsync(std::filesystem::path reportPath);
	// Returns false after a WinHTTP failure, an HTTP status other than 201, 400, or 422, or an exception, so the startup
	// pass stops there; a report skipped as already posted or without a valid header, or tagged as posted after a 400 or
	// 422, returns true.
	bool Post(const std::filesystem::path& rReportPath) const;

	// Empty when posting is disabled. Written only by the constructor, before any posting task starts.
	std::string mToken;
	std::future<void> mStartupPass;
	std::vector<std::future<void>> mPosts;
};

inline CrashReportPoster* gpCrashReportPoster = nullptr;

void TryPostCrashReport();

#endif // BT_SERVER

} // namespace engine
