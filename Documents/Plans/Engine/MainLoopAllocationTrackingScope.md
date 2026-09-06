<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T21:47:40.033Z","dependsOn":[]} -->
# Disable allocation tracking when an exception leaves the main loop

## Context

`MainThread` arms the allocator's main-loop heap-allocation tracking with a
plain sequential statement and disarms it with another one on the normal
fall-through path only:

- `Engine/Source/Main.cpp:354` — `EnableAllocationTracking(true);` just before
  the main loop.
- `Engine/Source/Main.cpp:465` — `EnableAllocationTracking(false);` immediately
  after the loop exits normally.

`EnableAllocationTracking` is a single relaxed store to the file-local
`sbTrackingReady` flag (`Engine/Source/Memory/GlobalAllocator.cpp:30-33`), and
`TrackAllocation` fires `DEBUG_BREAK()` for every heap allocation on a thread
with a `ThreadLocal` while that flag is set and the per-thread suppression
counter `giAllocationTrackingSuppressed` (`Common/AllocationTracking.h:4`) is
zero (`Engine/Source/Memory/GlobalAllocator.cpp:12-26`).

When an exception leaves the main loop, control never reaches
`Engine/Source/Main.cpp:465`, so the flag stays set for the whole unwind.
`pGraphics` is a `MainThread` local declared at `Engine/Source/Main.cpp:279`,
before the arming statement, so its destructor runs during that unwind with
tracking still armed and no `ScopedSuppressAllocationTracking` covering it.
`wWinMain` then catches at `Engine/Source/Main.cpp:867-878` and calls
`HandleException`.

Observed during a harness acceptance run of the network corruption-response
change, after a new client-fatal `ASSERT` in
`Engine/Source/Network/Client/Client.cpp` threw out of `MainThread`: the client
log ends with twelve

`DEBUG_BREAK at ...\Engine\Source\Memory\GlobalAllocator.cpp:25 in ... TrackAllocation`

lines immediately after the `Graphics::Destroy()` teardown lines
(`Temp/client-agent-status-change.log:178-189`; the same run tail appears in
`Temp/client-agent-engine-envelope.log` and `Temp/client-agent-game-packet.log`).

The defect is pre-existing, not introduced by that change:
`git diff 37fe4867cfcd80c24bd764ee0d5643910b7f453e -- Engine/Source/Main.cpp Engine/Source/CrashReport.cpp Engine/Source/Memory Common/`
is empty for the files involved. It had not surfaced before because the only
existing exercise of the fatal path, the `crash_report_fixture` agent command
(`Engine/Source/Agent/AgentCommandsShared.cpp:203-204`), calls
`HandleException()` and then `ExitProcess(0)` without unwinding `MainThread`.

Effect is noise only, on an already-fatal path: twelve debugger breaks when a
debugger is attached, twelve log lines otherwise. The crash report itself is
written correctly before them. Nothing about the crash report contents,
determinism, or shutdown correctness changes.

## Design

Recommended mechanism, smallest edit that removes the root cause: make the
disarm run on the unwind path too, by reusing the `common::ScopedLambda` RAII
pattern `MainThread` already uses for its other teardown steps
(`Engine/Source/Main.cpp:108,151,203,229,331`; `Common/ScopedLambda.h`).

Declare the guard immediately before `Engine/Source/Main.cpp:354`, so it is
constructed while tracking is still disarmed (its construction is itself a
potential heap allocation) and, being declared after `pGraphics`
(`Engine/Source/Main.cpp:279`), it is destroyed *before* `pGraphics` during
reverse-order unwind:

```cpp
common::ScopedLambda disableAllocationTracking([]()
{
	EnableAllocationTracking(false);
});
EnableAllocationTracking(true);
```

Keep the existing explicit `EnableAllocationTracking(false);` at
`Engine/Source/Main.cpp:465`. This is deliberate rather than redundant: the
normal path must disarm exactly where it does today, because the post-loop
teardown between `Engine/Source/Main.cpp:465` and the end of `MainThread`
(autosave, settings saves, manager destruction) allocates freely and is not
meant to be tracked. The guard's own call is then a no-op second relaxed store
on the normal path, and the only path on which it does work is the unwind.
`ScopedLambda` swallows callback exceptions, which is correct here because the
callback is a single store that cannot throw.

Author's note on the alternative considered: wrapping the main loop in an added
block scope so a single guard covers both exits is the tidier shape, but it
re-indents the whole loop body (`Engine/Source/Main.cpp:355-461`) for no
behavioral gain. The implementation may choose it if review prefers a single
disarm site; either way the required outcome is that no allocation during
exception unwind out of `MainThread` reaches `TrackAllocation`'s `DEBUG_BREAK`.

Do not use `ScopedSuppressAllocationTracking` for this: its counter is
per-thread and an instance placed early enough to cover `pGraphics` teardown
would also have to be constructed before `pGraphics`, which would suppress
tracking for the entire main loop and defeat the mechanism.

## Critical files

- `Engine/Source/Main.cpp:279,331,354,465,867-878` — `pGraphics` local
  declaration, the existing `ScopedLambda` teardown pattern, the arm/disarm
  statements, and the `wWinMain` catch that follows the unwind.
- `Engine/Source/Memory/GlobalAllocator.cpp:12-33` — `TrackAllocation`'s
  `DEBUG_BREAK` and the `EnableAllocationTracking` store it reads.
- `Common/AllocationTracking.h` — the per-thread suppression counter and its
  scoped guards.
- `Common/ScopedLambda.h` — the RAII helper the recommended mechanism reuses.
- `Common/AGENTS.md` — the allocation-tracking contract (per-thread
  suppression, `// Heap:` rationale requirement).

## In scope

- `Engine/Source/Main.cpp` around line 354: add the RAII disarm covering the
  main loop, declared after `pGraphics` and constructed before tracking is
  armed.
- `Engine/Source/Main.cpp:465`: keep the normal-path disarm where it is, or
  remove it only if the block-scope alternative is chosen and the guard's scope
  ends at that exact point.

## Out of scope

- Any change to `Engine/Source/Memory/GlobalAllocator.cpp`,
  `Common/AllocationTracking.h`, or the meaning of the tracking flag and the
  suppression counter.
- Any change to crash-report generation, `HandleException`, the `wWinMain`
  catch structure, `crash_report_fixture`, or process-exit policy.
- Suppressing, silencing, or reclassifying `TrackAllocation`'s `DEBUG_BREAK`
  itself, or adding tracking suppression to `Graphics` teardown or any other
  subsystem destructor.
- Removing or reordering any other `MainThread` local, `ScopedLambda`, or
  teardown step; the window-message teardown ordering work in
  `Documents/Plans/Engine/WindowMessageTeardownOrdering.md` owns a different
  region of the same function and is not touched here.
- Making other main-loop allocations on the fatal path disappear; the goal is
  that tracking is disarmed during unwind, not that the unwind stops
  allocating.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: scoped behavior of one subsystem —
local teardown behavior of the client/server entry point in a single file, with
no change to determinism or CRC, wire or protocol, serialization or data
layout, save/replay compatibility, threading, or a trust boundary.

Preserve these invariants:

- Tracking is armed at exactly the same point as today relative to the main
  loop, so ordinary main-loop allocation detection is unchanged.
- On the normal exit path, tracking is disarmed at the same point as today, so
  the post-loop teardown continues to allocate untracked.
- The guard is declared after `pGraphics` so that unwind destroys the guard
  before `pGraphics`.
- The crash report is still written by the existing `wWinMain` catch, with the
  same contents and ordering.

## Acceptance criteria

- Source inspection proves the disarm runs on both exits and that the guard's
  declaration sits between `pGraphics` and the arming statement.
- A client run that throws out of the main loop (for example the fatal receive
  `ASSERT` path used in the originating run) produces a client log whose tail
  contains no
  `DEBUG_BREAK at ...GlobalAllocator.cpp:25 in ... TrackAllocation` line after
  the `Graphics::Destroy()` teardown lines, while the crash-report lines that
  precede them are unchanged.
- A normal client close still logs no new `TrackAllocation` `DEBUG_BREAK`
  lines during post-loop teardown.
- The client `Debug|x64` build passes through `/compile`.

## Notes

Origin: residual proven at the acceptance-verification step of the network
corruption-response change (Plan claimed in that session), which is why the
observed log tails cite that session's `Temp/` client logs. The residual is
outside that change's boundary: its `## Out of scope` excludes crash-report and
local-failure policy beyond the receive boundary, and `Engine/Source/Main.cpp`
is not among its changed files. No source fix was attempted; baseline for the
evidence above is `37fe4867cfcd80c24bd764ee0d5643910b7f453e`.

The `Temp/` logs are session-local and will not survive; the source citations in
`## Context` are the durable evidence, and the symptom is reproducible by
forcing any exception out of the main loop.
