<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-31T16:24:35.571Z","dependsOn":[]} -->
# Reject out-of-range Fighter beam and Battleship missile cooldowns at read and harness boundaries

## Context

Root `AGENTS.md` (trust policy) says the server fully validates every grid
save under the bad-value rule in `.agents/references/cpp-conventions.md`. Two
ship weapon cooldowns are read from saves, replays, and `edit_frame` commits
without any check, while the Omni blaster cooldown is checked. A review of the
change that added the Battleship missile cooldown (built on purpose exactly
like the Fighter beam cooldown, so that change added no check) left this as a
pre-existing, out-of-scope residual.

Line numbers below were taken from the tree when this Plan was written; locate
each site by the named symbol if lines have moved.

Read from source:

- Precedent: `OmnisPostRender::IsBlasterFireTimeInRange`
  (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.h:211-216`)
  is one range test that also fails NaN and both infinities;
  `OmnisPostRender::PostRead` (`Omnis.h:225-228`) throws
  `std::ios_base::failure("OmnisPostRender pNextBlasterFireTimes")` on a row
  that fails it, and the `inject_payload` admission `BuildInjectedEntry`
  (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp:347-350`)
  refuses a `SpawnOmni` `nextBlasterFireTime` that fails it.
- `FightersPostRender::PostRead`
  (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h:174-198`)
  checks `pfHealths`, `pController`, `pNavigationDelays`, and the target
  flight plane, but not `pNextBeamFireTimes` (`Fighters.h:220`).
- `BattleshipsPostRender::PostRead`
  (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.h:171-199`)
  checks the same four plus `puiNextMissileTubes`, but not
  `pNextMissileFireTimes` (`Battleships.h:225`).
- `BuildInjectedEntry` checks nothing on a `SpawnFighter` `nextBeamFireTime`
  (`ServerSimulationHarnessRigs.cpp:333-334`) or a `SpawnBattleship`
  `nextMissileFireTime` (`:353-361`, which checks only `uiNextMissileTube`).

Values the simulation produces:

- Both cooldowns count down by the tick delta every tick, with no lower clamp
  (`Fighters.cpp:531`, `Battleships.cpp:477`), so a ship with no target holds
  an ever more negative finite value; float subtraction stops changing it
  long before it could reach -inf.
- A Fighter fires when its cooldown is below zero and then sets it to
  `kFighterBeamCooldown`, 1.0 s
  (`FightersCombat.cpp:241-248`, `Fighters.h:60`). A Battleship launches when
  its cooldown is at or below zero and then sets it to
  `kBattleshipMissileCooldown`, 0.125 s
  (`BattleshipsCombat.cpp:240-244` and `:327`, `Battleships.h:64`).
  Spawn defaults are zero (`Fighters.h:281`, `Battleships.h:285`).
- So every stored value is finite and at most its type's cooldown constant.

Effect of a bad value read from a save, following from the fire tests above:
+inf silences that ship's weapon permanently, and a finite value far above the
cooldown silences it for that long. NaN stays NaN through every decrement; the
Fighter's `!(x < 0)` skip test is true for NaN, so that Fighter never fires
again, while the Battleship's `x > 0` skip test is false for NaN, so that
Battleship launches at once and skips one cooldown.

`edit_frame` commits through the save reader's checks, `PostRead` hooks
included (`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`
`edit_frame` section), so a `PostRead` check covers it with no harness change.

## Design

1. The author recommends adding to `FightersPostRender` a static
   `IsBeamFireTimeInRange(float fTime)` and to `BattleshipsPostRender` a static
   `IsMissileFireTimeInRange(float fTime)`, each written as one range test
   that fails NaN and both infinities, like `IsBlasterFireTimeInRange`:
   `fTime >= -std::numeric_limits<float>::max() && fTime <= <cooldown>.count()`.
   The lower bound is the finite floor, not zero, because a ship with no
   target legitimately holds a large negative cooldown (Context). Rationale for
   a named predicate over an inline test: `PostRead` and `BuildInjectedEntry`
   both need it, as with the Omni precedent.
2. Call it first in each `PostRead` row loop beside the existing checks,
   throwing `std::ios_base::failure("FightersPostRender pNextBeamFireTimes")`
   and `std::ios_base::failure("BattleshipsPostRender pNextMissileFireTimes")`,
   and extend each `PostRead` comment's list of rejected values with one
   clause, as the Omni comment does.
3. In `BuildInjectedEntry`, refuse a `SpawnFighter` `nextBeamFireTime` and a
   `SpawnBattleship` `nextMissileFireTime` that fail the predicate with the
   same `"'{}' is out of range"` message and `FRAME_COLUMN(...)::kName` form
   the `nextBlasterFireTime` and `uiNextMissileTube` refusals use. The
   `SpawnFighter` case becomes a braced block like `SpawnOmni`.
4. Update the docs the change makes incorrect: the Fighter and Battleship
   `AGENTS.md` weapon-invariant bullets gain the range the simulation keeps and
   the `PostRead` rejection (the Omni `## State Boundaries` bullet is the
   model; the Battleship file already has a matching `puiNextMissileTubes`
   bullet), and the `inject_payload` refusal list in `commands-server.md`
   (around line 285, beside `nextBlasterFireTime`) gains both fields.
5. No `kiVersion` bump: the stored bytes and their meaning are unchanged, and
   no value the simulation writes is newly rejected.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.h`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/AGENTS.md`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`

## In scope

- The two new predicates and the `PostRead` row loops and comments of
  `FightersPostRender` and `BattleshipsPostRender`.
- The `kSpawnFighter` and `kSpawnBattleship` cases of `BuildInjectedEntry`.
- The Fighter and Battleship weapon-invariant bullets and the
  `inject_payload` refusal list in `commands-server.md`.

## Out of scope

- The countdown and fire logic (`Fighters.cpp`, `Battleships.cpp`,
  `FightersCombat.cpp`, `BattleshipsCombat.cpp`), including any lower clamp on
  the countdown and any `ASSERT` like the Omni `SpawnBlasters` one, which
  guards a burst loop these types do not have.
- Replay `FrameInput` admission (`AdmitFrameInputStatusChange`) and transfer
  status-change payloads: the Omni precedent checks neither, and
  server-to-client data is trusted.
- The Omni checks, other ship columns, and any `kiVersion`.

## Risk and invariants

- Tier trigger: Tier 2 — adding checks at existing read boundaries
  (`PostRead` for save, replay, and `edit_frame`; the `inject_payload`
  admission) with the format and the trust unchanged. Accepted values behave
  exactly as before, so CRC, determinism, wire, and save layout are unchanged.
- Rejection follows the bad-value rule: throw through the existing
  `PostRead` failure path, never clamp.

## Acceptance criteria

1. BrokenEngineSandbox client and server Debug|x64 build with no new warnings.
2. Live, through `/agent-harness` on a paused server: `edit_frame` writing a
   Fighter `pNextBeamFireTimes` of 2.0 and, separately, a Battleship
   `pNextMissileFireTimes` of 1.0 returns `ok:false` with the frame unchanged;
   writing -100.0 to either succeeds. No earlier run recorded a reproducing
   setup; the Agent Harness `edit_frame` recipe (pause, `create_cell` or
   `inject_payload` spawn, then `edit_frame`) is the expected route.
3. `inject_payload` refuses a `SpawnFighter` `nextBeamFireTime` of 2.0 and a
   `SpawnBattleship` `nextMissileFireTime` of 1.0 with the out-of-range
   message, and accepts the zero default.

## Notes

- Ordinary debt follow-up; no Plan covered this root cause when this Plan was
  written.
