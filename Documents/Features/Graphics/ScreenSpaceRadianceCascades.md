# Screen-Space Radiance Cascades

Revisit When: visible emissive effects need geometry-aware indirect illumination, and a renderer prototype can justify the scene-input and pass-order changes within the agreed frame budget.

## Objective and scope

Option 2: investigate PoE2-style screen-space radiance cascades (RC) for the
engine's distant RTS camera. This is an alternative to
[Option 1: planar RC, including HRC](PlanarRadianceCascades.md).
Both are manual feature proposals; saving either document approves no implementation.

The original RC paper describes a flatland-like screen-space probe hierarchy
populated by screen-space raymarching for Path of Exile 2. This proposal uses
that documented approach as inspiration; it does not claim to reproduce the
current shipping renderer. Neither a full 3D world volume nor ray tracing
hardware is required.

Target a bounded local-light/indirect contribution from visible scene data,
initially retaining direct sun lighting, shadows, and reflections separately.
Treat single indirect bounce as the starting scope, not a promise of complete
world lighting or hidden/offscreen source recovery.

## Existing integration anchors

Paths below are relative to the repository root.

- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp` records lighting
  deposit and spread before the main image pass. Screen-space RC needs scene
  inputs first, so it cannot simply occupy the existing spread slot unchanged.
- `Engine/Source/Graphics/Managers/SwapchainManager.cpp`, `CreateFramebuffers`,
  creates depth with attachment-only usage. The HDR framebuffer contains color,
  depth, and optional MSAA color; it supplies no general scene-normal target.
- `Engine/Data/Shaders/Model/Model.frag`, `Terrain/Terrain.frag`, and
  `Water/Water.frag` have normals during material shading. Making suitable
  normals, material response, and emission available to RC is deliberate work.
- `Engine/Data/Shaders/Lighting/LightCombine.comp` tone maps the current local
  field. It is not automatically a linear radiance source. `LightingTemporal.comp`
  in the same directory reprojects planar areas rather than scene surfaces.
- `Engine/Data/Shaders/ShaderFunctions.h` supplies directional local lighting;
  terrain, models, and water have established consumers. Replacing their selected
  local contribution must preserve normal response and avoid double counting.

## Candidate approach and stages

1. **Define sources and receivers.** Specify emission, direct-lit source color,
   albedo/material response, units, and the selected local contribution to replace.
   Separate self-emission and indirect transport so surface color is not applied
   twice. Ordinary particles are not already generic local-light receivers.
2. **Expose scene inputs.** Design shader-readable depth with a valid MSAA
   resolve or another explicitly chosen input path. Choose stored normals versus
   reconstruction, and obtain material/emission data without assuming depth can
   recover them. Define clear values, background, depth precision, and supported
   transparent surfaces before implementing targets or descriptors.
3. **Reorder rendering.** Produce depth and direct/emissive source data before
   tracing; build RC; then apply the resolved indirect contribution before final
   display tone mapping. Select a prepass, intermediate pass, or shading split
   based on existing materials. Never feed the current frame's RC-lit output back
   as its own source. Account for Vulkan barriers, layouts, per-frame resource
   lifetime, MSAA, and resize recreation.
4. **Build the probe hierarchy.** Use near/far ray intervals with dense nearby
   spatial sampling and finer distant directional sampling. Raymarch the visible
   representation, merge cascades, and resolve for material normals. Derive exact
   depth-hit, thickness, interpolation, and merging rules from primary sources
   and a focused prototype; the paper is not a complete production recipe.
5. **Stabilize and compare.** Add input/probe/hit/resolve debug views. Choose
   surface-aware history rejection for motion and disocclusion; do not assume
   planar area reprojection handles them. Compare against the baseline, then
   remove the superseded production contribution if this candidate is selected.

## Unresolved design choices

- Scene representation and pass split: available normals, material parameters,
  linear HDR sources, and the cost of producing them for the existing renderer.
- Probe placement, directional basis, interval coverage, trace thickness, depth
  sampling, and cascade resolution at the engine's zoom levels.
- Source coverage and receiver support for water, shields, smoke, and emissive
  effects; visible depth cannot fully describe transparent or hidden geometry.
- Edge and miss policy: offscreen sources and occluders are absent; select an
  explicit bounded response rather than promising recovered world lighting.
- Temporal validity, motion data needs, and treatment of small moving sources,
  disocclusion, ringing, depth discontinuities, and camera cuts.
- Which existing local illumination is replaced, and how retained direct light
  and reflection paths compose with RC without duplicate energy.

## Invariants and risks

- Client rendering only: simulation, CRC, network records, and saves stay unchanged.
- Reconstructed positions use the camera cell's `RenderBasis`; no absolute world
  positions. The unbounded world does not require an unbounded lighting field.
- Target Vulkan 1.2 without assuming ray tracing extensions. Verify selected
  depth formats, sampling/resolve support, and synchronization during implementation.
- Preserve directional material response and keep transport inputs linear before
  display tone mapping. Final tone-mapped scene color is not the source buffer.
- Screen visibility limits are inherent: behind-camera lights, hidden emitters,
  thin occluders, transparent layers, and silhouette edges can leak or disappear.
  Camera movement can change lighting even while world geometry is unchanged.
- Additional scene passes and bandwidth may outweigh cascade tracing costs.
  Existing planar flight-layer behavior cannot be assumed to transfer unchanged.

## Deferred acceptance and measurements

A future implementation plan must set quality and GPU time/memory budgets before
coding, declare source/receiver and edge policies, and turn these checks into
concrete acceptance criteria. No engine performance has been measured here.

Compare baseline and candidate on the same camera, content, GPU, and resolution:

- Emissive projectile beside a ship and a cliff blocker: inspect visible indirect
  response, occlusion, self-lighting, leakage, and preservation of direct lighting.
- Vertically separated objects and changing receiver normals: check depth hits,
  thickness assumptions, and directional resolve at the intended zoom range.
- Move an emitter across the screen edge and behind geometry: document the
  chosen miss policy and distinguish expected missing data from implementation bugs.
- Pan/zoom and cross a camera cell boundary: inspect history, disocclusion,
  flicker, seams, and energy changes. Include tiny moving lights and ringing.
- Exercise water/effects under their declared support rules, MSAA on/off, and
  resize; inspect depth sampling, transparent composition, and stale resources.
- Record GPU times for scene-input preparation, cascades/merging, resolve/history,
  and the complete frame, plus peak resource memory. Include dense effects scenes.

Use `/compile` and `/agent-harness` for future build and runtime verification,
including reproducible captures and timing evidence; no runtime checks are part
of saving this feature plan.

## Sources and research limits

- [Original RC paper and source](https://github.com/Raikiri/RadianceCascadesPaper),
  particularly `RadianceCascades.tex`, describe the PoE2 screen-space hierarchy,
  screen-space raymarching, missing hidden/offscreen information, and primarily
  single-bounce scope. Production implementation detail is incomplete; current
  shipping behavior has not been independently verified.
- [Fundamental RC](https://m4xc.dev/articles/fundamental-rc/) and the
  [RC walkthrough](https://jason.today/rc) explain hierarchy layout, merging,
  and artifacts. Their examples do not establish this engine's scene-input
  architecture, visual quality, or performance.
