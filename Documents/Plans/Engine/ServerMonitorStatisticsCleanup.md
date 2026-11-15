<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:33:09.363Z","dependsOn":[]} -->
# Clean up Server Monitor statistics and their allocation suppression

## Context

Found by an architecture review of `Engine/Source/Server` (dependency,
cohesion, data-shape, and threading lenses) and verified at commit
`64ec627b`. Pre-existing debt; no session changed these lines. Line numbers
below are that commit's.

### Mimalloc statistics on ProfileManager

`ProfileManager` carries four public members that only the Server Monitor
host uses:

- `miMimallocCommittedMebibytes`, `miMimallocPeakCommittedMebibytes`,
  `miMimallocHeapUsedMebibytes`, and `miMimallocPeakHeapUsedMebibytes` are
  declared at `Engine/Source/Profile/ProfileManager.h:428-431`, inside the
  `#if defined(BT_SERVER) && !defined(ENABLE_CRT_DEBUG_HEAP)` block at
  `:427`/`:432`.
- They are written only in the file-static `UpdateStatistics`
  (`Engine/Source/Server/ServerMonitorHost.cpp:42-45`) and read only in
  `ServerMonitorHost::Capture` (`ServerMonitorHost.cpp:127`). A search of the
  whole worktree for `miMimalloc` finds no other use, and the server
  `query_profile` handler (`CommandQueryProfile`,
  `Engine/Source/Agent/AgentCommandsServerGeneric.cpp:175-223`) does not
  return them.
- `ProfileManager.h` is aggregated by `Engine.h` into the PCH of both the
  client and the server project, so private Server Monitor state sits on
  another subsystem's public interface.

Neither `Engine/Source/Server/AGENTS.md` nor `Engine/Source/Profile/AGENTS.md`
records this placement as deliberate. The finding rests on the members
having one user, in another subsystem. `Engine/Source/AGENTS.md`
`## Hub Conventions` (Internals) is only the nearest related rule: it covers
heavy private state whose header changes often, which these four values are
not.

### Never-computed rows in a profiling-disabled snapshot

A server built with `kbProfiling` false (only the Release configuration,
`Engine/Source/EngineSwitches.h:66`) publishes Server Monitor rows whose
values were never computed:

- `UpdateStatistics` (`ServerMonitorHost.cpp:18-49`) returns at `:20-23`
  when `!kbProfiling`. So `sTotalEntityCounts` (`:16`, written only at
  `:25-34`) and the four mimalloc values (written only at `:42-45`) stay
  zero.
- `Capture` reads them with no gate: the entity-total section at `:107-112`
  and the memory section at `:126-130`.
- The FPS and Potential rows (`:101-105`, `:113`) are zero too: the FPS
  input `mFullUpdatesInTheLastSecond.Set` is gated at
  `Engine/Source/GameBase.cpp:517-520`, and Potential comes from the
  `kCpuTimerFrameUpdate` timer, which records nothing without profiling.
  The CPU timer and counter profile sections (`:114-125`) read unrecorded
  rows as well.
- The per-cell `iEntityCount` (`:141-145`) is computed in `Capture` with no
  gate, so the same snapshot carries real per-cell counts beside zero
  totals.

Effect: a Release server's monitor shows plausible zeros
(`Committed: 0 MiB`, zero entity totals, `FPS: 0`) that read as real
values. The snapshot text has no way to mark a value absent, but sections
are counted free text (`common::ServerMonitorSnapshotHeader`,
`Common/ServerMonitorProtocol.h`), and the section count already varies
with `ENABLE_CRT_DEBUG_HEAP` (`ServerMonitorHost.cpp:94-100`), so a section
can be left out with no protocol change.

Authority: `Engine/Source/Server/AGENTS.md` `## Ownership and Cadence`
("the existing profiling configuration gates") and `## Profile Presentation`
("Profiling-disabled builds keep the same configuration policy") record the
gate as policy, but neither says what such a build's snapshot contains. So
this is a product choice, not a document/code contradiction.

Related, same lines: the entity-total loop (`:25-34`) re-sums every cell on
every update, but its only reader is `Capture` (`:108-111`), which runs every
eighth authenticated update and already walks the same per-cell counts at
`:137-146`. The loop mirrors `game::PublishServerEntityCounts`
(`Projects/BrokenEngineSandbox/Source/Frame/ServerCellStats.cpp:40-49`). The
server `query_profile` handler needs only the `UpdateProfileText()` call
(`:48`) on every update, not the totals or `mi_stats_get`.

### Main-thread allocation suppression wider than capture

The main thread's only allocation-tracking suppression for the Server
Monitor host wraps more than the work its rationale names:

- `Engine/Source/Main.cpp:494-498` constructs
  `ScopedSuppressAllocationTracking` around the whole
  `pServerMonitorHost->Update()` call. Its `// Heap:` rationale names only
  snapshot strings and cell records.
- `ServerMonitorHost::Update` (`ServerMonitorHost.cpp:71-89`) calls
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

### Unmet criteria

- Server Monitor private state lives with its only user, not on
  `ProfileManager`.
- A profiling-disabled server's snapshot does not present never-computed
  values as real.
- The main-thread suppression covers capture only, so a future allocation in
  the every-update statistics path is reported.

## Design

### 1. Remove the mimalloc members from ProfileManager

Before deleting the members, the executing session searches the private
Coretide game project (present only in the primary checkout) for
`miMimalloc`. A search of its `.h` and `.cpp` files at commit `64ec627b`
found no use. If a use exists at execution time, stop and return the Plan
for re-planning.

Delete the block at `ProfileManager.h:427-432`. The four values become
locals of `Capture` (step 2), under the same `!defined(ENABLE_CRT_DEBUG_HEAP)`
condition the code that uses them already has; no file-static replaces the
members.

### 2. Real values in a profiling-disabled snapshot

User decision, made on 2026-11-14. Asked what to require instead of the
never-computed zero rows, the user chose the option presented as "Option 1:
real values", whose text was: "Compute entity totals and memory at capture
in every build so Release shows real values; omit only rows that cannot be
real without profiling (timing sections, Potential)."

What that decision means in `ServerMonitorHost.cpp`:

- Entity totals: sum them in `Capture`'s existing per-cell walk
  (`:137-146`), outside the `kbProfiling` gate, remove the loop at `:25-34`,
  and remove `sTotalEntityCounts` (`:16`) in favor of a local of `Capture`.
  `Capture` is the only reader and runs in the same `Update` call on the
  same thread. The entity-total section keeps its place in the statistics
  section order.
- Memory: read `mi_stats_get` in `Capture` into the four locals, outside the
  `kbProfiling` gate, and remove the block from `UpdateStatistics`.
  `UpdateStatistics` keeps its `kbProfiling` early return and only the
  `UpdateProfileText()` call.
- FPS: the chosen text omits "only rows that cannot be real without
  profiling", and FPS can be real, so it is sent. Remove the
  `if constexpr (kbProfiling)` around `mFullUpdatesInTheLastSecond.Set` at
  `GameBase.cpp:517-520` so the server records the count in every build. The
  gate wraps only that call, and its inputs (the local `iFullTicks` and the
  member at `Engine/Source/Profile/ProfileManager.h:422`) exist with
  profiling off. The client's gate at `GameBase.cpp:361-364` stays.
- Omitted in a profiling-disabled build: the Potential row of the two
  FPS/Potential sections (`:105`, `:113`), which then carry the FPS row
  alone, and the CPU timer and counter profile sections (`:114-125`).
  Profiling-enabled builds still send them.

One point is execution-time work, not an open decision:

- Memory rests on one fact the repository does not prove: whether mimalloc
  maintains `page_committed` (the "Heap used" and "Peak heap" rows) when
  `MI_STAT` is off. The repository shows only that reserved and committed
  totals are kept (`Engine/Source/Memory/GlobalAllocator.cpp:245` comment).
  The executing session settles that through `/verify-external-claims`
  before writing the memory section; if `page_committed` is not maintained,
  those two rows are omitted in such builds and the committed rows stay.

Before implementing, the executing session checks that the monitor tool
(`Tools/ServerMonitor/ServerMonitor.cpp`, its `ReadSections` callers and the
paint code) handles a snapshot whose FPS sections have no Potential row and
whose only profile sections are FPS and memory. If it does not, stop and
return to the user instead of widening scope into the tool.

Update `Engine/Source/Server/AGENTS.md` so it states what a
profiling-disabled snapshot contains: the statistics sentence in
`## Ownership and Cadence` and the last sentence of
`## Profile Presentation`. Update the third bullet of
`Engine/Source/Profile/AGENTS.md` `## Index and Lifetime Contracts` too: it
names Server Monitor capture as the user of the CPU accessors and formatters
"even when profiling is disabled", a use this change removes in such builds,
and it says recording code compiles out, which the server's update count no
longer does.

### 3. Narrow the main-thread suppression to capture

Move the guard and its `// Heap:` rationale from `Main.cpp` into
`ServerMonitorHost::Update`, in a scope that begins just before the
`Capture()` call at `:84` and runs to the end of the function, and call
`pServerMonitorHost->Update()` from `Main.cpp` with no guard. Rationale:
`Capture` is private, so `Update` is the narrowest place that can scope it;
running the scope to the end of `Update` keeps the slot exchange and the
release of the replaced snapshot under the same guard as today. The
`mi_stats_get` read and the entity-total sum that step 2 moves into
`Capture` fall inside this guard.

Update the one document the move makes false:
`Documents/Features/Engine/AddressSanitizerAndFuzzing.md:39-40` says the
main-thread allocation suppression "lives in `Engine/Source/Main.cpp`".

## Critical files

- `Engine/Source/Server/ServerMonitorHost.cpp`
- `Engine/Source/Profile/ProfileManager.h`
- `Engine/Source/GameBase.cpp`
- `Engine/Source/Main.cpp`
- `Engine/Source/Server/AGENTS.md`
- `Engine/Source/Profile/AGENTS.md`
- `Documents/Features/Engine/AddressSanitizerAndFuzzing.md`

## In scope

- `ProfileManager.h`: delete the four `miMimalloc*Mebibytes` members and
  their `#if`/`#endif` pair (`:427-432`).
- `ServerMonitorHost.cpp`:
  - `sTotalEntityCounts` (`:16`), removed in favor of a local of `Capture`.
  - `UpdateStatistics`: the `kbProfiling` early return, the entity-total
    loop, and the `mi_stats_get` block.
  - `ServerMonitorHost::Update`: the guard and its rationale around the
    `Capture()` call and the slot exchange.
  - `ServerMonitorHost::Capture`: the section `reserve` counts, the
    FPS/Potential, entity-total, profile, and memory sections, the four
    mimalloc locals and their `mi_stats_get` read, and the per-cell walk
    that gains the totals sum.
- `GameBase::ServerUpdate`: the `if constexpr (kbProfiling)` around
  `mFullUpdatesInTheLastSecond.Set` at `GameBase.cpp:517-520`.
- `Main.cpp:494-498`: remove the braces, the `// Heap:` comment, and the
  guard around the `Update()` call.
- `Engine/Source/Server/AGENTS.md` `## Ownership and Cadence` statistics
  bullet and `## Profile Presentation` last sentence.
- `Engine/Source/Profile/AGENTS.md` `## Index and Lifetime Contracts` third
  bullet.
- `AddressSanitizerAndFuzzing.md:39-40`: the clause naming `Main.cpp`.

## Out of scope

- Any other `ProfileManager` member.
- `Common/ServerMonitorProtocol.h`: no wire layout, version, or new "absent"
  marker.
- `Tools/ServerMonitor/`: no tool change.
- Turning on any other profiling recording in Release: `CpuStop`, the raw
  timers, `game::PublishServerEntityCounts`, and the profile counters stay
  gated, and so does the client's `mFullUpdatesInTheLastSecond.Set` gate at
  `GameBase.cpp:361-364`.
- The every-eighth-update cadence, the snapshot slot, and the worker thread,
  including the worker thread's guard in `ServerMonitorHost::Run`.
- `ScopedSuppressAllocationTracking` and the tracking mechanism.
- The cell records and the tick/time/active-cell/client section.
- `query_profile` output.
- The "separate suppression scopes" sentence of
  `Engine/Source/Server/AGENTS.md` `## Ownership and Cadence`, which stays
  true.

## Risk and invariants

Change Workflow Tier 2 — triggers: it removes public members from a header
`Engine.h` aggregates, a public interface change, so the Tier 1 "no public
signature exposure" condition does not hold; and it changes one subsystem's
runtime behavior (what the Server Monitor host puts in a snapshot, and which
main-loop work allocation tracking covers in tracked builds). The wire
format and its trust are unchanged: only which counted text sections are
sent.

- No determinism, CRC, wire, serialization, or trust exposure; nothing here
  enters the simulation or the CRC.
- Threading is unchanged: `UpdateStatistics` and `Capture` stay on the main
  thread inside `ServerMonitorHost::Update`. The guard stays on the main
  thread, the thread that runs `Capture`; its counter is per-thread.
- Capture's allocations, the slot exchange, and the replaced snapshot's
  release stay suppressed.
- `UpdateProfileText()` still runs on every update, monitor or not, so raw
  `query_profile` rows are preserved.
- Profiling-enabled builds (Debug, Profile) send the same sections and
  values as before; the memory section's text format is unchanged.
- A profiling-disabled build still never reads absent raw-timer storage.
- Build: `ProfileManager.h` is in the PCH, so both the client and the server
  rebuild; the `ENABLE_CRT_DEBUG_HEAP` configuration must still compile with
  the mimalloc values absent.
- If `UpdateStatistics` does allocate through `operator new`, the narrowed
  guard makes a Debug server break on the first update. That is the defect
  step 3 exists to expose; fix it only if it is inside
  `ServerMonitorHost.cpp`, otherwise stop and report it.

## Acceptance criteria

Build set: the client and the server, in the Debug and Release
configurations the live runs use.

Diff:

- A worktree-wide search for `miMimalloc` finds nothing.

Live runs, settled by one scenario the executing session builds (the
supplied evidence records no reproducing setup): a Release server and then a
Debug server, each with the Server Monitor tool connected and authenticated
through at least one capture; then server `query_profile` on the Debug server
with no monitor connected.

- Release: no row in the snapshot shows a value the server never computed.
  The entity totals are real and agree with the per-cell counts. The memory
  rows are real, except the "Heap used" and "Peak heap" rows when
  `## Design` omits them. The FPS row is sent and real: it follows the
  server's update rate. The Potential, timer, and counter rows are not sent.
- Release: the snapshot still shows tick, time, active cells, clients, and
  the per-cell map.
- Debug: the monitor shows the same sections as before the change, with the
  memory section's text in the baseline's format, and server `query_profile`
  still returns advancing timer rows with no monitor connected.
- Debug: the server runs through at least one capture with no
  allocation-tracking `DEBUG_BREAK` warning in its log.

## Coordination

- `Documents/Plans/Engine/ReduceGameBaseCpp.md` also edits
  `GameBase::ServerUpdate`. This Plan touches that function only at the
  `kbProfiling` gate near its end (`:517-520`); neither Plan needs the
  other.
