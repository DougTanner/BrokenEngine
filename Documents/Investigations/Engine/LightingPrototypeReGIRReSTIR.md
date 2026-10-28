# Prototype ReGIR and ReSTIR direct lighting

## Context and authority

The user requested isolated, quality-first lighting prototypes and a plan per technique. Can resampled direct lighting preserve quality and temporal stability with hundreds of dynamic lights? This document fixes an author-proposed candidate for future claim, review, and user approval; authoring it authorizes no renderer implementation or production adoption.

All five independent candidates must start from the same verified commit on the reusable
`lighting-foundation` branch, with no dependency on another candidate. The common foundation is
being completed; its verified commit and final runtime/capture evidence are not yet available.
Do not begin candidate implementation or comparison runs until that commit and evidence are
recorded. Create experiment worktrees through the supported wrapper, then establish the common
foundation ancestry through the repository workflow; the wrapper's current-primary starting point
is not the comparison baseline. Record the actual foundation/candidate commits and executable/pack
byte hashes. Do not adopt or delete worktrees to force a match. The historical
[per-flight-plane investigation](LightingPerFlightPlane.md) is context, not authority for gameplay.

The foundation removes projected local-light deposits and preserves gameplay with exactly zero
local diffuse. It supplies physical fixture sources, resident triangle/material queries, shared
radiometry and comparison inputs. Sun/IBL, shadows, water, smoke, sprites and shield visuals retain
their ordinary paths. The candidate must preserve real XYZ and evaluate the complete normal;
brightness alone is not evidence of better directional quality. Historical legacy captures remain
historical evidence, not a retained runtime comparison path.

## Design

Implement one direct-light path following RTXDI's ReGIR candidate generation and ReSTIR DI reservoir equations, adapted to GLSL. Use a bounded uniform camera-cell-relative 3D ReGIR grid, initially 16 x 16 x 8 cells, with eight light-sample reservoirs per cell. Rebuild candidates every frame from all local lights; use a nonzero uniform-light mixture so important lights are not excluded by the proposal distribution.

At each visible receiver, generate eight initial candidates, perform one temporal reuse pass and one
spatial pass with four neighbors, then evaluate selected-sample source visibility and full-normal
diffuse contribution. Preserve proposal PDFs, sample counts, normalization, and re-evaluated target
densities at reuse sites. Use geometric position/normal/depth rejection for disocclusion and moving
receivers. Area samples carry emitter position and sampling density; source IDs must be stable
across frames and invalidated on removal. Keep a high-candidate, no-reuse diagnostic output beside
the exhaustive reference to distinguish estimator error from reuse artifacts.

ReGIR stores candidate light distributions, not finished radiance. ReSTIR DI estimates direct illumination and requires the tracing prerequisite below for local shadows; neither supplies diffuse bounce lighting. Use explicit client-only light identities, geometry history, and motion reconstruction without adding simulation identity fields or a generic temporal framework.

## Implementation prerequisite

Use the verified foundation's Vulkan ray-query backend, resident terrain and rigid/skinned model
producers, material evaluation, fixed fixture, exhaustive direct reference and capture evidence.
Do not rebuild those common mechanisms inside this candidate. Confirm the selected physical GPU's
required features/extensions/formats and retain its capability evidence; unsupported hardware
blocks comparison rather than selecting a heightfield or screen-space substitute.

Trace resident terrain, opaque models and overhangs under the foundation's material policy:
MASK is opaque; BLEND, water, smoke, sprites and shields do not occlude rays. Water remains a
diffuse receiver. Preserve synchronous Frame borrowing, final GPU geometry/ranges, camera-cell
coordinates, resource retirement and capture input lifetimes without asset/pack format changes.
Verify static, moving and off-screen occluders before judging the candidate algorithm. A missing
producer or failed foundation gate requires plan revision rather than an unrecorded substitute.

## Critical files

- `Engine/Source/Graphics/Lighting/Lighting.cpp` and its producers: reuse physical fixture source publication and resident scene queries; gameplay light collections remain zero physical-source stubs.
- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp`: candidate composition and dispatch ordering.
- `Engine/Source/Graphics/Managers/{DeviceManager,InstanceManager,BufferManager,PipelineManager,RenderTargetTextures}.cpp`: capability enablement, resources, descriptors, synchronization, and recreation.
- `Engine/Data/Shaders/Lighting/{LightingCommon.h,LightingQuery.h,LightingRadiometry.h}`: shared source, material/query and radiometry contracts.
- `Engine/Data/Shaders/{Model/ModelForward.h,Terrain/Terrain.frag,Water/Water.frag}`: actual receiver position/normal and final composition.
- Proposed client-only `Engine/Source/Graphics/Debug/LightingPrototype.{h,cpp}` and `Engine/Data/Shaders/Lighting/PrototypeReGIRReSTIR*.comp`: candidate implementation reusing the foundation fixture/reference.

## In scope

- The critical-file regions above, their required headers/project membership, and candidate resources needed by this single technique; reuse foundation ray-query ownership. Add `Engine/Data/Shaders/Lighting/PrototypeReGIRReSTIR*.comp` for cell candidates, receiver reservoirs, temporal/spatial reuse, and visibility; `ShaderFunctions.h` for final full-normal direct contribution.
- Reuse the foundation fixture/light stream and capture manifest through supported debug/harness controls; no new simulation fields or network commands.
- A debug selector for foundation zero diffuse versus this one candidate, plus reference/diagnostic outputs. Preserve the zero path and reset candidate history when selected. This permits valid within-process paired profiling; it is not a multi-technique runtime framework.
- Reuse the foundation exhaustive shadowed direct-diffuse reference using all lights and its verified 256/512-area-sample convergence evidence. Keep indirect/environmental lighting fixed across candidate and direct-reference views.
- Client-only timers, resource-byte accounting, source/settings manifest, comparison screenshots and motion sequences. Integrate model/terrain/water local diffuse lighting; retain existing smoke/billboard policy and report its mismatch separately.

## Out of scope

Production adoption/landing of the experimental renderer, optimization to a guessed frame budget, algorithm variants, multiple hardware backends, generic configuration systems, speculative scalability, gameplay/flight-plane rules, simulation/CRC, save/replay/wire formats, asset formats, and unit tests. No edits to other prototype branches or the historical investigation. No claim of physically complete GI, volumetric transport, or specular correctness.

## Risk and invariants

Authoring these documents is Tier 1. Executing this proposal requires fresh classification; new CPU/GPU layout contracts, acceleration-resource synchronization/lifetime, and cross-owner geometry integration are prospective Tier-3 triggers, requiring the normal plan reviews and approval before implementation.

Preserve cell-local positions; convert to the camera cell only at publication/use, with retained caches rebased or invalidated on camera-cell changes. All added state is client-only and outside deterministic frames/CRC. Preserve record-once command buffers, in-flight descriptor safety, device recreation, and shared CPU/GLSL layouts. Three flight planes are test heights, never a replacement for full 3D visibility.

## Comparison fixture

Reuse the foundation fixture and its recorded exact blobs/hashes. Its specification is centered at
camera-cell XY (0,0): a 512 m square neutral diffuse receiving
surface, test receivers at Z=6/50/100 m, a vertical wall, a thin wall, an overhang, and a colored
bounce wall. Keep Lambertian albedo at 0.5 (bounce wall RGB 0.8/0.1/0.1), existing specular
parameters, and seed 0x51A7. Reuse the ordered 1000-light set generated by the fixture-owned fixed-seed RNG;
take its first 100/300/1000 entries, with every fifth light a 4 m square area emitter. Distribute XY
uniformly over the square and Z cyclically at 6/50/100 m; give all sources the same recorded linear
power and 150 m range. Freeze the full generated geometry, lights, and camera/event manifest before
collecting results and reuse its exact bytes across candidates.

Capture a close camera at (0,-600,900) looking at (0,0,6), and a wide camera at (0,-4000,6000) with
the same look-at. Include isolated source/receiver pairs at the three heights. Over a fixed
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
| Visibility | Light behind terrain ridge, thin wall, overhang, and moving mesh; lit and blocked receivers on different heights. Show occluded/unoccluded reference pairs and light-leak differences. Record the geometry traced and any omitted geometry explicitly. |
| Light density | Fixed nested sets of 100, 300, and 1000 point/area lights at mixed heights; same emitters, power, range, and seed across candidates. Capture both gameplay-scale zoom-out and close inspection. |
| Technique quality | ReGIR/ReSTIR: record noise and bias against the exhaustive direct reference, candidate support, moving/removed lights, sample identity validity, disocclusion, temporal ghosting, and spatial light leaks. Test a tiny bright source among 1000 lights and a high light passing behind an occluder; reservoir reuse must not flatten XYZ or normals. |
| Matched visual evidence | Foundation zero, candidate, and converged reference screenshots for every scene, plus video or equivalent ordered frame sequences for moving lights/occluders, camera pan/zoom, cell-boundary crossing, and history reset. Inspect water/terrain/models and retained smoke. Preserve floating-point diagnostic outputs where display tone mapping hides differences. |
| Honest radiometry | Compare candidate additive linear HDR against the foundation float reference; keep historical legacy MAX/rgba8 captures separately identified. Use common exposure/materials; include linear direct/indirect diagnostics and overlapping-light scenes rather than tuning exposure per candidate. |
| Cost and lag | Quality settings first: report per-pass and total GPU time, tracing/acceleration update cost, peak/persistent GPU bytes, update frequency, convergence and visible lag in frames/ms. Compare foundation zero/candidate timings only via the two-path selector in one process using `query_profile`'s documented caveats. Standalone reference timings are descriptive; no cross-process speedup ratios or guessed pass/fail budget. |
| Integrity | No validation errors during motion, resource rebuild, or camera-cell change; record client/server replay/CRC evidence through the harness showing visual state remains outside simulation. |

Finish by retaining the exact branch/commit and evidence bundle and presenting the user with quality findings, limitations, and measured costs for adoption, defer, or reject. Do not automatically land the experimental renderer onto primary. Any accepted production change requires its own approved scope and normal landing guards.

## Sources

[RTXDI integration guide](https://github.com/NVIDIA-RTX/RTXDI/blob/main/Doc/Integration.md). Pin the consumed source revision and estimator equations in the manifest; use ReGIR plus ReSTIR DI only, not RTXDI's indirect-lighting features or a full sample-application port.

