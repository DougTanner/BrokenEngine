<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:44:08.993Z","dependsOn":[]} -->
# Fighter launch terrain admission on a cell whose elevation grid is unbuilt

## Context

Found by a deep analysis of `Projects/BrokenEngineSandbox/Source/Network`
(architecture phase, simulation invariants). The root cause is confirmed by a
code trace against commit `4cf9d607`. The symptom was never observed in a
running server; `## Design` step 1 is the first live observation of it. Paths
are from the repository root.

### Observed behavior (code trace)

`ServerClientManager::SpawnFleetMember`
(`Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp:178`)
launches a Fighter from a bay of its fleet's Battleship. For each bay it runs
a terrain admission (`:276-281`): it calls `ComputeTerrainAvoidance` on the
flagship cell's static data at the launch position and heading, and skips the
bay when the result is not `0.0f`.

That admission runs in the fleet step, which the engine's `PrepareTick` runs
before the tick's dispatch. On the server the cell's elevation grid is built
only inside the tick, at the cell's first dispatch
(`Engine/Source/Frame/FrameBase.cpp`, the
`elevationGrid.empty() && !islands.empty()` block in the frame tick). So the
admission can run on a cell whose grid is still empty:

- After a load: the grid-save reader builds each cell with its islands only,
  so every grid is empty until that cell's first tick.
  `ServerFleetManager::RebuildFleetMembersForLoad` leaves a member with no
  row dead with a zero countdown, and `ServerClientManager::ResetState`
  clears the launch ticks, so the first fleet step after the load launches
  every such Fighter whose Battleship is alive, in a cell that has not
  ticked.
- After a flagship transfer into a newly created cell: the destination is
  created with its islands only, and the Battleship's row is in it before the
  cell's first dispatch on the next tick.

With an empty grid the elevation sampler holds no grid and every sample
returns the sea floor (`common::kfSeaBottomMeters`), so in
`ComputeTerrainAvoidance`
(`Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.cpp`) both averages
stay below the avoidance minimum and it returns the `0.0f` it was passed. The
admission therefore cannot refuse on those ticks, whatever the terrain.

### Verified consequence

Milder than a lost ship: a live Fighter takes no terrain contact
(`ApplyTerrainBounce` runs only for a dying row) and is lifted by
`RaiseAboveTerrain`, so a Fighter launched toward terrain is raised over it
and climbs to its plane. The trace found no replay divergence: playback
reruns the fleet step on the same empty grids, the start record then replaces
the result, and a cell activated by a transfer first dispatches one tick
later in both runs. That was not proven for every activation path.

The unmet condition: the launch admission should answer the same whether or
not the Battleship's cell has ticked.

## Design

1. Baseline check, on the unedited tree before the first edit, through
   `/agent-harness`: on a server with a fleet that has one dead Fighter
   member and whose Battleship stands where the terrain admission refuses,
   on ordinary ticks, the first bay in bay order whose launch position is
   inside the cell, `save` and `load`, and observe the first fleet step
   after the load. The refused bay must be that one because the bay loop
   (`ServerClientManager.cpp:265-291`) walks the bays in order from the
   fleet's first bay, skips a bay whose launch position is outside the cell
   or that the admission refuses, and launches from the first bay it admits
   and returns: a clear in-cell bay earlier in bay order gives the same
   launch with or without the elevation grid. The expected result is that
   the Fighter launches from that bay, which the same state refuses one tick
   later; record the harness setup and what a client sees. The code trace in
   `## Context` proved the mechanism, and this check is the first live
   observation of the symptom and the baseline the acceptance criteria
   compare against. The supplied evidence records no harness setup for this
   state. If the check does not show that launch, stop and report for
   re-planning instead of changing the launch decision.
2. Make the fix inside `SpawnFleetMember`: in the Fighter case, before the
   bay loop, build the flagship cell's elevation grid when it is empty and
   the cell lists islands (the same condition the frame tick uses), under a
   `ScopedSuppressAllocationTracking` scope with a `// Heap:` note, by
   calling `engine::gpIslandTerrain->BuildElevationGrid`. The fleet step
   runs on the main thread with no tick running, which is how
   `FlagshipReachesEdge` in `FleetNavigationController.cpp` already builds
   navigation data for the same reason, so the build cannot race the tick,
   and the tick's own build then finds the grid present. A fix that needs
   code outside the region `## In scope` names is surfaced for re-planning.
3. Version. The fix changes which Fighters launch on the affected tick, so
   that tick's post-tick spawn batch and the replay fleet record change. No
   byte format changes, but the corrected admission changes a launch
   (spawn-or-wait) decision that reaches the replay fleet records' checksum,
   so a recording made before the fix that holds such a launch would no
   longer play back to its recorded decisions and would log a fleet checksum
   mismatch. Raise `ReplayMeta::kiVersion`
   (`Projects/BrokenEngineSandbox/Source/Game.h`) by one so older recordings
   are rejected. The bump is this Plan's own choice, not a format bump: the
   bump clause of the root `AGENTS.md` backward-compatibility directive
   covers a changed format, and no format changes here. The choice rests on
   that directive's other half, "keep one current format, path, or
   behavior": older recordings are rejected rather than supported, and the
   version gate is the existing way to reject them.
   `ReplayMeta::kiVersion` is the owner because the replay metadata file
   carries the replay fleet records and is gated by that version
   (`Projects/BrokenEngineSandbox/Source/AGENTS.md` `## Architecture`,
   `Projects/BrokenEngineSandbox/Source/Save/AGENTS.md` `## Overview`).
   `Frame::kiVersion` stays unchanged: it also gates saves, and a save's
   bytes and its load are unaffected.
4. Update the spawn bullet in
   `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md`
   `## Deterministic Tick Contracts` to say the fleet step builds an unbuilt
   cell's elevation grid before the Fighter terrain admission, through
   `/update-claude-docs`.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp`
- `Projects/BrokenEngineSandbox/Source/Game.h`
- `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md`

## In scope

- `ServerClientManager::SpawnFleetMember`: the Fighter case, from the
  flagship cell lookup to the bay loop. This region bounds the fix.
- `ReplayMeta::kiVersion` in `Game.h`, raised by one.
- The Fighter launch sentence of the spawn bullet in `Server/AGENTS.md`
  `## Deterministic Tick Contracts`.

## Out of scope

- Building elevation grids at load, at cell creation, or anywhere in engine
  code; `Engine/Source/Frame/FrameBase.cpp` is unchanged.
- `ComputeTerrainAvoidance`, the elevation sampler, and what an empty grid
  samples.
- The Omni and Battleship spawn search, the launch cadence
  (`mNextFighterLaunchTicks`), and `RaiseAboveTerrain`.
- `Frame::kiVersion`, save bytes, and the wire.

## Risk tier

Tier 3 — invariant/integration. Trigger: determinism and save/replay
compatibility — the fix changes a launch decision that reaches the tick's
spawn batch and the fleet checksum, and the design raises a version that
gates replays.

## Acceptance criteria

- The baseline check of `## Design` step 1, run on the unedited tree, is
  recorded with its harness setup and shows the launch.
- After the fix, the same check no longer shows the symptom: on the first
  fleet step after the load, the first bay in bay order whose launch
  position is inside the cell, which the terrain admission refuses on later
  ticks, is refused on that step too.
- Code read: `ReplayMeta::kiVersion` in `Game.h` is one higher than the
  value found at claim, and `Frame::kiVersion` is unchanged.
- Code read: the Fighter terrain admission in `SpawnFleetMember` never runs
  on a flagship cell that lists islands and has an empty elevation grid.
- `/compile` builds the BrokenEngineSandbox server and client.
- The repository static checks pass.
- Harness: the replay determinism acceptance in
  `.agents/skills/agent-harness/references/replay.md`
  `### Replay determinism acceptance`, over a recording that starts right
  after a load and includes a Fighter launch, shows no frame CRC mismatch and
  no fleet checksum mismatch.

## Coordination

No directional dependencies. Mandatory nondirectional coordination:

- `Documents/Plans/Game/FleetOwnedSquadronState.md` rewrites
  `ServerClientManager::SpawnFleetMember` into placement only and also bumps
  `ReplayMeta::kiVersion`. Whichever lands second applies its change to the
  function as the other left it: the grid build stays directly before the
  Fighter bay loop. Each Plan raises the version once from the value it finds
  at claim and does not reuse the other's bump.
- `Documents/Plans/Game/RemoveClientStateFromReplayMeta.md` also bumps
  `ReplayMeta::kiVersion`. Each Plan raises the version once from the value
  it finds at claim and does not reuse the other's bump.
