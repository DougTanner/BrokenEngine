#include "WindowsUtils.h"

namespace common
{

// FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, ...) appends a trailing "\r\n" (often with a '.') and reports a
// length that includes it; trimming keeps single-line log output clean. Also NUL-terminates at the trim point.
static std::string_view TrimSystemMessage(std::span<char> buffer)
{
	int64_t iLength = std::ssize(buffer) - 1;
	while (iLength > 0 && (buffer[static_cast<size_t>(iLength - 1)] == '\r' || buffer[static_cast<size_t>(iLength - 1)] == '\n' || buffer[static_cast<size_t>(iLength - 1)] == '.' || buffer[static_cast<size_t>(iLength - 1)] == ' '))
	{
		--iLength;
	}

	buffer[static_cast<size_t>(iLength)] = 0;
	return std::string_view(buffer.data(), static_cast<size_t>(iLength));
}

std::string LastErrorString()
{
	char acReturn[MAX_PATH] {};
	int64_t iLength = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), acReturn, static_cast<DWORD>(std::size(acReturn)) - 1, nullptr);
	return std::string(TrimSystemMessage(std::span<char>(acReturn, static_cast<size_t>(iLength + 1))));
}

std::string HresultToString(HRESULT hresult)
{
	char acReturn[MAX_PATH] {};
	int64_t iLength = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, static_cast<DWORD>(hresult), 0, acReturn, static_cast<DWORD>(std::size(acReturn) - 1), nullptr);
	return std::string(TrimSystemMessage(std::span<char>(acReturn, static_cast<size_t>(iLength + 1))));
}

int64_t LogicalCoreCount()
{
	int64_t iLogicalCoreCount = std::thread::hardware_concurrency();
	if (iLogicalCoreCount == 0)
	{
		LOG(kDefault, kDebug, "std::thread::hardware_concurrency() returned 0");
		iLogicalCoreCount = 1;
	}

	return iLogicalCoreCount;
}

int64_t HardwareCoreCount()
{
	using LPFN_GLPI = BOOL(WINAPI*)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);
	HMODULE hmodule = GetModuleHandle(TEXT("kernel32"));
	if (hmodule == nullptr)
	{
		LOG(kDefault, kWarning, "GetModuleHandle(TEXT(\"kernel32\")) returned nullptr");
		return LogicalCoreCount();
	}

	LPFN_GLPI pGetLogicalProcessorInformation = reinterpret_cast<LPFN_GLPI>(GetProcAddress(hmodule, "GetLogicalProcessorInformation"));
	if (pGetLogicalProcessorInformation == nullptr)
	{
		LOG(kDefault, kWarning, "GetProcAddress(hmodule, \"GetLogicalProcessorInformation\") returned nullptr");
		return LogicalCoreCount();
	}

	std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer(1);
	DWORD uiReturnLength = static_cast<DWORD>(std::ssize(buffer) * static_cast<int64_t>(sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION)));

	BOOL bDone = FALSE;
	while (bDone == FALSE)
	{
		BOOL bSuccess = pGetLogicalProcessorInformation(buffer.data(), &uiReturnLength);

		if (bSuccess == FALSE)
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				int64_t iReturnLength = uiReturnLength;
				buffer.resize(static_cast<size_t>(iReturnLength / static_cast<int64_t>(sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION))));
			}
			else
			{
				LOG(kDefault, kWarning, "glpi GetLastError: {}", GetLastError());
				return LogicalCoreCount();
			}
		}
		else
		{
			bDone = TRUE;
		}
	}

	int64_t iProcessorCoreCount = 0;
	for (const SYSTEM_LOGICAL_PROCESSOR_INFORMATION& rSystemLogicalProcessorInformation : buffer)
	{
		if (rSystemLogicalProcessorInformation.Relationship == RelationProcessorCore)
		{
			++iProcessorCoreCount;
		}
	}

	if (iProcessorCoreCount >= 1)
	{
		return iProcessorCoreCount;
	}
	else
	{
		return LogicalCoreCount();
	}
}

std::tuple<std::string, std::string> FileTimeString(const std::filesystem::file_time_type& rFileTime)
{
	// file_time_type's clock has no standard layout/epoch relationship to FILETIME — aliasing one through the
	// other is UB that only works by MSVC-STL coincidence. Convert through system_clock (leap-second-naive, like
	// FileTimeToSystemTime) and rebuild the FILETIME from its 100ns tick count since the 1601 epoch.
	std::chrono::system_clock::time_point systemClockTime = std::chrono::clock_cast<std::chrono::system_clock>(rFileTime);
	uint64_t uiHundredNanosecondsSince1601 = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::duration<int64_t, std::ratio<1, 10'000'000>>>(systemClockTime.time_since_epoch()).count() + 116'444'736'000'000'000);
	FILETIME fileTime
	{
		.dwLowDateTime = static_cast<DWORD>(uiHundredNanosecondsSince1601 & 0xFFFFFFFF),
		.dwHighDateTime = static_cast<DWORD>(uiHundredNanosecondsSince1601 >> 32),
	};
	SYSTEMTIME systemTime {};
	VERIFY_SUCCESS(FileTimeToSystemTime(&fileTime, &systemTime));
	SYSTEMTIME localSystemTime {};
	VERIFY_SUCCESS(SystemTimeToTzSpecificLocalTime(nullptr, &systemTime, &localSystemTime));

	// Format failures leave the display strings empty.
	char pcDate[MAX_PATH] {};
	int64_t iWritten = GetDateFormat(LOCALE_USER_DEFAULT, 0, &localSystemTime, "yyyy-MM-dd", pcDate, static_cast<DWORD>(std::size(pcDate) - 1));
	if (iWritten == 0)
	{
		LOG(kDefault, kWarning, "GetDateFormat failed: {}", GetLastError());
	}

	char pcTime[MAX_PATH] {};
	iWritten = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &localSystemTime, "h:mm tt", pcTime, static_cast<DWORD>(std::size(pcTime) - 1));
	if (iWritten == 0)
	{
		LOG(kDefault, kWarning, "GetTimeFormat failed: {}", GetLastError());
	}

	return std::make_tuple(std::string(pcDate), std::string(pcTime));
}

std::optional<ExecutableResult> RunExecutable(const std::filesystem::path& rExecutableFile, std::wstring& rCommandLine)
{
	// RAII so each launch-failure early return below releases the handles / attribute list already acquired. Pipe and
	// process handles use a nullptr sentinel, so unique_ptr<void> (which skips the deleter on nullptr) fits.
	using ScopedHandle = std::unique_ptr<void, decltype([](HANDLE hHandle) noexcept
	{
		CloseHandle(hHandle);
	})>;

	SECURITY_ATTRIBUTES securityAttributes
	{
		.nLength = sizeof(SECURITY_ATTRIBUTES),
		.lpSecurityDescriptor = nullptr,
		.bInheritHandle = TRUE,
	};

	HANDLE hStandardInputPipeRead = nullptr;
	HANDLE hStandardInputPipeWrite = nullptr;
	if (!CreatePipe(&hStandardInputPipeRead, &hStandardInputPipeWrite, &securityAttributes, 0))
	{
		return std::nullopt;
	}
	ScopedHandle pStandardInputPipeRead(hStandardInputPipeRead);
	ScopedHandle pStandardInputPipeWrite(hStandardInputPipeWrite);

	HANDLE hStandardOutputPipeRead = nullptr;
	HANDLE hStandardOutputPipeWrite = nullptr;
	if (!CreatePipe(&hStandardOutputPipeRead, &hStandardOutputPipeWrite, &securityAttributes, 0))
	{
		return std::nullopt;
	}
	ScopedHandle pStandardOutputPipeRead(hStandardOutputPipeRead);
	ScopedHandle pStandardOutputPipeWrite(hStandardOutputPipeWrite);

	// Strip inheritance from parent-side pipe ends; the attribute list below only applies to the child-side two.
	if (!SetHandleInformation(hStandardInputPipeWrite, HANDLE_FLAG_INHERIT, 0))
	{
		return std::nullopt;
	}

	if (!SetHandleInformation(hStandardOutputPipeRead, HANDLE_FLAG_INHERIT, 0))
	{
		return std::nullopt;
	}

	// STARTUPINFOEX + PROC_THREAD_ATTRIBUTE_HANDLE_LIST whitelists exactly the two pipe ends the child needs.
	// Without this, the child inherits every HANDLE_FLAG_INHERIT=1 handle in the parent (log files, random
	// framework handles, etc.) — harmless for native C children like glslc, but .NET children like Gaea.Swarm
	// inspect inherited stdio handles during runtime startup and FailFast when they find unexpected extras.
	SIZE_T uiAttributeListSize = 0;
	InitializeProcThreadAttributeList(nullptr, 1, 0, &uiAttributeListSize);
	auto attributeListBuffer = std::make_unique<uint8_t[]>(uiAttributeListSize);
	LPPROC_THREAD_ATTRIBUTE_LIST pAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeListBuffer.get());
	_Analysis_assume_(pAttributeList != nullptr);
	if (!InitializeProcThreadAttributeList(pAttributeList, 1, 0, &uiAttributeListSize))
	{
		return std::nullopt;
	}
	// Tears down before attributeListBuffer frees (reverse declaration order) so the list outlives its backing.
	ScopedLambda attributeListGuard([pAttributeList]()
	{
		DeleteProcThreadAttributeList(pAttributeList);
	});
	HANDLE ahHandlesToInherit[] = { hStandardInputPipeRead, hStandardOutputPipeWrite };
	if (!UpdateProcThreadAttribute(pAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, ahHandlesToInherit, sizeof(ahHandlesToInherit), nullptr, nullptr))
	{
		return std::nullopt;
	}

	STARTUPINFOEXW extendedStartupInfo {};
	extendedStartupInfo.StartupInfo.cb = sizeof(STARTUPINFOEXW);
	extendedStartupInfo.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	extendedStartupInfo.StartupInfo.hStdInput = hStandardInputPipeRead;
	extendedStartupInfo.StartupInfo.hStdOutput = hStandardOutputPipeWrite;
	extendedStartupInfo.StartupInfo.hStdError = hStandardOutputPipeWrite;
	extendedStartupInfo.lpAttributeList = pAttributeList;

	PROCESS_INFORMATION processInformation {};
	if (!CreateProcessW(rExecutableFile.native().c_str(), rCommandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &extendedStartupInfo.StartupInfo, &processInformation))
	{
		return std::nullopt;
	}
	ScopedHandle pProcess(processInformation.hProcess);
	ScopedHandle pThread(processInformation.hThread);

	// Close the parent's child-side pipe ends now (before the read loop, not at scope exit) so ReadFile sees
	// EOF once the child exits; the child holds its own inherited duplicates.
	pStandardOutputPipeWrite.reset();
	pStandardInputPipeRead.reset();

	std::string output;
	char pcPipeOutput[1'024] {};
	DWORD uiBytesRead = 0;
	while (ReadFile(hStandardOutputPipeRead, pcPipeOutput, static_cast<DWORD>(sizeof(pcPipeOutput) - 1), &uiBytesRead, nullptr) == TRUE)
	{
		int64_t iBytesRead = uiBytesRead;
		pcPipeOutput[iBytesRead] = 0;
		output.append(pcPipeOutput, &pcPipeOutput[iBytesRead]);
	}

	WaitForSingleObject(processInformation.hProcess, INFINITE);
	DWORD uiExitCode = 0;
	GetExitCodeProcess(processInformation.hProcess, &uiExitCode);

	return ExecutableResult {.output = std::move(output), .iExitCode = static_cast<int64_t>(uiExitCode)};
}

ExecutableResult RunExecutableInNewConsole(const std::filesystem::path& rExecutableFile, std::wstring& rCommandLine)
{
	// CREATE_NEW_CONSOLE gives the child real console handles for stdin/stdout/stderr — required
	// for tools like Gaea.Swarm.exe that throw IOException("The handle is invalid") when their
	// console is a pipe or file. SW_HIDE keeps the new window off-screen so the bake doesn't
	// flash UI during a build.
	STARTUPINFOW wideStartupInfo {};
	wideStartupInfo.cb = sizeof(STARTUPINFOW);
	wideStartupInfo.dwFlags = STARTF_USESHOWWINDOW;
	wideStartupInfo.wShowWindow = SW_HIDE;

	PROCESS_INFORMATION processInformation {};
	VERIFY_SUCCESS(CreateProcessW(rExecutableFile.native().c_str(), rCommandLine.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr, &wideStartupInfo, &processInformation));

	WaitForSingleObject(processInformation.hProcess, INFINITE);

	DWORD uiExitCode = 0;
	GetExitCodeProcess(processInformation.hProcess, &uiExitCode);

	CloseHandle(processInformation.hThread);
	CloseHandle(processInformation.hProcess);

	return {.output = std::string {}, .iExitCode = static_cast<int64_t>(uiExitCode)};
}

} // namespace common
