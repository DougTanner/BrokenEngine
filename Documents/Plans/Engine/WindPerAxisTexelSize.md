<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:40:26.414Z","dependsOn":[]} -->
# Fix: Use independent U and V wind texel sizes

## Context

The accepted survivor `CAI/shard-0032/002` shows that
`RenderWindGlobal` publishes only the inverse wind-texture width
(`Engine/Source/Graphics/Render/WindUniforms.cpp:39-43`).  Wind textures are
created with independent width and height (`Engine/Source/Graphics/Managers/RenderTargetTextures.cpp:242-281`),
and `WindSpreadCommon.h` uses the single scalar for both horizontal and
vertical neighbors and for the maximum advection step
(`Engine/Data/Shaders/Wind/WindSpreadCommon.h:6-16,24-32`).  Ordinary 16:9
rendering therefore samples vertical neighbors at the wrong UV distance.

The source report is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0032.md:71`;
the consolidated selector is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:876`.
All seven frozen target rows matched baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; no source or shader change was
made in this routing session.

Impact: wind diffusion, vorticity, advection, and smoke coupling are
systematically anisotropic on normal non-square client windows.

## Design

Author's recommendation: replace the scalar GlobalLayout field with a
two-component inverse extent, publish `(1/width, 1/height)` from the existing
wind textures, and make the shader use `.x` for U neighbors and `.y` for V
neighbors.  Preserve the square-window advection behavior while making the
three-texel limit anisotropic by measuring displacement in texel units:
normalize by the two-component inverse extent, limit that metric to three, and
convert back to UV displacement.  Update the dual-language layout and every
CPU/GLSL consumer together.

## Critical files

- `Engine/Data/Shaders/ShaderGlobalLayout.h:80-108` — shared field layout.
- `Engine/Source/Graphics/Render/WindUniforms.cpp:39-43` — inverse extent
  publication.
- `Engine/Data/Shaders/Wind/WindSpreadCommon.h:4-32` — neighbor and advection
  consumers.
- `Engine/Source/Graphics/Managers/RenderTargetTextures.cpp:242-281` — wind
  width/height source extents.
- `Engine/Data/Shaders/ShaderLayoutsBase.h:45-69,81-99,367-370` — dual-
  language scalar-layout contract (reference; keep CPU/GPU sizes identical).

## In scope

- Changing the GlobalLayout wind texel field to per-axis inverse extents and
  updating its CPU publication.
- Updating WindSpreadCommon neighbor reads and advection clamp to use the
  corresponding axes while retaining the existing three-texel metric on square
  targets.
- Rebuilding/validating all shader consumers and the CPU/GPU layout contract.

## Out of scope

- Wind texture sizing, smoke-area mapping, occupancy buffers, ping-pong order,
  or tuning values.
- Unrelated shader layout fields, descriptor bindings, render passes, and
  simulation/network formats.
- Server code, save/replay/wire data, and unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: a dual-language uniform layout and
GLSL kernel change crosses CPU publication, shader compilation, and wind/smoke
GPU behavior; CPU/GPU contract and independently owned shader integration are
Tier-3 surfaces.

Preserve these invariants:

- `f2WindTexelSize.x == 1/width` and `.y == 1/height` for both ping-pong wind
  textures.
- Horizontal and vertical neighbor reads advance exactly one texel on their
  corresponding axes.
- The advection limit remains three texels in a texel-scaled metric and square
  targets retain the prior result.
- CPU and GLSL GlobalLayout offsets/sizes remain identical; no simulation CRC,
  wire, save, replay, or `.pack` bytes change.

## Acceptance criteria

- Shader reflection/layout validation confirms the new two-component field has
  the same CPU/GPU offset and expected total size.
- A 16:9 wind run shows one-row U/V neighbor offsets and a three-texel
  anisotropic advection limit; a square run matches the pre-change metric.
- Client Debug and Release builds and shader compilation pass `/compile`; a
  harness smoke/wind scenario has no layout or validation error.
- A scoped search finds no scalar `fWindTexelSize` consumer left in the tree.

## Notes

This is the render/shader counterpart to the separate wind-occupancy recovery
Plan; neither changes the wind simulation's ping-pong state ownership.
