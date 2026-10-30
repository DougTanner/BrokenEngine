# Prototype a plane light field

## Context and authority

The user requested isolated, quality-first lighting prototypes and a plan per technique. Can four 2D light layers seen from above, one following the terrain and water surface and one at each flight plane, preserve light and receiver height at texel resolution where nearly every receiver sits? This document fixes an author-proposed candidate for future claim, review, and user approval; authoring it authorizes no renderer implementation or production adoption.

All seven independent candidates must start from the same verified commit on the reusable
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
| Layer resolution, texels per side | 512, 1024 or 2048 | 1024 |
| Direction encoding | L1 SH/six-direction | L1 SH |
| Area samples per emitter | 1-64 | 64 |
| Layer refresh staggering | on/off | off |
| Layer view | off, surface, 6 m, 50 m or 100 m | off |

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

Keep four camera-cell-relative 2D light layers that share one XY square enclosing the visible footprint at sea level. The surface layer's height at each texel is the higher of the composite terrain elevation target and the water surface there; the other three sit at the flight-plane heights of 6, 50 and 100 m (`FlightPlaneTable.h`). Each layer is initially 1024 x 1024 texels.
Each texel stores the irradiance arriving at its own XYZ point as RGB L1 spherical harmonics (SH): four coefficients per color with the diffuse cosine convolution applied, in three RGBA16F targets, one per color. The six-direction encoding (east, west, north, south, up, down) is the alternative storage.

Rebuild every layer every frame initially, from cleared targets. Every light deposits additively
into every layer its range reaches, as one quad per light per layer covering its range footprint
clipped to the rectangle. Each covered texel evaluates the light at the texel's full XYZ point
with the same distance/range and radiometry definitions as the exhaustive reference, using 64
fixed stratified samples per area emitter; texels beyond range add nothing. Use no spread, filter,
temporal history or interleaving in the first comparison. The layer resolution control checks
whether visible softness is resolution-related.

A model, terrain or water receiver reads the layer stack at its XY: the surface layer, then each flight-plane layer above the surface there. It blends linearly between the two layers around its height, reads the surface layer alone at or below the surface and the 100 m layer alone above 100 m, and evaluates the blended SH with its full unit normal and the reference's albedo and diffuse BRDF.
Keep existing sun, material specular policy, and environmental illumination fixed. New local-light diffuse radiance is shown separately and then composed once into the model, terrain, and water shading paths at the foundation's composition seam (`LightingCompose.h`), not on the removed legacy targets. This prototype does not add indirect bounce.

This is the per-flight-plane investigation's option (b) with an exact deposit; its option (a)
sums the planes without receiver height and is not prototyped. Compare especially receivers
between planes, upward/downward normals, the overhang, and lights changing height; the known
limits are in `## Risk and invariants`.

With layer refresh staggering on, the surface layer refreshes every frame and one flight-plane layer refreshes per frame in smoke's `miFrameCounter % gpFlightPlanes->Count()` order. A skipped layer keeps its last output and rectangle and refreshes early once the visible footprint leaves that rectangle, as smoke does (`SmokeUniforms.cpp`).

Part 2: through the tracing prerequisite below, trace source visibility from each layer texel's
point during the deposit so a blocked light adds nothing there; reject a layer when the vertical
segment from the receiver to that layer's texel is blocked, giving its weight to the other layer; if neither
remains visible, evaluate direct lighting at that receiver rather than introducing unlit holes;
and add small occluders to the comparison.

## Implementation prerequisite

Use the verified foundation's Vulkan ray-query backend, resident terrain and rigid/skinned model
producers, material evaluation, fixed fixture, exhaustive direct reference and capture.
Do not rebuild those common mechanisms inside this candidate. Confirm the selected physical GPU's
required features/extensions/formats and retain its capability evidence; unsupported hardware
blocks comparison rather than selecting a heightfield or screen-space substitute.

Part 1 traces no geometry rays; the surface layer's height comes from the composite terrain
elevation target and the water surface. Part 2's light-visibility and receiver-to-layer rejection
rays trace resident terrain, opaque models and overhangs under the foundation's material policy:
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
- `Engine/Data/Shaders/Lighting/{LightingCommon.h,LightingQuery.h,LightingRadiometry.h,LightingCompose.h}`: shared source, material/query, radiometry and composition contracts.
- `Engine/Data/Shaders/{Model/ModelForward.h,Terrain/Terrain.frag,Water/Water.frag}`: actual receiver position/normal and final composition.
- `Engine/Source/Graphics/Render/SmokeUniforms.cpp`: the per-plane staggered refresh and held-area pattern the staggering control follows; read, not edited.
- Proposed client-only `Engine/Source/Graphics/Debug/LightingPrototype.{h,cpp}` and `Engine/Data/Shaders/Lighting/PrototypePlaneLightField*.{vert,frag}`: candidate implementation reusing the foundation fixture/reference.
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp`: the technique subtab holding the part-1 Tweaks controls.

## In scope

- The critical-file regions above, their required headers/project membership, and candidate resources needed by this single technique; reuse foundation ray-query ownership. Add `Engine/Data/Shaders/Lighting/PrototypePlaneLightField*.{vert,frag}` for the per-layer additive deposit; `ShaderFunctions.h` for layer-stack blending and full-normal SH evaluation and, in part 2, receiver-to-layer rejection.
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

Known limits of the technique, to measure rather than hide: a receiver between planes, such as a
climbing ship, gets a blend of the two layers around it; an overhang or island wall has only the
one surface layer seen from above, so a receiver under an overhang reads the lighting of its top;
with staggering on, a skipped flight-plane layer trails moving lights by up to two frames.

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
| Reproducible comparison | Manifest records foundation/experiment commits, hardware/driver, packs, fixed seed, scene/light parameters, camera path, material settings, 1920 x 1080 resolution, fixed exposure, sun/environment, warmup and frame/tick identifiers. Use identical fixture inputs across all seven worktrees. |
| Height and direction | Same-XY lights at 6/50/100 m; receivers at those heights and between them; continuous source/receiver travel across plane boundaries; upward/downward/sloped/side normals; unobstructed illumination between different planes. Preserve the actual XYZ distance and full-normal response in outputs. |
| Visibility (part 2) | Light behind terrain ridge, thin wall, overhang, and moving mesh; lit and blocked receivers on different heights. Show occluded/unoccluded reference pairs and light-leak differences. Record the geometry traced and any omitted geometry explicitly. |
| Light density | Fixed nested sets of 100, 300, and 1000 point/area lights at mixed heights; same emitters, power, range, and seed across candidates. Capture both gameplay-scale zoom-out and close inspection. |
| Technique quality | Plane light field: same-XY lights at 6/50/100 m read as different heights on water and terrain by footprint and brightness, and receivers on a plane are lit by that plane's lights. Report error against the exhaustive reference on each layer, between planes, under the overhang and on walls, and layer-resolution sensitivity. With staggering on, report moving-light lag and pan/zoom seams. Part 2: report visibility rejection and direct-evaluation fallbacks. |
| Matched visual evidence | Foundation zero, candidate, and converged reference screenshots for every scene, plus video or equivalent ordered frame sequences for moving lights/occluders, camera pan/zoom, cell-boundary crossing, and history reset. Inspect water/terrain/models and retained smoke. Preserve floating-point diagnostic outputs where display tone mapping hides differences. |
| Honest radiometry | Compare candidate additive linear HDR against the foundation float reference; keep historical legacy MAX/rgba8 captures separately identified. Use common exposure/materials; include linear direct/indirect diagnostics and overlapping-light scenes rather than tuning exposure per candidate. |
| Cost and lag | Quality settings first: report part-1 cost, then part 2's added visibility cost separately, each as per-pass and total GPU time, tracing/acceleration update cost, peak/persistent GPU bytes, update frequency, convergence and visible lag in frames/ms. Compare foundation zero/candidate timings only via the two-path selector in one process using `query_profile`'s documented caveats. Standalone reference timings are descriptive; no cross-process speedup ratios or guessed pass/fail budget. |
| Integrity | No validation errors during motion, resource rebuild, or camera-cell change; record client/server replay/CRC evidence through the harness showing visual state remains outside simulation. |
| Tweaks controls | Each `## Parts` control changes the live output, and its defaults reproduce the comparison capture. |
| Human evaluation gate | The user's part-1 decision is recorded in the evidence bundle; no part-2 edit precedes a recorded go. |

At the end of each part, finish by retaining the exact branch/commit and evidence bundle and presenting the user with quality findings, limitations, and measured costs: after part 1 for the human evaluation in `## Parts`, after part 2 for adoption, defer, or reject. Do not automatically land the experimental renderer onto primary. Any accepted production change requires its own approved scope and normal landing guards.

## Sources

[The Irradiance Volume](https://en-cg-web.coecis.cornell.edu/pubs/1998/GSHG98.html) stores irradiance for receivers to read, [Shading in Valve's Source Engine](https://advances.realtimerendering.com/s2006/Mitchell-ShadingInValvesSourceEngine.pdf) specifies the six-direction ambient cube, and the [SH irradiance representation](https://www-graphics.stanford.edu/papers/envmap/envmap.pdf) specifies L1 SH evaluation with a normal.
Repository precedent: the [per-flight-plane investigation](LightingPerFlightPlane.md) and smoke's per-plane fields and staggered refresh (`Engine/Source/Graphics/Render/SmokeUniforms.cpp`). The four layers, exact deposit and height blend above are this prototype's proposal.

