<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T23:43:25.725Z","dependsOn":[]} -->
# Planar Holographic Radiance Cascades Comparison Prototype

## Context and authority

Build one bounded client-render experiment replacing the selected local-light
spread contribution with Holographic Radiance Cascades (HRC). Retain the current
renderer temporarily for A/B comparison. This Plan is the executable successor
to the former planar feature proposal; the [screen-space alternative](ScreenSpaceRadianceCascades.md)
is a separate executable Plan. Saving this Plan authorizes no implementation; its claim still
requires user approval under the Change Workflow. Implementation is Tier 3
(render architecture, GPU resources, shader and receiver contracts).

Recorded user decisions: comparison prototype; HRC only; the existing two flight
planes are independent transport layers; no cross-layer transport; terrain
height and packed convex-hull/vertical-bound ship blockers are acceptable.
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
Visible sprites continue through their existing independent scene draws.

Define emission density j(x,y) as existing source color times texture RGB/alpha
times lighting intensity, in artistic linear RGB per world metre of ray travel.
This deliberately defines new transport units; it does not claim photometric
equivalence with spread controls. The source budget Q is the area integral of j
over its original lighting quad. Overlapping sources add j; sources do not
absorb. Empty cells have T=1. Opaque hull/terrain cells have T=0 and j=0,
including sources inside those blockers. A DDA segment of world length d in an
empty/emitting cell returns (j*d,1); the first opaque cell returns (0,0).
Compose near/far as (Ln+Tn*Lf,Tn*Tf). No reflection, bounce iteration, ambient
injection, existing spread snapshots, compressed deposits or scene color enters
transport. Never trace the final UNORM combine output.

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
water local-light values remains; this adds no particle-lighting system.

Terrain, live BES ships and water retain their current material lighting
responses, height blending, cardinal model specular approximation and ambient
scaling, subject to the terrain per-plane visibility policy below. These are
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

Use the full finite footprint for transport; no arbitrary additional range or
distance attenuation. Trace to its boundary, where incoming L=0 and T=1.
Outside samples are black, never clamped copies of an edge source. Restrict
quality comparisons to the visible rectangle unless testing the edge itself.
Sources entirely outside the footprint contribute zero. Include all existing
subscribed render-interpolate cells whose source or blocker bounds overlap the
footprint, even offscreen; visible-sprite frustum culling must not remove
transport contributors. Do not expand subscriptions or promise unseen cells.

Source centre Z selects the nearest existing flight plane using `FlightPlanes`'
current tie rule. Never duplicate a source across layers. A terrain sample
blocks a plane when raw terrain elevation is at or above that plane. Render raw
terrain elevations over the entire padded footprint using existing packed
heightmaps and placement buffers; the visible-area elevation texture and sun
shadow map are insufficient. Preserve submerged raw elevations.

Terrain receivers sample their true XY. At or above a plane, terrain receives
no local HRC contribution from that plane because it occupies the opaque field;
above both planes it receives no local HRC, while direct sun/moon and IBL remain.
Record unlit high terrain as the user-selected prototype limitation. Do not add
terrain boundary sampling or an ambient floor. Preserve height blending of the
remaining per-layer contributions; measure and report this limitation rather
than classifying the expected darkness as a shader bug.

For live Fighters, Omnis and Battleships, obtain `ModelOutline` through the
packed model header's `OutlineOffset`, as `ComputeModelShadowBounds` does.
Transform the vertices at both packed MinZ and MaxZ with the same remaining
interpolated pose/scale as the rendered instance. Authored alignment is already
baked into the packed hull: do not apply it twice. Project the transformed prism
to a conservative XY convex hull and transformed min/max Z; rasterize it in each
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
position. This applies only on a layer the ship blocks. For nonblocking layers
use the fragment's true XY. Resolve ambient from these four corrected channel
samples, preserving current height blending and downstream material equations.

Use four nearest exterior field taps with bilinear weights, discarding opaque
taps and renormalizing surviving weights; if none survive return black. Opaque
taps include terrain and other ships. Do not scan farther through another
blocker, erase a hull from transport, or add an ambient floor. Keep model pose,
blocker hull and receiver range from the same Main publication frame.

This rule deliberately shades convex outer boundaries rather than exact inset
mesh surfaces. It cannot resolve light inside concavities, overlapping hulls,
subtexel gaps or angular variation within a quadrant. Those are explicit test
limitations and promotion risks, not open architecture choices. Demonstrate a
lit visible source-facing hull and a dark region behind the same hull together;
failure ends the comparison with a negative result rather than silently adding
mesh tracing or another receiver backend.

### HRC construction and EWNS adaptation

Implement the paper's sections 3–4.3 and Algorithm 1, using the reference
functions linked under Sources to cross-check math. Four 90-degree quadrants
look toward source directions centred on +X, -X, +Y, -Y; rotate grid coordinates
accordingly, including texture Y sign. Thus a source east of a receiver fills E,
as `DirectionalLighting` requires. Do not reverse this as an outgoing direction.

For a quadrant, level n uses parallel probe stride 2^n, transverse stride 1,
2^n cones and 2^n+1 interval directions with endpoint offsets
v_n(k)=(2^n,2k-2^n). Levels n=0 through log2(N) are retained. Directly DDA-trace
levels 0,1,2; compose higher intervals upward, using straight two-segment paths
for even k and the average of the two bent converging paths for odd k. Keep
ordered radiance/transmittance composition. Downward reconstruction uses the
paper's distinct odd/even probe cases: even probes blend same-position higher
cones with their extended merged estimate; never just reuse the higher cone.
Use the paper's forward one-pixel base offset and opacity-aware cross filter.
Use the paper as authority where reference TODOs/alternative base code differ.

Weight cones with the difference of atan edge angles. Carry integrated RGB
angular radiance, never angular-scale transmittance. Divide each reconstructed
quadrant's integral by pi/2 exactly once to obtain its directional average.
Keep four outputs, mapped to EWNS, rather than summing into scalar fluence.
The reference uses normalization by tau; do not import that extra normalization.
Topmost absent radiance and out-of-grid lookups are zero; absent interval
transport is (0,1). Clip direct DDA intervals at the footprint. Check parity,
rotations, base offset and boundary independently with the reference shader.

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
separate raw elevation R32_SFLOAT, the shared interval hierarchy, reconstruction
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
source and ship-pose CPU publication. Do not mutate GlobalLayout from Main.
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

Sequence: clear inputs; render padded raw terrain and ship occupancy; additive
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

## Critical files

Existing integration anchors below bound the implementation; support shader
names listed under In scope are new. Read their local AGENTS.md before editing.
No deterministic collection columns, network messages or pack changes are needed.

## In scope

- `Engine/Source/Graphics/Render/LightingUniforms.cpp`: baseline publication branch
  and candidate footprint/control publication; `MainUniforms.cpp`/`Render.h`:
  candidate Main publication and shared render-only state.
- `Engine/Source/Frame/Collections/PointLights/PointLightsRender.cpp` and
  `AreaLights/AreaLightsRender.cpp`: candidate true-XY source records, layer
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
- `Engine/Data/Shaders/ShaderLayoutsBase.h`: CPU/GLSL Main/model and candidate
  binding/layout contract; `Model/ModelCommon.h`, `Model/ModelStatic.vert` and
  `Model/ModelSkinned.vert`:
  matching hull reference delivery where model layout is owned there.
- New `Engine/Data/Shaders/Lighting/{HrcCommon.h,HrcSource.vert,HrcSource.frag,HrcBlocker.vert,HrcBlocker.frag,HrcTerrain.frag,HrcInitialize.comp,HrcUp.comp,HrcDown.comp,HrcResolve.comp,HrcReference.comp}`:
  inputs, interval construction, reconstruction/filter, adapter and diagnostic
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
  and source-matched references without hiding geometry; both default enabled.
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
- Debug name registration in `RenderTargetTexturesLighting.cpp`: `HrcEmission`,
  `HrcOccupancy`, `HrcIntervals`, `HrcResolved`, `HrcReference`, using layer as
  index except interval scratch (index 0, last processed quadrant/layer).
  `HrcReference` holds only indexed receiver samples, never a full-grid oracle.
  Its raw dump records actual width, height, format and completed sample count;
  the run manifest maps each sample index to physical XY, layer, lattice/targeted
  role and validity/exclusion reason, with matching sampled HRC values.
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

## Verification and acceptance

Execute builds through `/compile`: DataPacker Release|x64 for shaders and
BrokenEngineSandbox client and server Debug|x64 and Release|x64 for shared-layout
and affinity checks. Run `/agent-harness` for every live operation; its command
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
   repeat for emission, occupancy, reference, both layers and baseline Combine.
   Enable the diagnostic DDA shader only in quality cohorts, not timing runs.
   It integrates the identical raster inputs to the boundary with 4096 uniform
   angle samples per receiver, preserving quadrant separation and source units.
   Select a fixed 16-by-16 stratified receiver lattice across the visible
   rectangle, plus at most 32 documented targeted edge/parity/shadow probes.
   Each receiver index identifies one physical XY sampled on both layers.
   Record their physical XY once and use identical coordinates at both grids;
   sample each candidate field at those coordinates. All selected valid probes
   participate in the metric, including targeted probes; report coverage,
   selected/valid/excluded counts and reasons per layer/grid. This is sampled
   receiver error, never a full-pixel RMS claim.
   Freeze only diagnostic input data: footprint, source/blocker snapshot and
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
| Algebra/directions | Empty field exactly black, finite outputs and T in [0,1]; constant transparent emission matches analytic path lengths within 2%; four rotated source fixtures select their matching EWNS dominant channel. Capture parity/edge probes and initialization/up/down debug data. |
| Transport accuracy | Against the completed sampled direct DDA oracle, linear RGB normalized RMS error <=10% of reference peak over all selected valid unobstructed visible-region receiver probes with reference >=1% of peak. Report sample coverage/counts and exclusions, worst error and images; targeted probes outside the visible rectangle remain explicit edge diagnostics. No full-pixel RMS claim; the target is a proposed gate, not an HRC guarantee. |
| Real hull and cliff | In a fixed same-height source/blocker scene, mean shadow luminance <=20% of a blocker-disabled reference, while source-facing visible hull receives >=50% of its blocker-disabled response. Keep source, sun and camera unchanged; measure local contribution separately. Cliff and hull require distinct actual geometry captures. |
| Receiver limitations | Show concave mesh inside convex hull, two nearby/overlapping ships, tilted hull, grazing source and 1/2/4-texel gaps. Mark false convex blocking and cardinal-boundary approximation in result. No disappearing lit hull, unrelated exterior tap through another blocker, NaN or ambient-floor workaround. Subtexel-gap closure is a declared limitation. |
| Tiny/moving sources | 0.25/0.5/1/2/8-texel sources: deposited area-integral error <=5% against original continuous quad quadrature; moving-source visible-region total linear output varies <=10% between adjacent equal-speed subtexel phases in empty space. Removed source/blocker has no residual after the next rendered world frame. Report Moire, broadening and ringing even when thresholds pass. |
| Heights/materials | Low and high source scenes show no transport in the other layer; terrain below a plane never blocks it. Preserve receiver height interpolation of available contributions. Verify EWNS normal turns, flat terrain below a plane and water ambient, model cardinal specular, smoke's existing ambient scaling, and unsupported shields with screenshots. Measure terrain at/above each plane receiving zero local HRC from that plane, including above both with no local HRC but sun/moon and IBL retained; record this as a prototype limitation, with no terrain boundary sampling or ambient floor. |
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

- [Holographic Radiance Cascades paper](https://arxiv.org/pdf/2505.02041),
  sections 3–4.3, equations 13–21 and Algorithm 1: parallel-only probe decimation,
  slope bins, interval hierarchy, parity reconstruction, base offset and filter.
  Full paper reviewed during primary research; small-source Moire, blur and poor 3D
  scaling are stated limitations. Published CUDA/LuisaCompute timings are not
  Broken Engine estimates.
- Primary [reference grid/reconstruction](https://github.com/entropylost/amitabha/blob/main/src/lib.rs),
  `Grid::next`, `merge`; [interval construction](https://github.com/entropylost/amitabha/blob/main/src/trace.rs),
  `StorageTracer::trace`, `merge_up`; [composition/units](https://github.com/entropylost/amitabha/blob/main/src/fluence.rs),
  `FluenceType::over`, `restrict_angle`; [storage](https://github.com/entropylost/amitabha/blob/main/src/storage.rs).
  These cross-check paper math; TODOs and alternate reconstruction do not override
  the paper. The proposed flattened Vulkan images are engine adaptation.
- [Original RC manuscript](https://raw.githubusercontent.com/Raikiri/RadianceCascadesPaper/main/RadianceCascades.tex):
  ordered interval composition and hierarchy context. It is not a second backend.
- Engine evidence: `PointLightsRender.cpp`/`AreaLightsRender.cpp` and their deposit
  shaders own source texture/intensity; `FlightPlaneProjection.cpp` and
  `ShaderFunctions.h` prove existing eye-ray projection must migrate on both
  sides; `LightCombine.comp` owns early local-field mapping, separately from
  final scene HDR resolve. `Common/DataFile.h::ModelOutline`,
  `Render.h::ComputeModelShadowBounds` and `ExportModel.cpp` establish packed
  hull availability, baked alignment and inset-hitbox mismatch. No pack change.
- Runtime verification owners: [engine harness commands](../../../.agents/skills/agent-harness/references/command-reference.md)
  and [BES server commands](../../../Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md).
  `query_profile` already supplies GPU averages/maxima, not memory or percentile
  distributions; the narrow diagnostics addition above closes that evidence gap.
