<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-22T17:35:11.970Z","dependsOn":[]} -->
# Assess Whether Any LOG Can Run Before the Log File Stream Is Constructed

## Context

Every `LOG` in the codebase reads a file-scope `std::ofstream` that is built by
dynamic initialization when its translation unit starts up. If any `LOG` can run
before that point, it touches an object that has not been constructed yet.

Evidence, current source:

- `Common/Log/Log.cpp:29` — `static std::ofstream sLogFileStream;` is a file-
  scope object with a non-trivial constructor, so it is dynamically initialized
  when `Log.cpp`'s static initialization runs, not before.
- `Common/Log/Log.cpp:129` — `LogWrite` calls `sLogFileStream.is_open()` on
  every emitted line. Reading a not-yet-constructed object is undefined
  behavior; in practice the zero-initialized bytes usually make `is_open()`
  answer false, which is why nothing has been observed.
- Callers reachable early: `common::Assert` logs before its break and throw
  (`Common/ErrorUtils.cpp:18`), and `common::LogDebugBreak` logs from every
  `DEBUG_BREAK()` (`Common/ErrorUtils.cpp:6-12`, macro at
  `Common/ErrorUtils.h:19`). `Engine/Source/Memory/GlobalAllocator.cpp` runs a
  static initializer that configures mimalloc before `main()` and trips
  `DEBUG_BREAK` on some conditions, which is the most plausible pre-`main()`
  logging route.

Pre-existing at the session baseline `2058c850eaf1c6c451d3a06c06b51ea99c5f285b`
and applying to every existing `LOG` call site: the session that recorded this
Plan neither introduced `sLogFileStream` nor the `is_open()` read. It added one
new logging caller (`LogDebugBreak`), which widens the set of call sites but does
not create the exposure; its implementer flagged the exposure as out of that
session's boundary.

Reachability is unproven. No pre-static-initialization `LOG` has been observed,
and nothing in this Plan asserts that one exists.

## Design

Assess first, then decide — do not start from the assumption that a fix is owed.

Step 1, the assessment, is the substance of this Plan: determine whether any
`LOG` can actually execute before `Log.cpp`'s dynamic initialization completes.
The author's suggested method is to enumerate the static initializers that can
log, starting with `Engine/Source/Memory/GlobalAllocator.cpp`'s
`MemoryInitializer` and any other file-scope object in the client, server, and
DataPacker builds whose constructor can reach `LOG`, `ASSERT`, `CHECK_HRESULT`,
or `DEBUG_BREAK`, and to check whether the linker's initialization order can put
any of them before `Log.cpp`. Static initialization order across translation
units is not guaranteed by the language, so a "usually fine" ordering is not a
negative result — the question is whether such a caller exists at all.

Step 2 depends on what step 1 finds, and the author recommends the cheapest
outcome that the evidence supports:

- No such caller exists in any build: close this out with a short note recorded
  where a future reader will meet the question — a comment beside
  `Common/Log/Log.cpp:29` stating that no static initializer logs, and why that
  is what makes the current code safe. Deleting the Plan with no repository
  change is also acceptable if the implementing session judges the comment
  unnecessary; say which was chosen and why.
- A caller exists: apply the smallest guard that makes the read safe. The
  author's recommended shape is the standard construct-on-first-use idiom — give
  `sLogFileStream` a function-local accessor so the stream is constructed on
  first use — because it removes the ordering question outright rather than
  detecting it. A separate zero-initialized `std::atomic<bool>` "stream is
  constructed" flag, checked before the `is_open()` read, is the alternative if
  the accessor proves to disturb `EnableLogFile`'s locking.

Keep this small either way. Logging is on every call path in the codebase, so a
mechanism heavier than one accessor or one flag is out of proportion to an
exposure that has never been observed.

## Critical files

- `Common/Log/Log.cpp:29`, `:36-51`, `:119-135` — `sLogFileStream`,
  `EnableLogFile`, and the `is_open()` read in `LogWrite`
- `Common/ErrorUtils.cpp:6-22` — `LogDebugBreak` and `Assert` (read-only
  reference: the earliest-reachable logging callers)
- `Engine/Source/Memory/GlobalAllocator.cpp` — the `MemoryInitializer` static
  initializer (read-only reference for the reachability assessment)

## In scope

- The reachability assessment described above, across the client, server, and
  DataPacker builds
- If a reachable caller is proven: the guard for `sLogFileStream` in
  `Common/Log/Log.cpp`, confined to its declaration at line 29, `EnableLogFile`,
  and the `is_open()` read in `LogWrite`
- If none is proven: the short explanatory comment beside the declaration, or a
  recorded decision that no repository change is warranted

## Out of scope

- Any change to log formatting, categories, levels, ring buffers, or
  `DiagnosticLog`
- `gLogMutex`'s locking contract and anything that would make it recursive
- The crash-handler pre-write logging deadlock (a separate Plan)
- Changing which code may call `LOG`, or adding logging to or removing it from
  any static initializer
- Auditing static initialization order for concerns other than this one stream

## Risk tier and invariants

Expected Tier 2 (scoped behavior in one subsystem's logging implementation).
Escalate to Tier 3 if the chosen guard changes `gLogMutex` usage, the ordering
between `EnableLogFile` and `LogWrite`, or anything reached from the crash path.
An assessment that ends in a comment or in no change at all is Tier 1.

Invariants the change must not disturb:

- `LogWrite` keeps emitting under `gLogMutex`, with `EnableLogFile` opening the
  stream under that same lock and logging its own failure only after releasing it
  (`Common/Log/Log.cpp:39-50`)
- Allocation-tracked call sites stay allocation-free; the file-sink write keeps
  its existing `ScopedSuppressAllocationTracking` guard
- No recursive logging is introduced (`Common/Log/AGENTS.md`)

## Acceptance criteria

- The assessment's result is stated with the enumerated static initializers it
  examined and whether any can log before `Log.cpp` initializes
- If a guard is added: client, server, and DataPacker build clean through
  `/compile`, and an `/agent-harness` run with a log file enabled still produces
  the same log file content as before the change
- If no guard is added: the recorded decision names the evidence that no
  reachable pre-initialization caller exists

## Notes

- Placed under `Documents/Plans/Engine/` because the Plans tree has no `Common`
  area and the Plan writer creates none; the owning code is `Common/Log/`.
