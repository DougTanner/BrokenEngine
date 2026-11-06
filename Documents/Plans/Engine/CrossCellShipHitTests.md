<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-01T01:31:57.670Z","dependsOn":[]} -->
# Test ship hulls that cross a cell edge against projectiles and missile blasts in the neighbour cell

## Context

Ship hits use per-row, model-derived hitboxes: each ship row carries its own
enclosing sphere and, as far as its model's `.model.json` `hitbox` setting
reaches, a box prism and an outline prism
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipHitboxes.h`,
`gShipHitboxShapes`, `BuildShipHitbox`). Blasters and missiles hit as swept
points. The change that introduced them accepted, by user decision, that a
hull part crossing a cell edge is not hit from the neighbour cell, and deferred
that gap to this Plan.

Observed gap (from code; no live run has reproduced it yet):

- Cells have a 450 m apothem (`kfCellApothem`,
  `Engine/Source/Frame/GridCoord.h:45`). A ship belongs to the cell holding its
  center and transfers only when its center leaves the hexagon
  (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsCombat.cpp:168-172`).
- Cells are flat-top hexagons (`common::HexDirections`,
  `Common/Math/MathUtils.h:239`). With apothem a, neighbour centers sit at
  (+-sqrt(3)a, +-a) and (0, +-2a), so +X points at a vertex and no edge faces
  east. The north edge is local y = +450, shared with the neighbour at
  (q, r + 1).
- Horizontal mesh reach from a ship's center, measured from each `.MODEL`'s
  vertices at its `.model.json` alignment: a random-controller Battleship
  (SpaceBattleshipAquamarine) 16.03 m at the nose; a fleet Battleship
  (BattleshipJesterz86) 17.25 m at the stern quarter, its length spanning
  +-16.00 m; Fighters 3.05 m (RavenStarConflictV2, fleet) and 2.28 m
  (Spaceship, random); Omnis 3.01 m (SpaceGliderGltf, fleet) and 3.10 m
  (SpaceFighter, random). Weapons test the outline only on the `outline`
  models, the two Battleships, where it lies inside that reach by the model's
  `outlineInset` (0.1 m today); SpaceFighter's `box` row is tested to its box
  corners, and a `sphere` row to its row radius (2.37 to 3.14 m) by a projectile
  and at its center by a blast. Whenever such a ship's center is closer to an
  edge than its mesh reach, part of its hull can lie in the neighbour cell.
- Each cell's tick runs `Collision::Collide` and the AreaDamage phase over that
  cell's own rows only (`Engine/Source/Frame/FrameBase.cpp:299-305`); the
  collision and area-damage state is `thread_local`
  (`Engine/Source/Frame/Collision.h:121`, `Engine/Source/Frame/AreaDamage.h:29`)
  and the zone grids hold only the cell's own rows
  (`Engine/Source/Frame/Collision.cpp:9-13`, `Engine/Source/Frame/AGENTS.md`
  entity-containment invariant).
- A Blaster or Missile stops testing the cell's ships at its cell exit and
  transfers (`CollisionOutcomeType::kCellExit`,
  `Engine/Source/Frame/Collision.cpp:872`,
  `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/BlastersUpdate.cpp:242`,
  `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesUpdate.cpp:323`).
  In the neighbour cell it never tests the ship whose hull reaches across.
- Missile area damage (`kfMissileDamageRadius` 7 m,
  `Projects/BrokenEngineSandbox/Source/Frame/HealthDamage.h:19`) is added to
  the exploding cell's own queue and queried only by that cell's ships
  (`AreaDamage::Get`, `Engine/Source/Frame/AreaDamage.h:27`), so a blast in the
  neighbour cell never damages the hull across the edge. Before per-row
  hitboxes this affected only ship centers within 7 m of an edge; box and
  outline prisms widen it to the full hull reach.

Constraints any mechanism meets:

- Cells tick in parallel and independently on the server
  (`Engine/Source/GameBase.cpp:556`), and the client reconciles one cell's tick
  alone (`Engine/Source/Network/Client/ReconcileReplayTick.cpp:118`), possibly
  without the neighbour's state.
- Positions are local to their own cell; a position crosses cells only through
  the existing conversion (`PrepareTransferRequest`, `TryAddGridCoordinate`,
  `Engine/Source/Frame/AGENTS.md`).
- Collision and area-damage results feed the PostRender CRC; the global
  candidate order and exclusive maximum-time cutoffs stay total and
  reproducible.

## Design

The cause above is confirmed from code; the mechanism is not decided. The
executing session:

1. Reproduces the gap live with `/agent-harness` using the scenario in
   `## Acceptance criteria` (the supplied evidence records no harness setup),
   and measures the reach that bounds the problem: the largest
   enclosing ship row radius across `gShipHitboxShapes`, plus the largest
   per-tick projectile step.
2. Picks the mechanism inside `## In scope` and presents the candidates to the
   user before implementing (`/plan-alternatives` at its Prepare step). The
   author recommends weighing at least these, without deciding between them:
   - Neighbour proxy rows: each cell publishes its ship rows whose hitboxes
     reach within that bound of an edge to the neighbour, rebased into the
     neighbour's frame at the tick boundary; the neighbour tests them as
     read-only rows, and hits travel back as a deterministic message applied
     on a later tick. Cost: one tick of hit latency and a new cross-cell
     record on the server and the client reconcile path.
   - Projectile and blast hand-back: a projectile or blast source near an edge
     is also tested by the cell owning the ship, by publishing it to the
     neighbour instead of the ship. Same latency and record questions,
     smaller payload when ships outnumber projectiles near edges.
   Any option that breaks per-cell independent ticking or client per-cell
   reconcile goes back to the user as a re-plan, never as a widening of scope.
3. Makes the smallest change implementing the chosen mechanism within
   `## In scope`, surfacing any needed change outside it for re-planning.

Change Workflow tier: Tier 3. Trigger: the change alters collision and
area-damage results that feed the PostRender CRC, adds cross-cell data whose
order must stay deterministic across server parallel ticks and client
reconcile, and spans Engine Frame and the sandbox ship and projectile
collections.

## Critical files

- `Engine/Source/Frame/Collision.h`, `Engine/Source/Frame/Collision.cpp`
- `Engine/Source/Frame/AreaDamage.h`, `Engine/Source/Frame/AreaDamage.cpp`
- `Engine/Source/Frame/FrameBase.h`, `Engine/Source/Frame/FrameBase.cpp`
- `Engine/Source/GameBase.cpp`
- `Engine/Source/Network/Client/ReconcileReplayTick.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipHitboxes.h`,
  `ShipHitboxes.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsCombat.cpp`,
  `Fighters/FightersCombat.cpp`, `Omnis/Omnis.cpp`, `Omnis/OmnisCombat.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/BlastersUpdate.cpp`,
  `Missiles/MissilesUpdate.cpp`, `Missiles/Missiles.cpp`

## In scope

- Root-cause confirmation and the live reproduction in `## Design` step 1.
- The fix region: ship hit tests against Blasters and Missiles
  (`Collision::AddLayer`, `Collision::Collide`, the ship PreCollision layer
  setup and PostCollision hit handling in the three ship collections) and
  missile area damage (`AreaDamage::Add`, `AreaDamage::Get`, the three ship
  AreaDamage passes), extended so a ship hull crossing a cell edge is tested by
  the neighbour cell's projectiles and blasts.
- The cross-cell plumbing the chosen mechanism needs at the tick boundary in
  `RunFrameTick`, the server per-cell dispatch in `GameBase.cpp`, and client
  reconcile in `ReconcileReplayTick.cpp`, plus the sandbox `Frame::kiVersion`
  bump and wire, replay, and save updates only if the mechanism adds a
  serialized or transmitted record.
- `Engine/Source/Frame/AGENTS.md` and `Documents/Architecture/Network.md`
  entries the chosen mechanism makes incorrect.

## Out of scope

- Hitbox shapes, settings, and the model outline bake.
- Terrain collision, pushers, and navigation clearance near cell edges.
- Ship-vs-ship collision and same-layer collision.
- Cell size, cell shape, and the rule that a row belongs to the cell holding
  its center.
- Gameplay rebalancing of damage values.

## Acceptance criteria

- Reproduction then fix: with the server paused and the north neighbour cell
  active (`create_cell`), `inject_payload` spawns a `"RandomSpawn"` Battleship
  of `"random"` alignment facing +Y with its center at about local (0, 440),
  10 m inside its cell's north edge, and a `"fleet"` Blaster in the north
  neighbour cell flying along X at local y about -447, 3 m past that edge,
  through the hull part beyond the edge at the ship's height; after ticks,
  `read_frame` on the unedited tree shows no health change, and after the fix
  shows the Battleship's health reduced by `kfBlasterDamage` and the Blaster
  gone. The author recommends this placement because about 5.9 m of the nose
  then crosses the edge, about 2 m wide where the path meets it, and a path
  parallel to the edge never transfers into the Battleship's cell, where it
  would hit the hull and hide the gap. Both spawns default to plane 0's height.
  The placement comes from the measured mesh; no harness run has exercised it.
- Blast: the same setup with an enemy Missile exploding in the neighbour cell
  within `kfMissileDamageRadius` of the crossing hull part reduces the
  Battleship's health after the fix, and does not on the unedited tree. A
  spawned Missile has no homing target and explodes only on entity or terrain
  contact (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesUpdate.cpp:304`,
  `:320`), so the author recommends flying it into a `"random"` Fighter spawned
  in the neighbour cell about 5 m from the nose. No harness run has exercised
  this setup either.
- Determinism: `replay_record` then `replay_play` over both scenarios reports
  no CRC mismatch, and a connected client's `get_logs` with a `[Dd]esync`
  pattern returns no lines.
- Regression: a Blaster hit on a ship far from any edge still damages it once.

## Notes

- Originating gap: the accepted cross-cell residual of the per-row ship
  hitbox change; the user chose to accept it there and track it here.
- Exposure: determinism and CRC, cross-cell threading on the server parallel
  tick, client reconcile, and possibly wire, replay, and save formats.
