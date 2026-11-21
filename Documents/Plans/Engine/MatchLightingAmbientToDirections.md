<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-21T21:01:11.459Z","dependsOn":[]} -->
# Match ambient lighting to the stored directional lobes

## Context

`LightCombine.comp:136-143` stores tone-mapped EWNS in RGBA8 UNORM and builds
ambient from the unclamped floats. For one mapped lobe at 1.25, directional
storage clamps to 1.0, while ambient uses 0.3125 instead of 0.25. This also
applies to negative lobes. Terrain and water smoke expect an average that
agrees with the four directional channels.

## Design

Clamp each color's tone-mapped lobes to [0, 1] before using the same values
for both stores. Retain ambient's one-quarter scale and alpha. Leave normal
UNORM conversion to storage and compare with the reconstructed directional
mean within the resulting per-component quantization error. Keep the current
temporal and history operators on all four textures.

## Critical files

- `Engine/Data/Shaders/Lighting/LightCombine.comp:94-143`
- `Engine/Data/Shaders/Lighting/LightingTemporal.comp`,
  `LightingHistoryCopy.comp`, `Engine/Data/Shaders/ShaderFunctions.h:115-124`,
  `Engine/Data/Shaders/Terrain/Terrain.frag` and `Water/Water.frag` for the
  ambient consumers and their smoke multiplier.

## In scope

- Final RGB lobe range and the directional and ambient stores in
  `LightCombine.comp::main`.

## Out of scope

- Tone curve, color policy, ambient packing and reader conventions,
  temporal equation and storage format changes, and unit tests.

## Risk triggers and invariants

Tier 2 for scoped lighting shader behavior. The directional store retains
its values to UNORM precision. Ambient retains the average of four channels
and smoke retains its existing conversion to a sum.

## Acceptance criteria

1. Calculate the output for one lobe at 1.25 and the rest black, mixed
   saturated lobes, and unsaturated colored inputs. Directional and ambient
   byte outputs agree with the clamped average within one UNORM8 step on a
   reset frame. With settled history, account for both texture conversions
   and the same reprojection and blend on each texture.
2. `/compile` builds the BrokenEngineSandbox client and ModelViewer with
   shader data. Use `/agent-harness` to inspect `Combine`, `AmbientCombine`,
   `LightingHistory`, and `AmbientHistory` for fixed scenes with saturated
   and dim lights and smoke.
   The supplied review recorded arithmetic and no live capture sequence.

## Notes

Use raw bytes for the comparison to avoid inferring ambient from a statement
about the look of smoke alone.
