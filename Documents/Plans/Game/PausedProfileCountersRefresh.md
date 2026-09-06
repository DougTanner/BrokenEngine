<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:52:29.792Z","dependsOn":[]} -->
# Refresh server profile counters after paused loads

## Context

The retained survivor `CAI/shard-0039/001` identifies stale server monitoring
counters after a paused state replacement. `ServerUpdateDisplayStats` recomputes
left-panel totals from active frames at
`Engine/Source/Server/ServerDisplay.cpp:96-110`, but the four game profile
counters are written only by `PublishServerEntityCounts` at
`Projects/BrokenEngineSandbox/Source/Frame/ServerCellStats.cpp:31-55`, called
from `ServerSession::FinalizeTickClients` during an advancing tick
(`Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:281-292`).
`pauseAfterLoad` can prevent that tick while the display still repaints and
`PaintProfilePanel` formats the old counters at
`ServerDisplay.cpp:455-463`.

The direct evidence is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0039.md:44`
and consolidated selector `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:993`. Frozen/live source
identity matches baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`; no source
was changed during routing. The accepted `load` command's `pauseAfterLoad`
parameter is a normal supported zero-tick state, not an error path.

## Design

The author's recommendation is to invoke the existing
`PublishServerEntityCounts` publication at the state-replacement boundary
after a valid load has installed the new frames, before the paused display or
agent query can read the counters. Keep one game-owned writer and reuse the
same frame population source; do not make the engine display recompute game
counters through a second path. Normal advancing-tick publication remains
unchanged.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:74-103` — load
  state replacement and pause handling.
- `Projects/BrokenEngineSandbox/Source/Frame/ServerCellStats.cpp:31-55` —
  game counter publisher.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:281-292`
  — existing advancing-tick writer.
- `Engine/Source/Server/ServerDisplay.cpp:96-124,455-482` — display totals,
  profile panel, and memory-independent counter reads.
- `Engine/Source/Server/AGENTS.md` and
  `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` — cadence and sole-
  writer contracts.

## In scope

- Entity-counter publication at valid save/load state replacement.
- Paused display and profile-query reads immediately after `pauseAfterLoad`.
- Existing advancing-tick publication and left-panel totals.

## Out of scope

- Save format, frame population semantics, pause policy, or profile layout.
- Memory peak invalidation, timer counters, and unrelated display text.
- Adding a second counter store or changing the network/query payload.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: this is scoped server monitoring
state-refresh behavior; the counters are display/diagnostic values outside
Frame CRC, wire, and save serialization.

Preserve these invariants:

- Players, Spaceships, Blasters, and Missiles counters describe the same
  authoritative frame state as the display after every load.
- The game remains the sole publisher of game entity counters.
- A paused/zero-tick update can display and query loaded state without requiring
  a later simulation tick; advancing-tick values remain unchanged.

## Acceptance criteria

- Loading a valid save with `pauseAfterLoad:true` and different populations
  updates both left-panel totals and Profile entity rows before the next tick.
- The server profile query returns the same post-load populations when it uses
  those counters.
- Normal unpaused load and advancing-tick counter publication remain correct.
- Server `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0039/001`; source selector is the shard line above and the
consolidated selector is `consolidated-index.md:993`. No source fix or build
was performed during routing.
