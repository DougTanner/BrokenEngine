<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T18:16:53.765Z","dependsOn":["Documents/Plans/Engine/RemoveModelViewerDefineUiScreens.md"]} -->
# ModelViewer world, stage 1: water Main uniforms as an engine function

## Context

The ModelViewer will render water and terrain from a seeded island cell behind its model, motivated by the
viewer's global tone-mapping settings also affecting water and terrain. User's words (agent-recorded): the viewer
"would be closer to a BrokenEngineSandbox but keep basically the same functionality as it does now (just has
water/terrain rendered behind instead of grey framebuffer)". The work is three stages, one Plan each:

- Stage 1 (this Plan): the generic water part of the game's `RenderFrameMain` becomes an engine function.
- Stage 2 (`Documents/Plans/Engine/ModelViewerModelOwnSpace.md`): the model gets its own view-projection and draws
  after the world over a cleared depth buffer; the viewer camera splits into a world camera and the model's orbit
  camera.
- Stage 3 (`Documents/Plans/Engine/ModelViewerModelOverWorld.md`): seeded islands, water, and the seed UI appear
  behind the model, and the gray background goes.

Recorded decision D9 (user choice, agent-recorded) this Plan implements: split the generic world part of
`MainUniforms.cpp` (water LOD/indirect, waves) into an engine function both `RenderFrameMain` definitions call.
The viewer's call is added in stage 3; this stage changes only where the sandbox's code lives.

After Phase 3 (`Documents/Plans/Engine/RemoveModelViewerDefineUiScreens.md`) the viewer defines its own
`engine::RenderFrameMain` and does not compile `MainUniforms.cpp` (Phase 1 Site 5), while it already compiles
`Engine/Source/Graphics/Render/WaterUniforms.cpp` (ModelViewer.vcxproj:200), the file that owns water's Global
uniforms. Everything the water block reads is engine-declared: `gWaterLevel`
(Engine/Source/Ui/GraphicsQualityWrappersBase.h:21), the `gWater*` wrappers (Engine/Source/Ui/WaterWrappersBase.h),
`gpBufferManager->mpWaterMeshLods`, `gpPipelineManager->mpPipelines`, and `CameraBase` members.

Line numbers cite commit bae72d574b2f422ad876e9804304065b040627c3; Phases 1-3 edit MainUniforms.cpp, so locate
each region by the content named.

## Design

1. New engine function `void RenderWaterMain(int64_t iCommandBuffer, std::chrono::duration<float> currentTime);`
   declared in Engine/Source/Graphics/Render/Render.h beside `RenderLightingMain` (:168), defined in
   WaterUniforms.cpp after `PopulateWaterParameters`. Body, in order:
   - `rMainLayout` from `gpBufferManager->mMainLayoutUniformBuffers.at(iCommandBuffer).mpMappedMemory`, and
     `eWaterLevel` from `gWaterLevel`, both exactly as `RenderFrameMain` computes them (MainUniforms.cpp:510-511).
   - The water block verbatim, from the `// Per-frame visible-area LOD draw params for water.` comment through the
     displacement dispatch `if`/`else` (MainUniforms.cpp:516-542), comments included.
   - `PopulateGerstnerWaves(rMainLayout, currentTime.count(), eWaterLevel);`.
2. Move verbatim from MainUniforms.cpp into WaterUniforms.cpp, above `RenderWaterMain`, with their comments:
   `kfWaveTwoPi`, `GerstnerWaveBandStaging`, `LowWaveTunables`, `MediumWaveTunables`, the four file statics,
   `RebuildLowWaveInvariants`, `RebuildMediumWaveInvariants`, `PopulateGerstnerLowWaves`,
   `PopulateGerstnerMediumWaves`, `PopulateGerstnerWaves` (MainUniforms.cpp:206-454). They stay file-static
   (Engine/Source/Graphics/Render/AGENTS.md `## Ordering and Publication`, the latch rule).
3. The game `RenderFrameMain` replaces the water block with `RenderWaterMain(iCommandBuffer, currentTime);` at the
   same position (after `ResetSkinningAllocations`, before `BeginFrameInterpolateRender`), deletes its trailing
   `PopulateGerstnerWaves(...)` call (:620) and the now-unused `eWaterLevel` local (:511), and keeps its own
   `rMainLayout`, which the view-projection writes still use; the early-return path (:493-507) is unchanged, so it
   still writes no water state.
4. Includes: WaterUniforms.cpp adds `Ui/GraphicsQualityWrappersBase.h`; MainUniforms.cpp drops each include only
   the moved code used (`/update-affected-code` confirms; the include-order check runs in `/compile`).
5. Sandbox equivalence: identical arithmetic on identical inputs. The waves are now written before the collection
   renders instead of after the view-projection; MainLayout is write-only mapped memory read only after
   submission, and nothing between the two points writes the camera fields the waves read (`mVecPosition`,
   `mBasisCoordinate`, `mfCameraEyeHeight`). Each wave latch still runs once per frame.
6. Docs (`/update-claude-docs`): Engine/Source/Graphics/Render/AGENTS.md names `RenderWaterMain` in
   `WaterUniforms.cpp` as Main's water publication that any `RenderFrameMain` calls once per frame.

## Critical files

- Engine/Source/Graphics/Render/MainUniforms.cpp, WaterUniforms.cpp, Render.h, AGENTS.md.

## Risk triggers and invariants

- Tier 2: client render code moved across files with one new engine function; no shader, data, project, CRC,
  PostRender, or determinism surface. Sandbox output unchanged.
- No ModelViewer file changes; the viewer compiles `RenderWaterMain` unused until stage 3.

## In scope

- Render.h: the `RenderWaterMain` declaration.
- WaterUniforms.cpp: the moved wave code, `RenderWaterMain`, one include.
- MainUniforms.cpp: the removed wave code, the water block replaced by the call, the deleted trailing call and
  `eWaterLevel` local, includes.
- Engine/Source/Graphics/Render/AGENTS.md: the one doc line.

## Out of scope

- Any ModelViewer change, including calling `RenderWaterMain` (stage 3).
- `PopulateHexShield`, debug draws, collection render order, camera shake, or any other part of `RenderFrameMain`.
- Changing any wave arithmetic, latch, or comment text beyond moving it; project membership changes.

## Acceptance criteria

- A1 `git grep -n -E "PopulateGerstner|RebuildLowWaveInvariants|kfWaveTwoPi" -- Engine` lists only
  WaterUniforms.cpp; `git grep -n "RenderWaterMain(" -- Engine` lists the declaration, the definition, and one
  call in MainUniforms.cpp.
- A2 `/compile` BrokenEngineSandbox client and server Debug|x64 and ModelViewer Debug|x64 succeed with warnings as
  errors.
- A3 Code read against the session baseline: moved text equals baseline text apart from the stated
  substitutions; the call sits where the water block was; no code between the old and new wave-write points writes
  `mVecPosition`, `mBasisCoordinate`, or `mfCameraEyeHeight`.
- A4 Sandbox Debug client+server per `/agent-harness`, once before the first edit and once after with the same
  steps: after connect and spawn, `describe_scene` `camera` matches between runs and a client `screenshot` of that
  pose shows the same water (waves, LOD, shoreline) with no difference beyond time-driven animation; no Vulkan
  validation (`DEBUG_BREAK at`) lines in either client log.
