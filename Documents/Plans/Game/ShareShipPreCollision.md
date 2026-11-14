<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:11:48.571Z","dependsOn":[]} -->
# Share the ship PreCollision body

## Context

Paths are under `Projects/BrokenEngineSandbox/Source/Frame/Collections/`.
Verified against commit `022915b6`.

The PostRender `PreCollision` of the three ship types is one body written
three times:

- `BattleshipsPostRender::PreCollision`,
  `Battleships/BattleshipsCombat.cpp:70-105`.
- `FightersPostRender::PreCollision`, `Fighters/FightersCombat.cpp:143-178`.
- `OmnisPostRender::PreCollision`, `Omnis/Omnis.cpp:500-535`.

Each returns on zero rows, adds a `kHitboxes` collision layer over the previous
and current positions, flags dying rows `kAlreadyCollided`, and writes every
row's radius from `gShipHitboxShapes` and its posed hitbox through
`BuildShipHitbox`. They differ only in the collection pair, the `ShipType`, the
`CollisionCategory`, and the `CollidesWith` mask. The rule is stated once, in
`Ships/AGENTS.md` `## Ship Invariants` ("Each ship PreCollision writes every
row's radius and posed hitbox from its current pose"), and it is not among the
Fighters and Battleships differences `Battleships/AGENTS.md` lists.

Originating gap: an architecture review of the Battleships collection; unmet
criterion "the shared ship pre-collision rule has one implementation". The
change is structural only, about 50 lines net, and was ranked lowest of that
review's candidates.

## Design

Recommended: one function template in the Ships folder, beside `BuildShipHitbox`
in `Ships/ShipHitboxes.h`, templated on the Interpolate and PostRender pair. It
takes the current pair, the previous Interpolate collection, the `ShipType`,
the category, and the mask, runs the layer add and the two row loops, and
returns the layer index.

- Each caller keeps its zero-row guard, which stops `AddLayer` registering an
  empty layer, and its own `thread_local` layer index (file-static in Fighters
  and Battleships, the `OmnisPostRender` member at `Omnis/Omnis.h:212`), and
  assigns the returned index to it.
- The three callers change together. Row operations, their order, and the
  layer registration order stay as they are.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipHitboxes.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsCombat.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/FightersCombat.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp`

## In scope

- One new function template in `ShipHitboxes.h`.
- The bodies of `BattleshipsPostRender::PreCollision`,
  `FightersPostRender::PreCollision`, and `OmnisPostRender::PreCollision` after
  their zero-row guard.

## Out of scope

- The zero-row guards and the three `siCollisionLayerIndex` variables.
- `BuildShipHitbox`, `gShipHitboxShapes`, and `engine::Collision`.
- Blasters and Missiles `PreCollision`.
- `PostCollision` and `AreaDamage`.
- `AGENTS.md` files: the rule text already describes one behavior.

## Risk tier

Tier 2 — scoped behavior. Trigger: a new shared declaration in a header used by
the three ship collections, in simulation code that feeds collision and so
CRC'd state; behavior is meant to be unchanged. No layout, wire, version, or
threading change; the layer index stays `thread_local` per caller.

## Acceptance criteria

- The layer add and the two row loops appear once, and each ship `PreCollision`
  holds only its guard and one call.
- The client and server build.
- A replay determinism check on a scene where ships of all three types take
  blaster and missile hits passes with no CRC mismatch. The supplied evidence
  records no harness setup; the executing session builds one through
  `/agent-harness`.

## Notes

`PostCollision` has no zero-row guard; the `PreCollision` guard stays.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
