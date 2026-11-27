<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-25T20:59:38.853Z","dependsOn":["Documents/Plans/Engine/AllocateOnlyActiveLightingSpreadPasses.md","Documents/Plans/Engine/FftMediumWaves.md","Documents/Plans/Engine/MergeRenderTargetTexturesLighting.md","Documents/Plans/Engine/ModernGerstnerLowDirectionalWaves.md","Documents/Plans/Engine/PrepareLightingSpreadTaps.md","Documents/Plans/Engine/RemoveCompassHaloFromLighting.md","Documents/Plans/Engine/SingleLayerSmoke.md"]} -->
# Add Shore-Directed Beach Waves, Swash Foam, and Wet Sand

## Context

After the FFT medium-wave and low-directional Gerstner rewrites, open-ocean
medium waves run on multi-cascade GPU FFT and low-directional swells run on a
spectral asymmetric Gerstner solver. Those low-directional swells travel in a
global world direction, so around an island they run parallel to or away from
leeward and side coasts instead of refracting inward from open water toward the
beach. Currently, the water pipeline simply fades low and medium waves out near
shore (`WaterDisplacement.comp`), leaving a flat depth-alpha edge with no
shore-directed breaking waves, no swash cycle, and no beach foam (Jacobian
whitecap foam in `WaterFoam.comp` also drops to zero where waves are damped).

From Broken Engine's top-down RTS camera (`FOV = 15 deg`, `Tilt = 20 deg`,
gameplay `framing = 100..600`, eye distance `~168..1000 m`), first-person
curling wave tubes or custom per-island shoreline ribbon meshes barely read and
add geometry seams and overdraw. What reads unmistakably from altitude is a
**distance-field-driven coastal wave and swash layer** displaced directly on the
existing ocean grid (`895 x 511` LOD 0 quads, `0.105..0.713 m` per quad across
`framing = 100..600`), paired with per-pixel breaking foam, rhythmic swash,
retained surf-zone foam trails, and wet-sand darkening/gloss on the beach.

This is the final Plan in the four-plan water-wave rewrite series.
`WaterWaveIsolationControls` and earlier series commits live on branch
`water-waves`, while the primary checkout stays on `main` so other agents are
not disrupted. Because `/next-plan` claims this Plan from `main`, the session
worktree starts at `main`. Keep the session branch at `main` through
`/next-plan` step 6's claim refresh — inspecting `water-waves` code via Git
during step-4/5 preparation and review so `Invoke-NextPlanClaim.ps1` does not
block with `claim.session-diverged` — and move the session worktree onto
`water-waves` only **after** step 6's claim refresh completes — merging `main`
into `water-waves` first (without rewriting `water-waves` history) if `main`
holds completed external prerequisites or overlapping updates — then implement,
build, and verify on `water-waves`, leaving `main`'s code untouched. At landing
(`/finalize-changes`), land the implementation and Plan completion onto
`water-waves` under the global landing lock — either by switching primary to
`water-waves` for preparation and landing together (then switching primary back
to `main`), or via a user-authorized locked update of `refs/heads/water-waves`
while primary stays on `main` — then release the Plan claim and sync only the
Plan deletion and any child `dependsOn` edge removals onto `main` (with no
water-wave code on `main`). Merging `water-waves` into `main` remains a
separate post-series, user-confirmed integration step; if the user directs
removal of the temporary wave-isolation controls, perform that removal and its
build/visual verification on `water-waves` before requesting confirmation to
land onto `main`.

### Engine Scale and Integration Constraints

- `DataPacker/Source/ExportJobs/Island/BeachPass.cpp` already computes a 2D
  Euclidean distance transform (`DistancesToLand`) to the `0.1 m` land
  threshold during island export and discards it; the shore SDF needs a
  dedicated signed 2D EDT to the `elevation >= 0.0f` zero-contour. Meanwhile,
  `mTerrainElevationTexture` bottoms out flat at `common::kfSeaBottomMeters`
  (`-10.0 m`) close to shore, so its gradient vanishes before the surf zone
  begins.
- `mTerrainElevationTexture` (`RenderTargetTextures.cpp:383-400`,
  `PipelineManager.cpp:170-187`) composites active islands using
  `VK_BLEND_OP_MAX` (`kMax`) over a clear value of `mfSeaFloorElevation`
  (`-10.0f`) and is already bound at `elevationTextureSampler` in
  `Terrain.frag`, `WaterDisplacement.comp`, `WaterFoam.comp`, and `Water.frag`.
- Introducing `kVkFormatTerrainElevationSdf = VK_FORMAT_R16G16_SFLOAT` for
  per-island elevation+SDF textures and `mTerrainElevationTexture` (while
  keeping the shadow elevation target at `kVkFormatElevation =
  VK_FORMAT_R16_SFLOAT`) stores `R = elevation` and `G = -shoreSdf`
  (`shoreSdf > 0` in water, `0` at the `elevation == 0` shoreline, `< 0` inland
  on the beach, cleared to `-kfShoreSdfMaxMeters`). The existing
  `VK_BLEND_OP_MAX` pass then composites **both** highest elevation and signed
  minimum shore distance (`max(-s_A, -s_B) == -min(s_A, s_B)` on the post-taper
  stored distances, selecting the nearest shore offshore where `s > 0` and the
  deeper interior where `s < 0`) in one pass with zero extra render targets,
  draw calls, or descriptor bindings.
- `WaterDisplacement.comp` must preserve the shared render-target and
  slope-addition contract from the FFT medium-wave and low-directional Gerstner
  rewrites: `displacementImage = vec4(Dx_total, Dy_total, Dz_total,
  fMediumWeight)` and `normalImage = vec4(n_low_and_shore, J_total)`, with
  `Water.frag` combining `normalize(f3InNormal)` with `n_fft` and applying
  `fWaterWaveNormalBlend` once.
- CPU terrain height queries in `Engine/Source/Frame/IslandTerrain.cpp` must
  remain bit-identical for simulation determinism and collision CRC.

### Academic Research and Industry Best Practices

1. **Boundary-Aware Procedural Waves — Stefan Jeschke, Christian Hafner,
   Nuttapong Chentanez, Miles Macklin, Matthias Müller-Fischer, & Chris Wojtan
   (2020), *"Making Procedural Water Waves Boundary-aware"* (Computer Graphics
   Forum 39(8), Sections 3-4):**
   - Primary source DOI: https://doi.org/10.1111/cgf.14100
   - Fits ellipsoidal **wave cages** to coastal terrain to transition smoothly
     from unbounded open-ocean procedural waves to shoreline-conforming waves
     that respect solid boundaries.
2. **Shallow-Water Refraction and Shoaling — Stefan Jeschke & Chris Wojtan
   (2017), *"Water Wave Packets"* (ACM Transactions on Graphics / SIGGRAPH
   2017, Vol. 36, No. 4, Article 103) & Manuel N. Gamito & F. Kenton Musgrave
   (2002), *"An Accurate Model of Wave Refraction over Shallow Water"*
   (Computers & Graphics):**
   - Primary source DOIs: https://doi.org/10.1145/3072959.3073678 (Jeschke &
     Wojtan 2017) and https://doi.org/10.1016/S0097-8493(01)00181-9 (Gamito &
     Musgrave 2002).
   - As water shallows toward a coast, wave speed drops (`c ~ sqrt(g * d)`),
     bending wave propagation direction (rays) nearly perpendicular to the
     shore contour (`D_shore = normalize(grad(-shoreSdf))`) so wave crests
     arrive parallel to the shoreline, while compressing wavelength (shoaling)
     before breaking.
3. **AAA Coastal Foam, Swash, and Wet Sand — Nicolas Longchamps (GDC 2017,
   *"Shore to Horizon"*, *Hitman*) and top-down RTS coastal pipelines:**
   - Longchamps (GDC 2017) uses a dedicated beach mesh with matched water and
     shore-floor topology and UVs for beach-level and high-vantage views.
     Broken Engine instead uses unified ocean-grid displacement plus SDF-driven
     shaders rather than per-island authored shore meshes — avoiding per-island
     shoreline mesh seams and authoring overhead while applying world-noise
     phase breakup (preventing uniform concentric contour rings), multi-stage
     crest + swash + decaying surf-zone foam, and terrain wet-sand darkening
     with sun/moon specular sheen in the swash band.

## Design

1. **Pre-baked Island Shore Signed Distance Field (`DataPacker`,
   `Common/DataFile.h`, `IslandTerrain`, `RenderTargetTextures`,
   `WorldLightingShadowPipelines`):**
   - Define `common::kfShoreSdfMaxMeters = 60.0f` (offshore clamp) and
     `common::kfShoreSdfMinInlandMeters = -12.0f` (inland clamp for swash and
     wet sand) in `Common/DataFile.h`. Always bump **both**
     `DataHeader::kiVersion` in `Common/DataFile.h` (for runtime stale-pack
     rejection even when `IslandHeader` layout is unchanged) and
     `ExportIsland::kiVersion` (for exporter cache invalidation); no backward
     compatibility.
   - In `DataPacker/Source/ExportJobs/Island/BeachPass.cpp` and
     `ExportIsland.cpp`, after `ApplyBeachPass` and `kfIslandScale` scaling of
     the elevation grid, add a dedicated signed 2D EDT
     (`ComputeSignedShoreDistance`) to the `elevation >= 0.0f` zero-contour
     (distinct from `DistancesToLand`'s `0.1 m` land threshold), in scaled
     world meters (`> 0` offshore in water, `0` at shoreline, `< 0` inland on
     land), clamped to `[kfShoreSdfMinInlandMeters, kfShoreSdfMaxMeters]`, and
     negated (`-shoreSdf`) for `VK_BLEND_OP_MAX` compositing.
   - Do not modify `ProcessBakedRegion.cpp` crop bounds (leaving
     `BakeRoute.cpp::kiSplitVersion`, box-filter origin, and island world
     footprint untouched so CPU heights at every world coordinate stay
     bit-identical). Compute `ComputeSignedShoreDistance` on the existing
     cropped elevation grid in `ExportIsland.cpp` and taper `-shoreSdf` toward
     `-kfShoreSdfMaxMeters` over a narrow outer border margin (e.g., `2 m`)
     inside the fixed island crop edge strictly for **offshore** texels
     (`shoreSdf > 0`), leaving inland texels (`shoreSdf <= 0`) untapered. This
     prevents offshore step jumps to the `-kfShoreSdfMaxMeters` clear value
     (gracefully attenuating beach waves across the border margin when a fixed
     crop edge falls closer than `kfShoreSdfMaxMeters` to shore) while
     preserving individual inland texels; in `Terrain.frag`, inland swash and
     wet sand are additionally guarded with `fTerrainElevation > 0.0f &&
     fShoreSdf <= 0.25f` to reject any positive-elevation sample whose linearly
     filtered SDF bled offshore from a `-kfShoreSdfMaxMeters` clear texel at a
     crop edge while keeping seamless waterline coverage at `fShoreSdf ~ 0`.
     Note that `max(-s_A, -s_B) == -min(s_A, s_B)` applies to the post-taper
     stored distances across both offshore (`s > 0`) and overlapping interiors
     (`s < 0`, selecting deeper interior).
   - Pin the decompressed island chunk layout to interleaved `RG16` half-floats
     (`R = elevation`, `G = -shoreSdf`) starting at byte 0 so
     `TextureUploadManager.cpp:426` uploads from byte 0 at 4 bytes/texel and
     avoids a CPU repacking copy (the upload still copies the interleaved
     payload bytes into the upload buffer). Update all three
     `IslandTerrain.cpp` consumers for the 4-byte (`2 * sizeof(uint16_t)`)
     texel stride: payload bounds check (`IslandTerrain.cpp:167`) and
     post-heightmap slicing (`IslandTerrain.cpp:155,188`), placement height
     sampling (`puiHeightmapHalf[2 * i]`), and navigation contour decode
     (`IslandTerrain.cpp:236`, `XMConvertHalfToFloatStream` input stride
     `2 * sizeof(uint16_t)`), preserving bit-identical CPU heights and nav
     contours.
   - Keep the shadow elevation target at `VK_FORMAT_R16_SFLOAT`
     (`shaders::kVkFormatElevation`) so shadow VRAM/bandwidth does not double.
     Introduce `shaders::kVkFormatTerrainElevationSdf = VK_FORMAT_R16G16_SFLOAT`
     in `ShaderLayoutsBase.h` for per-island elevation+SDF textures and
     `mTerrainElevationTexture`. Update placeholder creation in
     `TextureManager.cpp` (`{-10.0f, -kfShoreSdfMaxMeters}`), upload
     byte-stride in `TextureUploadManager.cpp` / `IslandTerrainResidency.cpp`,
     and `mTerrainElevationTexture` clear value in `RenderTargetTextures.cpp`
     to `{gpIslandTerrain->mfSeaFloorElevation, -common::kfShoreSdfMaxMeters,
     0.0f, 1.0f}`.
   - Split a dedicated `Engine/Data/Shaders/Terrain/TerrainShadowElevation.frag`
     (outputting `float fOutElevation` from `.x`) for the shadow pipeline
     (`WorldLightingShadowPipelines.cpp:120`), preserving the exact existing
     underwater elevation compression formula from `TerrainElevation.frag`
     (`pow(clamp(fRaw / -10.0, 0.0, 1.0), 1.0 / 0.8) * -10.0` via
     `globalLayout.fSeaFloorElevationInverse` and
     `globalLayout.fWaterUnderseaCompressionInverse` for `fRaw < 0`, unchanged
     for `fRaw >= 0`) on `.x` so shadow elevations stay bit-identical.
     `Terrain/TerrainElevation.frag` outputs `vec2 fOutElevation` (`x` =
     compressed elevation unchanged from today, `y` = raw `-shoreSdf` from
     `textureSampler[uiInTextureSlot].y`) for `mTerrainElevationTexture`,
     preserving `VK_BLEND_OP_MAX`.
2. **Shared Beach Math, Vertex Gate, & Shore-Normal Shoaling Waves
   (`WaterBeach.h`, `Water.vert`, `WaterDisplacement.comp`,
   `WaterUniforms.cpp`, `ShaderGlobalLayout.h`, `ShaderMainLayout.h`):**
   - Introduce a shared GLSL include `Engine/Data/Shaders/Water/WaterBeach.h`
     (included by `WaterDisplacement.comp`, `WaterFoam.comp`, `Water.frag`, and
     `Terrain.frag`) defining:
     - Three periodic reduced phases in `[0, 1)` cycle units —
       `globalLayout.fWaterBeachWavePhase`, `globalLayout.fWaterBeachSetPhase`,
       and `globalLayout.fWaterBeachSwashPhase` — integrated in persistent
       `double` accumulators on CPU in `WaterUniforms.cpp`
       (`fmod(acc + dDeltaTime * dRate, 1.0)`) and published to `GlobalLayout`
       for identical reads by `WaterDisplacement.comp`, `WaterFoam.comp`,
       `Water.frag`, and `Terrain.frag`. To avoid float timestamp quantization,
       thread the `double` simulation clock (`std::chrono::duration<double>`,
       computed directly via `common::NanosecondsToFloatSeconds<double>` in
       `GameBase::Render`, `Main.cpp`, and `ModelViewerApplication.cpp` around
       line 937 from ModelViewer's native nanosecond timer before any `float`
       conversion) through `Graphics::RenderGlobal` and `RenderFrameGlobal`
       into `PopulateWaterParameters` (`WaterUniforms.cpp`), and compute
       `dDeltaTime = dCurrentTime - dPreviousBeachTime` in `double` before any
       `float` conversion (initializing `dPreviousBeachTime = dCurrentTime` on
       the first frame or clock reset/rewind). Every consumer curve (wave
       crest, wave-set modulation, swash surge/drain, and wet-sand drying tail)
       is strictly `1.0`-periodic in its assigned phase so wraparound from
       `1.0` to `0.0` is seamless with zero long-session float drift. Spatial
       inputs use camera-cell-reduced world sampling coordinates (pinned to
       physical camera-cell XY `f3InPosition.xy` and its matching visible-area
       UV for swash in both `Water.frag` and `Terrain.frag`);
     - Purely procedural 2D smooth hash/value noise via `ShaderRandom.h` (zero
       new texture bindings, bit-identical phase breakup and wave-set envelope
       across all four shaders);
     - Bounded shoaling distance `s_eff = max(fShoreSdf, 0.0f) + fShoalEps`
       (`fShoalEps >= 0.5 m`) with `phi_base(s) = k_shore * (s + c_shoal *
       (sqrt(s + fShoalEps) - sqrt(fShoalEps)))` so `k_local = d(phi_base)/ds`
       stays strictly finite at `s = 0`;
     - Explicit breaker envelope `E_break(s) = smoothstep(0, s_rise, s) *
       (1 - smoothstep(s_break, s_zone, s))`, `1.0`-periodic wave-set envelope
       driven by `fWaterBeachSetPhase`, `1.0`-periodic asymmetric swash reach
       curve `fSwashReach(fWaterBeachSwashPhase)` (fast surge up-beach via a
       steep rise power curve, slow drain back via a gentler decay curve), and
       `1.0`-periodic wet-sand drying curve `fWetness(s, fWaterBeachSwashPhase)`
       (peaking at the swash envelope and decaying smoothly over the cycle
       before wraparound); and
     - A uniform CPU steepness cap and transition-width floor in
       `WaterUniforms.cpp`: enforce a minimum coast-handoff transition width
       `fCoastBlendWidth >= 3.0 * (fOpenHorizAmpMax + Q_beach * A_beach)` (so
       the smoothstep derivative `1.5 / fCoastBlendWidth` times horizontal
       displacement stays `<= 0.5`), and uniformly cap `Q_beach * A_beach` on
       CPU against `Q_beach * A_beach * (|d(E_break)/ds|_max + E_break_max *
       (k_local_max + k_noise_max) * (1 + |c_skew|)) + (fOpenHorizAmpMax +
       Q_beach * A_beach) * (1.5 / fCoastBlendWidth) < 1` (with `|c_skew| <
       0.7`, plus a safety margin for shore curvature). Paired in
       `WaterDisplacement.comp` with convex coastal blending (`fOpenWeight +
       fBeachWeight <= 1` in the overlap zone, preserving the deliberate
       spatially-varying envelope approximation from the low-directional
       Gerstner rewrite), this prevents vertex reversal across the entire final
       displaced grid sampled by `Water.vert` without per-vertex clamping kinks
       or attenuation discontinuities.
   - Define two flags in `MainLayout` / `GlobalLayout` (`ShaderMainLayout.h` /
     `ShaderGlobalLayout.h`, populated by `WaterUniforms.cpp`):
     - `iWaterBeachVisualsActive`: `0` when `gWaterDisableBeachWaves` or
       `gWaterIgnoreCoastMask` is true; gates the added SDF low-wave fade,
       beach breaker/swash foam in `Water.frag` and `WaterFoam.comp`, and
       inland swash/wet sand in `Terrain.frag`, falling back to existing
       depth-based coast fade when `0`.
     - `iWaterBeachDisplacementActive`: `iWaterBeachVisualsActive != 0 &&
       gWaterLevel != kLow && fWaterBeachAmplitudeScale > 0`; gates vertex
       displacement in `WaterDisplacement.comp` and `Water.vert`
       (`mainLayout.iWaterLowCount > 0 || mainLayout.iWaterFftActive != 0 ||
       mainLayout.iWaterBeachDisplacementActive != 0`).
   - In `WaterDisplacement.comp`, read `vec2 f2ElevSdf =
     texture(elevationTextureSampler, f2Texcoord).xy;` (`fShoreSdf =
     -f2ElevSdf.y`). Compute a smooth inward water-to-beach unit direction
     `f2ShoreDir` via a wide-stencil central/Sobel difference of
     `elevationTextureSampler.y` (e.g., `+/- 1.5 m` in visible-area UV) with a
     length guard (`> 1e-4f`) so flat open ocean produces zero beach-wave
     displacement and no NaNs.
   - When `iWaterBeachVisualsActive != 0`, fade open-ocean low/FFT displacement
     smoothly out as `fShoreSdf` drops through `[fWaterBeachLowFadeEnd,
     fWaterBeachLowFadeStart]` with minimum transition width `fCoastBlendWidth
     >= 3.0 * (fOpenHorizAmpMax + Q_beach * A_beach)` while keeping the coastal
     handoff convex (`fOpenWeight + fBeachWeight <= 1` in the overlap zone,
     preserving the deliberate spatially-varying envelope approximation from
     the low-directional Gerstner rewrite); when `0`, fall back to the existing
     depth-based coast fade (preserving `gWaterIgnoreCoastMask` open-ocean
     isolation mode).
   - Inside `0 < fShoreSdf < fWaterBeachZoneWidth` (when
     `iWaterBeachDisplacementActive != 0`), evaluate the shared `WaterBeach.h`
     shore wave train:
     - Displace vertices along `f2ShoreDir` and `+Z` using sine-phase skew
       (`theta_s = theta + c_skew * sin(theta)`) under the uniform CPU
       steepness cap and minimum transition width `fCoastBlendWidth` from
       `WaterUniforms.cpp` and convex open-ocean/beach blending so the entire
       final displaced grid sampled by `Water.vert` stays non-folding without
       per-vertex attenuation kinks, modulated by the surf-zone envelope that
       rises to peak height at the breaker distance and tapers cleanly to zero
       at `fShoreSdf <= 0` so vertices never clip through dry beach terrain.
     - Accumulate shore-wave displacement, analytical partial derivatives,
       normal contribution, and Jacobian compression into `displacementImage`
       and `normalImage` under the shared slope-addition contract.
   - Apply camera-framing zoom fade-out so beach wave displacement fades
     smoothly at far zoom (`framing >= 600`) while foam/swash remain readable.
3. **Multi-Stage Beach Breaking & Swash Foam (`Water.frag`, `WaterFoam.comp`):**
   - **Shared waterline classifier (`fTerrainElevation`) & swash coordinates:**
     pin `fTerrainElevation` (`elevationTextureSampler.x`) as the single shared
     waterline classifier across both shaders with zero depth-fighting and no
     sign-disagreement gap from independent filtering or `VK_BLEND_OP_MAX`:
     `Water.frag` renders offshore breaker crest foam and seaward swash foam
     everywhere `fTerrainElevation <= 0` (keeping the existing `Water.frag:67`
     `fTerrainElevation > 0` discard untouched), while `Terrain.frag` renders
     the inland swash run-up foam lip wherever `fTerrainElevation > 0.0f &&
     fShoreSdf <= 0.25f` and `fSwashReach > max(-fShoreSdf, 0.0f)` (masked by
     `fBeachPercent`, rejecting crop-edge samples whose linearly filtered SDF
     bled offshore from a `-kfShoreSdfMaxMeters` clear texel while preserving
     seamless waterline coverage at `fShoreSdf ~ 0`). Both shaders gate beach
     foam on `iWaterBeachVisualsActive != 0` and evaluate the same
     `WaterBeach.h` swash reach and phase helpers using the fragment's physical
     camera-cell XY (`f3InPosition.xy`, sampled at its matching visible-area UV
     in `elevationTextureSampler` — the same position already used for the
     `fTerrainElevation` waterline classifier in `Water.frag` and terrain
     shading in `Terrain.frag`), guaranteeing identical physical XY and SDF
     samples on both sides of the `fTerrainElevation == 0` waterline even on
     water triangles that straddle the shore.
   - **Breaker crest and seaward swash foam (`Water.frag`):** when
     `iWaterBeachVisualsActive != 0`, evaluate the shared `WaterBeach.h` phase,
     breaker envelope, and asymmetric swash curve per pixel (for sub-quad
     crispness at `framing = 100`) to generate frothy white foam along the
     steep shore-facing crest and lacy seaward swash up to
     `fTerrainElevation <= 0`, broken up by existing patched foam textures
     (`pFoamColorTextureSamplers` / `pFoamBreakupTextureSamplers`), modulating
     the `Water.frag:197` shoreline alpha fade so swash foam remains visible to
     the waterline.
   - **Surf-zone retained foam mats (`WaterFoam.comp`):** keep
     `WaterFoam.comp:60` world-anchored history reprojection without velocity
     advection — when `iWaterBeachVisualsActive != 0`, inject breaking
     shore-wave emission into the existing retained foam history texture so
     broken waves leave stationary decaying foam mats behind as waves pass,
     decaying naturally via `HalfLife`. When `iWaterBeachVisualsActive == 0`,
     `WaterFoam.comp` stops new beach-foam emission and any previously emitted
     retained surf-zone coverage decays naturally over `HalfLife` through the
     existing foam history path (no separate foam clear pass required).
4. **Inland Swash Lip, Wet Sand Darkening, & Specular Sheen
   (`Terrain/Terrain.frag`):**
   - Include `WaterBeach.h` and sample `vec2 f2ElevSdf =
     texture(elevationTextureSampler, f2InVisibleAreaTexcoord).xy` (already
     bound at `set = 1, binding = 5`), giving `fTerrainElevation = f2ElevSdf.x`
     and `fShoreSdf = -f2ElevSdf.y`. Gate all beach swash and wet-sand effects
     on `iWaterBeachVisualsActive != 0`.
   - Render the inland swash run-up foam lip wherever `fTerrainElevation > 0.0f
     && fShoreSdf <= 0.25f` and `fSwashReach > max(-fShoreSdf, 0.0f)` (masked
     by `fBeachPercent`) using the shared `WaterBeach.h` swash curve evaluated
     at the fragment's physical camera-cell XY (`f3InPosition.xy`, sampled at
     its matching visible-area UV `f2InVisibleAreaTexcoord` in
     `elevationTextureSampler`) so swash meets `Water.frag` seamlessly at
     `fTerrainElevation == 0` while rejecting any positive-elevation sample
     whose linearly filtered SDF bled offshore from a `-kfShoreSdfMaxMeters`
     clear texel at a crop edge.
   - Compute a wetness factor `fWetness` over the inland swash band
     (`fTerrainElevation > 0.0f && fShoreSdf <= 0.25f` and
     `-fWaterBeachWetSandWidth < fShoreSdf`) synced to the shared `WaterBeach.h`
     swash and slow-drying curve so sand stays dark and glossy right after a
     wave recedes and dries out before the next set, while rejecting crop-edge
     samples whose linearly filtered SDF bled offshore.
   - Apply wet-sand darkening (`mix(1.0, fWaterBeachWetSandDarken, fWetness)`)
     to the sand albedo (`f3SandColor`) *before* blending into `f3Color` by
     `fBeachPercent`, and add a sun/moon half-vector specular sheen weighted by
     `fBeachPercent * fWetness` and attenuated by `fShadowSun` / `fShadowMoon`,
     so rock (`fRockPercent`) and snow (`fSnowPercent`) contributions are never
     darkened even where masks overlap.
5. **Beach Sliders, Isolation Control, & Interactive User-Feedback Loop
   (`WaterWrappersBase.h`, `TweaksScreenWater.cpp`):**
   - Add a `"Beach"` subtab in `TweaksScreenWater.cpp` exposing zone width,
     low-wave coast fade range, wavelength, speed, shoaling compression,
     amplitude, front skew, along-shore breakup scale/strength, crest foam,
     swash reach/foam, surf-zone foam emission, and wet-sand width, darkening,
     and specular strength.
   - Support beach-only isolation mode setting `gWaterDisableLowWaves = true`,
     `gWaterDisableMediumWaves = true`, `gWaterDisableBeachWaves = false`, and
     `gWaterIgnoreCoastMask = false` (plus toggling beach waves alongside
     open-ocean waves), and iterate live with the user across zoom levels
     (`framing = 100..600`) until user sign-off.

## Critical files

- `Common/DataFile.h`
- `DataPacker/Source/ExportJobs/ExportIsland.h`
- `DataPacker/Source/ExportJobs/ExportIsland.cpp`
- `DataPacker/Source/ExportJobs/Island/BeachPass.h`
- `DataPacker/Source/ExportJobs/Island/BeachPass.cpp`
- `Engine/Data/Shaders/ShaderLayoutsBase.h`
- `Engine/Data/Shaders/ShaderGlobalLayout.h`
- `Engine/Data/Shaders/ShaderMainLayout.h`
- `Engine/Source/Frame/IslandTerrain.h`
- `Engine/Source/Frame/IslandTerrain.cpp`
- `Engine/Source/Frame/IslandTerrainResidency.cpp`
- `Engine/Source/Graphics/Managers/RenderTargetTextures.cpp`
- `Engine/Source/Graphics/Managers/TextureManager.cpp`
- `Engine/Source/Graphics/Managers/TextureUploadManager.cpp`
- `Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.cpp`
- `Engine/Data/Shaders/Terrain/TerrainElevation.frag`
- `Engine/Data/Shaders/Terrain/TerrainShadowElevation.frag`
- `Engine/Data/Shaders/Terrain/Terrain.frag`
- `Engine/Data/Shaders/Water/WaterBeach.h`
- `Engine/Data/Shaders/Water/Water.vert`
- `Engine/Data/Shaders/Water/WaterDisplacement.comp`
- `Engine/Data/Shaders/Water/WaterFoam.comp`
- `Engine/Data/Shaders/Water/Water.frag`
- `Engine/Source/GameBase.cpp`
- `Engine/Source/Main.cpp`
- `Projects/ModelViewer/Source/ModelViewerApplication.cpp`
- `Engine/Source/Graphics/Graphics.h`
- `Engine/Source/Graphics/Graphics.cpp`
- `Engine/Source/Graphics/Render/Render.h`
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`
- `Engine/Source/Graphics/Render/WaterUniforms.cpp`
- `Engine/Source/Ui/WaterWrappersBase.h`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenWater.cpp`

## In scope

- `Common/DataFile.h`: add `kfShoreSdfMaxMeters` and
  `kfShoreSdfMinInlandMeters` constants and bump `DataHeader::kiVersion`
  unconditionally for runtime stale-pack rejection.
- `DataPacker/Source/ExportJobs/Island/BeachPass.h`, `BeachPass.cpp`,
  `ExportIsland.h`, and `ExportIsland.cpp`: add `ComputeSignedShoreDistance`
  to the `elevation >= 0.0f` zero-contour on the existing cropped elevation
  grid (without modifying `ProcessBakedRegion.cpp` crop bounds or
  `BakeRoute.cpp::kiSplitVersion`), taper `-shoreSdf` toward
  `-kfShoreSdfMaxMeters` over a narrow outer border margin (e.g., `2 m`) inside
  the fixed island crop edge strictly for offshore texels (`shoreSdf > 0`),
  leaving inland texels (`shoreSdf <= 0`) untapered to preserve individual
  inland texels (while gracefully attenuating offshore beach waves across that
  margin when a crop edge falls closer than `kfShoreSdfMaxMeters` to shore),
  pack interleaved `RG16` half-floats (`R = elevation`, `G = -shoreSdf`)
  starting at byte 0, and bump `ExportIsland::kiVersion`.
- `Engine/Data/Shaders/ShaderLayoutsBase.h`,
  `Engine/Source/Frame/IslandTerrain.h`, `IslandTerrain.cpp`,
  `IslandTerrainResidency.cpp`,
  `Engine/Source/Graphics/Managers/RenderTargetTextures.cpp`,
  `TextureManager.cpp`, `TextureUploadManager.cpp`, and
  `WorldLightingShadowPipelines.cpp`: introduce
  `kVkFormatTerrainElevationSdf = VK_FORMAT_R16G16_SFLOAT` for per-island
  elevation+SDF textures and `mTerrainElevationTexture` (keeping shadow
  elevation at `kVkFormatElevation = VK_FORMAT_R16_SFLOAT`), clear
  `mTerrainElevationTexture` to `{mfSeaFloorElevation, -kfShoreSdfMaxMeters, 0,
  1}`, upload interleaved `RG16` from byte 0 at 4 bytes/texel, update all
  three `IslandTerrain.cpp` consumers for the 4-byte (`2 * sizeof(uint16_t)`)
  texel stride — payload bounds check (`IslandTerrain.cpp:167`) and
  post-heightmap slicing (`IslandTerrain.cpp:155,188`), placement height
  sampling (`puiHeightmapHalf[2 * i]`), and navigation contour decode
  (`IslandTerrain.cpp:236`, `XMConvertHalfToFloatStream` input stride
  `2 * sizeof(uint16_t)`) — for bit-identical CPU heights and nav contours,
  and wire `TerrainShadowElevation.frag` into the shadow pipeline.
- `Engine/Data/Shaders/Terrain/TerrainElevation.frag` and new
  `Engine/Data/Shaders/Terrain/TerrainShadowElevation.frag`:
  `TerrainElevation.frag` outputs `vec2(fElevation, fNegShoreSdf)` under
  `VK_BLEND_OP_MAX`; `TerrainShadowElevation.frag` outputs `float fOutElevation`
  from `.x` for the shadow pass while preserving the exact existing underwater
  elevation compression formula from `TerrainElevation.frag` for `fRaw < 0`
  (unchanged for `fRaw >= 0`).
- `Engine/Data/Shaders/ShaderGlobalLayout.h`, `ShaderMainLayout.h`,
  `Engine/Source/GameBase.cpp`, `Engine/Source/Main.cpp`,
  `Projects/ModelViewer/Source/ModelViewerApplication.cpp`,
  `Engine/Source/Graphics/Graphics.h`, `Graphics.cpp`,
  `Engine/Source/Graphics/Render/Render.h`, `GlobalUniforms.cpp`, and
  `WaterUniforms.cpp`: thread the `double` simulation clock
  (`std::chrono::duration<double>`, computed via
  `common::NanosecondsToFloatSeconds<double>` in `GameBase::Render`,
  `Main.cpp`, and `ModelViewerApplication.cpp` around line 937 from
  ModelViewer's native nanosecond timer before any `float` conversion) through
  `Graphics::RenderGlobal` and `RenderFrameGlobal` into
  `PopulateWaterParameters`, compute `dDeltaTime = dCurrentTime -
  dPreviousBeachTime` in `double` before any `float` conversion (initializing
  `dPreviousBeachTime = dCurrentTime` on the first frame / reset), integrate
  three periodic reduced phases in `[0, 1)` cycle units —
  `GlobalLayout.fWaterBeachWavePhase`, `GlobalLayout.fWaterBeachSetPhase`, and
  `GlobalLayout.fWaterBeachSwashPhase` — in persistent `double` accumulators
  (`fmod(acc + dDeltaTime * dRate, 1.0)`), enforce the minimum coast-handoff
  transition width `fCoastBlendWidth >= 3.0 * (fOpenHorizAmpMax + Q_beach *
  A_beach)` (keeping the smoothstep derivative `1.5 / fCoastBlendWidth` times
  horizontal displacement `<= 0.5`), apply the uniform CPU steepness cap
  clamping `Q_beach * A_beach` against `Q_beach * A_beach *
  (|d(E_break)/ds|_max + E_break_max * (k_local_max + k_noise_max) * (1 +
  |c_skew|)) + (fOpenHorizAmpMax + Q_beach * A_beach) * (1.5 /
  fCoastBlendWidth) < 1` (with `|c_skew| < 0.7` and a shore-curvature safety
  margin), and publish beach-wave, swash, and wet-sand uniforms, camera-cell
  reduced noise origins, `iWaterBeachVisualsActive`, and
  `iWaterBeachDisplacementActive`.
- `Engine/Data/Shaders/Water/WaterBeach.h` (new): shared GLSL include reading
  `globalLayout.fWaterBeachWavePhase`, `globalLayout.fWaterBeachSetPhase`, and
  `globalLayout.fWaterBeachSwashPhase` (identically across
  `WaterDisplacement.comp`, `WaterFoam.comp`, `Water.frag`, and `Terrain.frag`)
  with every consumer curve (wave crest, wave-set modulation, swash
  surge/drain, and wet-sand drying tail) strictly `1.0`-periodic in its
  assigned phase for seamless wraparound; camera-cell-reduced world sampling
  coordinates (pinned to physical camera-cell XY `f3InPosition.xy` and its
  matching visible-area UV for swash in both `Water.frag` and `Terrain.frag`);
  procedural 2D smooth hash/value noise via `ShaderRandom.h`; bounded shoaling
  distance `s_eff = max(fShoreSdf, 0.0f) + fShoalEps` (`fShoalEps >= 0.5 m`)
  with `phi_base(s) = k_shore * (s + c_shoal * (sqrt(s + fShoalEps) -
  sqrt(fShoalEps)))`; breaker envelope `E_break(s) = smoothstep(0, s_rise, s) *
  (1 - smoothstep(s_break, s_zone, s))`; wave-set envelope; and asymmetric
  swash reach and wet-sand drying curves (consuming the uniform CPU-capped
  `Q_beach * A_beach`, minimum coast-handoff transition width
  `fCoastBlendWidth`, and `|c_skew| < 0.7`).
- `Engine/Data/Shaders/Water/Water.vert`: gate displacement sampling on
  `mainLayout.iWaterLowCount > 0 || mainLayout.iWaterFftActive != 0 ||
  mainLayout.iWaterBeachDisplacementActive != 0`.
- `Engine/Data/Shaders/Water/WaterDisplacement.comp`: compute smooth inward
  shore direction from `elevationTextureSampler.y`, gate the SDF low/FFT
  coast fade on `iWaterBeachVisualsActive != 0` (falling back to existing
  depth-based coast fade when `0`) with minimum coast-handoff transition width
  `fCoastBlendWidth >= 3.0 * (fOpenHorizAmpMax + Q_beach * A_beach)` and convex
  coastal blending (`fOpenWeight + fBeachWeight <= 1` in the overlap zone,
  preserving the deliberate spatially-varying envelope approximation from the
  low-directional Gerstner rewrite), and gate asymmetric shoaling beach waves
  via `WaterBeach.h` (with analytical normals, Jacobian contribution, and
  uniform CPU-capped steepness so the entire final displaced grid sampled by
  `Water.vert` stays non-folding without per-vertex attenuation discontinuities)
  on `iWaterBeachDisplacementActive != 0`.
- `Engine/Data/Shaders/Water/WaterFoam.comp` and `Water.frag`: gated on
  `iWaterBeachVisualsActive != 0`, add per-pixel breaker crest foam and
  seaward swash foam everywhere `fTerrainElevation <= 0` (keeping the existing
  `Water.frag:67` `fTerrainElevation > 0` discard untouched and modulating
  `Water.frag:197` shoreline alpha fade) via `WaterBeach.h` using the
  fragment's physical camera-cell XY (`f3InPosition.xy`) and its matching
  visible-area UV in `elevationTextureSampler` for swash phase/SDF (matching
  `Terrain.frag` across shore-straddling triangles), and inject breaking
  shore-wave emission into `WaterFoam.comp` for stationary decaying surf-zone
  foam mats (stopping new emission when `iWaterBeachVisualsActive == 0` so
  retained coverage decays naturally over `HalfLife` without a separate clear
  pass).
- `Engine/Data/Shaders/Terrain/Terrain.frag`: gated on
  `iWaterBeachVisualsActive != 0`, guard inland swash and wet sand with
  `fTerrainElevation > 0.0f && fShoreSdf <= 0.25f` (rejecting any
  positive-elevation sample whose linearly filtered SDF bled offshore from a
  `-kfShoreSdfMaxMeters` clear texel at a crop edge while preserving seamless
  waterline coverage at `fShoreSdf ~ 0`), render the inland swash run-up foam
  lip wherever `fSwashReach > max(-fShoreSdf, 0.0f)` (masked by
  `fBeachPercent`) using `WaterBeach.h` at the fragment's physical camera-cell
  XY (`f3InPosition.xy`) and its matching visible-area UV in
  `elevationTextureSampler.xy`, darken `f3SandColor` before blending into
  `f3Color` by `fBeachPercent`, and add sun/moon specular sheen weighted by
  `fBeachPercent * fWetness`.
- `Engine/Source/Ui/WaterWrappersBase.h` and
  `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenWater.cpp`: add beach-wave
  and wet-sand wrappers, `gWaterDisableBeachWaves` and beach-only isolation
  controls, and the `"Beach"` Tweaks subtab, followed by interactive tuning
  with the user.

## Out of scope

- Custom per-island shoreline ribbon meshes or 3D curling-barrel overhang
  geometry.
- Runtime GPU Jump Flooding distance-field generation.
- Changes to FFT medium-wave spectrum math or open-ocean spectral Gerstner math
  beyond coastal fade handoff.
- Gameplay collision, ship buoyancy, or simulation determinism/CRC changes
  (CPU height queries must remain bit-identical).
- Merging or landing `water-waves` onto `main` (handled as a separate
  post-series user-confirmed integration step; any user-directed removal of the
  temporary wave-isolation controls and its build/visual verification happen on
  `water-waves` before requesting confirmation to land onto `main`).
- Unit tests.

## Risk triggers and invariants

Tier 3: changes the `DataPacker` island `.pack` binary format/version and spans
independently owned subsystems (`DataPacker`, `Frame`, `Terrain`, and `Water`).

- **Simulation & collision determinism:** Leaving `ProcessBakedRegion.cpp`
  crop bounds untouched and updating all three `IslandTerrain.cpp` stride-2
  consumers (payload bounds check/slicing, placement height sampling
  `puiHeightmapHalf[2 * i]`, and navigation contour `XMConvertHalfToFloatStream`
  decode) guarantees bit-identical CPU terrain heights at every world
  coordinate and unchanged replay determinism CRC.
- **No backward compatibility:** Bump both `ExportIsland::kiVersion` and
  `DataHeader::kiVersion` unconditionally so stale island `.manifest`/`.pack`
  pairs are rejected cleanly at runtime rather than misread as `R16`.
- **Multi-island `VK_BLEND_OP_MAX` correctness:** Negated signed distance
  (`-shoreSdf`) cleared to `-kfShoreSdfMaxMeters` and tapered toward
  `-kfShoreSdfMaxMeters` over a narrow outer border margin (e.g., `2 m`) inside
  the fixed island crop edge strictly for offshore texels (`shoreSdf > 0`) —
  leaving inland texels (`shoreSdf <= 0`) untapered — eliminates offshore
  quad-edge step jumps (gracefully attenuating beach waves across that margin
  when a crop edge falls closer than `kfShoreSdfMaxMeters` to shore) while
  preserving individual inland texels. In `Terrain.frag`, guarding inland swash
  and wet sand with `fTerrainElevation > 0.0f && fShoreSdf <= 0.25f` rejects
  any positive-elevation sample whose linearly filtered SDF bled offshore from
  a `-kfShoreSdfMaxMeters` clear texel at a crop edge while preserving seamless
  waterline coverage at `fShoreSdf ~ 0`. `max(-s_A, -s_B) == -min(s_A, s_B)`
  holds on the post-taper stored distances across both offshore (`s > 0`) and
  overlapping interiors (`s < 0`).
- **No NaNs or folding artifacts:** Guard the shore-SDF gradient length before
  normalization (flat open ocean yields zero beach-wave contribution), keep
  `k_local` strictly finite at `s = 0` via `s_eff = max(fShoreSdf, 0.0f) +
  fShoalEps` (`fShoalEps >= 0.5 m`), enforce a minimum coast-handoff transition
  width `fCoastBlendWidth >= 3.0 * (fOpenHorizAmpMax + Q_beach * A_beach)` (so
  the smoothstep derivative `1.5 / fCoastBlendWidth` times horizontal
  displacement stays `<= 0.5`), and uniformly cap `Q_beach * A_beach` on CPU in
  `WaterUniforms.cpp` against `Q_beach * A_beach * (|d(E_break)/ds|_max +
  E_break_max * (k_local_max + k_noise_max) * (1 + |c_skew|)) +
  (fOpenHorizAmpMax + Q_beach * A_beach) * (1.5 / fCoastBlendWidth) < 1` with
  `|c_skew| < 0.7` plus a safety margin for shore curvature. Paired with convex
  open-ocean and beach blending (`fOpenWeight + fBeachWeight <= 1` in the
  overlap zone, preserving the low-directional Gerstner rewrite's
  spatially-varying envelope approximation) in `WaterDisplacement.comp`, this
  prevents vertex reversal across the entire final displaced grid sampled by
  `Water.vert` without per-vertex attenuation kinks, and beach-wave displacement
  tapers cleanly to zero at `fShoreSdf <= 0`.
- **Unbounded cell precision:** Along-shore procedural noise phase offsets in
  `WaterBeach.h` must use CPU-reduced camera-cell origins so beach wave breakup
  never jumps across cell boundaries.
- **Zero-work Low quality and clean isolation:** `iWaterBeachVisualsActive` is
  `0` when `gWaterDisableBeachWaves` or `gWaterIgnoreCoastMask` is true
  (disabling the added SDF low-wave fade, new beach-foam emission/shading while
  retained surf-zone coverage decays naturally over `HalfLife`, and wet sand,
  and falling back to depth-based coast fade), and
  `iWaterBeachDisplacementActive` is `iWaterBeachVisualsActive != 0 &&
  gWaterLevel != kLow && fWaterBeachAmplitudeScale > 0`, keeping displacement
  compute disabled on Low quality and skipping beach-wave vertex displacement
  without validation errors.

## Acceptance criteria

1. `/compile` builds `DataPacker`, `BrokenEngineSandbox` client, and server
   cleanly, and island `.pack` files re-export cleanly under the bumped
   `ExportIsland::kiVersion` and `DataHeader::kiVersion`.
2. Runtime Vulkan validation (`agent-harness` with validation layers) passes
   with zero validation errors across normal rendering, beach-only isolation
   mode (`gWaterDisableLowWaves = true`, `gWaterDisableMediumWaves = true`,
   `gWaterDisableBeachWaves = false`, `gWaterIgnoreCoastMask = false`), and
   `gWaterLevel == kLow` (zero beach displacement).
3. Stale-version rejection is verified at runtime: loading a stale island
   `.manifest`/`.pack` pair carrying the pre-change `DataHeader::kiVersion`
   (checked from `.manifest` at `PackChunks.cpp:269` before opening `.pack`)
   is cleanly rejected instead of misread as `R16`.
4. Direct pre-change vs post-change CPU terrain height comparison at the same
   world positions confirms bit-identical heights (and identical navigation
   contour inputs), and the replay determinism check passes with zero CRC
   mismatch.
5. Around all 360 degrees of an island (windward, leeward, and side coasts, as
   well as coves and headlands), open-ocean directional swells fade smoothly
   into shore-normal beach waves that visibly march from water toward the
   beach, compress as they shoal, and break with staggered organic fronts (no
   uniform concentric contour rings).
6. Breaker crest foam, seamless waterline-split advancing/receding swash foam
   classified by `fTerrainElevation` (`Water.frag` for `<= 0`, `Terrain.frag`
   for `> 0`), and stationary decaying surf-zone foam mats read clearly from
   `framing = 100` through `framing = 600` without cell-boundary jitter,
   waterline gaps, depth-fighting, or shore clipping artifacts.
7. Beach sand in the swash band visibly darkens and catches sun/moon specular
   glint after each wave recedes, then dries back smoothly before the next
   wave, without affecting rock or snow masks.
8. User explicitly signs off on the beach waves, foam, and wet sand across zoom
   levels before landing onto `water-waves`.
9. The session branch stays at `main` through `/next-plan` step 6's claim
   refresh (inspecting `water-waves` via Git during step-4/5 preparation and
   review), then moves onto `water-waves` (with `main` merged into
   `water-waves` first if `main` holds completed external prerequisites or
   overlapping updates) for implementation, builds, and verification; at
   `/finalize-changes`, the implementation and Plan completion land onto
   `water-waves` under the global landing lock — either by switching primary to
   `water-waves` for preparation and landing together (then switching primary
   back to `main`), or via a user-authorized locked update of
   `refs/heads/water-waves` while primary stays on `main` — then the Plan claim
   is released and only the Plan deletion and any child `dependsOn` edge
   removals sync onto `main` (with no water-wave code on `main`). Merging
   `water-waves` into `main` remains a separate post-series user-confirmed
   integration step, with any user-directed removal of the temporary
   wave-isolation controls built and visually verified on `water-waves` before
   requesting confirmation to land onto `main`.
