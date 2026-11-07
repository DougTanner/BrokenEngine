# Planar Radiance Cascades

Revisit When: local emissive weapons and effects need visible occlusion by ships or cliffs, and the current lighting spread cannot deliver it within the agreed frame budget.

## Objective and scope

Option 1: investigate a camera-centered planar lighting field using 2D radiance
cascades (RC), including Holographic Radiance Cascades (HRC) as a candidate.
This is an alternative to [Option 2: screen-space RC](ScreenSpaceRadianceCascades.md).
Both are manual feature proposals; saving either document approves no implementation.

Target the selected local-light contribution on terrain, ships, and water at
the engine's distant RTS camera scale. Neither a full 3D world volume nor ray
tracing hardware is required. Keep direct sun lighting, shadows, and reflections
in their existing roles initially. Do not promise physically complete GI or
multiple bounces before a prototype establishes their need and cost.

## Existing integration anchors

Paths below are relative to the repository root.

- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp` records lighting
  deposit and spread before the main image render pass; this is a natural
  scheduling location for a planar field independent of visible scene depth.
- `Engine/Data/Shaders/Lighting/LightingSpread.frag` gathers radial rings.
  Terrain elevation changes low-layer spread distance and decay; it does not
  provide per-ray blocker tests.
- `Engine/Source/Graphics/Render/LightingUniforms.cpp` builds a camera-centered,
  texel-snapped XY area and rebases retained areas when the camera cell changes.
- `Engine/Data/Shaders/Lighting/LightCombine.comp` combines and tone maps the
  spread, with a separate ambient result. `LightingTemporal.comp` in the same
  directory reprojects retained lighting areas.
- `Engine/Data/Shaders/ShaderFunctions.h` provides directional EWNS sampling
  consumed by terrain, model, and water shading. The current flight-plane
  layer contract is owned by `kiLightingLayers`; water has specialized sampling.

## Candidate approach and stages

1. **Define the lighting contract.** Inventory deposited light sources and
   receiving materials. Decide which existing local contribution RC replaces,
   its emission units, and the linear HDR boundary before tone mapping. Do not
   trace the tone-mapped combine output or add RC over the same old spread.
2. **Build planar inputs.** Represent emission and blockers or absorption within
   the finite XY footprint. Terrain height alone cannot represent ship hulls,
   overhangs, shields, or effect opacity. Choose which geometry deposits masks,
   how moving sources update, and how thin or sub-texel sources remain visible.
3. **Select one cascade construction.** Conventional 2D RC can trace against a
   distance field built from blocker coverage, with jump flooding as a candidate
   preparation step. HRC instead warrants a separately understood short-interval
   construction. These are alternatives to evaluate, not two required backends.
   RC allocates dense spatial samples nearby and finer directional samples for
   farther intervals, then merges their transport information.
4. **Resolve for receivers.** Choose a directional representation and normal
   response, retaining enough angular information for ships and terrain rather
   than reducing every receiver to one scalar ambient value. Explicitly design
   height layers and cross-layer transport; a planar field alone cannot infer
   vertical separation. Establish water and effect receiver behavior separately.
5. **Integrate and compare.** Add Vulkan resources, synchronization, lifetime,
   resize handling, and debug views for inputs, cascade intervals, and resolve.
   Design temporal history for moving blockers and camera changes instead of
   assuming the current area reprojection is sufficient. Compare against the
   baseline before committing to replacement; remove the superseded production
   path if the candidate is selected.

## Unresolved design choices

- Conventional distance-field RC or HRC: obtain enough primary algorithm detail
  to implement the selected method correctly before choosing it.
- Footprint margin, texel density, cascade layout, interval lengths, precision,
  and update cadence: choose from representative scenes and an explicit budget.
- Height representation: retained flight planes, another bounded layer model,
  and the rule for occlusion and light exchange between them.
- Blocker coverage and absorption: opaque hulls, cliffs, shields, smoke, and
  transparent water require explicit inclusion or exclusion and material rules.
- Source and receiver support: emissive effects need deliberate emission data;
  ordinary particles are not already generic local-light receivers.
- Temporal filtering and small-source treatment: balance stability, ringing,
  light leakage, motion trails, and source intensity without hiding failures.

## Invariants and risks

- Client rendering only: simulation, CRC, network records, and saves stay unchanged.
- Positions remain local to cells; convert into the camera's `RenderBasis` at
  use. The unbounded world does not imply unbounded lighting storage.
- Target Vulkan 1.2 without assuming ray tracing extensions. Formats, storage
  image support, and synchronization need verification during implementation.
- Preserve direct-light roles and directional material response; avoid duplicate
  energy and keep display tone mapping out of transport inputs.
- Planar projection can leak across stacked geometry or falsely block unrelated
  heights. Finite coverage can lose edge sources. A distance field can erase
  narrow gaps; cascade merging can ring around small bright moving lights.

## Deferred acceptance and measurements

A future implementation plan must set quality and GPU time/memory budgets before
coding, identify supported sources/receivers, and turn these checks into concrete
acceptance criteria. No engine performance has been measured for this proposal.

Compare baseline and candidate on the same camera, content, GPU, and resolution:

- Emissive projectile beside a ship: visible local response and hull occlusion;
  cliff blocker: reduced light behind it without unrelated height blocking.
- Vertically separated objects: demonstrate the chosen layer policy and its
  limits; rotate receiver normals to check directional response.
- Edge emitter, camera pan/zoom, and camera cell crossing: inspect lost coverage,
  snapping, history resets, flicker, and discontinuities.
- Tiny moving lights: inspect ringing, leakage, temporal trails, and energy;
  include water and effects under their declared support rules.
- Record GPU times for input preparation, cascades/merging, resolve/history, and
  the complete frame, plus peak resource memory. Include dense effects scenes.

Use `/compile` and `/agent-harness` for future build and runtime verification,
including reproducible captures and timing evidence; no runtime checks are part
of saving this feature plan.

## Sources and research limits

- [Original RC paper and source](https://github.com/Raikiri/RadianceCascadesPaper)
  provide the transport hierarchy and context for the screen-space alternative.
- [Holographic Radiance Cascades](https://arxiv.org/abs/2505.02041) is a 2D method
  combining short ray intervals. The full PDF was inaccessible during research;
  exact construction, storage, and performance claims remain unverified here.
- [Fundamental RC](https://m4xc.dev/articles/fundamental-rc/),
  [distance-field foundations](https://jason.today/gi), and
  [RC walkthrough](https://jason.today/rc) are implementation explanations,
  including layout, merging, and artifacts; they do not establish engine costs.
