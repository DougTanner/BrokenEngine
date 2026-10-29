# Lighting foundation strip plan

Baseline: main `aa47e4a3e170a69b7dad971b94003f7ea30e5716`; session tree HEAD `bc69eea5` (foundation WIP
`f22cfa18`, handoff doc `a216544b`, experiment revert `bc69eea5`) plus uncommitted
`Documents/Investigations/Engine/LightingTerrainPreparationMemoryCost.md` and a handoff-doc note.
Worktree `C:\Users\dougt\.claude\worktrees\BrokenEngine\0cb8b7b1-9695-4b87-ab78-8bf06eceacad`.
Landing target: a `lighting-foundation` branch next to main; main and primary stay unchanged.

## Objective

Cut the unbuilt foundation down to what the five prototype investigations need for a possibly
discarded quick prototype, replace the L1-L14 acceptance list with the short table below, and record
the one-bounce indirect reference as the next foundation stage.

Binding user decisions (quoted in the brief): strip over-engineered and not-useful code without
further approval; terrain accuracy Option 2 or looser (ray start offset and emitter-segment shortening
raised from 1 mm to at least 1 cm, shared by reference and candidates; traced terrain agrees with drawn
terrain only within that offset; no raw-tap equality, no four shifted re-renders, no exhaustive
equivalence proofs); one-bounce reference is a second stage; the RAM placement policy in
`Engine/Source/Graphics/Objects/Buffer.cpp` stays; the memory-cost experiment stays reverted.

Main's decisions (user waived approval of strip decisions): ray offset 0.05 m (5 cm) — every
"0.01 m" / "1 cm" offset value below means 0.05 m / 5 cm; capture uses the progressive design under
client pause; row 3's baseline comes from the unedited tree, which built cleanly (envelope
`Temp/AgentBuildEnvelopes/compile-20261029T115329564Z-21120.md`), and is recorded in
`Temp/lighting-strip-baseline/` (replay steps `Temp/lighting-strip-baseline/temp-replay-steps.md`).
No terrain agreement probe is added; terrain is traced only where it is drawn (off-screen terrain is
not traced, off-screen models still are); terrain GPU time is an ordinary ProfileManager GPU timer;
capture results live only in `capture.json`.

## What the candidates need (derived)

Read from the five pre-foundation docs (`git show aa47e4a3:Documents/Investigations/Engine/LightingPrototype*.md`,
`## Implementation prerequisite`, `## Comparison fixture`, `## In scope`, `## Verification`) and the
current versions:

| Need | Consumers | Foundation mechanism kept |
| --- | --- | --- |
| Legacy projected local light removed, gameplay local diffuse exactly zero | all five (`## Critical files` of each) | legacy removal (already done), `LightingCompose.h` control-gated read |
| Vulkan ray-query backend with BLAS/TLAS | all five (`## Implementation prerequisite`) | `DeviceManager`/`InstanceManager` capability enablement, `LightingAccelerationStructure.*` |
| Resident terrain + rigid/skinned models incl. moving, animated, off-screen | all five | `Terrain.cpp` producer (visible pass only; off-screen terrain is not traced and the candidate docs say so), `LightingModels.*`, trace-only `AddModels` ranges in `Projects/BrokenEngineSandbox/Source/Frame/Collections/*/…Render.cpp` |
| Hit material/albedo | DDGI, Split Radiance Cascades (bounce shading) | `LightingClosestHit` in `Engine/Data/Shaders/Lighting/LightingQuery.h:48` |
| Fixed fixture (seed 0x51A7, 1000 nested sources, panels/walls/overhang, events, moving source/occluder/skinned mesh, cell crossing) | all five (`## Comparison fixture`) | `LightingFixture.*` generation (no blobs/hashes) |
| Exhaustive direct-diffuse reference, raw float + PNG export at 1920x1080 | all five (`## In scope` bullet 4) | `LightingReference.comp`, receivers, export (progressive capture) |
| Candidate composition seam added once to model/terrain/water | all five | `LightingCompose.h` + `TextureDescriptors::SetLightingReference` |
| One-bounce indirect reference | DDGI (`## In scope` bullet 4), Split Radiance Cascades (bullet 4) | not in this stage; recorded as stage 2 |

## Keep / simplify / strip table

| Component | Decision | Files / regions | Reason |
| --- | --- | --- | --- |
| Legacy local-light removal (deposit/spread/combine/history/blur, settings v20, UI) | keep | all legacy deletions in `git diff aa47e4a3 HEAD` | user request: old system replaced, stubs only |
| Ray-query capability, BDA, AS objects | keep | `Engine/Source/Graphics/Managers/{DeviceManager,InstanceManager}.cpp` non-TEMP lines, `Engine/Source/Graphics/Lighting/LightingAccelerationStructure.*`, `DataPacker/Source/ExportJobs/ExportShader.cpp:292` | every candidate traces |
| RAM placement / OOM fallback | keep | `Engine/Source/Graphics/Objects/Buffer.cpp` non-TEMP lines | user-selected policy |
| Preparation command buffer before Main | keep | `CommandBufferManager.cpp:131-148,197-206` minus capture hook | AS builds and producers must precede Main |
| Model producer | simplify | `LightingModels.{h,cpp}`: drop `LightingTerrainOperations&` parameter and `++rOperations.*` lines (406-471), `LightingModelBytes`, `GetResourceBytes*` (474-518); add `uiTraceOnlyInstances` to `LightingModelCounts` (`Engine/Source/Graphics/Lighting/LightingModels.h:23-29`), incremented in `Prepare` after the empty-primitives skip (`Engine/Source/Graphics/Lighting/LightingModels.cpp:313-318`) for every range with `iFirstInstance > 0` | accounting is L14 cost proof; the trace-only count is row 4's off-screen observation (see `### Trace-only count` below) |
| Terrain producer | simplify to the visible pass | `Terrain.cpp` deletes: `Placement` 25-34; `Framebuffer` 40, 44-57, 60-70, 72-78; `State` 82, 84-85, 87; `TerrainValidationBuffer` 92-102; timestamp and R16 raster checks 117-127; scratch and witness creation 132-145, 152-165; raster/validate/witness pipelines 180-195, 210-231, 236, 238-239; `TerrainTileBound` 242-251; `Prepare` 279-283, 286-288, 290, 295, 297-300, 305-312, 337-340, 358-359, 362-414, 417-418, 425-427, 457-507; `Write` 514-515, 522-546; `RecordPreparation` 587-588, 591-608, 611, 620, 624-679, 681-729, 731, 737, 740-768; and 770-891. Changed lines: `Prepare` signature (273) becomes `Prepare(int64_t iFramebuffer)`; 301-304 loop only over `vertexPipeline` (the binding-5 work-buffer refresh stays every frame); 521 builds the four-field work; 609 becomes a direct `vkCmdPipelineBarrier` with the same stages and access (the `Barrier` lambda goes); the `Dispatch` lambda (613, 618) drops the origin and pushes `{0, 0, placement, base}`; 680 is the one dispatch run per placement. Every other line, including signatures, braces, 589-590 and 769, stays. Kept members: `Placement` `uiPlacement`, `uiVertexCount`, `uiPrimitiveCount`, `vkXY`, `vkIndices`, `vkXYZOffset`, `acceleration`; `Framebuffer` `vertices`, `work`, `materials`, `placements`, `counts`; `State` `framebuffers`, `vertexPipeline`, `vkMaxAllocationSize`, `GetGeometry` | Option 2 drops the equivalence gate; the off-screen scratch-tile pass is dropped (vertices outside the visible elevation texture take `fSeaFloorElevation`), so off-screen terrain is not traced; the binding-5 refresh stays because the work buffer can grow every frame |
| Terrain production GPU time | replace with a ProfileManager GPU timer | `Engine/Source/Profile/ProfileManager.h:175` area: add `kGpuTimerLightingTerrain` after `kGpuTimerWaterDisplacement` and `"    Lighting Terrain"` at the same position in `kpGpuTimerNames`; `Lighting::RecordPreparation` wraps `mpState->terrain.RecordPreparation(...)` in `GpuStart`/`GpuStop` of that timer; Terrain's own query pool, durations and `terrainMs` are deleted | `Lighting::RecordPreparation` already resets `kGpuTimerMain..kGpuTimerUiRender` (`Engine/Source/Graphics/Lighting/Lighting.cpp:430`) and `Engine/Source/Graphics/Graphics.cpp:193` reads every row; an unwritten row is skipped as not ready (`Engine/Source/Profile/ProfileManager.cpp:378`); `query_profile` `gpuTimers` reports it |
| Terrain shader producer | simplify | `LightingTerrain.comp`: delete 10 and 18 (scratch binding 4); replace TEMP arithmetic 31-50 with the plain `TerrainVertex.h:101-102,122-123,144-147` expressions; delete 55-57 (tile origin), 59-61, 77-79, 83-153; replace 62-75 with `fZ = bCovered ? textureLod(visibleElevation, f2UV, 0.0f).r : globalLayout.fSeaFloorElevation` | see Finding F1; one dispatch run per placement, no tile ownership |
| Terrain validation and tile shaders | strip | delete `Engine/Data/Shaders/Lighting/{LightingTerrainValidate.comp,LightingTerrainValidation.h,LightingTerrainWitness.vert,LightingTerrainWitness.frag,LightingTerrainTile.vert}`; `TerrainVertex.h:9-13,55-61,82(#endif),93-99,100/103,110-121,124,137-143,148,157-161,173-257` validate variant | Option 2: no raw-tap/shift/witness proof; no off-screen tile raster |
| Terrain CPU/GPU contract | simplify | `LightingTerrainCommon.h`: delete tile constants 9-11 (keep `kiLightingTerrainGroup`); `LightingTerrainWork` keeps `uiXYAddress,uiXYZAddress,uiVertexCount,uiPadding` only; delete `LightingTerrainOriginal`, evidence structs, `LightingTerrainTileResult`, `LightingTerrainValidation`, `LightingTerrainValidatePush` and their asserts (20-40, 43-162, 179-190); 174-176 become one size-24 assert on `LightingTerrainWork`; keep `LightingTerrainMaterial` and the 173/191-192 guards | fields served only validation, evidence and the tile pass |
| `PipelineFlags::kPointList` | strip | `Engine/Source/Graphics/Objects/Pipeline.h:94`; `Engine/Source/Graphics/Objects/PipelineCreator.cpp:562` back to baseline (`git show aa47e4a3:Engine/Source/Graphics/Objects/PipelineCreator.cpp`, line 542); its only use, the witness pipeline (`Terrain.cpp:218`), goes with slice 1 | witness-only |
| Ray offset | change | `LightingCommon.h:21` `kfLightingRayOffset = 0.05f` | Option 2; shortening at `LightingRadiometry.h:18,38` and origin at `LightingQuery.h:126` already use the constant |
| Terrain agreement probe | not added | — | row 5 inspects the wide 1000-source `finished.png` for terrain self-shadow speckle instead |
| Scene diagnostics (source/sample validation, 8 probes) | strip | delete `LightingSceneDiagnostics.cpp`, `Engine/Data/Shaders/Lighting/LightingSceneDiagnostics.{comp,h}`; `Lighting.cpp:198,200,211,224,233,470`; `LightingState.h:5,32-38` | fixture sources are generated deterministically; counters had no candidate consumer |
| Fixture generation | keep | `LightingFixture.cpp:48-59,83-129,233-269,296-424` | fixture spec in every candidate doc |
| Fixture blobs and SHA-256 identities | strip | `LightingFixture.cpp:10-46,61-81,130-230,270-292,426-461`; `LightingFixture.h:10-18,38-42,67,82-84,94,102-103` | candidates branch from one commit; generation code is the identity |
| Capture: blocking held-publication capture with window/agent/ENet pump, cancel, resume, epochs | rewrite (progressive, under client pause) | `LightingCapture.cpp:44-97,99-119,209-432,434-579`; `LightingComparison.cpp:37-47,84-132`; `Lighting.cpp:236-306` (`BeginRender` keeps 238-239 and 246, 248 plus the latch in `## Capture design`; `CompletePreparation`, `AddFrame`, `HoldPublication`, `ReleasePublication` deleted), `Lighting.cpp:521`; `CommandBufferManager.{h,cpp}` capture hook; `AgentCommandServer.cpp:301-305,342-356`; `Client.{h,cpp}` `ServiceCaptureTransport`; `Main.cpp:650-662`; `Projects/ModelViewer/Source/Main.cpp:11-18`; `GameBase.cpp:942-956,983-985` | rewrite removes a cross-subsystem re-entrant pump and ~600 lines; see `## Capture design` |
| Capture manifest, buffer/texture/descriptor identity hashing, pack/executable hashing, sampler census, operation counts, byte accounting | strip | `LightingCapture.cpp:12-42,121-207,581-603(replace),605-862`; `Lighting.cpp:27-42,155,189-193,202-203,398-411` and the operation counters 437-438, 452, 456, 461, 465, 467; in `Lighting.cpp:412-423` keep the reset, the publication serial (413) and the source/instance/triangle values (419-421) and add the trace-only value — the other assignments go with their deleted `LightingDiagnostics` fields; `TextureCache.{h,cpp}` `pReadbackBytes`; `Screenshot.cpp` `SaveFinishedPng` readback fields; `PackChunks.h` `GetDataFilePath` back to private | reproducibility needs only the commit and a small `capture.json` |
| Frame identity rows | strip | `Lighting.cpp:279-284`; `MainUniforms.cpp:301`; `LightingState.h:53` | manifest-only, allocates every frame |
| RenderDoc native-capture TEMP machinery | strip | `LightingState.h:115-169`; `LightingCapture.cpp:106-108,211-225,270-285,310-339,630-640`; `LightingComparison.cpp:86-88,141-143,170-205` | diagnosis-only |
| All other TEMP blocks | strip, restoring baseline text | see Finding F2 and slice 4 | diagnosis-only (`.agents/references/change-workflow.md:177-184`) |
| Per-framebuffer duplication of lighting resources | keep | `LightingState.h:23-54`, `Terrain.cpp:38-79` (kept members only) | frames in flight; rewriting buys nothing for a prototype |
| L10 lifecycle matrix, L13 historical P0 replay, L14 cost accounting, 512-sample convergence gate | drop from acceptance | handoff doc `## L1–L14` | replaced by `## Acceptance table` |

### Trace-only count

Every visible range starts at instance 0 and every trace-only range is the capacity tail
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsRender.cpp:225-226`,
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/FightersRender.cpp:227-228`,
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesRender.cpp:142-143`), so a
range with `iFirstInstance > 0` is trace-only. A tail that fills its whole buffer starts at 0 and is
not counted; that undercount can only fail row 4, never pass it. No Frame-path file changes (row 9).

## Capture design

The capture runs inside ordinary frames while the client is paused; nothing blocks the main thread.

- `GameBase::Render` passes `mGameFlags & GameFlags::kPaused` to
  `Lighting::ApplyComparisonSettings(bool bScenePaused)` (`GameBase.cpp:958`). Client pause (Space,
  `Engine/Source/Input/Input.cpp:66`) zeroes the tick remainder (`GameBase.cpp:288-292`) and freezes
  the render clock (`GameBase.cpp:630-634`), so every paused frame republishes identical models,
  terrain, water time and fixture state.
- Capture state (request, stage/status, error, completed and total tiles, directory, start time, and
  the receivers-frame publication) lives in `Lighting::State`, not in `State::Comparison`, which holds
  only GPU resources. `DestroyComparisonResources` (pipeline and swapchain recreation) and
  `DestroyResources` set an in-progress capture to `failed` with reason "Graphics recreated during
  capture" and never reset status; `Lighting.cpp:216`'s reset of `completedDiagnostics` is removed, so
  recreation cannot turn `failed` back into `not-run`.
- A new request (`uiCaptureRequest` changed) in `kFixtureReference` mode starts a capture only when
  paused and the framebuffer extent is 1920x1080; otherwise status `failed` with the reason.
- Stages: `receivers` (EndRender creates the receiver model pipelines for that frame's ranges and
  records that frame's written `shaders::LightingPublication` — source, instance, triangle and
  material counts; the preparation command buffer, after the TLAS query barrier, transitions the
  reference image to general if needed and records the receiver render pass), `tracing` (each later
  preparation dispatches 120 reference tiles of 4x4 pixels — a quarter tile-row, 1080 frames per
  capture; each tracing frame's EndRender compares its written publication with the recorded one and,
  on any difference, sets `failed` with reason "published scene changed during capture" — the
  publication serial advances every frame, so the counts are the comparison), and `composing` (the
  frame that dispatches the last tiles adds a compute-to-fragment barrier, moves the reference to
  shader-read, and calls `SetLightingReference(iFramebuffer, &reference, true)`; Main composes it).
- `FinishCapture(iFramebuffer)` on the composing framebuffer, after the UI submit: `SaveFinishedPng`
  (waits that fence), reads back the four receivers and the reference, writes
  `position.rgba32f`, `shading-normal.rgba32f`, `geometric-normal.rgba32f`, `albedo.rgba32f`,
  `direct.rgba32f`, `finished.png` and `capture.json` under
  `<temp>/Lighting/capture-<request>-<start ticks>/`, resets the reference descriptor, and sets status
  `complete`.
- Failure transitions: unpause, settings change, a changed publication, or Graphics recreation during
  a capture set `failed` with a reason; the composing frame is the only frame that ever enables the
  reference.
- `capture.json` carries every capture result: request, sourceCount, samples, event, camera, scenario,
  width, height, wallMs, device (physical device name).
- Completed-after-fence latch (replaces `CompletePreparation`): `EndRender` stores the framebuffer's
  publication serial (`++mpState->uiPublication`) and its pending sources, instances, trace-only
  instances and triangles; `RecordPreparation` stores the recorded serial (`Lighting.cpp:472`);
  `BeginRender(iFramebuffer)`, which runs after that framebuffer's fence wait, copies the pending
  values into `State`'s `LightingDiagnostics` when the recorded serial is non-zero and equals the
  pending serial.
- `query_profile.lighting` becomes `{sources, instances, traceOnlyInstances, triangles,
  capture:{status, completedTiles, totalTiles, error, directory}}`; status is one of
  `not-run|receivers|tracing|composing|complete|failed`. Terrain GPU time is the `gpuTimers` row
  `Lighting Terrain`.
- Reference compute pipeline and receiver textures/render pass/framebuffer are created once, at the
  first capture, and rebound per frame (`BindQueryDescriptors`); the per-frame rebuild at
  `LightingComparison.cpp:297-325` is removed (Finding F5).
- The 512-sample Halton set and control stay; no 512 convergence export is required (see
  `## Acceptance table` note).

## Public interface after the strip (fixed for parallel slices)

`Engine/Source/Graphics/Lighting/Lighting.h`:

- `LightingComparisonSettings` loses `uiValidationRequest`; other fields unchanged.
- `LightingAllocation` is deleted. `LightingDiagnostics` keeps its name and becomes:
  `int64_t iSources, iInstances, iTraceOnlyInstances, iTriangles; std::string_view status = "not-run";
  int64_t iCompletedTiles, iTotalTiles; std::string error; std::filesystem::path directory`.
- `struct Lighting` public members: `Lighting(); ~Lighting(); BeginRender(int64_t);
  AddModels(crc, crc, int64_t, int64_t); EndRender(); RecordPreparation(VkCommandBuffer);
  SetComparison(const LightingComparisonSettings&); mRequestedComparisonSettings;
  ApplyComparisonSettings(bool bScenePaused); FinishCapture(int64_t);
  WriteProfile(nlohmann::json&) const; RecordFixtureDraws(VkCommandBuffer, int64_t);
  GetComparisonCamera(LightingCameraPose&) const; GetDiagnostics() const`.
- Deleted: free `DrainLightingCaptureCommands`, `AddFrame`, `CapturePreparation`, `IsCaptureHeld`,
  `CancelCapture`, private `CompletePreparation`.

`Engine/Source/Graphics/Lighting/Terrain.h`: `LightingTerrainCounts{iPlacements, iVertices,
iTriangles}`; `Prepare(int64_t iFramebuffer)`; `Write(...)` and `RecordPreparation(...)` unchanged.
`LightingTerrainOperations`, `LightingTerrainCompletion`, `LightingTerrainBytes`,
`LightingTerrainPlacementIdentity`, `Complete`, `GetResourceBytes*`, `WritePlacementIdentities` are
deleted; no terrain timing function exists.

`Engine/Source/Graphics/Lighting/LightingModels.h`:
`RecordPreparation(VkCommandBuffer, int64_t iFramebuffer, std::span<const LightingModelRange>)`;
`LightingModelCounts` gains `uint32_t uiTraceOnlyInstances`; `LightingModelBytes` and
`GetResourceBytes*` deleted.

`Engine/Source/Profile/ProfileManager.h`: `GpuTimers` gains `kGpuTimerLightingTerrain` directly after
`kGpuTimerWaterDisplacement`; `kpGpuTimerNames` gains `"    Lighting Terrain"` at the same index.

`Engine/Source/Graphics/Objects/Pipeline.h`: `PipelineFlags::kPointList` deleted.

`Engine/Source/Graphics/Screenshot.h`: `void SaveFinishedPng(int64_t iFramebufferIndex, const std::filesystem::path& rPath)`.

## Findings (broken or missing, not just over-built)

- F1: `Engine/Data/Shaders/Lighting/LightingTerrain.comp:31-50` — the production world-XY and UV
  arithmetic lives inside a `TEMP START original operand acquisition` block; deleting the block as
  diagnostic code deletes production. Slice 1 writes the plain expressions in place.
- F2: TEMP blocks own production calls: `Engine/Source/Graphics/Objects/Texture.cpp:225-240`
  (`vmaCreateImage` + `CHECK_VK`), `Engine/Source/Graphics/Managers/ImGuiManager.cpp:317-329`
  (`vmaCreateBuffer` + `CHECK_VK`), `Engine/Source/Graphics/Managers/TextureUploadManager.cpp:474-489`
  (`vmaCreateImage` + `CHECK_VK`), and `Engine/Source/Graphics/Managers/DeviceManager.cpp:380-384`
  replaces VMA's `vkAllocateMemory` with a logging hook. Removal restores the baseline lines from
  `git show aa47e4a3:<file>`.
- F3: `LightingClosestHit` (`LightingQuery.h:48-122`, hit material/albedo) loses its only consumer,
  the scene-diagnostic probes (`LightingSceneDiagnostics.comp:60`); its model and terrain material
  paths have never been verified. Stage 2 (one-bounce reference) is its first consumer and verifier.
- F4: the blocking capture re-enters window dispatch, agent command drain and ENet service from
  inside `RenderMainPresentAcquire` (`LightingCapture.cpp:44-66,68-81`, `Main.cpp:650-662`,
  `AgentCommandServer.cpp:342-356`) — a re-entrancy surface the progressive design deletes.
- F5: `LightingComparison.cpp:297-325` recreates every receiver model pipeline and the reference
  compute pipeline on every `EndRender` while a capture request is pending.
- F6: no fixture geometry can be off-screen at either fixed camera (all 13 solids and the skinned
  mesh lie inside the 512 m square, `LightingFixture.cpp:99-114`, `LightingFixture.h:50`), so the
  off-screen-occluder requirement is observed through trace-only model publication
  (`BattleshipsRender.cpp:225-226`) and the new `traceOnlyInstances` count. Off-screen terrain is not
  traced at all after the strip; the handoff and the five candidate docs record that.
- F7: with the off-screen tile rasters gone, the terrain producer measured in
  `LightingTerrainPreparationMemoryCost.md` `## Observation` no longer exists in that form; the
  question may be moot. Slice 6 keeps the record and states the producer change.

## In scope

- Slice 1 regions: `Engine/Data/Shaders/Lighting/LightingTerrain.comp` (whole file reduced to the
  visible-pass producer), `Engine/Data/Shaders/Lighting/LightingTerrainCommon.h` (constants, structs
  and asserts listed above), `Engine/Data/Shaders/Terrain/TerrainVertex.h` (validate variant),
  deletion of `LightingTerrainValidate.comp`, `LightingTerrainValidation.h`,
  `LightingTerrainWitness.vert`, `LightingTerrainWitness.frag`, `LightingTerrainTile.vert`;
  `Engine/Source/Graphics/Lighting/Terrain.{h,cpp}` (validation, witness, scratch-tile pass, query
  pool, completion, bytes, identities); `Engine/Source/Graphics/Lighting/LightingModels.{h,cpp}`
  (operations parameter, bytes, trace-only count).
- Slice 2 regions: `Engine/Source/Graphics/Lighting/{Lighting.h,Lighting.cpp,LightingState.h,
  LightingCapture.cpp,LightingComparison.cpp,LightingFixture.h,LightingFixture.cpp}` (strip,
  completed-after-fence latch, terrain GPU timer call, progressive capture), deletion of
  `Engine/Source/Graphics/Lighting/LightingSceneDiagnostics.cpp` and
  `Engine/Data/Shaders/Lighting/LightingSceneDiagnostics.{comp,h}`;
  `Engine/Data/Shaders/Lighting/LightingCommon.h:21`; `Engine/Source/Profile/ProfileManager.h`
  (`kGpuTimerLightingTerrain` and its name).
- Slice 3 regions: host reverts and callers — `Engine/Source/Graphics/Managers/CommandBufferManager.{h,cpp}`
  (capture hook and `bPreparationSubmitted`), `Engine/Source/Agent/AgentCommandServer.cpp:301-305,342-356`
  (back to baseline), `Engine/Source/Network/Client/Client.{h,cpp}` (`ServiceCaptureTransport`),
  `Engine/Source/Main.cpp:650-662`, `Projects/ModelViewer/Source/Main.cpp:11-18`,
  `Engine/Source/GameBase.cpp:942-958,983-985`, `Engine/Source/Graphics/Render/MainUniforms.cpp:301`,
  `Engine/Source/Graphics/Screenshot.{h,cpp}` (`SaveFinishedPng`),
  `Engine/Source/Graphics/Managers/TextureCache.{h,cpp}` (back to baseline),
  `Engine/Source/File/PackChunks.h` (back to baseline),
  `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp` (remove Validate button and
  terrain text; show status and tiles from `GetDiagnostics()`).
- Slice 4 regions: TEMP removal with baseline restoration in `Engine/Source/Graphics/Objects/{Buffer,
  Pipeline,PipelineCreator,PipelineDescriptorWriter,Texture}.cpp` and
  `Engine/Source/Graphics/Managers/{DeviceManager,ImGuiManager,TextureUploadManager,
  TextureDescriptors,SwapchainManager}.cpp`; `PipelineFlags::kPointList` removal in
  `Engine/Source/Graphics/Objects/Pipeline.h:94` and `Engine/Source/Graphics/Objects/PipelineCreator.cpp:562`.
- Slice 5 regions: project membership for the eight deleted files in
  `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj{,.filters}` and
  `Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj{,.filters}`.
- Slice 6 regions (documents): `Engine/Source/Graphics/AGENTS.md` `## Lighting Publication` and
  `## Assets and Capture`; `Engine/Source/Graphics/Managers/AGENTS.md` CommandBufferManager capture
  bullet; `Engine/Source/Graphics/Objects/AGENTS.md` "diagnostic work" wording;
  `Engine/Source/Agent/AGENTS.md` capture-hold bullet; `Engine/Source/Network/Client/AGENTS.md`
  `ServiceCaptureTransport` paragraph; `Engine/Data/Shaders/Lighting/AGENTS.md` terrain, scratch-tile
  and witness bullets; `Engine/Data/Shaders/Terrain/AGENTS.md` "raster-equivalence gate" sentence;
  `Projects/ModelViewer/AGENTS.md` `DrainLightingCaptureCommands` sentence;
  `Documents/Architecture/FrameUpdatePipeline.md` `## Client Rendering and Comparison Capture`;
  `.agents/skills/agent-harness/references/command-reference.md` `query_profile` lighting text;
  `Documents/Investigations/Engine/LightingFoundationHandoff.md` (rewrite to current state, new
  acceptance, stage 2, and the off-screen terrain omission);
  `Documents/Investigations/Engine/LightingTerrainPreparationMemoryCost.md` `## Method` (read the
  `query_profile` `gpuTimers` row `Lighting Terrain` instead of `productionMs`, and state that the
  producer changed: no off-screen tile rasters, no validation writes); the five
  `Documents/Investigations/Engine/LightingPrototype*.md` lines naming blobs/hashes, the capture
  manifest, verified 256/512 convergence evidence, (DDGI, Split Radiance Cascades) the in-candidate
  one-bounce reference, and the prerequisite text on off-screen occluders (off-screen models are
  traced, off-screen terrain is not), mirrored across the five.

## Out of scope

- The one-bounce indirect reference implementation (stage 2) and any candidate algorithm.
- The Buffer.cpp RAM placement/OOM policy, the reverted memory-cost experiment, and the open question
  in `LightingTerrainPreparationMemoryCost.md` beyond its `## Method` timing source and producer note.
- Tracing off-screen terrain, rewriting the model deformation producer, per-framebuffer resource
  ownership, or AS allocation.
- Legacy-removal content, `GraphicsSettings.bin` version 20, ModelViewer behavior other than the
  deleted empty hook, `Common/TextureFormat.cpp`, `DataPacker` changes.
- Exact raster equivalence, shifted re-renders, witness draws, terrain agreement probes, lifecycle
  matrix (L10), historical P0 replay (L13), cost/manifest accounting (L14), 512-sample convergence
  exports, 100- and 300-source reference exports (candidates generate their own).
- New harness commands, unit tests, backward compatibility, pack format changes, any Frame/simulation
  file (including the `Projects/BrokenEngineSandbox/Source/Frame/Collections/*/…Render.cpp` callers),
  landing onto primary.

## Slices

Run slices 1-4 in parallel (disjoint files; the interfaces above are fixed), then 5, then 6.

| Slice | Owner files | Done when |
| --- | --- | --- |
| 1 Terrain/model producers | `Engine/Data/Shaders/Lighting/{LightingTerrain.comp,LightingTerrainCommon.h}`, `Engine/Data/Shaders/Terrain/TerrainVertex.h`, 5 deleted shader files (`LightingTerrainValidate.comp`, `LightingTerrainValidation.h`, `LightingTerrainWitness.vert`, `LightingTerrainWitness.frag`, `LightingTerrainTile.vert`), `Engine/Source/Graphics/Lighting/{Terrain.h,Terrain.cpp,LightingModels.h,LightingModels.cpp}` | interface matches `## Public interface`; no validation, evidence, scratch, tile or query-pool symbol remains in these files |
| 2 Lighting core and progressive capture | `Engine/Source/Graphics/Lighting/{Lighting.h,Lighting.cpp,LightingState.h,LightingCapture.cpp,LightingComparison.cpp,LightingFixture.h,LightingFixture.cpp}`, deleted `LightingSceneDiagnostics.cpp`, deleted `Engine/Data/Shaders/Lighting/LightingSceneDiagnostics.{comp,h}`, `Engine/Data/Shaders/Lighting/LightingCommon.h`, `Engine/Source/Profile/ProfileManager.h` | `## Capture design` implemented; no TEMP, hash, manifest, byte or hold code |
| 3 Host reverts and callers | files listed in `## In scope` slice 3 | each listed hunk equals baseline text or the fixed interface call |
| 4 TEMP strip in shared graphics objects | files listed in `## In scope` slice 4, including `Engine/Source/Graphics/Objects/Pipeline.h` | no TEMP marker and no `kPointList`; each block's production lines equal `aa47e4a3` text, except Buffer.cpp keeps its non-TEMP policy |
| 5 Project membership | four project/filter files | `/update-vcxproj` validation passes for Sandbox client/server and ModelViewer |
| 6 Documents | files listed in `## In scope` slice 6 | no document describes a deleted mechanism; handoff carries the acceptance table, the off-screen terrain omission and stage 2 |

## Acceptance table

Scenario for rows 3-9 (one `/agent-harness` session): Sandbox client + server Debug x64 launched with
each process's `--log-file` (the harness launch doc's paths) and the row-3 app-data setup from
`Temp/lighting-strip-baseline/temp-replay-steps.md` `## After-run steps`; `window_state
{"minimized":false}` restores the agent client, then `resize` 1920x1080 must return
`{"width":1920,"height":1080}` (otherwise stop: rows 4-7 cannot run); server `load` of
`lighting-strip-baseline-t400` with `pauseAfterLoad`; after row 3, client pause with `key` Space when
`describe_scene` `gameFlags` lacks it; Tweaks `Lighting Comparison` tab controls; Capture button; poll
`query_profile` until `lighting.capture.status` is `failed`, or `complete` with a `directory` different
from the previous capture's.

| # | Criterion | Check and expected observation |
| --- | --- | --- |
| 1 | Builds | `/compile`: DataPacker Release x64 with Local shader data, BrokenEngineSandbox client and server Debug x64, ModelViewer Debug x64 all succeed. |
| 2 | Diagnostics and legacy gone | `rg -n "TEMP START\|TEMP END\|DESCRIPTOR_CENSUS\|DESCRIPTOR_OWNER\|MEMORY CREATE_\|LightingTerrainValidat\|LightingTerrainWitness\|LightingTerrainTile\|kiLightingTerrainScratch\|kPointList\|LightingSceneDiagnostic\|NativeCapture\|ServiceCaptureTransport\|DrainLightingCaptureCommands\|HoldPublication\|uiValidationRequest\|GetInputHash" Engine Projects/*/Source` and `rg -n "LightingSpread\|LightCombine\|LightingBlur\|LightingDeposit\|LightingWrappersBase\|CurveWidget\|HexShieldLighting\.frag" Engine Projects/*/Source` return nothing (the unedited tree fails the first search). |
| 3 | Gameplay zero diffuse, ordinary visuals preserved | Comparison disabled, client and server paused on the same save: follow `Temp/lighting-strip-baseline/temp-replay-steps.md` `## After-run steps` against the stripped build (same save, `ClientGuid.bin`, fresh client app data, close shot at client start + 60 s at eye height 156, wide shot at + 75 s at eye height 665.117), then compare each shot with `Temp/lighting-strip-baseline/baseline-close.png` and `Temp/lighting-strip-baseline/baseline-wide.png` using `Temp/lighting-strip-baseline/Compare.ps1`. Pass: `over8` is at most 2% of pixels for each shot, and side-by-side inspection shows the same islands, water, units, shadows and UI with no added or missing local glow. If `over8` exceeds 2%, retake the timed pair once; if it still exceeds 2%, row 3 is settled by the side-by-side inspection alone and the measured fractions are reported as a residual (the sun angle advances with client wall time, `temp-replay-steps.md` `## Sun angle caveat`). |
| 4 | Ray visibility on the fixture | Isolated point scenario, close camera, 256 samples, events 0 and 119: in `direct.rgba32f`, floor pixels inside the moving panel's geometric shadow are exactly 0 and nearby unshadowed floor pixels are > 0; the moving occluder's zero region moves between the two events. Skinned mesh: density 100, close camera, events 0 and 119; compare `direct.r` only at floor pixels (`position.z` within 0.01 m of 0, geometric-normal z > 0.99) within 60 m horizontally of the mesh anchor (-160, 160) (`Engine/Source/Graphics/Lighting/LightingFixture.h:50`). Each such pixel is at least 201 m from source 0's position (-205.3, -97.5), the first draw of seed 0x51A7 (`Engine/Source/Graphics/Lighting/LightingFixture.cpp:89-93`), and at least 179 m from the moving panel and blocker that follow source 0 within 22 m (`Engine/Source/Graphics/Lighting/LightingFixture.cpp:342-356`), beyond the 150 m source range (`Engine/Data/Shaders/Lighting/LightingCommon.h:23`), so the mesh animation is the only fixture change that reaches them; at least 10 such pixels differ by more than 1% between the events. Off-screen: fixture mode active with at least one opaque unit in a rendered cell outside the comparison camera's view (for example pin the client with `set_client_grid_coord` to a cell next to [0,0] before enabling the fixture, with [0,0]'s units still rendered): `query_profile.lighting.traceOnlyInstances` > 0. |
| 5 | Terrain agrees within the 5 cm offset | Inspect the row-7 wide 1000-source capture's `finished.png`: island terrain lit by local sources shows no self-shadow speckle (isolated dark pixels or dark streaks inside lit terrain). An island must be in view; if none is, `set_client_grid_coord` to a cell with islands before enabling the fixture and recapture. |
| 6 | Radiometry sanity | Isolated point, close camera, event 0: for 3 unshadowed floor pixels 10-100 m from (-200,-180), a PowerShell read of `position`, `shading-normal` and `direct` raws gives `direct.r` within 0.1% of the hand value `(0.5/pi) * (1000/(4*pi)) * cos(theta) / d^2` computed in double from source (-200,-180,6). |
| 7 | Reference exports | Density scenario, wide camera, 1000 sources, event 0, 256 samples reaches `complete` within 15 minutes wall; it and every capture rows 4 and 6 take each produce five 33,177,600-byte `.rgba32f` files, a 1920x1080 `finished.png` with visible local diffuse, and a `capture.json` carrying the keys in `## Capture design`. |
| 8 | No Vulkan validation errors | Search the client's `--log-file` over the whole scenario (restore, resize, mode switches, all captures) for `DebugUtilsCallback`, `CheckVk failed`, `VK_ERROR_DEVICE_LOST` and `DEBUG_BREAK at`: no match. |
| 9 | Simulation/CRC unaffected | `git diff bc69eea5 -- Engine/Source/Frame Projects/BrokenEngineSandbox/Source/Frame` is empty, and the client's and server's `--log-file`s over the scenario contain no `SendDesyncReport`, `RecoverFromDesync`, `LogDifferences CRC` or `DEBUG_BREAK at` line. |

512-sample convergence pair: not required. With 4x4 m emitters at least 2 m from every receiver
plane, 256 Halton samples leave integration error far below the visual differences the comparison
judges, and the pair doubles the most expensive capture; the 512 control stays for a candidate that
suspects area-sampling noise.

## Execution card

### What does this plan do?
It deletes the diagnostic and proof machinery from the unbuilt lighting foundation (terrain
validation shaders, the off-screen terrain tile pass, scene diagnostics, manifests, hashes, byte and
operation accounting, about 95 TEMP blocks), replaces the blocking held-publication capture and its
window/agent/network pump with a capture that advances inside ordinary frames while the client is
paused, raises the shared ray offset to 5 cm, and times terrain preparation with an ordinary GPU
profile timer. It also restores production lines hidden inside TEMP blocks, moves the documents to
the new state and records the one-bounce reference as stage 2.
### Why this is good for the codebase
The foundation exists so five possibly discarded lighting prototypes can branch from one small,
buildable base; L1-L14 and its exact-equivalence gates kept it unfinished. Candidate implementers get
the ray-query scene, fixture, reference and composition seam without a re-entrant capture path across
Agent, Network and Main, and a nine-row acceptance table they can run with existing tools.
- Goal: a buildable stripped foundation passing the nine-row acceptance table, with the handoff doc rewritten around it.
- Out of scope: stage 2 one-bounce reference, candidate algorithms, off-screen terrain tracing, Buffer.cpp memory policy, producer rewrites, legacy-removal content, new harness commands, unit tests, Frame/simulation files.
- Tier trigger: Tier 3 — spans independently owned subsystems (Graphics, Agent `AgentCommandServer.cpp:301-356`, Network/Client `Client.cpp:16-20`, Main loop `Main.cpp:650-662`, `GameBase.cpp:942-958`, Profile `ProfileManager.h`, ModelViewer) and changes CPU/GPU data layout (`LightingTerrainCommon.h:14-41`).
- Interfaces and invariants: public interfaces fixed in `## Public interface after the strip`;
  preparation submits before Main every frame with unchanged semaphore chain; descriptor writes only
  to the current or fence-waited framebuffer (the terrain work-buffer binding is refreshed every
  frame for the current framebuffer); diagnostics latch only after the covering fence; capture
  status survives Graphics recreation; gameplay local diffuse exactly zero; no Frame/CRC/RNG/pack
  change; RAM placement policy unchanged; ray offset 0.05 m shared by reference and candidates.
- Acceptance checks: rows 1-9 of `## Acceptance table`; row 3 compares against the unedited-tree screenshots in `Temp/lighting-strip-baseline/`; rows 2 and 7 are checks the unedited tree cannot pass; row 9 is settled by the diff plus logs.
- Roles: implementer per slice via `/implement-plan` (slices 1-4 parallel, then 5 with
  `/update-vcxproj`, then 6); `/update-affected-code` after slices 1-4; mechanic
  `/code-style-review`; builder `/compile` (DataPacker Release x64 Local data, Sandbox client and
  server Debug x64, ModelViewer Debug x64); Step 6 `/repo-code-review` (C++ and the shared CPU/GLSL
  headers `LightingTerrainCommon.h` and `LightingCommon.h`), `/glsl-review` (`LightingTerrain.comp`,
  `TerrainVertex.h`, the deleted shaders' includes, and the same shared headers), `/comment-review`,
  `/over-engineering-review`, `/coherence-review` for documents only, `/adversarial-review` (Tier
  3), `/external-skill-creator` validation for the command-reference edit, `/update-claude-docs` and
  `/progressive-disclosure-review` for AGENTS.md and skill-reference edits; `/agent-harness` for
  rows 3-9.
