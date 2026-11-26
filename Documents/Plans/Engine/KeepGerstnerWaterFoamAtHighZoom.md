<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-26T01:21:51.461Z","dependsOn":["Documents/Plans/Engine/SingleLayerSmoke.md"]} -->
# Keep Gerstner Wave-Based Water Foam Active at High Zoom

## Context

During live testing of retained ocean foam at high RTS camera altitudes, the
user reported that the zoomed-out foam handoff looks "absolutely awful" and
asked to replace the synthetic noise handoff so foam continues to come from the
actual Gerstner wave-based generation at high camera zooms.

Current code evidence:

- `Engine/Data/Shaders/Water/WaterFoam.comp:25-71` (`NativeSource`): when
  `mainLayout.fWaterFoamHighZoomBlend > 0.0f` (lines 56-70), the shader blends
  `fJacobianSource` into a synthetic two-octave `spawnNoiseTextureSampler`
  whitecap source (`fHighZoomSource`) scrolled by
  `mainLayout.f2WaterFoamHighZoomScroll`.
- `Engine/Data/Shaders/Water/WaterDisplacement.comp:73-185`: visual
  displacement and normals already scale by `mainLayout.fWaterLowAmplitudeScale`
  and `mainLayout.fWaterMediumAmplitudeScale`, while the foam Jacobian
  accumulators (`fFoamA`, `fFoamB`, `fFoamD`) stay un-faded by camera framing
  and write `fJacobianDet` into `normalImage.w`.
- `Engine/Source/Graphics/Render/WaterUniforms.cpp:365-435`
  (`PopulateGerstnerBand`) and `480-499` (`PopulateGerstnerWaves`): wave-based
  foam still dies out at high zoom because `PopulateGerstnerBand` multiplies
  every wave's `rStaging.pfBaseAmplitude[i]` by a mesh-quad Nyquist filter
  `fAliasWeight` derived from the coarse water-mesh `fQuadSize`, and zeroes
  `riLayoutCount` when `fAliasWeightSum <= 0.0f`. `PopulateGerstnerWaves` then
  sets `fWaterFoamHighZoomBlend = std::clamp(1.0f - fMeanLowAliasWeight, 0.0f,
  1.0f)` and computes `f2WaterFoamHighZoomScroll`.
- `Engine/Data/Shaders/ShaderMainLayout.h:39-42`: carries
  `fWaterLowAmplitudeScale`, `fWaterMediumAmplitudeScale`,
  `fWaterFoamHighZoomBlend`, and `f2WaterFoamHighZoomScroll`.
- `Engine/Data/Shaders/ShaderLayoutsBase.h:140-141`: `WaterFoam.comp` runs at
  `kiWaterFoamWidth` x `kiWaterFoamHeight`, whereas `WaterDisplacement.comp`
  evaluates on the active mesh LOD grid (`iWaterActiveQuadX` x
  `iWaterActiveQuadY`), which coarsens at high zoom.

This issue is outside the approved scope of the active foam-texture/tuning
change and requires a focused design and shader/uniform update of its own.

## Design

1. **Decouple foam Jacobian evaluation from mesh-quad Nyquist filtering**:
   - In `Engine/Source/Graphics/Render/WaterUniforms.cpp`
     (`GerstnerWaveBandStaging`, `PopulateGerstnerBand`,
     `PopulateGerstnerWaves`), keep `fQuadSize`-based mesh-quad Nyquist
     filtering on the visual wave amplitudes consumed by
     `WaterDisplacement.comp`, and publish separate foam-generation wave
     amplitudes (filtered against the foam grid's texel world size rather than
     the coarse water-mesh quad size, without zeroing foam wave evaluation when
     mesh quads outgrow low-band wavelengths).
   - Evaluate Gerstner foam Jacobian compression directly in
     `Engine/Data/Shaders/Water/WaterFoam.comp` at the foam grid's
     `kiWaterFoamWidth` x `kiWaterFoamHeight` resolution instead of reading
     coarse-mesh `normalTextureSampler.w` from `WaterDisplacement.comp`.
   - Remove the redundant foam Jacobian accumulators (`fFoamA`, `fFoamB`,
     `fFoamD`) from `Engine/Data/Shaders/Water/WaterDisplacement.comp` so
     `WaterDisplacement.comp` computes only visual displacement and normals.

2. **Remove the synthetic high-zoom noise handoff**:
   - Remove `fHighZoomSource` and its two-octave `spawnNoiseTextureSampler`
     branch from `NativeSource` in `Engine/Data/Shaders/Water/WaterFoam.comp`
     (keeping the ordinary `fWaterFoamSpawnNoiseAmount` J perturbation if still
     used for normal wave foam).
   - Remove `fWaterFoamHighZoomBlend` and `f2WaterFoamHighZoomScroll` from
     `Engine/Data/Shaders/ShaderMainLayout.h` and their population in
     `PopulateGerstnerWaves` (`Engine/Source/Graphics/Render/WaterUniforms.cpp`).
   - Update `Engine/Data/Shaders/Water/AGENTS.md` `## Retained Foam` to
     document the wave-based high-zoom foam contract and remove the retired
     two-octave handoff description.

3. **Preserve existing contracts**:
   - Keep `ShaderMainLayout.h` scalar-layout alignment, CPU/GLSL parity, and
     `BT_ENGINE` compatibility (client and server builds both include it).
   - Keep `PopulateGerstnerBand` (`Engine/Source/Graphics/Render/WaterUniforms.cpp`)
     mesh-quad Nyquist filtering on the visual amplitudes consumed by
     `WaterDisplacement.comp` so coarse water vertices do not alias or flicker
     as quad size re-latches.
   - Keep `WaterFoam.comp` terrain masking (`elevationTextureSampler >= 0.0f`),
     history reprojection, exponential emission/decay, and `iWaterFoamEnabled`
     gating intact.

## Critical files

- `Engine/Data/Shaders/Water/WaterFoam.comp`
- `Engine/Data/Shaders/Water/WaterDisplacement.comp`
- `Engine/Source/Graphics/Render/WaterUniforms.cpp`
- `Engine/Data/Shaders/ShaderMainLayout.h`
- `Engine/Data/Shaders/Water/AGENTS.md`

## In scope

- `Engine/Source/Graphics/Render/WaterUniforms.cpp`: `GerstnerWaveBandStaging`,
  `PopulateGerstnerBand`, and `PopulateGerstnerWaves` — separating
  foam-generation wave amplitudes (filtered for the foam texel world size) from
  `fQuadSize`-filtered visual wave amplitudes, and removing
  `fWaterFoamHighZoomBlend` and `f2WaterFoamHighZoomScroll` writes.
- `Engine/Data/Shaders/ShaderMainLayout.h`: removing `fWaterFoamHighZoomBlend`
  and `f2WaterFoamHighZoomScroll`, and adding or adjusting only the wave/foam
  uniform fields needed for foam-grid Gerstner Jacobian evaluation.
- `Engine/Data/Shaders/Water/WaterDisplacement.comp`: removing the redundant
  foam Jacobian accumulators (`fFoamA`, `fFoamB`, `fFoamD`) while continuing to
  consume the mesh-quad Nyquist-filtered visual amplitudes from
  `PopulateGerstnerBand`.
- `Engine/Data/Shaders/Water/WaterFoam.comp`: removing the two-octave
  `fHighZoomSource` handoff and evaluating Gerstner wave Jacobian compression
  directly at the foam grid's `kiWaterFoamWidth` x `kiWaterFoamHeight`
  resolution.
- `Engine/Data/Shaders/Water/AGENTS.md`: updating `## Retained Foam` to match
  the implemented wave-based high-zoom behavior.

## Out of scope

- Foam presentation shading in `Engine/Data/Shaders/Water/Water.frag` (material
  albedo, erosion, breakup, lighting, and reflection blending).
- Foam history reprojection, half-life decay, and reset policy in
  `PopulateWaterFoam`.
- Water mesh LOD bucket selection and quad-snapping policy in
  `Engine/Source/Graphics/CameraBase.h` / `CameraBase.cpp`.
- Tweaks UI controls in `WaterWrappersBase.h` / `TweaksScreenWater.cpp`.
- Any simulation, CRC, save/replay, or network code.

## Acceptance criteria

- `fWaterFoamHighZoomBlend`, `f2WaterFoamHighZoomScroll`, and the two-octave
  `fHighZoomSource` synthetic whitecap branch are removed from
  `ShaderMainLayout.h`, `WaterUniforms.cpp`, `WaterFoam.comp`, and
  `Engine/Data/Shaders/Water/AGENTS.md`.
- Retained ocean foam continues to generate from Gerstner wave Jacobian
  compression at high RTS camera altitudes where visual low-band waves fade or
  mesh quads outgrow low-band wavelengths, without reintroducing vertex-mesh
  aliasing or high-zoom foam flicker.
- Client and server builds (including DataPacker SPIR-V shader compilation)
  succeed cleanly.
- Live `/agent-harness` verification captures screenshots at low, medium, and
  high RTS camera altitudes with water quality above Low and water foam
  enabled, confirming the reported synthetic-noise high-zoom look no longer
  reproduces. (The supplied user report came from manual live testing and
  records no specific harness command sequence.)

## Notes

- **Change Workflow risk tier**: Tier 3 — invariant/integration (removing and
  adjusting fields in shared CPU/GLSL `MainLayout` at
  `Engine/Data/Shaders/ShaderMainLayout.h` changes the shared uniform buffer
  data layout consumed by both client and server builds, which
  `.agents/references/risk-tiers.md` excludes from Tier 2).
- **Verification exposure**: requires DataPacker shader export (SPIR-V),
  client/server compilation (`ShaderMainLayout.h` is shared across both
  targets), and live `/agent-harness` visual verification across zoom levels.
