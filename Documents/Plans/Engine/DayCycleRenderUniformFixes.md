<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:50:55.497Z","dependsOn":[]} -->
# Day-Cycle Render Uniform Fixes

## Context

Found by the architecture review phase of a deep analysis of
`Engine/Source/Graphics/Render` (the directory, not its subdirectories). Line
numbers are from commit `c675b6b2`; locate by symbol if they moved. `R/`
abbreviates `Engine/Source/Graphics/Render/`. Two independent fixes to the
CPU-side populators in `R/GlobalUniforms.cpp` and `R/WaterUniforms.cpp`.

### Westward shadow march skips elevation column 0

`Engine/Data/Shaders/Shadow/Shadow.comp:38-40` marches `i` from
`x + iShadowStartOffset + iShadowIncrement` while `i != iEnd`, with `iEnd` =
`globalLayout.iShadowElevationSize`. `PopulateShadowSunExtension`
(`R/GlobalUniforms.cpp:388`) publishes the loop parameters:

- East (`:410-412`): end = elevation texture width, increment +1. The last
  column sampled is width - 1, the texture's last column.
- West (`:422-424`): end = 0, increment -1. The last column sampled is 1;
  column 0 is never an occluder.

So the two directions are not mirror images. Nothing in
`Engine/Data/Shaders/Shadow/AGENTS.md` `## Terrain Shadow Contract` says the
exclusion is intended.

Size of the effect: skipping column 0 equals an elevation extension one texel
narrower, weighted by a distance falloff that is near zero at that range.
`fShadowDistanceFalloff` ends the march earlier (`Shadow.comp:43`) unless half
the shadow-area width falls below `gShadowDistanceFalloff`, which can happen
near the minimum framing; that was not evaluated against the field of view.
No visible defect was shown. This is the lowest-priority item of the
analysis.

Unmet criterion: the westward march samples through elevation column 0,
matching the eastward march through the last column.

### Water directional rebuilds the day window

`PopulateWaterDirectional` (`R/WaterUniforms.cpp:27-39`) rebuilds the day
window from `fSunAngle`: `1 - a / XM_PIDIV2` on `[0, pi/2)`,
`(a - XM_PIDIV2) / XM_PIDIV2` on `[pi/2, pi)`, else 1, then publishes its
square as `fWaterDirectional`.

Its caller `PopulateWaterParameters` (`R/WaterUniforms.cpp:267`) already
receives `fDayPercent`, computed by `PopulateDayCycleFeatherWindows`
(`R/GlobalUniforms.cpp:219-227`) from the same `fSunAngle`
(`R/GlobalUniforms.cpp:606,647,650`): `a / XM_PIDIV2` on `[0, pi/2]`,
`1 - (a - XM_PIDIV2) / XM_PIDIV2` on `(pi/2, pi]`, else 0. The caller uses it
at `:276` and `:281`.

`fDirectional` equals `1 - fDayPercent`. Branch by branch it is bit-equal
everywhere except on `(pi/2, pi)` when `(a - XM_PIDIV2) / XM_PIDIV2` is below
0.5, where `1 - fDayPercent` differs by at most 2^-25 because the inner
subtraction rounds; the published square then changes by less than 1e-7.

So the day window is defined twice, in a file `R/AGENTS.md`
`## Ordering and Publication` says the day cycle does not live in ("The whole
day cycle resolves on the CPU in `GlobalUniforms.cpp`"). A day-window edit in
`GlobalUniforms.cpp` today leaves water directional on the old window.

Unmet criterion: `fWaterDirectional` derives from the `fDayPercent`
`PopulateWaterParameters` receives; no second day-window ramp in
`WaterUniforms.cpp`.

## Design

The two steps are independent; either order works.

1. Westward shadow march: change the literal at `R/GlobalUniforms.cpp:422`
   from `0` to `-1`. With end = -1 and increment -1 the loop samples column 0
   and terminates; the start index is never below 0 because
   `iShadowStartOffset` is the elevation extension half-width, at least 1.
2. Water directional: replace the call at `R/WaterUniforms.cpp:290` with a
   direct write of the squared `1.0f - fDayPercent`, and delete
   `PopulateWaterDirectional`. `fSunAngle` stays a parameter of
   `PopulateWaterParameters` for `PopulateWaterSunsetFade`.

Both are the authors' recommendations, not user decisions.

## Critical files

- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`
- `Engine/Source/Graphics/Render/WaterUniforms.cpp`

## In scope

- `R/GlobalUniforms.cpp`, `PopulateShadowSunExtension`: the
  `iShadowElevationSize` assignment in the westward branch.
- `R/WaterUniforms.cpp`:
  - `PopulateWaterDirectional` (deleted).
  - `PopulateWaterParameters`: the one call at `:290`.

## Out of scope

- Renaming or commenting `iShadowElevationSize` in
  `Engine/Data/Shaders/ShaderGlobalLayout.h`; either touches the shared
  layout header for wording only, and the field carries no comment the new
  literal would make wrong.
- `Shadow.comp` and the eastward branch of `PopulateShadowSunExtension`.
- The shadow distance falloff and the shadow area extension.
- `PopulateWaterSunsetFade`: its ramp uses `pi/16` margins and has no
  existing equivalent.
- `PopulateDayCycleFeatherWindows` and every other day-cycle term.

## Risk tier

Tier 2 — scoped behavior. Triggers: one client-only render path computes a
different result (one more occluder column in the westward terrain shadow
march), and a published render value (`fWaterDirectional`) changes, by at
most the rounding stated above, in one client-only path. No layout field,
type, or order changes, and no shader edit. Render is outside the CRC
(`R/AGENTS.md` introduction): no determinism, serialization, wire, save,
replay, or threading exposure.

## Acceptance criteria

- The westward branch of `PopulateShadowSunExtension` publishes -1 and the
  eastward branch is unchanged.
- `WaterUniforms.cpp` holds no ramp over `fSunAngle` other than
  `PopulateWaterSunsetFade`, and `fWaterDirectional` is written from
  `fDayPercent`.
- `/compile` builds the BrokenEngineSandbox client and ModelViewer.
- Through `/agent-harness`, a screenshot with the sun in the western half of
  the sky shows terrain shadows with no new artifact at the western edge of
  the view. The evidence behind this Plan records no harness setup.

## Coordination

No directional dependencies.

- `Documents/Plans/Engine/ConsolidatedSmallDeduplications.md` item 9 edits
  `PopulateWaterParameters` in `R/WaterUniforms.cpp` (the
  `fWaterColorNoiseFrequency` write and the `PopulateWaterReducedUv` call),
  the same function this Plan's water directional step edits, on different
  lines. No ordering is required; whichever lands second re-finds
  `PopulateWaterParameters` and its sites by symbol from the landed tree.
