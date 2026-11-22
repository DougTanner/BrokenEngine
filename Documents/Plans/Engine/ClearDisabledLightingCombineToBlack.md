<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-22T17:45:42.942Z","dependsOn":[]} -->
# Clear disabled lighting combine outputs to exact black

## Context

Disabling player-facing Lighting (`gLightingEnabled = false`) sets
`gbLightingTemporalReset = true` in `RenderLightingGlobal`
(`Engine/Source/Graphics/Render/LightingUniforms.cpp:120-124`), suppressing
deposit and spread draws (`giLightingRefreshLayer = -1` at `:80,337`) while
dispatching one reset refresh of `LightCombine.comp`, `LightingTemporal.comp`,
and `LightingHistoryCopy.comp` per active lighting layer (`:79,352-358`). The
lighting-disable contract (`Engine/Source/Graphics/Render/AGENTS.md:83-89`)
requires that reset to clear combine outputs and temporal history to black
(`0.0`), leaving no world lighting contribution while disabled.

Because `LightCombine.comp:78-143` unconditionally runs the Uchimura tone curve
on the cleared zero spread inputs using the live `fCombineBlackTightness`
uniform (`LightingUniforms.cpp:144`), any nonzero Combine Black Tightness
(`gCombineBlackTightness` in `[0.0, 0.5]`, `LightingWrappersBase.h:76`,
`TweaksScreenLighting.cpp:93`) with `gCombineHuePreserve < 0.999` (default
`0.0`, `LightingWrappersBase.h:79`) writes a nonzero pedestal instead of black.
With `gCombineBlackTightness = 0.25` and `gCombineHuePreserve = 0.0`,
`LightCombine.comp:40,100` evaluates `Uchimura(vec4(0.0f)) = vec4(0.25f)` and
stores `0.25` into `combineImageR`, `combineImageG`, `combineImageB`, and
`combineImageAmbient` (`:136-143`). `LightingTemporal.comp:36` skips history
blending on reset (`fBlend >= 1.0f`), and `LightingHistoryCopy.comp:28-31`
copies `0.25` into `historyImageR/G/B/Ambient` for each lighting layer. With
intermediate `gCombineHuePreserve` in `[0.001, 0.999)`, `LightCombine.comp:123`
writes `b * (1.0 - fHuePreserve) > 0`.

Even after `Documents/Plans/Engine/StabilizeLightingToneCurve.md` fixes
Uchimura overflow and monotonicity, that Plan intentionally preserves the
effective black lift `b` at zero input for enabled lighting, so disabled resets
still publish a nonzero pedestal unless the disabled combine path bypasses tone
mapping. This pre-existing defect surfaced during code review of lighting
deposit culling (`LightCombine.comp`, `LightingUniforms.cpp:136-155` untouched
at baseline, outside that change's scope).

## Design

Author's recommendation: signal disabled lighting to `LightCombine.comp` by
publishing `0.0f` to `rGlobalLayout.fSpreadPassCount` in `RenderLightingGlobal`
(`LightingUniforms.cpp:180`) whenever `!bLightingEnabled`, while keeping the
local `fPassCount = gSpreadPassCount.mfCurrent` (`>= 1.0f`) for
`fCombinePassNormalizationScale` and `fCombinePassTotalScale` (`:156-161`). In
`LightCombine.comp::main`, check `if (uiPassCount == 0u)` immediately after
bounds checking and reading `fSpreadPassCount` (`:78-79`); when zero, skip the
spread-sampling loop and all Uchimura evaluation, store `vec4(0.0f)` into
`combineImageR`, `combineImageG`, and `combineImageB`, store
`vec4(0.0f, 0.0f, 0.0f, 1.0f)` into `combineImageAmbient`, and return.

Rationale: `gSpreadPassCount` has minimum `1.0f` (`LightingWrappersBase.h:28`)
and `LightingSpread.frag:56` is the only other reader of `fSpreadPassCount`,
which never draws when `!bLightingEnabled` (`giLightingRefreshLayer = -1`).
Reusing `fSpreadPassCount == 0.0f` keeps `GlobalLayout` unchanged (avoiding a
CPU/GPU struct change), avoids wasted spread texture reads and tone-curve math
during disabled resets, leaves `DebugTexture.frag`'s `fCombine*` uniforms
unmodified, preserves record-once command buffers and
`LightingTemporal.comp` / `LightingHistoryCopy.comp` behavior, and guarantees
exact black on disable and on texture recreation while disabled regardless of
any Combine slider setting.

## Critical files

- `Engine/Source/Graphics/Render/LightingUniforms.cpp:120-181`
- `Engine/Data/Shaders/Lighting/LightCombine.comp:69-144`
- `Engine/Data/Shaders/ShaderGlobalLayout.h:177` (comment only)
- `Engine/Data/Shaders/Lighting/LightingTemporal.comp:28-37` and
  `Engine/Data/Shaders/Lighting/LightingHistoryCopy.comp:21-32` (unchanged
  downstream consumers)
- `Engine/Source/Ui/LightingWrappersBase.h:28,71-79` and
  `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp:88-96`
  (unchanged slider definitions)

## In scope

- `RenderLightingGlobal` in `Engine/Source/Graphics/Render/LightingUniforms.cpp`
  (`fSpreadPassCount` publication when `!bLightingEnabled`).
- `main` in `Engine/Data/Shaders/Lighting/LightCombine.comp` (zero-pass early
  black store before spread sampling and tone mapping).
- Comment update on `GlobalLayout::fSpreadPassCount` in
  `Engine/Data/Shaders/ShaderGlobalLayout.h` and synchronized docs in
  `Engine/Data/Shaders/Lighting/AGENTS.md` and
  `Engine/Source/Graphics/Render/AGENTS.md` if needed.

## Out of scope

- Enabled-lighting Uchimura tone-curve stability, `P <= m` clamping, shoulder
  `exp` overflow, and `DebugTexture.frag` parity (owned by
  `Documents/Plans/Engine/StabilizeLightingToneCurve.md`).
- `GlobalLayout` binary layout changes, command-buffer topology, lighting layer
  count, `LightingWrappersBase.h` ranges, `LightingTemporal.comp`,
  `LightingHistoryCopy.comp`, and unit tests.

## Risk triggers and invariants

Tier 2: scoped client lighting render and shader behavior in one subsystem.
No `GlobalLayout` binary layout, determinism/CRC (render-only), serialization,
`.pack`/`kiVersion`, replay, wire, affinity, threading, or allocation changes.
Shader compilation and live client/ModelViewer verification apply.

Invariants:
- When `bLightingEnabled` is true, `rGlobalLayout.fSpreadPassCount` remains
  `gSpreadPassCount.mfCurrent` (`>= 1.0f`) and `LightCombine.comp` output is
  unchanged.
- `fCombinePassNormalizationScale` and `fCombinePassTotalScale` always divide
  by `gSpreadPassCount.mfCurrent >= 1.0f`, never by `0.0f`.
- When `bLightingEnabled` is false, every disabled combine reset (initial
  disable edge and any lighting texture recreation while disabled) stores exact
  `(0, 0, 0, 0)` into `combineImageR/G/B` and `(0, 0, 0, 1)` into
  `combineImageAmbient`, propagated to `historyImageR/G/B/Ambient` for every
  active lighting layer regardless of `gCombineBlackTightness`,
  `gCombineHuePreserve`, or other Uchimura sliders.

## Acceptance criteria

1. `/compile` builds the BrokenEngineSandbox client and ModelViewer with shader
   data.
2. Through `/agent-harness`, in a scene with active lights, set `Combine Black
   Tightness` to `0.25` with `Combine Hue Preserve` at `0.0` (default) and
   again at `0.5` and `1.0`. Toggle player-facing `Lighting` off and dump
   `Combine`, `AmbientCombine`, `LightingHistory`, and `AmbientHistory` for
   every active lighting layer (two layers today, or one if single-layer
   lighting has landed): every texel in `Combine` and `LightingHistory` is
   exact `0` in all channels, and every texel in `AmbientCombine` and
   `AmbientHistory` is exact `(0, 0, 0, 255)`. Also verify that a lighting
   texture recreation (such as resize) while `Lighting` remains off keeps all
   four targets exact black, and toggling `Lighting` back on restores lit
   combine output including the configured black tightness. The supplied review
   recorded code and arithmetic analysis and no live capture sequence.

## Notes

- Complementary to `Documents/Plans/Engine/StabilizeLightingToneCurve.md`, which
  owns enabled-lighting Uchimura shoulder overflow, `P <= m` clamping, and
  `DebugTexture.frag` parity while preserving black lift `b` at zero input.
  Neither Plan is a directional prerequisite of the other.
- Works identically under two lighting layers or after
  `Documents/Plans/Engine/SingleLayerWorldLighting.md` collapses to one layer.
