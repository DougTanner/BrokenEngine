<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-11T22:25:06.344Z","dependsOn":[]} -->
# Keep spawned Explosions inside their owning cell

## Context

`Engine/Source/Frame/AGENTS.md` states the repository standard that every
producer that creates a row in a cell collection must test the final position
against the cell's bounds before appending and must refuse an outside position,
never clamping it and never relying on a later transfer to repair ownership;
transfer arrivals are the only exception.  Three game producers append
`engine::ExplosionsPostRender` rows at a jittered or radially offset position
with no bounds test:

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp:234-252`
  — `SpawnSpaceshipExplosion` offsets the ship position by
  `common::RandomPositionJitter<kfSpaceshipExplosionPositionJitter>` and appends.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp:480-521`
  — `SpawnDeathExplosions` adds `common::RandomPositionJitter<1.0f>` plus a
  time-driven radial offset scaled by `kfExplosionsRadius`
  (`PlayersCombat.cpp:48`, twenty player radii) and appends.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp:273-300`
  — `SpawnMissileExplosion` appends three rows per detonation, the second and
  third offset by `common::RandomPositionJitter<0.2f>`.

Two reachability arguments apply.  Any nonzero offset from a legal position near
a cell edge lands outside the cell.
`Documents/Plans/Game/BlasterSpawnCellOwnership.md` makes that argument for the
player Blaster producer, and the landed `PlayersPostRender::SpawnMissiles`
refusal already acts on it.  Beyond that edge case, an exploding spaceship or player is permanently exempt
from the transfer mark — `SpaceshipsCombat.cpp:158,194` and
`PlayersCombat.cpp:252-255,287` both gate the out-of-bounds transfer mark on the
row not being `kExploding` — while death knockback keeps the row moving
(`SpaceshipsCombat.cpp:86-89`, `kfDeathKnockbackSpeed`).  A dying entity can
therefore drift arbitrarily far outside its cell and keep emitting staggered
explosion rows from there for the whole death animation
(`Spaceships.cpp:450-461`, `PlayersCombat.cpp:483-490`).

Explosions never transfer: the collection has no Transfer phase, so nothing
repairs an out-of-cell row later.  Their shared members are CRC-bearing on both
builds — `Explosions.h:231-238` puts `pVecPositions` in `SharedMembers()`,
`Engine/Source/Frame/FrameBase.h:102-128,212-238` puts `explosions` in
`ServerCollections()`, and `Engine/Source/Frame/FrameBase.cpp:11-25,74-88` folds
`ServerCollections()` into both `Crcs()` functions — and the same set travels
through the shared cross-build `ServerRead` path.  `randomEngine` is CRC'd
there too, so any change to how many draws a producer consumes is itself
CRC-visible.

Impact: shared, CRC-checked, cross-build-serialized rows are published whose
position lies in a neighbouring cell, against the recorded standard, with no
mechanism that can move them to their owner.

## Design

Author's recommendation: hoist `engine::ComputeFrameBounds(rStaticData.vecArea)`
once per producer and test each row's final position with
`engine::IsOutOfBounds` immediately before the `engine::ExplosionsPostRender::Spawn`
call, skipping the append and emitting one `LOG(kDefault, kDebug, ...)` line
naming tick, coord, index, and position on refusal.  Refuse; never clamp, never
queue the row for another cell.  This mirrors the refusal this session added to
`PlayersPostRender::SpawnMissiles` (`PlayersCombat.cpp:436-444`), which is the
pattern to follow.

Author's recommendation on draw order, the part that carries the CRC risk: a
refusal necessarily skips draws, because `ExplosionsPostRender::Spawn` itself
consumes shared draws unconditionally
(`Engine/Source/Frame/Collections/Explosions/ExplosionsSpawn.cpp:62-63,89,93,127-137,191-196`)
and `Missiles.cpp:294` draws inside that call's initializer list.  The invariant
to hold is therefore the one this session's `SpawnMissiles` refusal holds: both
builds reach the identical refuse-or-append decision from shared state alone, so
they skip the identical draws and their random streams stay in step.  Draw the
position jitter before the test and keep it consumed on refusal, so only the
`Spawn` call's own draws are at stake.  In `SpawnMissileExplosion` the three
rows are tested independently and the loop continues to the next size multiplier
on refusal, so each row's decision is made from shared state on its own.  In
`SpawnDeathExplosions` only the `common::Random<XM_2PI>` rotation precedes the
position jitter; `RandomDirectionJitter<0.5f>` follows it and the final radial
position depends on that draw, so the test belongs after all three.

Author's recommendation on plumbing: each producer needs the cell's
`engine::FrameStaticData`, which none of the three currently takes.  Every
caller already has one in scope — `Spaceships.cpp:459` and
`SpaceshipsCombat.cpp:75` (through `BeginExplosion`, itself called from
`PostCollision` and `AreaDamage`), `Players.cpp:423`, and `Missiles.cpp:534` —
so threading the parameter through those signatures is the smallest route, in
preference to reaching for `game::gpGame` or recomputing bounds from anything
other than `rStaticData.vecArea`.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp:234-252,450-461` — spaceship explosion producer and its staggered-death caller.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsCombat.cpp:52-76,144-199` — forward declaration, `BeginExplosion`, and the `kExploding` transfer exemption (read-only reachability evidence).
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp:480-521` and `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:238` — player death-explosion producer and its declaration.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:423` — its only caller, which already holds `rStaticData`.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp:273-300,534` — missile detonation producer and its caller.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp:436-444` — the refusal pattern to mirror (read-only).
- `Engine/Source/Frame/FrameUtils.h` — `ComputeFrameBounds` and `IsOutOfBounds` (read-only).
- `Engine/Source/Frame/Collections/Explosions/Explosions.h:209-265` and `Engine/Source/Frame/FrameBase.h:102-128,212-238` — shared/CRC member set (read-only tier evidence).

## In scope

- A cell-bounds refusal in each of `SpawnSpaceshipExplosion`,
  `PlayersPostRender::SpawnDeathExplosions`, and `SpawnMissileExplosion`,
  applied to every `engine::ExplosionsPostRender::Spawn` row each one appends,
  before the append.
- The `engine::FrameStaticData` (or equivalent bounds) parameter threading each
  producer and its intermediate callers need for that test, including
  `BeginExplosion` and the `Players.h` declaration.
- The refusal log line and the placement of the test that keeps the refuse-or-
  append decision derivable from shared state alone, so client and server skip
  the identical draws.

## Out of scope

- The player Blaster producer owned by `Documents/Plans/Game/BlasterSpawnCellOwnership.md`,
  and the player Missile producer `PlayersPostRender::SpawnMissiles`, which
  already refuses.
- The `kExploding` transfer exemption itself: making dying spaceships or players
  transfer, or bounding their motion, is a separate behavior change.
- Explosion visuals, tuning, sizes, trail and particle counts, audio, point
  lights, area damage, and any client-only effect collection.
- Clamping a refused position, adding a cross-cell explosion queue, changing the
  `Explosions` collection layout, `SharedMembers()`, serialization, save/replay,
  or protocol formats.
- Enemy blaster firing in `SpaceshipsPostRender::Spawn`, which spawns at the
  ship's own already-bounds-tested position and needs no change.

## Risk tier and invariants

Expected Change Workflow Tier 3.  Trigger: determinism and CRC — the
`Explosions` shared members are folded into both `FrameInterpolateBase::Crcs()`
and `FramePostRenderBase::Crcs()` through `ServerCollections()` and travel in the
shared cross-build `ServerRead` format, and the refusal sits next to
`randomEngine` draws that are themselves CRC'd, so client and server must agree
byte-for-byte.

Preserve these invariants:

- Every appended explosion row's position is inside the cell that owns it, or the
  row is refused; no row is clamped or deferred to another cell.
- Client and server reach the identical refuse-or-append decision for every row
  from shared state alone, so they skip the identical
  `rFrame.postRender.randomEngine` draws and their streams stay in step.
- Ordinary in-cell explosions keep their current positions, directions, sizes,
  timings, trail and particle counts, and CRC.
- The `Explosions` shared member set, serialized layout, and save/replay
  compatibility are unchanged.

## Acceptance criteria

- Every `engine::ExplosionsPostRender::Spawn` call site in all three producers is
  guarded by an `engine::IsOutOfBounds` test on the exact position it passes,
  settled by static diff: the harness cannot enumerate explosion rows
  (`query_collection` accepts only `spaceships|missiles|blasters`,
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:46`).
- The refuse-or-append decision reads only shared state, so client and server
  skip the identical draws; settled by static diff plus matching per-tick CRCs
  over a client/server run on the same input.
- Ordinary in-cell detonations and death sequences are visually and numerically
  unchanged, including the three-row missile detonation.
- Client and server `Debug|x64` builds clean through `/compile`, and a harness
  run with `Default` raised to `Debug` matches the refusal log line through
  `get_logs` without a CRC mismatch; no harness command kills an entity, so the
  refusal is provoked by ordinary combat near a cell edge or read from the same
  static diff.

## Notes

The standard this Plan enforces is already recorded in
`Engine/Source/Frame/AGENTS.md` (`## Architecture`, position bullet) and in
`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`, so the change adds no new
documented rule.  The landed refusal in `PlayersPostRender::SpawnMissiles` is the
pattern to mirror; `BlasterSpawnCellOwnership.md` covers the same standard but is
unimplemented, so it is not the pattern and not a prerequisite — each producer
hoists its own bounds locally and neither change introduces a shared helper.
