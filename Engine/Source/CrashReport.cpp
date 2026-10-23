#include "CrashReport.h"

#include "Game.h"

namespace engine
{

static std::string sDxDiag;
static constinit std::atomic<bool> sbDxDiagComplete(false);
static_assert(decltype(sbDxDiagComplete)::is_always_lock_free);
static wchar_t spcAppDataOverride[MAX_PATH + 1] {};
static constinit wchar_t spcDesktopReportPath[MAX_PATH + 1] {};
static constinit wchar_t spcUserReportPath[MAX_PATH + 1] {};
static constinit wchar_t spcPendingReportPath[MAX_PATH + 1] {};
constexpr wchar_t kpcFallbackReportPath[] = L"Crash-Report.txt";
constexpr wchar_t kpcFallbackPendingReportPath[] = L"Crash-Report-Pending.txt";
// The user-profile path in UTF-8 and in the ANSI code page, each empty when its conversion failed or did not fit. A
// UTF-16 unit takes at most three bytes in either encoding.
static constinit char spcUserProfileUtf8[3 * MAX_PATH + 1] {};
static constinit char spcUserProfileAnsi[3 * MAX_PATH + 1] {};
static constinit std::atomic<bool> sbReportWritten(false);
static_assert(decltype(sbReportWritten)::is_always_lock_free);

// HandleException writes this header, and ParseCrashReportHeader matches it.
constexpr char kpcCrashReportPreamble[] = "\n\n\nPlease send this crash report to brokenteapotstudios@gmail.com, and if possible describe exactly what you were doing when it occurred.\n";
constexpr char kpcGameNamePrefix[] = "Game name: ";
constexpr char kpcGameVersionPrefix[] = "Game version: ";
constexpr char kpcCrashReportVersionPrefix[] = "Crash report version: ";
constexpr char kpcExceptionSeparator[] = "\n\n\n";
// Bump on any change to the header or the report layout, so reports in the older format are rejected.
constexpr int64_t kiCrashReportVersion = 1;

static const wchar_t* ReportPath()
{
	// Startup resolution failed when the buffer is empty; the fallback path is relative to the working directory.
	return spcUserReportPath[0] != L'\0' ? spcUserReportPath : kpcFallbackReportPath;
}

static const wchar_t* PendingReportPath()
{
	// Same fallback rule as ReportPath.
	return spcPendingReportPath[0] != L'\0' ? spcPendingReportPath : kpcFallbackPendingReportPath;
}

void SetCrashReportAppDataDirectory(const wchar_t* pcDirectory)
{
	spcAppDataOverride[0] = L'\0';

	wchar_t pcGameName[64] {};
	swprintf_s(pcGameName, std::size(pcGameName), L"%hs", game::kGameName.data());
	wchar_t pcCrashReportFile[128] {};
	swprintf_s(pcCrashReportFile, std::size(pcCrashReportFile), L"\\%s-Crash-Report.txt", pcGameName);

	int64_t iDirectoryLength = wcsnlen_s(pcDirectory, std::size(spcAppDataOverride));
	int64_t iSuffixLength = 1 + static_cast<int64_t>(std::wcslen(pcGameName)) + static_cast<int64_t>(std::wcslen(pcCrashReportFile));
	if (iDirectoryLength >= std::ssize(spcAppDataOverride))
	{
		return;
	}

	if (iSuffixLength >= std::ssize(spcAppDataOverride) - iDirectoryLength)
	{
		return;
	}

	wcscpy_s(spcAppDataOverride, std::size(spcAppDataOverride), pcDirectory);
}

// Appends only after proving the result fits: wcscat_s on an overflowing input invokes the invalid-parameter handler,
// which would terminate this otherwise healthy process at startup. A non-fitting append empties the candidate so its
// buffer is either the complete intended path or empty, never a partial one the crash handler would write to.
static bool AppendReportPath(wchar_t (&rBuffer)[MAX_PATH + 1], const wchar_t* pcText)
{
	int64_t iUsedLength = wcsnlen_s(rBuffer, std::size(rBuffer));
	int64_t iTextLength = wcsnlen_s(pcText, std::size(rBuffer));
	if (iTextLength >= std::ssize(rBuffer) - iUsedLength)
	{
		rBuffer[0] = L'\0';
		return false;
	}

	wcscat_s(rBuffer, std::size(rBuffer), pcText);
	return true;
}

// A conversion that fails or does not fit leaves the buffer empty, which disables its replacement in the report.
static void StoreUserProfilePath(int64_t iCodePage, const wchar_t* pcUserProfile, char (&rBuffer)[3 * MAX_PATH + 1])
{
	if (WideCharToMultiByte(static_cast<UINT>(iCodePage), 0, pcUserProfile, -1, rBuffer, static_cast<int>(std::size(rBuffer)), nullptr, nullptr) == 0)
	{
		rBuffer[0] = '\0';
	}
}

void ResolveCrashReportPaths()
{
	spcDesktopReportPath[0] = L'\0';
	spcUserReportPath[0] = L'\0';
	spcPendingReportPath[0] = L'\0';
	spcUserProfileUtf8[0] = '\0';
	spcUserProfileAnsi[0] = '\0';

	wchar_t pcGameName[64] {};
	swprintf_s(pcGameName, std::size(pcGameName), L"%hs", game::kGameName.data());
	wchar_t pcCrashReportFile[128] {};
	swprintf_s(pcCrashReportFile, std::size(pcCrashReportFile), L"%s-Crash-Report.txt", pcGameName);
	std::replace(std::begin(pcCrashReportFile), std::end(pcCrashReportFile), L' ', L'-');
	wchar_t pcPendingReportFile[128] {};
	swprintf_s(pcPendingReportFile, std::size(pcPendingReportFile), L"%s-Crash-Report-Pending.txt", pcGameName);
	std::replace(std::begin(pcPendingReportFile), std::end(pcPendingReportFile), L' ', L'-');

	PWSTR pcUserProfile = nullptr;
	int64_t iUserProfileHresult = SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &pcUserProfile);
	if (SUCCEEDED(static_cast<HRESULT>(iUserProfileHresult)) && pcUserProfile != nullptr)
	{
		StoreUserProfilePath(CP_UTF8, pcUserProfile, spcUserProfileUtf8);
		StoreUserProfilePath(CP_ACP, pcUserProfile, spcUserProfileAnsi);
	}
	CoTaskMemFree(pcUserProfile);

	wchar_t pcDesktopDirectory[MAX_PATH + 1] {};
	bool bDesktopFound = SHGetSpecialFolderPathW(HWND_DESKTOP, pcDesktopDirectory, CSIDL_DESKTOP, FALSE) != FALSE;
	if (bDesktopFound && AppendReportPath(spcDesktopReportPath, pcDesktopDirectory) && AppendReportPath(spcDesktopReportPath, L"\\"))
	{
		AppendReportPath(spcDesktopReportPath, pcCrashReportFile);
	}

	if (spcAppDataOverride[0] != L'\0')
	{
		AppendReportPath(spcUserReportPath, spcAppDataOverride);
	}
	else
	{
		PWSTR pcWideCharacter = nullptr;
		int64_t iHresult = SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &pcWideCharacter);
		if (SUCCEEDED(static_cast<HRESULT>(iHresult)) && pcWideCharacter != nullptr)
		{
			AppendReportPath(spcUserReportPath, pcWideCharacter);
		}
		else if (bDesktopFound)
		{
			// OS failure on the roaming lookup: fall back to the Desktop so the report still lands somewhere writable.
			AppendReportPath(spcUserReportPath, pcDesktopDirectory);
		}
		CoTaskMemFree(pcWideCharacter);
	}

	if (spcUserReportPath[0] != L'\0' && AppendReportPath(spcUserReportPath, L"\\") && AppendReportPath(spcUserReportPath, pcGameName))
	{
		// Allocator-free single-level create (parent guaranteed present above): FileManager creates the same directory but
		// is constructed later, so a crash before that still needs it. Ignore the result — ERROR_ALREADY_EXISTS is fine.
		CreateDirectoryW(spcUserReportPath, nullptr);

		if (AppendReportPath(spcUserReportPath, L"\\"))
		{
			wcscpy_s(spcPendingReportPath, std::size(spcPendingReportPath), spcUserReportPath);
			AppendReportPath(spcUserReportPath, pcCrashReportFile);
			AppendReportPath(spcPendingReportPath, pcPendingReportFile);
		}
	}
}

void RenamePreviousCrashReport()
{
	// Exactly one empty buffer means the two paths resolved from different bases, and a rename would move the report
	// between the app-data folder and the working-directory-relative fallback.
	if ((spcUserReportPath[0] == L'\0') != (spcPendingReportPath[0] == L'\0'))
	{
		return;
	}

	WIN32_FILE_ATTRIBUTE_DATA fileAttributeData {};
	if (GetFileAttributesExW(ReportPath(), GetFileExInfoStandard, &fileAttributeData) == FALSE)
	{
		return;
	}

	if (fileAttributeData.nFileSizeHigh == 0 && fileAttributeData.nFileSizeLow == 0)
	{
		return;
	}

	if (MoveFileExW(ReportPath(), PendingReportPath(), MOVEFILE_REPLACE_EXISTING) == FALSE)
	{
		LOG(kDefault, kWarning, "RenamePreviousCrashReport Failed to rename the previous crash report Error: {}", static_cast<int64_t>(GetLastError()));
	}
}

bool HandleException(std::optional<const std::exception*> optionalException)
{
	// The first writer owns the report: ProcessMain's catch after the main-loop catch, or a SIGABRT during the upload
	// or unwinding, must not rewrite the report that may already be uploaded.
	if (sbReportWritten.exchange(true))
	{
		return false;
	}

	// Non-logging break: this runs from the SIGABRT handler during heap corruption, possibly while this thread already
	// holds the log mutex, so logging here would deadlock or re-fault before any crash-report bytes are written.
	DEBUG_BREAK_NO_LOG();

	// The path and the replaced user-profile prefixes are already resolved: this runs from the SIGABRT handler during
	// heap corruption, so no path lookup, directory creation, or string building may happen here —
	// ResolveCrashReportPaths did all of it at startup.
	common::CrashFileWriter writer(ReportPath(), spcUserProfileUtf8, spcUserProfileAnsi);

	writer.Write(kpcCrashReportPreamble);

	char pcLine[256] {};
	std::snprintf(pcLine, std::size(pcLine), "%s%.*s\n", kpcGameNamePrefix, static_cast<int>(game::kGameName.size()), game::kGameName.data());
	writer.Write(pcLine);
	std::snprintf(pcLine, std::size(pcLine), "%s%lld\n", kpcGameVersionPrefix, static_cast<long long>(game::kiGameVersion));
	writer.Write(pcLine);
	std::snprintf(pcLine, std::size(pcLine), "%s%lld\n", kpcCrashReportVersionPrefix, static_cast<long long>(kiCrashReportVersion));
	writer.Write(pcLine);

	writer.Write(kpcExceptionSeparator);
	if (optionalException.has_value())
	{
		writer.Write(optionalException.value()->what());
	}
	else
	{
		writer.Write("Unknown exception");
	}
	writer.Write("\n");

	writer.Write("\n\n\n<Begin callstack>\n");
	common::CrashFileStackWalker crashFileStackWalker(StackWalker::AfterCatch, &writer);
	crashFileStackWalker.ShowCallstack();
	writer.Write("<End callstack>\n");

	writer.Write("\n\n\n<Begin DxDiag>\n");
	// The DxDiag thread writes sDxDiag without a lock. The agent harness rig and SIGABRT handler can run before it completes,
	// so read the string only after completion is published; the section markers remain unconditional.
	if (sbDxDiagComplete.load(std::memory_order_acquire))
	{
		writer.Write(sDxDiag.c_str());
	}
	writer.Write("<End DxDiag>\n");

	common::LogDumpBuffers(writer);
	return true;
}

// Consumes prefix and the field after it up to the next newline, and returns that field.
static std::optional<std::string_view> ReadCrashReportHeaderLine(std::string_view& rRest, std::string_view prefix)
{
	if (!rRest.starts_with(prefix))
	{
		return std::nullopt;
	}
	rRest.remove_prefix(prefix.size());

	size_t uiLineEnd = rRest.find('\n');
	if (uiLineEnd == std::string_view::npos)
	{
		return std::nullopt;
	}

	std::string_view field = rRest.substr(0, uiLineEnd);
	rRest.remove_prefix(uiLineEnd + 1);
	return field;
}

static std::optional<int64_t> ParseCrashReportDecimal(std::string_view field)
{
	int64_t iValue = 0;
	auto [pEnd, eError] = std::from_chars(field.data(), field.data() + field.size(), iValue);
	if (eError != std::errc() || pEnd != field.data() + field.size())
	{
		return std::nullopt;
	}

	return iValue;
}

std::optional<CrashReportHeader> ParseCrashReportHeader(std::string_view report)
{
	if (!report.starts_with(kpcCrashReportPreamble))
	{
		return std::nullopt;
	}
	std::string_view rest = report.substr(std::size(kpcCrashReportPreamble) - 1);

	std::optional<std::string_view> optionalGameName = ReadCrashReportHeaderLine(rest, kpcGameNamePrefix);
	if (!optionalGameName.has_value() || optionalGameName->empty())
	{
		return std::nullopt;
	}

	std::optional<std::string_view> optionalGameVersion = ReadCrashReportHeaderLine(rest, kpcGameVersionPrefix);
	if (!optionalGameVersion.has_value() || !ParseCrashReportDecimal(*optionalGameVersion).has_value())
	{
		return std::nullopt;
	}

	std::optional<std::string_view> optionalReportVersion = ReadCrashReportHeaderLine(rest, kpcCrashReportVersionPrefix);
	if (!optionalReportVersion.has_value() || ParseCrashReportDecimal(*optionalReportVersion) != kiCrashReportVersion)
	{
		return std::nullopt;
	}

	// HandleException writes the exception text, then a newline, right after the separator.
	std::optional<std::string_view> optionalExceptionLine = ReadCrashReportHeaderLine(rest, kpcExceptionSeparator);
	if (!optionalExceptionLine.has_value())
	{
		return std::nullopt;
	}

	return CrashReportHeader
	{
		.gameName = *optionalGameName,
		.gameVersion = *optionalGameVersion,
		.exceptionLine = *optionalExceptionLine,
	};
}

void PromptCrashReportDesktopCopy()
{
	// An empty Desktop path leaves no copy target. A client agent launch must never block on a modal dialog; the server
	// has no user to ask, so it always copies.
	if (spcDesktopReportPath[0] == L'\0')
	{
		return;
	}

#if defined(BT_CLIENT)
	if (gLaunchOptions.iAgentPort != 0)
	{
		return;
	}

	if (MessageBox(nullptr, "Save crash report to desktop?", game::kGameName.data(), MB_YESNO | MB_SYSTEMMODAL) != IDYES)
	{
		return;
	}
#endif

	// Nothing on the crash path may recover or log a failed copy.
	CopyFileW(ReportPath(), spcDesktopReportPath, FALSE);
}

#if defined(BT_CLIENT) && !defined(BT_MODEL_VIEWER)

// A report counts as sent only when it ends with this trailer. HandleException truncates the report it writes, so
// every new report starts unsent.
constexpr char kpcCrashReportSentTrailer[] = "\nThis crash report was sent to the server.\n";
constexpr int64_t kiCrashReportSentTrailerSize = std::ssize(kpcCrashReportSentTrailer) - 1;

// Set by OnCrashReportPacketFreed once the server has acknowledged a whole crash report packet. Main thread only.
static bool sbPendingCrashReportDelivered = false;

// Runs inside ENet calls (Client::Poll, the UploadCrashReport wait, teardown), so it only sets the flag. ENet sets
// ENET_PACKET_FLAG_SENT only when the acknowledgment of the packet's last fragment frees it; a reset or a timeout
// frees it without the flag.
static void OnCrashReportPacketFreed(ENetPacket* pPacket)
{
	if ((pPacket->flags & ENET_PACKET_FLAG_SENT) != 0)
	{
		sbPendingCrashReportDelivered = true;
	}
}

// Returns the number of leading report bytes read into buffer; 0 when the report is missing or empty.
static int64_t ReadCrashReport(const wchar_t* pcReportPath, std::span<uint8_t> buffer)
{
	std::fstream reportStream(std::filesystem::path(pcReportPath), std::ios::in | std::ios::binary);
	reportStream.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(std::ssize(buffer)));
	return static_cast<int64_t>(reportStream.gcount());
}

static bool AppendCrashReportSentTrailer(const wchar_t* pcReportPath)
{
	std::fstream reportStream(std::filesystem::path(pcReportPath), std::ios::out | std::ios::binary | std::ios::app);
	reportStream.write(kpcCrashReportSentTrailer, kiCrashReportSentTrailerSize);
	reportStream.close();
	return !reportStream.fail();
}

void TryUploadCrashReport()
{
	try
	{
		if (gpClient == nullptr)
		{
			return;
		}

		if (!(gpClient->mStateFlags & Client::ClientStateFlags::kConnectionAccepted))
		{
			return;
		}

		// The server counts a second report on one connection as a rate violation and does not store it; this report
		// stays unsent and goes on the next accepted connection.
		if (gpClient->mStateFlags & Client::ClientStateFlags::kCrashReportSent)
		{
			return;
		}

		// Heap: the report path conversion, file streams, and ENet servicing allocate; this runs once, on the crash path.
		ScopedSuppressAllocationTracking suppress;
		common::ScopedWorkbufferAllocation<uint8_t*> reportAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<uint8_t*>(kiMaximumCrashReportSize);
		int64_t iReadSize = ReadCrashReport(ReportPath(), std::span<uint8_t>(reportAllocation.mpData, static_cast<size_t>(kiMaximumCrashReportSize)));
		if (iReadSize == 0)
		{
			return;
		}

		sbPendingCrashReportDelivered = false;
		gpClient->UploadCrashReport(std::span<const uint8_t>(reportAllocation.mpData, static_cast<size_t>(iReadSize)), &OnCrashReportPacketFreed);
		if (!sbPendingCrashReportDelivered)
		{
			LOG(kNetwork, kWarning, "TryUploadCrashReport Not delivered Bytes: {}", iReadSize);
			return;
		}

		LOG(kNetwork, kInfo, "TryUploadCrashReport Delivered Bytes: {}", iReadSize);
		if (!AppendCrashReportSentTrailer(ReportPath()))
		{
			LOG(kNetwork, kWarning, "TryUploadCrashReport Failed to tag the report as sent");
		}
	}
	// A failed upload must not replace the crash being handled.
	catch (...)
	{
	}
}

void PollPendingCrashReport(Client* pClient)
{
	// Runs before the pClient check: the delivery is observed inside a Client::Poll after this point, and the session
	// may destroy that Client before the next call.
	if (sbPendingCrashReportDelivered)
	{
		sbPendingCrashReportDelivered = false;
		// Heap: the report path conversion and file stream allocate; this runs at most once per delivered report.
		ScopedSuppressAllocationTracking suppress;
		if (AppendCrashReportSentTrailer(PendingReportPath()))
		{
			LOG(kNetwork, kInfo, "PollPendingCrashReport Tagged the report as sent");
		}
		else
		{
			LOG(kNetwork, kWarning, "PollPendingCrashReport Failed to tag the report as sent");
		}
	}

	if (pClient == nullptr)
	{
		return;
	}

	if (!(pClient->mStateFlags & Client::ClientStateFlags::kConnectionAccepted))
	{
		return;
	}

	if (pClient->mStateFlags & Client::ClientStateFlags::kCrashReportChecked)
	{
		return;
	}
	pClient->mStateFlags.Set(Client::ClientStateFlags::kCrashReportChecked);

	// Heap: the report path conversion and file streams allocate; this runs at most once per accepted connection.
	ScopedSuppressAllocationTracking suppress;
	std::fstream reportStream(std::filesystem::path(PendingReportPath()), std::ios::in | std::ios::binary | std::ios::ate);
	// tellg is -1 when no report exists.
	int64_t iFileSize = static_cast<int64_t>(reportStream.tellg());
	if (iFileSize <= 0)
	{
		return;
	}

	if (iFileSize >= kiCrashReportSentTrailerSize)
	{
		char pcTail[kiCrashReportSentTrailerSize] {};
		reportStream.seekg(iFileSize - kiCrashReportSentTrailerSize);
		reportStream.read(pcTail, kiCrashReportSentTrailerSize);
		if (reportStream.fail())
		{
			LOG(kNetwork, kWarning, "PollPendingCrashReport Failed to read the report");
			return;
		}

		if (std::string_view(pcTail, std::size(pcTail)) == std::string_view(kpcCrashReportSentTrailer, std::size(pcTail)))
		{
			return;
		}
	}
	reportStream.close();

	common::ScopedWorkbufferAllocation<uint8_t*> reportAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<uint8_t*>(kiMaximumCrashReportSize);
	int64_t iReadSize = ReadCrashReport(PendingReportPath(), std::span<uint8_t>(reportAllocation.mpData, static_cast<size_t>(kiMaximumCrashReportSize)));
	if (iReadSize == 0)
	{
		LOG(kNetwork, kWarning, "PollPendingCrashReport Failed to read the report");
		return;
	}

	// The server rejects a report without this build's header as corrupt input; it stays here until the next rename.
	if (!ParseCrashReportHeader(std::string_view(reinterpret_cast<const char*>(reportAllocation.mpData), static_cast<size_t>(iReadSize))).has_value())
	{
		LOG(kNetwork, kWarning, "PollPendingCrashReport Not sent, no crash report header of this version File: {}", std::filesystem::path(PendingReportPath()).filename());
		return;
	}

	pClient->SendCrashReport(std::span<const uint8_t>(reportAllocation.mpData, static_cast<size_t>(iReadSize)), &OnCrashReportPacketFreed);
	LOG(kNetwork, kInfo, "PollPendingCrashReport Sent unsent report Bytes: {}", iReadSize);
}

#endif // BT_CLIENT && !BT_MODEL_VIEWER

#if defined(BT_SERVER)

// A report counts as posted only when it ends with this trailer, appended once GitHub has created its issue.
constexpr char kpcCrashReportPostedTrailer[] = "\nThis crash report was posted as a GitHub issue.\n";
constexpr int64_t kiCrashReportPostedTrailerSize = std::ssize(kpcCrashReportPostedTrailer) - 1;
constexpr int64_t kiIssueTitleMaximumSize = 256;
// GitHub rejects an issue body over 65,536 characters, and a UTF-8 text never has more characters than bytes, so the
// shown report and its two fences fit in this and leave room for the lines around them.
constexpr int64_t kiIssueReportBudget = 60'000;
// WinHTTP times out resolve, connect, send, and receive separately, so one request takes at most four times this.
constexpr std::chrono::milliseconds kIssueRequestPhaseTimeout = 15s;
// WinHTTP defines no status macro for 422 (Unprocessable Content).
constexpr DWORD kuiHttpStatusUnprocessableContent = 422;

CrashReportPoster::CrashReportPoster()
: common::Singleton<CrashReportPoster>(gpCrashReportPoster, this)
{
	std::string token = common::GetFileOrStringContent(gpFileManager->mAppDataDirectory / L"GitHubCrashReportToken.txt").second;
	static constexpr std::string_view kWhitespace = " \t\r\n";
	std::string_view trimmedToken = token;
	while (!trimmedToken.empty() && kWhitespace.contains(trimmedToken.front()))
	{
		trimmedToken.remove_prefix(1);
	}
	while (!trimmedToken.empty() && kWhitespace.contains(trimmedToken.back()))
	{
		trimmedToken.remove_suffix(1);
	}

	if (trimmedToken.empty())
	{
		LOG(kNetwork, kInfo, "CrashReportPoster Crash reports are not posted: no GitHub token");
		return;
	}
	mToken = trimmedToken;
	LOG(kNetwork, kInfo, "CrashReportPoster Posting crash reports as GitHub issues");

	// RenamePreviousCrashReport has already moved the previous run's report to the pending file. The list is complete
	// before Server exists, so no report this run stores is on it, and the live report never is.
	std::vector<std::filesystem::path> startupReportPaths;
	std::error_code errorCode;
	if (std::filesystem::exists(PendingReportPath(), errorCode))
	{
		startupReportPaths.emplace_back(PendingReportPath());
	}
	for (const std::filesystem::directory_entry& rEntry : std::filesystem::directory_iterator(gpFileManager->mAppDataDirectory, errorCode))
	{
		std::filesystem::path fileName = rEntry.path().filename();
		if (fileName.native().starts_with(L"CrashReport-") && fileName.native().ends_with(L".txt"))
		{
			startupReportPaths.push_back(rEntry.path());
		}
	}

	// Heap: the std::async shared state and the worker thread escape into the startup pass.
	ScopedSuppressAllocationTracking suppress;
	mStartupPass = std::async(std::launch::async, common::ThreadLocal::Entry([this, startupReportPaths = std::move(startupReportPaths)]()
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		// Heap: the report reads, the issue body, and the WinHTTP request allocate on this background thread.
		ScopedSuppressAllocationTracking suppressPass;
		for (const std::filesystem::path& rReportPath : startupReportPaths)
		{
			// The failed report and every later one stay for the next start.
			if (!Post(rReportPath))
			{
				return;
			}
		}
	}, common::kiMinWorkbufferSize));
}

void CrashReportPoster::PostAsync(std::filesystem::path reportPath)
{
	if (mToken.empty())
	{
		return;
	}

	// Heap: the std::async shared state, the worker thread, and mPosts growth escape this call.
	ScopedSuppressAllocationTracking suppress;
	// Destroying a ready future does not block.
	std::erase_if(mPosts, [](const std::future<void>& rPost)
	{
		return rPost.wait_for(0s) == std::future_status::ready;
	});
	mPosts.push_back(std::async(std::launch::async, common::ThreadLocal::Entry([this, reportPath = std::move(reportPath)]()
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		// Heap: the report read, the issue body, and the WinHTTP request allocate on this background thread.
		ScopedSuppressAllocationTracking suppressPost;
		std::ignore = Post(reportPath);
	}, common::kiMinWorkbufferSize)));
}

// The largest size up to iMaximumSize that does not end inside a UTF-8 multi-byte sequence.
static int64_t Utf8CutSize(std::string_view text, int64_t iMaximumSize)
{
	if (std::ssize(text) <= iMaximumSize)
	{
		return std::ssize(text);
	}

	int64_t iSize = iMaximumSize;
	// A continuation byte (10xxxxxx) at the cut belongs to the code point before it.
	while (iSize > 0 && (static_cast<uint8_t>(text[static_cast<size_t>(iSize)]) & 0xC0) == 0x80)
	{
		--iSize;
	}
	return iSize;
}

// Server::ClientCrashReport stores a client report as CrashReport-<UTC>-<count>-<32 hex player GUID>.txt.
static std::optional<std::string_view> StoredCrashReportPlayerId(std::string_view fileName)
{
	std::string_view stem = fileName.substr(0, fileName.size() - std::string_view(".txt").size());
	std::string_view playerId = stem.substr(stem.rfind('-') + 1);
	bool bHexDigits = std::ranges::all_of(playerId, [](char cCharacter)
	{
		return std::string_view("0123456789ABCDEFabcdef").contains(cCharacter);
	});
	if (playerId.size() != 32 || !bHexDigits)
	{
		return std::nullopt;
	}

	return playerId;
}

bool CrashReportPoster::Post(const std::filesystem::path& rReportPath) const
{
	// TryPostCrashReport runs on the crash path, where a failed post must not replace the crash being handled.
	try
	{
		std::string fileName = common::ToString(rReportPath.filename().native());

		std::fstream reportStream(rReportPath, std::ios::in | std::ios::binary | std::ios::ate);
		// tellg is -1 when the report cannot be opened.
		int64_t iFileSize = static_cast<int64_t>(reportStream.tellg());
		if (iFileSize >= kiCrashReportPostedTrailerSize)
		{
			char pcTail[kiCrashReportPostedTrailerSize] {};
			reportStream.seekg(iFileSize - kiCrashReportPostedTrailerSize);
			reportStream.read(pcTail, kiCrashReportPostedTrailerSize);
			if (std::string_view(pcTail, std::size(pcTail)) == std::string_view(kpcCrashReportPostedTrailer, std::size(pcTail)))
			{
				return true;
			}
		}

		std::string report(static_cast<size_t>(std::max(iFileSize, 0i64)), '\0');
		reportStream.seekg(0);
		reportStream.read(report.data(), static_cast<std::streamsize>(std::ssize(report)));
		report.resize(static_cast<size_t>(reportStream.gcount()));
		reportStream.close();

		std::optional<CrashReportHeader> optionalHeader = ParseCrashReportHeader(report);
		if (!optionalHeader.has_value())
		{
			LOG(kNetwork, kWarning, "CrashReportPoster::Post Not posted, no crash report header of this version File: {}", fileName);
			return true;
		}

		std::string title = std::format("{} {}: {}", optionalHeader->gameName, optionalHeader->gameVersion, optionalHeader->exceptionLine);
		title.resize(static_cast<size_t>(Utf8CutSize(title, kiIssueTitleMaximumSize)));

		// The fence is longer than every backtick run in the shown report, so report text cannot close it. The shown
		// report grows while it and both fences fit the budget; the fence only grows with it, so the first byte that
		// does not fit ends the shown report.
		int64_t iShownSize = 0;
		int64_t iFenceSize = 3;
		int64_t iBacktickRun = 0;
		for (char cCharacter : report)
		{
			iBacktickRun = cCharacter == '`' ? iBacktickRun + 1 : 0;
			int64_t iNextFenceSize = std::max(iFenceSize, iBacktickRun + 1);
			if (iShownSize + 1 + 2 * iNextFenceSize > kiIssueReportBudget)
			{
				break;
			}
			++iShownSize;
			iFenceSize = iNextFenceSize;
		}
		// Cutting back to a code-point boundary only shortens the shown report, so the fence stays longer than its runs.
		iShownSize = Utf8CutSize(report, iShownSize);
		std::string_view shownReport = std::string_view(report).substr(0, static_cast<size_t>(iShownSize));
		std::string fence(static_cast<size_t>(iFenceSize), '`');

		std::optional<std::string_view> optionalPlayerId = StoredCrashReportPlayerId(fileName);
		std::string body = optionalPlayerId.has_value() ? std::format("Player ID: {}\n", *optionalPlayerId) : std::string("Source: server\n");
		body += std::format("File: {}\n\n{}\n{}\n{}\n", fileName, fence, shownReport, fence);
		if (iShownSize < std::ssize(report))
		{
			body += std::format("Report truncated: {} of {} bytes; the full report stays on the server as {}.\n", iShownSize, std::ssize(report), fileName);
		}

		nlohmann::json issue;
		issue["title"] = title;
		issue["body"] = body;
		issue["labels"] = nlohmann::json::array({"crash-report", std::string(game::kGameName)});
		std::string request = issue.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);

		std::wstring headers = L"Authorization: Bearer ";
		for (char cCharacter : mToken)
		{
			headers.push_back(static_cast<wchar_t>(static_cast<uint8_t>(cCharacter)));
		}
		headers += L"\r\nAccept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\nContent-Type: application/json\r\n";

		auto LogFailedCall = [](std::string_view call)
		{
			int64_t iError = static_cast<int64_t>(GetLastError());
			LOG(kNetwork, kWarning, "CrashReportPoster::Post {} failed Error: {}", call, iError);
			return false;
		};

		using ScopedInternetHandle = std::unique_ptr<void, decltype([](HINTERNET hinternet) noexcept
		{
			WinHttpCloseHandle(hinternet);
		})>;
		wchar_t pcUserAgent[64] {};
		swprintf_s(pcUserAgent, std::size(pcUserAgent), L"%hs", game::kGameName.data());
		// The static proxy setting runs no proxy discovery, so the phase timeouts below bound the whole request.
		ScopedInternetHandle pSession(WinHttpOpen(pcUserAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
		if (pSession == nullptr)
		{
			return LogFailedCall("WinHttpOpen");
		}

		ScopedInternetHandle pConnection(WinHttpConnect(pSession.get(), L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0));
		if (pConnection == nullptr)
		{
			return LogFailedCall("WinHttpConnect");
		}

		ScopedInternetHandle pRequest(WinHttpOpenRequest(pConnection.get(), L"POST", L"/repos/DougTanner/Coretide/issues", nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
		if (pRequest == nullptr)
		{
			return LogFailedCall("WinHttpOpenRequest");
		}

		int64_t iPhaseTimeoutMilliseconds = kIssueRequestPhaseTimeout.count();
		if (WinHttpSetTimeouts(pRequest.get(), static_cast<int>(iPhaseTimeoutMilliseconds), static_cast<int>(iPhaseTimeoutMilliseconds), static_cast<int>(iPhaseTimeoutMilliseconds), static_cast<int>(iPhaseTimeoutMilliseconds)) == FALSE)
		{
			return LogFailedCall("WinHttpSetTimeouts");
		}

		if (WinHttpSendRequest(pRequest.get(), headers.c_str(), static_cast<DWORD>(headers.size()), request.data(), static_cast<DWORD>(request.size()), static_cast<DWORD>(request.size()), 0) == FALSE)
		{
			return LogFailedCall("WinHttpSendRequest");
		}

		if (WinHttpReceiveResponse(pRequest.get(), nullptr) == FALSE)
		{
			return LogFailedCall("WinHttpReceiveResponse");
		}

		// Only the status is read, never the response body, so the receive timeout covers one read.
		DWORD uiStatusCode = 0;
		DWORD uiStatusCodeSize = sizeof(uiStatusCode);
		if (WinHttpQueryHeaders(pRequest.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &uiStatusCode, &uiStatusCodeSize, WINHTTP_NO_HEADER_INDEX) == FALSE)
		{
			return LogFailedCall("WinHttpQueryHeaders");
		}

		if (uiStatusCode == HTTP_STATUS_CREATED)
		{
			LOG(kNetwork, kInfo, "CrashReportPoster::Post Posted File: {}", fileName);
		}
		// 400 and 422 reject the request itself, and every byte after a stored client report's header is the client's, so
		// GitHub would reject the report the same way on every retry; tagging it as posted keeps it from blocking later
		// reports.
		else if (uiStatusCode == HTTP_STATUS_BAD_REQUEST || uiStatusCode == kuiHttpStatusUnprocessableContent)
		{
			LOG(kNetwork, kWarning, "CrashReportPoster::Post Not posted, GitHub rejected the request File: {} Status: {}", fileName, static_cast<int64_t>(uiStatusCode));
		}
		else
		{
			LOG(kNetwork, kWarning, "CrashReportPoster::Post Not posted Status: {}", static_cast<int64_t>(uiStatusCode));
			return false;
		}

		std::fstream trailerStream(rReportPath, std::ios::out | std::ios::binary | std::ios::app);
		trailerStream.write(kpcCrashReportPostedTrailer, kiCrashReportPostedTrailerSize);
		trailerStream.close();
		if (trailerStream.fail())
		{
			LOG(kNetwork, kWarning, "CrashReportPoster::Post Failed to tag the report as posted File: {}", fileName);
		}
		return true;
	}
	catch (...)
	{
		LOG(kNetwork, kWarning, "CrashReportPoster::Post Failed with an exception");
		return false;
	}
}

void TryPostCrashReport()
{
	if (gpCrashReportPoster->mToken.empty())
	{
		return;
	}

	// Heap: the report path, the file streams, the issue body, and the WinHTTP request allocate; this runs once, on the
	// crash path.
	ScopedSuppressAllocationTracking suppress;
	std::ignore = gpCrashReportPoster->Post(ReportPath());
}

#endif // BT_SERVER

static bool DxDiagCallFailed(int64_t iHresult, std::string_view call)
{
	if (SUCCEEDED(static_cast<HRESULT>(iHresult)))
	{
		return false;
	}

	// Heap: HresultToString returns a std::string, and this thread participates in main-loop allocation tracking.
	ScopedSuppressAllocationTracking suppress;
	LOG(kDefault, kError, "Failed to read DxDiag: {} failed: {}", call, common::HresultToString(static_cast<HRESULT>(iHresult)).data());
	return true;
}

void ReadDxDiag()
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

	// Declared before the try so every ComPtr releases first and completion publishes once, after the last sDxDiag write.
	common::ScopedLambda publishComplete([]()
	{
		sbDxDiagComplete.store(true, std::memory_order_release);
	});

	try
	{
		if (DxDiagCallFailed(CoInitialize(nullptr), "CoInitialize"))
		{
			return;
		}

		Microsoft::WRL::ComPtr<IDxDiagProvider> pIdxDiagProvider;
		if (DxDiagCallFailed(CoCreateInstance(CLSID_DxDiagProvider, nullptr, CLSCTX_INPROC_SERVER, IID_IDxDiagProvider, reinterpret_cast<void**>(pIdxDiagProvider.GetAddressOf())), "CoCreateInstance"))
		{
			return;
		}

		DXDIAG_INIT_PARAMS dxdiagInitParams
		{
			.dwSize = sizeof(DXDIAG_INIT_PARAMS),
			.dwDxDiagHeaderVersion = DXDIAG_DX9_SDK_VERSION,
			.bAllowWHQLChecks = FALSE,
			.pReserved = nullptr,
		};
		if (DxDiagCallFailed(pIdxDiagProvider->Initialize(&dxdiagInitParams), "IDxDiagProvider::Initialize"))
		{
			return;
		}

		Microsoft::WRL::ComPtr<IDxDiagContainer> pRoot;
		if (DxDiagCallFailed(pIdxDiagProvider->GetRootContainer(&pRoot), "GetRootContainer"))
		{
			return;
		}

		Microsoft::WRL::ComPtr<IDxDiagContainer> pDisplayDevices;
		if (DxDiagCallFailed(pRoot->GetChildContainer(L"DxDiag_DisplayDevices", &pDisplayDevices), "GetChildContainer(DxDiag_DisplayDevices)"))
		{
			return;
		}

		DWORD uiChildCount = 0;
		if (DxDiagCallFailed(pDisplayDevices->GetNumberOfChildContainers(&uiChildCount), "GetNumberOfChildContainers"))
		{
			return;
		}
		int64_t iChildCount = uiChildCount;
		LOG(kDefault, kDebug, "DxDiag found {} children", iChildCount);
		for (int64_t i = 0; i < iChildCount; ++i)
		{
			WCHAR pcChildName[256] {};
			if (DxDiagCallFailed(pDisplayDevices->EnumChildContainerNames(static_cast<DWORD>(i), pcChildName, 256), "EnumChildContainerNames"))
			{
				return;
			}
			Microsoft::WRL::ComPtr<IDxDiagContainer> pChild;
			if (DxDiagCallFailed(pDisplayDevices->GetChildContainer(pcChildName, &pChild), "GetChildContainer"))
			{
				return;
			}

			// Best-effort per-prop reads (GetNumberOfProps / EnumPropNames / GetProp / VariantClear) leave results
			// unchecked; zero-initializing each output keeps a failed call harmless, and only a GetProp result of VT_BSTR is
			// appended to the report.
			DWORD uiPropCount = 0;
			pChild->GetNumberOfProps(&uiPropCount);
			int64_t iPropCount = uiPropCount;
			LOG(kDefault, kDebug, "    {} props", iPropCount);
			for (int64_t j = 0; j < iPropCount; ++j)
			{
				WCHAR pcPropName[256] {};
				pChild->EnumPropNames(static_cast<DWORD>(j), pcPropName, static_cast<DWORD>(std::size(pcPropName) - 1));

				VARIANT variant {};
				pChild->GetProp(pcPropName, &variant);
				if (variant.vt == VT_BSTR && variant.bstrVal != nullptr)
				{
					// Heap: one-shot background startup enumeration on this dedicated thread, not main-loop work — it builds a
					// persistent crash-report string whose length depends on the machine's device properties, so neither the
					// ToString conversions nor sDxDiag's growth can be sized up front or moved to a Workbuffer.
					ScopedSuppressAllocationTracking suppress;

					sDxDiag += common::ToString(pcPropName);
					sDxDiag += ": ";
					sDxDiag += common::ToString(variant.bstrVal);
					sDxDiag += "\n";
				}
				VariantClear(&variant);
			}
		}
	}
	catch ([[maybe_unused]] const std::exception& rException)
	{
		LOG(kDefault, kError, "Failed to read DxDiag: {}", rException.what());
	}
}

} // namespace engine
