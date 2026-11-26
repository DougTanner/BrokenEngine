<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-21T21:04:06.020Z","dependsOn":["Documents/Plans/Engine/RemoveCompassHaloFromLighting.md"]} -->
# Sample zero-radius lighting spread once per color

## Context

`LightingSpread.frag:54-67,101-143` computes the same coordinate for every
tap when a pass's effective spread distance is zero. The default first
pass has eight rings with 8 through 15 directions, or 92 taps and 276 RGB
texture samples at the same position per fragment. The pass also repeats
the same deposit compression on that sample.

## Design

When the effective radius after height fade is exactly zero, sample each
RGB EWNS texture once. Apply the pass-0 deposit compressor to those
samples before the corrected normalized transport. Derive the result by
collapsing identical tap inputs under the final discrete operator, using
its per-lobe gain and any conserved energy term. Per-lobe division by the
sum of the same coefficients gives identity before decay in the normalized
operator. Keep the existing decay, output compressor and curve-weighted
snapshot. Where a later pass has zero radius, use the same raw center
sample for its additive accumulation contribution.

## Critical files

- `Engine/Data/Shaders/Lighting/LightingSpread.frag:52-169`
- `Engine/Source/Ui/LightingWrappersBase.h`, `gSpreadDistance`,
  `gSpreadRingCount` and `gSpreadDirectionCount`.

## In scope

- The zero-radius gather in `LightingSpread.frag::main` and sharing the
  equivalent source reads within that branch.

## Out of scope

- Changing any effective radius or spread operator, pass defaults, texture
  lifetime, temporal publication, or unit tests.

## Risk triggers and invariants

Tier 2 for lighting shader behavior with observable rounding. Preserve
sample location/filtering, the order of both compressors, EWNS gains,
height-dependent decay, accumulation order, and the six outputs. A tiny
nonzero radius uses the general path.

## Acceptance criteria

1. With same settings and deposit data as the immediate predecessor, verify
   `Spread` and `SpreadOnly` parity within the applicable Float16 rounding
   error at zero radius. Cover lit, empty, saturated, dim and unequal EWNS
   inputs, compression on and off, and one-pass and multi-pass settings.
   Near-zero nonzero reach retains the general path and its predecessor
   output. Cover any reachable later zero-radius configuration or prove the
   equivalent result for it from the code and an arithmetic probe.
2. Inspect the compiled shader or capture the zero-radius draw to show the
   path uses one texture sample per color and one deposit compression.
   Through `/agent-harness`, compare fixed-scene dumps and the existing GPU
   Lighting Spread profile with the predecessor, recording workload and
   measured deltas. The source sample count is separate from GPU timing.
3. `/compile` builds the BrokenEngineSandbox client and ModelViewer with
   shader data. The supplied review recorded the source count and no live
   capture sequence.

## Notes

Use a zero-radius camera-height case and a nonzero-radius control on the
same scene to distinguish rounding from a change in coverage.
