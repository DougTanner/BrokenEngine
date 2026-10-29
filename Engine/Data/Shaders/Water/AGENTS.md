# Water Shaders - Precision-Stable Ocean Rendering

Water renders a visible-area ocean grid. Medium and High quality run a compute prepass that bakes Gerstner displacement and full-Jacobian normals into two RGBA16F textures; Low issues no compute work and the vertex stage stays flat without sampling those textures. The fragment stage combines depth color, sampled normals, reflections, lighting, shadows, and smoke.

## Geometry and Shore Contracts

- Active LOD grid dimensions must match both displacement textures. Water-mesh recreation recreates the grid and textures together.
- Water quality selects wave bands without changing their tuned counts: Low runs no displacement, Medium runs the low-frequency band, and High runs both low- and medium-frequency bands.
- Low-frequency Gerstner amplitudes come directly from the base values in `MainLayout.pf4LowWavesTwo[i].y`; displacement applies no depth-based Green's-law shoaling multiplier.
- The low-frequency Gerstner band's breaker boundary weight transitions from `Break Start Depth` (`S`) to `Break End Depth` (`E`). When `S < E`, normalized depth `t` is shaped as `pow(t, exponent)` by `Break Blend Curve` (range `0.125`–`8`, default `1` is linear; values below `1` introduce low earlier and values above `1` retain medium longer); the clamped endpoints remain exact. When `S >= E`, the boundary is a hard step at `E`.
- The medium-frequency band starts with the complementary boundary weight
  `1 - low`. After both boundaries are calculated, one smooth shore envelope
  over physical depth `[0, Medium Shore Softness]` multiplies the medium weight
  only, taking medium to zero at terrain while leaving the low boundary
  unchanged; `Medium Shore Softness` `0` bypasses the envelope. Both weights
  remain bounded. The shore Z taper applies to both bands; over-land vertices
  remain flat and fragments discard above the configured terrain threshold.
- Compute accumulates tangent and bitangent derivatives and uses their cross product for the wave normal. Do not replace that full cross-product form with the simplified first-order normal: as a band's summed steepness x frequency x amplitude approaches 1 — the point where Gerstner waves start folding over on themselves — the simplified normal drifts away from the true one and eventually tilts the wrong way, while the cross product stays correct.
- The flat-normal dampener is applied once, in the compute prepass, where the baked wave normal is blended toward straight up. Tune it at that source; do not add a second fade in the vertex or fragment stage.

## Retained Foam

- Foam is bounded coverage in the undisplaced visible-area footprint. Native
  displacement-normal W supplies the Jacobian source: terrain masking and the
  compression threshold apply at each native sample before bilinear source
  interpolation. Coverage resolution does not increase native source detail.
- After terrain rejection, fixed Glass noise sampled at mip 0 at the undisplaced
  native position adds a signed offset to J before the compression threshold.
  Negative offsets promote emission and positive offsets suppress it; zero
  amplitude preserves the unmodified J source. Spawn amplitude and tile size
  are independent of material breakup controls.
- Reproject history from mip 0, rejecting beyond its half-texel border before
  sampling. Integrate emission toward saturation and half-life decay with the
  exact exponential update, then write the whole allocation. Reset skips
  history. Inactive foam skips the coverage pass and all fragment foam sampling,
  lighting, and blending while preserving ordinary water reflection attenuation;
  activation and history policy belongs to [Render Precision and History](../../../Source/Graphics/Render/AGENTS.md#precision-and-history).
- Presentation samples generated history mips at the undisplaced position.
  Independently selected BC7 UNORM primary color and BC4 UNORM secondary noise
  supply RGB albedo and linear red breakup, each with its own tile size independent
  of color noise. Decode primary RGB with `pow(rgb, 2.2)`, matching Model materials;
  secondary red remains undecoded. Primary RGB does not affect opacity. Clamp
  coverage times the blend from one to secondary red times strength to `[0, 1]`
  as foam opacity. Blend separately lit water and foam after attenuating only
  the water base by reflection; foam opacity also suppresses skybox specular.
  Keep mixed water/foam albedo for diffuse material evaluation. Breakup amount
  selects the noise blend. Sample both in undisplaced material coordinates with no
  time scroll, so Gerstner geometry stretches and squeezes the pattern.
  Material controls do not change coverage.

## Precision Contracts

- Fragment normal and color noise sampling uses camera-relative positions,
  `textureGrad`, and `fract`-wrapped UVs. Foam passes material and spawn UVs
  directly to the mirrored sampler without `fract`, preserving odd-tile
  reflections. Derivatives come from camera-relative positions before scaling
  and wrapping so mip selection remains stable.
- CPU code supplies reduced time offsets and pre-rotates normal origins before
  reduction; do not rotate those origins again in the shader. Sampling
  multipliers must preserve the integer-product modulus contract documented by
  Render (`../../../Source/Graphics/Render/AGENTS.md`).
- Independently rotated normal samples are transformed back into world orientation before weighted composition. Guard a zero weighted sum before normalization so fully faded samples cannot produce NaN.

## Lighting Contracts

- Analytic specular antialiasing is selected by `WATER_SPEC_AA_MODE`. How it passes the per-mip variance from one stage to the next is selected by `WATER_SPEC_AA_MIP_HANDOFF`, which consumes DataPacker's BC5 per-mip variance tables. Changes to either switch require shader repacking; keep analytic LOD and water-normal sampler bias aligned.
- Shoreline skybox-lobe reductions are applied once before every specular antialiasing branch: zero preserves a lobe, one removes it at the shoreline, and deep water remains unchanged.
- Specular beach-lobe reduction uses `Break End Depth` as its depth threshold; it does not use the displacement blend interval or `Medium Shore Softness`.
- View-dependent shading — the eye vector, reflected ray, and reflection Fresnel — reads `MainLayout.f4WaterEyePosition`, never `f4EyePosition`; smoke projections keep the true eye line (`ToEyeLine`).
- Smoke samples at each flight plane's projection from the undisplaced world position, keeping it stable under wave animation.
- Sun-driven energy and skybox sun/moon specular use full effective shadow. Sky ambient uses the separately controlled ambient-shadow mix.
- Terrain ray-march shadow attenuates the sun component but not moon light. Object shadow and smoke attenuation apply to both.
