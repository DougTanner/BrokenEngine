<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:33:02.557Z","dependsOn":["Documents/Plans/Engine/MoveServerMonitorMimallocStatistics.md"]} -->
# Stop a profiling-disabled server sending never-computed monitor rows

## Context

Found by an architecture review of `Engine/Source/Server` (data-shape and
cohesion lenses) and verified at commit `64ec627b`. Pre-existing debt. Line
numbers below are that commit's; the prerequisite Plan moves the four
mimalloc values from `ProfileManager` to file-static variables in
`ServerMonitorHost.cpp` first.

A server built with `kbProfiling` false (only the Release configuration,
`Engine/Source/EngineSwitches.h:66`) publishes Server Monitor rows whose
values were never computed:

- The file-static `UpdateStatistics`
  (`Engine/Source/Server/ServerMonitorHost.cpp:18-49`) returns at `:20-23`
  when `!kbProfiling`. So `sTotalEntityCounts` (`:16`, written only at
  `:25-34`) and the four mimalloc values (written only at `:42-45`) stay
  zero.
- `ServerMonitorHost::Capture` reads them with no gate: the entity-total
  section at `:107-112` and the memory section at `:126-130`.
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

Unmet criterion: a profiling-disabled server's snapshot does not present
never-computed values as real.

Related, same lines: the entity-total loop (`:25-34`) re-sums every cell on
every update, but its only reader is `Capture` (`:108-111`), which runs every
eighth authenticated update and already walks the same per-cell counts at
`:137-146`. The loop mirrors `game::PublishServerEntityCounts`
(`Projects/BrokenEngineSandbox/Source/Frame/ServerCellStats.cpp:40-49`). The
server `query_profile` handler (`CommandQueryProfile`,
`Engine/Source/Agent/AgentCommandsServerGeneric.cpp:175-223`) needs only the
`UpdateProfileText()` call (`:48`) on every update, not the totals or
`mi_stats_get`.

## Design

User decision, made on 2026-11-14. Asked what this Plan should require
instead of the never-computed zero rows, the user chose the option presented
as "Option 1: real values", whose text was: "Compute entity totals and memory
at capture in every build so Release shows real values; omit only rows that
cannot be real without profiling (timing sections, Potential)."

What that decision means in `ServerMonitorHost.cpp`:

- Entity totals: sum them in `Capture`'s existing per-cell walk
  (`:137-146`), outside the `kbProfiling` gate, and remove the loop at
  `:25-34`. `Capture` is the only reader and runs in the same `Update` call
  on the same thread. The entity-total section keeps its place in the
  statistics section order.
- Memory: read `mi_stats_get` in `Capture`, outside the `kbProfiling` gate,
  and remove the block from `UpdateStatistics`. `UpdateStatistics` keeps its
  `kbProfiling` early return and only the `UpdateProfileText()` call.
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
"even when profiling is disabled", a use this Plan removes in such builds,
and it says recording code compiles out, which the server's update count no
longer does.

## Critical files

- `Engine/Source/Server/ServerMonitorHost.cpp`
- `Engine/Source/GameBase.cpp`
- `Engine/Source/Server/AGENTS.md`
- `Engine/Source/Profile/AGENTS.md`

## In scope

- `UpdateStatistics`: the `kbProfiling` early return, the entity-total loop,
  and the `mi_stats_get` block.
- `ServerMonitorHost::Capture`: the section `reserve` counts, the
  FPS/Potential, entity-total, profile, and memory sections, and the
  per-cell walk that gains the totals sum.
- `sTotalEntityCounts` and the four file-static mimalloc values, which may
  become locals of `Capture`.
- `GameBase::ServerUpdate`: the `if constexpr (kbProfiling)` around
  `mFullUpdatesInTheLastSecond.Set` at `GameBase.cpp:517-520`.
- `Engine/Source/Server/AGENTS.md` `## Ownership and Cadence` statistics
  bullet and `## Profile Presentation` last sentence.
- `Engine/Source/Profile/AGENTS.md` `## Index and Lifetime Contracts` third
  bullet.

## Out of scope

- `Common/ServerMonitorProtocol.h`: no wire layout, version, or new "absent"
  marker.
- `Tools/ServerMonitor/`: no tool change.
- Turning on any other profiling recording in Release: `CpuStop`, the raw
  timers, `game::PublishServerEntityCounts`, and the profile counters stay
  gated, and so does the client's `mFullUpdatesInTheLastSecond.Set` gate at
  `GameBase.cpp:361-364`.
- The every-eighth-update cadence, the snapshot slot, and the worker thread.
- The cell records and the tick/time/active-cell/client section.
- `query_profile` output.

## Risk and invariants

Change Workflow Tier 2 — trigger: it changes one subsystem's runtime
behavior (what the Server Monitor host puts in a snapshot). The wire format
and its trust are unchanged: only which counted text sections are sent.

- Nothing here enters the simulation or the CRC.
- `UpdateProfileText()` still runs on every update, monitor or not, so raw
  `query_profile` rows are preserved.
- Capture stays on the main thread and inside the existing
  allocation-tracking suppression.
- Profiling-enabled builds (Debug, Profile) send the same sections and
  values as before.
- A profiling-disabled build still never reads absent raw-timer storage.

## Acceptance criteria

Settled by a live run of a Release server with the Server Monitor tool
connected; the supplied evidence records no reproducing setup, so the
executing session builds one.

- No row in the Release snapshot shows a value the server never computed.
  The entity totals are real and agree with the per-cell counts. The memory
  rows are real, except the "Heap used" and "Peak heap" rows when
  `## Design` omits them. The FPS row is sent and real: it follows the
  server's update rate. The Potential, timer, and counter rows are not sent.
- The Release snapshot still shows tick, time, active cells, clients, and
  the per-cell map.
- A Debug or Profile server's monitor shows the same sections as before the
  change, and server `query_profile` still returns advancing timer rows
  with no monitor connected.

## Notes

- `Documents/Plans/Engine/ReduceGameBaseCpp.md` also edits
  `GameBase::ServerUpdate`. This Plan touches that function only at the
  `kbProfiling` gate near its end (`:517-520`); neither Plan needs the
  other.
