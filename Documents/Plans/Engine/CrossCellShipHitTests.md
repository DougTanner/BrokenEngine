<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-01T01:31:57.670Z","dependsOn":[]} -->
# Test ship hulls that cross a cell edge against projectiles and missile blasts in the neighbour cell

## Context

Ship hits use per-row, model-derived hitboxes: each ship row carries its own
enclosing sphere and, as far as its model's `.model.json` `hitbox` setting
reaches, a box prism and a hull (outline) prism
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipHitboxes.h`,
`gShipHitboxShapes`, `BuildShipHitbox`). Blasters and missiles hit as swept
points. The change that introduced them accepted, by user decision, that a
hull part crossing a cell edge is not hit from the neighbour cell, and deferred
that gap to this Plan.

Observed gap (from code; no live run has reproduced it yet):

- Cells have a 450 m apothem (`kfCellApothem`,
  `Engine/Source/Frame/GridCoord.h:45`). A ship belongs to the cell holding its
  center and transfers only when its center leaves the hexagon
  (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsCombat.cpp:169-173`).
- A random-controller Battleship's aligned outline reaches up to about 89 m
  from its center (largest aligned XY outline radius 19.78 model units x
  `kfBattleshipRadius` 4.5); a fleet Battleship about 29 m; Fighters and Omnis
  under 2 m. Whenever such a ship's center is closer to an edge than that
  reach, part of its hull lies in the neighbour cell.
- Each cell's tick runs `Collision::Collide` and the AreaDamage phase over that
  cell's own rows only (`Engine/Source/Frame/FrameBase.cpp:299-305`); the
  collision and area-damage state is `thread_local`
  (`Engine/Source/Frame/Collision.h:112`, `Engine/Source/Frame/AreaDamage.h:29`)
  and the zone grids hold only the cell's own rows
  (`Engine/Source/Frame/Collision.cpp:9-13`, `Engine/Source/Frame/AGENTS.md`
  entity-containment invariant).
- A Blaster or Missile stops testing the cell's ships at its cell exit and
  transfers (`CollisionOutcomeType::kCellExit`,
  `Engine/Source/Frame/Collision.cpp:837`,
  `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/BlastersUpdate.cpp:242`,
  `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesUpdate.cpp:305`).
  In the neighbour cell it never tests the ship whose hull reaches across.
- Missile area damage (`kfMissileDamageRadius` 7 m,
  `Projects/BrokenEngineSandbox/Source/Frame/HealthDamage.h:19`) is added to
  the exploding cell's own queue and queried only by that cell's ships
  (`AreaDamage::Get`, `Engine/Source/Frame/AreaDamage.h:27`), so a blast in the
  neighbour cell never damages the hull across the edge. Before per-row
  hitboxes this affected only ship centers within 7 m of an edge; hull prisms
  widen it to the full hull reach.

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

- Reproduction then fix: with the server paused, `inject_payload` spawns a
  random-controller Battleship facing +X with its center about 40 m inside
  its cell's east edge, and an enemy Blaster in the east neighbour cell whose
  path crosses the hull part beyond that edge at the ship's height; after
  ticks, `read_frame` on the unedited tree shows no health change, and after
  the fix shows the Battleship's health reduced by `kfBlasterDamage` and the
  Blaster gone.
- Blast: the same setup with an enemy Missile exploding in the neighbour cell
  within `kfMissileDamageRadius` of the crossing hull part reduces the
  Battleship's health after the fix, and does not on the unedited tree.
- Determinism: `replay_record` then `replay_play` over both scenarios reports
  no CRC mismatch, and a connected client's `get_logs` with a `[Dd]esync`
  pattern returns no lines.
- Regression: a Blaster hit on a ship far from any edge still damages it once.

## Notes

- Originating gap: the accepted cross-cell residual of the per-row ship
  hitbox change; the user chose to accept it there and track it here.
- Exposure: determinism and CRC, cross-cell threading on the server parallel
  tick, client reconcile, and possibly wire, replay, and save formats.
