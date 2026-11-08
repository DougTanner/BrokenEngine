<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T23:43:25.725Z","dependsOn":[]} -->
# Planar Holographic Radiance Cascades Comparison Prototype

## Context and authority

Build one bounded client-render experiment replacing the selected local-light
spread contribution with Holographic Radiance Cascades (HRC). Retain the current
renderer temporarily for A/B comparison. This Plan is the executable successor
to the former planar feature proposal; the pending [screen-space alternative](ScreenSpaceIndirectLighting.md)
is the separate stage-2 SSRT3 transport Plan; its stage-1 input capture adds no indirect lighting. Saving this Plan authorizes no implementation; its claim still
requires user approval under the Change Workflow. Implementation is Tier 3
(render architecture, GPU resources, shader and receiver contracts).

Recorded user decisions: comparison prototype; HRC only; the existing two flight
planes are independent transport layers; no cross-layer transport; terrain
height and packed convex-hull/vertical-bound ship blockers are acceptable. The user selected finite terrain attenuation and requires terrain, water and model vertices above/below both planes to remain lighting receivers. Implementation is deferred to a future session; this session finalizes the Plan only.
Current FPS is ideal; complete candidate lighting GPU time must be at most twice
baseline lighting GPU time on the identical GPU, resolution, camera and scene.
No memory ceiling was approved: report actual allocation and peak usage.

All numerical quality targets and experimental choices below are author
recommendations for approval with this Plan, not user quotations or measurements.
No Broken Engine GPU performance or image-quality measurement exists yet.

There is one final acceptance cut, after implementation and the complete
comparison matrix. Completing the experiment means producing correct,
reproducible evidence and a recommendation. Promotion additionally requires the
quality and performance gates below. A failed GPU or quality gate is a valid
prototype result; it authorizes neither additional features nor a larger design.
There is no production rollout or baseline-removal stage in this Plan.

## Design

### Contribution, source units and receivers

Replace point/area-light local transport only. Use existing point and oriented
area render producers, including weapon flashes, blasters, missile exhaust,
fighter beams, Omni heal beams and explosions. Preserve source color, preblurred
lighting texture RGB/alpha, orientation, lighting size and scalar intensity.
Multiply packed color alpha AND blurred texture alpha; preserve point rotated/clamped UVs, area authored/repeat UVs and area per-instance intensity. Derive original size before baseline minimum-size floors, retaining area centre-to-corner conventions. Visible sprites continue through their existing independent scene draws.

Define emission density j(x,y) as existing source color times texture RGB/alpha
times lighting intensity, in artistic linear RGB per world metre of ray travel.
This deliberately defines new transport units; it does not claim photometric
equivalence with spread controls. The source budget Q is the area integral of j
over its original lighting quad. Overlapping sources add j; sources add no
extinction. Terrain extinction and emission coexist: do not erase sources merely
because their XY overlaps terrain. Hard ship cells alone have T=0 and j=0,
including sources inside those blockers. Every nonship DDA segment of world
length d uses tau=sigma*d, T=exp(-tau), L=j*d*phi(tau),
phi(tau)=(1-exp(-tau))/tau, phi(0)=1. Use the stable small-tau limit
1-tau/2+tau*tau/6 to avoid subtracting nearly equal values; cross-check its switch
against the analytic reference. Transparent emission yields (j*d,1); hard ships
return (0,0). Compose near/far as (Ln+Tn*Lf,Tn*Tf). Sigma is inverse world metre,
j is artistic linear RGB per world metre, and d is world metres including diagonal
and clipped partial-texel lengths. Never substitute texel/UV distance or blindly
copy upstream (1-T)*emiss source-function units. No reflection, bounce iteration,
ambient injection, spread snapshots, compressed deposits or scene color enters
transport. Never trace final UNORM combine output.
For grid subpixel support, expand each source quad's local axes to at least two
field texels, preserve UV orientation, and multiply j by original area / expanded
area. Rasterize with additive blending and 4x4 deterministic subpixel quadrature
per covered texel, including partial coverage. Do not reuse
`MinLightingDepositSize`, old directional weights or deposit edge fade.
This preserves the continuous area integral while broadening tiny sources;
measure discrete energy error rather than asserting exact raster conservation.
No automatic source brightness tuning to hide gate failures. A single uniform
candidate gain of 1 applies before the existing local-light presentation curve.

Exclude hex shields from candidate emission and blocking; retain their visible
geometry. Compare source-matched cohorts with baseline shield deposits disabled,
and separately show normal shield-rich baseline versus candidate to document the
unsupported contribution. Exclude material glTF emission extraction, arbitrary
particles as emitters/receivers, smoke absorption, transparent media and arbitrary
static models as blockers. Existing smoke shading that consumes terrain/model/
water local-light values remains; this adds no particle-lighting system. Mode-gate all deferred low/high reads in the model smoke loop as well as initial reads; preserve unblended per-plane smoke lighting. Terrain/water smoke retains four times the mapped ambient average.

Terrain, live BES ships and water retain their current material lighting
responses, height blending, cardinal model specular approximation and ambient
scaling, subject to the finite terrain extinction policy below. These are
stylized responses to planar transport, including upward
surfaces; a planar ray has no physical vertical Lambert contribution. Preserve
direct sun/moon, their shadow products, IBL, water reflections and final scene
HDR tone mapping. Apply the existing Uchimura/hue-preserving local-field mapping
once in the candidate adapter, without spread-pass normalization; output the
existing RGB-separated EWNS representation and the average of its four mapped
channels for ambient. Do not add baseline spread energy on top.

### Coordinates, footprint and height

Every source, blocker and receiver uses actual geometric XY converted once from
its cell into the camera's `RenderBasis`. Candidate mode bypasses
`ProjectToFlightPlane` and eye-ray flight-plane receiver projection together.
Keep baseline projection in baseline mode. Do not write absolute positions or
render-derived values into simulation, CRC, network, save or replay state.

Use a square N-by-N grid, initially N=256 and a mandatory fixed N=512 comparison.
No other quality configurations are part of this experiment. Each layer shares
the same square world rectangle: enclose `mf4RenderVisibleArea`, take its larger
side V, and use side 2V centred on the visible rectangle. Derive the 256-grid
texel pitch first, rounding up to a multiple of twice the current shared
water-quad world grid, then snap the rectangle centre to that pitch. Use this
same rectangle at 512, with half that pitch, so both configurations preserve
the shared grid and compare the same world footprint. Recompute coverage after
snapping; extend by a whole grid step when needed to retain the V/2 margin.
Publish the actual area,
pitch and N for captures. This explicit experimental resolution is independent
of the window, like other world targets; it changes no terrain/water grid policy.

Use the full finite footprint for transport; no arbitrary additional range or distance attenuation beyond the explicitly selected finite terrain extinction. Trace to its boundary, where incoming L=0 and T=1.
Outside samples are black, never clamped copies of an edge source. Restrict
quality comparisons to the visible rectangle unless testing the edge itself.
Sources entirely outside the footprint contribute zero. Include all existing
subscribed render-interpolate cells whose source or blocker bounds overlap the
footprint, even offscreen; gather sources before sprite culls and hulls before visible-model selection with capacity for full subscribed contributors. Visible-sprite frustum culling must not remove
transport contributors. Do not expand subscriptions or promise unseen cells.

Source centre Z selects the nearest existing flight plane using FlightPlanes'
current tie rule. Never duplicate a source across layers or reject it merely
for being above/below its selected plane. Terrain, water and model vertices
sample actual geometric XY and retain existing clamped low/high height blending
at actual Z, including below the low plane and above the high plane. Add no
above/below-plane eligibility cutoff. Terrain never hard-masks receiving or
emission; finite footprint and opaque ship masks can still make a sample black.

Render raw terrain elevations over the entire padded footprint using existing
packed heightmaps and placement buffers; visible-area elevation texture and sun
shadow map are insufficient. Preserve submerged raw elevations. The existing
LightingSpread.frag low-layer profile supplies
h=pow(clamp((rawElevation-fBaseHeight)*fSpreadHeightEndHeightInverse,0,1),
fSpreadHeightPower)*fSpreadHeightMultiplier. Publish the same current profile
inputs, retaining baseline tuning. Use sigma0=ln(2)*h/D and sigma1=0. Terrain extinction is finite and nonnegative, not a plane-intersection threshold. Derive the low-layer extinction image from raw elevation and the current profile before tracing; high-layer initialization uses uniform sigma=0 without a redundant zero image. Clear
raw elevation to the existing no-terrain/submerged representation giving h=0.

Use fixed prototype D=80 world metres, an author recommendation for this bounded
experiment. It is the half-transmittance distance at h=1, anchored to the nominal
island-chain water-span scale named kfWaterSpanMeters in IslandChainPlacement.cpp.
This tests attenuation over an island-scale horizontal traversal independently
of grid resolution. D is a separate fixed experiment constant, not a runtime
placement dependency, tuning UI or calibration framework. No baseline-equivalent
value is established: spread distance/per-pass decay is not a Beer-Lambert
coefficient. Record D/profile inputs with diagnostic captures. At h=1 paths D
and 2D yield T=1/2 and 1/4; h=0 and high-layer terrain yield T=1.

Terrain/water remain eligible at true XY even above both planes. Existing
height blending and material responses remain; no forced high-terrain darkness,
exterior terrain sampling or ambient floor. Source/terrain overlap integrates
simultaneous j/sigma exactly. Finite low-layer terrain attenuation and transparent
high terrain do not promise opaque cliff shadows. No vertical/cross-layer
transport is introduced.
For live Fighters, Omnis and Battleships, obtain `ModelOutline` through the
packed model header's `OutlineOffset`, as `ComputeModelShadowBounds` does.
Transform the vertices at both packed MinZ and MaxZ with the same remaining
interpolated pose/scale as the rendered instance. Authored alignment is already
baked into the packed hull: do not apply it twice. Project the transformed prism
with common::BuildConvexHull to a conservative XY convex hull and transformed min/max Z; rasterize it in each
plane its Z interval intersects. Use conservative texel overlap, filling gaps
and concavities. Do not use inset gameplay hitboxes or change the pack format.
Exclude sinking wrecks and skeletal deformation. This is an intentionally
conservative prism, including pitch/roll; it is not a mesh silhouette.

### Ship receiver self-occlusion

The same per-render-instance projected convex hull is available to the model
shader through a client-only storage range referenced by its model layout.
For each EWNS channel, cast a cardinal XY ray from the fragment: if inside its
own hull, solve the convex polygon exit and sample one field texel beyond that
exit; otherwise sample its true XY. Use that channel only from this exterior
position. This applies only on a layer the ship blocks, including vertices above/below that plane; no extra vertex-height rejection. For nonblocking layers
use the fragment's true XY. Resolve ambient from these four corrected channel
samples, preserving current height blending and downstream material equations.

Use four nearest exterior field taps with bilinear weights, discarding opaque
taps and renormalizing surviving weights; if none survive return black. Hard opaque taps include other ships; finite terrain never invalidates a tap. Do not scan farther through another
blocker, erase a hull from transport, or add an ambient floor. Keep model pose,
blocker hull and receiver range from the same Main publication frame.

Independent planar ship masks cannot distinguish an XY-overlapping receiver above/below another hull from one at its plane, or resolve vertically overlapping ships. Record that overocclusion; do not invent a height cutoff. This rule deliberately shades convex outer boundaries rather than exact inset
mesh surfaces. It cannot resolve light inside concavities, overlapping hulls,
subtexel gaps or angular variation within a quadrant. Those are explicit test
limitations and promotion risks, not open architecture choices. Demonstrate a
lit visible source-facing hull and a dark region behind the same hull together;
failure ends the comparison with a negative result rather than silently adding
mesh tracing or another receiver backend.

### HRC construction and EWNS adaptation

Adapt only the licensed Yaazarai kernels pinned in Source reuse and provenance. HrcUp.comp
implements Shd_Extensions: ordered near/far composition of lower/upper indices and swapped
upper/lower indices, averaging completed extension paths. HrcDown.comp implements Shd_MergingCones
angular endpoint weights and distinct parity cases: even planes interpolate completed merged
estimates, not incoming higher-level cones. Translate this concrete algorithm into native Vulkan
rather than independently inventing a solver or importing GameMaker.

For level n retain parallel probe stride 2^n, transverse stride 1, 2^n cones and 2^n+1 interval
directions with offsets v_n(k)=(2^n,2k-2^n), through log2(N). Engine HrcInitialize directly
DDA-traces levels 0,1,2 using the exact j/sigma and hard-ship contract above; extensions construct
higher levels. Replace normalized texture2D access with explicit bounded integer atlas fetches,
preserving probe centres/parity/endpoints. Absent intervals are (0,1); absent reconstructed radiance
and out-of-grid samples are zero. Clip direct intervals to the footprint.

Rotate the four source-facing quadrants to +X,-X,+Y,-Y, including texture Y sign, then retain EWNS
outputs. Preserve the kernel's half endpoint-angle factors with its two-edge summation; normalize
the integrated quadrant RGB by pi/2 exactly once. Never angular-scale interval transmittance or
treat summed reconstructed cone transmittance as physical interval T. Uniform emission and the
independent DDA oracle must settle normalization, units and orientation. No tau normalization or
omnidirectional sum.

Replace Shd_FrustumSeed gamma/input logic with engine linear emission, finite terrain extinction and
hard-ship occupancy. Replace Shd_FluenceSum gamma encoding and frustum addition with directional
output plus existing local-light presentation. Preserve the forward one-pixel offset in each rotated
quadrant and opacity-aware cross filter, checking parity/base-offset/boundary behavior
independently. Upstream math is reusable; finite-domain source integration, directional outputs and
engine acceptance remain the adaptation contract.

Resolve C0(P+e_q), one field texel forward in quadrant q, then apply the
paper opacity-aware cross filter. This is the deliberate anti-overlap approximation
relative to physical receiver P, not same-origin transport. Level 0 has diagonal
intervals (1,+/-1); the first cardinal interval is n=1,k=1, offset (2,0).
Diagnostic expected paths use these offsets. The direct oracle starts at P,
charging shifted-base error to the candidate. Cross-filter hard occupancy means
ships only; finite terrain must not invalidate receiving taps.
### Storage, pass order and lifetime

Reuse `RenderTargetTextures`, `WorldLightingShadowPipelines` and `PipelineManager`;
add no renderer manager or subsystem API. Use storage images for hierarchy data,
avoiding new packed 16-bit SSBO requirements: flatten level element i into
(i % N, i / N) of an N-wide RGBA16F image, with cumulative row-aligned level
offsets. Interval index within a level is
((x*N+y)*(2^n+1)+k), count (N/2^n)*N*(2^n+1).
Reconstruction count is (N/2^n)*N*2^n, packed by the same x,y,k order.
Retain the whole interval hierarchy; two N-by-N RGBA16F reconstruction images
ping-pong because every reconstruction level has N*N elements. Run quadrants
and then height layers serially and reuse this scratch; save only their output.
At 256 the interval hierarchy alone is about 5.50 MiB; at 512 about 24.00 MiB,
before row padding, allocation alignment, inputs and outputs. These are derived
payload estimates, not measured allocation or memory budgets.

Allocate per-layer N-by-N emission RGBA16F and occupancy R32_UINT images; use
separate raw elevation R32_SFLOAT and one low-layer extinction N-by-N R32_SFLOAT image, the shared interval hierarchy, reconstruction
ping-pong and per-layer RGB-separated RGBA16F EWNS images. The three directional
images hold four channels per RGB component. Candidate presentation targets use
existing combine format and include ambient; baseline resources remain allocated
for A/B. Debug reference/output scratch exists only while comparison diagnostics
are enabled. Record all extra hull ranges, alignment and debug allocations.
Check sampled/storage/attachment/blend format features and actual atlas height
against device limits before allocation. Lack of required features is an
explicit unsupported-device result; no alternative algorithm or silent precision
fallback. Query actual row counts rather than using estimated memory arithmetic.

Record the candidate chain once in Main before scene drawing, after current
source and ship-pose CPU publication. RenderLightingMain publishes footprint/control before BeginRender; finalize source/hull counts and candidate indirect parameters after EndFrameInterpolateRender in BOTH RenderFrameMain branches. Missing-camera and zero-count paths explicitly clear counts. Do not mutate GlobalLayout from Main.
Publish candidate area/control data in MainLayout, using a separate candidate
receiver path; baseline continues to use its existing Global lighting areas.
Keep execution on the existing graphics queue. Per-framebuffer source/hull
buffers follow existing Begin/Render/End zero-count publication and fences.

Every rendered world frame refreshes BOTH candidate layers, at current pose,
without temporal accumulation, reprojection or stagger. No work runs on hidden
world/minimized frames. Baseline keeps its existing shipped cadence for honest
end-to-end comparison. Charge the entire every-frame candidate cost; never
compare just a quadrant or amortize it over baseline cadence. Baseline and
candidate selected modes run only their own lighting work; skipped branches
publish zero indirect draws/dispatches. Prototype UI selectors are `Lighting
Prototype` (Baseline/HRC) and `HRC Grid` (256/512), in existing lighting Tweaks;
changing grid requests texture/pipeline recreation. They default to Baseline
and 256 and are developer controls, not production quality settings.

Sequence: clear inputs; render padded raw terrain, derive low-layer extinction and render ship occupancy; additive
source deposit with opacity clipping; for each layer/quadrant initialize short
intervals, construct upward, reconstruct downward, cross-filter and store that
quadrant; resolve presentation/ambient; scene sampling. Instrument input,
initialization, upward, downward/filter, resolve and whole candidate lighting.
Include source preparation GPU work, terrain/hull raster, adapter and any debug
work in their proper timing cohorts. CPU hull/source preparation is reported
separately with frame CPU/FPS consequences.

Order color attachment writes before compute reads, every compute write before
the dependent read, final compute writes before fragment sampling, and previous
frame fragment/compute reads before next-frame clear/write. Explicitly cover
same-layout dependencies, scratch reuse between quadrants/layers and output
reuse across frames. Own all descriptors, readback usages and allocations through
normal texture/pipeline destruction and partial/device recreation. Disable and
re-enable, empty active cells, rewind, footprint/basis change and world hide/show
must never expose prior candidate data. Current-only rebuilding needs no history.

## Resource inventory proof

This algebra preserves every resource category; it is payload arithmetic, not measured allocation or a feasibility verdict.

| Resource | Count/payload |
|---|---|
| Intervals | One RGBA16F image, level n count (N/2^n)*N*(2^n+1). Each level uses integer N+N/2^n rows; total height N*(log2(N)+3)-1, hence 2815/6143 at 256/512. Bytes 8*N*height. |
| Reconstruction | Two N*N RGBA16F images: 16*N*N bytes. |
| Emission | Two N*N RGBA16F images: 16*N*N bytes. |
| Occupancy | Two N*N R32_UINT: 8*N*N bytes. |
| Raw elevation | One N*N R32_SFLOAT: 4*N*N bytes. |
| Terrain extinction | One low-layer N*N R32_SFLOAT: 4*N*N bytes; high layer uses uniform zero. |
| Linear EWNS | Three RGBA16F per layer, two layers: 48*N*N bytes. |
| Presentation | Three directional plus one ambient R8G8B8A8_UNORM per layer: 32*N*N bytes if N*N; publish actual extents. |
| Diagnostics | Optional indexed reference, HRC debug output and readback; inventory actual formats/extents. |
| Source/hull buffers | Every framebuffer slot, counts/ranges/vertices and actual capacity. |
| Baseline | All simultaneous retained baseline images/buffers. |
| Lifetime overhead | VMA alignment, staging, retired allocations and recreation overlap, measured live/peak. |

Core intervals plus reconstruction/emission/occupancy/raw/extinction/linear EWNS are about 11.50/48.00 MiB at 256/512, excluding presentation and every subsequent category. N*N presentation adds 2/8 MiB. No memory ceiling or throughput prediction follows. Actual device formats/limits and allocations remain runtime checks. Source quadrature and accuracy targets are unvalidated proposed gates.

## Source reuse and provenance

Pin https://github.com/Yaazarai/Volumetric-HRC/tree/ed153d18f284acce9515880614b1091e9669adcf . Root LICENSE is Unlicense. The selected source paths are HolographicRadianceCascades/shaders/Shd_Extensions/Shd_Extensions.fsh and HolographicRadianceCascades/shaders/Shd_MergingCones/Shd_MergingCones.fsh. FrustumSeed and FluenceSum at the same subtree/commit are adaptation-boundary references, not copied engine input/output stages.

Smallest source reuse shape: adapt directly into planned HrcUp.comp and HrcDown.comp and add Lighting/Hrc.LICENSE.txt with full upstream license, exact commit, upstream-to-adapted file mapping and change description. Preserve the engine shader license. No pristine mirror, submodule, GameMaker project/runtime, new rendering framework or ThirdParty edits. 

Verified upstream: Extensions swaps lower/upper indices between completed paths. MergingCones uses
half endpoint-angle weights, ordered composition and completed-merge interpolation on even planes.
FrustumSeed applies pow(rgb,2.2); FluenceSum sums four frusta and applies pow(rgb,1/2.2). Replace
those gamma/scalar-output assumptions. Do not treat summed reconstructed alpha as interval T. The
algebra/directions and transport-accuracy acceptance rows independently verify engine
normalization/indexing/parity.

Supplied upstream research excludes ppmpreetham/hrc-gi as a primary source because zero-extinction emitter accumulation and four orientations are incomplete. amitabha and AdrianMargel remain math cross-checks only, with reusable licenses unestablished; no source copying. Paper benchmarks are not engine estimates.

## Critical files

Existing integration anchors below bound the implementation; support shader
names listed under In scope are new. Read their local AGENTS.md before editing.
No deterministic collection columns, network messages or pack changes are needed. On successful implementation landing rewrite ScreenSpaceRadianceCascades.md's separate-Plan citation to the current implementation; on rejection remove its pending claim. Its existing link remains correct during this documentation-only finalization.

## In scope

- `Engine/Source/Graphics/Render/LightingUniforms.cpp`: baseline publication branch
  and candidate footprint/control publication; `MainUniforms.cpp`/`Render.h`:
  candidate Main publication and shared render-only state.
- `Engine/Source/Frame/Collections/PointLights/PointLightsRender.cpp` and
  `Engine/Source/Frame/Collections/AreaLights/AreaLightsRender.cpp`: candidate true-XY source records, layer
  assignment and footprint culling; corresponding collection headers only for
  client render staging, never new simulation data.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/{Fighters/FightersRender.cpp,Omnis/OmnisRender.cpp,Battleships/BattleshipsRender.cpp}`:
  same-frame model prism publication and model receiver-range references only.
- `Engine/Source/Graphics/Render/Render.h` and new `PlanarLighting.cpp` in that
  directory: render-only packed-hull extraction, transformation, projected
  convex hull and bounded per-framebuffer source/hull staging. Capacity derives
  from existing rendered ship/source capacities, with zeroed unused counts.
- `Engine/Source/Graphics/Managers/{RenderTargetTextures.h,RenderTargetTexturesLighting.cpp,WorldLightingShadowPipelines.h,WorldLightingShadowPipelines.cpp,PipelineManager.h,PipelineManager.cpp,CommandBufferRecordMain.cpp}`:
  candidate resources, descriptors, pass recording and A/B indirect gating.
- `Engine/Data/Shaders/ShaderMainLayout.h`: MainLayout candidate publication; `ShaderLayoutsBase.h` aggregates CPU/GLSL layouts and model/candidate binding contracts; `Engine/Data/Shaders/Model/ModelCommon.h`, `Model/ModelStatic.vert` and
  `Model/ModelSkinned.vert`:
  matching hull reference delivery where model layout is owned there.
- `Engine/Source/Graphics/Objects/ModelPipeline.cpp::Create`: appended model candidate bindings, kiModelAdditionalDescriptors, vector capacity/count and valid static/skinned/material/shadow variants.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesRender.cpp` and `Projects/ModelViewer/Source/ModelViewerApplication.cpp`: initialize empty hull references for excluded shared-layout producers only.
- New `Engine/Data/Shaders/Lighting/{HrcCommon.h,HrcSource.vert,HrcSource.frag,HrcBlocker.vert,HrcBlocker.frag,HrcTerrain.frag,HrcInitialize.comp,HrcUp.comp,HrcDown.comp,HrcResolve.comp,HrcReference.comp}`:
  inputs, raw elevation/low-layer extinction preparation, interval construction, reconstruction/filter, adapter and diagnostic
  bounded sampled direct DDA oracle described under Verification and acceptance.
  Reuse terrain elevation vertex/placement input and existing
  local presentation math; move shared Uchimura math to HrcCommon only if needed
  without changing baseline results.
- `Engine/Data/Shaders/{ShaderFunctions.h,Model/Model.frag,Terrain/Terrain.frag,Water/Water.frag}`:
  mode-gated true-XY candidate sampling, ship exterior sampling, existing height
  and material responses; baseline calls preserve existing behavior.
- `Engine/Source/Ui/{LightingWrappersBase.h,LightingWrappersBase.cpp,Screens/TweaksScreen/TweaksScreenLighting.cpp}`:
  the two developer selectors and one analytical-fixture selector (Off/Empty/
  Uniform/SourceHull/Gap/TinyMoving/Edge), with raw field debug selection only.
  Analytical fixtures supply shader inputs, never simulated entities. Diagnostic
  `HRC Blockers` and `Baseline Shield Deposits` checkboxes allow blocker-disabled
  and source-matched references without hiding geometry; both default enabled. HRC Blockers=false zeroes ship occupancy and terrain sigma, retaining source data; analytical fixtures separately isolate terrain extinction from ship masks.
- `Engine/Source/Profile/{ProfileManager.h,ProfileManager.cpp}` and
  `Engine/Source/Agent/AgentCommandsClientGeneric.cpp`: named GPU timing rows and
  a narrow client `query_profile` extension reporting candidate allocation bytes,
  live/peak VMA allocation bytes, configuration and frame-coherent cumulative
  lighting/scene timestamp sums/frame count for the selected mode. The bounded
  comparison accumulator resets on mode/grid change, discards its first 300
  completed world-frame results, accumulates the next 1000 then freezes. Sums
  include zero work on skipped baseline frames and advance only when complete
  matching-frame GPU results become available. Publish frame count and completed
  status so polling cannot lose or double-count frames. Read-only
  diagnostics only; no new capture command or timer percentile framework.
- Debug name registration in `RenderTargetTexturesLighting.cpp` and `Engine/Source/Graphics/Screenshot.cpp::{ResolveRenderTarget,FormatName,kpcValidDumpNames}`: `HrcEmission`,
  `HrcOccupancy`, `HrcIntervals`, `HrcResolved`, `HrcReference`, using layer as
  index except interval scratch (index 0, last processed quadrant/layer).
  `HrcReference` holds only indexed FP32 receiver samples, never a full-grid oracle.
  Explicitly expose raw R32_UINT occupancy, layer/RGB channel selection and target extents/formats. Its raw dump records actual width, height, format, completed sample count, snapshot/world-frame identity, footprint/pitch/N, D/profile inputs and scratch layer/quadrant;
  the run manifest maps each sample index to physical XY, layer, lattice/targeted
  role and validity/exclusion reason, with matching sampled HRC values.
- `Engine/Data/Shaders/Lighting/Hrc.LICENSE.txt`: full pinned upstream Unlicense, exact commit, upstream/adapted file mapping and changes; no license-unknown source copying or ThirdParty edits.
- `Engine/Source/Graphics/Graphics.cpp`: only completed-frame submission/query association if ProfileManager cannot own it alone; preserve rendering behavior.
- `Documents/Plans/Engine/ScreenSpaceRadianceCascades.md`: successful implementation landing citation update or rejected experiment claim removal only.
- Necessary shader pack discovery and project/filter membership for precisely
  these new files through `/update-vcxproj`; owning AGENTS.md lighting/layout/
  publication notes through `/update-claude-docs`; harness command docs solely
  for the narrow diagnostics extension. No generic harness feature expansion.

## Out of scope

Production selection/removal of baseline; conventional RC, JFA/distance fields,
3D/vertical transport, additional layers, temporal filtering, multibounce GI,
glossy transport, exact mesh silhouettes, shield transport, new asset formats,
simulation edits, subscription expansion, async compute, general renderer
refactoring and new unit tests. Do not change baseline lighting tuning/cadence
to improve the comparison. `RemoveLightingHeldFlag.md` overlaps only the
baseline publisher; it is a simplification with no functional prerequisite.
There is no dependency edge; preserve its result if it lands first.

## Timing evidence sequence

P0 before the first edit: untouched baseline fixtures/source-off images, GPU/driver/build/settings manifest, FPS and existing timer snapshots. These preserve regression evidence but cannot settle exact completed-frame A9.

P1 first implementation work: only existing scoped read-only accumulator and query_profile fields,
at GpuRead's submission/query handoff. Associate selected mode/grid generation and one world-frame
submission with ready GPU results; consume once, keep unavailable samples pending, distinguish
zero/skipped work from unavailable timestamps and exclude UI-only frames/LogTimers rereads. Inspect
fence/query-reset order; Graphics.cpp changes only if needed for profiling association. Preserve
rendering/cadence/shaders.

P2 audit diagnostics-only diff and verify unchanged baseline output against P0; collect exact
baseline 300-warmup/1000-completed-frame cohorts before transport/receiver edits. P3 compare matched
alternating baseline/candidate cohorts in the same diagnostic-enabled final executable. The <=2x
gate, five cohorts per scene/mode, full-frame reporting and positive receiver-cost charge remain
unchanged. Invalid instrumentation is fixed in scope, never converted into a false pass.
Compile/harness owns future execution; no build or runtime verification has occurred for this Plan.

## Verification and acceptance

Execute builds through `/compile`: DataPacker Release|x64 for shaders and
BrokenEngineSandbox client and server Debug|x64 and Release|x64 for shared-layout
and affinity checks; ModelViewer Release|x64 verifies the excluded shared-layout producer. Run `/agent-harness` for every live operation; its command
reference owns transport/launch syntax. The request names and params below are
actual commands, not shell commands. Save the completed run manifest, raw dumps,
screenshots, timings and verdict under the implementation session's Temp evidence
directory; final evidence must identify build/GPU/driver and every cohort input.
Before any verification run, estimate the full planned verification duration
from cohort counts, frame counts and observed throughput. This Plan requires
user approval before any run expected to exceed 10 minutes,
offering a shorter cohort spot-check. A spot-check records its reduced scope and
does not satisfy unexecuted acceptance criteria.

1. Start from a fresh/load-paused server (`load {"pauseAfterLoad":true}`);
   `create_cell`/`edit_frame` before any injection. Use client
   `set_client_grid_coord {"coord":[q,r]}` and `describe_scene {}` to establish
   framing and unit positions. Use `cell_coordinate_probe {"coord":[q,r]}` to
   select and record an actual island/cliff placement; do not assume origin is
   a cliff. `describe_ui {}`, `click` and `set_slider` control camera/Tweaks;
   wheel input controls zoom. There is no invented `set_camera` command.
2. Real hull fixture uses server `inject_payload`, for example
   `{"entries":[{"coord":[0,0],"type":"SpawnBattleship","vecPosition":[0,0],"vecDirection":[1,0],"orders":0,"navigationDelay":600}],"pauseAfterWriterInput":true}`.
   Spawn a legal `SpawnBlaster` or `SpawnExplosion` near its hull using the
   documented type index and fields from the loaded collection, then unpause
   to consume the queued entry. Use orders 256 for a high-plane ship. Payload
   XY vectors put nonship effects on the low plane; high-plane source coverage
   therefore uses an actual high-plane ship firing sequence or analytical
   fixture, never a nonexistent arbitrary-light injection command. Record
   accepted payloads and post-spawn `query_collection` positions/ids. Pause/step
   using the documented `pause`/`timescale` recipe for captures; run unpaused
   sequences for motion and dense combat. Replays provide identical combat A/B.
3. Capture `screenshot {"format":"png"}` and raw
   `dump_render_target {"name":"HrcResolved","index":0,"raw":true}`;
   repeat for emission, occupancy, reference, both layers, RGB selections and baseline Combine. HrcResolved uses index=layer and channel=RGB component (0/1/2), each carrying EWNS in RGBA; intervals use index=0 with last processed layer/quadrant metadata.
   Enable the diagnostic DDA shader only in quality cohorts, not timing runs.
   It uses FP32 accumulation and integrates identical emission, hard ship masks and raw-terrain/profile/D inputs to the boundary with 4096 uniform
   angle samples per receiver, preserving quadrant separation and source units.
   Select a fixed 16-by-16 stratified receiver lattice across the visible
   rectangle, plus at most 32 documented targeted edge/parity/shadow probes.
   Each receiver index identifies one physical XY sampled on both layers.
   Record their physical XY once and use identical coordinates at both grids;
   sample each candidate field at those coordinates. All selected valid probes
   participate in the metric, including targeted probes; report coverage,
   selected/valid/excluded counts and reasons per layer/grid. This is sampled
   receiver error, never a full-pixel RMS claim.
   Freeze only diagnostic input data: footprint, source/ship/raw-terrain/profile/D snapshot and
   reference inputs remain fixed across capture; do not change simulation.
   Reuse that snapshot for matched candidate samples and the entire oracle.
   Process at most 8 receivers times 256 directions per rendered frame,
   completing 16 angular batches for each receiver group before advancing.
   At most 288 receiver positions require at most 576 rendered-frame batches;
   process the two layers together within that same receiver/direction batch.
   Publish completed/required batch and receiver counts plus completion status.
   Allow at most 120 seconds per fixture, including both layers; an incomplete
   capture fails evidence and produces no partial RMS or acceptance verdict.
   Use analytical fixtures to isolate transport from geometry publication.
4. Run the matrix below at BOTH 256 and 512, fixed camera/scene and common
   footprint, then choose the smallest passing grid; if neither passes, record
   failure. Preserve raw linear outputs for quantitative analysis before local
   presentation mapping; screenshots prove material behavior separately.

| Criterion | Proposed prototype target and required evidence |
|---|---|
| Algebra/directions | Empty field exactly black, finite outputs and T in [0,1]; constant transparent emission matches analytic path lengths within 2%; simultaneous positive j/sigma matches analytic integration within 2% including small tau and clipped diagonals; four rotated source fixtures select their matching EWNS dominant channel. Capture parity/edge probes and initialization/up/down debug data. |
| Transport accuracy | Against the completed sampled direct DDA oracle, linear RGB normalized RMS error <=10% of reference peak over all selected valid unobstructed visible-region receiver probes with reference >=1% of peak. Report sample coverage/counts and exclusions, worst error and images; targeted probes outside the visible rectangle remain explicit edge diagnostics. No full-pixel RMS claim; the target is a proposed gate, not an HRC guarantee. |
| Real hull and terrain | A real hull gives mean shadow luminance <=20% of ship-blocker-disabled reference and source-facing visible hull >=50% of blocker-disabled response. Keep source, sun and camera fixed; isolate local contribution. Distinct actual island/cliff captures demonstrate finite low-layer attenuation against terrain-extinction-disabled reference. Uniform h=1 paths D/2D match analytic attenuation within 2%; high terrain remains transparent. An opaque cliff shadow is not this contract. |
| Receiver limitations | Show concave mesh inside convex hull, two nearby/overlapping ships, tilted hull, grazing source and 1/2/4-texel gaps. Mark false convex blocking and cardinal-boundary approximation in result. No disappearing lit hull, unrelated exterior tap through another blocker, NaN or ambient-floor workaround. Subtexel-gap closure is a declared limitation. |
| Tiny/moving sources | 0.25/0.5/1/2/8-texel sources: deposited area-integral error <=5% against original continuous quad quadrature; moving-source visible-region total linear output varies <=10% between adjacent equal-speed subtexel phases in empty space. Removed source/blocker has no residual after the next rendered world frame. Report Moire, broadening and ringing even when thresholds pass. |
| Heights/materials | No cross-layer transport. Low h=0 terrain and all high terrain have T=1; positive low h follows finite extinction. Terrain/water/model vertices below low, between and above high planes receive at true XY using clamped existing height blending. Show reachable nonzero local light on high terrain and object vertices above/below planes, plus real ship-shadowed samples; no terrain eligibility mask. Verify EWNS normal turns, terrain/water ambient, model cardinal specular, all model deferred smoke reads and terrain/water smoke scaling, unsupported shields, own-hull exterior and vertical overlap limitations. |
| Coverage/continuity | Edge source disappears only at declared finite boundary; no repeated edge streak. Pan, zoom and adjacent cell crossing at equivalent physical sample points change stationary raw lighting <=10% outside newly exposed/occlusion-edge texels. Large jumps, empty cell, disable/re-enable, hide/show, resize/minimize/restore and recreation show no old data; use `presentation_continuity_probe {}` plus recorded candidate areas. |
| Preserved lighting | With local sources off, baseline/candidate scene images match within 1/255 per channel away from animated content; direct sun/shadows/reflections and visible sprites remain. No baseline+candidate duplicate local contribution. Baseline mode remains reproducible. |
| Cost | Candidate complete lighting mean GPU time <=2x baseline complete lighting mean in every matched cohort below. Report maxima and full-frame GPU/CPU/FPS, allocation and peak, including simultaneous A/B resource residency; no asserted p95 from average/max queries. |

For costs, use empty, real hull+source, cliff, layered and dense combat cohorts
on the same GPU, driver, build, resolution, camera, settings, scene/replay and
refresh policy. Disable diagnostics; warm up 300 rendered world frames and
measure 1000 completed GPU frames per cohort, five cohorts per mode
in one process lifetime, alternating modes. Use the completed bounded
`query_profile {}` sum/count result, dividing each sum by its reported count;
mode switching restarts the warm-up and cohort rather than averaging old samples.
Use named timestamp rows to explain the total, including baseline
skipped-refresh frames; do not compare candidate every-frame cost only against
baseline refresh-frame cost. Also charge positive candidate-minus-baseline
scene-draw GPU cost to candidate lighting, to include receiver hull sampling
overhead; retain the full-frame comparison to expose other costs. Report
medians and ranges of cohort means and the worst cohort, GPU maxima,
full-frame GPU time and FPS. A gap within cohort variation is not a measurable
improvement. Record current baseline FPS as
the ideal reference, without inventing a user-approved FPS floor.

For memory, report extent/format/count/payload for every resource, actual VMA
allocation bytes and observed peak device allocation during startup, grid
switch, resize and dense combat, with and without debug resources. Include
baseline overlap and retired allocations. Report device memory-budget extension
availability; if unavailable, allocation statistics still settle allocation
usage, with no claim they measure total physical VRAM residency. No fabricated
memory pass/fail ceiling.

Implementation completion requires all builds/checks clean, reproducible fixture
and measurement evidence for every row, and an explicit pass/fail/unsupported
verdict for each proposed quality/cost gate. Shader defects and invalid evidence
must be fixed inside scope. Correctly measured technique limitations or budget
failure complete the experiment with a recommendation against promotion. Any
production replacement requires a separately approved Plan.

## Risks and invariants

RGBA16F interval radiance/transmittance storage can round near-unity finite
transmittance to one or underflow small values; reconstruction/presentation also
rounds. Keep the selected formats and compare both grids against the raw FP32
DDA oracle. Include uniform weak-extinction long paths, h=0, h=1 and simultaneous
j/sigma fixtures, recording lost attenuation, underflow and grid dependence.
The accuracy gate charges these errors; no unmeasured precision/performance
success or automatic float32 fallback is assumed.

Spatial aliasing from a large world texel, tiny-source Moire, source broadening,
conservative hull blocking, boundary loss, quadrant receiver approximation,
planar height ambiguity and storage bandwidth may prevent promotion. Preserve
these observations in the final result. Do not conceal them with temporal reuse.
All state and controls are client-only; shared CPU/GLSL layouts remain identical
across compilation contexts. Record-once command buffers, Main/Global publication
order, format feature checks, cross-frame hazards and resource retirement are
acceptance invariants. A scope-expanding remedy returns to the user for a new
decision rather than extending this experiment.

## Sources and verified research

- [Original HRC paper v1](https://arxiv.org/abs/2505.02041v1), [PDF](https://arxiv.org/pdf/2505.02041v1): sections 2-4.3, emission/extinction transfer equations and ordered composition; equations 13-21 and Algorithm 1 establish hierarchy/parity. Section 4.3 discusses diagonal-ray overlap, the shifted base resolve and opacity-aware cross filter. Paper benchmarks are not Broken Engine estimates.
- Pinned Yaazarai
  [Unlicense](https://github.com/Yaazarai/Volumetric-HRC/blob/ed153d18f284acce9515880614b1091e9669adcf/LICENSE),
  [Extensions](https://github.com/Yaazarai/Volumetric-HRC/blob/ed153d18f284acce9515880614b1091e9669adcf/HolographicRadianceCascades/shaders/Shd_Extensions/Shd_Extensions.fsh)
  and
  [MergingCones](https://github.com/Yaazarai/Volumetric-HRC/blob/ed153d18f284acce9515880614b1091e9669adcf/HolographicRadianceCascades/shaders/Shd_MergingCones/Shd_MergingCones.fsh)
  are the licensed adaptation inputs.
  [FrustumSeed](https://github.com/Yaazarai/Volumetric-HRC/blob/ed153d18f284acce9515880614b1091e9669adcf/HolographicRadianceCascades/shaders/Shd_FrustumSeed/Shd_FrustumSeed.fsh)
  and
  [FluenceSum](https://github.com/Yaazarai/Volumetric-HRC/blob/ed153d18f284acce9515880614b1091e9669adcf/HolographicRadianceCascades/shaders/Shd_FluenceSum/Shd_FluenceSum.fsh)
  define seed/gamma/scalar-output adaptation boundaries. The full license and adapted file mapping
  belong in the future Hrc.LICENSE.txt.
- [LightingSpread.frag](../../../Engine/Data/Shaders/Lighting/LightingSpread.frag) owns the current low-layer profile; [LightingUniforms.cpp](../../../Engine/Source/Graphics/Render/LightingUniforms.cpp) publishes its inputs. [IslandChainPlacement.cpp](../../../Engine/Source/Frame/IslandChainPlacement.cpp)::kfWaterSpanMeters is the proposed D scale reference, not evidence of baseline-equivalent extinction.
- [PointLightsRender.cpp](../../../Engine/Source/Frame/Collections/PointLights/PointLightsRender.cpp), [AreaLightsRender.cpp](../../../Engine/Source/Frame/Collections/AreaLights/AreaLightsRender.cpp), [PointLight.frag](../../../Engine/Data/Shaders/Lighting/PointLight.frag), [AreaLight.frag](../../../Engine/Data/Shaders/Lighting/AreaLight.frag): texture/packed alpha, UV/clamp/repeat, instance intensity, original size and offscreen staging integration.
- [FlightPlaneProjection.cpp](../../../Engine/Source/Graphics/FlightPlaneProjection.cpp),
  [ShaderFunctions.h](../../../Engine/Data/Shaders/ShaderFunctions.h),
  [Model.frag](../../../Engine/Data/Shaders/Model/Model.frag),
  [Terrain.frag](../../../Engine/Data/Shaders/Terrain/Terrain.frag) and
  [Water.frag](../../../Engine/Data/Shaders/Water/Water.frag) own receiver projection,
  height/material response and all retained smoke paths.
  [LightCombine.comp](../../../Engine/Data/Shaders/Lighting/LightCombine.comp) owns the separate
  local-field presentation mapping.
- [DataFile.h](../../../Common/DataFile.h)::ModelOutline, [Render.h](../../../Engine/Source/Graphics/Render/Render.h)::ComputeModelShadowBounds, [ExportModel.cpp](../../../DataPacker/Source/ExportJobs/ExportModel.cpp), [ConvexHull.h](../../../Common/Math/ConvexHull.h)::BuildConvexHull and [FlightPlanes.cpp](../../../Engine/Source/Frame/FlightPlanes.cpp)::NearestPlaneIndex establish packed bounds, baked alignment, hull reuse and ties.
- [MainUniforms.cpp](../../../Engine/Source/Graphics/Render/MainUniforms.cpp)::RenderFrameMain and
  [render ordering](../../../Engine/Source/Graphics/Render/AGENTS.md#ordering-and-publication) own
  both post-EndFrameInterpolateRender count publications.
  [ShaderMainLayout.h](../../../Engine/Data/Shaders/ShaderMainLayout.h)::MainLayout,
  [ModelPipeline.cpp](../../../Engine/Source/Graphics/Objects/ModelPipeline.cpp)::Create and
  [Screenshot.cpp](../../../Engine/Source/Graphics/Screenshot.cpp)::{ResolveRenderTarget,FormatName}
  are actual layout/descriptor/dump owners.
- [MissilesRender.cpp](../../../Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesRender.cpp) and [ModelViewerApplication.cpp](../../../Projects/ModelViewer/Source/ModelViewerApplication.cpp) are excluded shared-layout producers requiring empty hull references.
- [ProfileManager.cpp](../../../Engine/Source/Profile/ProfileManager.cpp)::GpuRead, [Graphics.cpp](../../../Engine/Source/Graphics/Graphics.cpp) and [AgentCommandsClientGeneric.cpp](../../../Engine/Source/Agent/AgentCommandsClientGeneric.cpp)::query_profile own the narrow completed-frame diagnostic association. Current/average/max snapshots alone cannot establish completed-frame cohorts.
- [Engine harness commands](../../../.agents/skills/agent-harness/references/command-reference.md) and [BES server commands](../../../Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md) own future runtime operations. No runtime/build evidence is claimed.

## Execution card

Execution card:
### What does this plan do?
Build one opt-in two-layer native Vulkan HRC comparison by adapting licensed
Yaazarai kernels, with finite low-layer terrain extinction, hard ship prisms and
actual-XY terrain/water/model receivers above and below both planes.
### Why this is good for the codebase
It measures a bounded transport technique against shipped lighting using existing
source assets, packed hulls and renderer ownership. A measured negative result
settles the experiment without requiring production replacement.
- Goal: one complete evidence-backed 256/512 comparison; promotion requires every proposed quality/cost gate.
- Out of scope: Out of scope above; no production replacement, additional backend, simulation/pack changes, calibration system or unit tests.
- Tier trigger: future implementation is Tier 3: rendering architecture, GPU lifetime/synchronization and shared CPU/GLSL contracts. Saving the finalized Plan authorizes no implementation.
- Interfaces and invariants: source-facing EWNS; independent planes; finite terrain sigma0 and sigma1=0; exact simultaneous j/sigma integration; separate hard ship occupancy; true-XY receivers with existing clamped height blend; RenderBasis only; client-only non-CRC state; Main candidate publication finalized after EndRender in both branches; zero-count clears; current-frame complete resources; unchanged baseline cadence.
- Acceptance checks: every Verification and acceptance row, full resource inventory, timing sequence and required build; oracle remains at physical P with FP32 accumulation, charging shifted-base and storage errors. Above/below receiving and every smoke read require actual captures. No build/runtime evidence exists yet.
- Roles: main owns future approval/workflow; implementers own bounded changes, assumption audit and propagation; independent reviewers own technical plan/code/shader/coherence/adversarial reviews; builder owns compile; harness worker owns live verification. Licensed kernel adaptation and finite terrain are settled; no further user choice remains. Implementation is deferred to a future claimed session.
