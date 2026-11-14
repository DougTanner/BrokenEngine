<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:15:59.613Z","dependsOn":[]} -->
# Carry an untargeted Missile's stored direction across cells

## Context

Paths under `Frame/Collections/` are relative to
`Projects/BrokenEngineSandbox/Source/`. Line numbers are from commit
`022915b6`; locate by symbol if they moved. Nothing was built or run for this
Plan: every statement is from a code read.

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
unmet acceptance criterion of any change. Required condition that is false
today: an untargeted missile's stored direction crosses a cell edge by value.

## Design

User decision, as the authoring session's dispatch brief relayed it: offered
the choice between making the code match the documents and keeping the code
while documenting the replacement as an exception, with the note that the
first changes simulation results slightly so older saves and replays are
rejected through a version increment, the user answered "Option 1" — make the
code match the documents. Steps 1 and 2 carry that decision. Step 3 and the
remaining details are the author's recommendations, with rationale.

1. In `Transfer`, send `rCurrentPostRender.pVecStoredDirections[i]` as
   `vecStoredDirection` when `rCurrentPostRender.puiRegistryTargets[i]` is
   zero, and the current direction otherwise. The current direction stays for
   a targeted missile because sending the column unconditionally would turn a
   targeted arrival back toward its launch aim.
2. Increment `MissilesPostRender::kiVersion`
   (`Frame/Collections/Missiles/Missiles.h:114`). The comment on
   `Frame::kiVersion`
   (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:112-114`) asks for a
   version change on any change that shifts computed frame CRCs, and a replay
   recorded before this change computes a different stored direction after
   an untargeted crossing.
3. Recommended: state both cases in `Frame/Collections/Missiles/AGENTS.md`
   `## Invariants`, through `/update-claude-docs`: an untargeted missile's
   stored direction crosses verbatim; a targeted missile loses its target at
   the crossing and its stored direction becomes its current direction, as on
   any target loss. Rationale: the sentence listing what crosses cell
   boundaries does not name stored direction today.
4. Recommended: no `engine::kiProtocolVersion` or `FrameInput::kiVersion`
   increment. Rationale: `SpawnInfo::Fields()` keeps its members, order, and
   widths, so the payload bytes keep their format; only the value sent
   changes, and step 2 already rejects older saves and replays.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/AGENTS.md`

## In scope

- `MissilesPostRender::Transfer`: the `.vecStoredDirection` initializer of
  the `SpawnInfo`.
- `MissilesPostRender::kiVersion`.
- `Frame/Collections/Missiles/AGENTS.md` `## Invariants`: the sentence on
  what crosses cell boundaries.

## Out of scope

- Missiles Update, including the target-loss rewrite of the stored direction
  and both steering branches.
- `MissilesPostRender::Spawn`, `SpawnInfo::Fields()`,
  `engine::kiProtocolVersion`, and `FrameInput::kiVersion`: no payload member,
  order, or width changes.
- `SpawnInfo::flags` in `Transfer` (see `## Coordination`).
- `Frame/Collections/AGENTS.md`: its verbatim rule already covers the
  untargeted case this Plan fixes. A targeted arrival's stored direction still
  becomes its current direction, because the target is dropped at the crossing
  (the target-loss rule, `MissilesUpdate.cpp:155`); Design step 3 documents
  that in `Frame/Collections/Missiles/AGENTS.md`.

## Risk tier

Tier 3 — invariant/integration (the author's classification). Trigger: the
change alters what CRC'd simulation state computes after a cell crossing and
takes a `kiVersion` increment, so it touches determinism/CRC and save/replay
compatibility. Both builds run the same code, so it adds no client/server
divergence. No wire layout, threading, allocation, shader, or `.pack`
exposure.

## Acceptance criteria

1. `/compile` builds the BrokenEngineSandbox client and server.
2. Through `/agent-harness`, a missile injected with no target and a
   `vecStoredDirection` different from its `vecDirection`, flying across a
   cell edge, has that same `vecStoredDirection` in the destination cell's
   row after it arrives. The supplied evidence records no harness setup, so
   the executing session builds the scene.
3. Through `/agent-harness`, a replay recorded and played back on the changed
   build while missiles cross cell edges shows no per-tick CRC mismatch.

## Coordination

`Documents/Plans/Game/RemoveWriteOnlyMissileTargetFlags.md` edits the same
`SpawnInfo` initializer in `MissilesPostRender::Transfer` and also increments
`MissilesPostRender::kiVersion`. Neither Plan depends on the other. Whichever
lands second applies its edit on top of the first and takes its own increment
from the value then in the tree.
