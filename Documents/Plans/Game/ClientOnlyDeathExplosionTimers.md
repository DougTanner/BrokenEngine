<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-12T17:04:43.951Z","dependsOn":[]} -->
# Move the Fighter and Omni death explosion timers and the missile explosion direction out of shared state

## Context

Stage S6 of the split between simulation state and client-only visual state.
It depends on the stage that makes Explosions client-only, after which every
explosion spawn runs only under `BT_CLIENT` but three shared columns still
exist only to feed those spawns. Evidence baseline `e2279c55`; game paths are
relative to `Projects/BrokenEngineSandbox/Source/`.

- `FightersPostRender::pDestroyedExplosionTimes`
  (`Frame/Collections/Fighters/Fighters.h:239`): set at death
  (`FightersCombat.cpp:48`), decremented in PostRender Update
  (`Fighters.cpp:577`, `:690`), reset and read only by the Fighters Spawn
  emission loop (`Fighters.cpp:421-431`), zeroed at spawn (`:518`), logged
  (`:744`). It sits in `SharedMembers()` (`Fighters.h:262`) and
  `SharedCrcMembers()` (`:286`).
- `OmnisPostRender::pDestroyedExplosionTimes` (`Omnis/Omnis.h:298`): set at
  death (`OmnisCombat.cpp:103`), decremented (`Omnis.cpp:594`, `:770`), reset
  and read only by `SpawnDeathExplosions` (`OmnisCombat.cpp:278-318`), zeroed
  at spawn (`Omnis.cpp:304`), logged (`:823`); in `SharedMembers()`
  (`Omnis.h:319`) and the CRC tuple (`:343`).
- `MissilesPostRender::pVecExplosionDirections` (`Missiles/Missiles.h:130`):
  written at spawn (`Missiles.cpp:347`) and in `Explode` (`:416`), read only
  later in the same `Explode` call by `SpawnMissileExplosion` (`:428`); in
  `SharedMembers()` (`Missiles.h:147`) and both
  `PersistentMembers()` branches (`:166`, `:168`).

None of the three is read by a simulation decision, so each violates
`Engine/Source/Frame/AGENTS.md` `## Simulation and Visual State`. None
travels in a transfer payload (the transfer `SpawnInfo` literals name none of
them), so the cross-cell exception does not apply.

Change Workflow tier: Tier 3. Trigger: three shared columns leave the CRC,
wire, and save layouts of three collections, changing save/replay
compatibility.

## Design

1. Fighters and Omnis timers: move `pDestroyedExplosionTimes` from
   `SharedMembers()` and `SharedCrcMembers()` to `ClientMembers()` in both
   collections, and put every read and write of it under `BT_CLIENT`: the
   death write, the Update decrement, the spawn zeroing, and the emission
   loop's reset and test. Its `LogDifferences` line goes, since no
   client-only column is compared there. A row arriving by transfer gets its
   timer from spawn initialization, as every client-only column does.
2. Missile explosion direction: delete the column and every site; `Explode`
   computes the direction into a local and passes it to
   `SpawnMissileExplosion`, inside the `BT_CLIENT` block where that call
   already sits. The author recommends this over a `ClientMembers()` column:
   the value never outlives the `Explode` call that writes it, so a column
   would only carry it from one statement to a later one in that call.
   Also delete it from `Agent/Commands/ServerFrameEdit.cpp:24` and
   `Missiles.cpp:467`.
3. Remove the `pDestroyedExplosionTimes` columns from the `edit_frame` lists
   (`ServerFrameEdit.cpp:21`, `:22`).
4. Bump `FightersPostRender::kiVersion` (`Fighters.h:177`),
   `OmnisPostRender::kiVersion` (`Omnis.h:204`), and
   `MissilesPostRender::kiVersion` by one each; the `Frame::kiVersion` total
   rises with them.

## Critical files

- `Frame/Collections/Fighters/Fighters.h`, `Fighters.cpp`, `FightersCombat.cpp`
- `Frame/Collections/Omnis/Omnis.h`, `Omnis.cpp`, `OmnisCombat.cpp`
- `Frame/Collections/Missiles/Missiles.h`, `Missiles.cpp`
- `Agent/Commands/ServerFrameEdit.cpp`

## In scope

- The two `pDestroyedExplosionTimes` columns and every site Design 1 lists.
- `MissilesPostRender::pVecExplosionDirections` and every site Design 2
  lists, plus the local in `Explode`.
- The `edit_frame` column lists and the three `kiVersion` bumps.
- `Frame/Collections/Fighters/AGENTS.md`, `Omnis/AGENTS.md`, and
  `Missiles/AGENTS.md` only where they state these values are shared.

## Out of scope

- Fighters `pVecDamageDirections`, which drives death knockback.
- Battleship death columns (another stage) and missile pitch (D9).
- Death durations, explosion look, and backward compatibility.

## Coordination

- `Documents/Plans/Game/VisualDrawsLeaveSimulationRandom.md` also edits the
  Missiles `edit_frame` column list (`Agent/Commands/ServerFrameEdit.cpp:24`),
  `MissilesPostRender::SharedMembers()` (`Missiles.h:147`),
  `MissilesPostRender::LogDifferences` (`Missiles.cpp:467`, `:477`), and
  `MissilesPostRender::kiVersion`. Whichever lands second rebases onto the
  other, removes only its own column from each list, and bumps `kiVersion`
  by one more from the landed value.
- `Documents/Plans/Game/WreckHullExplosionsFromConvexHull.md` also edits the
  `edit_frame` column lists (`Agent/Commands/ServerFrameEdit.cpp:21-23`):
  this stage the Fighters and Omnis lines (`:21`, `:22`), that stage the
  Battleships line (`:23`). Whichever lands second rebases onto the other
  and keeps the other's removals.

## Acceptance criteria

- Builds: `/compile` BrokenEngineSandbox client and BrokenEngineSandboxServer
  Debug|x64. No ModelViewer build: no header in its project changes.
- Transfer check: confirm from the Fighters and Omnis Transfer hooks whether
  a dying row can transfer; if it can, a transferred dying row still emits
  its staggered explosions on the client after arrival (screenshot).
- Determinism: combat deaths of a Fighter and an Omni and a missile
  explosion, under `replay_record` then `replay_play`, report no CRC
  mismatch, and the connected client's `get_logs` with a `[Dd]esync` pattern
  returns no lines. The author recommends `inject_payload` spawns with
  `edit_frame` `pfHealths` lowered before an enemy hit; no harness run has
  exercised this setup.
- Visuals: screenshots show the staggered Fighter and Omni death explosions
  and directional missile explosions on terrain as before.
- Version gate: a replay or grid save from before the change is rejected.

## Notes

- Originating gap: the remaining shared columns that only time or orient
  client-only explosion spawns after the client-only Explosions stage.
- Exposure: CRC, wire and save layout of three collections, save/replay
  compatibility, live verification.
