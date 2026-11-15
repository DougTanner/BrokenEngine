<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:16:45.259Z","dependsOn":[]} -->
# Skip Expired Dying Omni Draws and Move the Ship-Model Render Helpers Into the Engine

## Context

Paths under `Frame/Collections/` (and bare collection paths such as
`Omnis/OmnisRender.cpp`) are relative to
`Projects/BrokenEngineSandbox/Source/`. Line numbers in Part A are from commit
`106613c6`, in Part B from commit `b5c75660`; locate by symbol if they moved.

### Part A: a dying Omni drawn at full size

`RenderPipeline` in `Omnis/OmnisRender.cpp` picks the death scale from the
interpolated `pDestroyedTimes` alone (`:191-195`): a positive time shrinks the
hull, anything else draws at scale 1. An alive Omni also stores zero
(`Omnis/Omnis.cpp:273`), so the test cannot tell "alive" from "dying and
finished".

Traced in code, not run live:

- `OmnisPostRender::BeginExplosion` sets `kDying` and `pDestroyedTimes` to
  `kDestroyTime` (`Omnis/OmnisCombat.cpp:100`, `:102`).
- Each later tick, Interpolate `Update` writes the previous value minus the
  tick delta, clamped at zero, for a row whose previous PostRender is `kDying`
  (`Omnis/Omnis.cpp:374-377`, `:381`). The Destroy sweep removes the row on the
  tick that value reaches zero (`Omnis/Omnis.cpp:215-218`).
- `kDestroyTime` is not a whole number of ticks at `kiTickRate`, so the last
  committed frame that still holds the row holds a positive remainder smaller
  than one tick.
- Render runs Interpolate `Update` on a copy of the source frame with a delta
  inside the one-tick window (`Engine/Source/GameBase.cpp:830-832`,
  `Engine/Source/GameBase.h:336`). Once that delta passes the remainder, the
  copy holds zero, the scale test fails, and the hull draws at scale 1 until
  the next tick removes the row. The render frame before it draws the hull
  almost fully shrunk.

So a dying Omni that is not healed (a random-spawn Omni; a fleet Omni heals
instead, `Omnis/OmnisCombat.cpp:114-125`) can be drawn at full size for the
end of its final tick window. Fighters avoid this: alive Fighters store -1
(`Fighters/Fighters.cpp:494`), and `Fighters/FightersRender.cpp:135-138` skips
a row whose time is exactly zero.

How visible the full-size draw is among the death explosions is unproven: the
path is proven from code only.

Originating gap: the in-function refactoring phase of a findings-only analysis
of the `Omnis` directory, confirmed by an independent verification read. It is
pre-existing debt. Unmet criterion: no render frame draws a `kDying` Omni row
at scale 1.

### Part B: repeated ship-model render code

Four BrokenEngineSandbox render files and the ModelViewer each repeat the code
that draws instanced models through the engine's model pipelines. A new game
project (Coretide) would copy it again, so the user asked for the extraction
to be planned before that project starts.

Nothing was built or run for Part B: every statement below is from a code
read.

Three repeated pieces:

1. The `shaders::ModelLayout` fill. After a caller has built its instance
   transform, the same statements write `f4PositionLayer`, the shadow range
   from `engine::ComputeModelShadowRange`, the flight-plane index from
   `engine::gpFlightPlanes->NearestPlaneIndex`, `f3x4Transform`,
   `f3x4TransformNormal`, `f4ColorAdd`, and `uiMeshDataBase` (zero, then
   `AnimationData::EvaluateInstance` when the model is animated):
   - `Frame/Collections/Fighters/FightersRender.cpp` 192-205
   - `Frame/Collections/Battleships/BattleshipsRender.cpp` 130-142, already a
     file-local `WriteModelLayout` (118-143) that also builds the transform
   - `Frame/Collections/Omnis/OmnisRender.cpp` 213-226
   - `Frame/Collections/Missiles/MissilesRender.cpp` 129-136, with no
     animation branch
   - `Projects/ModelViewer/Source/ModelViewerApplication.cpp` 812-834, which
     differs: it clears the layout first, writes the position and plane only
     while flying, and passes `matAlignmentDelta * matInstanceTransform` to
     the shadow range.
2. The render-active guard. Each BrokenEngineSandbox render file declares a
   file-local `std::atomic<bool> sbRenderActive` and an identical guard struct
   that asserts the flag was clear and clears it on scope exit:
   `FightersRenderActiveGuard` (`FightersRender.cpp` 41-58),
   `BattleshipsRenderActiveGuard` (`BattleshipsRender.cpp` 40-57),
   `OmnisRenderActiveGuard` (`OmnisRender.cpp` 43-60),
   `MissilesRenderActiveGuard` (`MissilesRender.cpp` 20-37).
3. The per-controller pipeline code. Fighters, Battleships and Omnis each keep
   one model pipeline per `ShipController` and repeat the same loops over
   `kiPipelineCount` in `GraphicsResources` (create the dynamic buffer and the
   model pipelines), `BeginRender` (resize the buffer and update the
   descriptors), `RequestTextures` (request textures for each pipeline that
   has a row), and `EndRender` (write the indirect buffers):
   `FightersRender.cpp` 29-36, 60-82, 84-110, 240-250; the functions of the
   same names start at `BattleshipsRender.cpp` 28, 61, 89, 272 and
   `OmnisRender.cpp` 31, 103, 127, 256. Missiles does the same for a single
   pipeline (`MissilesRender.cpp` 39-76, 140-147).

The engine already owns the neighbouring helpers:
`engine::ComputeModelShadowBounds` and `engine::ComputeModelShadowRange`
(`Engine/Source/Graphics/Render/Render.h` 21, 40) and
`engine::AccumulateRenderCapacity`
(`Engine/Source/Frame/Collections/Collection.h` 384).

Originating gap: a read-only audit of code a second game would have to copy.
It is not an unmet acceptance criterion of any change.

## Design

Run Part A first, then Part B. Both touch the row loop of `RenderPipeline` in
`OmnisRender.cpp`: Part A adds the skip test before the visibility test, and
Part B replaces the `ModelLayout` fill later in the same loop.

### Part A: skip the expired dying row

Recommended (the author's recommendation): in the row loop of `RenderPipeline`
(`Omnis/OmnisRender.cpp:184-230`), skip a row that is `kDying` in
`rPostRender.pLifecycleFlags` and whose interpolated `pDestroyedTimes` is not
positive, before the visibility test at `:200`.

Rationale: `rPostRender` is already a parameter of the function, and skipping
the row instead of scaling it to zero keeps the `XMMatrixInverse` of the
instance transform off a singular matrix. The count loop at `:158-162` may
then reserve for the skipped rows, as it already does for culled rows; leave
it.

### Part B: engine render helpers

Recommended shape (the author's recommendation; rationale given):

- One engine function that fills a `shaders::ModelLayout` from the values the
  callers already have: the shadow bounds, the alignment matrix, the instance
  transform, the position already converted into the camera's cell, and the
  optional animation inputs. Each caller keeps building its own instance
  transform, because the scale, roll, tilt and yaw terms differ per ship type.
  Rationale: the common part is exactly the statements listed above, and it
  sits beside `ComputeModelShadowRange` in `Render.h`.
- One engine guard type that takes the caller's flag, replacing the four
  identical structs; each file keeps its own flag.
- Engine helpers for the per-pipeline loops that take the pipeline CRCs, names
  and models as spans, so the engine names no game concept such as
  `ShipController` (`Engine/Source/AGENTS.md` `## Hub Conventions`). The
  executing session extracts only loops that are the same in all users after
  reading them side by side; a loop that differs stays in the game.
- The ModelViewer site adopts the fill function only if its output stays the
  same; otherwise it is left as it is and that is reported.
- `Frame/Collections/Battleships/AGENTS.md` lines 1-10 require Battleships
  and Fighters to stay parallel: whatever one file adopts, the other adopts in
  the same form, and the code left behind in the two files stays mirrored.

The seam is the engine/game contract in `Engine/Source/AGENTS.md`
`## Hub Conventions`: game code calls the engine helpers directly, and no
virtual function, callback or function pointer is added.

### Risk

Risk tier: Tier 2 (the authors' classification for both parts). Triggers:
client render behavior of one subsystem (the Omni row skip), and client render
behavior with a new engine-owned signature used by two projects (the helpers).
Exposure: client render only. `RenderPipeline` writes no simulation state, and
the layouts are client-only visual state outside the CRC, so no determinism,
save, `.pack`, `kiVersion`, wire, replay or shader-layout change is expected.
Threading: the fills run inside `common::gpMultithreading->Dispatch` in
`FightersRender.cpp` and `BattleshipsRender.cpp`, so the fill function stays
free of shared mutable state, and the render-active guard keeps its current
meaning. Build: any new engine file needs project membership through
`/update-vcxproj`.

## Critical files

- `Engine/Source/Graphics/Render/Render.h` (and a new engine file only if the
  helpers do not fit there)
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/FightersRender.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsRender.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/OmnisRender.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesRender.cpp`
- `Projects/ModelViewer/Source/ModelViewerApplication.cpp`

## In scope

- `RenderPipeline` in `Omnis/OmnisRender.cpp`: the row loop's skip test for an
  expired `kDying` row.
- The `ModelLayout` fill statements at the five sites listed in `## Context`,
  including the file-local `WriteModelLayout` in `BattleshipsRender.cpp`.
- The four `*RenderActiveGuard` structs and their uses in the four `Render`
  functions.
- The per-pipeline loops in `GraphicsResources`, `BeginRender`,
  `RequestTextures` and `EndRender` of the four BrokenEngineSandbox render
  files.
- The new engine helpers, their project membership, and the documentation
  sync `/update-claude-docs` requires.

## Out of scope

- The alive value of `pDestroyedTimes`, and any change to Interpolate
  `Update`, `BeginExplosion`, or the Destroy sweep.
- How each ship type builds its instance transform, its death scale
  expression (including `kfDeathShrinkPower`), its visibility test, and its
  `RenderPipeline` dispatch, apart from the Omni skip test above.
- The count loop of the Omni `RenderPipeline`.
- Any death-time skip in Fighters or Battleships render.
- The wreck drawing in `BattleshipsRender.cpp`, beyond its call to the fill
  function.
- `DebugRender`, the combat aim drawing in `OmnisRender.cpp`, and the profile
  counters and timers.
- `shaders::ModelLayout`, the model pipelines in `DynamicPipelines`, and
  `BufferManager`.
- Engine collection render files, and any other ModelViewer code.

## Acceptance criteria

1. No path through the Omni `RenderPipeline` row loop draws a row that is
   `kDying` with a non-positive interpolated `pDestroyedTimes`; the diff
   settles this. The full-size draw lasts less than one tick and no existing
   `/agent-harness` command shows it observably, so this criterion is verified
   by code reading plus the build of criterion 2, with no live observation.
2. `/compile` builds the BrokenEngineSandbox client and the ModelViewer.
3. Each repeated piece that was extracted has one definition in the engine,
   and `FightersRender.cpp` and `BattleshipsRender.cpp` remain parallel in a
   side-by-side read.
4. Through `/agent-harness`, screenshots of one paused scene that shows
   Fighters, Battleships, Omnis, Missiles and a Battleship wreck, with their
   shadows, are the same before and after the change. The supplied evidence
   records no harness setup, so the executing session builds the scene, saves
   it, and loads that save paused before each capture.
5. A ModelViewer screenshot of one animated model, taken through
   `/agent-harness`, is the same before and after.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
