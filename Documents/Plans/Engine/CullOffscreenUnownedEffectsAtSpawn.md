<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-12T17:04:36.661Z","dependsOn":[]} -->
# Skip spawning unowned visual effects outside the camera's visible area

## Context

Stage S3 of the split between simulation state and client-only visual state.
It depends on the stage that makes Explosions a client-only collection and
adds `Engine/Source/Frame/AGENTS.md` `## Simulation and Visual State`. The
authoring session recorded as the user's choice (D7) a spawn-time off-screen
cull for effects no simulation row owns; owned visuals (lights, trails,
shields synced from their owner each update) keep today's draw-time culling,
and battleship wrecks keep their cell rule in their own stage. Evidence
baseline `e2279c55`.

Observed cost: an explosion spawned anywhere in a loaded cell creates its
point lights, puffs, wind radial, smoke trails, and rows
(`Engine/Source/Frame/Collections/Explosions/ExplosionsSpawn.cpp:28-247`),
even when nothing of it can reach the screen; only its GPU particles are
culled, one by one, at `ParticleManager::Spawn`
(`Engine/Source/Graphics/Managers/ParticleManager.cpp:16-21`). The blaster
terrain impact likewise always adds a crater light and a puff
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/BlastersUpdate.cpp:233-236`).

Whether tick code may read the camera, settled from code: it already does.
`ExplosionsPostRender::Spawn` builds a basis from `gpCamera->mBasisCoordinate`
on the per-cell tick worker (`ExplosionsSpawn.cpp:193`), and
`ParticleManager::Spawn` reads `gpCamera->mf4RenderVisibleArea` during the
parallel dispatch, its comment calling the rectangle immutable there
(`ParticleManager.cpp:16-21`). The camera changes only in
`GameBase::UpdateCamera` (`Engine/Source/GameBase.cpp:871-881`), on the render
path after the frame interpolation. So no per-cell cull rectangle is
published; the cull reads the camera directly, under `BT_CLIENT`. The
executing session confirms this before relying on it (`## Acceptance
criteria`).

Change Workflow tier: Tier 2. Trigger: client-only spawn behaviour in one
subsystem once the Explosions collection is client-only; no CRC, wire,
layout, or save surface changes. Executed before that prerequisite it would be
Tier 3, which the `dependsOn` edge prevents.

## Design

1. Explosions: at the top of `ExplosionsPostRender::Spawn`, after the
   `IsInsideCell` refusal and before any append or draw, rebase the spawn
   position with `MakeRenderBasis(coordinate, gpCamera->mBasisCoordinate)`
   (move the existing basis from `:193` up and reuse it for the particles)
   and return `false` when
   `gpCamera->InVisibleArea(gpCamera->mf4RenderVisibleArea, rebased, margin,
   margin, margin, margin)` (`Engine/Source/Graphics/CameraBase.h:223-233`)
   fails. Every child effect is created inside `Spawn`, so this one test
   covers lights, puffs, wind, trails, and particles. Callers already ignore
   or tolerate a `false` return (it is the outside-cell result).
2. The margin is one `inline constexpr float kfUnownedEffectCullMargin` in
   `Explosions.h`, which the blaster site reaches through `FrameBase.h`. Its
   value is the largest distance, at default tuning, that any registered
   explosion type's or the blaster crater's visible footprint reaches from
   the spawn position: the longest trail
   (`fTrailStart` plus trail length maximum times the default
   primary-trail length and duration multipliers), the farthest secondary
   offset, the particle position jitter, and the point-light lighting reach.
   The comment records that derivation, not the inputs' values.
3. Blaster terrain impact: in the `BT_CLIENT` block, skip the crater light
   and puff when the jittered impact position fails the same test, with a
   basis from `rStaticData.coordinate`. The one-shot sound stays: the author
   recommends leaving audio to its distance attenuation, which this stage
   does not inspect. If the visual-draw stage has not landed, the shared
   jitter and rotation draws stay before the test, unconditionally.
4. Add the off-screen bullet to `Engine/Source/Frame/AGENTS.md`
   `## Simulation and Visual State`: "Off-screen: a visual no simulation row
   owns is not spawned outside the camera's visible area grown by its reach;
   an owned visual follows its owner; nothing visual exists for a cell that
   is not loaded."

## Critical files

- `Engine/Source/Frame/Collections/Explosions/ExplosionsSpawn.cpp`,
  `Explosions.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/BlastersUpdate.cpp`
- `Engine/Source/Frame/AGENTS.md`

## In scope

- `ExplosionsPostRender::Spawn`: the early cull test and the moved basis.
- `kfUnownedEffectCullMargin` in `Explosions.h`.
- The client effect calls in the Blasters terrain-impact branch.
- The `## Simulation and Visual State` off-screen bullet, and the Explosions
  `AGENTS.md` only if a statement there becomes false.

## Out of scope

- Pruning running rows that leave view (D7 chose spawn-time only).
- Owned visuals, wrecks, sounds, and GPU particle culling.
- Per-type reach values, new tuning wrappers, or a published cull rectangle.

## Coordination

- `Documents/Plans/Game/VisualDrawsLeaveSimulationRandom.md` also edits the
  blaster terrain-impact block. Whichever lands second rebases onto the
  other: this stage's test gates only the client effect calls and tests the
  jittered impact position, so inside the client block the jitter and
  rotation draws come first and the cull test follows them.
- `Documents/Plans/Game/BattleshipWrecksClientCollection.md` also edits
  `Engine/Source/Frame/AGENTS.md` `## Simulation and Visual State`, extending
  its simulation-state and visual-state bullets. The two stages change
  different bullets; whichever lands second rebases its bullet onto the
  other's.

## Acceptance criteria

- Camera-read check: a search for writes to `gpCamera` members
  (`mf4RenderVisibleArea`, `mBasisCoordinate`) finds none reachable from the
  client reconcile dispatch or `RunFrameTick`; otherwise stop and surface the
  per-cell cull rectangle alternative instead of implementing Design 1.
- Builds: `/compile` BrokenEngineSandbox client and BrokenEngineSandboxServer
  Debug|x64, and ModelViewer Debug|x64 because `Explosions.h` is in its
  project.
- Cull: with the camera framing one area, a missile explosion and a Battleship
  death well outside the visible area add no Explosions, PointLights, or
  Puffs rows on the client (`read_frame` on the client cell, or the profile
  counters), while the same events on screen show their full effect in a
  screenshot. The author recommends `inject_payload` spawns in a neighbour of
  the camera's cell; no harness run has exercised this setup.
- Edge: an explosion just outside the visible area whose trails reach into it
  still spawns (margin check), shown in a screenshot.
- Determinism: client/server CRC agreement holds during the scenarios
  (`get_logs` with a `[Dd]esync` pattern returns no client lines); the server
  is unchanged, so no replay check is required.

## Notes

- Originating gap: D7 of the simulation/visual split.
- Exposure: client-only behaviour; tick-worker reads of camera state
  (read-only, settled above); live verification.
- A rollback re-simulated after the camera moved may spawn or skip a
  different set of unowned effects; the rule section states visual state
  carries no determinism guarantee.
