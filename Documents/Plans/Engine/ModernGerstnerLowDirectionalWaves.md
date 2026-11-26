<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-25T18:34:50.433Z","dependsOn":["Documents/Plans/Engine/FftMediumWaves.md","Documents/Plans/Engine/WaterWaveIsolationControls.md"]} -->
# Rewrite Low-Directional Water Waves Using Modern Spectral Asymmetric Gerstner

## Context

The current low-frequency Gerstner wave band
(`Engine/Source/Graphics/Render/WaterUniforms.cpp:360-504` and
`Engine/Data/Shaders/Water/WaterDisplacement.comp:81-107`) was hand-tuned years
ago: it spaces `31` waves geometrically between `0.5 m` and `3.0 m` with equal
steepness (`k * A_i = slope / sqrt(N)`), uniform random directions inside a
narrow cone, and symmetric trochoidal profiles. Like the old medium waves, it
does not look right — waves look artificially uniform in size, lack natural
swell grouping, and have identical front and back slopes.

While medium waves move to multi-cascade GPU FFT in the preceding Plan,
**low-directional waves** (long, coherent directional swells and
coast-approaching wave trains that must never tile across large RTS views)
are well suited to an analytical **modern spectral asymmetric Gerstner**
solver. This Plan rewrites the low-directional wave band **from scratch** with
brand-new math, parameters, and sliders (reusing nothing from the legacy low
band), executed in an **isolated, user-feedback-driven** testing worktree
(low-directional waves everywhere, FFT medium waves and normal-map textures
off), iterating with the user until sign-off.

### Engine Scale and Integration Constraints

- Water mesh LOD 0 is `895 x 511` quads across all gameplay framings
  (`100..800`), with quad sizes from `0.105 x 0.155 m` (`framing = 100`) to
  `0.300 x 0.379 m` (`framing = 300`) and `0.592 x 0.713 m` (`framing = 600`).
- Double-precision camera-cell phase reduction (`CellCenterOffsetDouble` in
  `WaterUniforms.cpp:410-415, 442-444`) must be preserved so wave phases stay
  exact across the unbounded sparse cell grid.
- Nyquist vertex-grid anti-aliasing attenuation (`2..4` quads per wavelength)
  and camera-framing amplitude fade-out (`Fade Start` to `Fade End`) keep
  waves smooth as the camera zooms out.
- `WaterDisplacement.comp` combines low waves with the FFT medium waves under
  the shared render-target and slope-addition contract established by
  `FftMediumWaves.md`: it writes `displacementImage = vec4(Dx_total, Dy_total,
  Dz_total, fMediumWeight)` (packing `fMediumWeight`, computed from undisplaced
  terrain elevation and honoring `gWaterIgnoreCoastMask`, into `w`) and
  `normalImage = vec4(n_low, J_total)`, where `xyz` always stores the unblended
  unit low-wave-only normal `n_low` and `w` is the combined low + FFT Jacobian
  determinant for `WaterFoam.comp` (scaling partials by `s * dD` — omitting the
  envelope-gradient term `D * ds` across the smooth coast transition — is a
  deliberate spatially-varying envelope approximation that is exact wherever
  spatial weights are constant, such as open ocean and isolation mode).
  `Water.vert` forwards `displacementImage.w` as `fInMediumWeight` and passes
  `n_low` as `f3InNormal`; in `Water.frag`, when `iWaterFftActive != 0`, it
  computes the per-pixel FFT cross-product normal `n_fft` from mipmapped FFT
  partials weighted by `fInMediumWeight * mainLayout.fWaterMediumAmplitudeScale`
  and combines `n_low = normalize(f3InNormal)` and `n_fft` via
  partial-derivative blending (`normalize(vec3(n_low.xy * n_fft.z + n_fft.xy *
  n_low.z, n_low.z * n_fft.z))`, with no division clamp so `n_fft -> (0, 0, 1)`
  reduces identically to `normalize(f3InNormal)` for every allowed `n_low`) — or uses
  `normalize(f3InNormal)` when inactive — then applies `fWaterWaveNormalBlend`
  unconditionally once into `f3CombinedWaveNormal` (ensuring exact continuity
  across the `fMediumAmplitudeScale -> 0` / `iWaterFftActive` cutoff) and
  routes `f3CombinedWaveNormal` and its `dFdx`/`dFdy` derivatives to specular,
  directional lighting, EWNS lighting, and reflection projection.

### Academic Research and Speculative Notes

1. **Spectral Wave-Size Distribution — Christopher J. Horvath (2015),
   *"Empirical Directional Wave Spectra for Computer Graphics"* (ACM DigiPro
   '15, Sections 2-3, Eqs. 11-26) & Luca Donatini et al. (2024, Applied
   Ocean Research, Section 3.2, Eq. 4 directional-bin amplitude and Section
   4.1.4 Fourier spectral mapping adapted to GPU sum-of-Gerstner):**
   - Primary source DOIs:
     - Horvath 2015:
       https://doi.org/10.1145/2791261.2791267
     - Donatini et al. 2024 (Section 3.2, Eq. 4; Section 4.1.4):
       https://doi.org/10.1016/j.apor.2023.103866
   - Replace uniform `k * A_i = const` hand-picked amplitudes with **stratified
     spectral sampling** of a **JONSWAP / TMA** frequency spectrum (Horvath
     2015, Eqs. 11-16) paired with a **Cos-2s / Hasselmann** directional
     spreading distribution (Horvath 2015, Eqs. 20-26).
   - Adapt Donatini et al. (2024, Section 3.2, Eq. 4, adapting Section 4.1.4's
     Fourier spectral mapping) energy-conserving directional spectrum
     discretization to GPU sum-of-Gerstner waves: partition `[kMin, kMax]` into
     log-spaced bins and derive each wave's amplitude from the integrated
     spectral energy in its bin (`A_i = sqrt(2 * S(omega_i) * D(theta_i) *
     dOmega_i * dTheta_i)`), with stratified jitter in frequency and angle to
     break up regularity and create natural wave groups (sets).
2. **Asymmetric Wave Profiles (Steeper Front, Gentler Back) — Darwyn R. Peachey
   (1986), *"Modeling Waves and Surf"* (ACM SIGGRAPH '86, pp. 65-74, Section 3)
   & Alain Fournier & William T. Reeves (1986), *"A Simple Model of Ocean
   Waves"* (ACM SIGGRAPH '86, pp. 75-84, Sections 3-4):**
   - Primary source DOIs:
     - Peachey 1986:
       https://doi.org/10.1145/15922.15893
     - Fournier & Reeves 1986:
       https://doi.org/10.1145/15922.15894
   - Standard Gerstner waves sharpen crests and flatten troughs, but remain
     symmetric front-to-back.
   - **Pinned runnable first profile (Sine-phase warping):** For Broken
     Engine's Gerstner convention (`z = A * sin(theta)`,
     `xy = Q * A * D * cos(theta)`), warp the phase with a sine term:
     `theta_s = theta + c_skew * sin(theta)` (clamped `|c_skew| < 0.7` so
     `d(theta_s)/d(theta) = 1 + c_skew * cos(theta) > 0` stays strictly
     monotonic). Because `sin(theta)` breaks reflection symmetry about `pi/2`
     for the `sin`/`cos` pair, this bunches the forward face and stretches the
     rear slope, with exact chain-rule derivative
     `d(theta_s)/d(theta) = 1 + c_skew * cos(theta)` in the Jacobian. Include
     the peak derivative factor `1 + |c_skew|` in the steepness non-folding
     bound (`sum_i (Q_i * k_i * A_i * (1 + |c_skew|)) < 1`).
   - **(Speculative alternative, follow-up if needed):** Phase-locked 2nd-order
     Stokes harmonic (`2 * theta + phi_asym` scaled by `k * A`) can be tested
     only if interactive visual review shows sine-phase skew is insufficient.
3. **Boundary-Aware / Depth-Aware Shoaling — Stefan Jeschke, Christian Hafner,
   Nuttapong Chentanez, Miles Macklin, Matthias Müller-Fischer, & Chris Wojtan
   (2020), *"Making Procedural Water Waves Boundary-aware"* (Computer Graphics
   Forum 39(8), Sections 3-4, speculative):**
   - Primary source DOI (Jeschke et al. 2020):
     https://doi.org/10.1111/cgf.14100
   - **(Speculative)** Use the finite-depth dispersion relation
     `omega = sqrt(g * k * tanh(k * d))` and/or gentle depth-driven
     steepening/asymmetry boost near coasts once isolation mode is turned off,
     so incoming swells naturally shorten and lean forward as water shallows
     toward islands while staying smooth in deep ocean.
4. **Close-Up Detail at Minimum Zoom (`framing = 100`):**
   - Because low-directional waves are evaluated analytically in
     `WaterDisplacement.comp` (and optionally supplemented by exact analytical
     normals or high-frequency low-wave harmonics), tune the wavelength range
     and wave count so swells look crisp and sculpted even at `framing = 100`
     (`~15 cm` quads) with zero visible polygon faceting, while fading out
     cleanly as the camera zooms out.

## Design

1. **Remove legacy low Gerstner band cleanly:**
   - Remove the old low-wave wrappers (`gWaterLowCount`,
     `gWaterLowDirectionDegrees`, `gWaterLowSpreadDegrees`,
     `gWaterLowWavelengthShortest`, `gWaterLowWavelengthLongest`,
     `gWaterLowSlope`, `gWaterLowChoppiness`, etc.), `PopulateGerstnerBand`'s
     legacy formula, and the legacy low-wave loop in `WaterDisplacement.comp`
     (no backward compatibility).
2. **Spectral Low-Directional Wave Generator (`WaterUniforms.cpp`,
   `ShaderMainLayout.h`, `ShaderGlobalLayout.h`):**
   - Implement a clean-slate CPU spectral sampler that regenerates the
     low-directional wave table whenever low-wave tunables change:
     - Samples `N` waves (`16..128`) across `[L_min, L_max]` using stratified
       log-spaced bins from JONSWAP/TMA energy density and Cos-2s / Hasselmann
       directional spreading around the primary swell direction.
     - Precomputes per-wave direction `(Dx, Dy)`, wavenumber `k`, spectral
       amplitude `A`, dispersion rate `omega`, and per-wave steepness `Q`
       (energy-bounded including the `1 + |c_skew|` sine-phase derivative
       factor so summed steepness never self-intersects/loops).
     - Applies per-frame double-precision camera-cell phase reduction,
       Nyquist 2..4 quad anti-aliasing attenuation, and camera-framing zoom
       fade-out.
3. **Asymmetric Gerstner Evaluation & Combined Jacobian (`WaterDisplacement.comp`):**
   - Evaluate the new asymmetric Gerstner displacement and its analytical
     partial derivatives (`dP/dx`, `dP/dy`) for the low-wave tangent,
     bitangent, low-wave-only normal, and Jacobian determinant. Scaling wave
     partials by `s * dD` (omitting the envelope-gradient term `D * ds` across
     the smooth coast transition for both `fLowWeight` and `fMediumScale`) is a
     deliberate spatially-varying envelope approximation that is exact wherever
     the spatial weights are constant (open ocean and isolation mode).
   - Preserve the shared `normalImage` + slope-addition contract established by
     `FftMediumWaves.md`: write `displacementImage = vec4(Dx_total, Dy_total,
     Dz_total, fMediumWeight)` (packing `fMediumWeight`, computed from
     undisplaced terrain elevation and honoring `gWaterIgnoreCoastMask`, into
     `w` for `Water.vert` to forward as `fInMediumWeight`) and
     `normalImage = vec4(n_low, J_total)`, where `xyz` always stores the
     unblended unit low-wave-only normal `n_low` (`Water.frag` combines `n_low`
     with the per-pixel FFT cross-product normal `n_fft` via slope-addition
     when `iWaterFftActive != 0` using `fInMediumWeight *
     mainLayout.fWaterMediumAmplitudeScale`, or uses `normalize(f3InNormal)`
     when inactive, then applies `fWaterWaveNormalBlend` unconditionally once
     into `f3CombinedWaveNormal` for exact continuity across the
     `fMediumAmplitudeScale -> 0` / `iWaterFftActive` cutoff, and routes
     `f3CombinedWaveNormal` plus its `dFdx`/`dFdy` derivatives to specular,
     lighting, and reflection projection) and `w` is the combined low + FFT
     Jacobian determinant for `WaterFoam.comp` (with FFT partials scaled by
     `fMediumWeight * mainLayout.fWaterMediumAmplitudeScale`, respecting
     `gWaterIgnoreCoastMask` and `gWaterDisableLowWaves` /
     `gWaterDisableMediumWaves`).
4. **Brand-new Low-Directional sliders (`WaterWrappersBase.h`,
   `TweaksScreenWater.cpp`):**
   - Replace the `"Low"` subtab from scratch with physically meaningful
     controls:
     - Wave Count, Swell Direction, Directional Spread / Concentration (`s`),
       Peak Wavelength, Wavelength Range (`Min / Max`), JONSWAP Peak
       Enhancement (`gamma`), Amplitude Scale.
     - Choppiness (horizontal displacement), Front/Back Asymmetry Skew,
       Asymmetry Phase / Profile Bias (speculative), Wave Time Scale.
     - Zoom Fade Start / Fade End.
     - Depth Shoaling / Coast Steepening controls (used after isolated testing
       when re-enabling coast integration).
5. **Interactive user-feedback loop (required during execution):**
   - Launch with isolation active (low-directional waves everywhere, FFT
     medium waves off, normal-map textures off).
   - Present the build and sliders to the user, collect feedback across zoom
     levels (`framing = 100` through `600`), iterate on the pinned sine-phase
     skew profile, spectral shape, and defaults until the user approves
     (testing the speculative 2nd-order Stokes harmonic only if sine-phase
     skew needs replacement).
   - Finally, turn isolation off and verify the **combined ocean** (modern
     Gerstner low-directional swells + multi-cascade FFT medium waves + foam)
     across zoom levels and coastlines with the user before landing.

## Critical files

- `Engine/Data/Shaders/ShaderGlobalLayout.h`
- `Engine/Data/Shaders/ShaderMainLayout.h`
- `Engine/Data/Shaders/Water/WaterDisplacement.comp`
- `Engine/Source/Graphics/Render/WaterUniforms.cpp`
- `Engine/Source/Ui/WaterWrappersBase.h`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenWater.cpp`

## In scope

- Replacing the legacy low-wave CPU generator in `WaterUniforms.cpp` with a
  clean-slate spectral (JONSWAP/TMA + directional spreading) generator.
- Replacing the legacy low-wave shader loop in `WaterDisplacement.comp` with
  asymmetric Gerstner displacement and analytical Jacobian normals and
  determinant (under the `s * dD` spatially-varying envelope approximation).
- Replacing low-wave wrappers in `WaterWrappersBase.h` and the `"Low"` subtab
  in `TweaksScreenWater.cpp` from scratch.
- Interactive slider refinement rounds with the user in isolated mode,
  followed by combined-ocean verification with FFT medium waves.

## Out of scope

- Replacing or redesigning the FFT medium-wave solver (owned by the preceding
  FFT Plan).
- Adding new skybox assets.
- Simulation determinism/CRC, network protocol, or save/replay formats.
- Unit tests.

## Risk triggers and invariants

Tier 2: scoped client water rendering behavior (`BT_CLIENT` only; verify
server build too if shared `ShaderMainLayout.h` / `ShaderGlobalLayout.h`
fields change).

- **No Gerstner looping/folding artifacts:** Total steepness and sine-phase
  skew magnitude (`|c_skew| < 0.7`) must stay bounded with the `1 + |c_skew|`
  derivative factor (`sum_i (Q_i * k_i * A_i * (1 + |c_skew|)) < 1`) so
  `d(theta_s)/d(theta) = 1 + c_skew * cos(theta) > 0` and wave geometry never
  inverts unintentionally.
- **Jacobian normal, envelope approximation, shared `normalImage` +
  slope-addition contract, & foam:** Analytical partial derivatives must match
  the asymmetric displacement formula (with scaling partials by `s * dD` —
  omitting `D * ds` across the smooth coast transition — as a deliberate
  spatially-varying envelope approximation that is exact wherever spatial
  weights are constant, i.e., open ocean and isolation mode) so
  `displacementImage.w` packs `fMediumWeight` for `Water.vert`/`Water.frag`,
  `normalImage.xyz` always stores the unblended unit low-wave-only normal
  `n_low` (`Water.frag` combines `n_low` with `n_fft` when active and applies
  `fWaterWaveNormalBlend` unconditionally once for exact continuity across the
  `fMediumAmplitudeScale -> 0` / `iWaterFftActive` cutoff), and
  `normalImage.w` (combined low + FFT Jacobian determinant `J_total` for
  `WaterFoam.comp`, with FFT partials scaled uniformly by `fMediumWeight *
  mainLayout.fWaterMediumAmplitudeScale`) stay accurate without
  finite-difference artifacts or double-counted FFT slopes.
- **Unbounded cell precision:** Double-precision camera-cell phase reduction
  must remain intact across cell transitions.
- **Zero-work zoom-out:** When `gWaterLevel == kLow`,
  `gWaterDisableLowWaves` is true, or `fLowAmplitudeScale <= 0`, low-wave
  count is `0` and skips evaluation.

## Acceptance criteria

1. `/compile` builds BrokenEngineSandbox client (and server if shared layout
   headers changed) cleanly with zero Vulkan validation errors.
2. In isolated mode (low-directional waves everywhere, FFT medium waves and
   normal-map textures off), waves exhibit natural size distribution, clear
   directional travel, and visible steeper-front / gentler-back asymmetry at
   closest zoom (`framing = 100`) through `framing = 600`.
3. Waves fade out smoothly as the camera zooms out to `Amplitude Fade End`
   and show zero cell-boundary jitter.
4. All new Low-Directional sliders respond live.
5. User explicitly signs off on isolated low-directional waves AND on the
   combined ocean (low-directional + FFT medium + foam) before landing.
