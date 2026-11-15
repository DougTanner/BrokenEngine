<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:15:59.613Z","dependsOn":[]} -->
# Missile transfer: drop write-only target flags, carry pFlags and an untargeted stored direction

## Context

Paths under `Frame/Collections/` are relative to
`Projects/BrokenEngineSandbox/Source/`. Line numbers are from commit
`022915b6`; locate by symbol if they moved. Nothing was built or run for this
Plan: every statement is from a code read.

Both parts edit the same `SpawnInfo` initializer in
`MissilesPostRender::Transfer` and share one `MissilesPostRender::kiVersion`
increment and one replay CRC run.

### Write-only target flags and dropped falling state

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

### Untargeted stored direction replaced at the crossing

A missile's stored direction (`pVecStoredDirections`) is its launch aim
(`Frame/Collections/Battleships/BattleshipsCombat.cpp:330`). Missiles Update
rewrites it to the current direction when the target is lost
(`Frame/Collections/Missiles/MissilesUpdate.cpp:152-156`), and only the
no-target steering branch reads it (`MissilesUpdate.cpp:174-186`): an
untargeted missile steers back toward it.

`MissilesPostRender::Transfer` does not send the column. It sends the current
direction as `SpawnInfo::vecStoredDirection`
(`Frame/Collections/Missiles/Missiles.cpp:286`).

- For a missile that still has a target at the cell edge this matches the
  target-loss rule: the target never crosses (registry ids are per cell and
  `uiTarget` is not in `SpawnInfo::Fields()`,
  `Frame/Collections/Missiles/Missiles.h:197-198`), so the arrival is a
  missile that just lost its target, and its stored direction is its current
  direction. The row still holds its target when `Transfer` runs:
  PostCollision only sets `kTransfer` (`MissilesUpdate.cpp:299-302`).
- For a missile that was already untargeted, the heading it steers back
  toward is replaced by whatever direction jitter had turned it to on the
  crossing tick (`MissilesUpdate.cpp:133`, 141). The drift is small and
  deterministic, the same in both builds, so it is no CRC risk today.

The second case contradicts the documents:

- `Frame/Collections/AGENTS.md` `## Game-Specific Rules` says a transfer
  arrival restores carried gameplay state verbatim except the values it
  lists, and stored direction is not listed.
- `Frame/Collections/Missiles/AGENTS.md` `## Invariants` says an unguided
  missile flies on along its stored direction until its lifetime ends.

The code replaces the stored direction at every crossing, with no recorded
rationale. The assignment predates commit `096ea76c`, which moved it into
`Transfer`.

Originating gap: an architecture review of the Missiles collection, not an
unmet acceptance criterion of any change. Required conditions that are false
today: CRC'd state holds no value nothing reads, falling state crosses by
value, and an untargeted missile's stored direction crosses a cell edge by
value.

## Design

User decision, as the authoring session's dispatch brief relayed it, for the
stored direction: offered the choice between making the code match the
documents and keeping the code while documenting the replacement as an
exception, with the note that the first changes simulation results slightly so
older saves and replays are rejected through a version increment, the user
answered "Option 1" — make the code match the documents. Steps 4 and 5 carry
that decision. The remaining steps are the authors' recommendations, with
rationale.

1. Delete `kTargetFleet` and `kTargetRandom` from `MissileFlags`, leaving
   `kFalling` at its current value. Rationale: nothing reads them.
2. Delete the three writers: the `missileFlags` local and its `Set` call in
   `Transfer`; the `missileFlags` local and the `.flags` initializer in the
   Battleship launch; the default-flags block and its comment in the harness
   `kSpawnMissile` case, which then only calls `ParseSpawnEntry`.
3. In `Transfer`, send `.flags = rCurrentPostRender.pFlags[i]`. Rationale: it
   is what `Frame/Collections/Missiles/AGENTS.md` already states, and it
   removes the harness-only wrong arrival described above.
4. In `Transfer`, send `rCurrentPostRender.pVecStoredDirections[i]` as
   `vecStoredDirection` when `rCurrentPostRender.puiRegistryTargets[i]` is
   zero, and the current direction otherwise. The current direction stays for
   a targeted missile because sending the column unconditionally would turn a
   targeted arrival back toward its launch aim.
5. Increment `MissilesPostRender::kiVersion` (`Missiles.h:114`) once for the
   whole change; it feeds `Frame::kiVersion` (`Frame.cpp:115`). Rationale: the
   bits stored in a saved or replayed `pFlags` change meaning, and the comment
   on `Frame::kiVersion`
   (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:112-114`) asks for a
   version change on any change that shifts computed frame CRCs; a replay
   recorded before this change computes a different stored direction after an
   untargeted crossing.
6. Leave the `kFalling` derivation in `Spawn` (`Missiles.cpp:343-346`) as it
   is. Rationale: a harness `SpawnMissile` entry with `time` at or past the
   lifetime and no `flags` still relies on it, and for an arrival that already
   carries `kFalling` it changes nothing.
7. No `engine::kiProtocolVersion` increment. Rationale:
   `SpawnInfo::Fields()` keeps its members, order, and widths, and
   `Projects/BrokenEngineSandbox/Source/Network/AGENTS.md`
   `## Status-Change Wire Format` keys the increment on a payload layout
   change; only the values sent change.
8. No `FrameInput::kiVersion` increment. Rationale: its rule
   (`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` `## Invariants`)
   keys on the bytes a server build writes, which keep their format; only the
   values of the flags byte and the stored direction change, and the
   `Frame::kiVersion` change from step 5 already rejects older saves and
   replays.
9. In
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`
   lines 251-253, delete the clause giving a `SpawnMissile`'s default `flags`;
   an omitted `flags` then takes the payload struct default like any other
   field.
10. State both stored-direction cases in `Frame/Collections/Missiles/AGENTS.md`
    `## Invariants`, through `/update-claude-docs`: an untargeted missile's
    stored direction crosses verbatim; a targeted missile loses its target at
    the crossing and its stored direction becomes its current direction, as on
    any target loss. Rationale: the sentence listing what crosses cell
    boundaries does not name stored direction today. The falling-state
    sentence becomes true after step 3 and needs no edit.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsCombat.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`

## In scope

- `MissileFlags`: the `kTargetFleet` and `kTargetRandom` enumerators.
- `MissilesPostRender::Transfer`: the `missileFlags` local, its `Set` call,
  and the `.flags` and `.vecStoredDirection` initializers of the `SpawnInfo`.
- The Battleship missile launch in `BattleshipsCombat.cpp`: the
  `missileFlags` local and the `.flags` initializer of the
  `MissilesPostRender::Spawn` argument.
- The `StatusChangeType::kSpawnMissile` case in
  `ServerSimulationHarnessRigs.cpp`: the default-flags block and its comment.
- `MissilesPostRender::kiVersion`: one increment.
- The `SpawnMissile` default-`flags` clause in `commands-server.md`.
- `Frame/Collections/Missiles/AGENTS.md` `## Invariants`: the sentence on
  what crosses cell boundaries.

## Out of scope

- The `kFalling` value, every `kFalling` test, and `MissilesPostRender::Spawn`,
  including its `kFalling` derivation.
- Missiles Update, including the target-loss rewrite of the stored direction
  and both steering branches.
- `SpawnInfo::Fields()`, `engine::kiProtocolVersion`, and
  `FrameInput::kiVersion`: no payload member, order, or width changes.
- Rejecting a harness `flags` value that names a removed bit: the agent
  command channel is an exempt developer tool.
- `Frame/Collections/AGENTS.md`: its verbatim rule already covers the
  untargeted case this Plan fixes. A targeted arrival's stored direction still
  becomes its current direction, because the target is dropped at the crossing
  (the target-loss rule, `MissilesUpdate.cpp:155`); Design step 10 documents
  that in `Frame/Collections/Missiles/AGENTS.md`.

## Risk tier

Tier 3 — invariant/integration. Triggers: the change alters what the CRC'd,
saved, and transferred `pFlags` column carries and what CRC'd simulation state
computes after a cell crossing, and takes a `kiVersion` increment, so it
touches determinism/CRC, serialization, and save/replay compatibility. Both
builds run the same code, so it adds no client/server divergence. No wire
layout, threading, allocation, shader, or `.pack` exposure.

## Acceptance criteria

1. No `kTargetFleet` or `kTargetRandom` text remains under `Projects/`.
2. `/compile` builds the BrokenEngineSandbox client and server.
3. Through `/agent-harness`, a missile injected with no target and a
   `vecStoredDirection` different from its `vecDirection`, flying across a
   cell edge, has that same `vecStoredDirection` in the destination cell's
   row after it arrives.
4. Through `/agent-harness`, one replay recorded and played back on the
   changed build while Battleships launch missiles that cross cell edges
   shows no per-tick CRC mismatch.

The supplied evidence records no harness setup, so the executing session
builds both scenes.
