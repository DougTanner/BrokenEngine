<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-21T21:01:06.784Z","dependsOn":[]} -->
# Preserve lighting below the spread compression threshold

## Context

`CompressHuePreserving` in
`Engine/Data/Shaders/Lighting/LightingSpread.frag:38-49` floors the mapped
average at the threshold and divides by a floored input average. A lobe with
RGB average 0.0005 becomes 0.001 at threshold 0.001 and compression zero. The
deposit and spread output paths share the helper. Its stated zero-compression
identity and below-threshold behavior both fail on valid values.

## Design

Return before touching RGB when the compression coefficient is zero. For each
EWNS lobe, leave its RGB alone when the average is at or below the threshold.
Above the threshold, keep the existing Reinhard map and apply its ratio to
the three source components. A strictly positive average is guaranteed on
that path by the supported nonnegative thresholds. This keeps zero and tiny
positive inputs safe without a denominator floor that changes the map.

## Critical files

- `Engine/Data/Shaders/Lighting/LightingSpread.frag:35-49,130-134,151-162`
- `Engine/Source/Ui/LightingWrappersBase.h`, which defines the deposit and
  per-pass threshold and compression ranges.

## In scope

- `CompressHuePreserving` and its two uses in `LightingSpread.frag`.

## Out of scope

- Deposit sampling, spread geometry, combine tone mapping, slider ranges,
  presets, temporal history, and unit tests.

## Risk triggers and invariants

Tier 2 for scoped lighting shader behavior. Preserve hue per lobe, treatment
of zero energy, pass-0 compress-before-weighting, and per-pass compress after
decay and before snapshot and accumulation. Formats and simulation stay intact.

## Acceptance criteria

1. At each call site, zero compression is identity. With compression enabled,
   zero, tiny positive values, and the exact threshold remain unchanged, and
   just-above and bright values match the current Reinhard formula.
2. A colored input retains its RGB ratios in each compressed lobe. Finite
   nonnegative inputs over the supported coefficient and threshold ranges
   produce finite output no greater than the input average.
3. `/compile` builds the BrokenEngineSandbox client and ModelViewer with
   shader data. An `/agent-harness` fixed-scene lighting dump confirms black
   and dim falloff retained without threshold lift. The supplied review
   recorded arithmetic and no live capture sequence.

## Notes

Use a source calculation or a temporary shader probe for exact threshold
cases smaller than the displayed UNORM quantization step.
