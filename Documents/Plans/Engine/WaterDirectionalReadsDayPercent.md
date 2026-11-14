<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:50:55.497Z","dependsOn":[]} -->
# Derive Water Directional From the Day Percent Its Caller Holds

## Context

Found by the architecture review phase of a deep analysis of
`Engine/Source/Graphics/Render` (the directory, not its subdirectories). Line
numbers are from commit `c675b6b2`; locate by symbol if they moved. `R/`
abbreviates `Engine/Source/Graphics/Render/`.

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

Recommended: replace the call at `R/WaterUniforms.cpp:290` with a direct
write of the squared `1.0f - fDayPercent`, and delete
`PopulateWaterDirectional`. `fSunAngle` stays a parameter of
`PopulateWaterParameters` for `PopulateWaterSunsetFade`.

## Critical files

- `Engine/Source/Graphics/Render/WaterUniforms.cpp`

## In scope

- `PopulateWaterDirectional` (deleted).
- `PopulateWaterParameters`: the one call at `:290`.

## Out of scope

- `PopulateWaterSunsetFade`: its ramp uses `pi/16` margins and has no
  existing equivalent.
- `PopulateDayCycleFeatherWindows` and every other day-cycle term.

## Risk tier

Tier 2 — scoped behavior. Trigger: a published render value changes, by at
most the rounding stated above, in one client-only path. No layout change and
no shader edit. Render is outside the CRC (`R/AGENTS.md` introduction), so no
determinism, serialization, wire, save, replay, or threading exposure.

## Acceptance criteria

- `WaterUniforms.cpp` holds no ramp over `fSunAngle` other than
  `PopulateWaterSunsetFade`, and `fWaterDirectional` is written from
  `fDayPercent`.
- `/compile` builds the BrokenEngineSandbox client and ModelViewer.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
