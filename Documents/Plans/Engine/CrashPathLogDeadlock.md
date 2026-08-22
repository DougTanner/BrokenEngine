<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-22T17:35:08.088Z","dependsOn":[]} -->
# The Crash Handler Logs Before Writing the Report and Can Self-Deadlock

## Context

`engine::HandleException` — the function that writes the crash report — still
calls `LOG` on one branch, before a single byte of the report has been written.
When the crashing thread already holds the log mutex, that call blocks forever on
a mutex the same thread owns, and the user gets no crash report at all.

Evidence, current source:

- `Engine/Source/CrashReport.cpp:74` — inside the `SHGetKnownFolderPath`
  failure branch of `HandleException`:
  `LOG(kDefault, kError, "SHGetKnownFolderPath(FOLDERID_RoamingAppData) failed (hresult {}); writing crash report to Desktop", ...)`.
  The first `ofstream` write of the report is at `CrashReport.cpp:92-94`, so this
  log runs strictly before any report bytes exist.
- `Common/Log/Log.cpp:119-121` — `LogWrite` takes `std::unique_lock lockGuard(gLogMutex)`.
  `gLogMutex` is a plain `std::mutex` (`Common/Log/Log.cpp:9`), so it is not
  recursive: a second lock on the same thread is undefined behavior and in
  practice deadlocks.
- `Common/Log/Log.cpp:129-134` — with a log file sink open, `LogWrite` also
  writes and flushes an `ofstream` under a `ScopedSuppressAllocationTracking`
  guard whose `// Heap:` comment states the sink may allocate. The crash path is
  reachable during heap corruption, so re-entering the allocator there can
  re-fault.
- `Engine/Source/Memory/GlobalAllocator.cpp:109` — debug builds install
  `signal(SIGABRT, [](int) { engine::HandleException(); })`, so an `abort()`
  (including a mimalloc heap assertion) enters `HandleException` on whatever
  thread aborted, with whatever locks that thread already held. A thread that
  aborts from inside `LogWrite`, or while any other `gLogMutex` holder is mid-
  emission on that same thread, therefore re-enters `gLogMutex`.
- `Engine/Source/AGENTS.md`, "Crash Reporting" — states the exception path must
  not log at all, precisely because the log mutex is not recursive, and cites the
  non-logging entry break `DEBUG_BREAK_NO_LOG()` as the reason. The surviving
  `LOG` at `CrashReport.cpp:74` contradicts that documented contract.

Pre-existing at the session baseline `2058c850eaf1c6c451d3a06c06b51ea99c5f285b`.
The session that recorded this Plan changed only the break at the entry of
`HandleException` (now `DEBUG_BREAK_NO_LOG()`); it did not add, move, or touch
this `LOG` call, which predates it. Found by a focused Codex/Sol review in that
session and out of that session's approved boundary.

Impact: on the specific combination of an OS `SHGetKnownFolderPath` failure and a
crashing thread that holds the log mutex, the process hangs instead of producing
the crash report — the exact scenario the crash reporter exists for. The branch
itself is rare, which is why this is recorded as debt rather than treated as
urgent.

## Design

The author's recommendation is to delete the `LOG` at `Engine/Source/CrashReport.cpp:74`
outright and keep the existing fallback behavior (fall back to the Desktop path,
which the following line already does). Rationale: the branch's only purpose is
diagnostic, the crash report itself is the diagnostic artifact this path exists
to produce, and `Engine/Source/AGENTS.md` already says nothing on this path may
log. Deleting it is the smallest change that removes the deadlock and restores
agreement between code and its documented contract; the existing comment on
lines 72-73 already explains the fallback in place, so no information is lost to
a reader.

If the implementing session judges the failure worth reporting to the user, the
author's recommended alternative is to record the failed `HRESULT` in a
fixed-size static buffer on this path and emit it into the report body itself,
below the first `ofstream` writes at `CrashReport.cpp:92-96`, where a plain
stream write is already the established mechanism and no lock or allocation is
involved. Do not defer it to a post-report `LOG`: by the time the report is
written the thread may still hold `gLogMutex`, so the deadlock would only move.

Do not make `gLogMutex` recursive as a way out. That would change the locking
contract for every logging call site in the codebase to work around one crash-
path caller, and `Common/Log/AGENTS.md` explicitly forbids introducing recursive
logging.

While in this function, check whether any other call on the crash path logs
indirectly — in particular anything reached through `common::OfstreamStackWalker`
(`CrashReport.cpp:108-109`) and the DxDiag completion read — and report what was
checked. Only fix what is proven reachable and proven to log.

## Critical files

- `Engine/Source/CrashReport.cpp:39-110` — `HandleException`, its
  `SHGetKnownFolderPath` failure branch, and the first report writes
- `Common/Log/Log.cpp:9`, `:119-135` — `gLogMutex` and `LogWrite` (read-only
  reference: the non-recursive lock and the allocating file sink)
- `Engine/Source/Memory/GlobalAllocator.cpp:109` — the `SIGABRT` entry point
  (read-only reference)
- `Engine/Source/AGENTS.md`, "Crash Reporting" — the no-logging, no-allocation
  contract this change restores

## In scope

- Removing or replacing the `LOG` call at `Engine/Source/CrashReport.cpp:74` so
  no code path inside `HandleException` takes `gLogMutex`
- If the `HRESULT` is preserved, the fixed-buffer capture plus the report-body
  write inside `HandleException` that emits it
- Auditing the remainder of `HandleException` for other reachable logging calls
  and removing any that are proven reachable

## Out of scope

- Making `gLogMutex` recursive, adding a try-lock path to `LogWrite`, or any
  other change to `Common/Log/Log.cpp` behavior
- The `DEBUG_BREAK` / `DEBUG_BREAK_NO_LOG` macros and `Common/ErrorUtils.{h,cpp}`
- Static-initialization-order exposure of the log file stream (a separate Plan)
- The crash report's format, the DxDiag reader's threading, the app-data override
  path handling, and `SetCrashReportAppDataDirectory`
- Whether `SIGABRT` should be hooked at all, and the debug-only gating around it

## Risk tier and invariants

Expected Tier 3. Trigger: the changed region is a re-entrancy and locking path
(root `AGENTS.md` lists threading as a Tier-3 surface), reachable from a signal
handler. The author's recommendation is that a session which deletes the single
`LOG` line and nothing else may present that evidence at Step 1 and ask to
classify lower; that decision belongs to the classifying session, not this Plan.

Invariants the change must not disturb:

- Nothing on this path allocates, formats through the heap, or follows a heap-
  owned pointer — it is reachable during heap corruption
- All path construction stays in the existing fixed buffers
- The report still lands at the same location on every existing branch, including
  the Desktop fallback taken when `SHGetKnownFolderPath` fails
- The agent-launched instance still takes the unprompted, non-modal branch

## Acceptance criteria

- No call reachable from `HandleException` takes `gLogMutex`, shown by reading
  the final function body
- Client and server `Debug|x64` build clean through `/compile`
- The `/agent-harness` crash-report fixture still produces a complete crash report
  file, exercising the normal (non-failing) path end to end

## Notes

- The failure needs two conditions at once — the OS call failing and the crashing
  thread already holding `gLogMutex` — so it has likely never been observed. It
  is recorded because the cost of the fix is one line and the failure mode is a
  silent hang in the reporter itself.
