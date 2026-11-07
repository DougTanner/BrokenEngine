<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T23:42:44.499Z","dependsOn":[]} -->
# Screen-Space Radiance Cascades Prototype

## Context and decision authority

Build one bounded, opt-in client-rendering prototype to decide whether visible
opaque mesh emission can illuminate opaque terrain and models with useful
geometry-aware occlusion at the engine's distant RTS camera. This Plan replaces
the screen-space feature proposal; [planar RC](PlanarRadianceCascades.md)
is a separate executable Plan. Saving this Plan authorizes no implementation.

The author records these user decisions: a measurable prototype go/no-go;
dedicated single-sample terrain/opaque-model scene inputs; visible material
emission as the source; terrain and opaque models as receivers; water, shields,
transparent models, particles and visible-light sprites retain current lighting;
target this PC at 3840x2160. Maintaining current FPS is ideal; complete new effect
cost may be up to twice the baseline current lighting-producer cost. No absolute
millisecond or memory ceiling was approved. Windows reports an AMD Radeon RX 9070
XT and a separate integrated adapter; verify the actual Vulkan adapter at runtime.

All numerical algorithm choices, visual gates, half-resolution inputs and
composition details below are the author's concrete recommended design, subject
to approval when this Plan is executed. They are not additional recorded user
decisions. Research baseline: `07f763e2d96271ebf4b4794ea1185763a40451b7`.
No builds, GPU captures or runtime performance measurements informed this Plan.
The present change is Tier 1 documentation only. Execution is Tier 3: it changes
render-pass ordering, shared CPU/GPU layouts, directional transport, material
composition and resource synchronization. Run the required implementation-plan
reviews and user approval before coding; do not infer approval from scheduling.

## Design

### Source, receiver and composition contract

Keep the complete production lighting deposit/spread/combine/temporal path and
all current direct sun/moon, shadows, IBL, reflections, local diffuse/specular,
smoke and fog terms. Baseline disables all new passes and shader contribution;
candidate adds only the new diffuse term on eligible opaque receivers before
their existing smoke/fog composition. An existing debug-texture view displays
RC alone. This is an additive experiment, not an energy-correct production
replacement: an emitter that also has a planar light depositor can contribute
twice. Use a material-only emissive mesh with no associated light deposit in
controlled quality fixtures. Production replacement requires another approved
Plan identifying the exact old terms and sources to remove.

Source RGB is the same pre-fog linear mesh emission currently added by
`Model.frag`: decoded emissive texture times material emissive factor times
`mainLayout.fPbrEmissive`. Interpret it as outgoing radiance in the renderer's
existing arbitrary linear HDR units; no exposure, Uchimura mapping, IBL,
sun-lit albedo, old local field, instance glow or current RC enters the source.
Terrain currently has no material-emission term and writes zero emission.
Use nearest opaque visibility, including the opaque model path's existing alpha
cutout/discard and culling rules. BLEND materials never participate. Emit into
the outward geometric-normal hemisphere; a backface hit blocks but emits zero.
For a double-sided material use the geometric normal oriented toward the input
camera, explicitly accepting that this is only a visible-front-layer model.

At final material shading integrate incoming radiance with the actual current
shading normal, then multiply once by diffuse response divided by pi. Models
reuse the already computed metallic-aware `diffuseColor`; terrain uses its
linear layered `f3Color`. Do not multiply transported radiance by source albedo,
or apply the old art-directed local-light power/add/mix controls to RC. Leave
self-emission unchanged. No indirect specular, recursive bounce, sun/IBL bounce,
transparent transport, sky fallback or native projectile-sprite emission.
Water and excluded effects are neither RC sources, blockers nor receivers;
they can visually cover a receiver while RC still travels through their absent
representation. That limitation must appear in the result.

### Scene inputs and reconstruction

After existing Main preparation (including object shadows/water preparation),
before `RecordImageRenderPass`, rasterize the same currently visible terrain and
opaque-model geometry into a separate single-sample pass. Reuse draw lists,
transforms, LOD, skinning/material bindings and alpha cutoff, with no UI depth
mask, water or transparent draws. Use the actual shaken scene view-projection.
Support the ordinary world camera in orthographic and perspective modes. When
`GraphicsFlags::kDrawModelsOverWorld` is set, disable the prototype and expose
that status; its depth-clear presentation has no single physical scene depth.

Input extent is ceil(output extent / 2) on each axis: 1920x1080 at UHD.
Attachments are sampled single-sample depth, RGBA16F linear emission, and
RGBA16F geometric normal XYZ plus validity A. Clear emission/normal/validity to
zero and depth to the current renderer's far-depth clear. Store depth; use a
depth-only sampled view. Select a supported sampled depth-attachment format by
checking format features and image-format usage, preferring a 32-bit depth
format; inability to support this path is a no-go, not permission for an MSAA
redesign. Normal reconstruction from depth is insufficient at material edges;
the final shading normal stays in the forward material shader.

Add a CPU-computed inverse of the same shaken world view-projection and the
camera orientation to the Main uniform contract; mirror C++/GLSL layout and
uploads. Never call GLSL matrix inverse. Reconstruct positions in the camera's
`RenderBasis`, using Vulkan depth convention and actual viewport Y orientation.
No absolute positions or simulation columns. All origins are current-frame;
camera cell changes need no retained-surface reprojection. Input sampling uses
nearest depth/normal/emission, never filtered depth across a silhouette.

### Fixed initial hierarchy and trace

This is an engineering extension of RC interval composition to spherical
surface probes, not a claim to reproduce PoE2's unpublished production tracer.
Use four levels i=0..3. Probe spacing is `32*2^i` output pixels, equivalently
`16*2^i` input pixels. Probe origin is the nearest input pixel to the regular
grid cell center, clamped at the image edge; invalid/background origins remain
invalid. No search or relocation onto another surface. Spatial dimensions at
UHD are 120x68, 60x34, 30x17 and 15x9. Ceil dimensions cover the whole viewport.

Angular grids are 4x4, 8x8, 16x16 and 32x32. Use equal-solid-angle full-sphere
bins: midpoint phi uniform in [0,2pi), midpoint mu uniform in [-1,1]; direction
is `(sqrt(1-mu^2)*cos(phi), sqrt(1-mu^2)*sin(phi), mu)` rotated from camera
orientation into RenderBasis. Each angular bin has four children at the next
level. Keep this basis fixed within a frame; no stochastic rotation or history.

Define world pitch p as max(width/outputWidth, height/outputHeight) of the
camera's current ground-plane visible area, using the existing camera visible
area, in engine meters. Set d=32p for all probes in that frame. The intervals
are [0,d), [d,2d), [2d,4d), [4d,8d). Thus range is finite and intentionally
zoom dependent; perspective probes above ground can have different projected
density. Offset each origin by geometric normal*d/128; omit ray distances below
d/128. Reject the originating input pixel until the ray leaves its slab;
this rejection ends at the first other pixel, so it cannot skip another visible
surface. Use positive linear eye-depth slab thickness d/32. These are prototype
settings owned by its parameter definitions, not new global engine constants.

Trace with perspective-correct screen-space DDA following McGuire/Mara, including
the orthographic case. Clip the world segment against near plane and viewport;
compare its positive linear eye-depth span across each visited pixel with
`[surfaceDepth,surfaceDepth+d/32]`, with signs converted from the engine's RH
view convention. Do not compare nonlinear device-depth epsilon. Check the
first intersected opaque slab only. It yields `(radiance,transmittance)=(source,0)`;
a completed interval without a hit yields `(0,1)`. Viewport/near-plane loss or
the hard cap of 128 visited input pixels yields `(0,0)` and a distinct unavailable
flag; it must not borrow a farther cascade through unknown space. Background
pixels do not block; no sky or offscreen source is invented. Degenerate projected
rays still test their depth segment in the containing pixel. Preserve hit,
completed-miss, out-of-representation and exhausted counters separately.

### Merge and directional resolve

Store RGBA16F interval RGB and scalar transmittance; use separate raw-trace and
merged hierarchies. Each level is one image tiled by spatial probe, with its
angular grid inside that probe's tile; UHD image extents are 480x272 for the
first three levels and 480x288 for the fourth. Process far to near. Coarsest far
radiance is zero. For each
fine bin, fetch four spatially neighboring coarse origins and four corresponding
angular children per origin. Reject invalid origins or 3D displacement greater
than `4*max(d, intervalNearDistance)` of the fine origin, or geometric-normal
dot below 0.8; bilinear spatial
weights renormalize over survivors. Average the four angular children before
spatial interpolation. No survivors means zero far radiance and a rejection
counter. Compose `L=Ln+Tn*Lf`, `T=Tn*Tf`; raw input is immutable during merge.
This interpolation is approximate: different visible surface origins do not
share exact adjacent intervals. Do not use the paper's flatland translated
interval extension, which the paper excludes for depth-varying screen probes.

Keep all 16 base directions until the consumer normal is known. In each forward
opaque material, obtain four neighboring base probes; reject invalid origins,
position displacement greater than 1.5d, or geometric-normal dot below 0.8;
renormalize bilinear weights, zero when none survive. Also reject when current
fragment position differs from the nearest input surface by more than d/32,
preventing half-resolution foreground data from lighting unrelated MSAA edge
samples. For Q=16, compute
`E(n)=sum(Lk*max(dot(n,wk),0)*4pi/Q)` and add `diffuseResponse*E/pi`.
This is finite angular quadrature, not exact Lambertian integration. Measure
its normal dependence. Resolve executes in forward shading so full-resolution
material normals are available; there is no separate full-screen material G-buffer
or resolved irradiance texture. A debug pass resolves the same equation for
inspection using the stored geometric normal.

### Resources, ordering and bounded memory

Own inputs, raw/merged atlases, origin records, counters and debug images in
`RenderTargetTextures`, created/destroyed with screen-dependent resources in
`TextureManager`. Use one set per command-buffer frame slot and bind that set
through existing indexed descriptor machinery; do not assume shared HDR
attachment serialization automatically protects new resources. Allocate only
while the opt-in prototype is enabled. Mode changes request the existing
graphics/command-buffer recreation, since Main is recorded once. Drain the
appropriate in-flight work before destroying or republishing descriptors.
Resize/minimize/restore recreates extents; no history survives or exists.

Raster depth/color writes -> compute sampled reads; raw compute storage writes
-> merge sampled reads; each coarse merged write -> finer merge read; merged
compute writes -> forward fragment sampled reads. Specify image layouts and
Vulkan 1.2 barriers with the actual stage/access scopes, including slot reuse.
Keep the existing Global/Main semaphore dependencies, final HDR resolve and
MSAA attachments untouched. RC input samples stay single-sample regardless of
scene MSAA; verify scene multisample edges against the declared rejection rule.

At UHD there are 529,920 angular bins; each RGBA16F hierarchy is 4,239,360 bytes
(4.04 MiB), two are 8.09 MiB. Depth at four bytes plus two eight-byte input
targets is 39.55 MiB. Core payload is 47.64 MiB per set, or 142.92 MiB for three
sets, before origins, counters, debug images, driver alignment and resize overlap.
If selected depth format or actual slot count differs, recompute the inventory.
At the fixed trace cap there are at most 67,829,760 depth visits before merge
and resolve. Neither count predicts GPU time. Report actual all-live allocation
delta and peak during recreation using VMA statistics, not just texel payload.
There is no approved memory ceiling; memory is a reported decision input.

## In scope

Paths are relative to repository root. Add only the prototype and evidence
needed for this one complete buildable stage; the following are ordered tasks,
not separately landable stages.

| Region | Required work |
| --- | --- |
| `Engine/Source/Graphics/Managers/CommandBufferRecordMain.{h,cpp}`: `Record`, opaque draw recording, `RecordImageRenderPass` | Insert scene-input and trace/merge work before forward rendering; reuse terrain/opaque draws; disabled path preserves baseline; bracket timing regions. |
| `Engine/Source/Graphics/Managers/RenderTargetTextures.{h,cpp}`, new `RenderTargetTexturesRadianceCascades.cpp`; `TextureManager.cpp`: screen-dependent lifecycle | Own slot-indexed attachments, atlases, origins/debug/counters, render pass/framebuffers and debug registration. Reuse existing texture wrappers. |
| `Engine/Source/Graphics/Managers/{PipelineManager,DynamicPipelines,WorldLightingShadowPipelines}.{h,cpp}`; `Engine/Source/Graphics/Objects/ModelPipeline.{h,cpp}` | Register dedicated scene-input terrain/model variants and compute/debug pipelines; reuse `PipelineCreator` MRT support and material draw bindings, not a deferred-renderer abstraction. |
| `Engine/Source/Graphics/Managers/TextureDescriptors.{h,cpp}` and `Engine/Source/Graphics/Render/{MainUniforms,LightingUniforms}.cpp`; `Engine/Data/Shaders/ShaderMainLayout.h` | Publish per-slot bindings, inverse shaken projection/camera orientation and fixed prototype parameters/mode; keep CPU/GPU contract parallel. |
| `Engine/Data/Shaders/Model/Model.frag`, `Terrain/Terrain.frag` and their scene-input variants; new `Engine/Data/Shaders/Lighting/RadianceCascades*.{comp,h}` | Share material emission/alpha semantics without importing final shaded color; implement trace, merge, forward directional resolve, selected-pixel reference and debug visualization. Reuse current vertex variants. |
| `Engine/Source/Graphics/Graphics.{h,cpp}`, `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp`: lighting tweaks/debug texture selection | Opt-in prototype toggle/status and existing debug selection; disabled for model-over-world. Controls only for mode and diagnostic fixture/emission on/off; no general tuning UI. |
| `Engine/Source/Profile/ProfileManager.{h,cpp}`, profile-name table and existing profile display | Timers for baseline lighting producer, scene input, trace, merge, added resolve and total-frame measurements using existing timestamp machinery. Valid endpoints in both modes; no stale disabled values. |
| Relevant shader/project manifests and Visual Studio `.vcxproj`/`.filters`, subsystem `AGENTS.md` | Register new files through `/update-vcxproj`, compile shaders through existing DataPacker path, document only changed contracts. |

### Ordered implementation work

1. Revalidate these anchors after rebase, classify Tier 3, complete required
   alternatives/plan reviews and approval. Establish the fixed fixtures below
   with existing `/agent-harness` scene/collection/camera/UI commands. Record
   baseline device, settings, screenshots, timings and memory before enabling RC.
2. Implement resources, inverse upload, source pass and debug inspection first;
   confirm matching opaque silhouettes, alpha cutoff and source linearity.
3. Implement the fixed hierarchy, bounded DDA, merge and selected-pixel reference.
   Add the diffuse contribution and baseline toggle without modifying old terms.
4. Apply affected-site propagation, shader/code/comment/style/documentation
   reviews and Tier-3 adversarial review; build through `/compile`. Run the full
   acceptance matrix through `/agent-harness`; record a go/no-go and evidence.
   A failed quality or cost gate means no-go, not permission to add history,
   sources, backends, broader geometry or parameter-search infrastructure.

## Out of scope

Production replacement/removal of planar lighting; water/shields/transparents,
particles or light-sprite transport; generic effect-source injection; hidden or
offscreen recovery; full 3D volumes, hardware ray tracing, HRC, distance fields,
hierarchical depth acceleration, stochastic sampling, temporal accumulation,
motion vectors, multi-bounce, indirect specular, new harness commands, scheduler
changes, simulation/CRC/network/save changes and unrelated renderer cleanup.
No unit tests. No runtime implementation is part of creating this Plan.

## Risk triggers and invariants

- All work is client rendering. Preserve deterministic PostRender state and
  current server behavior. RenderBasis conversion happens at GPU use, once.
- Additional geometry/material evaluation and input bandwidth can dominate;
  single-layer visibility loses tiny emitters, hidden backsides and thin
  geometry. Camera motion can change lighting without world changes.
- Probe spacing is not a bound on 3D separation; silhouette rejection can erase
  illumination or still leak. Spherical interpolation and 16-direction resolve
  need empirical validation; published flatland examples do not prove them.
- Existing local lighting is tone mapped before its consumers; it cannot serve
  as the new linear radiance source. Current planar history is not reusable
  surface history. Preserve direct/reflection roles and final display grading.
- Other Plans may touch lighting uniforms/shadows; preserve their approved
  behavior after rebase. No logical prerequisite Plan is required here.

## Verification and go/no-go

Use `/compile` for BrokenEngineSandbox client Profile|x64 (timing) and Debug|x64
(validation), and required DataPacker shader generation; use `/agent-harness`
for all launch, drive, query, screenshot, target-dump and RenderDoc work. Existing
routes: `Projects/BrokenEngineSandbox/Documents/AgentHarness.md`, its
`AgentHarness/{verification,commands-server,commands-client}.md`, and
`.agents/skills/agent-harness/references/command-reference.md`. Use existing
`inject_payload`/`edit_frame`, pause, UI/camera input, `describe_scene`,
`describe_state`, `query_profile`, `dump_render_target` and `renderdoc_capture`;
no new command protocol. Record exact request sequences, IDs, camera state,
settings and packed asset identities in the run evidence so another session
can replay them. Synthetic shader diagnostics below use the existing debug UI
and texture readback, not a new test framework.

### Fixed quality fixtures

Use two fixture classes. A diagnostic input generator writes depth/normal/emission
for a plane, an emissive rectangle and an opaque box in camera-local coordinates
with exactly the same projection/units as real inputs. Add a second raised plane
and tilted receiver patches for height and normal checks. Positions are specified
in d units relative to the camera ground-plane look-at point: receiver plane
z=0 with normal +z, horizontal square emitter centered (0,0,d/2) with side d/2
and emitting normal -z,
box centered (d/2,0,d/4) with side d/4, raised patch at z=d/2, tilted patches at
0/45/90 degrees. Fit these within viewport and range with the normal world camera.
Source radiance is (4,1,0.25), receiver diffuse response (0.5,0.5,0.5). This tests
transport against known inputs, not geometry integration. The ordinary input
pass stays disabled only during this diagnostic mode.

For real integration use existing packed emissive ship materials (for example
`Projects/BrokenEngineSandbox/Data/Models/Spaceship/scene.gltf` has an emissive
texture/factor) beside nonemissive ships and an island cliff, spawned/positioned
with existing game commands. Freeze simulation for comparisons. Add a
prototype-only source multiplier 0/1 after extracting actual material emission
so on/off comparisons do not change geometry, forward self-emission or planar
deposits; do not fabricate a glow where the material has none. Confirm the
isolated source has no associated planar depositor in this fixture. Include
native projectiles/effects to demonstrate their deliberate absence as RC sources.
For reproducibility record the selected material/mesh and transforms, normal
orientation, material texture streaming readiness and camera. Run at source
sizes of 16, 8, 4 and 2 output pixels at the intended camera zoom; below one
input pixel is a documented representation limit, not a successful tiny-light
result. These fixtures use current engine assets; no new art or simulation type.

At a fixed 8x8 grid of valid receiver pixels in each diagnostic view, compute
a reference with 32x32 equal-solid-angle directions traced over [0,8d), using
the same input depth, thickness, source and cap per interval. For every base
direction also compare its RC merged radiance with a direct full-range trace
of that direction; this separates merge error from angular quadrature. Perform
reference work only in diagnostic capture mode, excluded from cost cohorts.
Dump reference, RC, validity and counters through debug textures. Reference rays
that exhaust the cap are invalid evidence and fail the reference-coverage gate.

### Acceptance table

| Criterion | Required evidence and pass condition |
| --- | --- |
| Source contract and integration | Input/debug dumps and a RenderDoc frame show coherent nearest opaque depth/normal/emission, alpha discard agreement, no old shaded color or RC feedback. Source multiplier zero makes RC black; doubling source in diagnostic data doubles linear RC within 2% before display mapping. Direct/IBL/shadows and self-emission stay present in baseline/candidate. |
| Reference coverage | At least 48 of the fixed 64 sample pixels are valid in each unblocked/blocked/raised/tilted fixture, with at least eight lit samples in its unblocked counterpart; no more than 5% of their traced reference directions exhaust or leave representation. Otherwise report no-go or invalid fixture evidence, never silently drop difficult samples. |
| Directional response and merge | Constant-radiance diagnostic has at most 20% irradiance error across the declared normal angles relative to pi*L. On the scene fixtures, luminance-normalized RMS error versus 1024-direction reference <=35%, using denominator max(reference luminance, 1% of fixture peak). Report same-direction merge error separately; no negative/NaN/infinite outputs. |
| Blocker and height behavior | On receiver samples whose reference blocked result is <=10% of its unblocked result, RC is <=20% of that unblocked reference. Raised/tilted surfaces obey the reference-error gate rather than lighting unrelated depth layers. Document thin-slab and hidden-layer failure examples explicitly. |
| Intended camera/source sizes | Real terrain/model receiver response is visible and changes under source on/off and cliff occlusion at 16/8/4 output-pixel source sizes; record 2-pixel disappearance as a limit. If required 4-pixel sources do not survive or show useful occlusion at the intended RTS zoom, no-go. Record zoom/perspective state rather than claim all zoom levels work. |
| Motion and boundaries | Capture a 120-frame pan/zoom sequence, source crossing screen edge/behind cliff, neighbor-cell crossing and multi-cell jump. No history trails, stale-cell lighting or coordinate seams; stationary valid reference samples keep the error gate. Record screen-edge/source loss and ringing; instability that defeats useful 4-pixel response is no-go, not authority for history. |
| Excluded materials and modes | Water/shields/transparents/particles/light sprites retain their baseline shaders/lighting; demonstrate absent transport and potential transparent-cover limitation. Model-over-world reports disabled. Test perspective and orthographic world views. |
| Vulkan/lifecycle | MSAA off and highest supported configured setting; UHD -> smaller window -> UHD, minimize/restore, toggle off/on, camera cut/cell moves. No validation errors, stale descriptor/resource access, nonfinite outputs or wrong input/forward alignment. RenderDoc verifies per-slot resources, barriers and unchanged final HDR/MSAA path. |
| Relative cost and memory | Apply the timing protocol below at UHD on verified target Vulkan GPU, normal and dense-effects fixtures. Complete new effect <=2L in every cohort pair; report total-frame/FPS changes and actual all-live allocation/resize peak. No absolute memory or ms cap is claimed. |
| Decision record | Save build/device/settings, fixture requests, captures, raw timing cohorts, input/resource inventory, every failed gate and a clear go/no-go. Prototype completion may record no-go; no-go does not authorize production deployment or silently relax criteria. |

### Timing protocol

L is baseline GPU time for the contiguous current lighting producer:
deposit + spread + combine + temporal, including all passes/layers the renderer
actually schedules, excluding shadows, smoke/wind, water and forward material
shading. Add a containing timestamp pair instead of summing unrelated latest
`query_profile` samples. Measure baseline with prototype fully off, then candidate
with unchanged content, camera, GPU, resolution, settings and simulation pause.
Use three alternating baseline/candidate cohort pairs, each 120 warm-up rendered
frames then 600 measured frames. Read a timer only when its timestamp query is
available. Use arithmetic means over equal completed-frame counts; no percentile
or same-frame subtraction claim from the current profile query API. Record both
cohort sums/counts through the existing profiler/log output; no new harness API.

Instrument scene input, trace and merge separately. Resolve occurs in terrain/model
forward shaders, so its cost includes the candidate-minus-baseline difference in
their complete forward GPU scopes, clamped to zero for the budget calculation.
Define C = candidate input+trace+merge mean plus that nonnegative forward delta.
The required per-pair gate is C<=2L. Include allocation-independent per-frame
clears/counter work in the new scopes. Disable reference/debug readback/capture
work during cohorts. Report diagnostic overhead separately. Report complete
frame GPU time and presented FPS, including retained planar lighting and all
other passes; C is not a claim that whole frame costs 2L. Use a full-frame query
span with valid endpoints across Global/Main and preserve their ordering; show
the whole-frame cohort mean and CPU/present limits. Do not use captured RenderDoc
timings as normal frame cost or subtract unrelated latest/maximum timer rows.

## Primary research and limits

- [Sannikov, Radiance Cascades paper source](https://github.com/Raikiri/RadianceCascadesPaper/blob/main/RadianceCascades.tex),
  sections Radiance Interval, Analysis, Radiant 2d and Path of Exile 2 GI:
  interval composition, screen-space depth probes, visible-data limitations,
  mainly single bounce. The production tracer is outside the paper; translated
  flatland interval extension does not apply to depth-varying screen-space
  probes. Spherical bins, rejection thresholds and this engine's recipe are
  author choices.
- [McGuire and Mara, Efficient GPU Screen-Space Ray Tracing (JCGT 2014)](https://jcgt.org/published/0003/04/04/): perspective-correct contiguous-pixel DDA and finite linear-depth thickness. It supplies tracing mechanics, not RC merging or this renderer's budget.
- [Khronos image usage](https://docs.vulkan.org/refpages/latest/refpages/source/VkImageUsageFlagBits.html) and [depth/stencil resolve properties](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceDepthStencilResolveProperties.html): sampled use must be requested; MSAA depth resolve is a capability/attachment contract. This Plan avoids depth resolve through separate single-sample inputs.

Engine evidence anchors are the critical files above plus
`Engine/Source/Graphics/Managers/SwapchainManager.cpp` (`CreateFramebuffers`:
current depth is attachment-only, discarded and unresolved),
`Engine/Source/Graphics/Objects/PipelineCreator.cpp` (existing MRT count support),
`Engine/Source/Graphics/CameraBase.cpp` (RH orthographic/perspective projection),
`Engine/Source/Profile/ProfileScreens.cpp` (VMA allocation statistics), and
`Engine/Source/Agent/AgentCommandsClientGeneric.cpp` (existing profile/debug dump
and RenderDoc commands). Read their owning AGENTS.md contracts before coding.
The older 2D tutorials are explanatory rather than proof of 3D engine behavior;
no independent renderer code is required or licensed for copying here.
