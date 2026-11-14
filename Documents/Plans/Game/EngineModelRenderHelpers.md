<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T23:35:10.329Z","dependsOn":[]} -->
# Move the Ship-Model Render Helpers Into the Engine

## Context

Four BrokenEngineSandbox render files and the ModelViewer each repeat the code
that draws instanced models through the engine's model pipelines. A new game
project (Coretide) would copy it again, so the user asked for the extraction
to be planned before that project starts.

Nothing was built or run for this Plan: every statement below is from a code
read at commit `b5c75660`. Line numbers are from that tree; locate by symbol
if they moved. Paths under `Frame/Collections/` are relative to
`Projects/BrokenEngineSandbox/Source/`.

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

Risk tier: Tier 2 (the author's classification). Trigger: client render
behavior of one subsystem with a new engine-owned signature used by two
projects. Exposure: client render only. The layouts are client-only visual
state outside the CRC, so no determinism, save, `.pack`, `kiVersion`, wire,
replay or shader-layout change is expected. Threading: the fills run inside
`common::gpMultithreading->Dispatch` in `FightersRender.cpp` and
`BattleshipsRender.cpp`, so the fill function stays free of shared mutable
state, and the render-active guard keeps its current meaning. Build: any new
engine file needs project membership through `/update-vcxproj`.

## Critical files

- `Engine/Source/Graphics/Render/Render.h` (and a new engine file only if the
  helpers do not fit there)
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/FightersRender.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsRender.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/OmnisRender.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesRender.cpp`
- `Projects/ModelViewer/Source/ModelViewerApplication.cpp`

## In scope

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

- How each ship type builds its instance transform, its death scale, its
  visibility test, and its `RenderPipeline` dispatch.
- The wreck drawing in `BattleshipsRender.cpp`, beyond its call to the fill
  function.
- `DebugRender`, the combat aim drawing in `OmnisRender.cpp`, and the profile
  counters and timers.
- `shaders::ModelLayout`, the model pipelines in `DynamicPipelines`, and
  `BufferManager`.
- Engine collection render files, and any other ModelViewer code.

## Acceptance criteria

1. `/compile` builds the BrokenEngineSandbox client and the ModelViewer.
2. Each repeated piece that was extracted has one definition in the engine,
   and `FightersRender.cpp` and `BattleshipsRender.cpp` remain parallel in a
   side-by-side read.
3. Through `/agent-harness`, screenshots of one paused scene that shows
   Fighters, Battleships, Omnis, Missiles and a Battleship wreck, with their
   shadows, are the same before and after the change. The supplied evidence
   records no harness setup, so the executing session builds the scene, saves
   it, and loads that save paused before each capture.
4. A ModelViewer screenshot of one animated model, taken through
   `/agent-harness`, is the same before and after.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
