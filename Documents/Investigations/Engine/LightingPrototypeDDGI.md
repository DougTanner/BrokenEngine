# Prototype DDGI diffuse indirect lighting

## Context and authority

The user requested isolated, quality-first lighting prototypes and a plan per technique. Does visibility-aware 3D probe interpolation add useful diffuse bounce lighting at the game's camera scales? This document fixes an author-proposed candidate for future claim, review, and user approval; authoring it authorizes no renderer implementation or production adoption.

Before any prototype edits, create all five independent experiment worktrees from the same current
primary tip using `pwsh -NoProfile -File .codex/codex-worktree.ps1`, once per experiment. The wrapper
chooses current primary; it does not accept a historical baseline ref. Record each worktree's actual
initial `git rev-parse HEAD` and require all five to equal the recorded comparison baseline. If
primary advances during creation or that baseline cannot run, stop and reconcile a common baseline
through the supported wrapper workflow before implementation or comparison runs. Do not use an
arbitrary checkout or adopt or delete worktrees to force a match. All five candidates use identical
renderer and fixture baselines, the same fixture specification, and no dependency on another
candidate. Record baseline and candidate executable/pack byte hashes. The historical
[per-flight-plane investigation](../../Investigations/Engine/LightingPerFlightPlane.md) is context,
not a user-approved design or authority for current gameplay.

Current point/area uploads project emitters to the base plane; deposit/spread uses horizontal EWNS, MAX blending, and an rgba8 combine. The candidate must preserve real XYZ and evaluate the complete normal. A brighter additive result alone is not evidence of better directional quality.

## Design

Implement one camera-relative DDGI volume, initially 24 x 24 x 12 probes spanning the visible XY footprint plus lighting margin and terrain-to-highest-plane-plus-150 m in Z. Store octahedral irradiance and first/second hit-distance moments per probe; use the paper's visibility-aware trilinear lookup with full XYZ and the full receiver normal. Start with 512 rays per probe per frame, no probe classification, relocation, interleaving, or multi-volume system.

Use exhaustive shadowed direct lighting at the visible receiver and at ray hits: all point lights
and 64 stratified samples per area emitter, with the same linear radiometry as the reference.
Ray-hit shading supplies direct radiance only, producing one diffuse indirect bounce; do not feed
prior irradiance into hit shading or count direct lighting twice. Record initialization,
convergence, and hysteresis. Compare an accumulated high-ray reference before reducing quality.
Probes inside solid geometry are omitted from interpolation using traced hit/backface evidence;
document any uncovered regions.

DDGI represents diffuse indirect lighting; it does not replace direct light evaluation, specular reflections, or shadow geometry. The tracing prerequisite below is part of this prototype, including material/albedo lookup at hits and dynamic geometry updates. Neither the terrain heightfield nor the sun-shadow pass provides that backend.

## Implementation prerequisite

Before renderer edits, inspect the selected physical GPU for `VK_KHR_acceleration_structure`, `VK_KHR_ray_query`, their required features/dependencies, and usable storage formats. The current `DeviceManager` does not enable a ray tracing backend. If unavailable, stop this candidate with the capability evidence; do not silently substitute heightfield or screen-space visibility.

Within this prototype, implement the minimal Vulkan ray-query backend: static triangle acceleration
structures for loaded terrain/model meshes, per-frame instances and the visible animated geometry
used by the fixture, safe resource retirement, and hit material/albedo access. Trace terrain, opaque
meshes, and overhangs; water is a diffuse receiving surface for this comparison, and
smoke/transparency do not occlude rays. Restrict this to existing resident mesh data and runtime
buffers; no asset/pack format change. Verify static and moving occluders before judging the lighting
algorithm. If existing resident geometry cannot supply this, return the concrete missing producer
for plan revision before proceeding.

## Critical files

- `Engine/Source/Frame/Collections/PointLights/PointLightsRender.cpp` and `AreaLights/AreaLightsRender.cpp`: publish unprojected camera-cell XYZ to the prototype without altering simulation collections.
- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp`: deposit/spread/combine/temporal ordering and prototype dispatch.
- `Engine/Source/Graphics/Managers/{DeviceManager,InstanceManager,BufferManager,PipelineManager,RenderTargetTexturesLighting}.cpp`: capability enablement, resources, descriptors, synchronization, and recreation.
- `Engine/Source/Graphics/Render/LightingUniforms.cpp`, `Engine/Data/Shaders/{ShaderLayoutsBase.h,ShaderFunctions.h}`: shared layout/publication and receiver lookup.
- `Engine/Data/Shaders/{Model/Model.frag,Terrain/Terrain.frag,Water/Water.frag}`: actual receiver position/normal and final composition.
- Proposed client-only `Engine/Source/Graphics/Debug/LightingPrototype.{h,cpp}` and `Engine/Data/Shaders/Lighting/PrototypeDDGI*.comp`: bounded fixture/reference/experiment implementation.

## In scope

- The critical-file regions above, their required headers/project membership, and minimal graphics-object ray-query ownership needed by this single candidate. Add `Engine/Data/Shaders/Lighting/PrototypeDDGI*.comp` for probe tracing, irradiance/distance integration, and history; `ShaderFunctions.h` for visibility-weighted full-3D diffuse-indirect lookup.
- One fixed developer fixture/light stream and capture manifest, controlled through existing supported debug/harness mechanisms; no new simulation fields or network commands.
- A debug selector for the unchanged baseline versus this one candidate, plus reference/diagnostic outputs. Preserve the baseline path exactly and reset candidate history when selected. This permits valid within-process paired profiling; it is not a multi-technique runtime framework.
- Exhaustive, high-sample shadowed direct-diffuse reference using all lights and 256 area samples per emitter; double samples to document stability. Also accumulate a ray-traced one-diffuse-bounce reference, doubling ray/sample count to document convergence; compare indirect separately using identical direct illumination and materials.
- Client-only timers, resource-byte accounting, source/settings manifest, comparison screenshots and motion sequences. Integrate model/terrain/water local diffuse lighting; retain existing smoke/billboard policy and report its mismatch separately.

## Out of scope

Production adoption/landing of the experimental renderer, optimization to a guessed frame budget, algorithm variants, multiple hardware backends, generic configuration systems, speculative scalability, gameplay/flight-plane rules, simulation/CRC, save/replay/wire formats, asset formats, and unit tests. No edits to other prototype branches or the historical investigation. No claim of physically complete GI, volumetric transport, or specular correctness.

## Risk and invariants

Authoring these documents is Tier 1. Executing this proposal requires fresh classification; new CPU/GPU layout contracts, acceleration-resource synchronization/lifetime, and cross-owner geometry integration are prospective Tier-3 triggers, requiring the normal plan reviews and approval before implementation.

Preserve cell-local positions; convert to the camera cell only at publication/use, with retained caches rebased or invalidated on camera-cell changes. All added state is client-only and outside deterministic frames/CRC. Preserve record-once command buffers, in-flight descriptor safety, device recreation, and shared CPU/GLSL layouts. Three flight planes are test heights, never a replacement for full 3D visibility.

## Comparison fixture

Use a client-only fixture centered at camera-cell XY (0,0): a 512 m square neutral diffuse receiving
surface, test receivers at Z=15/40/65 m, a vertical wall, a thin wall, an overhang, and a colored
bounce wall. Fix Lambertian albedo to 0.5 (bounce wall RGB 0.8/0.1/0.1), existing specular
parameters, and seed 0x51A7. Generate one ordered 1000-light set using the existing fixed-seed RNG;
take its first 100/300/1000 entries, with every fifth light a 4 m square area emitter. Distribute XY
uniformly over the square and Z cyclically at 15/40/65 m; give all sources the same recorded linear
power and 150 m range. Freeze the full generated geometry, lights, and camera/event manifest before
collecting results and reuse its exact bytes across candidates.

Capture a close camera at (0,-600,900) looking at (0,0,15), and a wide camera at (0,-4000,6000) with
the same look-at. Include isolated source/receiver pairs at the three heights. Over a fixed
120-frame sequence, move one source from Z=15 to 65 m, move one receiver through those heights, and
move one opaque occluder between that pair; repeat a fixed pan/zoom path and a camera-cell boundary
crossing. Record scene-relative positions via the normal cell conversion. Keep the fixture identical
for direct comparisons; indirect candidates additionally use the same colored-wall bounce
diagnostic.

## Verification

Use `/compile` for the affected client, shader/DataPacker path, and server/header consumers the approved execution scope requires; use `/agent-harness` for launch, scene setup, captures, replay checks, and profiling. Inspect supported commands first; do not invent harness verbs. Where commands cannot express the fixture, implement the scoped client debug fixture rather than claiming the test ran.

| Criterion | Required observation/evidence |
| --- | --- |
| Reproducible comparison | Manifest records baseline/experiment commits, hardware/driver, packs, fixed seed, scene/light parameters, camera path, material settings, 1920 x 1080 resolution, fixed exposure, sun/environment, warmup and frame/tick identifiers. Use identical fixture inputs across all five worktrees. |
| Height and direction | Same-XY lights at 15/40/65 m; receivers at those heights and between them; continuous source/receiver travel across plane boundaries; upward/downward/sloped/side normals; unobstructed illumination between different planes. Preserve the actual XYZ distance and full-normal response in outputs. |
| Visibility | Light behind terrain ridge, thin wall, overhang, and moving mesh; lit and blocked receivers on different heights. Show occluded/unoccluded reference pairs and light-leak differences. Record the geometry traced and any omitted geometry explicitly. |
| Light density | Fixed nested sets of 100, 300, and 1000 point/area lights at mixed heights; same emitters, power, range, and seed across candidates. Capture both gameplay-scale zoom-out and close inspection. |
| Technique quality | DDGI: capture a colored diffuse bounce between heights with direct contribution disabled only in the diagnostic output; show dark occluded regions, thin-wall leakage, probes near geometry, convergence from cleared history, and a moving source/occluder. Compare indirect-only and direct-plus-indirect outputs separately. |
| Matched visual evidence | Baseline, candidate, and converged reference screenshots for every scene, plus video or equivalent ordered frame sequences for moving lights/occluders, camera pan/zoom, cell-boundary crossing, and history reset. Inspect water/terrain/models and retained smoke. Preserve floating-point diagnostic outputs where display tone mapping hides differences. |
| Honest radiometry | Record legacy MAX blending and rgba8/tone-map effects separately from candidate additive linear HDR. Use common exposure/materials; include linear direct/indirect diagnostics and overlapping-light scenes rather than tuning exposure per candidate. |
| Cost and lag | Quality settings first: report per-pass and total GPU time, tracing/acceleration update cost, peak/persistent GPU bytes, update frequency, convergence and visible lag in frames/ms. Compare baseline/candidate timings only via the two-path selector in one process using `query_profile`'s documented caveats. Standalone reference timings are descriptive; no cross-process speedup ratios or guessed pass/fail budget. |
| Integrity | No validation errors during motion, resource rebuild, or camera-cell change; record client/server replay/CRC evidence through the harness showing visual state remains outside simulation. |

Finish by retaining the exact branch/commit and evidence bundle and presenting the user with quality findings, limitations, and measured costs for adoption, defer, or reject. Do not automatically land the experimental renderer onto primary. Any accepted production change requires its own approved scope and normal landing guards.

## Sources

[DDGI paper](https://www.jcgt.org/published/0008/02/01/paper-lowres.pdf) and [volume integration guidance](https://github.com/NVIDIAGameWorks/RTXGI-DDGI/blob/main/docs/DDGIVolume.md). Implement the algorithm in the engine's GLSL path; no wholesale SDK/framework import.

