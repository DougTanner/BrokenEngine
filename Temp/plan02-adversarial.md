# Plan 02 adversarial review

## Identity and inventory

Reviewed committed head `b01f2286e10144f36a8031f4daad51c060015151` against fixed baseline `b56f20fa44d0424c2fdf5003157563684ed747f5`.
Working directory for every shell command: `C:/Users/dougt/.claude/worktrees/BrokenEngine/0cb8b7b1-9695-4b87-ab78-8bf06eceacad`.
No source, project, skill, or acceptance artifact was edited. This evidence file is the only reviewer-created file.
Concurrent identity-only changes to 00-context.md and CODEX-HANDOFF.md do not change the reviewed head.
User's worktree correction overrides the older primary-checkout prose.

Documented invocation:
`pwsh -NoProfile -File .agents/scripts/Get-SessionChangeInventory.ps1 -RepositoryRoot 'C:/Users/dougt/.claude/worktrees/BrokenEngine/0cb8b7b1-9695-4b87-ab78-8bf06eceacad' -Baseline b56f20fa44d0424c2fdf5003157563684ed747f5 -Head b01f2286e10144f36a8031f4daad51c060015151`

Result: schemaVersion broken-engine-session-change-inventory/v1; status pass; code ok; truncated false; 213 entries; unlistedUntracked 0.
Counts: 99 cpp, 48 glsl, 3 dual-language-header, 44 doc, 5 script, 6 vcxproj, 4 binary, 4 other.
The complete entries/class inventory is under Changed-artifact inventory below.

## Findings

ADV001 Critical Engine/Source/Graphics/Lighting/LightingComparison.cpp:202 — Counts-only capture invalidation permits scene-affecting tweaks
while the saved receivers stay unchanged. Reachable example: start a paused 1920x1080 fixture-reference capture, wait for tracing, open
Water > Depth, change Water Height, and allow capture to finish. The Water Height slider remains enabled and updates only its Wrapper; the
next ordinary frame renders the new water height. The four publication counts, pause flag and LightingComparisonSettings all remain
identical, so no failure branch runs. Final PNG therefore composes the original receivers' diffuse onto a different water surface. Block
scene-affecting tweak mutations for the running capture or cancel capture at the existing tweak mutation path; no scene hashing or copied
geometry is needed.

ADV002 Required .agents/skills/agent-harness/references/command-reference.md:273 — presentation_continuity_probe still promises a lighting retained-area object although legacy removal deleted that result. CommandPresentationContinuityProbe now writes only shadow and smoke; a consumer following the documented schema requests an absent field. Remove lighting from this command's documented returned-object list.

These findings do not duplicate queued CXX001-CXX004 or the accepted naming work.

## ADV001 evidence and refutation

- LightingComparison.cpp:109 only cancels a running capture for unpause or changed LightingComparisonSettings; :114 returns immediately for unchanged comparison settings.
- LightingComparison.cpp:198-215 compares LightingPublication only. LightingCommon.h:39-45 defines that record as source/instance/triangle/material counts; it contains no water state.
- TweaksScreenWater.cpp:254-263 exposes Water > Depth > Water Height without checking capture status. WaterWrappersBase.cpp:100 defines its allowed range as -0.1 to 0.03 m.
- TweaksScreen.cpp:205-207 forwards slider edits to Wrapper::Set; WrapperBase.h:126-129 only assigns the clamped mfCurrent. No capture cancellation or renderer recreation runs there.
- WaterUniforms.cpp:235-239 republishes gWaterHeight into globalLayout.fWaterHeight every ordinary frame. Water.vert:67-77 adds that height to displaced water outside the terrain-shore fade, changing position without changing LightingPublication counts.
- Graphics.cpp:515-600 Refresh checks settings that require recreation; gWaterHeight is absent. Thus DestroyComparisonResources' recreation-failure branch does not refute this path.
- LightingCapture.cpp:29-35 renders the receivers only once and switches to tracing. :37-51 keeps tracing those saved receivers and then enables composition. Water.frag:161 fetches that image at the final screen pixel.
- LightingReference.comp:29-49 reads saved position/normal/albedo, rather than rerendering current water. FinishCapture then writes these old receiver raws and the new final PNG as one completed capture.
- Water is deliberately nonoccluding, but still receives direct light, so omitting water from the BLAS is no refutation: this failure is the mismatch between receiver inputs and final composition.
- Pausing stops simulation interpolation, not the main-thread Tweaks sliders. Captures intentionally continue ordinary ImGui/render frames. The code path is sufficient static proof; no runtime experiment was performed.
- The governing handoff Decisions promises that changing the scene fails capture, and Graphics/AGENTS Lighting Publication relies on paused inputs staying unchanged. This reachable edit violates that promise.
- Small correction can reuse the existing UI mutation boundary to reject edits or fail the running request; no exhaustive identity/cost machinery, hashes, or tests are requested.

## ADV002 evidence and refutation

- command-reference.md:268-274 describes presentation_continuity_probe and explicitly includes shadow, lighting and smoke.
- Engine/Source/Agent/Commands/PresentationContinuityProbe.cpp:50-51 emits shadow and smoke only; the lighting assignment is removed by this foundation.
- query_profile's new lighting capture object is a different command and schema; its documentation does not satisfy this removed retained-area field.
- The obsolete sentence predates the diff, but deletion of its producer newly makes it false, so it is an in-scope propagation omission, not an unrelated pre-existing defect.

## Traced clean

- Final-frame deadlock — Graphics.cpp:214-217 submits Main, joins/submits UI, then calls FinishCapture for that same framebuffer; LightingCapture.cpp:65 waits its already-queued fence and Screenshot.cpp SaveFinishedPng repeats the completed wait.
- Semaphore signal reuse — CommandBufferManager.cpp:128-143 always submits preparation waiting Global-finished, then Main waits preparation-finished at TOP_OF_PIPE; no conditional path skips consuming either per-frame signal.
- Pending Set 0 lighting update — TextureDescriptors.cpp:48-61 marks only bindings 29/30 UAB and fragment-only; repository search finds LightingCompose consumers only in Main model/terrain/water/fixture shaders. Global's CommandBufferRecordGlobal.cpp pass graph does not consume these bindings. FinishCapture resets them after the UI fence. External VK1 verdict supplied by main is settled and no new API finding depends on it.
- Resource destruction during comparison — ApplyComparisonSettings drains submit/present/upload/device users before settings mutations; Graphics::Destroy destroys command buffers and lighting pipelines/resources before their backing managers. DestroyComparisonResources fails running capture and preserves diagnostics.
- Descriptor growth — Terrain.cpp:135-139 grows the current framebuffer's work buffer and refreshes binding 5; per-framebuffer Prepare runs after the acquired fence; publication resolves handles after producer growth.
- Terrain missing XYZ — Terrain.cpp:277-291 partitions every vertex into bounded dispatches; LightingTerrain.comp:25-44 guards the tail and always writes XYZ, including authorized sea-floor vertices outside visible coverage.
- BLAS/TLAS visibility — model and terrain compute writes precede RecordInputBarrier; BLAS builds precede RecordBuildBarrier/TLAS; RecordQueryBarrier precedes receivers/reference queries (Lighting.cpp:333-378 and LightingAccelerationStructure.cpp:111-143).
- Scene indexing — LightingModels::Write starts independent instance/geometry cursors at zero; Terrain::Write receives the corresponding model counts; fixture offsets add placement counts independently. Query indexes geometry from instance.uiFirstGeometry plus committed geometry index.
- Skinned geometry disagreement — LightingModels.comp matches ModelSkinned.vert's mesh slot, joint addressing, weighted rows, mesh/skin transform order and normal approximation; static ModelStatic.vert directly uses the same packed local positions as static BLAS.
- Trace-only overlap — FightersRender.cpp and BattleshipsRender.cpp reserve total active capacity per pipeline, write visible prefixes and reverse-filled off-screen tails, reserve animation once before joined workers, then publish both disjoint ranges.
- Direct diffuse scaling — LightingRadiometry.h applies albedo/pi and point 1/(4*pi), or rectangle area/PDF/sample factors once; Reference's 4x4 group writes one nonoverlapping pixel tile; 1920x1080 divides exactly. Ray offset 0.05 is shared.
- Failure local diffuse — receivers/tracing do not enable reference controls; final composition enables only its framebuffer; handled export failure and success reset it after the fence. Initial controls/fallback are zero. No alternative gameplay source/deposit path remains.
- Simulation and CRC — changed collection render hooks stay BT_CLIENT, own only GPU publication, and do not add SOA fields, IDs, RNG calls, save/wire payload or PostRender state. TypeRegistry rename preserves both source/particle texture-registration calls.
- Settings format — GraphicsSettings.cpp uses one version 20 layout, 44-byte static assertions and current read/write methods; old compatibility was not added.

## Artifact contract coverage

- C++/GLSL/shared headers: read the lighting producers, capture/comparison, fixture, AS helper, model deformation/query/reference/compose, shared record declarations, ordinary raster consumers, Graphics lifecycle, submission chain, camera basis and render-time control paths. Reviewed full-baseline diffs for shared Buffer/BufferManager, reflection, pipeline writer/creator, legacy removal and settings changes. No additional surviving failure beyond ADV001.
- Documents/instructions: traced handoff Decisions/Known omissions/Acceptance table/Stage 2, context/review/strip plans, FrameUpdatePipeline
  capture sequence, Graphics/shader invariants and command-schema consumer. Stage 2 material lookup and plan03 runtime proof are
  intentionally deferred; visible-only terrain and 5 cm approximation are binding. ADV002 is the surviving schema omission. Older snapshot
  identity/history is subordinate to the explicit worktree brief and concurrently updated identity prose.
- Scripts: reviewed Update-EngineVcxprojItem.ps1 Get-Games exclusion against separate standalone ModelViewer membership, and baseline Setup/Send/Capture/Compare inputs/outputs. Capture has parameterized Harness and Owner and checks load rejection/timed shots; Compare checks dimensions and measures per-pixel channel deltas. Setup/Send are historical save-generation helpers, not required plan03 execution.
- Project metadata: inspected six project/filter changes and client/server boundaries. Sandbox/ModelViewer gain current lighting source/header membership and lose deleted files; server retains shared declarations but no client implementation. No new metadata failure survives the recorded membership/build evidence.
- Binary/other fixtures: parsed all three baseline JSON documents successfully. Close/wide scene tick is 415 with eye heights 156 and
  665.116882324219, as replay notes state (server save tick 400 is distinct from the client's predicted tick). Both PNG IHDRs report
  1920x1080 RGBA; sizes 2,894,743 and 5,728,123 bytes. ClientGuid file has version/payload header and length 32; server save is 2,547 bytes.
  Read saved validator patch as historical provenance. Actual save admission and rendered matching remain plan03, not a static-review claim.
- Verification artifacts: existing build-envelope JSON records successful exitCode 0, empty diagnostics and original primary checkout paths. Those are prior evidence, not a new worktree build. git diff --check over fixed baseline/head returned exit 0. No builds, runtime/harness actions, source edits, branch switches, commits or delegation were performed.

## Residuals

Plan03 still owns live visibility/radiometry/terrain/export/CRC/validation-log evidence; this pass does not claim those rows passed.
Final four-target rebuild in this adopted worktree remains main's pending work.
LightingClosestHit's unconsumed material lookup remains explicitly deferred to plan05.
The brief's newer full-foundation scope and corrected worktree identity were trusted over older strip exclusions and primary-checkout prose.

## Changed-artifact inventory

| Class | Status | Path |
| --- | --- | --- |
| doc | M | `.agents/skills/agent-harness/references/command-reference.md` |
| script | M | `.agents/skills/update-vcxproj/scripts/Update-EngineVcxprojItem.ps1` |
| cpp | M | `Common/TextureFormat.cpp` |
| cpp | M | `DataPacker/Source/ExportJobs/ExportShader.cpp` |
| doc | M | `Documents/Architecture/FrameUpdatePipeline.md` |
| doc | A | `Documents/Investigations/Engine/LightingFoundationHandoff.md` |
| doc | M | `Documents/Investigations/Engine/LightingPrototypeClustered.md` |
| doc | M | `Documents/Investigations/Engine/LightingPrototypeDDGI.md` |
| doc | M | `Documents/Investigations/Engine/LightingPrototypeDirectionalCache.md` |
| doc | M | `Documents/Investigations/Engine/LightingPrototypeReGIRReSTIR.md` |
| doc | M | `Documents/Investigations/Engine/LightingPrototypeSplitRadianceCascades.md` |
| doc | A | `Documents/Investigations/Engine/LightingTerrainPreparationMemoryCost.md` |
| doc | M | `Engine/Data/Shaders/AGENTS.md` |
| glsl | M | `Engine/Data/Shaders/DebugTexture.frag` |
| doc | M | `Engine/Data/Shaders/Lighting/AGENTS.md` |
| glsl | D | `Engine/Data/Shaders/Lighting/AreaLight.frag` |
| glsl | D | `Engine/Data/Shaders/Lighting/LightCombine.comp` |
| glsl | D | `Engine/Data/Shaders/Lighting/LightingBlurH.comp` |
| glsl | D | `Engine/Data/Shaders/Lighting/LightingBlurV.comp` |
| glsl | A | `Engine/Data/Shaders/Lighting/LightingCommon.h` |
| glsl | A | `Engine/Data/Shaders/Lighting/LightingCompose.h` |
| glsl | A | `Engine/Data/Shaders/Lighting/LightingFixture.frag` |
| glsl | A | `Engine/Data/Shaders/Lighting/LightingFixture.vert` |
| glsl | A | `Engine/Data/Shaders/Lighting/LightingFixtureReceiver.frag` |
| glsl | D | `Engine/Data/Shaders/Lighting/LightingHistoryCopy.comp` |
| glsl | A | `Engine/Data/Shaders/Lighting/LightingModelHit.h` |
| glsl | A | `Engine/Data/Shaders/Lighting/LightingModelReceiver.frag` |
| glsl | A | `Engine/Data/Shaders/Lighting/LightingModels.comp` |
| glsl | A | `Engine/Data/Shaders/Lighting/LightingModelsCommon.h` |
| glsl | A | `Engine/Data/Shaders/Lighting/LightingNeutralModel.frag` |
| glsl | A | `Engine/Data/Shaders/Lighting/LightingNeutralModelReceiver.frag` |
| glsl | A | `Engine/Data/Shaders/Lighting/LightingQuery.h` |
| glsl | A | `Engine/Data/Shaders/Lighting/LightingRadiometry.h` |
| glsl | A | `Engine/Data/Shaders/Lighting/LightingReceiver.h` |
| glsl | A | `Engine/Data/Shaders/Lighting/LightingReference.comp` |
| glsl | D | `Engine/Data/Shaders/Lighting/LightingSpread.frag` |
| glsl | D | `Engine/Data/Shaders/Lighting/LightingTemporal.comp` |
| glsl | A | `Engine/Data/Shaders/Lighting/LightingTerrain.comp` |
| glsl | A | `Engine/Data/Shaders/Lighting/LightingTerrainCommon.h` |
| glsl | A | `Engine/Data/Shaders/Lighting/LightingTerrainReceiver.frag` |
| glsl | A | `Engine/Data/Shaders/Lighting/LightingWaterReceiver.frag` |
| glsl | D | `Engine/Data/Shaders/Lighting/PointLight.frag` |
| glsl | A | `Engine/Data/Shaders/Lighting/TerrainElevation.h` |
| doc | M | `Engine/Data/Shaders/Model/AGENTS.md` |
| glsl | M | `Engine/Data/Shaders/Model/Model.frag` |
| glsl | A | `Engine/Data/Shaders/Model/ModelDiffuse.h` |
| glsl | A | `Engine/Data/Shaders/Model/ModelForward.h` |
| glsl | A | `Engine/Data/Shaders/Model/ModelMaterial.h` |
| doc | M | `Engine/Data/Shaders/Objects/AGENTS.md` |
| glsl | M | `Engine/Data/Shaders/Objects/HexShield.vert` |
| glsl | D | `Engine/Data/Shaders/Objects/HexShieldLighting.frag` |
| doc | M | `Engine/Data/Shaders/Quads/AGENTS.md` |
| glsl | A | `Engine/Data/Shaders/Quads/QuadsAxisAligned.h` |
| glsl | M | `Engine/Data/Shaders/Quads/QuadsAxisAlignedVisibleArea.vert` |
| glsl | M | `Engine/Data/Shaders/Quads/QuadsVisibleArea.vert` |
| glsl | M | `Engine/Data/Shaders/ShaderFunctions.h` |
| dual-language-header | M | `Engine/Data/Shaders/ShaderGlobalLayout.h` |
| dual-language-header | M | `Engine/Data/Shaders/ShaderLayoutsBase.h` |
| dual-language-header | M | `Engine/Data/Shaders/ShaderMainLayout.h` |
| doc | M | `Engine/Data/Shaders/Terrain/AGENTS.md` |
| glsl | M | `Engine/Data/Shaders/Terrain/Terrain.frag` |
| glsl | M | `Engine/Data/Shaders/Terrain/Terrain.vert` |
| glsl | M | `Engine/Data/Shaders/Terrain/TerrainElevation.frag` |
| glsl | A | `Engine/Data/Shaders/Terrain/TerrainMaterial.h` |
| glsl | A | `Engine/Data/Shaders/Terrain/TerrainVertex.h` |
| doc | M | `Engine/Data/Shaders/Water/AGENTS.md` |
| glsl | M | `Engine/Data/Shaders/Water/Water.frag` |
| glsl | M | `Engine/Data/Shaders/Water/Water.vert` |
| glsl | A | `Engine/Data/Shaders/Water/WaterMaterial.h` |
| glsl | D | `Engine/Data/Shaders/Water/WaterReflectionProjection.h` |
| cpp | M | `Engine/Source/Agent/AgentCommandsClientGeneric.cpp` |
| cpp | M | `Engine/Source/Agent/Commands/PresentationContinuityProbe.cpp` |
| cpp | M | `Engine/Source/Engine.h` |
| doc | M | `Engine/Source/Frame/Collections/AGENTS.md` |
| doc | M | `Engine/Source/Frame/Collections/AreaLights/AGENTS.md` |
| cpp | M | `Engine/Source/Frame/Collections/AreaLights/AreaLightsRender.cpp` |
| cpp | M | `Engine/Source/Frame/Collections/Collection.h` |
| doc | M | `Engine/Source/Frame/Collections/HexShields/AGENTS.md` |
| cpp | M | `Engine/Source/Frame/Collections/HexShields/HexShieldsRender.cpp` |
| doc | M | `Engine/Source/Frame/Collections/PointLights/AGENTS.md` |
| cpp | M | `Engine/Source/Frame/Collections/PointLights/PointLightsRender.cpp` |
| cpp | M | `Engine/Source/GameBase.cpp` |
| doc | M | `Engine/Source/Graphics/AGENTS.md` |
| cpp | M | `Engine/Source/Graphics/CameraBase.cpp` |
| cpp | M | `Engine/Source/Graphics/CameraBase.h` |
| cpp | M | `Engine/Source/Graphics/Graphics.cpp` |
| cpp | M | `Engine/Source/Graphics/Graphics.h` |
| cpp | M | `Engine/Source/Graphics/GraphicsUtils.cpp` |
| cpp | M | `Engine/Source/Graphics/GraphicsUtils.h` |
| cpp | M | `Engine/Source/Graphics/Islands.cpp` |
| cpp | A | `Engine/Source/Graphics/Lighting/Lighting.cpp` |
| cpp | A | `Engine/Source/Graphics/Lighting/Lighting.h` |
| cpp | A | `Engine/Source/Graphics/Lighting/LightingAccelerationStructure.cpp` |
| cpp | A | `Engine/Source/Graphics/Lighting/LightingAccelerationStructure.h` |
| cpp | A | `Engine/Source/Graphics/Lighting/LightingCapture.cpp` |
| cpp | A | `Engine/Source/Graphics/Lighting/LightingComparison.cpp` |
| cpp | A | `Engine/Source/Graphics/Lighting/LightingFixture.cpp` |
| cpp | A | `Engine/Source/Graphics/Lighting/LightingFixture.h` |
| cpp | A | `Engine/Source/Graphics/Lighting/LightingModels.cpp` |
| cpp | A | `Engine/Source/Graphics/Lighting/LightingModels.h` |
| cpp | A | `Engine/Source/Graphics/Lighting/LightingState.h` |
| cpp | A | `Engine/Source/Graphics/Lighting/Terrain.cpp` |
| cpp | A | `Engine/Source/Graphics/Lighting/Terrain.h` |
| doc | M | `Engine/Source/Graphics/Managers/AGENTS.md` |
| cpp | M | `Engine/Source/Graphics/Managers/BufferManager.cpp` |
| cpp | M | `Engine/Source/Graphics/Managers/BufferManager.h` |
| cpp | M | `Engine/Source/Graphics/Managers/CommandBufferManager.cpp` |
| cpp | M | `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp` |
| cpp | M | `Engine/Source/Graphics/Managers/CommandBufferRecordMain.h` |
| cpp | M | `Engine/Source/Graphics/Managers/DeviceManager.cpp` |
| cpp | M | `Engine/Source/Graphics/Managers/DynamicPipelines.cpp` |
| cpp | M | `Engine/Source/Graphics/Managers/DynamicPipelines.h` |
| cpp | M | `Engine/Source/Graphics/Managers/InstanceManager.cpp` |
| cpp | M | `Engine/Source/Graphics/Managers/InstanceManager.h` |
| cpp | M | `Engine/Source/Graphics/Managers/PipelineManager.cpp` |
| cpp | M | `Engine/Source/Graphics/Managers/PipelineManager.h` |
| cpp | M | `Engine/Source/Graphics/Managers/RenderTargetTextures.cpp` |
| cpp | M | `Engine/Source/Graphics/Managers/RenderTargetTextures.h` |
| cpp | D | `Engine/Source/Graphics/Managers/RenderTargetTexturesLighting.cpp` |
| cpp | M | `Engine/Source/Graphics/Managers/TextureDescriptors.cpp` |
| cpp | M | `Engine/Source/Graphics/Managers/TextureDescriptors.h` |
| cpp | M | `Engine/Source/Graphics/Managers/TextureManager.cpp` |
| cpp | M | `Engine/Source/Graphics/Managers/TextureManager.h` |
| cpp | M | `Engine/Source/Graphics/Managers/TextureUploadManager.cpp` |
| cpp | M | `Engine/Source/Graphics/Managers/TextureUploadManager.h` |
| cpp | M | `Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.cpp` |
| cpp | M | `Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.h` |
| doc | M | `Engine/Source/Graphics/Objects/AGENTS.md` |
| cpp | M | `Engine/Source/Graphics/Objects/Buffer.cpp` |
| cpp | M | `Engine/Source/Graphics/Objects/Buffer.h` |
| cpp | M | `Engine/Source/Graphics/Objects/CommandBuffers.cpp` |
| cpp | M | `Engine/Source/Graphics/Objects/CommandBuffers.h` |
| cpp | M | `Engine/Source/Graphics/Objects/ModelPipeline.cpp` |
| cpp | M | `Engine/Source/Graphics/Objects/Pipeline.cpp` |
| cpp | M | `Engine/Source/Graphics/Objects/Pipeline.h` |
| cpp | M | `Engine/Source/Graphics/Objects/PipelineCreator.cpp` |
| cpp | M | `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp` |
| cpp | M | `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.h` |
| doc | M | `Engine/Source/Graphics/Render/AGENTS.md` |
| cpp | M | `Engine/Source/Graphics/Render/GlobalUniforms.cpp` |
| cpp | M | `Engine/Source/Graphics/Render/LightingUniforms.cpp` |
| cpp | M | `Engine/Source/Graphics/Render/MainUniforms.cpp` |
| cpp | M | `Engine/Source/Graphics/Render/Render.h` |
| cpp | M | `Engine/Source/Graphics/Render/SmokeUniforms.cpp` |
| cpp | M | `Engine/Source/Graphics/Render/WaterUniforms.cpp` |
| cpp | M | `Engine/Source/Graphics/Screenshot.cpp` |
| cpp | M | `Engine/Source/Graphics/Screenshot.h` |
| cpp | M | `Engine/Source/Profile/ProfileManager.h` |
| doc | M | `Engine/Source/Ui/AGENTS.md` |
| cpp | D | `Engine/Source/Ui/CurveData.h` |
| cpp | D | `Engine/Source/Ui/CurveWidget.cpp` |
| cpp | D | `Engine/Source/Ui/CurveWidget.h` |
| cpp | M | `Engine/Source/Ui/GraphicsQualityWrappersBase.cpp` |
| cpp | M | `Engine/Source/Ui/GraphicsQualityWrappersBase.h` |
| cpp | M | `Engine/Source/Ui/GraphicsSettings.cpp` |
| cpp | M | `Engine/Source/Ui/GraphicsSettingsWrappersBase.cpp` |
| cpp | M | `Engine/Source/Ui/GraphicsSettingsWrappersBase.h` |
| cpp | D | `Engine/Source/Ui/LightingWrappersBase.cpp` |
| cpp | D | `Engine/Source/Ui/LightingWrappersBase.h` |
| cpp | M | `Engine/Source/Ui/PbrWrappersBase.cpp` |
| cpp | M | `Engine/Source/Ui/PbrWrappersBase.h` |
| cpp | M | `Engine/Source/Ui/Screens/GraphicsMenuScreen.cpp` |
| cpp | M | `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp` |
| cpp | M | `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenPbr.cpp` |
| cpp | M | `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenSmoke.cpp` |
| cpp | M | `Engine/Source/Ui/SmokeWrappersBase.cpp` |
| cpp | M | `Engine/Source/Ui/SmokeWrappersBase.h` |
| vcxproj | M | `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj` |
| vcxproj | M | `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj.filters` |
| vcxproj | M | `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj` |
| vcxproj | M | `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj.filters` |
| cpp | M | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsRender.cpp` |
| cpp | M | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/FightersRender.cpp` |
| cpp | M | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesRender.cpp` |
| cpp | M | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/OmnisRender.cpp` |
| cpp | M | `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/Tweaks.cpp` |
| cpp | M | `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/Tweaks.h` |
| cpp | D | `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreenLightingEffectsLighting.cpp` |
| doc | M | `Projects/ModelViewer/AGENTS.md` |
| vcxproj | M | `Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj` |
| vcxproj | M | `Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj.filters` |
| cpp | M | `Projects/ModelViewer/Source/Graphics/Camera.cpp` |
| cpp | M | `Projects/ModelViewer/Source/ModelViewerApplication.cpp` |
| doc | A | `Temp/AgentBuildEnvelopes/compile-20261029T135736358Z-8248.md` |
| doc | A | `Temp/AgentBuildEnvelopes/compile-20261029T142029388Z-38852.md` |
| doc | A | `Temp/lighting-foundation-strip-plan.md` |
| doc | A | `Temp/lighting-plans/00-context.md` |
| doc | A | `Temp/lighting-plans/01-build-and-static-checks.md` |
| doc | A | `Temp/lighting-plans/02-reviews-and-cleanup.md` |
| doc | A | `Temp/lighting-plans/03-runtime-acceptance.md` |
| doc | A | `Temp/lighting-plans/04-commit-and-branch.md` |
| doc | A | `Temp/lighting-plans/05-stage2-bounce-reference.md` |
| doc | A | `Temp/lighting-plans/CODEX-HANDOFF.md` |
| binary | A | `Temp/lighting-strip-baseline/AppData/Broken Engine Sandbox Server/lighting-strip-baseline-t400` |
| binary | A | `Temp/lighting-strip-baseline/AppData/Broken Engine Sandbox/ClientGuid.bin` |
| script | A | `Temp/lighting-strip-baseline/Capture.ps1` |
| script | A | `Temp/lighting-strip-baseline/Compare.ps1` |
| script | A | `Temp/lighting-strip-baseline/Send.ps1` |
| script | A | `Temp/lighting-strip-baseline/Setup.ps1` |
| other | A | `Temp/lighting-strip-baseline/baseline-close-scene.json` |
| binary | A | `Temp/lighting-strip-baseline/baseline-close.png` |
| other | A | `Temp/lighting-strip-baseline/baseline-query_profile.json` |
| other | A | `Temp/lighting-strip-baseline/baseline-wide-scene.json` |
| binary | A | `Temp/lighting-strip-baseline/baseline-wide.png` |
| doc | A | `Temp/lighting-strip-baseline/temp-replay-steps.md` |
| doc | A | `Temp/plan02-agents-sync.md` |
| doc | A | `Temp/plan02-cpp.md` |
| doc | A | `Temp/plan02-glsl.md` |
| doc | A | `Temp/plan02-skill-validation.md` |
| doc | A | `Temp/plan02-validator.md` |
| other | A | `Temp/plan02-validator.patch` |
| doc | A | `Temp/temp-slice2-handoff.md` |
| doc | A | `Temp/temp-style-review-detail.md` |
