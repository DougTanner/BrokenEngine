<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T23:42:44.499Z","dependsOn":[]} -->
# SSRT3 Stage 1: Opaque Inputs and Measurement

## Context

Prepare inspectable scene inputs for the selected SSRT3 visibility-bitmask
experiment. This is one future, independently buildable stage; it produces no
indirect lighting. The transport and complete experiment verdict live in
[stage 2](ScreenSpaceIndirectLighting.md). The historical filename is retained
for scheduler identity. The separate [planar experiment](PlanarRadianceCascades.md)
remains independent.

The author's record of the selected decisions is SSRT3 shader-core adaptation,
current opaque visibility (the user's “Option 1”), and “Two stages”. Current
non-BLEND models remain solid even when authored MASK; doubleSided remains
unsupported and existing backface culling stays. These replace the former
cascade hierarchy and unsupported cutout/two-sided promises. The latest request
finalizes Plans only; neither stage has executed. Saving or scheduling a Plan
is not implementation approval. Future execution is Tier 3 and uses the
Change Workflow's plan reviews and approval before editing runtime code.

Target UHD on this PC's actual Vulkan adapter, verified at runtime. Existing
lighting stays. The complete effect's allowance is twice baseline lighting
producer cost, with no absolute millisecond or memory ceiling; only stage 2
can evaluate it. Parameters and numerical gates here are author recommendations
for execution approval, not user quotations or measurements. No build, capture,
quality, allocation or timing result supports feasibility yet.

## Design

### Raster inputs and geometry

After existing Main preparation (object shadows/water included), before
`CommandBufferRecordMain::RecordImageRenderPass`, rasterize dedicated terrain
and non-BLEND model MRT variants. Reuse actual visible draws, vertex variants,
transforms, LOD/elevation, skinning, material/primitive ranges and backface
culling. Do not reuse UI depth, resolved MSAA depth or the model-over-world
presentation's depth clear. When `GraphicsFlags::kDrawModelsOverWorld` is set,
visibly disable the experiment. Water, shields, BLEND, particles, light sprites
and UI are absent from these inputs.

At the exact output extent, allocate single-sample sampled depth and two
RGBA16F color attachments: linear emission and geometric normal XYZ/validity A.
Clear colors/validity to zero and depth to the existing far clear; nearest
opaque depth wins. Valid geometry writes unit normals and A=1. Use geometric
normals before material normal maps, with transformed/skinned vertex normals
or position derivatives consistent with the rasterized geometry; terrain
normals must follow actual elevation, not its detail-normal layers. Do not
filter depth or interpolate normals across silhouettes. Inputs are coherent
because all attachments are rasterized with the same depth test/pass.

Require sampled depth-attachment support and usage support, preferring
D32_SFLOAT, using a depth-only sampled view. Require RGBA16F sampled/color
attachment support (and storage only for a chosen diagnostic compute output).
A required unsupported format/usage or descriptor/resource limit is a measured
stage no-go; no new format fallback architecture or device feature is licensed.
Record the selected depth format and actual extents. Scene MSAA stays unchanged.

Source RGB matches Model.frag's decoded emissive texture times material
emissive factor times `mainLayout.fPbrEmissive`, before fog/exposure, in current
linear HDR units. Missing emissive texture leaves the material factor, as in
forward rendering. Terrain writes zero. Input-only multiplier 0/1/2 changes
only captured source emission, never forward self-emission or geometry.
No shaded color, sun/IBL, old local light field, instance glow or feedback enters
it. Geometric normals preserve the outward orientation of actual culled draws;
stage 2 treats back-facing source hits as blockers emitting zero.

### Reconstruction and publication

Extend the parallel C++/GLSL MainLayout and `RenderFrameMain` uploads with a
CPU inverse of the same shaken world view-projection used for these draws,
view orientation/inverse, projection mode and current framebuffer slot. Match
CPU transpose/storage conventions. Unproject pixel centers with Vulkan [0,1]
depth, actual negative-height viewport Y mapping and homogeneous divide into
camera-cell RenderBasis positions. Support both orthographic and perspective;
neither a shader inverse nor an upstream reversed-Z shortcut is acceptable.
No absolute position or simulation column is introduced. Current-frame data
is rebuilt after camera-cell/footprint changes; no history exists.

Register the new model-input variant beside main/shadow variants throughout
`DynamicPipelines::CreateModelPipelines`, descriptor updates, indirect-buffer
writes (including zero instances) and `UpdateAllModelPipelineDescriptors`.
Include lazy model creation and instance/material/mesh/joint-buffer growth.
Reuse ModelPipeline's material splitting and static/skinned vertex selection;
do not silently limit the input pass to already-created static models.

### Resources, synchronization and inspection

Own targets through RenderTargetTextures/TextureManager with one set per actual
swapchain framebuffer slot, without assuming a slot count. At UHD with D32,
input payload is 165888000 bytes (158.203125 MiB) per slot before diagnostics,
alignment and resize overlap. This arithmetic proves neither residency nor
speed. Report actual all-live VMA delta and resize peak. Allocate only while
opt-in is enabled; fully off omits all new passes and releases targets safely.

Add a narrow `Graphics::Refresh`/`RecreateResources` toggle hook using existing
drained recreation: wait all users, replace resources, republish descriptors
and rerecord immutable Main commands. Cover resize, minimize/restore, off/on,
world-hidden/resume, empty cells and rewind. Preserve texture retirement,
bindless publication epochs, Global/Main/particle/ImGui dependencies, HDR/MSAA
attachments and existing submission serialization. Never mutate pending sets.

Sampled-image arrays may hold live slots with a dynamically uniform slot index
under the existing sampled-image indexing feature. A diagnostic storage image,
if needed, is a singleton descriptor/non-array image in a
`PipelineFlags::kPerCommandBuffer` pipeline. In drained creation/recreation,
call existing `PipelineDescriptorWriter::UpdateImageDescriptorInSet` for each
slot's resolved binding/view, VK_NULL_HANDLE sampler, GENERAL layout and
STORAGE_IMAGE type. Republish after pipeline-only recreation too. No helper
implementation/API edit or storage-image array indexing feature is needed.

Use Vulkan 1.2 barriers/layouts for early/late depth writes and color attachment
writes to shader reads, diagnostic writes to transfer reads, and slot reuse.
Keep source pass clears/transitions within its measured scope. Extend existing
debug selection and `DumpRenderTarget` target resolution with completed-slot
emission/normal/validity and depth inspection. Convert sampled depth to an
R32_SFLOAT color diagnostic before readback; never read it with COLOR aspect.
Reuse one selected diagnostic color target, add required TRANSFER_SRC usage
and raw RGBA16F conversion; preserve linear numeric dumps independently of
view tonemapping. Debug registration also refreshes on pipeline recreation.
Diagnostics/readback are off during timing. Reuse current lighting tweaks for
opt-in, disabled status and source multiplier; no harness protocol addition.

### Exact baseline and input timing

Add a containing lighting-producer timestamp scope around deposit and all
spread/combine/temporal/history/unconditional work in Main. Include scheduled
layer and refresh-skip frames; exclude shadows, smoke, wind, water and forward.
Separate input raster scope includes its clears/transitions. Preserve existing
root query ranges, aligning GpuTimers, name/row tables and reset ranges.

Extend `ProfileManager::GpuRead`'s completed-query path to log bounded cohorts:
one record per completed submitted frame/slot, mode and sequence, ready paired
timestamp sums/counts, and named-scope means. Read after the owning fence and
before slot reset/reuse; consume each frame once. Skip not-ready query pairs
without reusing a prior value, count skipped samples and continue until the
required completed sample count. Reset/disable absent scopes to prevent stale
values; a legitimately skipped producer frame is a measured zero only when
that frame's containing scope was executed. Use timestampPeriod/valid-bit wrap
rules. For total-frame GPU time use valid Global-start to final Image/HDR-end
endpoints belonging to the same frame, respecting command-buffer reset/submit
ownership; do not sum overlapping roots. Existing smoothed `query_profile`
rows are context, never the exact cohort source. Use existing log output.

For each ordinary/dense-effects UHD paused fixture, collect five alternating
fully-off/input-on pairs in one instrumented executable/process/device,
identical content/settings. Each side has 120 warm-up and 600 completed
measured frames. Report raw records, lighting L and input means, median/range,
whole-frame GPU/CPU/FPS and memory. No complete-effect C<=2L verdict is possible
here. Before editing capture original-tree preservation evidence separately;
the original tree lacks the new containing timer, so its smoothed rows cannot
be substituted for the instrumented fully-off L baseline.

### Profile-accessible comparison controls

In `Engine/Source/Ui/Screens/GraphicsMenuScreen.cpp::Render`, immediately after
its Lighting controls' `EndDisabled` and before Smoke, add the ordinary
`SSRT3 experiment` toggle, `Record SSRT3 cohort` button and bounded status.
Use the same opt-in wrapper as the Debug tweaks, default off; do not persist
these experimental controls or change GraphicsSettings.bin. They remain outside
`kbDebugInput` and independent of the old Lighting checkbox. Debug input views
and source multiplier stay in Tweaks. This adds no harness command/key binding.

Existing `PauseMenuScreen::Render` reaches GraphicsMenu and `RenderUiScreens`
invokes it without a Debug guard. In Profile use existing `key` Escape,
`describe_ui`, `click` Graphics, toggle/record, Back and Resume routes, using
full labels/window returned by `describe_ui`. Record arms one 120-warm-up/
600-completed-frame cohort through ProfileManager; counting starts only after
return to `UiState::kNone`, required recreation/drain and valid world rendering.
Exclude menu/world-hidden frames; cancel an active cohort on subsequent mode,
fixture/settings or resource changes. Save completion/status to the existing
log. The harness repeats the documented alternating sides/pairs in that same
process; no automatic multi-scene runner is added. Keep presentation mode and
sun override identical on both sides, since opening Graphics can alter them.
Acceptance must demonstrate these controls in an actual Profile client and
show that menu/recreation frames cannot enter its exact timing cohort.

## Critical files

Paths below are relative to the repository root; links name current anchors.
New input/debug helper filenames may follow local conventions inside Lighting.

- [Main recording](../../../Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp): `Record`, `RecordLightingDeposit`, `RecordLightingSpreadPipeline`, `RecordImageRenderPass`; add only the input pass and scopes here. Global recording owns total-frame start/reset coordination.
- [Uniform upload](../../../Engine/Source/Graphics/Render/MainUniforms.cpp): `RenderFrameMain` shaken matrix region; [MainLayout](../../../Engine/Data/Shaders/ShaderMainLayout.h) mirrored fields.
- [Model variants](../../../Engine/Source/Graphics/Managers/DynamicPipelines.cpp): named publication functions above; [ModelPipeline](../../../Engine/Source/Graphics/Objects/ModelPipeline.cpp) `Create`, `RecordDrawIndirect`, `WriteIndirectBuffer`, `UpdateStorageBufferDescriptors`; [BufferManager](../../../Engine/Source/Graphics/Managers/BufferManager.cpp) growth callers.
- [Resources](../../../Engine/Source/Graphics/Managers/RenderTargetTextures.h), RenderTargetTextures.cpp and RenderTargetTexturesLighting.cpp: new per-slot input owners/create/destroy/debug registration; [TextureManager](../../../Engine/Source/Graphics/Managers/TextureManager.cpp) screen-dependent create/destroy and retirement. A dedicated RenderTargetTexturesScreenSpaceIndirectLighting.cpp may hold this owner.
- [Graphics](../../../Engine/Source/Graphics/Graphics.cpp): `Refresh`, `RecreateResources`, `WaitFramebuffer`; PipelineManager, WorldLightingShadowPipelines and TextureDescriptors create/registration regions as required for the input variant.
- [Model emission](../../../Engine/Data/Shaders/Model/Model.frag) `main` emissive region and [terrain](../../../Engine/Data/Shaders/Terrain/Terrain.frag) geometry interface; dedicated input fragments and Lighting input/debug helpers. Share only necessary meaning-preserving source/geometry evaluation with forward shaders.
- [Inspection](../../../Engine/Source/Graphics/Screenshot.cpp): `ResolveRenderTarget`, format/conversion functions and `DumpRenderTarget`; RenderTargetTextures debug registration and TweaksScreenLighting opt-in/inspection region.
- [Ordinary Profile controls](../../../Engine/Source/Ui/Screens/GraphicsMenuScreen.cpp): `Render` Lighting-to-Smoke region; existing pause/graphics UI route is reused unchanged outside this owner.
- [Profiler](../../../Engine/Source/Profile/ProfileManager.cpp): `GpuRead`, `GpuStart`, `GpuStop`, `ResetQueryPools`; ProfileManager.h and ProfileRows.h enums/names/rows. Existing [singleton descriptor writer](../../../Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp) is reused unchanged.

## In scope

Implement exactly the input/reconstruction/publication/resource/inspection/timing
regions above, including GraphicsMenuScreen::Render Profile controls, as one integrated stage. Header declarations, shader generation,
project/filter entries and correctness-dependent callers of those regions are
included. Update only owning AGENTS.md descriptions made incorrect by this
stage. Complete pre-edit preservation checks and the acceptance table below.
No upstream SSRT3 code or license is copied in this stage.

## Out of scope

Stage-2 gather/filter/upsample/composition, sector transport diagnostics,
independent radiance reference, SSRT3 port/notice delivery, full quality/cost
verdict; production replacement/removal of lighting; MASK/doubleSided expansion
or material/pack changes; new art or simulation types; water/shield/transparent/
particle/sprite transport; generic sources; offscreen/hidden recovery; HRC or
custom cascade/merge; hardware ray tracing, volumes, distance fields/HZB;
stochastic/history/motion-vector/multibounce/indirect-specular paths; new
harness commands/dependencies; async compute; scheduler/simulation/CRC/network/
save/replay changes; unrelated cleanup; unit tests.

## Verification and acceptance

Before the first runtime edit, `/compile` and `/agent-harness` capture the
original tree's device/driver/build, exact requests/settings, assets/material
IDs and streaming readiness, camera/projection/transforms, screenshots,
whole-frame/FPS/raw existing profile observations and VMA/resize observations.
Use ordinary and dense-effects UHD paused fixtures. Include packed emissive
ships without planar depositors, nonemissive ships and terrain/cliffs,
static/skinned/material splits, excluded effects, both camera modes and the
lifecycle matrix below. These are preservation baselines, not fabricated new
timer evidence. Record actual Vulkan GPU rather than Windows adapter order.

Use existing `inject_payload`, `edit_frame`, pause/describe/debug,
`query_profile`, `dump_render_target`, `renderdoc_capture` and camera/settings
routes via `/agent-harness`; save exact reproducible requests with the evidence.
Debug-only scene edits/captures run in Debug. Profile timing uses identical
Profile-supported setup on both sides, no Debug-only client commands; server-side fixture authoring may use a Debug server with the client remaining Profile; if existing
packed content cannot reproduce a fixture in Profile, report a blocked timing
check rather than add a protocol. Wait for textures before sampling.

| ID | Required evidence and result |
| --- | --- |
| A1 Inputs | Raw output-exact depth/normal/validity/emission match nearest actual forward silhouettes, elevation, skinning, material ranges and current solid/backface coverage. Terrain/background emission zero. Multiplier 0 gives zero captured emission; 2 versus 1 doubles valid RGB within 2%, forward self-emission unchanged. |
| A2 Reconstruction | Logged pixels plus CPU projection/unprojection and captures align reconstructed geometry with real draws under both projections, shake, neighbor-cell crossing and multicell jump; no nonfinite/stale-cell/misaligned data. |
| A3 Preservation | Fully-off versus original-tree baselines preserves direct/IBL/shadows/self-emission/display and water/shields/transparents/particles/sprites. On changes inspection only, adds no indirect scene contribution; excluded classes absent from inputs. Model-over-world visibly disables. Record authored MASK/two-sided limits. |
| A4 Lifecycle | Repeat MSAA off/highest supported setting, UHD->smaller->UHD, minimize/restore, off/on, world-hidden/resume, empty cells/rewind and pipeline-only recreation across all slots. No validation errors, stale accesses or pending-set writes. Capture plus lifetime review prove barriers/slot views/publication and retained submission graph/HDR/MSAA. Fully off frees targets/omits work. |
| A5 Descriptors | Generated SPIR-V/reflection and writes match singleton diagnostic storage counts/bindings/slot-selected views, no StorageImageArrayDynamicIndexing capability/new feature/helper extension. Do not add compute/storage diagnostics gratuitously if raster conversion suffices. |
| A6 Timing | Exact five-pair cohorts above, 600 unique completed frames per side, valid frame endpoints and query resets in on/off/hidden paths; report L/input/full frame/FPS, raw means/median/range and readiness counts. No stale disabled values or repeated smoothed rows, no full-effect budget claim. |
| A7 Memory | Record actual slot count/formats/limits, payload inventory versus all-live VMA delta and resize peak including diagnostics/retirement. No imposed memory ceiling or feasibility claim from arithmetic. |
| A8 Integration | `/compile` shader generation/DataPacker Release|x64, BES client Debug|x64 validation/Profile|x64 timing and affected server-safe builds; project/layout/static checks and triggered reviews pass. Server/simulation/CRC/network/save/replay/.pack contracts untouched. Independent stage acceptance review verifies A1-A7. |

Timing needs 7200 completed frames per fixture, excluding loading/draining/
captures. Duration is unmeasured. Follow [Change Workflow User Interaction](../../../.agents/references/change-workflow.md#user-interaction)
Long test runs: before dispatching a run expected to exceed 10 minutes, obtain
user approval with expected duration, checks and a shorter alternative. Do not
truncate mandatory cohorts or treat a spot check as full acceptance.

A required unsupported resource is a documented stage no-go. Shader defects,
incoherent inputs or invalid evidence require correction; input-only completion
never claims useful four-pixel lighting or a complete experiment verdict.

## Risks and invariants

Future Tier 3 triggers are shared CPU/GLSL layout, sampled-depth MRT,
per-slot model publication, immutable recording and GPU lifetime/synchronization.
Keep current Vulkan 1.2 feature requirements, camera-cell RenderBasis,
server-safe file affinity, deterministic simulation/CRC and retained lighting.
All original and new resource users must be drained before descriptor/resource
replacement; no assumed shared-HDR serialization protects new slot resources.
Use `/update-affected-code`, `/update-vcxproj`, `/update-claude-docs` and required
C++/GLSL/comment/style/overengineering/coherence/progressive/adversarial reviews
as triggered; `/verify-acceptance` verifies stage completion. No unit tests.

## Supporting documentation and research

Source anchors were checked at baseline 379a17c99d379c1e6ad4e3ce3ad1a46977c0fc08
and primary f6c5a7befa65f4792037fce372b82c5488740b81: the graphics/shader/profile
and attribution code named here is unchanged between them. These are navigation
and contract evidence, never performance proof. Recheck after future drift.

- [Graphics ownership](../../../Engine/Source/Graphics/AGENTS.md), [manager publication](../../../Engine/Source/Graphics/Managers/AGENTS.md), [pipeline ownership](../../../Engine/Source/Graphics/Objects/AGENTS.md), [render ordering](../../../Engine/Source/Graphics/Render/AGENTS.md), [profiling](../../../Engine/Source/Profile/AGENTS.md) and [shader contracts](../../../Engine/Data/Shaders/AGENTS.md) govern the named integration sites.
- [Model decoder](../../../DataPacker/Source/ExportJobs/Scene/GltfSceneDecoder.cpp) `DecodeMaterialShaderDatas` warns on unsupported doubleSided; Model.frag and pipeline culling establish current visibility. Input fixtures test agreement, not glTF feature compliance.
- [SSRT3 pinned source](https://github.com/cdrinmatane/SSRT3/blob/b38d82140acf646ee485f9ab16a5fa84906f2231/HDRP/Shaders/Resources/SSRTCS.compute) establishes why coherent inputs are needed; stage 2 owns its port and licensing. This stage claims no transport implementation.
