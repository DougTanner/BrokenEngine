<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-22T17:52:35.234Z","dependsOn":[]} -->
# Fix the 134 tracked heap allocations on the shared engine startup path

## Context

`Engine/Source/Memory/GlobalAllocator.cpp:25` fires `DEBUG_BREAK()` from
`TrackAllocation()` whenever a heap allocation happens on a thread that owns a
`common::ThreadLocal` after `EnableAllocationTracking(true)` has armed the
tripwire and while no `ScopedSuppressAllocationTracking` guard is active. That
is the repository's main-loop allocation boundary
(`Engine/Source/Memory/AGENTS.md`, `Engine/Source/AGENTS.md`).

Until `DEBUG_BREAK()` started always logging one `kWarning` naming its call site
(`Common/ErrorUtils.h:19`, `common::LogDebugBreak`), those breaks were invisible
without an attached debugger. With logging in place, Debug|x64 harness runs of
both the client and the server each emit exactly 134 identical warnings:

`DEBUG_BREAK at ...\Engine\Source\Memory\GlobalAllocator.cpp:25 in void __cdecl \`anonymous-namespace'::TrackAllocation(void)`

Observed evidence (harness runs during the PusherCellWideCoverage session,
session baseline `2058c850eaf1c6c451d3a06c06b51ea99c5f285b`):

- Server log: one contiguous block of 134 such lines at lines 12-145, sitting
  between `[Tick: 1] ServerUpdate FullTicks: 2 (expected 1)` and
  `[Tick: 661] Server::Connect Client: 1`.
- Client log: the same contiguous block of 134 lines at lines 101-234.
- The count was exactly 134 on both processes and did not change across three
  reset-plus-scenario cycles, including a 768-player stress run.

Two facts follow from that evidence and make this worth a Plan rather than a
one-off: the burst is identical on the client and the server, so it lives on a
startup path both builds share, and it is fixed in size and independent of
scenario content, so it is startup work rather than per-entity or per-tick work.
The corresponding log files lived under `Temp/` (transient, not tracked); the
line evidence above is the durable record and the burst reproduces on any
Debug|x64 harness launch.

This is pre-existing behavior newly made visible; the allocations themselves
predate the session that observed them.

## Design

Recommended approach, in order:

1. Diagnose first, per `/external-diagnose-bug` discipline: prove the actual
   allocation call sites before changing anything. The tripwire already names
   the thread and the moment; a debugger break on
   `GlobalAllocator.cpp:25` in a Debug|x64 harness launch yields the calling
   stack for each of the 134 hits directly, and the surrounding log lines bound
   the window to between the first server tick and the first client connect.
   Recording which distinct call stacks account for the 134 hits, and how many
   hits each contributes, is the first deliverable.
2. Classify each proven site. A site whose allocation is avoidable is fixed by
   removing the allocation — preferring `gpThreadLocal->mWorkbuffer` scratch,
   an already-owned persistent buffer, or reserving during the genuine
   pre-`EnableAllocationTracking` startup window. A site whose allocation is
   genuinely unavoidable is wrapped in `ScopedSuppressAllocationTracking` with
   the root-required `// Heap:` rationale comment, per
   `Engine/Source/Memory/AGENTS.md` and `Common/AGENTS.md`.
3. Prefer the first option. Suppression is the fallback for a site that is
   proven unavoidable in the write-up, not the default; a suppression added
   without that justification hides the tripwire instead of satisfying it.

An alternative the author considered and does not recommend: moving
`EnableAllocationTracking(true)` later so the burst falls outside the armed
window. That would silence the warnings without establishing whether the
allocations are in fact outside the main loop, and it would shrink the
tripwire's coverage for every future allocation on the same path.

If diagnosis shows a site whose fix would reach outside the engine startup path
named in `## In scope` — for example a `ThirdParty` allocation or a game-owned
startup path — surface that for re-planning rather than expanding scope.

## Critical files

- `Engine/Source/Memory/GlobalAllocator.cpp:12-33` — `TrackAllocation()`, the
  reporting site, and `EnableAllocationTracking`; read to locate the armed
  window, not expected to change.
- `Common/AllocationTracking.h` — `ScopedSuppressAllocationTracking` and the
  thread-local suppression counter, if a suppression fallback is used.
- `Engine/Source/Memory/AGENTS.md` — the allocation-tracking contract, which
  gains a note only if the fix establishes new policy.
- The allocating call sites themselves, which diagnosis identifies; they are
  currently unknown and are the actual change surface.

## In scope

- Diagnosing and recording the distinct call stacks behind the 134 tracked
  allocations on the shared client/server engine startup path, with hit counts.
- For each proven site on that path: removing the allocation, or wrapping it in
  `ScopedSuppressAllocationTracking` with a `// Heap:` rationale when it is
  proven unavoidable.
- Updating the owning subsystem `AGENTS.md` only where the fix changes a
  documented contract.

## Out of scope

- The `DEBUG_BREAK` / `DEBUG_BREAK_NO_LOG` macros, `common::LogDebugBreak`, and
  the decision that `DEBUG_BREAK` logs a `kWarning`.
- Any other `DEBUG_BREAK` call site, including the pusher zone overflow break
  (`Documents/Plans/Engine/PusherOverflowBreakPerCall.md`), the crash-path
  breaks (`Documents/Plans/Engine/CrashPathLogDeadlock.md`), and the
  pre-`main()` static-initialization break
  (`Documents/Plans/Engine/LogFileStreamStaticInit.md`).
- The mimalloc arena configuration, the allocator override block, the CRT
  debug-heap alternative, and the per-frame allocation counter.
- General startup performance work, allocation reduction outside the proven
  sites, and any allocation that does not trip the armed tripwire.
- Backward compatibility, runtime toggles, and unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 — scoped runtime behavior on one subsystem's
startup path — under the root `AGENTS.md` tiers. Escalate to Tier 3 if
diagnosis shows a site whose fix reaches threading, serialization, or
determinism/CRC state, or spans independently owned subsystems.

Invariants to preserve:

- No change to simulation results: the fix must not alter any value that feeds
  the per-tick shared CRC, and client and server must continue to agree.
- The allocation tripwire keeps its current coverage: the armed window is not
  narrowed, and suppression stays scoped to the specific proven allocation
  rather than to an enclosing region.
- Suppression is thread-local, so a guard placed at a `Dispatch()` call site
  does not cover worker threads; suppress inside the dispatched function
  (`Common/AGENTS.md`).

## Acceptance criteria

- The client and server both build (Debug|x64).
- A Debug|x64 harness launch of both processes produces zero
  `GlobalAllocator.cpp:25` `DEBUG_BREAK` warnings in either log, where the
  baseline run produces exactly 134 in each.
- Every remaining allocation on the diagnosed path that is left in place is
  covered by a `ScopedSuppressAllocationTracking` guard carrying a `// Heap:`
  comment that states why it is unavoidable.
- Per-tick client and server CRCs for the same harness scenario match each
  other and match the pre-change run.
