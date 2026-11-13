#include "Log.h"

#include "AllocationTracking.h"

// Library-phase dynamic initialization: this TU is constructed before every default-phase (user) static and destroyed
// after them, so ordinary static initializers and destructors are inside the window where logging is safe.
#pragma init_seg(lib)

namespace common
{

static std::mutex sLogMutex;

static constinit char spcLogGlobalLines[kiGlobalBufferLineCount][kiLogBufferSize] {}; // Non-wrapping; freezes after kiGlobalBufferLineCount lines — feeds the crash-dump path (do not repurpose).
static constinit std::atomic<int64_t> siLogGlobalWritePosition = 0;

// Constant-initialized, so it reads false before this TU's dynamic initialization and after its destruction.
static constinit std::atomic<bool> sbLogAlive = false;

// Whole-stream file sink (Log.h EnableLogFile); teed by LogWrite under sLogMutex.
static std::ofstream sLogFileStream;

// Declaration order relative to sLogFileStream is load-bearing: dynamic init runs in declaration order within a TU and
// destruction in reverse, so declaring the guard after the stream opens the flag once the stream exists and closes it
// again before the stream is destroyed.
struct [[nodiscard]] LogAliveGuard
{
	LogAliveGuard()
	{
		sbLogAlive.store(true, std::memory_order_relaxed);
	}

	LogAliveGuard(const LogAliveGuard&) = delete;
	LogAliveGuard& operator=(const LogAliveGuard&) = delete;

	~LogAliveGuard()
	{
		sbLogAlive.store(false, std::memory_order_relaxed);
	}
};
static LogAliveGuard sLogAliveGuard;

void SetLogRuntimeLevel(LogCategory eCategory, LogLevel eLevel)
{
	gpeLogRuntimeLevels[static_cast<int64_t>(eCategory)].store(eLevel, std::memory_order_relaxed);
}

void EnableLogFile(const std::filesystem::path& rPath)
{
	bool bOpened = false;
	{
		std::unique_lock lockGuard(sLogMutex);
		sLogFileStream.open(rPath, std::ios::out | std::ios::trunc);
		bOpened = sLogFileStream.is_open();
	}

	// LOG tees through LogWrite, which locks sLogMutex — must log only after the lock above releases (sLogMutex is
	// non-recursive). Runs at startup before allocation tracking arms, so rPath.string() allocating is acceptable.
	if (!bOpened)
	{
		LOG(kDefault, kWarning, "EnableLogFile failed to open log file: {}", rPath.string());
	}
}

void LogIndent(int64_t iIndent)
{
	if constexpr (kbLogging)
	{
		if (gpThreadLocal != nullptr)
		{
			gpThreadLocal->miLogIndent += iIndent;
		}
	}
}

char* LogPrefix(char* pLogBuffer, char* pEnd)
{
	char* pWrite = pLogBuffer;

	auto Put = [&pWrite, pEnd](char c)
	{
		if (pWrite < pEnd)
		{
			*(pWrite++) = c;
		}
	};

	if (gpThreadLocal != nullptr) [[likely]]
	{
		int64_t iLogIndent = gpThreadLocal->miLogIndent;
		for (int64_t i = 0; i < iLogIndent; ++i)
		{
			Put(' ');
			Put(' ');
		}

		if (gpThreadLocal->mOptionalThreadId.has_value())
		{
			std::to_chars_result threadIdResult = std::to_chars(pWrite, pEnd, gpThreadLocal->mOptionalThreadId.value());
			pWrite = threadIdResult.ptr;

			Put(':');
			Put(' ');
		}

		if (gpThreadLocal->miLogTickCounter >= 0)
		{
			Put('[');
			Put('T');
			Put('i');
			Put('c');
			Put('k');
			Put(':');
			Put(' ');
			std::to_chars_result toCharsResult = std::to_chars(pWrite, pEnd, gpThreadLocal->miLogTickCounter);
			pWrite = toCharsResult.ptr;
			Put(']');
			Put(' ');
		}
	}
	else
	{
		Put('#');
		Put(':');
		Put(' ');
	}

	return pWrite;
}

void LogWrite(char* pLogBuffer)
{
	if (!sbLogAlive.load(std::memory_order_relaxed)) [[unlikely]]
	{
		// Outside this TU's lifetime sLogMutex and sLogFileStream may not exist, so emit without them: giMyOutputDebugString
		// is constant-initialized and EnableLogFile only ever runs inside the window, so the sink cannot be open here.
		DEBUG_BREAK_NO_LOG();
		++giMyOutputDebugString;
		OutputDebugString(pLogBuffer);
		--giMyOutputDebugString;
		if constexpr (kbAlsoLogToPrintf)
		{
			std::printf("%s", pLogBuffer);
		}
		return;
	}

	std::unique_lock lockGuard(sLogMutex);
	++giMyOutputDebugString;
	OutputDebugString(pLogBuffer);
	--giMyOutputDebugString;
	// Heap: console output and the file sink may allocate while emitting a tracked-loop log.
	ScopedSuppressAllocationTracking suppress;
	if constexpr (kbAlsoLogToPrintf)
	{
		std::printf("%s", pLogBuffer);
	}
	if (sLogFileStream.is_open())
	{
		sLogFileStream << pLogBuffer;
		sLogFileStream.flush();
	}
}

void LogWriteRingBuffers(std::string_view logBuffer, LogCategory eCategory)
{
	auto CopyToLine = [logBuffer](char* pLine)
	{
		if (pLine == nullptr)
		{
			return;
		}
		std::memcpy(pLine, logBuffer.data(), logBuffer.size());
	};

	int64_t iCategory = static_cast<int64_t>(eCategory);
	CopyToLine(LogAcquireLine<true>(gpcLogRingLines[iCategory], gpiLogRingWritePositions[iCategory]));
	CopyToLine(LogAcquireLine<false>(spcLogGlobalLines, siLogGlobalWritePosition));
	CopyToLine(LogAcquireLine<true>(gpcLogAgentLines, giLogAgentWritePosition));
}

void LogDumpBuffers(CrashFileWriter& rWriter)
{
	rWriter.Write("\n\n\n<Begin Global Log>\n");
	LogDump<false>(spcLogGlobalLines, siLogGlobalWritePosition, rWriter);
	rWriter.Write("<End Global Log>\n");

	for (int64_t i = 0; i < kiLogCategoryCount; ++i)
	{
		rWriter.Write("\n\n\n<Begin ");
		rWriter.Write(kpcLogCategoryNames[i]);
		rWriter.Write(" Log>\n");
		LogDump<true>(gpcLogRingLines[i], gpiLogRingWritePositions[i], rWriter);
		rWriter.Write("<End ");
		rWriter.Write(kpcLogCategoryNames[i]);
		rWriter.Write(" Log>\n");
	}
}

} // namespace common
