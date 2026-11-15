<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:44:08.993Z","dependsOn":[]} -->
# Remove client state from ReplayMeta and fix Fighter launch terrain admission

## Context

Both problems were found by deep analyses of
`Projects/BrokenEngineSandbox/Source` (architecture phase). Paths are under
`Projects/BrokenEngineSandbox/Source/` unless they start at the repository
root. Line numbers are from the commit each part names; locate by symbol if
they moved. Both parts raise `ReplayMeta::kiVersion`, and one bump covers
both.

### Always-zero client state in replay metadata

Line numbers in this part are from commit `022915b6`. The finding was
confirmed by code reading; nothing was compiled or run.

`ReplayMeta` (`Game.h:32-39`) is the fixed-size value at the head of the replay
metadata file. Besides `clientGridCoordinate` it carries
`iClientOwnedEntityIdentifierValue`, `fPreviousClientArmor`, and `puiPadding`.
Both fields are always written as zero, and the code that reads them back never
does anything:

- The only writer and reader of the file are whole-file `BT_SERVER`:
  `WriteReplayMetadata`, `ReadReplayMetadata`, and `AdoptReplayMetadata` in
  `Save/GameSaveLoad.cpp` (guard at line 3; lines 23-28 and 81), called from
  `Engine/Source/File/Replay.cpp` (guard at line 3). No client code reads
  `ReplayMeta`.
- `WriteReplayMetadata` fills `iClientOwnedEntityIdentifierValue` from
  `Game::ClientOwnedEntityIdentifier()` (`Save/GameSaveLoad.cpp:26`). That
  function's body is `BT_CLIENT`-only (`Game.cpp:52-63`), so on the server it
  returns `{}`.
- It fills `fPreviousClientArmor` from `Game::mfPreviousClientArmor`
  (`Save/GameSaveLoad.cpp:27`). That member becomes non-zero only in
  `Game::UpdateCameraShake` (`Game.cpp:120`) and
  `Network/Client/ClientReconciler.cpp:138`, both client-only.
- So in `Game::RestoreReplayMetadata` (`Game.cpp:319-328`) the
  `AddClientOwnedEntity` branch (lines 322-326) never runs and line 327 always
  stores zero.

The same dead path is the only reason the client owned-entity state compiles
into the server: `Game::mClientOwnedEntities`, `mfPreviousClientArmor`
(`Game.h:123-125`), and `ClientOwnedEntityIdentifier`,
`AddClientOwnedEntity`, `RemoveClientOwnedEntity`, `ClientOwnedEntity`
(`Game.h:87-89`, `:91`; `Game.cpp:50-95`) are unguarded. A search of both
source trees found every other use of these symbols in client-only code:
`FleetSelection.cpp`, `Graphics/Camera.cpp`, `Network/Client/`,
`Ui/Screens/HudScreen.cpp`, and `Ui/Screens/MapScreen.cpp`. Root `AGENTS.md`
`## Client/Server Targets` asks for client-only code to be guarded.

Unmet condition: no server translation unit compiles owned-entity state, and
`ReplayMeta` holds no field that is constant in its only writer.

### Fighter launch terrain admission on an unbuilt elevation grid

Found in `Projects/BrokenEngineSandbox/Source/Network` (simulation
invariants). The root cause is confirmed by a code trace against commit
`4cf9d607`. The symptom was never observed in a running server; `## Design`
step 1 is the first live observation of it.

#### Observed behavior (code trace)

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

#### Verified consequence

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
2. `ReplayMeta` (`Game.h`): delete `iClientOwnedEntityIdentifierValue`,
   `fPreviousClientArmor`, and `puiPadding`; keep `clientGridCoordinate`.
3. `WriteReplayMetadata` (`Save/GameSaveLoad.cpp:23-28`): drop the two
   initializers for the deleted fields.
4. `Game::RestoreReplayMetadata` (`Game.cpp:319-328`): delete lines 322-327,
   leaving the `SetClientGridCoordinate` call.
5. Guard the owned-entity state under `BT_CLIENT`:
   - `Game.h`: the declarations of `ClientOwnedEntityIdentifier`,
     `AddClientOwnedEntity`, `RemoveClientOwnedEntity`, `ClientOwnedEntity`,
     and the members `mClientOwnedEntities`, `mfPreviousClientArmor`.
     `RestoreReplayMetadata` (`Game.h:90`) sits among them and stays outside
     the guard.
   - `Game.cpp`: one `BT_CLIENT` block around lines 50-95; the inner
     `#if`/`#endif` inside `ClientOwnedEntityIdentifier` (lines 52 and 63)
     goes.
   - `Game::ResetGameState` (`Game.cpp:174-185`): move the one `clear()` call
     (lines 176-177) and the `mfPreviousClientArmor` reset (line 184) into the
     existing `BT_CLIENT` block at line 178. The server body is then empty;
     the function keeps its declaration because
     `Engine/Source/GameBase.cpp:1196` calls it in both builds.
6. Fighter admission fix, inside `SpawnFleetMember`: in the Fighter case,
   before the bay loop, build the flagship cell's elevation grid when it is
   empty and the cell lists islands (the same condition the frame tick uses),
   under a `ScopedSuppressAllocationTracking` scope with a `// Heap:` note, by
   calling `engine::gpIslandTerrain->BuildElevationGrid`. The fleet step runs
   on the main thread with no tick running, which is how
   `FlagshipReachesEdge` in `FleetNavigationController.cpp` already builds
   navigation data for the same reason, so the build cannot race the tick,
   and the tick's own build then finds the grid present. A fix that needs
   code outside the region `## In scope` names is surfaced for re-planning.
7. Version: raise `ReplayMeta::kiVersion` (`Game.h`) by one from its value at
   claim (7 when the `ReplayMeta` part was written). This single bump covers
   both parts:
   - The `ReplayMeta` layout change (and therefore the offset of the replay
     fleet records that follow it) makes older replay metadata files
     rejected by the version header. No backward compatibility.
   - The Fighter fix changes which Fighters launch on the affected tick, so
     that tick's post-tick spawn batch and the replay fleet record change;
     the corrected admission changes a launch (spawn-or-wait) decision that
     reaches the replay fleet records' checksum, so a recording made before
     the fix that holds such a launch would no longer play back to its
     recorded decisions and would log a fleet checksum mismatch. On its own
     that part changes no byte format; its reason for rejecting older
     recordings is the root `AGENTS.md` directive's "keep one current format,
     path, or behavior", and the version gate is the existing way to reject
     them. `ReplayMeta::kiVersion` is the owner because the replay metadata
     file carries the replay fleet records and is gated by that version
     (`Projects/BrokenEngineSandbox/Source/AGENTS.md` `## Architecture`,
     `Projects/BrokenEngineSandbox/Source/Save/AGENTS.md` `## Overview`).
   - `Frame::kiVersion` stays unchanged: it also gates saves, and a save's
     bytes and its load are unaffected.
8. Update the spawn bullet in
   `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md`
   `## Deterministic Tick Contracts` to say the fleet step builds an unbuilt
   cell's elevation grid before the Fighter terrain admission, through
   `/update-claude-docs`.

Exposure: the replay metadata file format changes (`ReplayMeta` layout) and
one launch decision changes, both covered by the one `ReplayMeta::kiVersion`
bump. No wire, `Frame`, CRC, grid save, or `.pack` change. Affinity changes for
the step 5 symbols only.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Game.h`
- `Projects/BrokenEngineSandbox/Source/Game.cpp`
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md`

## In scope

- `Game.h`: `ReplayMeta` (its three deleted members and `kiVersion`, raised
  once by one); the `BT_CLIENT` guard around the six declarations `## Design`
  step 5 names.
- `Game.cpp`: `ClientOwnedEntityIdentifier`, `AddClientOwnedEntity`,
  `RemoveClientOwnedEntity`, `ClientOwnedEntity` (guard only, plus the inner
  guard removal); `ResetGameState`; `RestoreReplayMetadata`.
- `Save/GameSaveLoad.cpp`: the `ReplayMeta` initializer in
  `WriteReplayMetadata`.
- `ServerClientManager::SpawnFleetMember`: the Fighter case, from the
  flagship cell lookup to the bay loop. This region bounds the Fighter fix.
- The Fighter launch sentence of the spawn bullet in `Server/AGENTS.md`
  `## Deterministic Tick Contracts`.
- Other documentation sync that `/update-claude-docs` finds necessary.

## Out of scope

- `clientGridCoordinate`, the replay fleet records, the fleet checksums, and
  every other part of the replay metadata file.
- Moving `ReplayMeta` or `ReplayStagedMeta` out of `Game.h`, guarding
  `ReplayMeta` or `RestoreReplayMetadata` under `BT_SERVER`, and include
  cleanup in `Game.h`.
- `ConfirmedClientState::fPreviousClientArmor` and the reconciler's handling
  of it (`Network/Client/`).
- Any behavior change in `Game::UpdateCameraShake`.
- Backward compatibility with older replay files.
- Building elevation grids at load, at cell creation, or anywhere in engine
  code; `Engine/Source/Frame/FrameBase.cpp` is unchanged.
- `ComputeTerrainAvoidance`, the elevation sampler, and what an empty grid
  samples.
- The Omni and Battleship spawn search, the launch cadence
  (`mNextFighterLaunchTicks`), and `RaiseAboveTerrain`.
- `Frame::kiVersion`, save bytes, and the wire.

## Risk tier

Tier 3 — invariant/integration. Triggers: the change alters a save/replay file
layout (`ReplayMeta`) and bumps its version; determinism and save/replay
compatibility — the Fighter fix changes a launch decision that reaches the
tick's spawn batch and the fleet checksum.

## Acceptance criteria

1. The baseline check of `## Design` step 1, run on the unedited tree, is
   recorded with its harness setup and shows the launch.
2. After the fix, the same check no longer shows the symptom: on the first
   fleet step after the load, the first bay in bay order whose launch
   position is inside the cell, which the terrain admission refuses on later
   ticks, is refused on that step too.
3. Code read: `ReplayMeta` holds only `clientGridCoordinate`;
   `ReplayMeta::kiVersion` is exactly one higher than the value found at
   claim; `Frame::kiVersion` is unchanged.
4. Code read: the six symbols `## Design` step 5 names are declared and
   defined only under `BT_CLIENT`.
5. Code read: the Fighter terrain admission in `SpawnFleetMember` never runs
   on a flagship cell that lists islands and has an empty elevation grid.
6. `/compile` builds the BrokenEngineSandbox client and server. The guard was
   not compiled when the `ReplayMeta` part was written, so a server use the
   search missed shows up here.
7. The repository static checks pass.
8. Harness (`/agent-harness`): one run of the replay determinism acceptance in
   `.agents/skills/agent-harness/references/replay.md`
   `### Replay determinism acceptance`, over a recording that starts right
   after a load and includes a Fighter launch, shows no frame CRC mismatch and
   no fleet checksum mismatch. This run covers both the `ReplayMeta` layout
   change and the Fighter fix. The supplied evidence records no harness setup;
   the executing session sets it up from that reference.

## Notes

- Not checked when the `ReplayMeta` part was written: the ModelViewer build
  (it has its own `Game.h` and does not compile these files) and the private
  Coretide project, which is absent from session worktrees.
- Observations only, not work items: after this change
  `Game::RestoreReplayMetadata` is one `SetClientGridCoordinate` call, and
  `Game.h:10` includes `Network/Server/ServerFleetSerialization.h` only for
  `ReplayFleetRecord` in `ReplayStagedMeta`.

## Coordination

No directional dependencies. Mandatory nondirectional coordination:

- `Documents/Plans/Game/FleetOwnedSquadronState.md` rewrites
  `ServerClientManager::SpawnFleetMember` into placement only, edits
  `Save/GameSaveLoad.cpp` (a different function), and also bumps
  `ReplayMeta::kiVersion`. Whichever lands second applies its change to the
  function as the other left it: the grid build stays directly before the
  Fighter bay loop. Each Plan raises the version once from the value it finds
  at claim and does not reuse the other's bump.
