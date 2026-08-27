<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:28:06.643Z","dependsOn":[]} -->
# Keep crash-report stream construction allocator-independent

## Context

The accepted finding `CAI/shard-0012/001` identifies a failure in the crash
path's first report write. `HandleException` enters `DEBUG_BREAK_NO_LOG()` but
constructs `std::ofstream` at `Engine/Source/CrashReport.cpp:92` before any
report byte exists. The pinned VS2026 headers show the stream construction
creates a `basic_streambuf`/`std::locale`; the engine global allocator routes
that allocation through `TrackAllocation` (`Engine/Source/Memory/GlobalAllocator.cpp:12-44`).
That can re-enter the logger or damaged heap while a SIGABRT thread still owns
the non-recursive log mutex. The crash-report contract in
`Engine/Source/AGENTS.md` requires fixed-buffer, no-allocation handling.

The source shard's frozen/live target hashes match audit baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`, and current status contains only
the six pre-existing `Documents/Plans/Agents/Cpp*Audit.md` edits. The stream
construction is unchanged, unresolved, and outside the approved audit work.

## Design

The author's recommendation is to replace the crash-path `std::ofstream`
object and its construction with a fixed-buffer OS-level writer (or an
equivalent implementation proven not to allocate or lock the engine logger).
Keep the existing fixed path formatting, report layout, and no-log entry.
Write the first report bytes through that writer before any optional diagnostic
collection, and preserve the existing completion behavior. A blanket
`ScopedSuppressAllocationTracking` is not sufficient because it would hide the
allocator dependency rather than make heap-corruption handling safe.

## Critical files

- `Engine/Source/CrashReport.cpp:39-110` — `HandleException` and report stream construction.
- `Engine/Source/Memory/GlobalAllocator.cpp:104-110` — SIGABRT entry (read-only reachability reference).
- `Engine/Source/AGENTS.md` — fixed-buffer/no-allocation/no-log crash contract.
- `Documents/Plans/Engine/CrashPathLogDeadlock.md` — existing LOG-call plan is a distinct boundary (read-only duplicate inventory reference).

## In scope

- The report writer construction and write calls inside `HandleException`,
  including any small helper required to keep them allocator- and
  logger-independent.
- Verifying that the first report bytes can be written after allocation
  tracking is armed or while the crashing thread is inside logging.

## Out of scope

- The pre-write `LOG` at `CrashReport.cpp:74`, owned by
  `CrashPathLogDeadlock.md`.
- Making `gLogMutex` recursive, changing `DEBUG_BREAK` macros, or changing the
  static initialization of `Common/Log/Log.cpp`.
- Crash-report format, path-selection policy, DxDiag collection, and agent
  fixture semantics except for exercising the existing report.
- Any source or library changes outside the crash writer boundary.

## Risk tier and invariants

Expected Change Workflow Tier 3. The path is reachable from a signal/exception
handler during heap corruption and exposes allocation and locking invariants.

Preserve these invariants:

- No heap allocation, allocator-dependent formatting, logger lock, or
  heap-owned pointer is required before the first report bytes.
- Existing fixed path branches, Desktop fallback, report markers, and agent
  non-modal behavior remain intact.
- The normal crash/exception entry remains safe when allocation tracking and
  `gLogMutex` were active on the failing thread.

## Acceptance criteria

- Final source inspection finds no allocator-backed stream/object construction
  reachable from `HandleException` before or during the first report writes.
- Client and server `Debug|x64` builds clean through `/compile`.
- The `/agent-harness` crash-report fixture still emits a complete report on
  the normal path, and a focused failure-path exercise confirms the writer does
  not re-enter allocation tracking or the logger.

## Notes

The consolidated index marks the local MSVC construction-chain request as
`CAI-EXT-003`; this Plan relies on the pinned installed-header evidence already
captured by the accepted finding and does not create a separate follow-up for
that proposition.
