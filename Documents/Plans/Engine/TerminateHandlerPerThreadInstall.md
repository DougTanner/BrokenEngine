<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-29T15:25:06.310Z","dependsOn":[]} -->
# Install the terminate handler on every engine thread

## Context

`common::SetupExceptionHandling()` (`Common/Determinism.cpp:21-172`) has two
parts. Lines 23-65 install the per-thread CRT handler slots
(`_set_se_translator`, `_set_invalid_parameter_handler`) unconditionally, with
a comment at `Common/Determinism.cpp:23` stating that these are per-thread slots
and must be installed on every thread. Lines 67-171 install the process-global
handlers exactly once through `std::call_once` on `sOnceFlag`.

`std::set_terminate` is inside that once-block, at
`Common/Determinism.cpp:165-170`. Its handler logs `"std::set_terminate"`, calls
`DEBUG_BREAK()`, then `std::abort()`. MSVC's terminate handler is a per-thread
slot, not a process-global one, so placing it inside the once-block installs it
only on the first thread to reach `SetupExceptionHandling()` — in practice the
main thread, via `common::ThreadLocal threadLocal(...)` at
`Engine/Source/Main.cpp:78`. Every other engine thread runs with the CRT default
terminate handler.

Every engine-created worker does construct a `common::ThreadLocal`, whose
constructor calls `ConfigureThreadFloatingPoint()` and then
`SetupExceptionHandling()` when `bSetupExceptionHandling` is true
(`Common/Threading/ThreadLocal.cpp:6-23`); that parameter defaults to true
(`Common/Threading/ThreadLocal.h:39`) and no call site in the repository passes
false. Construction sites: `Common/Threading/PersistentWorker.cpp:10` (which
covers `common::Multithreading` dispatch workers),
`Engine/Source/File/PackChunks.cpp:427` and `:662`,
`Engine/Source/Graphics/Managers/TextureUploadManager.cpp:226`,
`Engine/Source/Graphics/Screenshot.cpp:115` and `:585`,
`Engine/Source/Agent/AgentCommandServer.cpp:119`,
`Engine/Source/CrashReport.cpp:170`, and `Engine/Source/Main.cpp:78`. So each of
these threads already runs the per-thread half of the function and would receive
the terminate handler if it were placed there.

Observed this session while investigating an unrelated change, on a
deliberately corrupted data directory: a failed
`ASSERT(static_cast<int64_t>(iLz4Result) == rLazyChunk.iDataSize)` at
`Engine/Source/File/PackChunks.cpp:808-809`, inside
`engine::PackChunks::LoadChunk`, running on a `PackChunks::LoadingThread` raw
`std::thread` started at `Engine/Source/File/PackChunks.cpp:481`. The assert
threw, nothing on that thread caught it, and the process terminated through the
CRT default terminate handler. Evidence: `Temp/client-agent-negB2.log` line 96
holds the assert text, and the whole log has zero occurrences of the handler's
`"std::set_terminate"` line; the crash report at
`Temp/AppData-negB/Broken Engine Sandbox/Broken-Engine-Sandbox-Crash-Report.txt`
is headlined "Unknown exception" and its embedded global log also lacks that
line. Reproduction used the client with
`--data-directory Temp/next-plan-tfcr/data-negB`. Those `Temp/` artifacts are
machine-local scratch and are not expected to survive.

Crash reporting itself was not lost: the CRT default handler still calls
`abort()`, and the process-wide `signal(SIGABRT, ...)` hook installed at
`Engine/Source/Memory/GlobalAllocator.cpp:95` still ran
`engine::HandleException()` and wrote the report. Its own comment there already
anticipates "std::terminate on a thread outside MainThread's try/catch". What is
lost is the handler's diagnostic log line and its `DEBUG_BREAK()`, so an
attached debugger does not stop at the terminate point on a worker thread and
the log carries no marker distinguishing a terminate from another abort.

This is pre-existing behavior, unrelated to and untouched by the session change
that observed it.

## Design

Recommendation: move the existing `std::set_terminate(...)` statement from
inside the `std::call_once` lambda (`Common/Determinism.cpp:165-170`) up into the
unconditional per-thread section of `SetupExceptionHandling()`, next to
`_set_se_translator` and `_set_invalid_parameter_handler`
(`Common/Determinism.cpp:23-65`), and extend the comment at
`Common/Determinism.cpp:23` to name `std::set_terminate` as a third per-thread
CRT handler slot. The handler lambda body is unchanged.

Rationale for preferring this over installing the handler at each thread entry
point: every engine-created thread already constructs a `common::ThreadLocal`
that calls `SetupExceptionHandling()`, so one moved statement covers all of them,
whereas per-thread-entry installation would add a repeated call at eight sites
and would silently miss any future thread. Re-installing on each thread is
cheap and idempotent per thread, matching the two handlers it joins.

Known limitation to record rather than solve here: a thread that never
constructs a `common::ThreadLocal` — an OS, driver, COM, or third-party library
thread, the category `Common/Log/Log.h:138` already describes — still uses the
CRT default handler. Covering those would require a different mechanism and is
out of scope.

## Critical files

- `Common/Determinism.cpp` — `SetupExceptionHandling()`; the per-thread section
  at lines 23-65 and the `std::set_terminate` call at lines 165-170 inside the
  `std::call_once` block.

## In scope

- Moving the existing `std::set_terminate` call in
  `common::SetupExceptionHandling()` out of the `std::call_once(sOnceFlag, ...)`
  lambda and into that function's unconditional per-thread section, leaving the
  handler lambda body byte-identical.
- Updating the per-thread handler comment at `Common/Determinism.cpp:23` so it
  names `std::set_terminate` alongside `_set_se_translator` and
  `_set_invalid_parameter_handler`.

## Out of scope

- The handler lambda's own behavior: its log text, `DEBUG_BREAK()`, and
  `std::abort()` stay exactly as they are.
- Everything else inside the `std::call_once` block: `_set_error_mode`,
  `SetErrorMode`, `_CrtSetReportMode`, and the vectored exception handler.
- `Engine/Source/File/PackChunks.cpp` — the failing LZ4 assert at lines 808-809
  and the `LoadingThread` structure are the reproduction vehicle, not the defect
  this Plan fixes; no change there.
- `Engine/Source/Memory/GlobalAllocator.cpp` and the `SIGABRT` /
  `engine::HandleException` crash-report path, which already work.
- `Common/Threading/ThreadLocal.{h,cpp}`, `PersistentWorker`, `Multithreading`,
  and every individual thread entry point — no new call, parameter, or
  installation site.
- Coverage for threads that never construct a `common::ThreadLocal`.
- Adding a `try`/`catch` to any worker thread, or changing how assert failures
  propagate.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped behavior in one subsystem's fault-path
handling). Trigger: the change alters process fault-path behavior on engine
worker threads. It is not Tier 3 — it touches no determinism/CRC value, no wire
or serialization format, no save/replay compatibility, and no trust boundary,
and it adds no thread, no synchronization, and no cross-subsystem coupling.

Preserve these invariants:

- The handler installed on a worker thread is the same one the main thread
  installs today: same log message, `DEBUG_BREAK()`, then `std::abort()`.
- `std::abort()` still reaches the `SIGABRT` hook at
  `Engine/Source/Memory/GlobalAllocator.cpp:95`, so the crash report is still
  written on every terminate path, on the main thread and on workers alike.
- The remaining `std::call_once` contents still install exactly once per
  process.
- Installation stays allocation-free and adds no work to any per-frame path;
  it runs only in `SetupExceptionHandling()`, i.e. once per thread at
  `ThreadLocal` construction.
- No behavior change on the main thread.

## Acceptance criteria

- Source inspection shows `std::set_terminate` called in the unconditional part
  of `common::SetupExceptionHandling()`, outside `std::call_once`, with the
  handler lambda body unchanged.
- Reproducing the observed failure — a client launched against a corrupted pack
  data directory so `PackChunks::LoadChunk` fails its LZ4 assert on a
  `LoadingThread` — produces a client log that now contains the
  `"std::set_terminate"` error line, where the pre-fix run
  (`Temp/client-agent-negB2.log`) contained zero. Recreate the corrupted data
  directory as part of the future implementation; the `Temp/` artifacts cited in
  `## Context` are machine-local and may be gone.
- The same run still writes the crash report through the existing `SIGABRT`
  path, so crash-report coverage is unchanged.
- Client and server `Debug|x64` builds pass through `/compile`.

## Notes

Proven during an unrelated session by a live harness run; recorded as a
pre-existing, out-of-scope leftover. Baseline commit
`d6084a2f6460a77275fb89a44ce87035a8339a94`. No source fix, build, or harness run
was performed while authoring this Plan.

The per-thread nature of the MSVC terminate handler is the load-bearing external
claim behind this Plan; the implementing session should confirm it against
current Microsoft `set_terminate` documentation via `/verify-external-claims`
before relying on it, since the in-repository evidence above shows only the
observed symptom.
