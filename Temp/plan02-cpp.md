# Plan 02 C++ review

## Findings

Status: NEEDS_ACTION. No confirmed reachable runtime correctness defect survived tracing. Four Required repository-contract/minimality findings remain; the naming decision below is separately routed style work.

- CXX001 Required Engine/Source/Graphics/Lighting/Lighting.cpp:412 — SetComparison and GetDiagnostics (line 427) are forbidden trivial state accessors
  — SetComparison only assigns already-public mRequestedComparisonSettings; GetDiagnostics only returns State::diagnostics. Rule 49 and the review's
  Public state and forwarding APIs check require direct access. Remove the two methods, assign requested settings directly in
  TweaksScreenLighting.cpp, and move diagnostics to a public Lighting member while retaining private coupled resource/capture state.
- CXX002 Required Engine/Source/Graphics/Lighting/LightingFixture.cpp:106 — GetAnimation, GetAnchor (137), GetCamera (142), and GetEvent (147) each perform one member/indexed-state access — these are independent immutable fixture inputs, with no complete multi-statement mutation protected by the getters. Make the four underlying members public and replace calls with direct member/.at access; retain GetSolid, MakeCamera, InitializeAnimation and the Write operations, which perform actual work.
- CXX003 Required Engine/Source/Graphics/Lighting/LightingState.h:93 — eScenario duplicates settings.eScenario without a cache rationale — both initialize to kDensity; ApplyComparisonSettings first copies the entire settings and immediately copies that field again at LightingComparison.cpp:139-140; all four readers can read settings.eScenario. The cpp-conventions derived-members rule is a hard flag. Delete the redundant field/assignment and use settings.eScenario.
- CXX004 Required Engine/Source/Graphics/Managers/InstanceManager.cpp:522 — the deleted terrain validation still contributes an unconditional
  capability requirement — the reason string explicitly names original terrain vertex-output validation, while current vertex stages have no
  storage/image writes or atomics. DeviceManager.cpp:211 enables it unconditionally; the prior optional GPU-validation/debug-printf enable at 222
  already remains. This unused feature configuration violates the authorized strip of unused machinery and Scope Authorization minimality pass. Remove
  the new unconditional enable and required-feature row; retain the existing optional instrumentation enable.

## Contract checks

- Publication/indexing: model Write starts both cursors at zero; its only caller passes spans sized for models + terrain placements + fixture solids. Terrain receives model instance and geometry counts separately; fixture offsets add the same terrain-placement count to each corresponding model count. Each AS custom index names its corresponding LightingInstance. Geometry count metadata removed by OE04 is CPU-only; the shader count remains. No removed CPU metadata is serialized.
- Producer ownership: Battleships/Fighters reserve each controller pipeline for total active capacity, write visible prefixes and disjoint trace-only
  tails, reserve poses for both before workers run, and join workers before EndRender publication. Missiles use the same nonoverlap rule; Omnis
  publish their existing uncullled range. These changes are client render hooks, with no SOA column, RNG, simulation phase, save/wire format or CRC
  membership change. ModelViewer publishes its selected current model from its own RenderFrameMain implementation.
- Packed model trust: BufferManager validates model chunk bounds, index count, stride and vertex extent before upload. LightingModels validates scene
  flags/counts/material extent, present texture indices, primitive range/order/triangle divisibility and vertex indices before use. Static assets
  retain resident buffers/BLAS; animated per-framebuffer outputs resolve final mesh/joint and instance addresses after all ordinary producers and
  fixture publication have finished growing them. Zero-weight joints are skipped by the deformation shader.
- Terrain: every Placement is resident and has an actual mesh draw index range. Work storage is sized by the whole placement arena; writes index it by
  uiPlacement, while material tables use the compact lighting index. Compute dispatches partition every vertex below uiVertexCount, with a tail guard,
  and write XYZ even outside the visible area (sea floor). Compute-to-AS input barriers and BLAS-to-TLAS barriers remain. The elevation color-write to
  compute/vertex-read barrier follows Global completion. Visible-elevation-only coverage and the 5 cm shared offset are accepted limitations.
- Submission: one Global-finished signal is consumed by each preparation submit; each preparation-finished signal is consumed by Main. Preparation is recorded/submitted every frame, including disabled comparison and empty gameplay publication. RecordWaterDisplacement precedes lighting preparation, RecordEnvironment precedes capture receivers/tiles, and Main waits preparation at TOP_OF_PIPE. Per-framebuffer command pool/reset resources follow that framebuffer's covering fence.
- Pending Set 0 update: bindings 29/30 carry UPDATE_AFTER_BIND, and the corresponding sampled-image/storage-buffer UAB features are required/enabled.
  They are fragment-only; the current Global pass's shader graph does not consume LightingCompose. RecordCapture writes them before Main submission;
  FinishCapture resets them only after the UI fence. CLAIM-VK1 is VERIFIED by the manager's independently dispatched external-claims worker:
  VUID-vkUpdateDescriptorSets-None-03047 permits UAB updates while pending users do not use those bindings; UPDATE_UNUSED_WHILE_PENDING is not
  additionally required. Official references: https://docs.vulkan.org/refpages/latest/refpages/source/vkUpdateDescriptorSets.html and
  https://docs.vulkan.org/refpages/latest/refpages/source/VkDescriptorBindingFlagBits.html .
- Fence/readback: Graphics::RenderMainPresentAcquire submits Main, then SubmitUiCommandBuffer (which waits the submit worker and synchronously queues
  ImGui's fence signal), then FinishCapture with the same captured framebuffer index. FinishCapture waits it before SaveFinishedPng/readbacks;
  SaveFinishedPng's repeated wait therefore cannot wait for a future submission. The present image is read before Present. Five RGBA32F images are
  1920*1080*16 = 33,177,600 bytes each; PNG and capture.json complete the seven-file export.
- Capture failure/zero diffuse: initial controls and fallback image are zero. Receivers/tracing never enable composition. Only the last tile frame
  enables its own framebuffer; FinishCapture disables it after success or handled export failure, and teardown drains users before resetting all sets
  and destroying comparison resources. Unpause/settings changes/publication-count mismatch fail before composition; recreation retains the failure
  status. Thus a failed capture leaves gameplay local diffuse zero on the handled paths. Device loss deliberately escapes to renderer recovery.
- Recreation: Graphics drains submit/present/upload workers and the device before destruction. Lighting pipelines are destroyed before
  PipelineManager; full comparison/terrain/model resources are destroyed before TextureManager/BufferManager/device. State and request/capture
  diagnostics survive resources recreation. Dynamic pipeline registrations unregister at individual Pipeline destruction. Comparison compute/receiver
  pipelines are created once in the null-render-pass block, and receiver model pipelines are added only when absent.
- Timers/agent: preparation resets the Main-to-UI timer range and starts Main; the immutable Main buffer stops it. Lighting Terrain endpoints occur only in active comparison, inside that preparation span; enum/name ordering is paired. GpuRead is nonblocking and handles unavailable disabled queries. AgentCommandServer::Drain increments miDeferredDrainCount on every live deferred poll before timeout comparison; no capture-hold bypass remains.
- Legacy propagation: removed lighting dynamic maps/passes, descriptor bindings, debug arrays, render targets, curves and wrappers have no remaining source references under their removed names. Retained visible sprites/shields, sun/moon, IBL, shadow, smoke, wind and water paths keep their independent producers. GraphicsSettings changes version 19 to 20 and locks the new 44-byte layout; there is one current format.
- Restored production calls: direct comparison against aa47e4a3e170a69b7dad971b94003f7ea30e5716 confirms the exact CHECK_VK(vmaCreateImage/Buffer(...)) lines in Texture.cpp, ImGuiManager.cpp and TextureUploadManager.cpp exist identically in baseline and current files. Buffer.cpp's final CHECK_VK follows the authorized one-retry ordinary-RAM policy. That policy was not challenged or changed by this review.

## Routed style decisions

- Lighting remains a struct: rule 67 expressly exempts structs, so its three implementation files are valid. Converting it to class would create a rule-67 violation unless merged, which is unnecessary churn.
- Rule 3 requires mpState -> pState and mRequestedComparisonSettings -> requestedComparisonSettings throughout Lighting's implementation and the Tweaks caller. These are style changes for main to authorize/route, not a runtime correctness finding.
- The historical routed note overstates the access-label rule: neither rule 3 nor rule 49 prohibits private: in structs. Rule 49 permits coupled resource/capture state protected by complete operations. Retain that private State boundary and its private lifecycle helpers; independently readable diagnostics move public for CXX001. The initial public: label is redundant but harmless; no broad access-label sweep is necessary.
- LightingFixture can remain a class with public mAnimation, mAnchor, mCameras and mEvents for CXX002; class members retain m prefixes. Other members need no rename or access change merely to settle these four accessors.
- GetComparisonCamera, GetRenderEyeHeight and UpdateComparisonCamera are semantic/multi-statement operations and do not match the one-access/one-assignment/one-pass-through check. Keep them.
- OE01's unused fixture animation fields are already absent; OE05's zero-based signature is correctly propagated. The TerrainVertexCoordinates question is settled by the current shared helper used by Terrain.vert and LightingTerrain.comp; there is no remaining duplicate world-XY/UV implementation to extract. Pre-existing ModelViewer include grouping stays outside scope.

## Scope

NEEDS_ACTION for CXX003/CXX004's unused added state/configuration. Other reviewed changed regions map to the original reusable-prototype foundation, full legacy-lighting replacement and approved stripping, including necessary render/descriptor/agent/UI integration. No new candidate algorithm, one-bounce reference, backward compatibility, unit test, pack-format mutation, or simulation behavior change was introduced. Ordinary-RAM/OOM behavior is a binding user-selected exception and was preserved.

## Measurements

One batched documented invocation of Measure-Tokens.ps1 measured all 59 surviving changed cpp targets: bt-token-v1 total 250,717 tokens, 21,953 lines, 1,002,779 bytes. The other four cpp targets are deleted. No changed region exposed an additional cohesive size split that warrants a follow-up observation; the existing Lighting capture/comparison split remains permitted for a struct.

## Project membership trigger

/update-vcxproj is triggered for 13 added C++ files under Engine/Source/Graphics/Lighting (Lighting.cpp/.h, LightingAccelerationStructure.cpp/.h,
LightingCapture.cpp, LightingComparison.cpp, LightingFixture.cpp/.h, LightingModels.cpp/.h, LightingState.h, Terrain.cpp/.h) and seven removed targets
(RenderTargetTexturesLighting.cpp, CurveData.h, CurveWidget.cpp/.h, LightingWrappersBase.cpp/.h, game TweaksScreenLightingEffectsLighting.cpp). The
supplied successful build/static-check evidence is relevant but this reviewer did not inspect project XML or rerun membership tooling.

## Residuals

Authority discrepancy: the older strip plan excludes Frame files, Common/TextureFormat.cpp, DataPacker and legacy-removal content; the current
full-review brief explicitly authorizes the entire foundation and newer validated client-only residency removals under the user's
reusable-prototype/strip authorization. This review trusts the current user-authorized scope and does not misclassify those regions as unauthorized.
Runtime acceptance remains plan03 and one-bounce material-hit verification remains plan05; neither was run or claimed by this static review. No
incomplete target trace or pending external verdict remains.

## Coverage

All 102 unique paths in the manager-produced schemaVersion/paths-only targets file were traced against the immutable baseline-to-working snapshot. The table records the contract checked for every path, including all seven deletions. TRACED does not override the findings above.

| Target path | Trace result |
| --- | --- |
| Common/TextureFormat.cpp | TRACED: Format byte sizes through texture creation/readback; RGBA32F export stride. |
| DataPacker/Source/ExportJobs/ExportShader.cpp | TRACED: Reflection collects AS resources into binding table; runtime sparse-binding writer consumes it. |
| Engine/Data/Shaders/ShaderGlobalLayout.h | TRACED: Scalar CPU/GLSL layout, removed fields and binding constants through populators and rebuilt shader consumers. |
| Engine/Data/Shaders/ShaderLayoutsBase.h | TRACED: Scalar CPU/GLSL layout, removed fields and binding constants through populators and rebuilt shader consumers. |
| Engine/Data/Shaders/ShaderMainLayout.h | TRACED: Scalar CPU/GLSL layout, removed fields and binding constants through populators and rebuilt shader consumers. |
| Engine/Source/Agent/AgentCommandsClientGeneric.cpp | TRACED: Main-thread query serialization and removed dump channel/continuity lighting field through request/readback consumers. |
| Engine/Source/Agent/Commands/PresentationContinuityProbe.cpp | TRACED: Main-thread query serialization and removed dump channel/continuity lighting field through request/readback consumers. |
| Engine/Source/Engine.h | TRACED: Guarded aggregation includes Lighting without exposing its heavy State definition. |
| Engine/Source/Frame/Collections/AreaLights/AreaLightsRender.cpp | TRACED: Visible quad capacity/cull/packed writes/indirect count retained; deposit path removed. |
| Engine/Source/Frame/Collections/Collection.h | TRACED: Registration rename reaches both type CRC and particle CRC plus retained startup texture requests. |
| Engine/Source/Frame/Collections/HexShields/HexShieldsRender.cpp | TRACED: Visible shield buffer layout/descriptor growth/indirect count paired with shared shader record; deposit removed. |
| Engine/Source/Frame/Collections/PointLights/PointLightsRender.cpp | TRACED: Visible sprite capacity/controller intensity/camera-facing writes/indirect count retained; deposit removed. |
| Engine/Source/GameBase.cpp | TRACED: Apply settings before camera/publication; missing-camera comparison update; deferred rendering remains paced. |
| Engine/Source/Graphics/CameraBase.cpp | TRACED: Comparison pose rebasing, projection, render eye height and unchanged ordinary camera path. |
| Engine/Source/Graphics/CameraBase.h | TRACED: Comparison pose rebasing, projection, render eye height and unchanged ordinary camera path. |
| Engine/Source/Graphics/Graphics.cpp | TRACED: Lighting ownership, begin/end publication, submit-UI-finish-present order, recreate dependency order. |
| Engine/Source/Graphics/Graphics.h | TRACED: Lighting ownership, begin/end publication, submit-UI-finish-present order, recreate dependency order. |
| Engine/Source/Graphics/GraphicsUtils.cpp | TRACED: Deleted legacy deposit sizing declaration/definition has no remaining consumers. |
| Engine/Source/Graphics/GraphicsUtils.h | TRACED: Deleted legacy deposit sizing declaration/definition has no remaining consumers. |
| Engine/Source/Graphics/Islands.cpp | TRACED: Stable mesh arena gains acceleration input/device-address usage without changing residency/indirect indexing. |
| Engine/Source/Graphics/Lighting/Lighting.cpp | TRACED: Source/model/terrain/fixture spans and offsets, resource lifetime, descriptors, diagnostics latch; CXX001. |
| Engine/Source/Graphics/Lighting/Lighting.h | TRACED: Source/model/terrain/fixture spans and offsets, resource lifetime, descriptors, diagnostics latch; CXX001. |
| Engine/Source/Graphics/Lighting/LightingAccelerationStructure.cpp | TRACED: Move/RAII, AS storage/scratch alignment and compute/build/query barriers; scratch-size removal. |
| Engine/Source/Graphics/Lighting/LightingAccelerationStructure.h | TRACED: Move/RAII, AS storage/scratch alignment and compute/build/query barriers; scratch-size removal. |
| Engine/Source/Graphics/Lighting/LightingCapture.cpp | TRACED: Progressive tile recording, composing-frame enable, fence-completed export and failure disable. |
| Engine/Source/Graphics/Lighting/LightingComparison.cpp | TRACED: Drained settings changes, status lifecycle, persistent pipeline creation, receiver draws and current descriptors. |
| Engine/Source/Graphics/Lighting/LightingFixture.cpp | TRACED: Immutable seeded sources/samples/events/cameras, asset initialization, solid geometry and animation publication. |
| Engine/Source/Graphics/Lighting/LightingFixture.h | TRACED: Immutable seeded sources/samples/events/cameras, asset initialization, solid geometry and animation publication. |
| Engine/Source/Graphics/Lighting/LightingModels.cpp | TRACED: Validated packed materials/primitives, static and per-framebuffer animated BLAS, zero-based model cursors. |
| Engine/Source/Graphics/Lighting/LightingModels.h | TRACED: Validated packed materials/primitives, static and per-framebuffer animated BLAS, zero-based model cursors. |
| Engine/Source/Graphics/Lighting/LightingState.h | TRACED: Framebuffer/private resource ownership, count-only completion latch and capture state; CXX003. |
| Engine/Source/Graphics/Lighting/Terrain.cpp | TRACED: Placement/vertex/material/work indexing, per-framebuffer growth, shared producer pipeline and full XYZ writes. |
| Engine/Source/Graphics/Lighting/Terrain.h | TRACED: Placement/vertex/material/work indexing, per-framebuffer growth, shared producer pipeline and full XYZ writes. |
| Engine/Source/Graphics/Managers/BufferManager.cpp | TRACED: Model/instance/mesh/joint addresses preserve flags through growth, final reservations and descriptor repointing. |
| Engine/Source/Graphics/Managers/BufferManager.h | TRACED: Model/instance/mesh/joint addresses preserve flags through growth, final reservations and descriptor repointing. |
| Engine/Source/Graphics/Managers/CommandBufferManager.cpp | TRACED: Exactly-once Global-to-preparation-to-Main semaphore chain and post-fence dynamic command recording. |
| Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp | TRACED: Moved environment/water preparation entry points, receiver-before-Main ordering, retained immutable image passes. |
| Engine/Source/Graphics/Managers/CommandBufferRecordMain.h | TRACED: Moved environment/water preparation entry points, receiver-before-Main ordering, retained immutable image passes. |
| Engine/Source/Graphics/Managers/DeviceManager.cpp | TRACED: AS/ray feature chain, entry points, allocator device addresses and descriptor capacity; CXX004. |
| Engine/Source/Graphics/Managers/DynamicPipelines.cpp | TRACED: Deleted deposit/hex-light maps and creation methods; remaining visible/model callers and enum counts. |
| Engine/Source/Graphics/Managers/DynamicPipelines.h | TRACED: Deleted deposit/hex-light maps and creation methods; remaining visible/model callers and enum counts. |
| Engine/Source/Graphics/Managers/InstanceManager.cpp | TRACED: Physical feature/property chain, extension/format checks and enabled-device agreement; CXX004. |
| Engine/Source/Graphics/Managers/InstanceManager.h | TRACED: Physical feature/property chain, extension/format checks and enabled-device agreement; CXX004. |
| Engine/Source/Graphics/Managers/PipelineManager.cpp | TRACED: Deleted spread/combine state and creation; retained debug texture descriptors and world pipeline constructor. |
| Engine/Source/Graphics/Managers/PipelineManager.h | TRACED: Deleted spread/combine state and creation; retained debug texture descriptors and world pipeline constructor. |
| Engine/Source/Graphics/Managers/RenderTargetTextures.cpp | TRACED: Retained targets initialized before debug registration; all debug slots receive a valid elevation texture. |
| Engine/Source/Graphics/Managers/RenderTargetTextures.h | TRACED: Retained targets initialized before debug registration; all debug slots receive a valid elevation texture. |
| Engine/Source/Graphics/Managers/RenderTargetTexturesLighting.cpp | TRACED: Entire obsolete lighting texture/renderpass/debug-array producer deleted; all entry points removed. |
| Engine/Source/Graphics/Managers/TextureDescriptors.cpp | TRACED: Global layout flags/features/bindings, zero fallback/control buffers and post-fence reset; CLAIM-VK1 verified. |
| Engine/Source/Graphics/Managers/TextureDescriptors.h | TRACED: Global layout flags/features/bindings, zero fallback/control buffers and post-fence reset; CLAIM-VK1 verified. |
| Engine/Source/Graphics/Managers/TextureManager.cpp | TRACED: No blur slots/production/deferred-adoption branch remain; retained texture registration/recreation requests. |
| Engine/Source/Graphics/Managers/TextureManager.h | TRACED: No blur slots/production/deferred-adoption branch remain; retained texture registration/recreation requests. |
| Engine/Source/Graphics/Managers/TextureUploadManager.cpp | TRACED: Retained registration set lifetime plus authorized ordinary-RAM coherent upload staging. |
| Engine/Source/Graphics/Managers/TextureUploadManager.h | TRACED: Retained registration set lifetime plus authorized ordinary-RAM coherent upload staging. |
| Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.cpp | TRACED: Terrain/water sampler renumbering and preserved shadows/displacement/foam through current shader bindings. |
| Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.h | TRACED: Terrain/water sampler renumbering and preserved shadows/displacement/foam through current shader bindings. |
| Engine/Source/Graphics/Objects/Buffer.cpp | TRACED: Device-address/AS usage, host coherence, logical handle selection and preserved ordinary-RAM/OOM behavior. |
| Engine/Source/Graphics/Objects/Buffer.h | TRACED: Device-address/AS usage, host coherence, logical handle selection and preserved ordinary-RAM/OOM behavior. |
| Engine/Source/Graphics/Objects/CommandBuffers.cpp | TRACED: Preparation buffer/semaphore create/move/destroy and reset-capable per-framebuffer pool ownership. |
| Engine/Source/Graphics/Objects/CommandBuffers.h | TRACED: Preparation buffer/semaphore create/move/destroy and reset-capable per-framebuffer pool ownership. |
| Engine/Source/Graphics/Objects/ModelPipeline.cpp | TRACED: Removed lighting descriptor shifts shadow/smoke append slots; explicit mesh/joint bindings preserved. |
| Engine/Source/Graphics/Objects/Pipeline.cpp | TRACED: Per-framebuffer Set1 inference still covers all current lighting consumers after unused force flag removal. |
| Engine/Source/Graphics/Objects/Pipeline.h | TRACED: Per-framebuffer Set1 inference still covers all current lighting consumers after unused force flag removal. |
| Engine/Source/Graphics/Objects/PipelineCreator.cpp | TRACED: Removed lighting MRT special case; explicit four receiver attachments and single-sample offscreen targets. |
| Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp | TRACED: AS descriptor pNext/count/destination set through reflected sparse layouts and per-framebuffer query bindings. |
| Engine/Source/Graphics/Objects/PipelineDescriptorWriter.h | TRACED: AS descriptor pNext/count/destination set through reflected sparse layouts and per-framebuffer query bindings. |
| Engine/Source/Graphics/Render/GlobalUniforms.cpp | TRACED: Removed local-light fields and publisher; retained sun/moon/shadow/terrain plus comparison render eye height. |
| Engine/Source/Graphics/Render/LightingUniforms.cpp | TRACED: Deleted legacy global lighting; retained water/IBL/BRDF/HDR parameters and render-height fade consumers. |
| Engine/Source/Graphics/Render/MainUniforms.cpp | TRACED: Skinning reset, collection begin/end publication, empty-frame camera uniforms and comparison shake suppression. |
| Engine/Source/Graphics/Render/Render.h | TRACED: Removed lighting history state/signature/snapshot field reaches every current producer and agent consumer. |
| Engine/Source/Graphics/Render/SmokeUniforms.cpp | TRACED: Deleted local-light multiplier write only; smoke history, color and spread publication retained. |
| Engine/Source/Graphics/Render/WaterUniforms.cpp | TRACED: Two render-eye-height consumers follow comparison camera; phase/foam producer contracts retained. |
| Engine/Source/Graphics/Screenshot.cpp | TRACED: Finished PNG encoding after UI fence; removed legacy target/channel routes and retained dump validation. |
| Engine/Source/Graphics/Screenshot.h | TRACED: Finished PNG encoding after UI fence; removed legacy target/channel routes and retained dump validation. |
| Engine/Source/Profile/ProfileManager.h | TRACED: Terrain timer enum/name indices align with reset range and query write/read owners. |
| Engine/Source/Ui/CurveData.h | TRACED: Whole obsolete curve type/widget deleted with all includes/callers and lighting spread consumer. |
| Engine/Source/Ui/CurveWidget.cpp | TRACED: Whole obsolete curve type/widget deleted with all includes/callers and lighting spread consumer. |
| Engine/Source/Ui/CurveWidget.h | TRACED: Whole obsolete curve type/widget deleted with all includes/callers and lighting spread consumer. |
| Engine/Source/Ui/GraphicsQualityWrappersBase.cpp | TRACED: Lighting quality removed; remaining quality wrappers and apply functions remain paired. |
| Engine/Source/Ui/GraphicsQualityWrappersBase.h | TRACED: Lighting quality removed; remaining quality wrappers and apply functions remain paired. |
| Engine/Source/Ui/GraphicsSettings.cpp | TRACED: Version20/44-byte format, offsets, validation, save/load/reset removal all agree. |
| Engine/Source/Ui/GraphicsSettingsWrappersBase.cpp | TRACED: Removed enabled wrapper agrees with menus, persistent flags and deleted renderer path. |
| Engine/Source/Ui/GraphicsSettingsWrappersBase.h | TRACED: Removed enabled wrapper agrees with menus, persistent flags and deleted renderer path. |
| Engine/Source/Ui/LightingWrappersBase.cpp | TRACED: Whole old-lighting tuning vocabulary deleted with shader/populator/menu consumers. |
| Engine/Source/Ui/LightingWrappersBase.h | TRACED: Whole old-lighting tuning vocabulary deleted with shader/populator/menu consumers. |
| Engine/Source/Ui/PbrWrappersBase.cpp | TRACED: Deleted local-light BRDF controls paired with shaders, populators and Tweaks maps. |
| Engine/Source/Ui/PbrWrappersBase.h | TRACED: Deleted local-light BRDF controls paired with shaders, populators and Tweaks maps. |
| Engine/Source/Ui/Screens/GraphicsMenuScreen.cpp | TRACED: Only obsolete lighting rows removed; retained table/disabled scopes and settings save remain balanced. |
| Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp | TRACED: Bounded UI settings feed next-frame Apply; capture request/status and remaining extension tab; CXX001. |
| Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenPbr.cpp | TRACED: Removed lighting map keys and widget rows together; retained material/HDR rows unchanged. |
| Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenSmoke.cpp | TRACED: Removed local-light multiplier registrar/widget together; retained smoke table/tab scopes. |
| Engine/Source/Ui/SmokeWrappersBase.cpp | TRACED: Removed local-light wrapper declaration/definition paired with smoke populator and UI. |
| Engine/Source/Ui/SmokeWrappersBase.h | TRACED: Removed local-light wrapper declaration/definition paired with smoke populator and UI. |
| Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsRender.cpp | TRACED: Controller-partition visible prefix/trace tail, total-capacity bound, joined skinning and dual-range publication. |
| Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/FightersRender.cpp | TRACED: Mirrored visible prefix/trace tail indexing and joined skinning; climb transform retained. |
| Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesRender.cpp | TRACED: Visible prefix/trace tail, dead-row skip, normal transform and two range publications. |
| Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/OmnisRender.cpp | TRACED: Dead-row count matches actual output/pose reservation; complete rendered range publishes to Lighting. |
| Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/Tweaks.cpp | TRACED: Removed obsolete game subtab hook; retained visible-effects tab numbering follows engine Comparison tab. |
| Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/Tweaks.h | TRACED: Removed obsolete game subtab hook; retained visible-effects tab numbering follows engine Comparison tab. |
| Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreenLightingEffectsLighting.cpp | TRACED: Deleted obsolete game lighting-only controls; extension function and declaration removed. |
| Projects/ModelViewer/Source/Graphics/Camera.cpp | TRACED: Removed deleted lighting-height cache only; ordinary orbit/flight projection retained. |
| Projects/ModelViewer/Source/ModelViewerApplication.cpp | TRACED: Selected-model publication after final pose and neutral ordinary renderer with no legacy lighting counts. |
