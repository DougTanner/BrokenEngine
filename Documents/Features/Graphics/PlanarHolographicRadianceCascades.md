# Planar Holographic Radiance Cascades Comparison Prototype

Revisit When: not stated by the user when this was moved from Plans (2026-11-12)

## Context

Today's local-light spread (`deposit -> spread -> combine -> temporal`,
`Engine/Data/Shaders/Lighting/AGENTS.md`) is a stylized blur with no occlusion. This feature is a
bounded client-only experiment: add a developer switch that replaces the spread with Holographic
Radiance Cascades (HRC) transport, plus an independent ray-marched reference, and produce evidence
on whether HRC is accurate and affordable enough to consider. It decides nothing about production
lighting; a failed accuracy or cost gate is a complete, valid negative result that ends the
experiment and authorizes no larger design. A separate screen-space alternative is pending as
[ScreenSpaceIndirectLighting.md](ScreenSpaceIndirectLighting.md).

This was a redrafted, reviewed executable Plan, moved here unimplemented at the user's direction.
Code citations below were verified at commit 075d7a45 and must be re-verified when the work is
picked up.

## Recorded choices

- The user's own picks when this was planned (2026-11-12): approach "Option 5" (HRC swapped in at
  the spread step inside the existing lighting chain, with the existing deposit as emission, the
  existing elevation texture for layer-0 terrain extinction, and combine, temporal and receivers
  unchanged; packed convex-hull/vertical-bound ship blockers; an independent sampled reference
  ray-march as a third mode in the same slot); two stages (`## Stage 1` and
  `## Stage 2`); GPU memory evidence through a new `query_profile` field; on ship hull lighting,
  the question quoted in `### Hull blocking (open design question)`; and moving this work,
  unimplemented, to Features.
- Recorded by the earlier planning agent (its record, not the user's words): Option 5 carries three
  trade-offs, presented to the user as its cons before they picked it: soft pre-blurred deposit
  emission, cost compared per lighting refresh under the existing cadence, and the verdict seen
  through the existing combine/temporal chain. Also: comparison prototype; HRC only; the two
  existing flight planes are independent transport layers with no cross-layer transport; finite
  terrain attenuation; baseline kept for A/B; no production replacement; candidate lighting GPU
  time at most 2x baseline on the identical GPU, resolution, camera and scene; no memory ceiling,
  report actual allocation.

## Approach

One client-only developer tunable `gLightingTransport` (`Lighting Transport`: Baseline default, HRC,
Reference) in the lighting Tweaks; changing it recreates the lighting textures, pipelines and
command buffers. Baseline records exactly today's chain. In HRC and Reference the Main recording
omits the spread passes and, inside the existing `kGpuTimerLightingSpread` bracket on the refreshing
layer, prepares a transport medium from the existing deposit targets and the elevation texture, runs
HRC (or the reference ray-march), and writes RGB-separated EWNS fluence into the pass-0 spread-only
targets (`mpSpreadOnlyTextures[0][0..2]`). LightCombine reads them with a published pass count of 1
and total scale of 1, so combine, temporal, history, area latch, border sampling and every receiver
shader are unchanged and all three modes reach the screen through identical code.

## Design

### Grid and domain

- Transport grid: the pass-0 spread-only extent W x H, covering the refreshing layer's
  `f4LightingArea` (`LightingUniforms.cpp`). It is neither square nor a power of two
  (`TextureManager.cpp`), so the dispatch domain is the square N = bit_ceil(max(W, H)) anchored at
  texel (0,0); texels at or past W or H are padding with zero emission and unit transmittance
  (incoming light 0, T = 1 at the boundary). Output is written only over [0,W) x [0,H), 1:1 with no
  resampling. Lighting-texture +y is south (`LightingSpread.frag`); quadrant rotation follows that.
- Path lengths use the per-axis world texel size derived in the shader as 1 /
  (`f2LightingAreaExtentInverse` * imageSize of the target); lighting texels are only approximately
  square (`ComputeWorldSizedTexelArea`, `Render.h`). Nothing new is published for it.
- The deposit-resolution grid was rejected: at 1440p its N would need an interval image taller than
  a typical 16,384 `maxImageDimension2D` and hundreds of MiB.

### Transport algebra

- Emission density j: artistic linear RGB per world metre of ray travel; terrain extinction sigma
  per world metre; a DDA segment of world length d has tau = sigma * d, T = exp(-tau), L = j * d *
  phi(tau), phi(tau) = (1 - exp(-tau)) / tau with the stable small-tau series 1 - tau/2 + tau^2/6
  and phi(0) = 1. Compose near/far as (Ln + Tn * Lf, Tn * Tf). Never substitute texel distance or
  upstream (1 - T) * emission units. No bounce, ambient injection or scene color enters transport.
- Layer 0 terrain: h from the elevation texture sampled exactly as `LightingSpread.frag` does (same
  visible-area mapping, same `globalLayout` profile fields `fBaseHeight`,
  `fSpreadHeightEndHeightInverse`, `fSpreadHeightPower`, `fSpreadHeightMultiplier`),
  sigma0 = ln(2) * h / D, where D is a new fixed experiment constant set to the island-chain
  water-span scale `kfWaterSpanMeters` (`IslandChainPlacement.cpp`): the half-transmittance
  distance at h = 1. Layer 1: sigma = 0. Terrain never hard-masks emission or receiving.

### Medium preparation (`HrcPrepare.comp`, one dispatch over N x N)

- Emission: the box average of every deposit texel (`mpLightingTextures[0..2]`) whose centre lies in
  the transport texel, each reduced to its EWNS channel sum. Point and area deposits weight EWNS by
  a cos^2 lobe summing to 1 (`PointLight.frag`, `AreaLight.frag`), so the sum recovers scalar
  emission; shield deposits use |x|,|y| weights
  (`Engine/Data/Shaders/Objects/HexShieldLighting.frag`) and enter as their sum. No deposit compress
  (the spread's pass-0 compress stays Baseline-only).
- Output: one N x N RGBA16F medium, rgb = j, a = sigma. Stage 2 adds ship occupancy.

### HRC transport

- Adapt the licensed upstream kernels (`## Source reuse and provenance`): `HrcUp.comp` implements
  Extensions (ordered near/far composition of lower/upper indices and swapped indices, averaging
  completed paths); `HrcDown.comp` implements MergingCones (half endpoint-angle weights; even planes
  interpolate completed merged estimates, not incoming higher-level cones). Port to native Vulkan
  with explicit bounded integer fetches; no independently invented solver.
- Level n: parallel probe stride 2^n, transverse stride 1, 2^n cones and 2^n + 1 interval directions
  with offsets v_n(k) = (2^n, 2k - 2^n), through log2(N). `HrcInitialize.comp` DDA-traces levels 0-2
  directly over the medium; Up builds higher levels one dispatch per level; Down reconstructs one
  dispatch per level. Absent intervals are (0,1); absent radiance and out-of-grid samples are zero.
- Per quadrant (+X, -X, +Y, -Y, serial, scratch reused), `HrcResolve.comp` takes the shifted base
  C0(P + e_q), one texel forward, applies the paper's opacity-aware cross filter (hard taps are
  ships only, Stage 2; terrain never invalidates a tap), normalizes the quadrant by pi/2 exactly
  once, and writes channel q of the three spread-only images. Never angular-scale interval
  transmittance or treat summed reconstructed cone alpha as interval T.
- Storage: interval level element i at (i % N, i / N) of an N-wide RGBA16F image with cumulative
  row-aligned level offsets; interval index ((x*N + y)*(2^n + 1) + k), count (N/2^n)*N*(2^n + 1);
  two N x N RGBA16F reconstruction images ping-pong. Check the interval image height against
  `maxImageDimension2D` before allocation (fail loud). RGBA16F storage and R8_UNORM attachment are
  mandatory formats, so no other feature probe.

### Reference transport (`HrcReference.comp`)

One dispatch over W x H per quadrant q and direction index a (push constants), with
`kiHrcReferenceDirections` = 1024 per quadrant in `HrcCommon.h`. Each invocation DDA-marches one ray
from its texel P through the medium to the [0,W) x [0,H) boundary and adds FP32 RGB into channel q
of three W x H RGBA32F accumulators; direction 0 stores instead of adding, and the last direction of
each quadrant normalizes like the HRC output and writes channel q of the spread-only images. A
compute write-to-read barrier separates consecutive dispatches. Splitting per direction bounds each
dispatch to one ray per texel (W*H*(W+H) steps), because the grid grows with window size and the
spread texture multiplier range, so no single fixed-count dispatch has a provable bound. It starts
at P, so HRC's shifted-base error is charged to HRC.

### Gate, publication, recording and lifetime

- Gate: every transport compute pipeline carries `kIndirectHostVisible` and is recorded with
  `RecordComputeIndirect`; outside Baseline `RenderLightingSpreadIndirect` writes each one's group
  counts when `giLightingRefreshLayer` >= 0 and zero otherwise (combine precedent,
  `LightingUniforms.cpp`). All dispatches of one pipeline share its per-framebuffer slot
  (`Pipeline.cpp`), so a pipeline whose dispatches differ in size publishes its largest and its
  shader returns for invocations past the current level's extent (level in push constants). A frame
  with no refresh therefore publishes zero compute work (`Engine/Source/Graphics/Render/AGENTS.md`
  `## Ordering and Publication`). No GlobalLayout field is added.
- `RenderLightingGlobal`: outside Baseline publish `fSpreadPassCount` = 1 and
  `fCombinePassTotalScale` = 1 (LightCombine sums pass 0 alone at unit gain and applies its existing
  presentation mapping). Spread instance counts are left alone: only the spread loop reads them, and
  the transport branch does not record it.
- `RecordLightingSpreadPipeline`: Baseline branch unchanged. Transport branch, inside the spread
  timer: Prepare; the HRC sequence or Reference; compute write-to-read barriers between every
  dependent dispatch and between quadrants; spread-only images transitioned shader-read -> general
  before the first write (which also orders the previous frame's combine reads) and back after the
  last; the existing final barrier to combine stays.
- `CreateLightingTextures` adds STORAGE usage to the pass-0 spread-only textures and, outside
  Baseline only, creates the medium, interval, two reconstruction and three Reference accumulator
  images; `DestroyLightingTextures` destroys them. A transport change joins the lighting-texture
  recreation block in `Graphics.cpp`, so `CreateLightingTextures` zero-clears every layer's combine
  outputs and re-arms `gbLightingTemporalReset` (`RenderTargetTexturesLighting.cpp`): no mode shows
  another mode's held output, with no extra edge detector.
- Transport compute pipelines are created in `WorldLightingShadowPipelines.cpp`
  `CreateLightingPipelines`, outside Baseline only.
- With Lighting disabled the deposit is cleared, so the reset refresh transports zero emission and
  publishes black, as the spread clears do in Baseline.

### Ship occupancy (Stage 2)

- Fighters, Omnis and Battleships, at the site that already computes `matInstanceTransform` for the
  ModelLayout, write one `LightingBlockerLayout` per drawn non-wreck instance only when the
  transport is not Baseline and the instance's `NearestPlaneIndex` (the value `f4PositionLayer.w`
  carries) equals `giLightingRefreshLayer`; a frame with no refresh (-1) writes none. The record
  holds that instance transform (authored alignment excluded: the packed hull already contains it)
  and the packed `ModelOutline` hull (`pf2HullVertices`, count, `fMinimumZ`, `fMaximumZ`, reached
  through `OutlineOffset` as `ComputeModelShadowBounds` does, `Render.h`). `RenderFrameGlobal` sets
  `giLightingRefreshLayer` before `RenderFrameMain` runs these sites (`Graphics.cpp`).
- `HrcBlocker.vert` emits each instance's prism faces (caps as fans, side quads) projected
  orthographically into the refreshing layer's `f4LightingArea`; the union of a convex prism's
  projected faces is its projected convex hull, so no CPU hull building. Vertices past the hull
  count collapse to degenerate triangles. `HrcBlocker.frag` writes into an R8_UNORM W x H target
  cleared by its render pass. Instance count = blocker cursor, published in
  `RenderLightingSpreadIndirect` after `EndFrameInterpolateRender` in both `RenderFrameMain`
  branches; the cursor resets in `BeginFrameInterpolateRender` (`MainUniforms.cpp`).
- The blocker storage buffer is created in `CreateLightingPipelines` through
  `gpBufferManager->CreateDynamicBuffer` (idempotent) with a file-local
  `kiDynamicBufferInitialCapacity`, following the `CreateDebugRenderPipelines` precedent (fixed
  pipeline, `kIndirectHostVisible`, per-command-buffer storage); growth repoints it through
  `PipelineDescriptorWriter::UpdateStorageBuffer` (`PointLightsRender.cpp` precedent).
  `PipelineManager.cpp` is not edited.
- Recording: occupancy render pass and indirect draw inside the spread timer, then a
  color-attachment-write to compute-read barrier before Prepare, which reads occupancy; Resolve's
  cross filter treats ship texels as its hard taps.

### Hull blocking (open design question)

The original design made an occupied texel hard (T = 0, j = 0). Receivers sample the field at their
own texel, so with hard blockers every texel under a hull resolves black and ships get no local
light in HRC and Reference. The user asked, verbatim: "Should block % based on height from light
source if possible?" Main's interpretation (not the user's words): block only a fraction of the
light, the fraction depending on the height relationship between the light source and the hull,
rather than a fully dark hull or a copy-fill step.

What the code allows:

- Light-source height does not survive into the transport. The deposit reduces each light's height
  to a layer index and projects its position onto that plane (`PointLightsRender.cpp:118-120`,
  `AreaLightsRender.cpp:145-151`); all four channels of the three RGBA16F deposit targets already
  hold EWNS per color (`PointLight.frag:89-93`), and overlapping deposits combine by per-channel MAX
  blending (`DynamicPipelines.cpp:189`, `PipelineCreator.cpp:449-450`), so a texel cannot carry
  per-source heights.
- HRC composes intervals over one medium per layer: a texel's transmittance is shared by every
  source and direction, so a blocking fraction that differs per source cannot be represented. Adding
  a deposit height channel (a fourth MAX-blended target giving the highest source per texel) would
  not change that; per-source height would need one transport per height band, which is the excluded
  vertical/extra-layer transport.
- Hulls do carry vertical bounds (`ModelOutline::fMinimumZ`/`fMaximumZ`,
  `Common/DataFile.h:388-389`), and the flight-plane heights and their spacing are already
  GlobalLayout fields (`pfFlightPlaneHeights`, `fLightingLayerHeightInverse`,
  `ShaderGlobalLayout.h:23,160`). Every source on a layer is deposited at that layer's plane height,
  or on the terrain where that is higher (`GraphicsUtils.h:51-53`).

Candidates, none decided:

- (A) Per-layer partial opacity (feasible, smallest): the blocker shader writes a fraction instead
  of 1 — the share of the layer's height band (the plane height plus or minus half the plane
  spacing) covered by the instance's transformed [minZ, maxZ] — with MAX blending for overlapping
  ships; Prepare converts it to finite extinction over the texel width, and the cross filter treats
  it like terrain rather than a hard tap. Adds a blend flag and a few shader lines; under-hull
  texels are then partly lit. It is height relative to the sources' common plane, not to each
  source; ships on a layer straddle its plane, so the fraction is roughly hull height over plane
  spacing and may block little (not measured). A hull spanning both bands blocks in each by its
  overlap only when written for both layers, which the refresh-layer gating above does not do.
- (B) Hard blocking, accepted as a recorded limitation (under-hull texels and ships stay dark in HRC
  and Reference).
- (C) Hard blocking plus `HrcFill.comp` after Resolve/Reference: each occupied texel copies channel
  q from the nearest unoccupied texel along the cardinal direction q faces, bounded by N; one
  dispatch with no read/write overlap; its time counts against HRC cost.

The accuracy gate does not depend on this choice, since HRC and Reference read the same medium;
under (C) the fill's time counts toward the cost gate.

## Stage 1: transport switch and the two new methods

Builds and passes its own checks without ship blockers.

### In scope

- Selector: `Engine/Source/Ui/LightingWrappersBase.h` discrete `gLightingTransport`;
  `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp` `RenderLightingSection` Write
  subtab: one `Lighting Transport` slider; `Engine/Source/Graphics/Graphics.cpp` lighting-texture
  change block: a transport change sets `kLightingTextures` at pipeline tier.
- Publication: `Engine/Source/Graphics/Render/LightingUniforms.cpp` `RenderLightingGlobal` (pass
  count and total scale outside Baseline) and `RenderLightingSpreadIndirect` (transport group
  counts).
- Resources: `Engine/Source/Graphics/Managers/RenderTargetTextures.h` members;
  `RenderTargetTexturesLighting.cpp` `CreateLightingTextures`/`DestroyLightingTextures`.
- Pipelines: `PipelineManager.h` enum entries; `WorldLightingShadowPipelines.cpp`
  `CreateLightingPipelines` transport compute pipelines.
- Recording: `CommandBufferRecordMain.cpp` `RecordLightingSpreadPipeline` transport branch.
- Shaders: new
  `Engine/Data/Shaders/Lighting/{HrcCommon.h,HrcPrepare.comp,HrcInitialize.comp,HrcUp.comp,HrcDown.comp,HrcResolve.comp,HrcReference.comp}`.
- License: new `Engine/Data/Shaders/Lighting/Hrc.LICENSE.txt` per `## Source reuse and provenance`.
- Memory evidence: a VMA allocation-bytes field in the client `query_profile` reply
  (`Engine/Source/Agent/AgentCommandsClientGeneric.cpp` `CommandQueryProfile`, the value the GPU
  profile screen shows as "VMA allocations", `ProfileScreens.cpp`) and its entry in
  `.agents/skills/agent-harness/references/command-reference.md`.
- Live citations of this document updated to the landed fact (currently
  `Documents/Features/Graphics/ScreenSpaceIndirectLighting.md` `## In scope` and `## Context`
  of `Documents/Features/Graphics/ScreenSpaceIndirectLightingInputs.md`); project/filter membership
  through `/update-vcxproj`; `Engine/Data/Shaders/Lighting/AGENTS.md`,
  `Engine/Source/Graphics/Render/AGENTS.md`, `Engine/Source/Graphics/Managers/AGENTS.md` notes
  through `/update-claude-docs`.

## Stage 2: ship hulls block light

### In scope

- `Engine/Data/Shaders/ShaderLayoutsBase.h` `LightingBlockerLayout` and its hull-vertex maximum,
  asserted equal to `common::kiModelOutlineVertexMaximum` on the C++ side.
- `Render.h` blocker cursor and reserve/allocate declarations; `LightingUniforms.cpp` their
  definitions (capacity growth with descriptor repoint, empty range in Baseline) and the occupancy
  instance count in `RenderLightingSpreadIndirect`; `MainUniforms.cpp` `BeginFrameInterpolateRender`
  cursor reset.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/{Fighters/FightersRender.cpp,Omnis/OmnisRender.cpp,Battleships/BattleshipsRender.cpp}`:
  `BeginRender` adds capacity; the ModelLayout site writes the blocker record under the gating in
  `### Ship occupancy (Stage 2)`.
- The occupancy target, render pass and framebuffer (`CreateLightingTextures`), the blocker buffer
  and occupancy pipeline (`CreateLightingPipelines`), the occupancy pass in
  `RecordLightingSpreadPipeline`.
- New `HrcBlocker.vert`/`HrcBlocker.frag`; occupancy reads in `HrcPrepare.comp` and
  `HrcResolve.comp`; whatever `### Hull blocking (open design question)` resolves to.
- `PipelineManager.h` `Pipelines` enum entry for the occupancy pipeline; `RenderTargetTextures.h`
  members for the occupancy target, render pass and framebuffer; `HrcBlocker.vert`/`.frag`
  project/filter membership through `/update-vcxproj`; AGENTS.md notes through
  `/update-claude-docs`.

## Out of scope

Production selection or removal of Baseline; changes to deposit producers and shaders,
`LightingSpread.frag`, `LightCombine.comp`, `LightingTemporal.comp`, `LightingHistoryCopy.comp`,
terrain/water/model receiver shaders, `ModelLayout`, `ModelPipeline` and Missiles/ModelViewer
producers; separate or square world grids; separate emission, raw-elevation or extinction
rasterization; true-XY receiver sampling; completed-frame timing cohorts and ProfileManager or
`Graphics.cpp` profiling changes; new render-target dump names; shield gating checkboxes;
`Common/Math/ConvexHull.h`; conventional radiance cascades, distance fields, 3D/vertical or
cross-layer transport, extra layers, multibounce, glossy transport, exact mesh silhouettes; asset or
pack format changes; simulation edits; subscription expansion; async compute; renderer refactoring;
Baseline tuning or cadence changes; ThirdParty edits; unit tests.

## Invariants

- EWNS slot order east, west, north, south with texel +y south; two independent layers, no
  cross-layer transport; layer-0 finite terrain sigma, layer-1 none; transport writes only
  `mpSpreadOnlyTextures[0]`, inside the spread timer.
- Baseline recording and results identical to the unedited tree; combine, temporal, history, area
  latch and border sampling unchanged.
- Every transport dispatch is indirect with zero groups when `giLightingRefreshLayer` is -1; no
  GlobalLayout field added and no GlobalLayout write from Main.
- Stage 2: blocker counts finalized after `EndFrameInterpolateRender` in both `RenderFrameMain`
  branches, zero on the no-camera path.
- Client-only state outside CRC, network, save and replay; no main-loop heap allocation; record-once
  command buffers with no re-record outside recreation; cross-frame write-after-read on spread-only
  and transport images ordered; every resource destroyed through lighting-texture and pipeline
  recreation.

## Acceptance criteria

Every harness check runs on the Debug|x64 client through `/agent-harness`: Release sets
`kbDebugInput` and `kbProfiling` false (`Engine/Source/EngineSwitches.h`), so the Tweaks slider,
replays and GPU timers are unavailable there. Estimate total harness time first; a run over 10
minutes needs user approval, offering a shorter spot-check that records its reduced scope.

### Stage 1

- P0, before the first edit, on the unedited tree: empty ocean, a single source, an island/cliff
  placement from `cell_coordinate_probe`, a high-plane source; screenshots with Lighting off and on,
  and `query_profile` snapshots; record GPU, driver, resolution and build configuration.
- Builds: DataPacker Release|x64, BrokenEngineSandbox client and server Debug|x64 and Release|x64,
  ModelViewer Release|x64; zero errors and zero new warnings.
- Mode switching: each Lighting Transport value, then resize, minimize/restore and Lighting off/on,
  with `presentation_continuity_probe {}`; no validation errors and black local light with Lighting
  off in every mode. No stale output: code read of the recreation path (combine zero-clear plus
  temporal reset), and on a paused static fixture, `Combine` raw dumps of both layers right after
  each switch versus after at least ten further refreshes: each early dump all zero or within 1/255
  per channel of the settled one.
- Zero and directions: with no local sources, HRC and Reference `SpreadOnly` index 0 raw dumps are
  exactly zero and finite; with one source east, west, north, then south of a probe region, the
  matching EWNS channel dominates there in both modes.
- Transport accuracy: paused fixtures (single source, island/cliff on layer 0, high-plane source on
  layer 1), `Combine` raw dumps per layer in HRC and Reference; normalized RMS at most 10% of the
  Reference peak over visible-area texels whose Reference value is at least 1% of peak, with counts
  and worst error reported; Reference refreshes complete with no device loss or validation error. A
  miss is a valid negative verdict.
- Terrain layers: layer-0 output behind high terrain is attenuated relative to the same point with
  terrain below base height; layer 1 shows no terrain attenuation.
- Removal: paused fixture with one source and no other emitter, Lighting Update Cadence 1, in HRC
  and Reference; remove the source and step until both layers refresh; raw dumps of `SpreadOnly`
  index 0 channels 0-2 are exactly zero. (Combine and screenshots keep a decaying temporal residual,
  and the transport branch records no spread clear, so `SpreadOnly` index 0 holds the last refreshed
  layer's output.)
- Preserved lighting: Lighting-off screenshots in Baseline, HRC and Reference within 1/255 per
  channel of the P0 Lighting-off screenshot away from animated content. Baseline unchanged by diff
  read: the Baseline branch of `RecordLightingSpreadPipeline`, Baseline
  `fSpreadPassCount`/`fCombinePassTotalScale` publication, and `RenderLightingSpreadIndirect`'s
  spread, combine, temporal and history writes are unchanged, every added line in a non-Baseline
  branch. (A Lighting-on Baseline screenshot cannot match across processes: the spread jitter seed
  is time-seeded once per process, `LightingUniforms.cpp`.)
- Cost: per scene (empty, single source, island), five alternating cohorts per mode (Baseline, HRC)
  of equal sample count in one process, Lighting Update Cadence 1 in both modes (every rendered
  frame refreshes one layer), a warm-up cohort discarded after every mode switch; metric the sum of
  the `LightingDeposit`, `LightingSpread`, `LightingCombine` and `LightingTemporal` `averageUs` rows
  under the `query_profile` caveat in the harness command reference; Reference excluded. HRC median
  at most 2x Baseline median in every scene, with medians, ranges and FPS reported; a miss is a
  valid negative verdict.
- Allocation: the new `query_profile` VMA allocation field read in Baseline and in HRC; the
  difference is reported beside `## Feasibility estimate` (no ceiling).
- License: diff read of `Hrc.LICENSE.txt` names the pinned commit, carries that commit's full root
  Unlicense text, and maps `Shd_Extensions.fsh` to `HrcUp.comp` and `Shd_MergingCones.fsh` to
  `HrcDown.comp` with the changes described.

### Stage 2

- P0 for the hull scenes before Stage 2's first edit: a battleship with an explosion or blaster near
  its hull, and a dense-combat replay.
- Builds as Stage 1; the Stage 1 Preserved lighting diff read repeated.
- Transport accuracy: the hull-plus-source fixture, gate as Stage 1.
- Ship hull: paused hull-plus-source fixture in HRC and Reference; the expected shadow and hull
  appearance follow the resolution of `### Hull blocking (open design question)` (with hard
  blocking: mean `Combine` luminance in the hull's shadow at most 20% of the source-symmetric
  unoccluded point at equal distance; with the fill, the source-facing hull is also visibly lit;
  with (A), the expected result is set when the question is resolved).
- Cost: the hull-plus-source and dense-combat replay scenes, method and gate as Stage 1.
- Diff read: blocker counts are zero on frames with no refresh and on the no-camera path.

## Known trade-offs and limitations

- Emission is the existing pre-blurred deposit, box-averaged, so sources are soft; overlapping
  deposits combine by MAX, not by adding emission. Shields emit through the shared deposit in every
  mode.
- Cost is compared per lighting refresh under the existing one-layer-per-frame cadence, not as an
  every-frame both-layer refresh.
- The verdict is seen through the existing combine and temporal chain; receivers are unchanged, so
  receivers above or below a plane sample the field as they do today.
- Blockers are visible-area ships on their nearest plane only, with standard raster coverage (point
  deposits and ships are already culled to the visible area, `IsPointVisible`, `GraphicsUtils.cpp`,
  so emitters and blockers share one visible set).
- The Reference is a fixed direction set over the same medium, not an analytic oracle;
  HRC-versus-Reference and the four-side direction check replace analytic fixtures.

## Feasibility estimate

Derived arithmetic, unmeasured: transport scratch is about 7 MiB at N=256 and about 31 MiB at N=512
(at commit 075d7a45's defaults these were the 1440p and 2160p domains); Reference accumulators add
48*W*H bytes; Stage 2's blocker buffer is about 848 bytes per ship instance per framebuffer
(transform, 96 hull vertices, count and Z bounds, padded to 16). GPU cost has
no estimate; the Cost check settles it.

## Source reuse and provenance

Pin https://github.com/Yaazarai/Volumetric-HRC/tree/ed153d18f284acce9515880614b1091e9669adcf (root
LICENSE: Unlicense). Adapt `HolographicRadianceCascades/shaders/Shd_Extensions/Shd_Extensions.fsh`
into `HrcUp.comp` and `HolographicRadianceCascades/shaders/Shd_MergingCones/Shd_MergingCones.fsh`
into `HrcDown.comp`, and add `Engine/Data/Shaders/Lighting/Hrc.LICENSE.txt` with the full upstream
license, exact commit, file mapping and change description. No mirror, submodule, GameMaker runtime
or ThirdParty edits. `Shd_FrustumSeed` and `Shd_FluenceSum` at the same commit mark the adaptation
boundary only: they apply pow(rgb, 2.2) and pow(rgb, 1/2.2) and sum four frusta into a scalar, which
the engine replaces with linear emission, finite extinction and directional output.
ppmpreetham/hrc-gi is excluded as a source (zero-extinction emitter accumulation and four
orientations incomplete); amitabha and AdrianMargel are math cross-checks only, licenses
unestablished, no copying.

## Research

- [Holographic Radiance Cascades paper v1](https://arxiv.org/abs/2505.02041v1): sections 2-4.3 give
  the emission/extinction transfer and ordered composition; equations 13-21 and Algorithm 1 the
  hierarchy and parity; section 4.3 the diagonal-ray overlap, shifted-base resolve and opacity-aware
  cross filter. Its benchmarks are not Broken Engine estimates.
- Upstream sources at the pinned commit:
  [LICENSE](https://github.com/Yaazarai/Volumetric-HRC/blob/ed153d18f284acce9515880614b1091e9669adcf/LICENSE),
  [Extensions](https://github.com/Yaazarai/Volumetric-HRC/blob/ed153d18f284acce9515880614b1091e9669adcf/HolographicRadianceCascades/shaders/Shd_Extensions/Shd_Extensions.fsh),
  [MergingCones](https://github.com/Yaazarai/Volumetric-HRC/blob/ed153d18f284acce9515880614b1091e9669adcf/HolographicRadianceCascades/shaders/Shd_MergingCones/Shd_MergingCones.fsh),
  [FrustumSeed](https://github.com/Yaazarai/Volumetric-HRC/blob/ed153d18f284acce9515880614b1091e9669adcf/HolographicRadianceCascades/shaders/Shd_FrustumSeed/Shd_FrustumSeed.fsh),
  [FluenceSum](https://github.com/Yaazarai/Volumetric-HRC/blob/ed153d18f284acce9515880614b1091e9669adcf/HolographicRadianceCascades/shaders/Shd_FluenceSum/Shd_FluenceSum.fsh).
  Extensions swaps lower/upper indices between completed paths; MergingCones uses half
  endpoint-angle weights, ordered composition and completed-merge interpolation on even planes.
