# SSRT3 Screen-Space Indirect Lighting Experiment

Revisit When: [stage 1](ScreenSpaceIndirectLightingInputs.md) has landed and
the user schedules the transport experiment.

Note: stage 1 now follows the user's Option 2, so it no longer provides a CPU
inverse view-projection or other reconstruction data in MainLayout, per-slot
input sets or a toggle that allocates and releases them, paired cohort-recorder
timing (it adds one GpuTimers row; the lighting baseline is the sum of the four
existing lighting rows), or the Record button, status, Tweaks controls, source
multiplier and depth-conversion viewers. Read every statement below that
assumes those (the MainLayout inverse, per-slot ownership and toggle hook, the
landed Record button and cohort accounting) against stage 1's recorded design.

## Context

The author records the selected SSRT3 visibility-bitmask core and split into
two future stages. The [input stage](ScreenSpaceIndirectLightingInputs.md)
supplies inspectable opaque inputs, one resident target set and baseline/input
timing. This second stage supplies transport and the complete quality/cost
verdict. Input-only evidence cannot settle any complete-effect gate below.

At authoring baseline 379a17c99d379c1e6ad4e3ce3ad1a46977c0fc08,
`Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp:59` enters
`RecordImageRenderPass` without this transport, and
`DataPacker/Source/Attribution.cpp:145` begins `BuildPendingCopies`, whose
existing library discovery does not deliver an SSRT3 shader notice. The gap is
the explicitly deferred implementation, rather than an observed renderer bug.

The author records these already selected decisions: full-resolution opaque
inputs, half-resolution gather, no history/offscreen recovery/multibounce,
retained current lighting, and useful four-output-pixel sources at RTS framing.
Match actual solid non-BLEND visibility and existing backface culling:
MASK-authored materials remain solid and doubleSided remains unsupported.
These choices supersede earlier cutout/two-sided and custom cascade promises.
Numerical algorithm settings and gates below are author recommendations for
execution approval, not user quotations or measured feasibility. Target UHD
on this PC's actual Vulkan GPU, identified at runtime. Neither stage has
executed; future Tier-3 plan reviews and approval remain required.

## Design

Implement the selected SSRT3 shader-core adaptation using engine-native view,
texture and pipeline support. Read the landed input implementation first and
confirm the following contracts; missing contracts are a blocker to resolve
with the executing session, not permission to substitute reduced inputs.

### Input and retained-renderer contracts

Inputs are output-exact, single-sample sampled depth plus RGBA16F linear
emission and RGBA16F geometric normal XYZ/validity A, rerasterized together
before `RecordImageRenderPass` after Main preparation. They match real opaque
model/terrain draws, transforms, LOD/elevation, skinning and material ranges.
Clear validity/colors to zero and depth to the current far value. Prefer
sampled D32_SFLOAT with a depth-only view after feature/usage checks; missing
required format support is no-go. Never substitute resolved MSAA/UI depth.
Nearest texel reads preserve silhouettes; no depth pyramids or smoothing.

Sources are decoded model emissive texture times material emissive factor
times `mainLayout.fPbrEmissive`, before fog/exposure in linear HDR. Terrain
emission is zero. Do not feed shaded color, old light fields, sun/IBL, instance
glow or new irradiance back into emission. Source radiance is constant over
the outward geometric-normal hemisphere: reverse-facing samples block and
emit zero. Water, BLEND, shields, particles and light sprites are absent as
sources, blockers and receivers. Retained planar depositors can double-count;
controlled source fixtures therefore have no associated depositor.

Use the CPU inverse of the actual shaken world view-projection, view
orientation/inverse and projection mode already published through MainLayout.
Unproject pixel-center NDC with Vulkan [0,1] depth and the actual negative
viewport-height mapping; divide by w into camera-cell RenderBasis positions.
Use one view-space convention with positive depth away from the eye.
Perspective eye rays vary with position; orthographic rays are constant.
No absolute positions, shader inverse or upstream reversed-Z shortcut.
Geometric normals precede normal mapping and agree with actual culling.
Model-over-world visibly disables the experiment because its depth-clear
presentation does not provide coherent world depth.

Retain current deposit/spread/combine/temporal lighting, direct sun/moon,
shadows, IBL, reflections, local diffuse/specular, smoke/fog and grading.
Fully off omits all prototype work/contribution and releases its resources.
Models add metallic-aware existing diffuseColor*E/pi and terrain adds its
linear layered color*E/pi once before smoke/fog. Self-emission is unchanged.
E is integrated for the stored geometric receiver normal; final normal mapping
cannot rotate it. Record this approximation. No prototype AO scales retained
direct or ambient lighting.

### Gather

Prototype constants are fixed definitions, not a tuning search/UI. Let
p=max(ground-plane visible width/outputWidth, visible height/outputHeight),
d=32p, R=8d, positive view-depth slab thickness t=d/32 and receiver offset
b=d/128. The range deliberately follows zoom. Gather extent is ceil(output/2),
1920x1080 for UHD. Select nearest valid depth in each clamped 2x2 input block,
ties in row-major order. Raw RGBA16F stores E in RGB and representative code
0 invalid or 1..4 in A. Invalid blocks write zero; reload geometry via the code.

Use eight fixed rotations theta_i=(i+0.5)*pi/8, both slice sides, 32 sectors in
one uint and 32 ordered samples per side. Clear one mask per receiver/slice,
shared by both sides. Project P and P+R times normalized view-plane slice
direction with the actual projection to obtain radius rho in output pixels.
Sample a_j=max(j+1,rho*((j+0.5)/32)^2), j=0..31, clipped to rho; skip repeated
texels. Orthographic mode has no perspective depth scaling. Terminate
out-of-viewport, near-clipped or degenerate segments with unavailable
diagnostics, rather than border-clamping sources. Reject samples beyond R or
within b of the geometric-normal-offset receiver. Reject only the originating
texel, never a whole object/depth layer. Background has no slab.

Adapt upstream `ComputeOccludedBitfield`, `HorizonSampling` and `CSMain`
projected-normal/slice construction. Sample S represents finite positive-view-
depth slab [z(S),z(S)+t]. With normalized away-from-eye camera ray q, its back
endpoint is S+t*q/q.z in the positive-depth view convention; orthographic q
is constant. Thus t is depth thickness, not distance along an oblique ray.
Map front/back directions into receiver-hemisphere sectors, clamp endpoints,
remove covered bits and OR coverage even for dark/reverse-facing sources.
Increasing screen-distance order follows upstream and approximates visibility;
it is not exact nearest-ray ordering. Empty intervals cover no bits; endpoint
and full-mask handling never shift a uint by 32. A degenerate projected receiver
normal contributes zero for that slice without normalizing zero.

For each slice let V point from receiver toward eye (constant for orthographic),
D=(cos(theta),sin(theta),0), B=normalize(cross(D,V)), T=cross(V,B),
Np=n-B*dot(n,B), beta=atan(dot(Np,T),dot(Np,V)). Skip degenerate B/Np.
The sectors partition [beta-pi/2,beta+pi/2]; midpoint alpha_k maps to
w_k=cos(alpha_k)*V+sin(alpha_k)*T. Use the same atan convention for sample
endpoint angles, unwrap their shorter interval, intersect it with this
hemisphere (split at the 2*pi boundary if needed), then cover intersected
sectors. Process positive then negative side in increasing j with one shared
mask as upstream. Receiver origin is P+b*n; angles/distances use that origin.

Remove source-normal cosine scaling: source normal only chooses the emitting
hemisphere. Omit shaded-light pyramids, AO/direct-AO, backface emission, HSV
clamp, exposure, fallback and multibounce branches. For each newly covered
sector midpoint w, use max(dot(receiverNormal,w),0)*abs(sin(alpha)), with alpha
the signed angle about V defined above. Normalize all possible weights over all eight
slices to pi independently of visibility/emission, then accumulate newly
covered weights times source RGB: W is the sum of all possible weights and
E=(pi/W)*sum(coveredWeight*sourceRGB), zero when W=0. Source emits only when
dot(sourceNormal,-w)>0, without another cosine factor. Uniform fully represented L gives E=pi*L;
uncovered directions remain zero. This angular quadrature is an adaptation
whose bias the independent reference measures. No radiance distance falloff.
The work bound is 512 attempts per valid receiver, not a performance estimate.

### Filtering and forward composition

Write a separate half-resolution RGBA16F output with one 3x3 spatial filter,
product weights [1,2,1] in X times [1,2,1] in Y, and unchanged center code. Reload representative
geometry through raw codes. Reject invalid neighbors, geometric-normal dot<0.8
or absolute positive view-depth difference>t, then normalize survivors.
Valid center survives; invalid centers receive no hole fill.

Forward shading bilinearly samples four filtered neighbors, reloads their
input geometry and rejects invalid, normal dot<0.8 or view-depth difference>t
against the actual fragment's pre-normal-map geometric normal/depth. Require agreement within t with its nearest
full-resolution input surface for MSAA edge samples. Normalize surviving
weights; none means zero. Keep existing HDR/MSAA resolve. Source loss or
filter leakage faces the gates below rather than authorizing history.

### Ownership, descriptors and diagnostics

Extend actual-framebuffer-slot input ownership with half-resolution raw and
filtered RGBA16F outputs through RenderTargetTextures/TextureManager. No
persistent masks or origin atlas. UHD D32 input plus output payload is
199065600 bytes (189.84375 MiB) per slot, excluding diagnostics/alignment/resize
overlap. Measure actual slot count, format/descriptor limits and all-live VMA
delta/resize peak; this arithmetic proves neither allocation nor speed.
Require RGBA16F sampled/storage support for raw/filter outputs and required
diagnostic usage support. Unsupported format/usage/limits are documented no-go,
never permission for a reduced representation or new device feature.

Reuse the narrow drained Graphics toggle/resize/recreation hook, bindless
epochs, retirement and full model-input variant registration. Sampled-image
arrays may retain all live slots with a dynamically uniform slot index.
Every new storage image is a singleton declaration/descriptorCount=1.
Gather/filter/diagnostic pipelines use `PipelineFlags::kPerCommandBuffer`.
During drained creation/recreation, before recording, update each slot's set
with existing `PipelineDescriptorWriter::UpdateImageDescriptorInSet`, its
resolved binding, VK_NULL_HANDLE sampler, that slot's view, GENERAL layout and
STORAGE_IMAGE type. Include pipeline-only recreation; sampler consumer
registration does not republish output storage descriptors. Existing dispatch
selects the framebuffer set. Preserve buffer routing and device features;
no descriptor helper edit, new flag or storage-image array indexing feature.

Synchronize raster depth/color to gather reads, gather writes to filter reads,
filter writes to forward fragment reads, slot reuse and diagnostic readback
with actual Vulkan 1.2 stages/access/layouts. Preserve Global/Main ordering,
cross-frame particle edges, ImGui fences and HDR/MSAA/submission serialization.
Do not mutate pending sets. Clear/rebuild current data after off/on, hidden
world/resume, empty cells, rewind, camera basis/footprint changes and recreation.

Existing debug selection/dump paths expose raw E, filtered E and final
contribution, plus bounded valid-receiver/source/new-sector/background/
hemisphere-reject/unavailable counts and selected-receiver masks. Reuse one
selected color diagnostic target. Depth inspection copies to R32_SFLOAT color;
never read depth as COLOR. Include transfer usage and raw RGBA16F conversion
where needed. No new harness command or full-image mask atlas. Reference,
debug views and readback are excluded from timing.

### Pinned port and notice delivery

Pin [cdrinmatane/SSRT3](https://github.com/cdrinmatane/SSRT3/tree/b38d82140acf646ee485f9ab16a5fa84906f2231)
at b38d82140acf646ee485f9ab16a5fa84906f2231. Port only the named routines
from `HDRP/Shaders/Resources/SSRTCS.compute` into
`Engine/Data/Shaders/Lighting/ScreenSpaceIndirectLighting*.{comp,h}`.
Replace `PositionSSToVS` with engine reconstruction. No Unity/HDRP dependency,
vendor tree, copied Unity includes or ThirdParty edits. Exclude
`TraceReflectionProbes` and separately sourced library blocks; unresolved
helper provenance blocks copying. The [paper](https://arxiv.org/abs/2301.11376v2)
motivates finite-thickness sectors, not this adaptation's accuracy or speed.

The complete port inventory is:

| Pinned upstream region | Local treatment |
| --- | --- |
| `ComputeOccludedBitfield` (218-228) | Adapt interval/new-sector mask mechanism; explicit empty/full cases replace undefined boundary shifts. |
| `HorizonSampling` (229-294) | Adapt slab angles/shared coverage; engine inputs, fixed schedule and weighted emission replace HDRP dependencies. |
| `CSMain` slice loop (334-352) | Adapt slice/projected-normal construction and two-sided calls; fixed eight rotations, no noise/history. |
| `GetLinearDepth`, `PositionSSToVS`, normal readers (66-117), dispatch/setup/output | Replace with engine reconstruction, RGBA16F normal/validity and slot descriptors. No Unity encoding/reversed-Z path copied. |
| `SpatialOffsets`, `GradientNoise`, `rand`, `GTAOFastAcos`, `MipLevelFromConeAngle`, `TraceReflectionProbes`, AO/fallback/HSV tail | Exclude entirely; GLSL atan/acos and original weight code need no copied externally credited helper. |

Filter, upsample, counters and reference are engine-native integration, not
claims to reproduce HDRP filtering/output normalization. Map local counterparts
even if names change. Any discovered copied helper requires proven provenance;
do not assume Unity includes share the repository license.

Fetch the exact [pinned upstream LICENSE](https://github.com/cdrinmatane/SSRT3/blob/b38d82140acf646ee485f9ab16a5fa84906f2231/LICENSE) when porting. Retain its complete MIT
copyright (2024 CDRIN), permission and warranty text verbatim in
`Engine/Data/Shaders/Lighting/SSRT3-LICENSE.txt`; adapted files reference it.
`Documents/Architecture/ScreenSpaceIndirectLighting.md` records revision,
upstream/local function map, all helpers/adaptations, hashes and notice paths.

In `DataPacker/Source/Attribution.cpp`, extend `BuildPendingCopies` and
`CopyThirdPartyLicenses` to queue that source notice to generated sibling
`Attribution/SSRT3/LICENSE.txt`. Pass the canonical engine-data root already
owned by `FileManager::mpInputDirectories[0]` into the collector. Reuse
`AddPendingCopy` destination validation and `PublishPendingCopies` copying;
preserve `EnsureLocal(OutputRoot::kAttribution)`, change detection, cancellation
and additive semantics. Notice delivery is independent of shader discovery;
do not make .txt a shader job or invent a distribution framework. The delivered
artifact inventory explicitly includes both shader pack and generated notice.

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

- [Main recording](../../../Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp): `Record` input->gather->filter->forward ordering/new scopes; `RecordImageRenderPass` eligible complete-forward timing.
- New `Engine/Data/Shaders/Lighting/ScreenSpaceIndirectLighting*.{comp,h}`: gather/filter/selected diagnostics/reference; `SSRT3-LICENSE.txt`: notice. [ShaderMainLayout.h](../../../Engine/Data/Shaders/ShaderMainLayout.h), ShaderLayoutsBase.h and ShaderFunctions.h: only needed parameters/descriptors/helpers.
- [Model.frag](../../../Engine/Data/Shaders/Model/Model.frag) `main` diffuse/emission/smoke region and [Terrain.frag](../../../Engine/Data/Shaders/Terrain/Terrain.frag) `main` layered color/smoke region: E/pi exactly once before smoke/fog.
- [RenderTargetTextures](../../../Engine/Source/Graphics/Managers/RenderTargetTextures.h) landed input owner, [TextureManager](../../../Engine/Source/Graphics/Managers/TextureManager.cpp) screen create/destroy/retirement, PipelineManager/WorldLightingShadowPipelines compute-creation regions: raw/filter outputs, descriptors/barriers. Reuse [UpdateImageDescriptorInSet](../../../Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp) unchanged;
[BlurLightingTexture](../../../Engine/Source/Graphics/Managers/TextureManager.cpp) demonstrates singleton writes.
- [Graphics](../../../Engine/Source/Graphics/Graphics.cpp) `Refresh`/`RecreateResources` landed hook; [MainUniforms](../../../Engine/Source/Graphics/Render/MainUniforms.cpp) `RenderFrameMain` and model/terrain pipeline bindings only for new parameter/consumer propagation.
- [Screenshot.cpp](../../../Engine/Source/Graphics/Screenshot.cpp) `ResolveRenderTarget`/conversion/`DumpRenderTarget`, RenderTargetTextures debug registration, TweaksScreenLighting/LightingWrappersBase lighting tweaks: fixture/source multiplier, selected masks/counters/raw/final inspection.
- [Profile controls](../../../Engine/Source/Ui/Screens/GraphicsMenuScreen.cpp): `Render` Lighting-to-Smoke region extends landed toggle/cohort controls to the complete effect, with the same existing click/describe_ui route.
- [ProfileManager](../../../Engine/Source/Profile/ProfileManager.cpp) `GpuRead` completed-cohort extension; ProfileManager.h/ProfileRows.h and Main/Global reset ranges: gather/filter/forward accounting.
- [Attribution.cpp](../../../DataPacker/Source/Attribution.cpp) `BuildPendingCopies`, `CopyThirdPartyLicenses`: notice queue/caller; FileManager canonical engine root through existing API. Shader/project membership; `Documents/Architecture/ScreenSpaceIndirectLighting.md` port/result inventory; only owning AGENTS.md descriptions made incorrect.

## In scope

Implement the named gather/filter/forward upsample/composition, resource,
descriptor, lifecycle and timestamp regions plus GraphicsMenuScreen::Render Profile control extensions as one complete experiment. Add
bounded transport diagnostics and independent reference/fixtures below;
correct defects within this mechanism. Deliver exact upstream notice and
provenance inventory. Repeat input/preservation/lifetime checks for the complete
effect. Propagate only correctness-dependent layout/caller/project/docs updates.
In the completing diff, replace the [planar feature document](PlanarHolographicRadianceCascades.md)'s
pending screen-space citation with actual effect toggle/symbol/path, without
claiming promotion on no-go. If that manually executed feature has been
implemented and its document removed, update its surviving live citation only
if one exists; never recreate the removed document.

## Out of scope

Production replacement/removal of existing lighting; material MASK discard or
doubleSided expansion; material/pack format changes; new art/simulation types;
water/shield/transparent/particle/sprite transport; generic light sources;
hidden/offscreen recovery; HRC or custom cascade/merge algorithms; hardware ray
tracing; volumes, distance fields/HZB; stochastic/history/motion-vector paths;
multibounce or indirect specular; new dependencies/harness protocol; scheduler,
simulation, CRC, network/save/replay changes; async compute; unrelated cleanup;
unit tests. Failed gates do not authorize any excluded remedy.

## Fixtures and independent reference

F1: Diagnostic inputs project camera-local primitives into the same full-size
depth/normal/emission contract. Receiver plane z=0, normal +z; square emitter
center (0,0,d/2), side d/2, normal -z, RGB (4,1,0.25); box centered
(d/2,0,d/4), side d/4. Include raised receiver z=d/2 and tilted patches at
0/45/90 degrees; raised variants move emitter to (-d,0,d) to avoid coincident
planes. Fit view/radius and record transforms. Synthetic downward emission
does not prove real-camera visibility. A uniform-radiance-before-visibility
mode tests angular scale separately. Diagnostics cannot replace real raster
integration.

F2: Paused packed emissive ships without an associated planar depositor,
nonemissive ship receivers and actual island cliff/terrain. Input-source
multiplier 0/1/2 leaves geometry/self-emission/old lighting unchanged. Record
actual materials/meshes/streaming/camera. Test 16/8/4/2 output-pixel emission
footprints at intended RTS framing in both camera modes. Full-resolution
presence alone does not establish useful four-pixel lighting. Native effects
demonstrate excluded transport.

F3: Select and log a fixed 8x8 receiver grid per diagnostic view before candidate
evaluation. In each grid cell choose the nearest valid gather representative to
its center using input geometry alone, row-major ties; log that exact full-size
pixel. Raw and final comparisons use that same position, never a different
full-resolution receiver hidden under the half-resolution sample.
Independently implement 32x32 equal-solid-angle hemisphere midpoint
rays about the actual geometric normal over [b,R], with the same emission/slab
t and receiver origin P+b*n. Use full-resolution screen-space segment DDA, not candidate sectors or its
sparse walk. Homogeneous projection/clipping and CPU inverse support both
camera modes. Cap visited pixels at outputWidth+outputHeight+1 after viewport/
near clipping; even a degenerate projected ray tests its depth segment. First
slab hit emits only into its outward hemisphere, otherwise zero. Leaving the
representation before R or exhausting the cap is unavailable; fully represented
background is a zero miss. Choose mu=(a+0.5)/32 and phi=2*pi*(c+0.5)/32, a,c=0..31, mu the cosine about n, with a logged orthonormal tangent basis. E=(2*pi/1024)*sum(Lk*max(dot(n,wk),0)). Intersect ray spans with each visited positive-depth slab; test first hit in increasing ray distance. Reject only the origin texel until leaving it. Dump raw and
final filtered/upsampled values at the same full-resolution positions; count
coverage separately. Never run this reference/readback during timing.

## Acceptance criteria

No runtime setup or quality/cost measurement exists yet. Reproducible fixture
records include exact existing harness requests/JSON, asset CRC/material IDs,
paused tick, camera cell/local pose/projection/extent, transforms/settings and
texture readiness. Record before evaluating candidate; replay unchanged except
for deliberate source multipliers. Debug-only edits/captures run in Debug.
Client Profile cohorts use Profile-supported controls; server-side fixture authoring may use a Debug server while the client remains Profile. An
unavailable fixture is blocked evidence, never permission to time Debug or add
a protocol. The
executing session uses `/agent-harness` for launch, scene/camera/UI controls,
queries, debug dumps and captures. Use existing `inject_payload`, `edit_frame`,
pause/describe/debug/`query_profile`/`dump_render_target`/`renderdoc_capture`
routes; record exact requests, assets/material IDs, transforms, settings,
texture readiness, device/driver/build and completed evidence. Before editing,
capture the landed input-only stage's preservation screenshots/profile/VMA and
excluded-class scenes. Compare exact cost within the same instrumented
executable fully off. Do not treat smoothed latest profile rows as cohorts.

| ID | Required observation |
| --- | --- |
| Q1 Source/integration | Full-resolution nearest opaque inputs match actual forward silhouettes, solid non-BLEND coverage and culling, with no old color/feedback. Zero source produces zero E; doubling produces raw/final linear contribution within 2% of double. Direct/IBL/shadows/self-emission stay intact. Record authored MASK/two-sided limits. |
| Q2 Reference coverage | Each unblocked/blocked/raised/tilted view has >=48/64 valid receivers, >=8 lit in the unblocked counterpart and <=5% unavailable reference directions per valid receiver. Do not silently exclude difficult samples; invalid fixtures fail evidence/no-go. |
| Q3 Response | Uniform E versus pi*L error <=20% across geometric-normal angles. Raw and final luminance-normalized RMS against independent 1024-ray reference each <=35%, denominator max(reference luminance,1% fixture peak). No negative/nonfinite E. Record final-normal-map approximation. |
| Q4 Blocker/height | Where blocked reference <=10% of unblocked reference, final E <=20% of unblocked reference. Raised/tilted surfaces satisfy Q3 without unrelated-depth lighting. Record thin/hidden-layer limits. |
| Q5 Source sizes | Useful on/off response on real terrain AND model receivers, with cliff occlusion, at 16/8/4 output-pixel footprints/intended framing. Four-pixel failure is no-go even when input captures it. Record two-pixel and sparse-sampling loss. |
| Q6 Motion | 120-frame pan/zoom, edge/cliff crossing, neighbor-cell crossing and multicell jump: no trails, stale-cell lighting or seams; stationary valid samples retain Q3. Record screen loss/ringing. Instability defeating four-pixel usefulness is no-go. |
| Q7 Exclusions | Existing water/shield/transparent/particle/sprite shading remains; demonstrate absent transport and transparent-cover limit, visible model-over-world disable, and both orthographic/perspective operation. |
| Q8 Lifecycle | MSAA off/highest supported configured value; UHD->smaller->UHD; minimize/restore; off/on; world-hidden/resume; pipeline-only recreation; all actual slots. No validation errors, stale accesses, nonfinite/misaligned values or pending-set mutation. Capture proves barriers, slot selection, Global/Main/particle/ImGui ordering and HDR/MSAA/submission preservation. SPIR-V/reflection proves singleton storage descriptors and no StorageImageArrayDynamicIndexing capability. Each per-framebuffer set references its own output view after every recreation. |
| Q9 Cost/memory | At UHD on target Vulkan GPU, ordinary and dense-effects fixtures satisfy C<=2L in every five-pair cohort below. Report full-frame GPU/CPU/FPS, all-live VMA delta and resize peak; no invented absolute-ms/memory ceiling. |
| Q10 Evidence | Shader generation, BES client Debug validation/Profile timing and affected server-safe checks, static/project/shared-layout checks and required reviews pass. Save exact setup/raw cohorts/captures, every gate verdict and final go/no-go. Pinned notice source/generated output match exact upstream text; hashes/paths, fresh generation, unchanged repeat and delivered shader-pack-plus-notice inventory prove delivery. Inventory maps every copied routine/helper; unresolved provenance blocks copying. |

For Q3 use linear Rec.709 luminance Y and, at each fixed valid receiver s,
e_s=(Y(candidate_s)-Y(reference_s))/max(Y(reference_s),0.01*peakReferenceY).
Normalized RMS=sqrt(mean_s(e_s*e_s)), raw and final evaluated separately.
Peak comes from the fixed reference grid, never candidate output. Also report
RGB errors; no tonemapped comparison. Uniform-field error is relative error
against pi*L per nonzero RGB channel. Q5 saves source-on/off linear contribution
at logged real receiver regions plus paired visible screenshots; response must
be distinguishable from repeated-off variation with reference/occlusion gates
passing. Input presence alone fails.

### Complete timing protocol

For each ordinary/dense UHD fixture, run five alternating fully-off/on pairs,
each side 120 warm-up then 600 completed measured frames, in one executable/
process/device with identical settings/paused content. L is the fully-off mean
for that pair's containing
current deposit+spread+combine+temporal scope, including scheduled layers,
history, unconditional work and skipped refresh frames; exclude shadows,
smoke, wind, water and forward shading. C is candidate input+gather+filter
means plus max(0,on eligible complete-forward means minus fully-off eligible
complete-forward means). Charge all clears, transitions, counters and forward
upsample/composition; no uncharged standalone resolve. Every pair C<=2L.

Reuse landed unique completed-frame/slot cohort accounting, adding scopes rather than reading smoothed rows. Extend exact ready sums/counts/frame
identity, aligned timer enums/reset ranges/names. Respect Global/Main query
ownership and valid total-frame endpoints. Report cohort medians/ranges and
avoid claiming measurable difference inside `query_profile` variability.
Report GPU/FPS and CPU limits. Disable diagnostics/reference/readback/capture/
validation layers for timing and report their overhead separately. No upstream
benchmark transfer or unmeasured improvement claim.

For memory, record resource extents/formats/counts/payload and actual live VMA
delta/resize peak, including all real slots, baseline overlap, diagnostics and
retired allocations. Do not equate payload arithmetic with actual residency.
Timing duration is unmeasured; five pairs require 7200 completed frames per
fixture, excluding builds/loading/draining/captures. Follow [Change Workflow User Interaction](../../../.agents/references/change-workflow.md#user-interaction) Long test runs: before dispatching a run expected to exceed 10 minutes, obtain user approval with expected duration, checks and a shorter alternative. Do not truncate required cohorts or claim full acceptance from a spot check.

## Risks and invariants

Future implementation is Change Workflow Tier 3: independently owned renderer,
CPU/GLSL layout/descriptor/lifetime, asset attribution and forward composition
integration. Stage-2 feature requirements remain Vulkan 1.2 with the current
device feature set. Keep server-safe affinity, deterministic simulation/CRC,
wire, saves/replays and .pack layouts untouched. Actual shader/layout publication,
record-once command buffers and pending-use lifetime are acceptance invariants.
Finite visibility, sparse samples, geometric normals, slab bias, half-size
filtering and bandwidth can fail promotion; report them without hiding failure
using temporal reuse, ambient floors or relaxed gates.

Use `/compile` for shader generation/DataPacker Release|x64, BES client
Debug|x64 validation and Profile|x64 timing, and affected server-safe builds.
Use `/update-vcxproj`, affected-code/docs propagation, C++/GLSL/comment/style/
overengineering/coherence/progressive/adversarial reviews as triggered, then
independent acceptance verification and `/finalize-changes`. No unit tests.

## Coordination

The planar experiment is independent; its only required edit here is the final
pending citation rewrite. No shared implementation ordering or nondirectional
constraint is imposed. Preserve whichever independent lighting changes have
already landed when integrating this opt-in effect.

## Supporting documentation and research

- [Pinned SSRT3 core](https://github.com/cdrinmatane/SSRT3/blob/b38d82140acf646ee485f9ab16a5fa84906f2231/HDRP/Shaders/Resources/SSRTCS.compute#L218), [license](https://github.com/cdrinmatane/SSRT3/blob/b38d82140acf646ee485f9ab16a5fa84906f2231/LICENSE), [author explanation](https://cdrinmatane.github.io/posts/ssaovb-code/) and [paper](https://arxiv.org/abs/2301.11376v2) support visibility sectors/provenance, never this adaptation's speed/accuracy.
Source-normal emission, weights, projection, sampling and composition deliberately differ.
- [McGuire/Mara DDA](https://jcgt.org/published/0003/04/04/) supplies perspective-correct segment traversal theory for the independent diagnostic reference. Implement locally without copying candidate sector logic or third-party code.
- [Managers](../../../Engine/Source/Graphics/Managers/AGENTS.md), [render ordering](../../../Engine/Source/Graphics/Render/AGENTS.md), [shaders](../../../Engine/Data/Shaders/AGENTS.md), [model](../../../Engine/Data/Shaders/Model/AGENTS.md), [terrain](../../../Engine/Data/Shaders/Terrain/AGENTS.md), [lighting](../../../Engine/Data/Shaders/Lighting/AGENTS.md), [profiling](../../../Engine/Source/Profile/AGENTS.md) and
[attribution](../../../DataPacker/Source/AGENTS.md) govern integration.

Named graphics/shader/profile/Attribution source was checked unchanged between
379a17c99d379c1e6ad4e3ce3ad1a46977c0fc08 and primary
f6c5a7befa65f4792037fce372b82c5488740b81. Read landed inputs/current owners at
execution; these anchors establish no runtime proof.

A correctly measured technique/budget failure completes the experiment with
no-go and a recommendation against promotion; shader defects and invalid
evidence require correction. Record every failed gate and excluded-surface/
transparent-cover limitation. Production replacement needs a separate approved
Plan. This is self-contained: scratch snapshots and the completed stage-1
Plan are not required execution inputs.
