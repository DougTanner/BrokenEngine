# Water Shaders - Precision-Stable Ocean Rendering

Water renders a visible-area ocean grid. Medium and High quality run a compute
prepass that bakes Gerstner displacement and full-Jacobian normals into two
RGBA16F textures; Low issues no compute work and the vertex stage stays flat
without sampling those textures. The fragment stage combines depth color,
sampled normals, reflections, lighting, shadows, and smoke.

## Geometry and Shore Contracts

- Active LOD grid dimensions must match both displacement textures. Water-mesh
  recreation recreates the grid and textures together.
- Water quality selects wave bands without changing their tuned counts: Low runs
  no displacement, Medium runs the low-frequency band, and High runs both low-
  and medium-frequency bands.
- Low-frequency Gerstner amplitudes come directly from the base values in
  `MainLayout.pf4LowWavesTwo[i].y`; displacement applies no depth-based
  Green's-law shoaling multiplier.
- The low-frequency Gerstner band's breaker boundary weight transitions from
  `Water Break Start Depth` (`S`) to `Water Break End Depth` (`E`). When
  `S < E`, normalized depth `t` is shaped as `pow(t, exponent)` by
  `Water Break Blend Curve` (`gWaterBreakBlendCurve`; `1` is linear, values
  below `1` introduce low earlier); the clamped endpoints remain exact. When
  `S >= E`, the boundary is a hard step at `E`.
- The medium-frequency band starts with the complementary boundary weight
  `1 - low`. After both boundaries are calculated, one smooth shore envelope
  over physical depth `[0, Water Medium Shore Softness]` multiplies the medium
  weight only, taking medium to zero at terrain while leaving the low boundary
  unchanged; `Water Medium Shore Softness` `0` bypasses the envelope. Both
  weights remain bounded. The shore Z taper applies to both bands; over-land
  vertices remain flat and fragments discard where terrain elevation is above
  zero.
- Compute accumulates tangent and bitangent derivatives and uses their cross
  product for the wave normal. Do not replace that full cross-product form with
  the simplified first-order normal: as a band's summed steepness x frequency x
  amplitude approaches 1 — the point where Gerstner waves start folding over on
  themselves — the simplified normal drifts away from the true one and
  eventually tilts the wrong way, while the cross product stays correct.
- The flat-normal dampener is applied once, in the compute prepass, where the
  baked wave normal is blended toward straight up. Tune it at that source; do
  not add a second fade in the vertex or fragment stage.

## Retained Foam

- Foam is bounded coverage in an undisplaced world-space footprint
  ([Render Area Roles](../../../Source/Graphics/Render/AGENTS.md#area-roles)).
  Native displacement-normal W supplies a Jacobian source built from raw,
  un-Nyquist-filtered base amplitudes (`MainLayout` and `bComputeFoam` in
  `WaterDisplacement.comp` own the packed coefficient, formula, and enable
  condition). When emission is enabled, native source sampling is restricted to
  the visible area (`WaterFoam.comp`), so margin texels outside it emit zero
  and their retained coverage decays when foam time advances. Visual
  displacement and normals keep mesh-quad Nyquist filtering and camera-framing
  fades (`MainLayout` and `PopulateGerstnerWaves` own the filtered fields,
  amplitude-scale publication, and vertex fetch gate). Foam generation fades
  independently with camera height, and retained coverage continues decaying
  once generation reaches zero (`ComputeWaterFoamEmissionRate` and
  `WaterFoam.comp` own the fade-endpoint and source-sampling conditions).
  Terrain masking and the compression threshold apply at each native sample
  before bilinear source interpolation. Coverage resolution does not increase
  native source detail.
- After terrain rejection, fixed Glass noise sampled at mip 0 at the undisplaced
  native position adds a signed offset to J before the compression threshold.
  Negative offsets promote emission and positive offsets suppress it; zero
  amplitude preserves the unmodified J source. Spawn amplitude and tile size
  are independent of material breakup controls.
- Reproject history from mip 0, rejecting beyond its half-texel border before
  sampling; when the footprint grows to a coarser bucket, preserve narrow
  retained peaks (`WaterFoam.comp` `main` owns the local-maximum sampling rule
  and width-ratio cutoff). Integrate emission toward saturation and half-life
  decay with the exact exponential update when foam time advances, then write
  the whole allocation. Reset skips history. Inactive foam skips the coverage
  pass and all fragment foam sampling, lighting, and blending while preserving
  ordinary water reflection attenuation; activation and history policy belongs
  to [Render Precision and History](../../../Source/Graphics/Render/AGENTS.md#precision-and-history).
- Presentation samples generated history mips at the undisplaced position.
  Independently selected BC7 UNORM primary color and BC4 UNORM secondary noise
  each have their own tile size independent of color noise: the primary read
  supplies RGB albedo and a linear alpha erosion heightfield (with `.a == 1`
  for opaque textures), and the secondary read supplies linear red breakup.
  Decode primary RGB with `pow(rgb, 2.2)`, matching Model materials; primary
  alpha and secondary red remain undecoded. Primary RGB does not affect
  opacity. Erosion and opacity (`Water.frag`) must derive their UV gradients
  before terrain discard for RTS-altitude mip safety (no post-discard
  derivatives). Blend separately lit water and foam after attenuating only the
  water base by reflection; foam opacity also suppresses skybox specular. Keep
  mixed water/foam albedo for downstream EWNS lighting. Breakup amount selects
  the noise blend. Sample both in undisplaced material coordinates with no time
  scroll, so Gerstner geometry stretches and squeezes the pattern. Material
  controls do not change coverage.

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
- Independently rotated normal samples are transformed back into world
  orientation before weighted composition. Guard a zero weighted sum before
  normalization so fully faded samples cannot produce NaN.

## Lighting Contracts

- Skybox One extra sampling: capture position and geometry-normal derivatives
  before terrain rejection and sample branches; every normal sample uses the
  original full-pixel unwrapped gradients; the extra samples read the fragment's
  patch triangles while `iWaterNormalExtraSamplePatchBlend` is set and plain
  octave One reads otherwise. Nonpositive factors contribute zero and stay in
  the denominator. Only Skybox One samples extra.
- Shoreline skybox-lobe reductions are applied once, scaling each lobe's
  intensity: zero preserves a lobe, one removes it at the shoreline, and deep
  water remains unchanged.
- Specular beach-lobe reduction uses `Water Break End Depth` as its depth
  threshold; it does not use the displacement blend interval or
  `Water Medium Shore Softness`.
- View-dependent shading — the eye vector, reflected ray, and reflection Fresnel
  — reads `MainLayout.f4WaterEyePosition`, never `f4EyePosition`; base-height
  lighting and smoke projections keep the true eye line (`ToEyeLine`).
- Lighting samples the single lighting layer at the directional position from
  `ProjectWaterReflection` and at `BaseHeightPosition` for ambient lighting
  (`ReadAmbientLighting`, whose ambient sample smoke reuses per
  `../Smoke/AGENTS.md`). `BaseHeightPosition` derives from the undisplaced world
  position so ambient lighting and smoke coordinates stay stable under wave
  animation.
- Sun-driven energy and skybox sun/moon specular use full effective shadow. Sky
  ambient uses the separately controlled ambient-shadow mix; EWNS (defined in
  the shader hub) and ambient emitter deposits remain unshadowed.
- Terrain ray-march shadow attenuates the sun component but not moon light.
  Object shadow and smoke attenuation apply to both.
