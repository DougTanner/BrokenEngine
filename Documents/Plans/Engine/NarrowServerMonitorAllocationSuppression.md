<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:33:09.363Z","dependsOn":[]} -->
# Narrow the Server Monitor allocation-tracking suppression to capture

## Context

Found by an architecture review of `Engine/Source/Server` (threading lens)
and verified at commit `64ec627b`. Pre-existing debt, low priority.

The main thread's only allocation-tracking suppression for the Server
Monitor host wraps more than the work its rationale names:

- `Engine/Source/Main.cpp:494-498` constructs
  `ScopedSuppressAllocationTracking` around the whole
  `pServerMonitorHost->Update()` call. Its `// Heap:` rationale names only
  snapshot strings and cell records.
- `ServerMonitorHost::Update`
  (`Engine/Source/Server/ServerMonitorHost.cpp:71-89`) calls the file-static
  `UpdateStatistics` at `:73` on every update, monitor or not, before the
  cadence and authentication checks; `Capture()`, the allocating work, is
  called only at `:84`.
- So the every-update statistics path runs untracked. It holds no tracked
  allocation today (by inspection; tracking counts `operator new` only,
  `Engine/Source/Memory/GlobalAllocator.cpp:5-29`, and is armed for the
  server loop at `Main.cpp:393-397`), so nothing is hidden yet.

Authority: `Engine/Source/Server/AGENTS.md` `## Ownership and Cadence`
("Main-thread capture and the worker lifetime have separate
allocation-tracking suppression scopes") and `Common/AGENTS.md`
`## Allocation-Free Scratch` (suppress only unavoidable, reviewed heap
work).

Unmet criterion: the main-thread suppression covers capture only, so a
future allocation in the every-update statistics path is reported.

## Design

Recommended: move the guard and its `// Heap:` rationale from `Main.cpp`
into `ServerMonitorHost::Update`, in a scope that begins just before the
`Capture()` call at `:84` and runs to the end of the function, and call
`pServerMonitorHost->Update()` from `Main.cpp` with no guard. Rationale:
`Capture` is private, so `Update` is the narrowest place that can scope it;
running the scope to the end of `Update` keeps the slot exchange and the
release of the replaced snapshot under the same guard as today.

Update the one document the move makes false:
`Documents/Features/Engine/AddressSanitizerAndFuzzing.md:39-40` says the
main-thread allocation suppression "lives in `Engine/Source/Main.cpp`".

## Critical files

- `Engine/Source/Main.cpp`
- `Engine/Source/Server/ServerMonitorHost.cpp`
- `Documents/Features/Engine/AddressSanitizerAndFuzzing.md`

## In scope

- `Main.cpp:494-498`: remove the braces, the `// Heap:` comment, and the
  guard around the `Update()` call.
- `ServerMonitorHost::Update`: the guard and its rationale around the
  `Capture()` call and the slot exchange.
- `AddressSanitizerAndFuzzing.md:39-40`: the clause naming `Main.cpp`.

## Out of scope

- The worker thread's guard in `ServerMonitorHost::Run`.
- `UpdateStatistics`, `Capture`, the cadence, and the snapshot slot.
- `ScopedSuppressAllocationTracking` and the tracking mechanism.
- `Engine/Source/Server/AGENTS.md`: its "separate suppression scopes"
  sentence stays true.

## Risk and invariants

Change Workflow Tier 2 — trigger: it changes one subsystem's runtime
behavior in tracked builds (which main-loop work allocation tracking
covers). No determinism, wire, serialization, threading, or trust exposure.

- The guard stays on the main thread, the thread that runs `Capture`; its
  counter is per-thread.
- Capture's allocations, the slot exchange, and the replaced snapshot's
  release stay suppressed.
- If `UpdateStatistics` does allocate through `operator new`, the narrowed
  guard makes a Debug server break on the first update. That is the defect
  this Plan exists to expose; fix it only if it is inside
  `ServerMonitorHost.cpp`, otherwise stop and report it.

## Acceptance criteria

Settled by one live Debug server run with an authenticated Server Monitor
connection; the supplied evidence records no reproducing setup.

- The server runs through at least one capture with no allocation-tracking
  `DEBUG_BREAK` warning in its log.

## Notes

- `Documents/Plans/Engine/ServerMonitorProfilingDisabledSnapshot.md` may
  move work from `UpdateStatistics` into `Capture`; that work then falls
  inside the narrowed guard either way, so neither Plan needs the other.
