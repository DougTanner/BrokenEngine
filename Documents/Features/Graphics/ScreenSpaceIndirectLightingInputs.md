# SSRT3 Stage 1: Opaque Inputs and Measurement

Revisit When: the user schedules the SSRT3 screen-space indirect lighting
experiment; this stage runs first and
[stage 2](ScreenSpaceIndirectLighting.md) builds on what it lands.

## Context

Prepare inspectable scene inputs for the selected SSRT3 visibility-bitmask
experiment. This stage produces no indirect lighting. The transport and the
complete quality/cost verdict live in [stage 2](ScreenSpaceIndirectLighting.md).
The separate [planar experiment](PlanarHolographicRadianceCascades.md)
remains independent.

The author's record of the earlier selected decisions is SSRT3 shader-core
adaptation, current opaque visibility (the user's "Option 1") and a split into
input and transport work. Current non-BLEND models remain solid even when
authored MASK; doubleSided remains unsupported and existing back-face culling
stays. These replace the former cascade hierarchy and the unsupported
cutout/two-sided promises. Nothing here has executed; execution is Tier 3 and
goes through the Change Workflow's plan reviews and approval before any
runtime edit.

Target UHD on this PC's actual Vulkan adapter, verified at runtime. Existing
lighting stays. The complete effect's allowance is twice the baseline lighting
cost, with no absolute millisecond or memory ceiling; only stage 2 can evaluate
it. Numerical gates here are author recommendations, not user quotations or
measurements. No build, capture, quality, allocation or timing result supports
feasibility yet.

## Recorded decisions

The design below is the reviewed one: plan audit, simplicity review and a grill
round have been applied. The user's decisions on it:

- Option 2 (drafted pass, extra machinery removed): one resident input target
  set shared by every frame in flight; the existing GpuTimers rows instead of a
  cohort recorder; no CPU inverse view-projection or other reconstruction data
  in MainLayout; the existing debug-texture slots and Screenshot dump names
  instead of new viewers; one Graphics-menu toggle that requests command-buffer
  recreation.
- D1: the work is split into stage 1a (Profile timer row, Graphics-menu toggle
  and its Refresh poll) and stage 1b (input pass, targets, model variant,
  shaders, inspection and the depth-aspect and format-check changes). Each stage
  carries its own acceptance checks below.
- D2: the input targets stay resident while the toggle is off (about 158 MiB at
  UHD); the toggle never allocates or releases them.
- D3: the lighting baseline is the sum of the four existing lighting timer rows
  (deposit, spread, combine, temporal); no new containing lighting row.

## Design

### Raster inputs

After the existing Main preparation (after `RecordWaterFoam` in
`CommandBufferRecordMain::Record`) and before `GpuStop(kGpuTimerMain)`, one
single-sample render pass at the exact framebuffer extent
(`Graphics::mFramebufferVkExtent2D`) writes:

- sampled depth, pinned to D32_SFLOAT independent of the HDR depth format
  `SelectDepthFormat` picks, with a depth-only sampled view and the far clear
  `kfMaxDepth`;
- two RGBA16F color attachments, cleared to zero: linear source emission, and
  geometric normal XYZ with validity in A.

Terrain (the `kPipelineTerrain` draw shape, island mesh arena and
per-framebuffer indirect records) and non-BLEND model materials
(`ModelDrawPass::kOpaque`, back-face culling as in the main variant) draw into
it with dedicated input fragment shaders in `Engine/Data/Shaders/Model/` and
`Engine/Data/Shaders/Terrain/`. They reuse the actual visible draws, vertex
variants, transforms, LOD/elevation, skinning and material/primitive ranges.
Nearest opaque depth wins; valid geometry writes unit normals and A=1. Normals
are geometric, before material normal maps, consistent with the rasterized
geometry; terrain normals follow actual elevation, not its detail-normal
layers. Water, shields, BLEND materials, particles, light sprites and UI are
absent.

Model source emission equals Model.frag's emissive term: the material emissive
factor, times the decoded emissive texture when present, times
`mainLayout.fPbrEmissive`, before fog/exposure. There is no input-only
multiplier. Terrain writes zero emission. No shaded color, sun/IBL, old light
field or instance glow enters it.

The pass is recorded only while the toggle is on and
`GraphicsFlags::kDrawModelsOverWorld` is clear.

### Model variant publication

A third model variant is registered beside the main and shadow variants in
every `DynamicPipelines` model function: creation, descriptor update, indirect
write (including zero instances) and `UpdateAllModelPipelineDescriptors`. This
covers lazy model creation and instance/material/mesh/joint-buffer growth.

### Resources and synchronization

One set of input targets is owned by RenderTargetTextures, created by
`RenderTargetTextures::Create` (reached from
`TextureManager::CreateScreenDependentResources`) and resident whether the
toggle is on or off, as the water foam images are. The HDR attachments are
SwapchainManager members, not RenderTargetTextures; the input set borrows only
their single-image rule: every frame in flight reuses the one set, and the Main
queue's submission order serializes it. The input render pass's incoming
`EXTERNAL -> 0` dependency keeps the previous frame's depth/color writes and
shader reads of these targets in its source scopes.

Every target is parked in `SHADER_READ_ONLY_OPTIMAL` (its
`TextureInfo::eTextureLayout` is a shader-read-only `TextureLayout`), reached at
creation by `Texture::Create`'s transition. `Texture::TransitionImageLayout`
selects the DEPTH aspect for a D32_SFLOAT image and COLOR otherwise (today it
hard-codes COLOR), so the depth target's creation barrier is valid. The input
render pass's attachments end in that parked layout, and its outgoing
`0 -> EXTERNAL` dependency makes the color-attachment writes
(`COLOR_ATTACHMENT_OUTPUT`, `COLOR_ATTACHMENT_WRITE`) and depth-attachment
writes (`EARLY|LATE_FRAGMENT_TESTS`, `DEPTH_STENCIL_ATTACHMENT_WRITE`) visible
to fragment-shader reads (`FRAGMENT_SHADER`, `SHADER_READ`), because the
debug-texture draw samples these targets later in the same Main command buffer.
A recording with the toggle off never touches the targets, so they stay in the
parked layout.

Usage: the depth image carries SAMPLED, DEPTH_STENCIL_ATTACHMENT and
TRANSFER_SRC; the two color images carry SAMPLED, COLOR_ATTACHMENT and
TRANSFER_SRC. At UHD the payload is 165888000 bytes (158.203125 MiB) before
alignment; that arithmetic proves neither residency nor speed.

Device check: beside the water-foam format check in InstanceManager capability
validation, require D32_SFLOAT optimal-tiling `DEPTH_STENCIL_ATTACHMENT`,
`SAMPLED_IMAGE`, `SAMPLED_IMAGE_FILTER_LINEAR` and `TRANSFER_SRC` format
features. `SelectDepthFormat` checks attachment support only. Linear filtering
is required because the debug-texture pipeline binds every slot through
combined samplers, every sampler `TextureManager::CreateSamplers` makes is
LINEAR, and `DebugTexture.frag` samples the selected slot before branching on
format. A missing feature fails startup with a `kError` log, as the water-foam
check does. On the development device that is a stage no-go: the stage stops
and reports it, with no format fallback.

No reconstruction data, MainLayout field or framebuffer-slot index is added;
stage 2 adds or derives reconstruction.

### Inspection

The emission, normal/validity and depth targets are registered in the existing
debug-texture slots (`RenderTargetTextures::RegisterDebugTextures`, within
`kiMaxDebugTextures`) and in `Screenshot.cpp` `ResolveRenderTarget` and
`kpcValidDumpNames`. `DumpRenderTarget` with `raw:true` already writes raw
texels of any format, so no conversion code is added for any target.

The D32_SFLOAT depth image is dumped by a direct depth-aspect transfer copy,
with no conversion shader, pipeline, target or pass:

- `TextureCache::CopyImageToHostMemory` selects the DEPTH aspect for D32_SFLOAT
  and COLOR otherwise, in both its barriers' subresource range and its copy's
  subresource;
- `common::SizeInBytes` (`Common/TextureFormat.cpp`) gains a four-byte
  D32_SFLOAT case;
- `Screenshot.cpp` `FormatName` names D32_SFLOAT and
  `IsSingleChannelNormalizable` accepts it, so the existing float path of
  `SingleChannelToFloat` normalizes it to a PNG and `raw:true` writes the raw
  floats.

Debug-slot display formats: depth uses the existing
`kiDebugTextureFormatGrayscaleR`; emission uses the existing
`kiDebugTextureFormatRgb`; normal/validity uses one new
`kiDebugTextureFormatSignedRgb` constant (`ShaderLayoutsBase.h`) whose
`DebugTexture.frag` branch displays `rgb * 0.5 + 0.5`, so negative normal
components are not clamped as the Rgb branch would clamp them.

### Timing

One GPU timer row for the input pass (its clears and transitions included),
named `kGpuTimerScreenSpaceInputs`, is a child of `kGpuTimerMain` inserted after
`kGpuTimerWaterFoamHistory` in `Engine/Source/Profile/ProfileManager.h`, with
its aligned `kpGpuTimerNames` entry. Main's existing reset range then covers
it, and `GpuSubmitted`/`GpuRead`, the GPU profile screen and the agent
`query_profile` GPU rows report it beside the existing lighting, Terrain and
Objects rows with no further edit. A recording with the toggle off keeps the
row's paired timestamps around no work, as water foam's inactive recording
does, so an empty result replaces the previous sample. There is no cohort
recorder, and ProfileManager.cpp, the per-project ProfileRows.h (CPU rows only)
and CommandBufferRecordGlobal.cpp are not edited.

### Controls

One unpersisted Boolean toggle wrapper, `gSsrt3Experiment` in
`Engine/Source/Ui/LightingWrappersBase.h`, default off. Its label follows
the wrapper identifier's words (`Engine/Source/Ui/AGENTS.md`
`## Wrapper Contracts`), for example `Ssrt3 Experiment`. It is shown as a
Graphics-menu toggle in `GraphicsMenuScreen::Render` between the Lighting
controls' `EndDisabled` and Smoke, drawn disabled while
`GraphicsFlags::kDrawModelsOverWorld` is set. `Graphics::Refresh` polls it with
`DestroyType::kCommandBuffers`, as it polls `gWaterFoamSystemEnabled`. There is
no Tweaks control, Record button or status, and GraphicsSettings.bin and
TweaksSettings.bin are unchanged.

## Stage 1a: Profile row and toggle

In scope: the Timing and Controls regions above —
`Engine/Source/Profile/ProfileManager.h` (enumerator and name),
`Engine/Source/Ui/LightingWrappersBase.h` (wrapper),
`Engine/Source/Ui/Screens/GraphicsMenuScreen.cpp` (`Render` toggle),
`Engine/Source/Graphics/Graphics.cpp` (`Refresh` poll only), and the owning
Profile and Ui AGENTS.md descriptions this makes incorrect. The timer row is
declared but has no recorded scope until stage 1b.

Acceptance:

- The Graphics menu shows the toggle default off, disabled while
  model-over-world is set; flipping it requests command-buffer recreation with
  no validation error, and the setting is not persisted.
- With the toggle off and on, rendering and the existing `query_profile` GPU
  rows match the pre-edit baseline.
- `/compile` BrokenEngineSandbox client Debug|x64 and Profile|x64,
  BrokenEngineSandboxServer and ModelViewer succeed; triggered reviews pass.

## Stage 1b: Inputs and graphics

In scope: the Raster inputs, Model variant publication, Resources and
synchronization, and Inspection regions above, plus recording the stage 1a timer
row around the pass: `CommandBufferRecordMain`, `RenderTargetTextures*`,
`DynamicPipelines`, `PipelineManager`, `WorldLightingShadowPipelines`,
`TextureDescriptors`, `BufferManager`, `TextureCache` and `InstanceManager`
under `Engine/Source/Graphics/Managers/`; `ModelPipeline.{h,cpp}` and
`Texture.cpp` under `Engine/Source/Graphics/Objects/`;
`Common/TextureFormat.cpp`; `Engine/Source/Graphics/Screenshot.cpp`;
`ShaderLayoutsBase.h`, `DebugTexture.frag` and the new input fragment shaders
under `Engine/Data/Shaders/Model/` and `Terrain/`; BrokenEngineSandbox and
ModelViewer project/filter membership (ModelViewer compiles
CommandBufferRecordMain, RenderTargetTextures, Screenshot and ProfileManager,
so any new file its owner needs joins it too); and the owning Graphics and
Shaders AGENTS.md descriptions this makes incorrect.

Acceptance:

- P0, before the first edit, on the unedited tree via `/compile` and
  `/agent-harness`: device/driver/build, exact requests/settings, assets and
  streaming readiness, camera/projection, screenshots, whole-frame/FPS and the
  existing `query_profile` GPU rows (Main and Image roots, the four lighting
  rows, Terrain, Objects) for ordinary and dense-effects UHD paused fixtures in
  both camera modes.
- A1 inputs: raw dumps and debug-texture views (depth GrayscaleR, emission Rgb,
  normal/validity SignedRgb) match forward silhouettes, elevation, skinning,
  material ranges and back-face coverage; terrain/background emission is zero;
  the depth dump arrives through the depth-aspect copy as D32_SFLOAT; excluded
  classes are absent. Emission equality is settled by two signals: a code read
  showing the input model shader's emission expression is Model.frag's emissive
  term, and a raw dump of an untextured emissive material whose valid texels
  equal its emissive factor times `fPbrEmissive`.
- A3 preservation: frames with the toggle off match P0 for
  direct/IBL/shadows/self-emission/display and the excluded effects; on adds no
  scene contribution; with model-over-world set the toggle is disabled and the
  pass absent, including the ModelViewer orbit.
- A4 lifecycle: MSAA off/highest, UHD to smaller to UHD, minimize/restore,
  toggle off/on, world-hidden/resume, empty cells/rewind and pipeline-only
  recreation, with no validation errors or stale accesses. A RenderDoc capture
  plus a lifetime review prove the single set's barriers, layouts, the
  cross-frame incoming dependency and the outgoing dependency to the
  debug-texture draw. Viewing each debug slot and dumping each target with the
  toggle off and on, before and after recreation, is validation-clean; this
  settles the DEPTH-aspect creation barrier and depth-aspect copy at runtime.
  Off omits the pass and keeps the targets resident.
- A5 device and descriptors: the first post-edit client launch on the
  development device passes the D32_SFLOAT format-feature startup check (a
  failure is the stage no-go, reported with its log line, never worked around);
  no new capability or device feature; debug-slot descriptors valid after every
  recreation.
- A6 timing: in one Profile client process per fixture, `query_profile`
  cohorts under the harness caveat — a discarded warm-up, then at least five
  alternating off/on cohorts of equal sample count, switched through the
  Graphics menu by existing harness routes — report the input row, the four
  lighting rows and their sum, Terrain, Objects, the Main and Image roots and
  FPS as median and range. Rows compared across off and on are the four
  lighting rows and their sum, Terrain, Objects, the roots and FPS; the input
  row is reported for on cohorts only, and its all-zero off value is checked to
  be zero rather than stale, never grounds for discarding an off cohort.
  Presentation mode and the sun override are identical on both sides (opening
  Graphics from the pause menu can snap the sun,
  `Engine/Source/Ui/Screens/AGENTS.md`), recorded before the first cohort and
  rechecked after the last. A run expected over 10 minutes gets user approval
  first.
- A7 memory: a payload inventory of each input target's format, extent and
  byte size from its `TextureInfo` (via `common::SizeInBytes`), checked against
  the resource list of one `renderdoc_capture`; no VMA delta or resize-peak
  figure.
- A8 integration: `/compile` DataPacker Release|x64, BrokenEngineSandbox client
  Debug|x64 and Profile|x64, BrokenEngineSandboxServer and ModelViewer;
  project/layout/static checks and triggered reviews pass.

## Out of scope

Stage-2 gather/filter/upsample/composition, sector transport diagnostics,
independent radiance reference, SSRT3 port and notice delivery, the full
quality/cost verdict; production replacement or removal of lighting;
MASK/doubleSided expansion or material/pack changes; new art or simulation
types; water/shield/transparent/particle/sprite transport; generic sources;
offscreen/hidden recovery; HRC or custom cascade/merge; hardware ray tracing,
volumes, distance fields/HZB; stochastic/history/motion-vector/multibounce/
indirect-specular paths; new harness commands or dependencies; async compute;
scheduler/simulation/CRC/network/save/replay changes; unrelated cleanup; unit
tests. Also out by the Option 2 pick: MainLayout reconstruction fields and their
uploads; per-framebuffer target sets; allocation or release on toggle and new
`DestroyFlags` entries; a cohort recorder, Record button or status; a containing
lighting timer; Tweaks controls, a source multiplier and any new inspection
viewer. Also out: a depth-to-R32_SFLOAT conversion shader, pipeline, target or
pass, and any depth-format fallback. No upstream SSRT3 code or license is
copied in this stage.

## Invariants

- The model variant is registered as a unit across every `DynamicPipelines`
  model function.
- Global/Main are recorded once and rebuilt only by drained recreation; the
  toggle requests command-buffer recreation.
- The single input set relies on same-queue Main ordering, with the incoming and
  outgoing render-pass dependencies described above; every target is parked in
  `SHADER_READ_ONLY_OPTIMAL`, and every barrier and copy on the D32_SFLOAT
  image uses the DEPTH aspect.
- D32_SFLOAT is pinned, its four format features are checked at startup, and
  there is no fallback.
- Texture retirement, bindless publication epochs and the
  Main/particle/ImGui submission graph are unchanged.
- The GPU timer enum and name table stay aligned inside Main's reset range,
  with paired timestamps in the off recording.
- MainLayout and the Vulkan 1.2 feature set are unchanged; positions stay in
  camera-cell RenderBasis.
- Server builds, simulation, CRC, network, save, replay and pack contracts are
  untouched.

## Supporting documentation

- [Graphics ownership](../../../Engine/Source/Graphics/AGENTS.md),
  [manager publication](../../../Engine/Source/Graphics/Managers/AGENTS.md),
  [pipeline ownership](../../../Engine/Source/Graphics/Objects/AGENTS.md),
  [profiling](../../../Engine/Source/Profile/AGENTS.md) and
  [shader contracts](../../../Engine/Data/Shaders/AGENTS.md) govern the named
  integration sites.
- [SSRT3 pinned source](https://github.com/cdrinmatane/SSRT3/blob/b38d82140acf646ee485f9ab16a5fa84906f2231/HDRP/Shaders/Resources/SSRTCS.compute)
  establishes why coherent inputs are needed; stage 2 owns its port and
  licensing.
