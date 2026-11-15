<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:16:47.585Z","dependsOn":[]} -->
# Ship combat helpers

## Context

Paths without a root are under
`Projects/BrokenEngineSandbox/Source/Frame/Collections/`. Each part's line numbers are as of the commit it
names; locate by symbol if they moved. The three parts are pre-existing debt
from architecture analyses, not unmet acceptance criteria of any change, and
share `Fighters/FightersCombat.cpp`, `Battleships/BattleshipsCombat.cpp`, and
`Omnis/Omnis.cpp`.

### Ship PreCollision body (commit `022915b6`)

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

### Registry id by locator (commit `fe1fb69b`)

Four sites spell the same `VisitShip` lambda to read the registry id of a
located ship row. Each is inside a `ForEachEnemyShip` visitor and returns
`rInterpolate.pRegistryIds[iRow]`:

- `Ships/ShipQueries.cpp:51-57`, the `RegistryId` local lambda in
  `FindAttackTarget`, reading `rPreviousFrame`.
- `Ships/ShipQueries.cpp:183-186`, the `bKeptTarget` test, reading
  `rPreviousFrame`.
- `Fighters/FightersCombat.cpp:105-108`, the `bKeptTarget` test, reading the
  current `rFrame`.
- `Battleships/BattleshipsCombat.cpp:281-284`, the `target` read, reading the
  current `rFrame`.

The by-locator accessors that exist cover position and hull only: `ShipPosition`
and `ShipHull` (`Ships/ShipQueries.h:115-117`). So each kept-attack-target
comparison re-derives which frame part and column hold the registry id, a rule
`Ships/AGENTS.md` `## Ship Invariants` (last bullet) owns.

Originating gap: a cohesion finding of the architecture phase of a deep
analysis of `Ships/` (the directory, not its subdirectories). The verifying
review confirmed the four copies and accepted the finding with one caveat,
recorded under `## Design`; it rated the benefit small but real, since four
current copies in three files meet the root `AGENTS.md` DRY directive's bar for
a helper.

### SyncFighterBeam width and declaration (commit `106613c6`)

`SyncFighterBeam` is defined in `Fighters/Fighters.cpp:136-152` under
`BT_CLIENT` and no header declares it. It is declared by hand at
`Fighters/FightersCombat.cpp:35` and at `Omnis/Omnis.cpp:72`. The second
crosses from one collection into another, and it is the only hand declaration
under `Collections/` that does so: `giBattleshipExplosionTypeIndex`, which
`Wrecks/Wrecks.cpp` reads, is declared under `BT_CLIENT` in
`Battleships/Battleships.h`.

It has three callers. `Fighters/Fighters.cpp:349` and
`Fighters/FightersCombat.cpp:136` pass `giFighterBeamAreaLightTypeIndex`;
`Omnis/Omnis.cpp:492` passes `siHealBeamAreaLightTypeIndex` for the Omni heal
beam.

The function picks its width by comparing the type index
(`Fighters/Fighters.cpp:144`): the Fighter index takes half of
`gFighterBeamWidth`, anything else takes `kfFighterBeamHalfWidth`
(`Fighters/Fighters.cpp:74`, no other reader). So the else arm and that
constant serve only the Omni heal beam: `Fighters.cpp` holds an Omni-only
width under a Fighter name, chosen by a type-index comparison.

Originating gap: the architecture phase (dependency and duplication lenses) of
a findings-only analysis of the `Omnis` directory, confirmed by an independent
verification read. Unmet criterion: the Omni heal beam's width is owned by
Omnis, and the cross-collection call has one declaration.

## Design

### 1. Shared ship PreCollision

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
- `PostCollision` has no zero-row guard; the `PreCollision` guard stays.

### 2. ShipRegistryId accessor

Recommended: add
`[[nodiscard]] engine::registry_id_t ShipRegistryId(const FrameInterpolate& rInterpolate, ShipLocator locator);`
beside `ShipPosition` in `Ships/ShipQueries.h`, defined beside it in
`Ships/ShipQueries.cpp`, and call it at the four sites. Rationale: it follows
the pattern `ShipPosition` already sets, and it keeps the column choice in the
query module.

Caveat the executing session preserves: each site keeps its own source frame.
The two `ShipQueries.cpp` sites pass `rPreviousFrame.interpolate`; the two
Combat sites pass the current `rFrame.interpolate`. Swapping a site's frame
shifts CRC-visible behavior by one tick (`Ships/AGENTS.md` `## Ship
Invariants`, the previous-frame read bullet). The reads are integer loads, so
with the frames kept the change is bit-identical.

Not checked by the review: whether an out-of-line accessor inlines inside the
per-enemy visitors. `ShipPosition` is already out of line at the same kind of
site, so the recommendation accepts the same cost.

### 3. SyncFighterBeam half-width parameter

Recommended:

- Add a half-width parameter to `SyncFighterBeam` and remove the type-index
  comparison. The two Fighter callers pass half of
  `gFighterBeamWidth.mfCurrent`; the Omni caller passes a constant Omnis owns,
  holding the value `kfFighterBeamHalfWidth` has today.
- Delete `kfFighterBeamHalfWidth`.
- Declare the function once in `Fighters/Fighters.h` under `BT_CLIENT` and
  delete both hand declarations. `Omnis.cpp` already sees `Fighters.h` through
  `Ships/ShipHitboxes.h`.

Rationale: the explicit parameter removes the branch and puts the Omni width
with the other Omni heal-beam constants. The function stays in `Fighters.cpp`:
two of the three ship types use it and `Ships/` has no client-only visual file
today. `kfFighterBeamStartOffset` stays where it is, since all callers share
that geometry.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipHitboxes.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipQueries.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipQueries.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/FightersCombat.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsCombat.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp`

## In scope

- Shared PreCollision: one new function template in `ShipHitboxes.h`; the
  bodies of `BattleshipsPostRender::PreCollision`,
  `FightersPostRender::PreCollision`, and `OmnisPostRender::PreCollision` after
  their zero-row guard.
- Registry id: a new `ShipRegistryId` declaration in `ShipQueries.h` and its
  definition in `ShipQueries.cpp`; the four `VisitShip` registry-id lambdas
  listed under `## Context`, each replaced by a call that passes the same
  frame's `interpolate` part.
- SyncFighterBeam: its signature, its width selection, and its three call
  sites; `kfFighterBeamHalfWidth` and the new Omni-owned half-width constant;
  the declaration in `Fighters/Fighters.h` and the hand declarations at
  `Fighters/FightersCombat.cpp:35` and `Omnis/Omnis.cpp:72`.

## Out of scope

- The zero-row guards and the three `siCollisionLayerIndex` variables.
- `BuildShipHitbox`, `gShipHitboxShapes`, and `engine::Collision`.
- Blasters and Missiles `PreCollision`.
- `PostCollision` and `AreaDamage`.
- Any other `pRegistryIds` read: the direct column reads in `Frame.cpp`,
  `AgentCommandsServerQueries.cpp`, `FindNearestEnemyFlagship`
  (`ShipQueries.cpp:249`), `BuildShipRegistryWindow`, and every write.
- Changing which frame any registry-id site reads.
- Any other change to `FindAttackTarget` or to the Combat functions beyond the
  three parts above.
- Moving `SyncFighterBeam` to `Ships/` or renaming it.
- `kfFighterBeamStartOffset`, the beam fade, and the heal-beam pulse.
- The other hand declarations in `FightersCombat.cpp` and `Omnis.cpp`, which
  stay inside their own collection.
- Any change to either beam's drawn width.
- `AGENTS.md` files: the PreCollision rule text already describes one
  behavior, and none names the registry-id lambda.

## Risk tier

Tier 2 — scoped behavior. Triggers: a new shared function template in
`Ships/ShipHitboxes.h`, used by the three ship collections, in simulation code
that feeds collision and so CRC'd state; a new `ShipRegistryId` declaration in
a shared game header on a path that feeds the per-tick CRC; and a
`SyncFighterBeam` signature change exposed in `Fighters/Fighters.h` and used
across two collections. `Fighters.h` is in the game precompiled-header closure.

Invariants and exposure:

- Simulation behavior is unchanged: PreCollision row operations, their order,
  and layer registration order stay as they are, and each layer index stays
  `thread_local` per caller.
- Each registry-id site keeps its source frame; the values read are unchanged,
  so no determinism/CRC surface changes.
- The beams are client-only visuals outside the CRC (`Omnis/AGENTS.md`
  `## Heal Beam`); both beams keep their current widths.
- No layout, serialization, `.pack`, `kiVersion`, wire, replay-format,
  threading, allocation, or shader change.

## Acceptance criteria

1. The PreCollision layer add and the two row loops appear once, and each ship
   `PreCollision` holds only its guard and one call.
2. No `VisitShip` lambda returning `pRegistryIds[iRow]` remains under
   `Projects/BrokenEngineSandbox/Source/Frame/Collections/`; the four sites
   call `ShipRegistryId`.
3. The diff shows each registry-id site passing the frame it read before:
   `rPreviousFrame` at the two `ShipQueries.cpp` sites, `rFrame` at the two
   Combat sites.
4. `SyncFighterBeam` has one declaration, in `Fighters/Fighters.h`, and no
   type-index comparison; `kfFighterBeamHalfWidth` is gone; the diff settles
   this.
5. `/compile` builds the BrokenEngineSandbox client and server.
6. One `/agent-harness` scene with Fighters, Battleships, and Omnis fighting
   enemies, where ships of all three types take blaster and missile hits and
   an Omni heals: a replay determinism check reports no CRC mismatch, and a
   client screenshot shows a Fighter beam and an Omni heal beam drawn as
   before. The supplied evidence records no harness setup; the executing
   session builds one.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
