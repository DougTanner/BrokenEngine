#pragma once

#include "Threading/ThreadLocal.h"
#include "LogFormatters.h"
#include "LogTypes.h"

namespace common
{

// Category names for crash dump labels
inline constexpr const char* kpcLogCategoryNames[] = {"Default", "Temp", "Audio", "Graphics", "Loading", "NavData", "Network", "Input", "Replay"};
static_assert(std::size(kpcLogCategoryNames) == kiLogCategoryCount, "kpcLogCategoryNames out of sync with LogCategory");

// Nonzero while LogWrite emits its OutputDebugString, so the vectored exception handler does not re-log the engine's own debug string
inline constinit std::atomic<int64_t> giMyOutputDebugString = 0;
static_assert(decltype(giMyOutputDebugString)::is_always_lock_free);

// Log line buffers: fixed-size line storage plus a separate atomic write counter, written locklessly. The last byte of
// every line is always 0. A wrapping buffer reuses its oldest line; a non-wrapping one returns null once full.
template <bool WRAP, int64_t LINE_COUNT>
char* LogAcquireLine(char (&rpcLines)[LINE_COUNT][kiLogBufferSize], std::atomic<int64_t>& riWritePosition)
{
	int64_t iPosition = riWritePosition.fetch_add(1, std::memory_order_relaxed);
	if constexpr (!WRAP)
	{
		if (iPosition >= LINE_COUNT)
		{
			return nullptr;
		}
	}
	return rpcLines[iPosition % LINE_COUNT];
}

// Wrapping buffers only. Fills ppLines (caller capacity LINE_COUNT) with pointers to the most recent buffered lines in
// chronological order (oldest returned line first, newest last); iMaxLines is the requested scan cap. Returns the
// number of pointers written. Reuses LogAcquireLine's wrap math. Lockless/racy — a line being written concurrently may
// be torn and empty slots are skipped (same tolerance as the crash-dump LogDump path). Pass iMaxLines == LINE_COUNT to
// scan the whole buffer.
template <int64_t LINE_COUNT>
_Ret_range_(0, LINE_COUNT)
int64_t LogTail(const char (&rpcLines)[LINE_COUNT][kiLogBufferSize], const std::atomic<int64_t>& riWritePosition, _Out_writes_to_(LINE_COUNT, return) const char** ppLines, int64_t iMaxLines)
{
	int64_t iCurrentWritePosition = riWritePosition.load(std::memory_order_relaxed);
	int64_t iAvailable = std::min(iCurrentWritePosition, LINE_COUNT);
	int64_t iCount = std::min(iAvailable, iMaxLines);
	int64_t iFilled = 0;
	int64_t iFirst = iCurrentWritePosition - iCount;
	for (int64_t i = 0; i < iCount; ++i)
	{
		const char* pLine = rpcLines[(iFirst + i) % LINE_COUNT];
		if (pLine[0] != '\0')
		{
			ppLines[iFilled++] = pLine;
		}
	}
	return iFilled;
}

template <bool WRAP, int64_t LINE_COUNT>
void LogDump(const char (&rpcLines)[LINE_COUNT][kiLogBufferSize], const std::atomic<int64_t>& riWritePosition, CrashFileWriter& rWriter)
{
	int64_t iCurrentWritePosition = riWritePosition.load(std::memory_order_relaxed);
	if constexpr (WRAP)
	{
		int64_t iCount = std::min(iCurrentWritePosition, LINE_COUNT);
		int64_t iStart = (iCurrentWritePosition >= LINE_COUNT) ? (iCurrentWritePosition % LINE_COUNT) : 0;
		for (int64_t i = 0; i < iCount; ++i)
		{
			const char* pLine = rpcLines[(iStart + i) % LINE_COUNT];
			if (pLine[0] != '\0')
			{
				rWriter.Write(pLine);
			}
		}
	}
	else
	{
		int64_t iCount = std::min(iCurrentWritePosition, LINE_COUNT);
		for (int64_t i = 0; i < iCount; ++i)
		{
			if (rpcLines[i][0] != '\0')
			{
				rWriter.Write(rpcLines[i]);
			}
		}
	}
}

inline constexpr int64_t kiRingBufferLineCount = 128;
inline constexpr int64_t kiGlobalBufferLineCount = 1'024;
inline constexpr int64_t kiAgentBufferLineCount = 512; // Wrapping cross-category ring for the agent get_logs default. Half of the global buffer's line count so the added static footprint stays bounded (kiAgentBufferLineCount * kiLogBufferSize = 16 MiB).

inline constinit char gpcLogRingLines[kiLogCategoryCount][kiRingBufferLineCount][kiLogBufferSize] {};
inline constinit std::atomic<int64_t> gpiLogRingWritePositions[kiLogCategoryCount] {};
inline constinit char gpcLogAgentLines[kiAgentBufferLineCount][kiLogBufferSize] {}; // Wrapping; always holds the most recent lines across all categories for the agent get_logs default.
inline constinit std::atomic<int64_t> giLogAgentWritePosition = 0;
static_assert(decltype(giLogAgentWritePosition)::is_always_lock_free);

// Runtime per-category log threshold — checked by LOG() after compile-time filtering but before any argument
// formatting, so a suppressed line costs one relaxed atomic load plus a branch. Default kInfo (the documented
// out-of-box threshold; kVerbose in DataPacker); the agent lowers it live via set_log_level. Written from the
// main-thread agent Drain, read from every logging thread.
#if defined(BT_DATA_PACKER)
inline constexpr LogLevel keLogRuntimeDefault = kVerbose; // Offline tool: no agent / set_log_level, keep full verbosity
#else
inline constexpr LogLevel keLogRuntimeDefault = kInfo;
#endif
inline constinit std::atomic<LogLevel> gpeLogRuntimeLevels[kiLogCategoryCount]
{
	keLogRuntimeDefault, kVerbose, // [1] kTemp: transient agent/dev diagnostics — always emit (compile floor keLogLevelTemporary = kVerbose)
	keLogRuntimeDefault, keLogRuntimeDefault, keLogRuntimeDefault, keLogRuntimeDefault, keLogRuntimeDefault, keLogRuntimeDefault,
	keLogRuntimeDefault, // [8] kReplay: compiled in at kVerbose but silent by default — the agent lowers this category alone to scrape per-tick checksums
};
static_assert(std::size(gpeLogRuntimeLevels) == kiLogCategoryCount, "gpeLogRuntimeLevels out of sync with LogCategory");
void SetLogRuntimeLevel(LogCategory eCategory, LogLevel eLevel);

// Whole-stream log-file sink — opened once from startup (--log-file). LogWrite tees each emitted line here under
// the log mutex. Distinct from the selective per-file DiagnosticLog (FILE_LOG); the two coexist.
void EnableLogFile(const std::filesystem::path& rPath);

void LogIndent(int64_t iIndent);
char* LogPrefix(char* pLogBuffer, char* pEnd);
void LogWrite(char* pLogBuffer);
void LogWriteRingBuffers(std::string_view logBuffer, LogCategory eCategory);
void LogDumpBuffers(CrashFileWriter& rWriter);

// Single per-thread fallback buffer for threads without a ThreadLocal (early startup; OS / driver / COM / gamepad
// threads). Hoisted out of the Log() template so there is exactly one kiLogBufferSize thread_local per thread rather
// than one per template instantiation. thread_local makes it race-free across threads, and a zero-init POD array is
// safe at the earliest startup (no dynamic init).
inline char* GetLogFallbackBuffer()
{
	static thread_local char spcLogBuffer[kiLogBufferSize] {};
	return spcLogBuffer;
}

// Unfiltered log writer — called by LOG() macro after compile-time filtering
template <typename... TUV>
void Log(LogCategory eCategory, std::format_string<const TUV&...> format, const TUV&... parameters)
{
	char* pLogBuffer = gpThreadLocal != nullptr ? gpThreadLocal->mpLogBuffer : GetLogFallbackBuffer();
	char* pEnd = pLogBuffer + kiLogBufferSize - 2; // Reserve 2 bytes for the trailing '\n' and '\0'.
	char* pWrite = LogPrefix(pLogBuffer, pEnd);

	if (eCategory == LogCategory::kTemp)
	{
		static constexpr const char kpcTempPrefix[] = "kTemp: ";
		int64_t iTempLength = std::min(static_cast<int64_t>(sizeof(kpcTempPrefix) - 1), pEnd - pWrite);
		std::memcpy(pWrite, kpcTempPrefix, iTempLength);
		pWrite += iTempLength;
	}

	std::format_to_n_result<char*> formatResult = std::format_to_n(pWrite, pEnd - pWrite, format, parameters...);
	pWrite = formatResult.out;

	*(pWrite++) = '\n';
	*(pWrite++) = 0;

	LogWriteRingBuffers(std::string_view(pLogBuffer, static_cast<size_t>(pWrite - pLogBuffer)), eCategory);
	LogWrite(pLogBuffer);
}

class [[nodiscard]] ScopedLogIndent
{
public:

	ScopedLogIndent()
	{
		if constexpr (kbLogging)
		{
			LogIndent(1);
		}
	}

	~ScopedLogIndent()
	{
		if constexpr (kbLogging)
		{
			LogIndent(-1);
		}
	}

	ScopedLogIndent(const ScopedLogIndent&) = delete;
	ScopedLogIndent& operator=(const ScopedLogIndent&) = delete;
};

} // namespace common

using common::Log;
using common::LogIndent;
using common::ScopedLogIndent;

// Per-category compile-time thresholds (keLogLevel* defined before Common.h by each project's Pch.h; the game's come from Engine/Source/EngineSwitches.h, which its Pch.h includes)
inline constexpr LogLevel kpeLogLevels[]
{
	keLogLevelDefault, keLogLevelTemporary,
	keLogLevelAudio, keLogLevelGraphics, keLogLevelLoading, keLogLevelNavigationData, keLogLevelNetwork, keLogLevelInput, keLogLevelReplay,
};
static_assert(std::size(kpeLogLevels) == common::kiLogCategoryCount, "kpeLogLevels out of sync with LogCategory");

// Log.cpp initializes in the library phase for ordinary static initializers and destructors; LogWrite
// handles calls outside its lifetime without the log mutex or the file sink.
#define LOG(category, level, format, ...) \
do { \
	if constexpr (kbLogging && (level) >= kpeLogLevels[static_cast<int64_t>(category)]) \
	{ \
		if ((level) >= common::gpeLogRuntimeLevels[static_cast<int64_t>(category)].load(std::memory_order_relaxed)) \
		{ \
			common::Log((category), format __VA_OPT__(,) __VA_ARGS__); \
		} \
	} \
} while (false)
