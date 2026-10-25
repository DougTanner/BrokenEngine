# Water foam prototype comparison and shared design

Research/design baseline: `5f205af6a24a17ebf732f7005f03a48e0e6f91e0`, 2026-10-23. This non-executable reference owns shared contracts for two executable scheduler Plans in `Documents/Plans/Engine/`. No prototype implementation, worktree setup, build, capture, or performance measurement is claimed. Runtime work awaits Tier3 execution approval; saving these documents authorizes documentation only.

## Comparison overview

| Prototype Plan | Emission | Lifetime | Purpose |
|---|---|---|---|
| [A](../Engine/WaterFoamPrototypeA.md) | Existing horizontal Jacobian compression | Instantaneous | Cheap control; does not satisfy requested persistence |
| [B](../Engine/WaterFoamPrototypeB.md) | Same horizontal Jacobian compression | Exponential persistent coverage | Test persistent fading and filtered coverage |

A deliberately retains its cheap interpolated-J source and lacks B's coverage filtering. The comparison therefore
measures the whole cheap baseline against persistent filtered coverage, rather than isolating persistence alone.
Use identical shading, sea state, noise, camera, and daylight. Keep shared source thresholds fixed for the first
comparison, with unit source strength in A and the stated initial emission rate in B; record their values, source
coverage, and resulting foam coverage rather than tuning to equal brightness. Label any later tuning sweep separately.
A is a control because it has no temporal fade and does not satisfy the requested persistence. Recommend **B,
compression-triggered persistent fading coverage**, as the first solution experiment: it reuses the existing compression
signal and directly tests fading. Four seconds is an initial half-life hypothesis, not a requirement or measured result.
No runtime winner has been visually verified.

## Primary research and interpretation

The [Crest foam manual](https://docs.crest.waveharmonic.com/Manual/Appearance/Foam.html) separates foam generation,
dissipation, and appearance. Its [public legacy
implementation](https://raw.githubusercontent.com/wave-harmonic/crest/master/crest/Assets/Crest/Crest/Shaders/Resources/UpdateFoam.compute)
remaps prior foam into the new footprint and applies timestep-dependent decay before new sources. This motivates the
retained coverage field, without asserting this is the latest package's exact implementation.

[Dupuy and Bruneton's deformation whitecaps paper](https://liris.cnrs.fr/Documents/Liris-5812.pdf) and [author implementation](https://github.com/jdupuy/whitecaps) address prefiltered coverage at large viewing scales. Their moment representation is a materially different filtering approach, not a temporal lifetime mechanism. These prototypes first generate bounded coverage and then filter it; they do not implement the paper's moment model.

Particle transport is another real approach: [SideFX Ocean Foam](https://www.sidefx.com/docs/houdini/nodes/sop/oceanfoam.html)
emits particles and simulates their motion using ocean velocity and neighboring-particle proximity. Crest's public
shader also backtraces history through a flow field and has depth-driven shoreline inputs. These are transport and
additional-source choices, separate from compression emission and fading. They do not warrant separate prototypes now:
the engine has no existing foam-particle or fluid-current field, and shoreline emission would change the question from
offshore whitecaps to surf. The moment-filtering alternative targets a separate sampling limitation; revisit it if B's
measured distant coverage is unstable. A and B are the two useful experiments for the present request.

Research links are references, not measured results for Broken Engine. The research inventory and repository evidence below are part of this durable design bundle.

## Authority and supersession

The current request for fade-out supersedes the former instantaneous-only WaterFoam feature proposal, which is removed.
A serves only as a control. Distinct shoreline guidance is retained below as deferred reference, outside both Plans.
[WaterDisplacement.comp](../../../Engine/Data/Shaders/Water/WaterDisplacement.comp) already writes J to normal W and
zero to displacement W. Preserve that J ownership and leave displacement W unchanged. Shore and ship-wake sources
remain separate future work, not part of these comparisons.

## Shared source and shading rules

Use `S=clamp((Jstart-J)/(Jstart-Jfull),0,1)` in A/B, with starting hypotheses Jstart=0.8 and Jfull=0.3; enforce Jfull<Jstart in existing tweak-control style. J is dimensionless and identity J=1 emits zero. Terrain at/above water emits zero even though the compute land path stores J=0. Near-shore wave-band weights remain the current implementation's; no added shore-depth emitter is mixed into this experiment.

Lifetime sweeps use half-lives 1s, 4s, and 12s, all hypotheses. Foam albedo starts at linear RGB (0.8,0.85,0.85), a
hypothesis. Mix it into `f3PreLightingColor` before directional lighting in `Water.frag:109-113`; reduce skybox specular
by `(1-F)` in the existing specular combine (`:128-137`). Retain shared height-darken, shadows, smoke, and ambient
lighting. Use identical stationary precision-safe existing noise with a weak modulation `Fshade=F*mix(1,noise,0.2)`; no
noise threshold, scrolling speed, new normals, or bubbles. Noise is shading only, not a source or history eraser. At
unresolved noise scale retain its mean through existing textureGrad sampling.

B computes emission on the native active water grid before interpolation: threshold each of four native J samples and
mask each with terrain depth at its native grid position, then bilinearly interpolate their bounded source values.
Clamp native sample indices to active dimensions, never allocated inactive texels. This avoids thresholding an averaged
J, but cannot reconstruct foam events missed by the native wave grid. A thresholds interpolated J, an explicit baseline
limitation. For stable bird's-eye coverage, B filters the bounded coverage through presentation mips for the fragment's
pixel footprint. That preserves resolved mean coverage; it cannot recover unresolved native-grid events or guarantee
temporal stability. Judge the distinction in the moving-camera resolution sweep below.

## Retained coverage design for B

Use two R16_SFLOAT images at a starting 1024x512 allocation: current coverage (one mip, storage/sampled/transfer source)
and combined history/presentation coverage (full mip chain, sampled/transfer source/destination). Compute reads previous
coverage explicitly from mip0 of the second image; fragments use its generated mips after publication. Existing Texture
sampled views include all mips, and existing copy support targets mip0 (`Texture.cpp:62,415`;
`PipelineDescriptorWriter.cpp:255,268`); no texture API change is needed. Approximate allocation is
`(1+4/3)*1024*512*2`, about 2.33 MiB plus allocation overhead; record the exact rectangular mip sum at implementation.
Creation explicitly checks format storage, sampled-linear, and blit source/destination support using existing
InstanceManager capability validation. Unsupported devices fail with a named prototype requirement; no fallback format
matrix.

The history coordinate is the undisplaced horizontal wave parameter q. Newly emitted foam is shaded at its corresponding
wave parameter; it follows the prescribed wave displacement and is stationary in parameter space between emissions. No
current/advection or crest-speed transport is implied. Fragment sampling converts the existing
`f2InInitialPosition` (undisplaced camera-relative XY) to the current camera-cell basis with
`qCell=f2InInitialPosition+vec2(globalLayout.fWaterOriginX,globalLayout.fWaterOriginY)`, then maps qCell to coverage UV
using the current visible-area bounds and the same top-is-max-Y convention below. The existing
`f2OutTexcoord`/fragment texcoord is displaced and must not be used for this mapping; no new vertex varying is needed.

Each texel center maps to qCell using current visible area `(left,top,right,bottom)`, where top is max-Y. Keep previous area
in its own retained camera basis. On a one-cell move use `RetainedAreaBasis::Advance` and `ShiftArea` (`Render.h:59-87`)
to shift the previous rectangle into the current basis before publishing; multi-cell jumps reset per that existing
helper. With previous rectangle in the same current camera-cell basis as qCell:
`u=(qCell.x-left)/(right-left)`, `v=(top-qCell.y)/(top-bottom)`. Reject outside
[0,1] before clamp sampling; reject source border beyond half a texel so clamped edge foam does not stretch. New area
starts at zero history. Same mapping handles pan, snap, zoom, and mesh LOD change. The coverage grid remains fixed-size
across active mesh LOD; source detail can still change with the native wave grid. Do not reset merely for LOD/ordinary
zoom, because that would discard the effect being compared.

Per presentation update integrate `dF/dt = r*S*(1-F) - lambda*F`. For the frame-held source, `a=r*S+lambda`,
`Feq=r*S/a`, `Fnew=Feq+(Freprojected-Feq)*exp(-a*dt)`; half-life=4s (`lambda=ln(2)/4`) and r=1.5/s are starting
hypotheses. With S=0 this is exact exponential fade; at dt=0 preserve F, and a=0 explicitly preserves F. CPU precomputes
lambda and uniform-only terms. Derive dt once from the same monotonic render elapsed clock used by
`PopulateWaterReducedUv` (`WaterUniforms.cpp:78-82`), not simulation ticks, render frame count, or a clamped fixed
timestep. A skipped render uses time since last submitted/published foam update. First publication, graphics recreation,
a backwards/reset clock, large basis jump, Low quality, and off/on reset history. Disable/Low runs the fixed
full-coverage dispatch and writes zero without touching stale wave textures; copies remain valid. Normal time gaps decay
using their actual duration. Fast-changing emission remains temporally sampled; no claim of frame-rate-invariant event
detection is made.

Publish current/previous area, dt, reset, enabled state, source thresholds, and lifetime controls once in Global from
WaterUniforms, before Global submission; no Main-to-Global writes. Keep single-owner area/timestamp statics there;
externally-triggered recreate reset lives in Render.h. Register new retained area/reset count in the existing
presentation continuity snapshot, since its guarantee covers it. Updating the probe serialization must be explicitly
added to scope if the existing probe needs fields to expose that snapshot; otherwise inspect via capture and avoid
widening this prototype into Agent work.

## GPU order and lifetime for B

Record a fixed chain after `RecordWaterDisplacement` and before scene rendering (`CommandBufferRecordMain.cpp:45-53`).
Current starts shader-read and transitions to compute-write; compute writes the full allocation while reading previous
history/presentation mip0 explicitly at LOD0. Transition current compute-write to transfer-read. Before overwriting
history/presentation, barrier its prior compute reads of mip0 and prior-frame fragment reads of all mips into transfer
writes. Copy current to history/presentation mip0 once, then generate the remaining mip levels with per-subresource
transfer-write/read barriers and linear blits. Transition each completed level back to shader-read with both compute and
fragment destination scopes; return current to shader-read. Only mip0 is temporal state; other mips are presentation
filtering rebuilt from the new coverage.

Reuse the existing serialized Main submission chain, never alternate descriptors by CPU frame parity. The start/end layout of every subresource is identical each frame. `Texture::TransitionImageLayout` maps shader/compute/transfer masks (`Texture.cpp:18-45`); record subresource mip transitions locally because the whole-image helper covers all mips. Verify the existing frame submission dependencies in review rather than inferring safety merely from one queue.

First-use reset branches do not sample undefined history; every output texel is initialized before presentation/copy.
CPU reset clears only once after publishing an actual update. Create/destroy both textures with the existing water
render-target recreation, under the existing device-drain ownership; destroy dependent pipelines/descriptors before
views and rebuild after allocation. Do not add queues, thread workers, resource retirement, per-frame command
rerecording, or runtime descriptor swapping. 

## Altitude and fair setup

`MainUniforms.cpp:436-453` fades both amplitudes with eye height; wrapper defaults are 300–600 with maximum 1000
(`WaterWrappersBase.cpp:79-94`). Engine positions/heights use meters (shader layout flight-plane field comment). Thus
sources vanish above 600m with defaults, including 2km and 5km. Record that baseline explicitly. Each prototype raises
only the four existing fade wrappers' allowed maximum to 10000; retain default values. The shared diagnostic preset sets
start=5000/end=10000 for both bands. This intentionally changes visible geometry equally in both worktrees; capture
matched no-foam screenshots under that same preset. Do not add a foam distance fade. Whether production should keep
geometry bands alive, generate an independent source, or accept no far whitecaps remains a production decision after
results.

## Runtime comparison matrix

After execution approval, use separate preserved prototype worktrees/branches from the same recorded comparison baseline;
none is created by this documentation change. Through /compile build BrokenEngineSandbox client Debug|x64 and
Release|x64 with Local shader/data generation, plus server Release|x64 because both targets consume shared layouts.
Through /agent-harness launch each worktree's own assets/executables. No new unit tests.

Each Plan is independent and produces its own concrete result report in this Investigations directory. Capture the
applicable cohorts independently and perform A/B comparison when both reports exist; neither Plan depends on the
other's completion. Reports identify the preserved branch, commit, worktree, runnable executables/assets, commands,
captures, GPU results, and limitations. Plan-completion landing carries reports and bookkeeping only; experimental
runtime changes remain on the preserved branches/worktrees. Neither candidate automatically becomes production water.

| Test cohort | Fixed inputs | Evidence and interpretation |
|---|---|---|
| Default altitude | High quality; heights 100,450,700,2000,5000m; default fade | No-foam baseline before the first edit; show zero source above600 and existing band transition |
| Fair source comparison | Same camera/sea/day/seed/time; extended fade preset; heights100,1000,2000,5000 | Matched screenshots and moving captures A/B; annotate A control/filter difference |
| Lifetime | Emit to stable field, set r=0; fixed camera; dt cohorts30/60/120fps | B residual after4s should be 0.5 of starting coverage within R16/filter/readback error (target2% relative away from zero); A disappears immediately when source disabled |
| Reprojection | Stationary wave phase/source off; translate camera, cross snap/LOD threshold and one cell; zoom in/out | Same-world patch survives in overlap; no swimming/edge streak, newly exposed area starts empty; multi-cell jump clears by design |
| Recreation | Shape detail/resize, quality Low/Medium/High, toggle foam, minimize/restore, graphics reset | No uninitialized flashes or validation errors; documented reset on allocation/toggle/Low; actual elapsed fade on ordinary render gaps |
| Source filtering | Water shape detail1/8,1/4,1/2; foam dimensions512x256,1024x512,2048x1024; slowly zoom/pan | Measure coverage fraction and temporal variance; report native-grid aliasing separately from presentation-mip filtering. Fixed resolution is not a solution claim |
| Cost | Same scene, cohorts with foam off/on, warmup and repeated captures | Whole-frame profile and RenderDoc pass durations/resources; show coverage pass, copy/mips, shading cost and actual bytes. No promised speedup or budget invented |

Acceptance for a prototype is correct implementation, safe lifecycle, bounded output, truthful comparison evidence, and recorded visual/aliasing/cost outcomes. Poor visual quality or high cost is an experimental result; it cannot justify claiming a production winner. If native-grid loss makes a source untestable, record that failure and return the concrete next decision, rather than silently adding another source generator.

## Classification and fixed experiment decisions

Document authorship is Tier1 only. All proposed runtime work is Tier3 because it changes shared CPU/GLSL uniform data layout (`Engine/Data/Shaders/AGENTS.md:9-13`; layouts consumed by both targets at `:7`). B additionally exposes retained GPU history and resource visibility/lifetime. All remain client rendering outside simulation CRC (`Graphics/Render/AGENTS.md:3-5`); no protocol, serialization, replay, collection, or trust-boundary format changes are authorized.

The Plans adopt the extended-band comparison preset, B parameter-space stationary history as the first approximation,
and bounded native-grid sources with measured limitations. These are Plan-authored design choices, not choices
explicitly made by the user. They leave no pending architecture choice for executing the experiments; ordinary
Tier3 execution approval is still required.
Production altitude policy, physical transport, moment-based filtering, source resolution upgrades, shore foam, wakes,
and production winner remain deferred decisions outside these Plans.

## Broader research inventory

### Recommendation constraints

Research recommendation, conditional on the actual wave representation: use crest compression to emit a scalar foam
coverage history, fade that history with elapsed time, and filter its rendered coverage for the pixel footprint. Keep
emission, transport, lifetime, and surface texture separate. This preserves the requested sense of foam appearing at
concentrated wave activity and remaining after the crest passes. Treat optional height or motion gates as artistic controls rather
than universal physical criteria. No FFT or Gerstner migration is implied.

For a kilometers-high camera, judge broad foam patches, coast bands, and visible wakes first. Bubble geometry and detailed foam normals are optional close-view features; no source establishes their benefit for this view. Do not enlarge every tiny whitecap until visible: preserve its average reflectance contribution instead. Validate while moving and zooming, because a good still can hide temporal aliasing.

### Emission alternatives

| Signal | Strength | Limitation / recommendation |
|---|---|---|
| Horizontal displacement Jacobian determinant | Direct measure of mapped surface area compression; useful where the wave model already has horizontal displacement | Low determinant identifies pinching; negative indicates map folding. It is a geometric rendering proxy, not proof that the modeled fluid physically broke. A height-only surface has identity horizontal map and needs another signal. |
| Signed crest curvature / steepness | Can derive from a height field without inventing horizontal displacement | Curvature scales strongly with wavelength and needs scale-aware thresholds. Steepness magnitude alone is zero at the summit of a smooth sinusoidal crest; combine with crest location if desired. Artistic heuristic unless separately validated. |
| Absolute height | Cheap broad crest mask | Tall nonbreaking swell also passes; zero level depends on representation. Use only as a modulation. |
| Particle velocity / evolving crest speed | [Boettger et al.](https://arxiv.org/abs/2008.00633) study a normalized energy-flux breaking criterion in phase-resolved waves | Requires water velocity and crest tracking absent from this artistic wave field. Particle motion is distinct from wave propagation ([NOAA](https://oceanexplorer.noaa.gov/ocean-fact/waves/)); motion alone is not a recommended source here. |

### Persistence and source implementation evidence

[Current Crest documentation](https://docs.crest.waveharmonic.com/Manual/Appearance/Foam.html) describes pinched-wave and shallow-water emission, incremental dissipation (default 30 updates/second), texture-based shading, and additional authored foam inputs. It identifies the maximum accumulation as relevant to lasting wakes. These are pragmatic rendering choices.

[Crest's public
UpdateFoam.compute](https://raw.githubusercontent.com/wave-harmonic/crest/master/crest/Assets/Crest/Crest/Shaders/Resources/UpdateFoam.compute)
provides concrete source evidence: lines 43-65 backtrace the previous history through a flow field and account for
cascade changes; lines 68-69 multiply by a timestep-dependent fade; lines 74-94 sample displaced neighbors, calculate a
horizontal determinant, and add thresholded foam; lines 98-104 add depth-driven shoreline foam and saturate. The public
legacy shader is corroborating implementation evidence, not a claim that the current proprietary package uses identical
code.

Planning inference: retain an existing flow field if available; do not create a fluid solver solely for advection.
B uses parameter-space stationary history with fading as its first approximation. Constant current and wave phase motion are
different transport choices. Optional exponential decay exp(-dt/tau) makes the chosen lifetime independent of update
subdivision. A camera-following history requires spatial reprojection and explicit handling of newly exposed regions;
otherwise foam swims with camera motion. B's concrete mapping is specified in its retained-coverage section above.

### Filtering and cost evidence

[Dupuy and Bruneton, 2012](https://liris.cnrs.fr/Documents/Liris-5812.pdf) propose prefilterable deformation-based
whitecap coverage over very large viewing scales. Their [author implementation](https://github.com/jdupuy/whitecaps) is
primary executable evidence. [whitecap_precompute.glsl lines
35-44](https://raw.githubusercontent.com/jdupuy/whitecaps/master/whitecap_precompute.glsl) stores determinant-related
values and squared values; [ocean.glsl lines
310-324](https://raw.githubusercontent.com/jdupuy/whitecaps/master/ocean.glsl) reconstructs mean and variance and
evaluates fractional coverage. This is an established alternative to applying a hard threshold to a point sample. It
does not supply lingering history by itself.

Planning inference: thresholding a filtered displacement signal loses unresolved foam events because nonlinear thresholding and averaging do not commute. Generate a resolved scalar coverage field before mip filtering where practical; alternatively adopt moment-based threshold coverage when its quality justifies its extra textures and passes. Derivative-smoothed thresholds help edges but do not reconstruct missing subpixel events. Preserve mean coverage as foam detail mips away.

Cost is dominated by coverage area, resolution, update rate, texture traffic, and mip generation. A ping-pong
single-channel history requires two surfaces; an N by N pair with b bytes/texel costs 2*N*N*b bytes before mips
(inference). Compression from an existing displacement field can need neighboring texture samples; the cited Crest path
uses center and two neighbors. Crest's current [Water Renderer
documentation](https://docs.crest.waveharmonic.com/Components/System/WaterRenderer.html) exposes independent foam
resolution and formats, and explicitly trades sharper resolution against memory usage. Its blur is restricted to inner
LODs at lower scales.

### Shoreline and wake constraints

Deferred shoreline reference, consolidated from the superseded feature: a future emitter can use a smoothstep depth
window on `-fTerrainElevation`, with separate minimum/maximum shore-depth controls, and combine it with offshore
coverage using `max`. Reuse the existing noise texture first; preserve wrapped UVs, textureGrad, CPU-reduced origin/time,
prelighting diffuse blending, and foam-weighted specular reduction under the shared precision/shading contract.
This adds no shoreline emitter or controls to A/B. Revisit when offshore prototype results are recorded and the user
requests coastline surf; shoreline width, emission strength, and interaction with retained coverage then need a separate
approved design. Refraction, dedicated foam textures, and additional crest-height sources remain outside this work.

Depth-band shoreline emission is cheap and coherent with terrain but its apparent width depends on beach slope; it is an
approximation to surf, not a simulation of breaking or run-up. Current Crest supports additional foam inputs, providing
a precedent for depositing moving-object wakes into the same fading field. Wake deposition should depend on a meaningful
object-water interaction and movement, otherwise stationary ships accumulate arbitrary foam. Broad white wake trails and
broken shoreline patches are likely more legible than tiny bubbles at the stated camera distance (recommendation, not a
measured result). Keep distinct emission controls for offshore caps, shoreline bands, and wakes so one cannot force the
others to excessive coverage.

## Repository evidence and navigation

Baseline observations, with file links for durable lookup:

- [WaterDisplacement.comp](../../../Engine/Data/Shaders/Water/WaterDisplacement.comp): two Gerstner bands already accumulate horizontal-map derivatives; `(1-fA)*(1-fD)-fB*fB` is stored in normal W. Land writes zero to both outputs. No velocity or foam field exists.
- [Water.vert](../../../Engine/Data/Shaders/Water/Water.vert): fetches only displacement/normal xyz and exports initial position and a displaced visible-area UV; B must derive coverage UV from initial position. [Water.frag](../../../Engine/Data/Shaders/Water/Water.frag): existing noise uses camera-relative coordinates, wrapped UVs, and textureGrad; diffuse/specular composition has no foam.
- [TextureManager.cpp](../../../Engine/Source/Graphics/Managers/TextureManager.cpp) and [RenderTargetTextures.h](../../../Engine/Source/Graphics/Managers/RenderTargetTextures.h): wave textures follow the water grid/detail and are single-mip RGBA16F; there is no water history. At default quarter detail, LOD0 is inferred as 896x512 from the snapped fixed grid; active LOD may be smaller than allocation.
- [MainUniforms.cpp](../../../Engine/Source/Graphics/Render/MainUniforms.cpp) and [WaterWrappersBase.cpp](../../../Engine/Source/Ui/WaterWrappersBase.cpp): Low bypasses wave compute; Medium uses the low band and High both. Both default amplitude fades are 300–600 meters.
- [WaterUniforms.cpp](../../../Engine/Source/Graphics/Render/WaterUniforms.cpp) owns reduced noise origins/time; [Render.h](../../../Engine/Source/Graphics/Render/Render.h) owns retained-area basis helpers. Reuse these precision and coordinate contracts.
- Governing contracts: [shader layouts](../../../Engine/Data/Shaders/AGENTS.md), [water shaders](../../../Engine/Data/Shaders/Water/AGENTS.md), [render state](../../../Engine/Source/Graphics/Render/AGENTS.md), [immutable command recording](../../../Engine/Source/Graphics/Managers/AGENTS.md), and [GPU objects](../../../Engine/Source/Graphics/Objects/AGENTS.md). Runtime changes must preserve their CPU/GLSL alignment, quality bypass, active-grid, precision, lifetime, and client-only rules.
