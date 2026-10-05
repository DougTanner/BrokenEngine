#pragma once

namespace common
{

// Shared base for the crash-path stack walkers: owns the emission gate, the DbgHelp error emit, and the
// process-wide DbgHelp serialization (dbghelp.dll is documented single-threaded per process — see
// gDbgHelpMutex in Determinism.h). Leaf classes implement only the Emit sink.
class FilteredStackWalker : public StackWalker
{
public:
	FilteredStackWalker(ExceptType eExceptType)
	: StackWalker(eExceptType)
	{
	}

	// Each driver serializes on gDbgHelpMutex; a contended try_to_lock skips the walk, and recursive locking permits a same-thread nested attempt.
	BOOL ShowCallstack()
	{
		std::unique_lock<std::recursive_mutex> uniqueLock(gDbgHelpMutex, std::try_to_lock);
		if (!uniqueLock.owns_lock())
		{
			Emit("<callstack skipped - DbgHelp busy>");
			return FALSE;
		}

		return StackWalker::ShowCallstack();
	}

protected:

	virtual void Emit(std::string_view text) = 0;

	void OnSymInit([[maybe_unused]] LPCSTR pcSearchPath, [[maybe_unused]] DWORD uiSymbolOptions, [[maybe_unused]] LPCSTR pcUserName) override
	{
	}

	void OnLoadModule([[maybe_unused]] LPCSTR pcImagePath, [[maybe_unused]] LPCSTR pcModuleName, [[maybe_unused]] DWORD64 uiBaseAddress, [[maybe_unused]] DWORD uiSize, [[maybe_unused]] DWORD uiResult, [[maybe_unused]] LPCSTR pcSymbolType, [[maybe_unused]] LPCSTR pcPdbName, [[maybe_unused]] ULONGLONG uiFileVersion) override
	{
	}

	void OnCallstackEntry(CallstackEntryType eType, CallstackEntry& rEntry) override
	{
		// Upstream re-delivers the final frame with eType == lastEntry after its regular emit — skip the duplicate.
		// _TRUNCATE prevents oversized frame text from invoking the throwing invalid-parameter handler on this fault path.
		if (eType == lastEntry)
		{
			return;
		}

		if (rEntry.name[0] == '\0')
		{
			return;
		}

		char pcLine[2'048] {};
		if (rEntry.lineNumber > 0)
		{
			_snprintf_s(pcLine, std::size(pcLine), _TRUNCATE, "%s | %lu | %s", rEntry.name, rEntry.lineNumber, rEntry.lineFileName);
		}
		else
		{
			// Named frame with no line info (release PDB / system DLL / inlined) — emit offset form instead of dropping
			_snprintf_s(pcLine, std::size(pcLine), _TRUNCATE, "%s + 0x%llX | %s", rEntry.name, rEntry.offsetFromSymbol, rEntry.moduleName);
		}

		Emit(pcLine);
	}

	void OnDbgHelpErr(LPCSTR pcFunctionName, DWORD uiLastError, DWORD64 uiAddress) override
	{
		// Per-frame line-lookup failures are expected with line-stripped PDBs and already visible as offset-form frames
		if (std::strcmp(pcFunctionName, "SymGetLineFromAddr64") == 0 || std::strcmp(pcFunctionName, "SymGetLineFromInlineContext") == 0)
		{
			return;
		}

		char pcLine[1'024] {};
		_snprintf_s(pcLine, std::size(pcLine), _TRUNCATE, "DbgHelp error: %s | %lu | 0x%llX", pcFunctionName, uiLastError, uiAddress);
		Emit(pcLine);
	}

	void OnOutput([[maybe_unused]] LPCSTR pcText) override
	{
	}
};

class LogStackWalker : public FilteredStackWalker
{
public:
	using FilteredStackWalker::FilteredStackWalker;

protected:

	void Emit(std::string_view text) override
	{
		// Fault-path emit: relies on Log()'s thread_local fallback buffer (GetLogFallbackBuffer) for race-freedom on
		// ThreadLocal-less faulting threads, and stays allocation-free.
		LOG(kDefault, kError, "{}", text);
	}
};

class CrashFileStackWalker : public FilteredStackWalker
{
public:
	CrashFileStackWalker(ExceptType eExceptType, CrashFileWriter* pWriter)
	: FilteredStackWalker(eExceptType)
	, mpWriter(pWriter)
	{
	}

protected:

	void Emit(std::string_view text) override
	{
		mpWriter->Write(text);
		mpWriter->Write("\n");
	}

	CrashFileWriter* mpWriter = nullptr;
};

} // namespace common
