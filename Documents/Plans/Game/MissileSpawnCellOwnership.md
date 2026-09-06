<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T17:50:04.722Z","dependsOn":[]} -->
# Keep newly fired Missiles inside their owning cell

## Context

The scope-boundary audit retained `CSB/shard-0050/001` as
SERIOUS_CONFIRMED/HIGH.  `PlayersPostRender::SpawnMissiles` computes a missile
position from a legal player center plus a lateral barrel offset and a
pre-move (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp:380-438`),
then acquires a target and appends the row
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp:440-466`).
The player center can
be arbitrarily close to a strict cell edge, so the nonzero offset can place
that final position outside the source cell.  `MissilesPostRender::Spawn`
validates only vector lanes before appending (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp:401-455`).

The fixed-tick pipeline runs collision and transfer before Destroy/Spawn
(`Engine/Source/Frame/FrameBase.cpp:347-361`).  Missile collision preparation
uses the previous/current segment and `TracePointToFrameExit`, and
`PostCollision` marks a boundary transfer when no earlier event wins
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesUpdate.cpp:298-425`;
`Engine/Source/Frame/FrameUtils.h:67-87,132-139`).  A late out-of-cell row
therefore misses the current tick's collision/transfer phases; on the next
tick its time-zero boundary result can suppress an inward collision and cause
a later repair transfer.  The Frame contract requires positions to stay in
their owning cell and refuses out-of-cell spawns.  This gap is pre-existing,
unresolved, and outside the audit implementation boundary: at the audit
checkpoint, the session's tracked implementation change list contained only
the session-inventory scripts.

Impact: an edge-fired Missile can be stored in the wrong cell, miss its first
collision opportunity, and transfer only on a later tick, while the shared
deterministic simulation and CRC have already accepted the invalid row.

## Design

Author's recommendation: apply the current cell's `FrameStaticData` bounds at
the player-fired Missile spawn boundary and refuse every out-of-cell final
position before a row is appended.  Preserve the existing in-cell firing and
target-acquisition behavior, deterministic random-draw order, shared CRC, and
transfer-field restoration; do not clamp an illegal position or defer it into
another cell.  The transfer arrival path remains an explicit restoration of
the fields already carried by `SpawnTransfer`.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp:380-472` — player missile position, target acquisition, random draws, and spawn call.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp:401-455` and `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.h:168-190` — Missile spawn boundary and `SpawnInfo` contract.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesUpdate.cpp:298-425` — collision cutoff, earliest-event resolution, and transfer marking.
- `Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp:36-76` — carried Missile field restoration (read-only transfer evidence).
- `Engine/Source/Frame/FrameUtils.h:67-87,132-139` — frame-exit tracing and strict bounds predicate.
- `Engine/Source/Frame/FrameBase.cpp:347-361` — fixed-tick collision, transfer, Destroy/Spawn, and CRC order.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:415-426` — game Destroy/Spawn forwarding (read-only phase evidence).

## In scope

- Cell-boundary validation for player-fired Missile positions in
  `PlayersPostRender::SpawnMissiles`, including any required propagation to
  the existing `FrameStaticData`/spawn contract before row append.
- Preservation of target acquisition, random-draw order, valid in-cell
  initialization, and client/server shared-state behavior on the player spawn
  path.
- The late-spawn interaction with Missile `PreCollision`/`PostCollision` and
  the fixed phase order, so an invalid row cannot rely on a later transfer to
  repair ownership.
- Preservation of `SpawnTransfer`'s carried Missile fields and transfer wire
  semantics while validating the new player-spawn boundary.

## Out of scope

- Collision geometry, earliest-event tie policy, boundary tracing, terrain
  handling, target-acquisition range/selection, firing timers, missile tuning,
  or unrelated projectile producers such as Blasters and Spaceships.
- Clamping an illegal position, introducing a cross-cell spawn queue, or
  changing collection layout, serialization, save/replay, protocol, or
  transfer payload formats.
- Generic vector validation, client-only audio/exhaust effects, or changes to
  transferred Missile field values and arrival random-draw rules.
- The separately owned `BlasterSpawnCellOwnership.md` change; it is a related
  projectile boundary but its scope explicitly excludes unrelated collection
  ownership.

## Risk tier and invariants

Expected Change Workflow Tier 3.  Trigger: deterministic fixed-tick collision,
cell transfer, late projectile spawn, and shared CRC must agree across client
and server.

Preserve these invariants:

- Every newly created player-fired Missile is strictly inside the frame that
  owns it before its first collision phase, or is refused without a repair
  transfer.
- Valid in-cell firing, target acquisition, random-draw order, and shared CRC
  state remain unchanged across client and server.
- An inward segment cannot have its valid first collision suppressed by a
  time-zero frame-boundary cutoff.
- Transfer fields and the explicit `SpawnTransfer` restoration path remain
  unchanged.

## Acceptance criteria

- A fixed-tick scenario placing a legal player near each cell edge, exercising
  both alternating Missile spawn sides, publishes no out-of-cell Missile.
- An edge case with an in-cell target does not suppress the Missile's valid
  inward collision and does not create a later repair transfer.
- Ordinary in-cell player firing preserves target acquisition, positions,
  timing, random draws, shared CRC, and client/server behavior.
- Missile transfers restore every carried field exactly as before, with no
  protocol or serialized-layout change.
- Client and server `Debug|x64` builds pass through `/compile`, and a focused
  fixed-tick edge-of-cell scenario proves the first-event and ownership
  outcomes.

## Notes

The candidate's durable source is
`80896f33661aaab99cf180a96db54600099be652`; consolidated triage is
`Temp/CppScopeBoundaryAudit/80896f33661aaab99cf180a96db54600099be652/triage-0001.md`
(SHA-256 `ea32fd84e984dea3d14ed1993c62adf54b86b0c81fed41e3f0ff665c035a3150`).
The current source still has the computed out-of-cell path and late append
described above.  `BlasterSpawnCellOwnership.md` is not an exact duplicate:
its `## Out of scope` section excludes unrelated collection ownership, while
this Plan owns the distinct Missile spawn and collision path.
