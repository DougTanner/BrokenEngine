# Prototype a directional irradiance cache

## Context and authority

The user requested isolated, quality-first lighting prototypes and a plan per technique. Can a compact directional cache preserve light height and full-normal response with acceptable spatial softness? This document fixes an author-proposed candidate for future claim, review, and user approval; authoring it authorizes no renderer implementation or production adoption.

All five independent candidates must start from the same verified commit on the reusable
`lighting-foundation` branch, with no dependency on another candidate. Do not begin candidate
implementation or comparison runs until a foundation commit passes the
[foundation acceptance table](LightingFoundationHandoff.md#acceptance-table). Create experiment worktrees through
the supported wrapper, then establish the common foundation ancestry through the repository
workflow; the wrapper's current-primary starting point is not the comparison baseline. Record the
actual foundation and candidate commits. Do not adopt or delete worktrees to force a match. The historical
[per-flight-plane investigation](LightingPerFlightPlane.md) is context, not authority for gameplay.

The foundation removes projected local-light deposits and preserves gameplay with exactly zero
local diffuse. It supplies physical fixture sources, resident triangle/material queries, shared
radiometry and comparison inputs. Sun/IBL, shadows, water, smoke, sprites and shield visuals retain
their ordinary paths. The candidate must preserve real XYZ and evaluate the complete normal;
brightness alone is not evidence of better directional quality. Historical legacy captures remain
historical evidence, not a retained runtime comparison path.

## Parts

Part 1 is unshadowed base lighting. The candidate evaluates no light-to-point visibility, neither
at receivers nor at bounce hits. It compares against foundation captures with `Shadows`
unchecked and adds the Tweaks controls below to a technique subtab of the Tweaks Lighting section
(`TweaksScreenLighting.cpp`).

| Control | Range | Default |
| --- | --- | --- |
| Probe grid XY | 8-64 | 32 |
| Probe grid Z | 4-32 | 16 |
| Area samples | 1-64 | 64 |
| Rebuild every N frames | 1-8 | 1 |
| Interpolation | trilinear/nearest | trilinear |
| Negative-SH clamp | on/off | off |
| Probe view | on/off | off |

Each control's default is the value in its table row; where `## Design` states an initial
setting, the default equals it. Comparison captures use those defaults. No control alters the
shared fixture sources, so the reference stays comparable. The candidate's passes appear as GPU
timer rows on the Profile screen and in `query_profile`.

Part 1 ends at a human evaluation. Present the user the candidate running live on the fixture
with these controls, plus the part-1 evidence: comparison against the unshadowed reference,
per-pass GPU time and bytes. The user judges base-lighting quality and performance and chooses
one of three outcomes:
- reject or defer the candidate;
- tune it further;
- start part 2.

Record that decision in the evidence bundle. Part 2 adds light-to-point visibility through the
foundation's ray queries and compares against the default `Shadows`-checked reference. It never
begins without the user's explicit go. Terrain shadows and drop shadows keep their ordinary paths
in both parts.

## Design

Use a finite camera-cell-relative 3D probe lattice, initially 32 x 32 x 16, covering the visible XY footprint plus the current lighting margin and Z from terrain minimum to 150 m above the highest flight plane. Store RGB order-2 spherical-harmonic (SH) irradiance: nine coefficients per color, evaluated with the receiver's full unit normal. This is a low-frequency approximation of direct diffuse lighting, not a multi-bounce GI solver or a specular representation.

Rebuild every probe every frame initially. Project incident radiance into SH and apply the diffuse
cosine convolution. Integrate every local point light analytically and every area emitter with 64
fixed stratified samples. Interpolate all eight neighboring probes in XYZ. Use floating-point
storage, linear additive radiometry, and no interleaving or temporal history in the first
comparison. The runtime probe-grid controls check whether visible softness is resolution-related.

Storing three flight-plane slices alone cannot resolve arbitrary receiver heights or overhangs. Compare especially upward/downward normals, cache-cell crossings, and lights changing height.

Part 2: through the tracing prerequisite below, trace source visibility and reject
receiver-to-probe segments blocked by geometry, normalizing the remaining weights; if none remains
visible, evaluate direct lighting at that receiver rather than introducing unlit holes; and add
small occluders to the comparison.

## Implementation prerequisite

Use the verified foundation's Vulkan ray-query backend, resident terrain and rigid/skinned model
producers, material evaluation, fixed fixture, exhaustive direct reference and capture.
Do not rebuild those common mechanisms inside this candidate. Confirm the selected physical GPU's
required features/extensions/formats and retain its capability evidence; unsupported hardware
blocks comparison rather than selecting a heightfield or screen-space substitute.

Part 1 traces no geometry rays. Part 2's light-visibility and receiver-to-probe rejection rays
trace resident terrain, opaque models and overhangs under the foundation's material policy:
MASK is opaque; BLEND, water, smoke, sprites and shields do not occlude rays. Water remains a
diffuse receiver. Preserve synchronous Frame borrowing, final GPU geometry/ranges, camera-cell
coordinates, resource retirement and capture input lifetimes without asset/pack format changes.
The foundation traces off-screen models but not off-screen terrain, which sits at the sea floor
([known omissions](LightingFoundationHandoff.md#known-omissions)). Verify static, moving and off-screen model
occluders for part 2's rays before judging its results. A missing
producer or failed foundation gate requires plan revision rather than an unrecorded substitute.

## Critical files

- `Engine/Source/Graphics/Lighting/Lighting.cpp` and its producers: reuse physical fixture source publication and resident scene queries; gameplay light collections remain zero physical-source stubs.
- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp`: candidate composition and dispatch ordering.
- `Engine/Source/Graphics/Managers/{DeviceManager,InstanceManager,BufferManager,PipelineManager,RenderTargetTextures}.cpp`: capability enablement, resources, descriptors, synchronization, and recreation.
- `Engine/Data/Shaders/Lighting/{LightingCommon.h,LightingQuery.h,LightingRadiometry.h}`: shared source, material/query and radiometry contracts.
- `Engine/Data/Shaders/{Model/ModelForward.h,Terrain/Terrain.frag,Water/Water.frag}`: actual receiver position/normal and final composition.
- Proposed client-only `Engine/Source/Graphics/Debug/LightingPrototype.{h,cpp}` and `Engine/Data/Shaders/Lighting/PrototypeDirectionalCache*.comp`: candidate implementation reusing the foundation fixture/reference.
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp`: the technique subtab holding the part-1 Tweaks controls.

## In scope

- The critical-file regions above, their required headers/project membership, and candidate resources needed by this single technique; reuse foundation ray-query ownership. Add `Engine/Data/Shaders/Lighting/PrototypeDirectionalCache*.comp` for SH integration and probe publication; `ShaderFunctions.h` for XYZ interpolation and full-normal SH evaluation.
- Reuse the foundation fixture/light stream and capture exports (`capture.json` plus raw images) through supported debug/harness controls; no new simulation fields or network commands.
- A debug selector for foundation zero diffuse versus this one candidate, plus reference/diagnostic outputs. Preserve the zero path and reset candidate history when selected. This permits valid within-process paired profiling; it is not a multi-technique runtime framework.
- Reuse the foundation exhaustive direct-diffuse reference using all lights at 256 area samples, captured with `Shadows` unchecked for part 1 and with the default `Shadows` checked for part 2; the foundation records no 256/512 convergence evidence and keeps its 512-sample control for suspected area-sampling noise. Keep indirect/environmental lighting fixed across candidate and direct-reference views.
- The part-1 Tweaks controls listed in `## Parts`, each changing the live candidate output.
- Client-only timers, resource-byte accounting, source/settings manifest, comparison screenshots and motion sequences. Integrate model/terrain/water local diffuse lighting; retain existing smoke/billboard policy and report its mismatch separately.

## Out of scope

Production adoption/landing of the experimental renderer, optimization to a guessed frame budget, algorithm variants beyond the part-1 Tweaks evaluation controls, multiple hardware backends, generic configuration systems, speculative scalability, gameplay/flight-plane rules, simulation/CRC, save/replay/wire formats, asset formats, and unit tests.
No edits to other prototype branches or the historical investigation. No claim of physically complete GI, volumetric transport, or specular correctness.

## Risk and invariants

Authoring these documents is Tier 1. Executing this proposal requires fresh classification; new CPU/GPU layout contracts, acceleration-resource synchronization/lifetime, and cross-owner geometry integration are prospective Tier-3 triggers, requiring the normal plan reviews and approval before implementation.

Preserve cell-local positions; convert to the camera cell only at publication/use, with retained caches rebased or invalidated on camera-cell changes. All added state is client-only and outside deterministic frames/CRC. Preserve record-once command buffers, in-flight descriptor safety, device recreation, and shared CPU/GLSL layouts.
Ship-borne lights fly and fire on all three flight planes: fighters, omnis and battleships each climb toward their ordered plane (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp:255`, `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp:330`, `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp:250`),
up to planes 2, 1 and 0 respectively (100, 50 and 6 m), so lights at 6, 50 and 100 m are ordinary gameplay; the fixture's three heights are test heights, never a replacement for full 3D evaluation.

## Comparison fixture

Reuse the foundation fixture; its generation code at the foundation commit identifies it. Its specification is centered at
camera-cell XY (0,0): a 512 m square neutral diffuse receiving
surface, test receivers at Z=6/50/100 m, a vertical wall, a thin wall, an overhang, and a colored
bounce wall. Keep Lambertian albedo at 0.5 (bounce wall RGB 0.8/0.1/0.1), existing specular
parameters, and seed 0x51A7. Reuse the ordered 1000-light set generated by the fixture-owned fixed-seed RNG;
take its first 100/300/1000 entries, with every fifth light a 4 m square area emitter. Distribute XY
uniformly over the square and Z cyclically at 6/50/100 m; give all sources the same recorded linear
power and 150 m range. Generation is deterministic, so every candidate built on the same foundation
commit uses identical geometry, lights, and camera/events.

Use the Lowest zoom and Default zoom cameras defined by the
[foundation fixture](LightingFoundationHandoff.md#what-the-foundation-provides).
Include isolated source/receiver pairs at the three heights. Over a fixed
120-frame sequence, move one source from Z=6 to 100 m, move one receiver through those heights, and
move one opaque occluder between that pair; repeat a fixed pan/zoom path and a camera-cell boundary
crossing. Record scene-relative positions via the normal cell conversion. Keep the fixture identical
for direct comparisons; indirect candidates additionally use the same colored-wall bounce
diagnostic.

## Verification

Use `/compile` for the affected client, shader/DataPacker path, and server/header consumers the approved execution scope requires; use `/agent-harness` for launch, scene setup, captures, replay checks, and profiling. Inspect supported commands first; do not invent harness verbs. If a required foundation control is unavailable, record the missing control for plan revision rather than claiming the test ran.

| Criterion | Required observation/evidence |
| --- | --- |
| Reproducible comparison | Manifest records foundation/experiment commits, hardware/driver, packs, fixed seed, scene/light parameters, camera path, material settings, 1920 x 1080 resolution, fixed exposure, sun/environment, warmup and frame/tick identifiers. Use identical fixture inputs across all five worktrees. |
| Height and direction | Same-XY lights at 6/50/100 m; receivers at those heights and between them; continuous source/receiver travel across plane boundaries; upward/downward/sloped/side normals; unobstructed illumination between different planes. Preserve the actual XYZ distance and full-normal response in outputs. |
| Visibility (part 2) | Light behind terrain ridge, thin wall, overhang, and moving mesh; lit and blocked receivers on different heights. Show occluded/unoccluded reference pairs and light-leak differences. Record the geometry traced and any omitted geometry explicitly. |
| Light density | Fixed nested sets of 100, 300, and 1000 point/area lights at mixed heights; same emitters, power, range, and seed across candidates. Capture both gameplay-scale zoom-out and close inspection. |
| Technique quality | Directional cache: upward, downward, and side-facing patches respond to source elevation; report negative SH reconstruction/clamping, angular smoothing, spatial blur, and probe-density sensitivity. A colored source crossing 6/50/100 m must change the lit footprint continuously. Part 2: report visibility rejection, including around small occluders. |
| Matched visual evidence | Foundation zero, candidate, and converged reference screenshots for every scene, plus video or equivalent ordered frame sequences for moving lights/occluders, camera pan/zoom, cell-boundary crossing, and history reset. Inspect water/terrain/models and retained smoke. Preserve floating-point diagnostic outputs where display tone mapping hides differences. |
| Honest radiometry | Compare candidate additive linear HDR against the foundation float reference; keep historical legacy MAX/rgba8 captures separately identified. Use common exposure/materials; include linear direct/indirect diagnostics and overlapping-light scenes rather than tuning exposure per candidate. |
| Cost and lag | Quality settings first: report part-1 cost, then part 2's added visibility cost separately, each as per-pass and total GPU time, tracing/acceleration update cost, peak/persistent GPU bytes, update frequency, convergence and visible lag in frames/ms. Compare foundation zero/candidate timings only via the two-path selector in one process using `query_profile`'s documented caveats. Standalone reference timings are descriptive; no cross-process speedup ratios or guessed pass/fail budget. |
| Integrity | No validation errors during motion, resource rebuild, or camera-cell change; record client/server replay/CRC evidence through the harness showing visual state remains outside simulation. |
| Tweaks controls | Each `## Parts` control changes the live output, and its defaults reproduce the comparison capture. |
| Human evaluation gate | The user's part-1 decision is recorded in the evidence bundle; no part-2 edit precedes a recorded go. |

At the end of each part, finish by retaining the exact branch/commit and evidence bundle and presenting the user with quality findings, limitations, and measured costs: after part 1 for the human evaluation in `## Parts`, after part 2 for adoption, defer, or reject. Do not automatically land the experimental renderer onto primary. Any accepted production change requires its own approved scope and normal landing guards.

## Sources

[SH irradiance representation](https://www-graphics.stanford.edu/papers/envmap/envmap.pdf). SH specifies the representation; the live direct-light integration, grid, and rejection policy above are this prototype's proposal.

