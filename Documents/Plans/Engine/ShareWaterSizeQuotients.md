<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:51:02.314Z","dependsOn":[]} -->
# Give Each Water Size Quotient One Expression

## Context

Found by the architecture review phase of a deep analysis of
`Engine/Source/Graphics/Render` (the directory, not its subdirectories). Line
numbers are from commit `c675b6b2`, except those of `R/WaterUniforms.cpp`,
which are from the commit that added this Plan; locate by symbol if they
moved. `R/` abbreviates `Engine/Source/Graphics/Render/`.

`R/AGENTS.md` `## Precision and History` requires that, because Water Scale
is applied on the CPU, "each scaled size, frequency, or tile size must be the
identical float in its origin reduction and its published uniform, or the
pattern drifts as the camera moves". Two quotients meet that today only
because the same expression is copied:

- Normal size, three bands: published by `RenderLightingMain` at
  `R/LightingUniforms.cpp:234-236`
  (`gLightingSampledNormals*Size / gWaterScale`), reduced by
  `PopulateWaterReducedUv` at `R/WaterUniforms.cpp:58-60`. Six expressions
  for three values.
- Color-noise frequency: reduced at `R/WaterUniforms.cpp:156`, published by
  `PopulateWaterParameters` at `:277`
  (`gWaterColorNoiseFrequency / gWaterScale`).

Three comments carry the rule (`R/LightingUniforms.cpp:232`,
`R/WaterUniforms.cpp:56`, `:277`).

There is no current bug: every site is a `float / float` of the same two
operands, the reduction sites widen to `double` after the division, and all
sites run on the main thread in one frame.

Unmet criterion: each scaled size or frequency has one expression that both
its reduction and its publication read.

## Design

Recommended (the author's recommendation; rationale given):

- Color noise: compute the quotient once in `PopulateWaterParameters` and
  pass it to `PopulateWaterReducedUv`; both are in `WaterUniforms.cpp`, so no
  new interface is needed.
- Normal size: one function returning the `float` quotient for a band,
  declared in `R/Render.h`, called at `R/LightingUniforms.cpp:234-236` and
  `R/WaterUniforms.cpp:58-60`. Rationale: the two sites are in different
  files and different entry points, so a shared function is the smallest
  shape that removes the copies; current duplication exists, so a helper is
  within the root `AGENTS.md` rule.

Each replacement returns the same `float / float`, so published and reduced
values do not change.

## Critical files

- `Engine/Source/Graphics/Render/WaterUniforms.cpp`
- `Engine/Source/Graphics/Render/LightingUniforms.cpp`
- `Engine/Source/Graphics/Render/Render.h`

## In scope

- `PopulateWaterReducedUv`: the three `fSizeBase*` initializers and the
  `fNoiseFrequency` initializer, and its parameter list.
- `PopulateWaterParameters`: the `fWaterColorNoiseFrequency` write and the
  `PopulateWaterReducedUv` call.
- `RenderLightingMain`: the three `fLightingSampledNormals*Size` writes.
- One function declaration in `Render.h`.
- The three comments named above, where the shared expression makes them
  redundant.

## Out of scope

- The `LerpAtHeight` fades at `R/LightingUniforms.cpp:242-244` and
  `R/WaterUniforms.cpp:62`: they have different output endpoints and share
  only constants `CameraBase` already owns, so they are not duplication.
- Moving any water-normal Main field out of `RenderLightingMain`. ModelViewer
  calls `RenderLightingMain` unconditionally and `RenderWaterMain` only when
  the world is visible
  (`Projects/ModelViewer/Source/ModelViewerApplication.cpp:1487-1491`), so
  such a move would stop publishing those fields in its world-hidden case.
- The foam tile sizes and the wave parameters.
- The wrapper minimums and `PopulatePatchLattice`.

## Risk tier

Tier 2 — scoped behavior. Trigger: a function is added to `Render.h`, a
header shared across the render sources, which Tier 1's "no public signature
exposure" excludes. Values are unchanged. Render is outside the CRC: no
determinism, serialization, wire, save, replay, layout, or threading
exposure.

## Acceptance criteria

- Each of the four quotients is written as a division in exactly one place.
- `/compile` builds the BrokenEngineSandbox client and ModelViewer.
- With default settings, the agent presentation continuity probe reports the
  same `noiseFrequency` as before the change. The evidence behind this Plan
  records no harness setup; any in-game view serves.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- `Documents/Plans/Engine/PositiveWaterColorNoiseFrequency.md` concerns the
  same color-noise frequency but edits the wrapper minimums and
  `PopulatePatchLattice`, and lists `PopulateWaterReducedUv` as out of its
  scope. The two do not touch the same lines and need no ordering.
