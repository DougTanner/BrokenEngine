<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-25T18:34:47.535Z","dependsOn":[]} -->
# Rewrite Medium Water Waves Using Multi-Cascade GPU FFT

## Context

The current medium-wave implementation (`Engine/Data/Shaders/Water/WaterDisplacement.comp:109-135`
and `Engine/Source/Graphics/Render/WaterUniforms.cpp:360-504`) was written by
hand years ago as a 63-wave sum of Gerstner sine waves evaluated per grid
vertex every frame (`896 x 512 = 458,752` vertices at default
`gWaterShapeDetail = 0.25`, LOD 0 `895 x 511` quads). It does not look like
real ocean water: discrete hand-tuned waves create repetitive interference
patterns, standard Gerstner waves are symmetric front-to-back, and vertex-only
normals lack crisp detail when zoomed all the way in without legacy normal-map
textures.

This Plan replaces the medium-wave system **from scratch** with a
**multi-cascade GPU compute FFT** ocean solver, starting clean with brand-new
parameters and sliders (reusing nothing from the legacy medium Gerstner band).
Execution is **isolated and user-feedback driven**: implement in the testing
worktree with isolation enabled (medium waves everywhere, low waves and
normal-map textures off), ask the user to test with sliders across zoom levels,
refine, and repeat until the user approves.

`WaterWaveIsolationControls` and earlier water-wave series commits live on
branch `water-waves`, while the primary checkout stays on `main` so other
agents are not disrupted. Because `/next-plan` claims this Plan from `main`,
the session worktree starts at `main`. Keep the session branch at `main`
through `/next-plan` step 6's claim refresh — inspecting `water-waves` code via
Git during step-4/5 preparation and review so `Invoke-NextPlanClaim.ps1` does
not block with `claim.session-diverged` — and move the session worktree onto
`water-waves` only **after** step 6's claim refresh completes (merging `main`
into `water-waves` first if `main` holds prerequisite or overlapping updates),
then implement, build, and verify on `water-waves`, leaving `main`'s code
untouched. At landing (`/finalize-changes`), land the implementation and Plan
completion onto `water-waves` under the global landing lock — either by
switching primary to `water-waves` for preparation and landing together (then
switching primary back to `main`), or via a user-authorized locked update of
`refs/heads/water-waves` while primary stays on `main` — then release the Plan
claim and sync only the Plan deletion and child `dependsOn` edge removals onto
`main` (with no water-wave code on `main`) so `/next-plan` on `main` picks up
the next Plan in the series cleanly.

### Engine Scale and Camera Constraints

- Gameplay camera framing (`Engine/Source/Graphics/CameraBase.h:76-92`) spans
  `kfFramingMinimum = 100.0` (closest zoom), `kfCameraFramingDefault = 300.0`,
  `kfWaveFadeEndFraming = 600.0`, up to `kfFramingMaximum = 800.0`.
- All gameplay framings `100..800` stay in water mesh LOD 0 (`895 x 511`
  quads; LOD 1 begins at framing `1200`). Exact 16:9 quad sizes at default
  `FOV=15°`, `Tilt=20°`:
  - `framing = 100`: `94.08 x 79.36 m` visible -> `0.105 x 0.155 m` per quad.
  - `framing = 300`: `268.36 x 193.43 m` visible -> `0.300 x 0.379 m` per quad.
  - `framing = 600`: `529.79 x 364.53 m` visible -> `0.592 x 0.713 m` per quad.
- Because the world is an unbounded sparse grid of cells
  (`Engine/Source/Frame/AGENTS.md`), world-space UV sampling of periodic FFT
  cascades must reduce camera-cell origin offsets in double precision on the
  CPU (`CellCenterOffsetDouble`, modulo each cascade patch length `L_c`) so
  waves never jitter or jump across cell boundaries.
- `WaterFoam.comp` reads the Jacobian determinant from
  `mWaterDisplacementNormalTexture.w` (`fWaterFoamJStart = 0.7`,
  `fWaterFoamJFull = 0.0`), so the FFT output must supply a compatible
  Jacobian determinant `J` for whitecap/foam generation.

### Academic Research and Speculative Notes

1. **Empirical Ocean Spectra — Christopher J. Horvath (2015), *"Empirical
   Directional Wave Spectra for Computer Graphics"* (ACM DigiPro '15, Sections
   2-3, Eqs. 2-26) & Jerry Tessendorf (2004), *"Simulating Ocean Water"*
   (SIGGRAPH Course Notes, §4.4, Eqs. 42-43):**
   - Primary sources:
     - Horvath 2015:
       https://doi.org/10.1145/2791261.2791267
     - Tessendorf 2004:
       https://people.computing.clemson.edu/~jtessen/reports/papers_files/coursenotes2004.pdf
   - Use the **TMA (Texel MARSEN ARSLOE) / JONSWAP** energy spectrum (Horvath
     2015, Eqs. 11-16) rather than Phillips or Pierson-Moskowitz. JONSWAP
     models fetch-limited seas with realistic peak enhancement, and TMA extends
     JONSWAP with the Kitaigorodskii depth-attenuation factor for finite water
     depth.
   - Use **Hasselmann / Donelan-Banner directional spreading** paired with
     Horvath's normalized **Swell** parameter (`0..1`, Horvath 2015, Eqs.
     20-26) to smoothly control short-crested wind chop vs. elongated
     directional wave trains.
   - Convert discrete frequency-direction density to Cartesian wavenumber
     amplitudes on an `N x N` grid of patch length `L` with
     `k = (2 * pi * m / L, 2 * pi * n / L)` (`m, n in [-N/2, N/2 - 1]`),
     explicitly zeroing DC (`k = 0`) before evaluating `1 / k` and strictly
     excluding self-conjugate Nyquist indices (`m == -N/2` or `n == -N/2`,
     where odd `i * k` derivative multipliers break real-field complex
     packing), with `dk_x = dk_y = 2 * pi / L`, finite-depth dispersion
     `omega(k) = sqrt(g * k * tanh(k * d))`, and discrete bin energy
     `E(k) = S(omega) * D(theta) * (dOmega/dk) * (1 / k) * dk_x * dk_y`:
     `H0(k) = (1 / sqrt(2)) * (xi_r + i * xi_i) * sqrt(0.5 * E(k))` (i.e.,
     `0.5 * (xi_r + i * xi_i) * sqrt(E(k))` with `xi_r, xi_i ~ N(0, 1)`,
     Horvath 2015, Eq. 29; Tessendorf 2004, §4.4, Eq. 42), so
     `E[|H0(k)|^2] = 0.5 * E(k)`. Evolving via Hermitian time evolution
     `H(k, t) = H0(k) * exp(i * omega * t) + conj(H0(-k)) * exp(-i * omega * t)`
     (Tessendorf 2004, §4.4, Eq. 43) and transforming with an unnormalized
     synthesis IFFT (`sum_k H(k, t) * exp(i * k · x)`, no `1 / N^2` divisor)
     sums `H0(k)` with `conj(H0(-k))` to yield strictly real spatial output
     with **expected** height variance `sum_k E(k)` over independent draws for
     the retained bins (a single fixed seed realizes spatial variance
     `sum_k |H(k,t)|^2`).
2. **Multi-Cascade Repeat-Period Extension:**
   - A single periodic FFT tile visibly repeats from a top-down RTS camera.
   - Pin **2 `256x256` cascades** for the runnable first iteration with
     coprime integer patch lengths that extend the combined repeat period to
     `lcm(17, 67) = 1,139 m` (larger than the `530 x 365 m` visible area at
     `framing = 600`) and a seamless half-open `[kMin_c, kMax_c)` wavenumber
     split at `k_split = 6.0 rad/m`:
     - Cascade 1 (mid-scale chop & swells): `L1 = 67.0 m` (texel size ~26 cm),
       `k in [2 * pi / L1 ≈ 0.094, 6.0) rad/m`.
     - Cascade 0 (fine detail, full close-up sharpness at `framing = 100`):
       `L0 = 17.0 m` (texel size ~6.6 cm, resolving waves down to ~13 cm per
       pixel in the fragment shader), `k in [6.0, pi * N / L0 ≈ 47.31) rad/m`.
     - Both cascades also apply the explicit index guard (`m == -N/2 ||
       n == -N/2` -> `H0 = 0`) so self-conjugate Nyquist bins stay excluded
       dynamically whenever live `L0`/`L1` sliders change.
   - **(Speculative follow-up)** A 3rd cascade (`L2 = 193.0 m`) can be added
     only if interactive testing at `framing = 300..600` reveals any residual
     patterning with 2 cascades.
3. **Front/Back Wave Asymmetry — Dennis B. Creamer, Frank Henyey, Robert
   Schult, & Jon Wright (1989), *"Improved Linear Representation of Ocean
   Surface Waves"* (Journal of Fluid Mechanics 205, pp. 135-161, Section 2):**
   - Primary source DOI (Creamer et al. 1989):
     https://doi.org/10.1017/S0022112089001977
   - Standard linear Airy + choppy horizontal FFT displacement produces sharp
     crests and flat troughs, but wave profiles remain symmetric front-to-back.
   - **Pinned runnable first asymmetry operation (`WaterFftMerge.comp`, spatial
     domain):** After the unnormalized IFFT produces real `Dx, Dy, Dz` and
     their spatial derivatives, apply a wind-aligned height-proportional
     horizontal lean `Dx_skew = Dx + lambda_asym * Dz * W_x`,
     `Dy_skew = Dy + lambda_asym * Dz * W_y` (where `W = (cos(theta_w),
     sin(theta_w))` is the unit wind direction and `lambda_asym` is the
     asymmetry slider, neutral `0.0`), with exact spatial derivatives
     `d(Dx_skew)/dx = dDx/dx + lambda_asym * (dDz/dx) * W_x`,
     `d(Dy_skew)/dy = dDy/dy + lambda_asym * (dDz/dy) * W_y`,
     `d(Dx_skew)/dy = dDx/dy + lambda_asym * (dDz/dy) * W_x`,
     `d(Dy_skew)/dx = dDy/dx + lambda_asym * (dDz/dx) * W_y`, and exact 2x2
     Jacobian determinant `(1 + dDx'/dx) * (1 + dDy'/dy) - (dDx'/dy) *
     (dDy'/dx)`. This stays strictly real, keeps derivatives exact and
     consistent, is neutral at `0.0`, and steepens the leeward front while
     flattening the windward back (with Creamer-style second-order canonical
     correction as a speculative follow-up alternative). Validate visual
     quality and stability (no self-intersection folds) interactively with the
     user.
4. **High-Performance GPU Stockham Compute FFT & Packing:**
   - Implement a shared-memory **Stockham radix-2 compute IFFT** (no
     bit-reversal step, coalesced memory access) in Vulkan GLSL compute.
   - **Complex-channel packing:** Pack two real-valued spectral fields into
     one complex FFT (`h_a(k) + i * h_b(k)`) so height `Dz`, horizontal chop
     `Dx, Dy`, vertical slopes `dDz/dx, dDz/dy`, and horizontal Jacobian
     derivatives `dDx/dx, dDy/dy, dDx/dy` transform in half the IFFT dispatches
     (unmeasured estimate: ~0.2-0.4 ms total for 2x `256x256` cascades; profile
     on the dev GPU during execution).
5. **Zoomed-In Per-Pixel Detail & Zoom-Out Filtering — Jonathan Dupuy & Eric
   Bruneton (2012), *"Real-time Animation and Rendering of Ocean Whitecaps"*
   (SIGGRAPH Asia Technical Briefs, Section 3) & Slope-Variance Filtering
   (speculative):**
   - Primary source DOI (Dupuy & Bruneton 2012):
     https://doi.org/10.1145/2407746.2407761
   - Sample FFT **displacement and full derivatives** in
     `WaterDisplacement.comp` for vertex geometry and the combined Jacobian
     `J_total` (`normalImage.w`), while writing the unblended low-wave-only
     normal to `normalImage.xyz` and sampling mipmapped FFT **per-pixel
     derivatives** (generated via recorded `vkCmdBlitImage` after
     `WaterFftMerge.comp`) in `Water.frag` so `framing = 100` renders full
     sub-vertex detail without double-counting the FFT slope and without legacy
     normal-map textures.
   - **(Speculative)** Fade both displacement amplitude and per-pixel FFT
     normal weight smoothly over configurable camera-framing fade start/end
     sliders, and optionally feed sub-pixel slope variance (Dupuy & Bruneton
     2012, Section 3) into specular roughness/softening to prevent distance
     shimmer as the camera zooms out.

## Design

1. **Remove legacy medium Gerstner band cleanly:**
   - Remove the legacy medium Gerstner wrappers (`gWaterMediumCount`,
     `gWaterMediumDirectionDegrees`, `gWaterMediumSpreadDegrees`,
     `gWaterMediumWavelengthShortest`, `gWaterMediumWavelengthLongest`,
     `gWaterMediumSlope`, `gWaterMediumChoppiness`, etc.), `pf4MediumWavesOne`,
     `pf4MediumWavesTwo`, `iWaterMediumCount`, and the medium loop in
     `WaterDisplacement.comp` and `WaterUniforms.cpp` (no backward
     compatibility).
   - Update `Water.vert:58` (`if (mainLayout.iWaterLowCount > 0 ||
     mainLayout.iWaterMediumCount > 0)`) to gate displacement sampling on
     `mainLayout.iWaterLowCount > 0 || mainLayout.iWaterFftActive != 0` so
     FFT-only displacement stays active when low waves are disabled.
2. **GPU FFT Compute Pipeline (`Engine/Data/Shaders/Water/WaterFft*.comp`,
   `RenderTargetTextures`, `WorldLightingShadowPipelines`,
   `CommandBufferRecordMain`, `Graphics`):**
   - Pin the runnable first-iteration configuration to **2 `256x256` cascades**
     with seamless half-open `[kMin_c, kMax_c)` wavenumber bands meeting at
     `k_split = 6.0 rad/m` (Cascade 1 `L1 = 67.0 m`, `k in [2 * pi / L1 ≈
     0.094, 6.0) rad/m`; Cascade 0 `L0 = 17.0 m`, `k in [6.0, pi * N / L0 ≈
     47.31) rad/m`), DC (`k = 0`) and self-conjugate Nyquist indices
     (`m == -N/2` or `n == -N/2`) explicitly zeroed by index check regardless
     of live `L0`/`L1` values, **finite-depth dispersion** `omega(k) =
     sqrt(g * k * tanh(k * d))`, `RGBA32F` complex IFFT scratch, and `RGBA16F`
     cascade outputs.
   - Allocate per-cascade `256x256` compute storage/sampled textures in
     `RenderTargetTextures` (`VK_IMAGE_USAGE_SAMPLED_BIT |
     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
     VK_IMAGE_USAGE_TRANSFER_DST_BIT`):
     - `H0` initial spectrum texture (`RGBA32F`: `h0(k)` + conjugate
       `h0*(-k)`), initialized on first active use via a CPU dirty flag that
       starts `true` and re-dirtied whenever spectrum sliders change.
     - Ping-pong spectral/IFFT scratch textures (`RGBA32F`).
     - Per-cascade mipmapped `Displacement` (`RGBA16F`: `Dx_skew, Dy_skew, Dz,
       d(Dx_skew)/dy`) and `Derivatives` (`RGBA16F`: `dDz/dx, dDz/dy,
       d(Dx_skew)/dx, d(Dy_skew)/dy`) textures carrying the skewed horizontal
       displacement, height, both vertical slopes, and horizontal derivatives
       (with `d(Dy_skew)/dx = d(Dx_skew)/dy - lambda_asym * (dDz/dy) * W_x +
       lambda_asym * (dDz/dx) * W_y` reconstructed from the stored channels
       because pre-skew irrotational chop satisfies `dDy/dx == dDx/dy`).
   - Reconcile zero-work with record-once command buffers
     (`Engine/Source/Graphics/Managers/AGENTS.md:134`):
     - Use conditional recording via `PollSetting(...,
       DestroyType::kCommandBuffers)` (matching `WaterFoamActive()` in
       `CommandBufferRecordMain.cpp:320` and `Graphics.cpp:604-606`) gated on
       `WaterFftActive()` (`gWaterLevel == kHigh && !gWaterDisableMediumWaves`,
       adding `PollSetting<bool>(gWaterDisableMediumWaves, ...,
       DestroyType::kCommandBuffers)` in `Graphics.cpp` alongside
       `gWaterLevel`). When `WaterFftActive()` is false,
       `CommandBufferRecordMain.cpp` records zero FFT compute dispatches, zero
       layout transitions, and zero `vkCmdBlitImage` mip blits.
     - When `WaterFftActive()` is true, record the four compute passes and
       mipmap `vkCmdBlitImage` chains once in `CommandBufferRecordMain.cpp`
       before `RecordWaterDisplacement`, driving dynamic zoom-out and `H0`
       dirty gating via `kIndirectHostVisible` indirect compute dispatch
       buffers (writing `(0, 0, 0)` when `fMediumAmplitudeScale <= 0`, and
       writing `(0, 0, 0)` to the `H0` spectrum indirect buffer whenever the
       dirty flag is false; note that recorded mip blits are omitted when FFT
       is disabled via the command-buffer recording gate, not on dynamic
       zoom-out):
     1. `WaterFftSpectrum.comp` — evaluates JONSWAP/TMA + Hasselmann/Swell
        directional spectrum into `H0` on first active use and whenever
        spectrum parameters are dirty, zeroing DC (`k = 0`) before evaluating
        `1 / k` and strictly excluding self-conjugate Nyquist indices
        (`m == -N/2` or `n == -N/2`, remaining valid dynamically whenever live
        patch-size sliders `L0`/`L1` change), using half-open `[kMin_c, kMax_c)`
        cutoffs meeting at `k_split = 6.0` and `H0(k) = (1 / sqrt(2)) *
        (xi_r + i * xi_i) * sqrt(0.5 * E(k))` (i.e., `0.5 * (xi_r + i * xi_i) *
        sqrt(E(k))` for `E(k) = S(omega) * D(theta) * (dOmega/dk) * (1 / k) *
        dk_x * dk_y`, giving `E[|H0(k)|^2] = 0.5 * E(k)`).
     2. `WaterFftTimeStep.comp` — pure linear Hermitian time evolution
        `H(k, t) = H0(k) * exp(i * omega * t) + conj(H0(-k)) *
        exp(-i * omega * t)` with finite-depth dispersion
        `omega(k) = sqrt(g * k * tanh(k * d))`, packing complex fields for
        IFFT.
     3. `WaterFftInverse.comp` — unnormalized shared-memory horizontal +
        vertical Stockham radix-2 synthesis IFFT (`sum_k H(k, t) *
        exp(i * k · x)`, no `1 / N^2` divisor, using push constants for
        pass/cascade index) so spatial output is strictly real with **expected**
        height variance `sum_k E(k)` over independent draws for the retained
        bins (a single fixed seed realizes spatial variance
        `sum_k |H(k,t)|^2`).
     4. `WaterFftMerge.comp` — applies `(-1)^(x+y)` sign flip, unpacks real
        `Dx, Dy, Dz`, vertical slopes `(dDz/dx, dDz/dy)`, and pre-skew
        horizontal derivatives `(dDx/dx, dDy/dy, dDx/dy)`, applies the
        spatial-domain wind-aligned asymmetry lean (`Dx_skew = Dx + lambda_asym
        * Dz * W_x`, `Dy_skew = Dy + lambda_asym * Dz * W_y`, neutral at
        `lambda_asym = 0.0`) with exact derivatives `d(Dx_skew)/dx = dDx/dx +
        lambda_asym * (dDz/dx) * W_x`, `d(Dy_skew)/dy = dDy/dy + lambda_asym *
        (dDz/dy) * W_y`, `d(Dx_skew)/dy = dDx/dy + lambda_asym * (dDz/dy) *
        W_x`, `d(Dy_skew)/dx = dDy/dx + lambda_asym * (dDz/dx) * W_y`, and
        writes mip 0 of each cascade's `Displacement` and `Derivatives`
        textures.
     5. Recorded `vkCmdBlitImage` mipmap chains in
        `CommandBufferRecordMain.cpp` (outside the shader, recorded only when
        `WaterFftActive()` is true) with explicit layout/memory barriers before
        `WaterDisplacement.comp` and `Water.frag`.
   - Publish `int iWaterFftActive` (`1` only when `WaterFftActive()` is true
     and `fMediumAmplitudeScale > 0`, `0` otherwise) and
     `float fWaterMediumAmplitudeScale` (the CPU-computed camera-framing zoom
     fade factor) in `MainLayout` so `WaterDisplacement.comp`, `Water.vert`,
     and `Water.frag` never read stale FFT textures when FFT is disabled or
     zoomed out and share the exact same zoom fade factor.
3. **Integrate into `WaterDisplacement.comp`, `Water.vert`, `Water.frag`, and
   `WaterReflectionProjection.h` (zero double-counting, zero extra render
   targets):**
   - In `WaterDisplacement.comp`, compute the low-wave displacement and
     low-wave-only unit normal `n_low = (nx_low, ny_low, nz_low)` from low-wave
     derivatives first. When `mainLayout.iWaterFftActive != 0`, sample each
     active FFT cascade using double-precision cell-reduced UV offsets
     (`f2CascadeOriginUv[c]`), compute the uniform medium scale
     `fMediumScale = fMediumWeight * mainLayout.fWaterMediumAmplitudeScale`
     (where `fMediumWeight` is computed from undisplaced terrain elevation and
     honors `gWaterIgnoreCoastMask` from the isolation Plan), add
     `fMediumScale`-weighted FFT displacement to `f3Displacement`, and
     accumulate the combined derivatives with the exact signs required by
     `WaterDisplacement.comp:138-145` (where tangent is `(1 - fA, -fB_yx, fG)`
     and bitangent is `(-fB_xy, 1 - fD, fE)`, reducing to `fB_xy == fB_yx == fB`
     when `lambda_asym == 0`):
     `fA -= fMediumScale * d(Dx_skew)/dx`,
     `fD -= fMediumScale * d(Dy_skew)/dy`,
     `fB_xy -= fMediumScale * d(Dx_skew)/dy`,
     `fB_yx -= fMediumScale * d(Dy_skew)/dx`, while vertical slopes add
     directly with positive sign: `fG += fMediumScale * dDz/dx` and
     `fE += fMediumScale * dDz/dy`. Scaling partials by `s * dD` (omitting the
     envelope-gradient term `D * ds` across the smooth coast transition, with
     `s = fMediumScale`) is a deliberate spatially-varying envelope
     approximation that is exact wherever `fMediumWeight` is constant (open
     ocean and isolation mode). Compute the combined 2x2 Jacobian determinant
     `J_total = (1 - fA) * (1 - fD) - fB_xy * fB_yx` (`= (1 + dDx'/dx) *
     (1 + dDy'/dy) - (dDx'/dy) * (dDy'/dx)` under that envelope approximation),
     so foam compression fades in lockstep with geometry and normals as the
     camera zooms out.
   - Write `displacementImage = vec4(Dx_total, Dy_total, Dz_total,
     fMediumWeight)` (packing `fMediumWeight` into the currently unused `w`
     channel, `0.0` over land) and `normalImage = vec4(n_low, J_total)` (where
     `xyz` always stores the **unblended** unit low-wave-only normal `n_low` —
     removing `fWaterWaveNormalBlend` from `WaterDisplacement.comp` — and `w`
     is the combined low + FFT Jacobian determinant for `WaterFoam.comp`).
   - In `Water.vert`, read `displacementImage` for vertex displacement (gated
     on `mainLayout.iWaterLowCount > 0 || mainLayout.iWaterFftActive != 0`),
     forward `displacementImage.w` to `Water.frag` as `fInMediumWeight` (`0.0`
     over land or when inactive), and pass `normalImage.xyz` (`f3InNormal`, the
     unblended unit low-wave-only normal `n_low`) to `Water.frag`.
   - In `Water.frag`, before terrain `discard` so 2x2 quad derivatives stay
     valid:
     - When `mainLayout.iWaterFftActive != 0`, compute
       `fMediumScale = fInMediumWeight * mainLayout.fWaterMediumAmplitudeScale`,
       sample the mipmapped per-pixel FFT cascade partials weighted by
       `fMediumScale` (using the same `s * dD` envelope approximation, exact
       wherever `fMediumWeight` is constant), evaluate the per-pixel FFT wave
       normal `n_fft` from the full 2x2 tangent/bitangent cross product, and
       combine `n_low = normalize(f3InNormal)` and `n_fft` via
       partial-derivative blending (`n_combined = normalize(vec3(n_low.xy *
       n_fft.z + n_fft.xy * n_low.z, n_low.z * n_fft.z))`, which has no
       division clamp and reduces identically to `normalize(f3InNormal)` as
       `n_fft -> (0, 0, 1)` for every allowed `n_low`). When
       `mainLayout.iWaterFftActive == 0`, set `n_combined =
       normalize(f3InNormal)`.
     - Apply `fWaterWaveNormalBlend` **unconditionally** once in `Water.frag`
       (`f3CombinedWaveNormal = normalize(mix(vec3(0.0, 0.0, 1.0), n_combined,
       globalLayout.fWaterWaveNormalBlend))`), ensuring exact continuity across
       the `fMediumAmplitudeScale -> 0` / `iWaterFftActive` cutoff.
     - Compute `f3NormalDx = dFdx(f3CombinedWaveNormal)` and
       `f3NormalDy = dFdy(f3CombinedWaveNormal)` (replacing `Water.frag:48-49`'s
       vertex-input derivatives) for multi-tap Skybox One specular sampling,
       and route `f3CombinedWaveNormal` to primary specular
       (`ComposeWaterSpecular` / `WaterSpecular.h`), directional lighting
       (`Water.frag:136`), water EWNS lighting (`Water.frag:232`), and
       reflection projection (`ProjectWaterReflection` in
       `WaterReflectionProjection.h`, taking `f3CombinedWaveNormal` as a
       parameter instead of reading `f3InNormal` directly). This counts the FFT
       slope exactly once with zero extra render targets and gives full
       pixel-sharp detail at `framing = 100`.
4. **Brand-new Medium FFT sliders (`WaterWrappersBase.h`,
   `TweaksScreenWater.cpp`):**
   - Replace the `"Medium"` subtab from scratch with physically meaningful
     controls:
     - Wind Speed, Wind Direction, Fetch, Water Depth (TMA), Swell (Horvath),
       Directional Spread, Short-Wave Damping.
     - Choppiness (horizontal displacement scale), Front/Back Asymmetry Skew,
       Vertical Amplitude Scale, Wave Time Scale.
     - Cascade Patch Sizes (`L0`, `L1`) and Cascade Weights (3rd cascade `L2`
       remains a speculative follow-up).
     - Zoom Fade Start / Fade End (matching camera framing `100..600`).
     - Per-pixel Normal Blend / Slope-Variance Roughness (speculative).
5. **Interactive user-feedback loop (required during execution):**
   - Launch with isolation active (medium waves everywhere, low waves off,
     normal-map textures off).
   - Present the build and sliders to the user, collect visual feedback across
     zoom levels (`framing = 100` through `600`), iterate on spectrum,
     asymmetry math, cascade sizes, and defaults until the user approves, then
     verify full combined mode (isolation off, foam working, zoom fade-out
     smooth) before landing onto `water-waves` (leaving `main`'s code
     untouched).

## Critical files

- `Engine/Data/Shaders/Water/WaterFftSpectrum.comp` (new)
- `Engine/Data/Shaders/Water/WaterFftTimeStep.comp` (new)
- `Engine/Data/Shaders/Water/WaterFftInverse.comp` (new)
- `Engine/Data/Shaders/Water/WaterFftMerge.comp` (new)
- `Engine/Data/Shaders/Water/WaterDisplacement.comp`
- `Engine/Data/Shaders/Water/Water.vert`
- `Engine/Data/Shaders/Water/Water.frag`
- `Engine/Data/Shaders/Water/WaterReflectionProjection.h`
- `Engine/Data/Shaders/ShaderLayoutsBase.h`
- `Engine/Data/Shaders/ShaderGlobalLayout.h`
- `Engine/Data/Shaders/ShaderMainLayout.h`
- `Engine/Source/Graphics/Graphics.cpp`
- `Engine/Source/Graphics/Managers/RenderTargetTextures.h`
- `Engine/Source/Graphics/Managers/RenderTargetTextures.cpp`
- `Engine/Source/Graphics/Managers/PipelineManager.h`
- `Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.cpp`
- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp`
- `Engine/Source/Graphics/Render/WaterUniforms.cpp`
- `Engine/Source/Ui/WaterWrappersBase.h`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenWater.cpp`

## In scope

- Adding the 4 FFT compute shaders and registering their pipelines, cascade
  textures, `kIndirectHostVisible` dispatch buffers, barriers, conditional
  `WaterFftActive()` command-buffer recording (`Graphics.cpp` `PollSetting` on
  `gWaterDisableMediumWaves` + `gWaterLevel`), and recorded `vkCmdBlitImage`
  mip chains in `RenderTargetTextures`, `PipelineManager`,
  `WorldLightingShadowPipelines`, `CommandBufferRecordMain`, and `Graphics`.
- Removing the legacy medium Gerstner band from `ShaderMainLayout.h`,
  `WaterUniforms.cpp`, `WaterDisplacement.comp`, `WaterWrappersBase.h`, and
  `TweaksScreenWater.cpp`.
- Updating `WaterDisplacement.comp` to read
  `mainLayout.fWaterMediumAmplitudeScale`, apply `fMediumScale = fMediumWeight
  * mainLayout.fWaterMediumAmplitudeScale` uniformly across FFT displacement
  and compute Jacobian partials (subtracting horizontal partials into
  `fA, fB_xy, fB_yx, fD`, adding vertical slopes to `fG, fE`, using the
  `s * dD` spatially-varying envelope approximation that omits `D * ds` across
  the smooth coast transition and is exact wherever `fMediumWeight` is
  constant), pack `fMediumWeight` (from undisplaced terrain elevation, honoring
  `gWaterIgnoreCoastMask`) into `displacementImage.w` (`vec4(Dx_total,
  Dy_total, Dz_total, fMediumWeight)`), and write `normalImage = vec4(n_low,
  J_total)` with `n_low` always unblended (removing `fWaterWaveNormalBlend`
  from `WaterDisplacement.comp`); updating `Water.vert` to gate displacement
  sampling on `mainLayout.iWaterLowCount > 0 || mainLayout.iWaterFftActive !=
  0`, forward `displacementImage.w` as `fInMediumWeight`, and pass
  `normalImage.xyz` (`f3InNormal`, the unblended unit low-wave-only normal
  `n_low`) to `Water.frag`; and updating `Water.frag` and
  `WaterReflectionProjection.h` to compute the per-pixel FFT normal `n_fft`
  from mipmapped FFT partials weighted by `fMediumScale = fInMediumWeight *
  mainLayout.fWaterMediumAmplitudeScale`, combine `n_low` and `n_fft` via
  slope-addition when `iWaterFftActive != 0` (or `normalize(f3InNormal)` when
  inactive), apply `fWaterWaveNormalBlend` unconditionally once into
  `f3CombinedWaveNormal` before terrain `discard`, compute `dFdx`/`dFdy` of
  `f3CombinedWaveNormal` for multi-tap Skybox One specular, and route
  `f3CombinedWaveNormal` to primary specular, directional lighting, water EWNS
  lighting, and reflection projection, with double-precision cell-reduced UVs
  and uniform camera-framing zoom fade-out.
- Interactive slider refinement rounds with the user in the testing worktree.

## Out of scope

- Rewriting the low-directional Gerstner wave band (handled in its own
  downstream Plan).
- Adding or changing skybox assets.
- Changing water simulation determinism/CRC (water visuals are client-only
  `BT_CLIENT` and excluded from PostRender CRC).
- Changing `Water.vert` mesh resolution or LOD table (only the `:58`
  displacement-active gate, forwarding `displacementImage.w` as
  `fInMediumWeight`, and passing `normalImage.xyz` as unblended low-wave-only
  `f3InNormal` change).
- Landing or merging changes onto `main` (leave `main` untouched until explicit
  user confirmation after the full four-plan water-wave series completes).
- Unit tests.

## Risk triggers and invariants

Tier 2: scoped client water rendering subsystem (`BT_CLIENT` only; no
PostRender determinism/CRC, network wire, save/replay, or threading-model
changes). Note: if execution adds new `.comp` files or touches shared
`ShaderMainLayout.h` consumed by server builds, verify both client and server
builds compile cleanly.

- **Unbounded cell precision:** Cascade UV origin offsets must be reduced in
  `double` on the CPU (`CellCenterOffsetDouble` modulo `L_c`) so waves stay
  smooth at arbitrarily large cell coordinates.
- **Zero-work disabled mode, dynamic zoom-out, and no stale reads:** When
  `gWaterLevel < kHigh` or `gWaterDisableMediumWaves` is true, conditional
  command-buffer recording (`PollSetting(..., DestroyType::kCommandBuffers)`)
  omits all FFT dispatches, layout transitions, and `vkCmdBlitImage` mip blits.
  When recorded active and zoomed out (`fMediumAmplitudeScale <= 0`), indirect
  FFT dispatch buffers write `(0, 0, 0)` and `mainLayout.iWaterFftActive` is
  `0` so `WaterDisplacement.comp`, `Water.vert`, and `Water.frag` never read
  stale FFT textures (recorded mip blits remain in the active command buffer on
  dynamic zoom-out).
- **Derivative signs, envelope approximation, uniform zoom fade, foam contract,
  and single-counted normals:** `displacementImage.w` packs `fMediumWeight`
  (from undisplaced terrain elevation, honoring `gWaterIgnoreCoastMask`) and
  `Water.vert` forwards it as `fInMediumWeight`, so both compute and fragment
  apply `fMediumScale = fMediumWeight * mainLayout.fWaterMediumAmplitudeScale`
  uniformly across FFT displacement, compute Jacobian partials (horizontal
  partials subtracted into `fA, fB_xy, fB_yx, fD`; vertical slopes added to
  `fG, fE`), and fragment per-pixel FFT partials. Scaling partials by `s * dD`
  (omitting the coast-transition envelope-gradient term `D * ds`) is a
  deliberate spatially-varying envelope approximation that is exact wherever
  `fMediumWeight` is constant (open ocean and isolation mode), producing the
  combined 2x2 Jacobian determinant `J_total` in `normalImage.w` (`~1.0` flat
  water, `< 0.7` folding crests) for `WaterFoam.comp` that fades in lockstep
  with geometry and normals. `normalImage.xyz` always carries the unblended
  unit low-wave-only normal `n_low`, and `Water.frag` combines `n_low` with
  `n_fft` via slope-addition when active (or `normalize(f3InNormal)` when
  inactive) and applies `fWaterWaveNormalBlend` unconditionally once into
  `f3CombinedWaveNormal` (routed to specular, multi-tap `dFdx`/`dFdy`
  derivatives, directional lighting, EWNS lighting, and reflection projection)
  for exact continuity across the `fMediumAmplitudeScale -> 0` /
  `iWaterFftActive` cutoff.
- **No visible tiling or band overlap:** Seamless half-open `[kMin_c, kMax_c)`
  wavenumber bands meeting at `k_split = 6.0 rad/m` (Cascade 1 `[2*pi/L1,
  6.0)`, Cascade 0 `[6.0, pi*N/L0)`, with DC `k = 0` and self-conjugate
  Nyquist indices `m == -N/2` or `n == -N/2` dynamically zeroed across live
  `L0`/`L1` changes) and coprime cascade sizes (`17 m`, `67 m`, combined repeat
  period `1,139 m`) must show no obvious grid repetition from `framing = 100`
  to `600`.
- **Vulkan barriers:** Explicit layout/memory barriers between Spectrum,
  TimeStep, horizontal/vertical IFFT, Merge, recorded `vkCmdBlitImage` mip
  blits, and `WaterDisplacement.comp` / `Water.frag` with zero validation
  errors.

## Acceptance criteria

1. `/compile` builds BrokenEngineSandbox client (and server if shared layout
   headers changed) cleanly with zero Vulkan validation errors.
2. In isolated mode (medium waves everywhere, low waves and normal-map
   textures off, foam disabled via the existing control), FFT-only displacement
   renders through `Water.vert` and looks natural and detailed at closest zoom
   (`framing = 100`) with non-double-counted per-pixel slope detail, visible
   size distribution, and tunable front/back asymmetry.
3. No obvious periodic tiling is visible across `framing = 100..600`, and
   waves fade out smoothly as the camera zooms out to `Amplitude Fade End`.
4. All new Medium FFT sliders respond live (including first-use and live `H0`
   spectrum regeneration).
5. GPU frame time for the FFT passes is profiled on the dev GPU at the pinned
   2x `256x256` configuration (unmeasured estimate: target `< 0.5 ms`; verify
   real-time budget on hardware).
6. User explicitly signs off on visual quality after interactive slider
   testing, and combined mode (isolation toggles off, foam active) renders
   cleanly.
7. The session branch stays at `main` through `/next-plan` step 6's claim
   refresh (inspecting `water-waves` via Git during step-4/5 preparation and
   review), then moves onto `water-waves` for implementation, builds, and
   verification; at `/finalize-changes`, the implementation and Plan completion
   land onto `water-waves` under the global landing lock — either by switching
   primary to `water-waves` for preparation and landing together (then
   switching primary back to `main`), or via a user-authorized locked update of
   `refs/heads/water-waves` while primary stays on `main` — then the Plan claim
   is released and only the Plan deletion and child `dependsOn` edge removals
   sync onto `main` (with no water-wave code on `main`).
