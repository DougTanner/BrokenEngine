<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-25T18:34:44.806Z","dependsOn":[]} -->
# Add Water Wave Layer Isolation Controls and Zero-Normal Fallback

## Context

Both upcoming wave rewrites (multi-cascade FFT medium waves and modern
spectral asymmetric Gerstner low-directional waves) are user-feedback driven
and require testing one wave layer in complete isolation across the entire
ocean:

1. Show the wave layer under test everywhere on the ocean, not masked to the
   coast or offshore depth transition.
2. Disable the other wave layer (e.g., disable low waves while tuning medium
   waves, and vice versa).
3. Disable the legacy high-frequency normal-map textures so only the
   displacement wave geometry and wave normals are visible.

Code inspection revealed an existing shader defect when normal-map textures
are disabled: in `Engine/Data/Shaders/Water/WaterNormalSampling.h:111-112`,
when all three normal-map band weights are `0.0`, `f3WeightedSum` is
`vec3(0.0)` and `result.f3SampledNormal` becomes `vec3(0, 0, 0)` instead of a
valid unit normal. Downstream consumers (`WaterSpecular.h:16`,
`Water.frag:232`, `WaterReflectionProjection.h:24-25`) then attenuate
`f3LightingNormal` by `fWaterNormalBlendWave` (not renormalized) or risk
`normalize(vec3(0))` NaN when `fSkyboxNormalBlendWave == 0`.

Adding clean isolation toggles and fixing the zero-normal fallback once as a
small prerequisite keeps both wave-rewrite sessions focused solely on wave
physics, shaders, and interactive slider refinement.

## Design

1. **Fix zero-weight normal-map fallback (`WaterNormalSampling.h`,
   `Water.frag`, `WaterSpecular.h`, `WaterReflectionProjection.h`):**
   - In `WaterNormalSampling.h`, track whether sampled normal-map detail is
     active (`length(f3WeightedSum) > kfEpsilon`) via a `bHasSampledNormal`
     flag on `WaterNormalSamplingResult`.
   - When `bHasSampledNormal` is false (either `gWaterDisableNormalTextures` is
     enabled or `length(f3WeightedSum) <= kfEpsilon`), fall back to the
     geometry normal at full weight without `(1 - blend)` attenuation or
     `normalize(vec3(0))` NaN at every consumer; when all four isolation
     toggles are `false` and `length(f3WeightedSum) > kfEpsilon`, output
     matches baseline identically:
     - `WaterSpecular.h:16` (primary specular path) uses `f3InNormal`.
     - `WaterSpecular.h:59` (multi-tap Skybox One Sampling ring loop) uses each
       offset sample's `f3SampleGeometryNormal` when
       `!sampleNormalResult.bHasSampledNormal`.
     - `Water.frag:232` and `WaterReflectionProjection.h:24` use `f3InNormal`.
2. **Add isolation wrappers (`WaterWrappersBase.h`):**
   - `gWaterDisableLowWaves` (bool, default `false`) — forces
     `iWaterLowCount = 0` in `RenderWaterMain`.
   - `gWaterDisableMediumWaves` (bool, default `false`) — forces medium wave
     contribution off in `RenderWaterMain` / compute prepass.
   - `gWaterDisableNormalTextures` (bool, default `false`) — forces
     `fWaterNormalWeightOne/Two/Three = 0.0f` in `LightingUniforms.cpp`.
   - `gWaterIgnoreCoastMask` (bool, default `false`) — bypasses the depth
     split (`fLowBoundaryWeight`, `fMediumBoundaryWeight`, `fShoreEnvelope`)
     in `WaterDisplacement.comp` so active wave bands render at full weight
     everywhere over water (`fTerrainElevation < 0.0f`), while still keeping
     over-land vertices flat and preserving the shore Z taper at `Z = 0`.
3. **Publish flags to GPU (`ShaderGlobalLayout.h`, `WaterUniforms.cpp`,
   `LightingUniforms.cpp`, `WaterDisplacement.comp`):**
   - Add a compact isolation bitmask or float/uint flag in `GlobalLayout` for
     `gWaterIgnoreCoastMask` (CPU-side gating handles disabling low waves,
     medium waves, and normal textures with zero shader branching overhead).
   - Keep `WaterDisplacement.comp` dispatch active whenever either low or
     medium waves are enabled.
4. **Expose in Tweaks UI (`TweaksScreenWater.cpp`):**
   - Add an `"Isolation"` section (or checkboxes at the top of the Water
     tweaks tab) for the four toggles so the user and downstream wave sessions
     can switch between isolated testing and full combined ocean rendering in
     one click.

## Critical files

- `Engine/Data/Shaders/ShaderGlobalLayout.h`
- `Engine/Data/Shaders/Water/WaterNormalSampling.h`
- `Engine/Data/Shaders/Water/WaterSpecular.h`
- `Engine/Data/Shaders/Water/WaterReflectionProjection.h`
- `Engine/Data/Shaders/Water/Water.frag`
- `Engine/Data/Shaders/Water/WaterDisplacement.comp`
- `Engine/Source/Graphics/Render/WaterUniforms.cpp`
- `Engine/Source/Graphics/Render/LightingUniforms.cpp`
- `Engine/Source/Ui/WaterWrappersBase.h`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenWater.cpp`

## In scope

- `WaterNormalSampling.h`, `WaterSpecular.h` (both the primary specular path
  and the Skybox One multi-tap ring loop at `:59`),
  `WaterReflectionProjection.h`, `Water.frag`: safe fallback to `f3InNormal`
  (or `f3SampleGeometryNormal` per offset tap) whenever
  `length(f3WeightedSum) <= kfEpsilon` (covering disabled normal-map textures,
  all-zero weights, and nonzero weights whose weighted sum is at or below
  `kfEpsilon`).
- `WaterWrappersBase.h`, `TweaksScreenWater.cpp`: four isolation checkboxes.
- `ShaderGlobalLayout.h`, `WaterUniforms.cpp`, `LightingUniforms.cpp`,
  `WaterDisplacement.comp`: wiring the four toggles with default-off behavior
  matching current rendering whenever `length(f3WeightedSum) > kfEpsilon`.

## Out of scope

- Changing any wave math, spectrum, or skybox assets.
- Changing default visual appearance when all four isolation toggles are off
  and `length(f3WeightedSum) > kfEpsilon`.
- Adding persistence for isolation toggles in `GraphicsSettings.bin`.
- Unit tests.

## Risk triggers and invariants

Tier 2: scoped client water shader and UI behavior. No simulation CRC, wire,
serialization, save/replay, or threading changes.

- When all four toggles are `false` (default) and
  `length(f3WeightedSum) > kfEpsilon`, rendered output is identical to baseline
  (inputs where `length(f3WeightedSum) <= kfEpsilon` intentionally gain the
  NaN-free geometry-normal fallback).
- When `gWaterDisableNormalTextures` is `true`, no NaNs occur in primary
  specular, multi-tap Skybox One specular, lighting, or reflection projection
  even if `fSkyboxNormalBlendWave` is `0`, and wave normals drive reflections
  and directional lighting at full weight.
- Over-land vertices (`fTerrainElevation >= 0.0f`) remain flat and fragments
  still discard over land even when `gWaterIgnoreCoastMask` is `true`.

## Acceptance criteria

1. `/compile` builds BrokenEngineSandbox client cleanly with zero shader or
   Vulkan validation errors.
2. With all isolation toggles off and `length(f3WeightedSum) > kfEpsilon`, water
   renders identically to baseline.
3. With `gWaterDisableLowWaves`, `gWaterDisableNormalTextures`, and
   `gWaterIgnoreCoastMask` enabled via Tweaks UI (or harness) — and retained
   foam disabled via the existing Water Foam control (`Water.frag:115`) for
   geometry-only inspection — medium waves appear across the entire ocean with
   clean geometry-normal reflections (including multi-tap Skybox One sampling)
   and no NaN/black-water artifacts.
4. With `gWaterDisableMediumWaves`, `gWaterDisableNormalTextures`,
   `gWaterIgnoreCoastMask` enabled, and foam disabled via the existing control,
   low waves appear across the entire ocean cleanly; restoring foam and turning
   isolation toggles off restores combined ocean rendering.
