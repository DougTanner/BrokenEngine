<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-30T20:37:36.237Z","dependsOn":[]} -->
# Keep ship beams from hitting a row DestroyShip marked for removal this tick

## Context

A review of the Fighter hit-scan beam change left this residual,
pre-existing and widened by that change: `ForEachEnemyShip` does not skip
`kDestroy` ship rows, so a row the `DestroyShip` status change removes this
tick can still take a Battleship beam or (now) a Fighter beam, and so start an
explosion, before compaction removes it. It is deterministic and reachable
only through a developer command. No acceptance criterion of that change was
unmet. The root cause has not been confirmed by a dedicated diagnosis, and no
run has reproduced it.

Line numbers below were taken from the tree when this Plan was written; locate
each site by the named symbol if lines have moved.

Code path read from source:

- `ForEachEnemyShip`
  (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipQueries.h:47-71`)
  skips a row carrying `engine::LifecycleFlags::kDying` (`:56`), a row inside
  arrival grace, and a non-enemy alignment; it has no `kDestroy` test. Its
  comment says it visits "every non-exploding row".
- `ApplyUpdateStatusChanges`
  (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipStatusChanges.cpp:80-88`)
  applies `kDestroyShip` by setting only `kDestroy` on the row (`:85`), in
  Update after every ship Update (`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`).
- `engine::CompactCollections` (`Engine/Source/Frame/FrameUtils.h:388-433`),
  the last step of the Destroy phase, is the only removal of a `kDestroy` row,
  so the row stays in the current frame through PostCollision.
- Battleship `FireBeam`
  (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsCombat.cpp:85`)
  runs in PostCollision over the current frame and passes each
  `ForEachEnemyShip` row (`:98`) through only a `kTransfer`, plane-0 and range
  test, so a `kDestroy` row can be chosen and handed to the target type's
  `ApplyBeamDamage`; its own missile fallback already skips `kDestroy` missiles
  (`:158`). Fighter `FireBeam`
  (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/FightersCombat.cpp:65`,
  `ForEachEnemyShip` at `:77`, damage at `:113-116`) applies plane, in-cell,
  range and aim tests only.
- Each type's `ApplyBeamDamage` (`Fighters.h:209`, `Omnis.h:244`,
  `Battleships.h:206`) lowers health and, at zero, starts the natural
  explosion (Battleship and Fighter `BeginExplosion`, for example
  `BattleshipsCombat.cpp:37-59`): it sets `kDying`, releases the pusher,
  clears the registry id and spawns an explosion row, on a row compaction then
  removes in the same tick.
- Producers of `kDestroy` on a ship row outside its own death path:
  `inject_payload` `DestroyShip` entries
  (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp:392-409`)
  and `edit_frame` kill-state writes, which the next tick's compaction removes
  (`Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` `## Contracts`). No
  simulation code emits `kDestroyShip`.
- The other current-frame caller, Omni `FindMissileAimTarget`
  (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/OmnisCombat.cpp:47-51`),
  shares the same filter; every other caller reads the previous frame or the
  render frame.

## Design

1. Confirm the root cause first with `/external-diagnose-bug`: that a row
   marked `kDestroy` by `DestroyShip` (or by `edit_frame`) reaches either
   `FireBeam` through `ForEachEnemyShip` and can start an explosion before
   compaction. If the diagnosis refutes it, stop and report.
2. Then make the smallest fix inside the region `## In scope` names. The
   author recommends skipping a `kDestroy` row in `ForEachEnemyShip` beside
   the existing `kDying` skip and updating its comment: one site covers both
   beams and the Omni missile aim, a row leaving the cell this tick is never a
   valid target, and on the previous and render frames the test changes only
   rows an `edit_frame` kill-state write left there. The alternative, a
   `kDestroy` skip in each `FireBeam` like the Battleship missile fallback,
   leaves the Omni missile aim unchanged. A fix outside the region is surfaced
   for re-planning, not made.
3. The fix changes CRC'd simulation results only on ticks where a developer
   command marked a ship row `kDestroy`. The executing session decides from
   `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` whether the
   `Frame::kiVersion` base needs a bump.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipQueries.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsCombat.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/FightersCombat.cpp`

## In scope

- The root-cause investigation in Design 1.
- Fix region: the row filter and comment of `ForEachEnemyShip`
  (`ShipQueries.h`), and the `ForEachEnemyShip` visitor lambdas of Battleship
  `FireBeam` and Fighter `FireBeam`.
- A `Frame::kiVersion` base bump, only if Design 3 finds one is needed.

## Out of scope

- `ApplyUpdateStatusChanges` and the `DestroyShip` payload, its harness
  parsing, and `edit_frame`.
- Each type's `ApplyBeamDamage` and `BeginExplosion`, beam range, aim, plane
  and priority rules, and the Battleship missile fallback.
- Collision layers (blaster hits on a `kDestroy` row) and frame-registry
  eligibility of a `kDestroy` row: separate mechanisms; surface them if the
  diagnosis shows them affected.
- `engine::CompactCollections` and the engine lifecycle flags.

## Risk and invariants

- Tier trigger: Tier 3 — determinism/CRC: the fix changes which row a CRC'd
  beam hits and whether an explosion row is spawned.
- Client and server run the same filter, so the change stays deterministic;
  replay recordings that inject `DestroyShip` near a beam shooter may play
  back differently (Design 3).
- No wire, save layout or threading change.

## Acceptance criteria

1. The diagnosis records the confirmed root cause, or the Plan stops at
   Design 1.
2. The recorded symptom no longer reproduces: after the fix, a `kDestroy` ship
   row takes no Battleship or Fighter beam hit and starts no explosion in the
   tick it is marked. The supplied evidence records no harness setup that
   reproduced it; the diagnosis names one, or a code read of the fix region
   settles it.
3. BrokenEngineSandbox client and server Debug|x64 build with no new
   warnings, and a `/agent-harness` replay determinism check passes.

## Notes

- Ordinary debt follow-up from the Fighter hit-scan beam change; no Plan
  covered this root cause when this Plan was written.
