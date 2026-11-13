<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-12T17:04:42.731Z","dependsOn":["Documents/Plans/Game/BattleshipWrecksClientCollection.md"]} -->
# Emit two seconds of hull explosions from battleship wrecks and drop the shared death emission

## Context

Stage S5 of the split between simulation state and client-only visual state.
It depends on the stage that made Explosions client-only and on the stage
that adds the client-only Wrecks collection. The Explosions stage kept the
frame engine's name, `FramePostRenderBase::randomEngine`, and added no
visual-random helper: each client-only explosion draws from a local
`common::RandomEngine` seeded from one
`engine::Uuid::GenerateVisual(rFrame.postRender)` mint per explosion, inside
the `BT_CLIENT` guard, and passes it to `ExplosionsPostRender::Spawn`, which
takes it as its `common::RandomEngine& rRandomEngine` parameter. The rule is
`Engine/Source/Frame/AGENTS.md` `## Simulation and Visual State` (bullet
"Random draws") and `Common/Math/AGENTS.md` `## Random Streams`. Evidence
baseline `e2279c55`, before both prerequisite stages, so locate each cited
site by its symbol; game paths are relative to
`Projects/BrokenEngineSandbox/Source/`.

At that baseline a battleship's death explosions are a shared, CRC'd schedule
on the dying row:

- `BeginExplosion` (`Frame/Collections/Battleships/BattleshipsCombat.cpp:36-60`)
  sets `pDestroyedExplosionTimes` to `kBattleshipDestroyExplosionInterval`,
  stores `pVecDamageDirections`, and spawns one explosion along the velocity.
- The Battleships Spawn hook (`Battleships.cpp:335-346`) spawns another each
  time the timer runs out, flying away from the stored damage direction, for
  the row's `kBattleshipDestroyTime` (`Battleships.h:26`) of dying.
- `pDestroyedExplosionTimes` and `pVecDamageDirections` are shared columns
  (`Battleships.h:233-235`, in `SharedMembers()` `:256`, `PersistentMembers()`
  `:274`, `SharedCrcMembers()` `:280`). The staggered emission
  (`Battleships.cpp:343`) is the only reader of either; the Fighters column of
  the same name is simulation (death knockback, `Fighters.cpp:666`) and is not
  touched.

The Explosions stage put both spawns, `SpawnBattleshipExplosion` with its
declaration in `BattleshipsCombat.cpp`, and `giBattleshipExplosionTypeIndex`
under `BT_CLIENT`; `SpawnBattleshipExplosion` seeds its local engine from one
`GenerateVisual` mint. The timer and damage direction remain shared state
that no simulation decision reads, and the effect lasts only
`kBattleshipDestroyTime` at one point.

The wanted effect: two seconds of complete explosions from random points on
the battleship's convex hull, following the moving, tilting wreck. The
`.hitbox` data has no hull: `ShipHitboxShape::hitbox` keeps only the four box
corners and a possibly concave outline
(`Frame/Collections/Ships/ShipHitboxes.cpp:41-65`). The convex hull exists
only in the packed `common::ModelOutline::pf2HullVertices`
(`Common/DataFile.h:387`), which `InitializeShipHitboxes` reads to derive the
box and radius and then drops; those vertices are in the same aligned,
gameplay-size, +X-facing frame as the box, whose corners are their unchanged
minimum and maximum (`ShipHitboxes.cpp:50-65`).

Does a wreck need the damage direction? No reader other than the staggered
emission exists, so nothing carries it once that emission goes. The author
recommends directing each hull explosion horizontally from the hull center
out through its anchor, jittered by the existing
`kfBattleshipExplosionDirectionJitter`, so no damage direction is captured
into the Wrecks row (`## Notes` records the alternative).

Change Workflow tier: Tier 3. Trigger: removing two shared Battleships
columns and their timer logic changes the Battleships PostRender layout, the
CRC, and save/replay compatibility, and the change spans the ship hitbox data
and the client-only Wrecks and Explosions collections.

## Design

1. Hull access. `ShipHitboxShape` gains
   `std::span<const XMFLOAT2> hullVertices`, set in
   `InitializeShipHitboxes` from `rOutline.pf2HullVertices` and
   `rOutline.iHullVertexCount`, borrowed from the eager chunk exactly as
   `missileTubes` and `fighterLaunchBays` are (`ShipHitboxes.h:43-44`,
   `ShipHitboxes.cpp:35-39`). No alignment is applied again.
2. Emission. In `WrecksPostRender::Spawn`, after capture, each non-`kDying`
   Wrecks row whose `pFallTimes` is below a new
   `kWreckExplosionDuration` of two seconds emits one explosion per tick: the
   capture tick emits at age 0, and every later tick at its advanced age, so
   one row emits `kWreckExplosionDuration` times `kiTickRate`
   (`Engine/Source/Frame/TimeStep.h`) explosions, at ages from 0 up to one
   tick short of `kWreckExplosionDuration`. Each emission seeds one local
   `common::RandomEngine` from one
   `engine::Uuid::GenerateVisual(rFrame.postRender)` mint, the established
   explosion pattern, draws every value below from it, passes it to
   `ExplosionsPostRender::Spawn`, and:
   - picks a triangle of the vertex-0 fan over `hullVertices` with
     probability proportional to its area and samples a point in it with
     reflected-square barycentric coordinates;
   - places it at model-local z = 0, then applies the row's yaw, the tilt the
     renderer would use at the row's current height (the slerp toward
     `pVecTilts` by the fall fraction), and the row's cell-local position,
     the same order as `WriteModelLayout`
     (`Battleships/BattleshipsRender.cpp:236-262`);
   - spawns `ExplosionsPostRender::Spawn` with the battleship explosion type
     and the existing size, smoke, trail, and particle formulas of
     `SpawnBattleshipExplosion` (`Battleships.cpp:141-160`) at
     `fPercent = 1 - age / kWreckExplosionDuration`, with no further position
     jitter, its direction as recommended in `## Context`.
   An anchor outside the cell hexagon is refused by `Spawn`'s existing
   `IsInsideCell` test and skipped for that tick.
3. Removal from Battleships. Delete `SpawnBattleshipExplosion` and its
   declaration (`BattleshipsCombat.cpp:31`), the `BeginExplosion` spawn
   (`:55-59`) and its `vecDamageDirection` parameter with the arguments its
   callers pass (`:78`, `:161`, `:211`) and any computation left unused
   (including the client-only velocity direction), any `BT_CLIENT` guard
   these removals leave empty, the
   staggered loop in `BattleshipsPostRender::Spawn`
   (`Battleships.cpp:335-346`, keeping `SpawnMissiles`),
   `kBattleshipDestroyExplosionInterval` (`Battleships.h:27`), and both
   columns with every site: spawn initialization (`Battleships.cpp:426`,
   `:428`), the PostRender Update timer (`:484`, `:587`), `LogDifferences`
   (`:638`, `:640`), the `BeginExplosion` writes (`BattleshipsCombat.cpp:44`,
   `:47`), the member tuples, and the `edit_frame` columns
   (`Agent/Commands/ServerFrameEdit.cpp:23`). The battleship explosion type
   registration and `giBattleshipExplosionTypeIndex` stay for the wreck
   emission; the emission constants the wreck uses move to the Wrecks files,
   and `kfBattleshipExplosionPositionJitter` goes.
4. Bump `BattleshipsPostRender::kiVersion` (`Battleships.h:173`) by one; the
   `Frame::kiVersion` total rises with it.

## Critical files

- `Frame/Collections/Ships/ShipHitboxes.h`, `ShipHitboxes.cpp`
- `Frame/Collections/Wrecks/Wrecks.h`, `Wrecks.cpp`
- `Frame/Collections/Battleships/Battleships.h`, `Battleships.cpp`,
  `BattleshipsCombat.cpp`
- `Agent/Commands/ServerFrameEdit.cpp`
- `Frame/Collections/Battleships/AGENTS.md`

## In scope

- `ShipHitboxShape::hullVertices` and its fill (Design 1).
- The emission in `WrecksPostRender::Spawn`, `kWreckExplosionDuration`, and
  the moved emission constants (Design 2).
- Every Battleships removal Design 3 lists, and the
  `BattleshipsPostRender::kiVersion` bump (Design 4).
- `Frame/Collections/Battleships/AGENTS.md`: the Weapon Invariants sentence
  that the Spawn hook "then emits the staggered death explosions" goes; the
  wreck section gains the two-second hull emission and the outside-cell
  skip. `Frame/Collections/Ships/AGENTS.md` only where it describes the
  battleship death explosion.

## Out of scope

- Fighter and Omni death explosions and their timers (another stage).
- `ModelOutline`, DataPacker, the `.pack` format, and the hitbox bake.
- The death sound, death duration, wreck motion, and explosion tuning.
- Admitting explosion positions outside the cell.
- Renaming `FramePostRenderBase::randomEngine` or adding a shared
  visual-random helper.
- Backward compatibility.

## Coordination

- `Documents/Plans/Game/ClientOnlyDeathExplosionTimers.md` also edits the
  `edit_frame` column lists (`Agent/Commands/ServerFrameEdit.cpp:21-23`):
  that stage the Fighters and Omnis lines (`:21`, `:22`), this stage the
  Battleships line (`:23`). Whichever lands second rebases onto the other
  and keeps the other's removals.

## Acceptance criteria

- Builds: `/compile` BrokenEngineSandbox client and BrokenEngineSandboxServer
  Debug|x64. No ModelViewer build: no header in its project changes.
- Emission count: for one combat death, a temporary client trace (removed
  after capture) records `kWreckExplosionDuration` times `kiTickRate`
  emissions with ages below `kWreckExplosionDuration` and none at or after
  it, each anchor inside the transformed hull polygon.
- Visuals: screenshots near 0, 0.5, 1.5, and 2.1 s after a random-controller
  Battleship's combat death show full explosions spread over the hull while
  the wreck falls, and none at 2.1 s. The author recommends lowering its
  `pfHealths` with `edit_frame` and flying an enemy `SpawnBlaster` into it;
  no harness run has exercised this setup.
- Determinism: `replay_record` then `replay_play` over that death report no
  CRC mismatch, and the connected client's `get_logs` with a `[Dd]esync`
  pattern returns no lines.
- Version gate: a replay or grid save from before the change is rejected.
- No `pDestroyedExplosionTimes` or `pVecDamageDirections` remains in the
  Battleships collection.

## Notes

- Originating gap: the user-directed simulation/visual split and the wanted
  two-second hull explosions.
- Exposure: Battleships data layout, CRC and save/replay compatibility, ship
  hitbox shape data borrowed from the eager chunk, client-only emission on
  the per-cell worker, live verification.
- Alternative not recommended: keep the damage direction for the explosion
  heading by moving it into a Battleships `ClientMembers()` column written by
  `BeginExplosion` under `BT_CLIENT` and copying it into a Wrecks column at
  capture. It keeps the old heading at the cost of one client-only column on
  each of two collections, while the authoring session's direction was to
  remove the column from Battleships.
- An earlier version of this Plan used a `MakeVisualRandom` helper keyed by
  coordinate, tick, and row index; the Explosions stage adopted none, so this
  Plan uses the per-mint seeding that stage established.
