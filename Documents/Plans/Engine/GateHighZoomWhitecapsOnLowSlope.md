<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-26T13:43:25.652Z","dependsOn":[]} -->
# Gate High-Zoom Whitecaps on the Low-Band Slope

## Context

A post-landing quality audit of commit `81c17f92c` (which squashed the RGBA
turbulent-band water foam commit `11a45f267`) found that the high-zoom whitecap
handoff produces foam even when the Low wave band is switched off.

Evidence at baseline `804f4cf2d`:

- `Engine/Source/Graphics/Render/WaterUniforms.cpp:480`
  (`PopulateGerstnerWaves`) sets
  `rMainLayout.fWaterFoamHighZoomBlend = std::clamp(1.0f - fMeanLowAliasWeight, 0.0f, 1.0f)`
  whenever `WaterFoamActive()` holds.
- `fMeanLowAliasWeight` is the return value of `PopulateGerstnerBand`
  (`WaterUniforms.cpp:361-435`): the mean per-wave mesh-quad Nyquist weight,
  which depends only on wavenumber and `fQuadSize`. The band's slope reaches
  only `pfBaseAmplitude` (through `fAmplitudeNumerator`), never the weight.
- `Engine/Data/Shaders/Water/WaterFoam.comp:56-70` (`NativeSource`) mixes the
  Jacobian source toward the synthetic two-octave whitecap source
  (`fHighZoomSource`) by that blend alone.
- The Low band's on/off gate is `GlobalLayout::fWaterLowAmplitude`, which
  `WaterUniforms.cpp:314` publishes from `gWaterLowSlope` (the Tweaks
  `Water Low Slope` slider, range down to 0 in
  `Engine/Source/Ui/WaterWrappersBase.h:90`).
  `Engine/Data/Shaders/Water/WaterDisplacement.comp:85` skips the whole Low
  band when `fWaterLowAmplitude <= 0`.

Observed consequence: with `Water Low Slope` at 0 the Low band contributes no
displacement and no Jacobian compression, yet once mesh quads outgrow the
low-band wavelengths the blend approaches 1 and the synthetic whitecaps appear.

## Design

The author recommends the audit's correction, applied CPU-side because the
blend is a uniform-only term (`Engine/Data/Shaders/AGENTS.md`
`## Shared Contract`): in `PopulateGerstnerWaves`, publish
`fWaterFoamHighZoomBlend = 0` when `gWaterLowSlope.mfCurrent <= 0`, the same
value `WaterDisplacement.comp` gates the Low band on. With the blend at 0,
`NativeSource` already returns the Jacobian source and skips the whitecap
reads, so `WaterFoam.comp` needs no change. The scroll may stay as computed;
the shader reads it only when the blend is positive.

## Critical files

- `Engine/Source/Graphics/Render/WaterUniforms.cpp`

## In scope

- The `fWaterFoamHighZoomBlend` assignment in the `bKeepUnfadedForFoam`
  branch of `PopulateGerstnerWaves` in `WaterUniforms.cpp`.

## Out of scope

- `WaterFoam.comp`, `WaterDisplacement.comp`, `ShaderMainLayout.h`, and
  `ShaderGlobalLayout.h` field layout.
- `PopulateGerstnerBand` alias weighting and the Medium band.
- Replacing or removing the high-zoom handoff, owned by
  `Documents/Plans/Engine/KeepGerstnerWaterFoamAtHighZoom.md`.
- Tweaks controls and wrapper ranges.

## Acceptance criteria

- With water quality above Low, water foam enabled, and `Water Low Slope` at 0,
  the synthetic high-zoom whitecap contribution is absent at high RTS camera
  altitude (`fWaterFoamHighZoomBlend` is 0); Jacobian-source foam from the
  remaining bands and foam history may still appear.
- With default water tunables, high-altitude whitecaps look unchanged.
- Client build succeeds. The supplied audit evidence records no harness command
  sequence; live verification sets `Water Low Slope` through the Tweaks screen
  with `/agent-harness` UI input and compares high-altitude screenshots.

## Notes

- Change Workflow risk tier: Tier 2. Trigger: scoped client rendering behavior
  in one subsystem (Graphics Render uniforms); no layout, CRC, serialization,
  wire, or threading surface changes.
- Determinism and CRC: none; water foam is client-only visual state.
- If `KeepGerstnerWaterFoamAtHighZoom.md` lands first and removes
  `fWaterFoamHighZoomBlend`, this defect no longer exists; preparation reports
  the problem gone and `/next-plan` takes its empty-realized-change route
  (`.agents/skills/next-plan/SKILL.md` `## Rules`). Both Plans touch
  `PopulateGerstnerWaves`; this is minor file overlap, not a dependency.
