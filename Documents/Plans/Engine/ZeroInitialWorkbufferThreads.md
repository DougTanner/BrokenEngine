<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-20T16:05:17.568Z","dependsOn":[]} -->
# Give the loader and screenshot threads a non-zero initial workbuffer size

## Context

Four threads construct their `common::ThreadLocal` with a zero initial
workbuffer size, so the thread's workbuffer starts with no committed bytes at
all:

- `Engine/Source/File/PackChunkLoader.cpp:230` — the lazy chunk loading threads,
  `common::ThreadLocal threadLocal(0, common::kThreadLazyLoad)`.
- `Engine/Source/File/PackChunks.cpp:472` — the eager loading `std::async`
  thread, `common::ThreadLocal threadLocal(0, common::kThreadEagerLoad)`.
- `Engine/Source/Graphics/Screenshot.cpp:112` — the screenshot encoder thread.
- `Engine/Source/Graphics/Screenshot.cpp:582` — the render-target dump encoder
  thread.

`ThreadLocal`'s constructor sizes the workbuffer to exactly that argument
(`Common/Threading/ThreadLocal.cpp:9,14`), so the first formatted log line on
such a thread that routes through the workbuffer has nowhere to write and calls
`Workbuffer::Grow` (`Common/Workbuffer.cpp`), which exists only to report an
under-sized buffer: it fires `DEBUG_BREAK()` unconditionally and, with a frame
open, logs `kError` "Workbuffer under-sized".

That is reached on every agent screenshot. The encoder thread logs the output
file at `Engine/Source/Graphics/Screenshot.cpp:173`
(`LOG(kGraphics, kDebug, "  {}", filename)`) with a `std::filesystem::path`
argument, and that formatter pushes a workbuffer arena and appends the path
(`Common/Log/LogFormatters.h:88-104`), so depth is 1 when `Grow` runs: a debug
client breaks into the debugger and a Release client writes a `kError` line for
a screenshot that succeeded. Separately, the first `Workbuffer::Append(int64_t)`
or `AppendFloat` on such a thread forms its `std::to_chars` range from a null
`Data()` with length 0 (`Common/Workbuffer.cpp:38-52,55-70`) before taking the
same grow path — well defined, but only because the range is empty.

This is pre-existing: all four zero arguments are present at the baseline
commit, where `Grow` additionally relocated the backing bytes and invalidated
every live handle. The reserve-then-commit storage change keeps the grow
in place, so nothing is corrupted today, but the `DEBUG_BREAK()` and the `kError`
line remain and a buffer that is under-sized by construction still signals the
condition the signal exists to catch. It surfaced as a narrowing of the second
acceptance check of
`Documents/Plans/Engine/WorkbufferGrowthHandleSafety.md`, which had to exclude
zero-initial-size threads, and that Plan's `## Out of scope` bars "Changing any
initial size or pre-allocate constant".

## Design

Pass a non-zero initial workbuffer size at each of the four constructions and
change nothing else.

The author recommends `4 * 1024` at all four sites, the same initial size the
`DataPacker` export-job threads use
(`DataPacker/Source/ExportJobs/ExportJob.cpp:214`), for two reasons. It is
comfortably above the largest single payload any of these four threads formats
through the workbuffer — a `std::filesystem::path`, bounded near `MAX_PATH` — so
no site starts under-sized. And the derived address-space reservation does not
move: `ThreadLocal` computes it as 64 times the initial size with that size
floored at 64 KiB first (`Common/Threading/ThreadLocal.h:38-41`;
`Common/Threading/ThreadLocal.cpp:9`), so both 0 and 4 KiB yield the same 4 MiB
reservation per thread, and the only change is how much of it is committed at
construction.

A single shared named constant for the four sites is deliberately not
introduced: the existing sites each pass their own literal
(`Engine/Source/Agent/AgentCommandServer.cpp:143` passes `64 * 1024`,
`Engine/Source/Graphics/Managers/TextureUploadManager.cpp:223` and
`Engine/Source/CrashReport.cpp:170` pass `1024`), and matching that local
convention keeps the change to four argument values.

## Critical files

- `Engine/Source/File/PackChunkLoader.cpp:230` — lazy loading thread
  `ThreadLocal` construction.
- `Engine/Source/File/PackChunks.cpp:472` — eager loading thread `ThreadLocal`
  construction.
- `Engine/Source/Graphics/Screenshot.cpp:112,582` — the two encoder thread
  `ThreadLocal` constructions; `:173` is the path log that reaches `Grow`.
- `Common/Threading/ThreadLocal.h:38-41`, `Common/Threading/ThreadLocal.cpp:9,14`
  — read-only: the initial-size and reserve-size contract the new arguments feed.
- `Common/Workbuffer.cpp` — read-only: `Grow`'s `DEBUG_BREAK()` and `kError`
  report being removed from these threads' normal path.

## In scope

- The first constructor argument of the four `common::ThreadLocal`
  constructions named in `## Critical files`, and nothing else on those lines.

## Out of scope

- `common::ThreadLocal`'s signature, its parameter defaults, the 64-KiB floor,
  and the reserve-size formula.
- `Workbuffer::Grow`, its `DEBUG_BREAK()`, and its `kError` log, including the
  null-`Data()` `to_chars` range in `Append(int64_t)` and `AppendFloat`.
- Every other `ThreadLocal` construction, including the `DataPacker` sites and
  `Engine/Source/Main.cpp:100`.
- The log lines themselves, the `std::filesystem::path` formatter, and the
  screenshot and chunk-loading logic around them.
- The workbuffer reserve size at any site, and `common::StableVector`.

## Risk tier and invariants

Tier 2 — scoped behavior. Highest trigger: a runtime allocation-sizing change on
one subsystem's worker threads. It touches no determinism/CRC path (all four
threads are outside the sim tick), no wire, serialization, `.pack`, or replay
format, no thread creation, affinity, or synchronization, and no trust boundary;
`Screenshot.cpp` is client-only and the two loader sites compile into both
executables.

## Acceptance criteria

A diff alone does not settle the observable behavior, so verify through
`/agent-harness`:

1. Launch the client, take an agent screenshot, and confirm the client log
   contains the `kGraphics`/`kDebug` output-path line for it and no
   `Workbuffer under-sized` `kError` line from the screenshot thread.
2. Confirm the same log carries no `Workbuffer under-sized` line from the lazy
   or eager loading threads across startup and play.

## Notes

- Deliberately no dependency: the fix is correct against both the baseline
  relocating `Grow` and the reserve-then-commit one, so it is independently
  landable.
