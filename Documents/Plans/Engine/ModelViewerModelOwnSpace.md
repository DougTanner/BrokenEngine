<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T18:17:44.927Z","dependsOn":["Documents/Plans/Engine/ModelViewerWorldUniforms.md"]} -->
# ModelViewer world, stage 2: the model in its own space over a world camera

## Context

Stage 2 of three (stage 1 `Documents/Plans/Engine/ModelViewerWorldUniforms.md`, stage 3
`Documents/Plans/Engine/ModelViewerModelOverWorld.md`; the shared context is in stage 1). User's words
(agent-recorded): the model "would just exist in it's own 3D space not in world-space and would not check Z so it
would always render over the 'background'". Recorded decisions (user choices, agent-recorded) this stage
implements:

- D4: a fixed top-down world camera at the sandbox's default height, separate from the model's orbit camera.
- D5: same pass: draw the world, clear depth, draw the model with its own view-projection (the model still
  self-occludes; no extra target).
- D6 (one shared sun) needs only the sun drag's screen basis here: the sun rotation is already world-space and
  applied to every sun uniform (Phase 1 Site 6).

Nothing new becomes visible in this stage: the viewer still writes no terrain or water indirect record, so its
world draws keep zero instances (Phase 1 `### Why the convergence changes nothing on screen`), and its look stays
exactly as before. Stage 3 makes the world visible.

Current state after Phase 3 (line numbers cite commit bae72d574b2f422ad876e9804304065b040627c3; locate by content):

- `game::Camera` (ModelViewer/Source/Graphics/Camera.h/.cpp) writes the orbit pose into the `CameraBase` members
  every engine pass reads: `mMatView`, `mMatPerspective`, `mVecEyePosition`, `mVecToEyeNormal`,
  `mfCameraEyeHeight`, both texel eye heights, the visible areas (Camera.cpp:16-34), with `mVecPosition` as the
  model center (`Fit`, :37-43). `RotateSun` reads `mMatView` for the screen basis (:50-62).
- Model draws read `mainLayout.f4x4ViewProjection` (Engine/Data/Shaders/Model/ModelCommon.h:103, mode 0) and the
  view vector from `mainLayout.f4EyePosition` (Engine/Data/Shaders/Model/Model.frag:229); world draws read the same
  two fields, and one mapped MainLayout serves the whole pass, so the model needs fields of its own.
- The image pass records UI depth, opaque models, terrain, water, hex shields, transparent models, then particles,
  lights, billboards, debug (Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp:342-445). The begin clears
  depth to `kfMaxDepth` (Engine/Source/Graphics/Objects/Texture.cpp:73).
- `CameraBase::CalculateMatricesAndVisibleArea` (protected, CameraBase.cpp:381) builds the straight-down view,
  perspective, visible areas, and visible-area LOD from `mVecPosition` and `mVecEyePosition`; the sandbox sets the
  target Z to `gpFlightPlanes->Lowest().fHeightMeters` and the eye `mfCameraEyeHeight` above it (CameraBase.cpp:178,
  189-193). The sandbox default is `CameraBase::kfCameraEyeHeightDefault` (CameraBase.h, 300 m).

## Design

1. MainLayout (Engine/Data/Shaders/ShaderMainLayout.h after `f4BillboardUp`, :19): add
   `vec4 f4x4ModelViewProjection[4] INIT;` and `vec4 f4ModelEyePosition INIT;` under one comment: main-pass model
   draws read these; a host that draws models in the world writes the world camera's values. No pack version bump
   (precedent: 29ced7a17 added MainLayout fields).
2. Shaders: ModelCommon.h:103 mode 0 reads `mainLayout.f4x4ModelViewProjection`; Model.frag:229 reads
   `mainLayout.f4ModelEyePosition`. Model.frag:306 (the world lighting-deposit sample line) keeps
   `f4EyePosition`: it samples a world texture, and the viewer zeroes that term (D7). Modes 1 and 2 are unchanged.
3. Sandbox: the game `RenderFrameMain` (MainUniforms.cpp:608-610) computes the transposed view-projection once
   into a local and stores it to both `f4x4ViewProjection` and `f4x4ModelViewProjection`, and stores
   `mVecEyePosition` to both `f4EyePosition` and `f4ModelEyePosition`. Every sandbox shader input is bit-identical.
4. Program switch `kbDrawModelsOverWorld`, in the program-switch block Phase 2 created in each `Pch.h`, after
   `kbProfileScreens`: sandbox `false`, viewer `true`. It names the feature, not the application.
5. `CommandBufferRecordMain::RecordImageRenderPass`: the UI depth pre-pass block (:342-351) and the opaque model
   loop (:365-370, timer included) become two local lambdas. The opaque-model lambda body is unchanged; the
   pre-pass lambda body is :345-349, and its `kGpuTimerUiDepth` `GpuStart`/`GpuStop` (:344, :350) bracket only its
   first call, because each timer writes a fixed query index reset once per command buffer
   (ProfileManager.cpp:310, 325-326, 352-353), so a second timed run would rewrite a query that is not unavailable
   (VUID-vkCmdWriteTimestamp-None-00830). The timed UI pre-pass stays at its current position, before the
   debug-texture branch (:353-361), under both switch values, so debug-texture mode records exactly what it does
   today. With the switch false the opaque models also run at their current position, so the recorded commands
   equal today's. With it true, the opaque models are not recorded there; inside `!bDebugTextureMode`, after the
   world draws' closing `#endif` (:399, which Phase 1 Site 4 deletes) and before the transparent models'
   `GpuStart` (:401), record `vkCmdClearAttachments` for the depth aspect to `kfMaxDepth` over
   `gpGraphics->mFramebufferVkExtent2D` (layer 0, count 1), then the untimed UI pre-pass again, then the opaque
   models. Each switch-dependent site is an `if constexpr`. Re-running the pre-pass
   after the clear keeps the model's UI culling exactly as today; the first run keeps the sandbox's UI culling for
   the world draws.
6. Viewer camera (Camera.h/.cpp):
   - New public members `XMVECTOR mVecModelCenter {0, 0, 0, 1}`, `XMMATRIX mMatModelView`,
     `XMMATRIX mMatModelPerspective`, `XMVECTOR mVecModelEyePosition`.
   - `Update()` keeps the orbit arithmetic verbatim but reads `mVecModelCenter` and writes the three model members.
     It no longer writes any `CameraBase` member. Then it sets the world camera:
     `mVecPosition = XMVectorSetZ(mVecPosition, engine::gpFlightPlanes->Lowest().fHeightMeters)`,
     `mfCameraEyeHeight = kfCameraEyeHeightDefault`, both texel eye heights to it, `mVecToEyeNormal = (0, 0, 1, 0)`,
     and `mVecEyePosition = mVecPosition + (0, 0, mfCameraEyeHeight, 0)`, then calls
     `CalculateMatricesAndVisibleArea()`, mirroring CameraBase.cpp:189-193, 198. `mVecPosition` keeps the constructor's
     XY (the cell origin) in this stage.
   - `Fit` writes `mVecModelCenter` instead of `mVecPosition`; `RotateSun` reads `mMatModelView`.
7. Viewer `engine::RenderFrameMain` (ModelViewer/Source/ModelViewerApplication.cpp, the Phase 1 definition): writes
   the world camera's view-projection, eye, and to-eye normal to the existing fields as today, and
   `mMatModelView * mMatModelPerspective` (transposed) and `mVecModelEyePosition` to the two new fields, read from
   `static_cast<game::Camera*>(engine::gpCamera)`. The four PBR isolation writes stay (D7).
8. Docs (`/update-claude-docs`): Engine/Data/Shaders/AGENTS.md (model draws use the model view-projection and
   eye); the image-pass order owner in Engine/Source/Graphics/Managers/AGENTS.md (`kbDrawModelsOverWorld` order);
   ModelViewer/AGENTS.md `## Engine integration` (the viewer's `CameraBase` state is a fixed top-down world camera;
   the orbit pose is viewer-owned and reaches only model draws; sun drag uses the orbit view's screen axes).

## Critical files

- Engine/Data/Shaders/ShaderMainLayout.h, Engine/Data/Shaders/Model/ModelCommon.h, Model.frag.
- Engine/Source/Graphics/Render/MainUniforms.cpp, Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp.
- Projects/BrokenEngineSandbox/Source/Pch.h, ModelViewer/Source/Pch.h.
- ModelViewer/Source/Graphics/Camera.h, Camera.cpp, ModelViewer/Source/ModelViewerApplication.cpp.

## Risk triggers and invariants

- Tier 3: shared CPU/GPU layout and shaders, sandbox image-pass recording, both programs' `Pch.h`, and the viewer.
- Sandbox: every GPU input bit-identical; command order unchanged under `false`. No CRC, PostRender, or
  determinism change.
- Viewer: model pixels identical to before (A5); its world draws still record zero instances.
- No switch names the application; no switch is defined twice.

## In scope

- ShaderMainLayout.h (two fields, comment); ModelCommon.h:103; Model.frag:229.
- MainUniforms.cpp:608-610 region.
- Both `Pch.h` switch blocks.
- `RecordImageRenderPass` timed pre-pass, opaque-model, new clear, and untimed second pre-pass sites.
- Camera.h/.cpp: new members, `Update`, `Fit`, `RotateSun`.
- ModelViewerApplication.cpp: `engine::RenderFrameMain`.
- The doc lines in Design item 8.

## Out of scope

- Island generation, `UpdateActiveIslands`, `RenderWaterMain` in the viewer, the seed UI, and the gray background
  (stage 3).
- Model.frag:306 and every other shader line; transparent, particle, light, billboard, or debug draw order.
- Removing the PBR isolation writes; camera shake for the model; sandbox camera behavior.

## Acceptance criteria

- A1 `git grep -n kbDrawModelsOverWorld -- '*.h'` lists definitions only in the two `Pch.h` files;
  `git grep -n -E "f4x4ModelViewProjection|f4ModelEyePosition" -- Engine ModelViewer` lists the layout, the two
  shader reads, and the writes in both `RenderFrameMain` definitions.
- A2 `/compile` ModelViewer Debug|x64 and Release|x64 and BrokenEngineSandbox client and server Debug|x64 succeed
  with warnings as errors; Engine/Data changed, so the game build resolves Local data and recompiles the shaders.
- A3 Code read against the session baseline: sandbox writes equal values to old and new fields; under `false` the
  recorded commands are unchanged; under `true` the order is timed UI pre-pass, world draws, depth clear, untimed
  UI pre-pass, opaque models, transparent models, with each `kGpuTimerUiDepth` query written once; debug-texture mode
  records the timed UI pre-pass then the debug texture as today; the orbit arithmetic is unchanged apart from
  member names.
- A4 Sandbox Debug client+server per `/agent-harness`, once before the first edit and once after with the same
  steps: after connect and spawn, `describe_scene` `camera` matches and a client `screenshot` of that pose shows
  the same scene (water, islands, units); no `DEBUG_BREAK at` lines in either client log.
- A5 ModelViewer Debug per `/agent-harness` `references/model-viewer.md` `## Capture` with `-RequireStable`, once
  before the first edit and once after: both runs `ok` and `stable`; a script under `Temp` finds the two final
  PNGs' decoded pixels identical; no `DEBUG_BREAK at` line in either log.
