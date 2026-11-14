<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:07:34.818Z","dependsOn":[]} -->
# Remove the BlastersRendered profile counter

## Context

Found by the in-function refactoring phase of a deep analysis of
`Projects/BrokenEngineSandbox/Source/Frame` (direct files only), handed to its
architecture phase, and verified independently. Pre-existing at commit
`022915b6`; line numbers are from that commit, except the `Frame.cpp` ones,
which are from the commit that added this Plan, so locate by symbol if they
moved. `BES/` abbreviates `Projects/BrokenEngineSandbox/Source/`.

`kCpuCounterBlastersRendered` (`BES/Profile/ProfileRows.h:13`, name
`"    Rendered"` at `:64`) always equals `kCpuCounterBlasters`. Its only
writer in the repository is `BES/Frame/Frame.cpp:613`, inside
`FrameInterpolate::BeginRender`, which assigns the same `iTotalBlasters` that
`:612` assigns to `kCpuCounterBlasters`. Both sit in the six flat counter
assignments (`:611-616`) below the function's one
`if constexpr (!kbProfiling)` early return (`:587-590`).

Blasters have no CPU render function that could cull a subset
(`BES/Frame/Collections/Blasters/Blasters.h` declares no `Render`). Missiles,
Fighters, and Battleships differ: their render files write their own Rendered
counters. On the server `PublishServerEntityCounts`
(`BES/Frame/ServerCellStats.cpp`) writes `kCpuCounterBlasters` only, so the
Rendered row is never written there.

Every reader is a generic loop over all counters: the profile overlay
(`Engine/Source/Profile/ProfileScreens.cpp`), the harness profile output on
both sides (`Engine/Source/Agent/AgentCommandsClientGeneric.cpp`,
`AgentCommandsServerGeneric.cpp`), and the dump log
(`Engine/Source/Profile/ProfileManager.cpp`). Nothing reads the row by name.

Unmet criterion: no profile counter that always duplicates another. The row
also shows a number labelled "Rendered" that is not a rendered subset.

## Design

Recommended: delete the counter — its enumerator, its name-table entry, and
its writer — because it carries no information and its label misleads. Omnis
is the precedent for a population row with no Rendered row
(`ProfileRows.h:11`).

1. `BES/Profile/ProfileRows.h`: remove `kCpuCounterBlastersRendered` from
   `GameCpuCounters` and the `"    Rendered"` entry that follows `"Blasters"`
   in `kpGameCpuCounterNames`. The `static_assert` below the table checks that
   both were removed together.
2. `BES/Frame/Frame.cpp`, `FrameInterpolate::BeginRender`: remove the
   `GetCpuCounter(kCpuCounterBlastersRendered)` assignment (`:613`). The
   early return above it and the other five assignments stay.

Removing only the writer would leave a row that always reads 0, so the two
files change together.

Removing the row lowers the index of every later game counter in the harness
`counters[].index` field and in the dump log. No by-index consumer was found
in the repository, and the engine pins only `kGameCpuCounterFirst` and
`kGameCpuCounterCount` (`BES/Profile/AGENTS.md` `## Invariants`), neither of
which this removes.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Profile/ProfileRows.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`

## In scope

- `kCpuCounterBlastersRendered` in `GameCpuCounters`.
- The `"    Rendered"` entry under `"Blasters"` in `kpGameCpuCounterNames`.
- The `kCpuCounterBlastersRendered` assignment in
  `FrameInterpolate::BeginRender`.

## Out of scope

- `kCpuCounterBlasters` and every other counter, name, and timer.
- The Missiles, Fighters, and Battleships Rendered counters, which are real
  subsets.
- The engine profile overlay, harness output, and dump log code.
- The rest of `FrameInterpolate::BeginRender`, including its
  `if constexpr (!kbProfiling)` early return and the other five counter
  assignments.
- `BES/Profile/AGENTS.md`: its text names no removed symbol.

## Risk tier

Tier 2 — scoped behavior. Trigger: one subsystem's runtime and tool behavior
changes — the profile overlay loses a row, and the harness and dump-log index
of each later game counter drops by one. Client and server profiling only: no
determinism/CRC, serialization, wire, replay, or threading exposure.

## Acceptance criteria

- No `kCpuCounterBlastersRendered` identifier remains in the tracked tree, and
  `kpGameCpuCounterNames` holds no `"    Rendered"` entry directly after
  `"Blasters"`.
- The BrokenEngineSandbox client and server build (the `static_assert` on the
  name table passes).
- Through `/agent-harness`, the client profile output lists `Blasters`
  followed by `Missiles`, with no Rendered row between them. The supplied
  evidence records no harness setup.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- The private game's source was not available to the authoring session, so
  it was not searched for this enumerator. The executing session confirms no
  private-game use where that source is available and reports it as
  unconfirmed otherwise.
