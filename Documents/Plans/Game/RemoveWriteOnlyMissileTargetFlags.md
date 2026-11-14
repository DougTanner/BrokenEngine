<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:09:55.048Z","dependsOn":[]} -->
# Remove the write-only Missile target flags and carry pFlags across cells

## Context

Paths under `Frame/Collections/` are relative to
`Projects/BrokenEngineSandbox/Source/`. Line numbers are from commit
`022915b6`; locate by symbol if they moved. Nothing was built or run for this
Plan: every statement is from a code read.

`MissileFlags` (`Frame/Collections/Missiles/Missiles.h:33-38`) has three
values. `kTargetFleet` and `kTargetRandom` are written and never tested:

- writers: `MissilesPostRender::Transfer`
  (`Frame/Collections/Missiles/Missiles.cpp:278`), the Battleship launch
  (`Frame/Collections/Battleships/BattleshipsCombat.cpp:324`, passed at 333),
  and the `kSpawnMissile` case of the harness spawn parser
  (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp:384-388`);
- readers: none. Every `pFlags` test in the tree is `kFalling`
  (`Missiles.cpp:90`, 115, 123, 147, 362, 367, 384; `MissilesUpdate.cpp:60`,
  108, 192, 210, 290; `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:93`).

The values ride in `pFlags`, a `SharedMembers()` column (`Missiles.h:142`),
and in `SpawnInfo::flags`, so they are part of the CRC, the save, and the
transfer payload (`Engine/Source/Frame/AGENTS.md`
`## Simulation and Visual State`) while deciding nothing.

Related, in the same statements: `Transfer` builds a fresh `MissileFlags_t`
(`Missiles.cpp:277-278`) and sends it (`Missiles.cpp:288`), so `kFalling` is
dropped at the cell edge. `MissilesPostRender::Spawn` sets it again when the
carried `time` is at or past `kMissileLifetime` (`Missiles.cpp:342-346`).
`Frame/Collections/Missiles/AGENTS.md` `## Invariants` says falling state
crosses cell boundaries and the arrival restores it verbatim. The two agree
for every missile the simulation itself makes falling, because `Update` calls
`Fall` on the tick `time` reaches the lifetime (`MissilesUpdate.cpp:210-214`).
They disagree only for a harness-injected row spawned with `kFalling` set and
`time` below the lifetime: it gets `pfDeltaRotationMaximum` 0
(`Missiles.cpp:360-362`), crosses, arrives flying with that 0 restored, and
`MissilesUpdate.cpp:114` then divides by it. That case was derived by reading
and was not run.

Originating gap: an architecture review of the Missiles collection, not an
unmet acceptance criterion of any change. Required condition that is false
today: CRC'd state holds no value nothing reads, and falling state crosses by
value.

## Design

The author's recommendations, with rationale:

1. Delete `kTargetFleet` and `kTargetRandom` from `MissileFlags`, leaving
   `kFalling` at its current value. Rationale: nothing reads them.
2. Delete the three writers: the `missileFlags` local and its `Set` call in
   `Transfer`; the `missileFlags` local and the `.flags` initializer in the
   Battleship launch; the default-flags block and its comment in the harness
   `kSpawnMissile` case, which then only calls `ParseSpawnEntry`.
3. In `Transfer`, send `.flags = rCurrentPostRender.pFlags[i]`. Rationale: it
   is what `Frame/Collections/Missiles/AGENTS.md` already states, and it
   removes the harness-only wrong arrival described above.
4. Leave the `kFalling` derivation in `Spawn` (`Missiles.cpp:343-346`) as it
   is. Rationale: a harness `SpawnMissile` entry with `time` at or past the
   lifetime and no `flags` still relies on it, and for an arrival that already
   carries `kFalling` it changes nothing.
5. Increment `MissilesPostRender::kiVersion` (`Missiles.h:114`), which feeds
   `Frame::kiVersion` (`Frame.cpp:115`). Rationale: the bits stored in a saved
   or replayed `pFlags` change meaning.
6. No `engine::kiProtocolVersion` increment. Rationale:
   `SpawnInfo::Fields()` keeps its members, order, and widths, and
   `Projects/BrokenEngineSandbox/Source/Network/AGENTS.md`
   `## Status-Change Wire Format` keys the increment on a payload layout
   change.
7. No `FrameInput::kiVersion` increment. Rationale: its rule
   (`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` `## Invariants`)
   keys on the bytes a server build writes, which keep their format; only the
   value of the flags byte changes, and the `Frame::kiVersion` change from
   step 5 already rejects older saves and replays.
8. In
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`
   lines 251-253, delete the clause giving a `SpawnMissile`'s default `flags`;
   an omitted `flags` then takes the payload struct default like any other
   field.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsCombat.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`

## In scope

- `MissileFlags`: the `kTargetFleet` and `kTargetRandom` enumerators.
- `MissilesPostRender::Transfer`: the `missileFlags` local, its `Set` call,
  and the `.flags` initializer of the `SpawnInfo`.
- The Battleship missile launch in `BattleshipsCombat.cpp`: the
  `missileFlags` local and the `.flags` initializer of the
  `MissilesPostRender::Spawn` argument.
- The `StatusChangeType::kSpawnMissile` case in
  `ServerSimulationHarnessRigs.cpp`: the default-flags block and its comment.
- `MissilesPostRender::kiVersion`.
- The `SpawnMissile` default-`flags` clause in `commands-server.md`.

## Out of scope

- The `kFalling` value, every `kFalling` test, and the `kFalling` derivation
  in `MissilesPostRender::Spawn`.
- `SpawnInfo::Fields()`, `engine::kiProtocolVersion`, and
  `FrameInput::kiVersion`.
- `SpawnInfo::vecStoredDirection` in `Transfer`: which direction crosses is
  a separate question this Plan does not settle.
- Rejecting a harness `flags` value that names a removed bit: the agent
  command channel is an exempt developer tool.
- `Frame/Collections/Missiles/AGENTS.md`: its falling-state sentence becomes
  true and needs no edit.

## Risk tier

Tier 3 — invariant/integration (the author's classification). Trigger: the
change alters what the CRC'd, saved, and transferred `pFlags` column carries
and takes a `kiVersion` increment, so it touches determinism/CRC,
serialization, and save/replay compatibility. No threading, allocation,
shader, or `.pack` exposure.

## Acceptance criteria

1. No `kTargetFleet` or `kTargetRandom` text remains under `Projects/`.
2. `/compile` builds the BrokenEngineSandbox client and server.
3. Through `/agent-harness`, a replay recorded and played back on the changed
   build while Battleships launch missiles that cross cell edges shows no
   per-tick CRC mismatch. The supplied evidence records no harness setup, so
   the executing session builds the scene.

## Coordination

`Documents/Plans/Game/CarryUntargetedMissileStoredDirection.md` edits the same
`SpawnInfo` initializer in `MissilesPostRender::Transfer` and also increments
`MissilesPostRender::kiVersion`. Neither Plan depends on the other. Whichever
lands second applies its edit on top of the first and takes its own increment
from the value then in the tree.
