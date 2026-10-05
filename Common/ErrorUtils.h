#pragma once

namespace common
{

void Assert(bool bCondition, std::string_view expression, std::source_location sourceLocation = std::source_location::current());
void CheckHresult(int64_t iHresult, std::string_view expression, std::source_location sourceLocation = std::source_location::current());

// Out-of-line so DEBUG_BREAK() needs only this declaration: the macro is used from Common headers that are parsed
// before Log.h defines LOG. The default argument resolves at the DEBUG_BREAK() call site.
void LogDebugBreak(std::source_location sourceLocation = std::source_location::current());

} // namespace common

// Breaks without logging, for a path that must not take the log mutex or allocate — the log mutex is not recursive, so a
// crash handler reached while this thread already holds it would deadlock instead of writing its report.
#define DEBUG_BREAK_NO_LOG() do { if constexpr (kbDebugBreak) { if (IsDebuggerPresent() == TRUE) { __debugbreak(); } } } while (false)
// Calls LOG at warning level even when no debugger is attached.
#define DEBUG_BREAK() do { common::LogDebugBreak(); DEBUG_BREAK_NO_LOG(); } while (false)
#define ASSERT(a) do { bool bAssertMacro = a; if (!bAssertMacro) [[unlikely]] { common::Assert(bAssertMacro, #a); } _Analysis_assume_(bAssertMacro); } while (false)
#define CHECK_HRESULT(a) do { int64_t iHresultMacro = a; if (iHresultMacro < 0) [[unlikely]] { common::CheckHresult(iHresultMacro, #a); } _Analysis_assume_(iHresultMacro >= 0); } while (false)
#define VERIFY_SUCCESS(a) do { bool bReturnMacro = a; if (!bReturnMacro) [[unlikely]] { common::Assert(bReturnMacro, #a); } _Analysis_assume_(bReturnMacro); } while (false)
