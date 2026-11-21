<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-21T21:01:13.001Z","dependsOn":[]} -->
# Sample the lighting combine curve over active passes

## Context

`LightingUniforms.cpp:157-161` samples the curve at `i / 127` for the current
128-slot capacity, while `LightingSpread.frag:54-56` uses `i / 63` for the
default 64 active passes. `CurveWidget.cpp:36-45` draws one tick for each
capacity slot. The last active snapshot misses the end of either editable
curve, and sampling more of the curve changes its contribution to exposure.

## Design

Put the floating-point sample position calculation
`i / max(activeCount - 1, 1)` next to `gSpreadPassCount` in
`LightingWrappersBase.h`. Use the integral value of `gSpreadPassCount` in
`RenderLightingGlobal` to evaluate the selected curve for active passes and
zero the unused tail of the fixed CPU/GPU array. Pass the same count from
`RenderLightingSection` to `CurveWidget` and use the shared calculation to
submit one tick per active pass to `PlotInfLines`. One pass samples zero.
Multiple passes sample both endpoints.

Keep weights as absolute curve values so linear edits to the curve keep their
gain and an all-zero curve produces black with default black lift. Keep
`gCombinePassNormalize`, `gCombineExposurePassScale`, and the existing final
division by pass count. Report the old and new sums of active weights at fixed
settings to explain the exposure change from sampling the full curve. Leave
preset points in place.

## Critical files

- `Engine/Source/Graphics/Render/LightingUniforms.cpp:151-161,175` and
  `Engine/Source/Ui/LightingWrappersBase.h:28-32,81-83`.
- `Engine/Source/Ui/CurveWidget.h:12`,
  `Engine/Source/Ui/CurveWidget.cpp:13,36-45`, and
  `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp:100-106`.
- `Engine/Data/Shaders/Lighting/LightingSpread.frag:54-56,158-162`,
  `Engine/Data/Shaders/Lighting/LightCombine.comp:78-92`,
  and `Engine/Data/Shaders/ShaderGlobalLayout.h`.

## In scope

- Active sampling and inactive tail of `GlobalLayout::pfCombineCurvePoints`
  in `RenderLightingGlobal`.
- The sample position calculation beside `gSpreadPassCount`, the pass count
  parameter to `CurveWidget` and its `RenderLightingSection` call, and the
  spread-pass ticks in `CurveWidget`.
- Correct the combine-curve paragraph in `Engine/Source/Ui/AGENTS.md:145-149`
  by removing the claim that the presets have identical control points.
  Preserve both presets and the `gbUseCombineCurveNew` A/B toggle. Use the
  code-defined `gCombineCurveOld` and `gCombineCurveNew` values in
  `LightingWrappersBase.h` as the numerical reference for the acceptance checks.

## Out of scope

- Control defaults, curve editing, other plot drawing, pass count controls,
  normalization and exposure controls, texture formats, and unit tests.

## Risk triggers and invariants

Tier 2 for scoped lighting exposure behavior. Keep the shared layout and the
integer active count contract. The curve continues to weight the spread-only
snapshot, before accumulation. The brightness change is derived from the
weights and documented at the same slider settings.

## Acceptance criteria

1. For each preset, verify the sample positions and uploaded weights for one,
   two, the default, and the maximum pass counts. One pass uses the start.
   Both endpoints apply for multiple passes. The maximum-count samples match
   the former positions. Lower the count after a longer chain and verify the
   inactive slots are zero. A flat curve is unchanged and an all-zero curve
   stays zero.
2. `/compile` builds the BrokenEngineSandbox client and ModelViewer with
   shader data. Using `/agent-harness`, set the pass count in Lighting > Write
   and capture the Pass Contribution Curve in Lighting > Combine at each
   count in criterion 1. Verify one tick at zero for one pass and evenly
   spaced ticks from zero to one for multiple passes. Inspect the count and
   positions passed to `PlotInfLines` if ticks overlap the plot border.
3. At a fixed camera and emitter, use two passes and set both Spread
   Accumulation Decay controls to zero for the weight measurement. For each
   unedited preset, capture a lighting refresh through `/agent-harness` with
   RenderDoc and inspect the `LightingSpread1` draw. Record the bound
   `pfCombineCurvePoints[1]` and the raw `SpreadOnly[1] / Spread[1]` ratio for
   the same texel, color, and directional component at that draw. Choose a
   finite, nonzero `Spread` value with both stored Float16 values in the
   normal range. With accumulation at zero, `Spread` holds the unweighted
   pre-accumulation value. Require the bound weight to equal the preset's
   value at `x = 1`, and the stored ratio to agree within 0.5% relative error
   for Float16 rounding. Record the former weight from `CurveData::Evaluate`
   at `1 / (shaders::kiMaxSpreadPasses - 1)` and show that it falls outside
   that tolerance for both presets.
4. Restore the accumulation controls and record the former and new active
   weight sums and measured brightness shift for the same scene and slider
   settings.

## Notes

Keep the existing Float16 debug pass-normalization scale separate from these
curve weights.
