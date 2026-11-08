<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-08T13:52:08.495Z","dependsOn":[]} -->
# Randomized Patch Blending for Smoke and Wind Noise

## Context

Stage 3 of three stages of randomized patch blending for tiled world textures. The user chose
randomized patch blending on a stationary triangular lattice (over two-layer superposition,
noise-masked rotated fetch and a shared domain warp) and chose to land it in stages: stage 1, the
shared mechanism plus water, landed in the same commit that added this Plan; the user rejected
stage 2, terrain, so it never landed; stage 3 (this Plan) covers smoke and wind. This record of
those choices is the authoring agent's, not the user's words.

The smoke and wind spreads read one tiled noise texture by camera-cell-local world position, so its
repeats show in plume and swirl shapes. `Smoke/SmokeSpreadCommon.h` `SmokeSpread` reads it three
times per call with `textureLod` at LOD 0: the natural-wind read at
`fSmokeWindNoiseScale * fNoiseScale * f2WindWorldPosition` (line 17, whose sample also drives the
displacement term at line 38) and the swirl X and Y reads at `f2TimeNoise + fNoiseScale *
f2WorldPosition` and its `.yx` form (lines 21-22). `SmokeSpreadOne.comp:73` passes
`fSmokeNoiseScaleOne` and `SmokeSpreadTwo.comp:81` passes `fSmokeNoiseScaleTwo`. `SmokeCurlOffset`
(lines 54-72, called only at `SmokeSpreadTwo.comp:80`) reads four potential taps at
`fSmokeCurlScale * f2WorldPosition + f2SmokeCurlTimeOffset` plus or minus a half-step `h` of
`4/2048` noise UV. `Wind/WindSpreadCommon.h:48` reads swirl noise at `f2WorldPosition * fSwirlScale
+ time`, where `fSwirlScale` mixes `fWindSwirlScaleLow/High` per texel by `WindMagnitudeFactor`.
Generated field, occupancy and ping-pong reads are not tiled inputs.

Source UVs already use camera-cell-local positions; this Plan preserves those expressions and does
not fix any pre-existing behavior they show on a camera cell change or over long times.

## Design

### Shared mechanism this stage reuses

Stage 1 added, and this Plan reuses without changing its behavior:

- Shader helpers in `ShaderFunctions.h` that build a triangle description on the stationary lattice
  (axial `floor`/`fract` split on `f.x+f.y<=1`; weights are the cubed nonnegative barycentrics
  normalized by their sum, less `GlobalLayout::fPatchWeightThreshold` capped just below the largest normalized cube, floored at 0 and renormalized), hash each
  canonical vertex identity with the descriptor's seed, which `PopulatePatchLattice` has already
  mixed with the exponent, through one pcg3d integer hash into two offsets in `[0,1)` (upper 24
  bits times `2^-24`), and blend up to three offset-only reads, skipping a read whose weight is 0.
  Strength 0 branches to the original sample expression
  with no patch construction and no extra fetch; `0<s<1` returns `mix(original, blend, s)`; `s=1`
  omits the original fetch. Stage 1's compute spawn-noise read uses `TextureLodPatched`, which
  returns `vec4` and takes a level of detail, at LOD 0.
- A common CPU helper `PopulatePatchLattice` declared in `Render.h` that, from
  `engine::gpCamera->mBasisCoordinate`, a read's representative frequency `F` and the read's seed
  index, computes in double precision the exponent `k=floor(log2(length(B.q)*F)+0.5)` clamped to
  `[-6,15]` (`F=0` gives `k=0`), and for `k<0` the floor-divided coarse camera-cell quotient
  (unsigned bits) and remainder, fills the read's descriptor and returns its `LatticeReport`; the
  per-read lattice descriptor type `PatchLatticeLayout` in `ShaderGlobalLayout.h`; the common
  lattice field `f4PatchInverseBasis`, the analytic inverse of the axial cell basis, which
  `RenderFrameGlobal` publishes separately; `LatticeReport` entries in
  `PresentationContinuitySnapshot` and the `"lattices"` probe object, keyed by read name, each
  `{"exponent","anchor","remainder"}`.

If the executing session finds this mechanism absent, it stops and returns to the user rather than
re-implementing it here.

### Smoke and wind reads

The user chose one lattice per texture read at that read's own frequency, with wind swirl taking
the larger of its Low/High scales. This stage publishes six descriptors:

| Descriptor | Read | Representative frequency F |
|---|---|---|
| `smokeNaturalWindOne`, `smokeNaturalWindTwo` | natural-wind read, per pass | `fSmokeWindNoiseScale` times that pass's noise scale |
| `smokeSwirlOne`, `smokeSwirlTwo` | swirl X and Y reads, per pass, sharing one | that pass's noise scale |
| `smokeCurl` | the four curl potential taps, sharing one | `fSmokeCurlScale` |
| `windSwirl` | wind swirl read | `max(fWindSwirlScaleLow, fWindSwirlScaleHigh)` |

The table's first column is also each descriptor's probe key. Each descriptor passes its own fixed
seed index to `PopulatePatchLattice`: 22 to 27 in the table's key order (water uses 0 to 7; see
`## Coordination`). At default settings and `length(B.q)=900` m
the exponents are natural wind 5 and 1, swirl 11 and 7, curl 2 and wind swirl 10. The coarse
`k<0` path is reached by natural wind and curl at low settings (curl at its minimum scale `0.0001`
gives `k=-3`), and natural wind reaches the `-6` clamp near its minimums.

The author recommends:

- Lattice position is the stationary `f2WorldPosition` for every read, never the time-circled
  `f2WindWorldPosition`, the `f2TimeNoise`/curl time offsets, or the wind time term; those stay in
  the source UV only, so the moving noise scrolls under fixed weights.
- `SmokeSpread` takes the pass's natural-wind and swirl descriptors as parameters beside
  `fNoiseScale`, supplied by `SmokeSpreadOne.comp` and `SmokeSpreadTwo.comp`.
- Curl: evaluate the same randomized scalar potential independently at each of the four taps,
  recomputing the triangle description and vertex identities at the tap's physical position
  `f2WorldPosition +/- (h / fSmokeCurlScale)` along the tap's axis; the wrapper minimum of curl
  scale is positive (`Ui/SmokeWrappersBase.cpp:25`). All taps share descriptor, seed and strength;
  never salt taps or hold the centre weights fixed. Keep the finite-difference normalization,
  unit-length clamp and magnitude-dependent strength unchanged.
- Wind swirl strength is `mix(Low, High, fMagnitudeFactor)` from two strengths, a Low/High pair as
  `Engine/Data/Shaders/Wind/AGENTS.md` requires of kernel constants; the per-texel swirl scale
  changes only the source UV, never the lattice density or identity. The zero-time copy path is
  unchanged.
- Four strengths, `Wrapper`s with range `[0,1]`, default 1.0 like the water strengths, continuous
  snapping, named after the
  pattern stage 1 used for its water strengths, labels per `Engine/Source/Ui/AGENTS.md`
  `## Wrapper Contracts`, declaration and slider order aligned: smoke Noise (covers natural wind,
  swirl X/Y and the displacement noise in both passes) after `Smoke Wind Noise Scale`; smoke Curl
  after `Smoke Curl Speed`; wind Swirl Low/High beside the wind swirl scale pair. No settings
  persistence or Tweaks settings version change.
- Publication, each computed through the common CPU helper from the camera basis and the wrappers'
  current values, never read back from the write-only mapped `GlobalLayout` (`RenderFrameGlobal`
  calls `RenderSmokeGlobal` and `RenderWindGlobal` before Global's own fields,
  `GlobalUniforms.cpp:574-576`): `RenderSmokeGlobal` publishes the smoke Noise strength and four
  smoke descriptors beside the noise scales, before any of its early exits; `RenderFrameGlobal`
  publishes the curl strength and descriptor beside `f2SmokeCurlTimeOffset`; `RenderWindGlobal`
  publishes the wind Low/High strengths and descriptor. Each writer also writes its
  `LatticeReport`s, all before the water publication that advances `iPublishedFrames`.
- The Graphics screen's Break Repetition toggle (`gBreakRepetition`, persisted, default on) is
  the player switch for the whole repetition-breaking feature: each writer above publishes each
  smoke and wind strength as its wrapper value when `gBreakRepetition` is on, and 0 otherwise,
  without writing the wrappers. If the cost measurement (criterion 8) suggests a cheaper subset of
  smoke and wind reads, return it to the user. Strength 0, through the toggle or a Tweaks slider,
  is the zero-strength path of criterion 2.

Cost: each read costs one fetch at zero strength, at most four at intermediate and at most three at
full; the curl early-out at zero curl strength still skips all curl reads. The blend is a convex
combination of in-range samples, so noise, displacement and curl magnitudes keep their current
ranges. If quality is inadequate, return to the user rather than adding a second algorithm.
Context, not a requirement: for cost, stage 1 blends only octave One of each water normal band;
the executing session weighs the same for reads here that act as octaves of one noise pattern
(the per-pass One/Two reads) against its cost measurement (criterion 8), and returns a restriction
to the user because it changes this Plan's read scope.

## Critical files

- `Engine/Data/Shaders/Smoke/SmokeSpreadCommon.h`
- `Engine/Data/Shaders/Wind/WindSpreadCommon.h`
- `Engine/Data/Shaders/ShaderGlobalLayout.h`
- `Engine/Source/Graphics/Render/SmokeUniforms.cpp`, `WindUniforms.cpp`, `GlobalUniforms.cpp`

## In scope

- `Smoke/SmokeSpreadCommon.h`: the natural-wind and swirl X/Y reads in `SmokeSpread` and the four
  potential taps in `SmokeCurlOffset`; `Smoke/SmokeSpreadOne.comp` and `SmokeSpreadTwo.comp`: the
  `SmokeSpread` call arguments only. `Wind/WindSpreadCommon.h`: the swirl noise read only.
- `ShaderFunctions.h`: a helper variant only if stage 1 provides none for a read here.
- `ShaderGlobalLayout.h`: the four strengths and six descriptors beside their owners' existing
  fields, CPU/GLSL scalar types and order aligned.
- `Render/SmokeUniforms.cpp` `RenderSmokeGlobal`, `Render/WindUniforms.cpp` `RenderWindGlobal`,
  `Render/GlobalUniforms.cpp` `RenderFrameGlobal` (curl only): publication and `LatticeReport`
  writes. `Render/Render.h`: the six `LatticeReport` members and the ownership comment naming each
  writer.
- `Ui/SmokeWrappersBase.h/.cpp`, `Ui/WindWrappersBase.h/.cpp`,
  `Ui/Screens/TweaksScreen/TweaksScreenSmoke.cpp` and `TweaksScreenWind.cpp`: the four strengths
  and sliders.
- `Agent/Commands/PresentationContinuityProbe.cpp`: the six keys in `"lattices"`.
  `.agents/skills/agent-harness/references/command-reference.md`: the same keys in the
  `presentation_continuity_probe` `lattices` description.
- Owning shader/Render `AGENTS.md` updates the diff requires.

## Out of scope

- Smoke and wind field, occupancy, ping-pong, lookup-table and deposit reads; water and terrain
  reads.
- Changing the stage 1 mechanism's behavior, its water reads or its water descriptors.
- Fixing pre-existing source-UV behavior on a camera cell change or over long times; changing
  pass B's extinction terms, decay, thresholds, remap or occupancy logic.
- Asset changes, histogram/LUT assets, new samplers or texture descriptors, random rotations,
  coordinate distortion, a second algorithm, tuning sliders beyond the four strengths, persistence,
  quality presets, simulation/CRC/network/save/replay formats, unit tests.

## Risk triggers and invariants

Change Workflow Tier 3: `GlobalLayout` CPU/GLSL scalar layout change, camera-cell identity
precision, and integration across independently owned shader, Render, Ui and Agent subsystems.
Writing this Plan is Tier 1. Invariants: CPU/GLSL layout aligned; full signed camera-cell identity
hashed as unsigned bits, never `D*cell` in int32 and never a cell index as float; small local
lattice coordinates; LOD 0 in compute; one randomized scalar potential at all four curl taps;
smoke and wind outputs still converge to exact zero; Smoke before Wind ordering; smoke clear,
disable, skip and refresh exits and wind pause rules unchanged; wind stays out of deterministic
state and render state outside the CRC; probe schema matches the command reference. The
command-reference edit sits in a skill package, so a fresh `reviewer` runs
`/external-skill-creator` validate mode at the cleanup step.

## Acceptance criteria

The supplied evidence records no harness setup; the executing session records its own commands.

1. Before the first edit, `/compile` and `/agent-harness` capture active smoke and wind scenes with
   camera basis and position, pause state, smoke and wind values, quality, resolution, and
   per-pass GPU timings, saved under `Temp/`.
2. Decisive: a shader read shows each smoke and wind read's zero-strength path, including every
   curl tap, is its unchanged sample expression with no patch construction or extra fetch, and
   generated-field reads are unchanged. Supporting: zero-strength captures judged visually
   equivalent to (1); wall-clock spread state cannot repeat exactly between runs.
3. Zero, half and full strength captures of smoke plumes and wind swirl, close and RTS height,
   moving slowly: full shows fewer repeated motifs, half intermediate, no visible seams,
   conspicuous triangles or new shimmer.
4. A read at this session's baseline commit shows none of the four controls; the changed client
   shows each.
5. Code proof of canonical identities including distant and extreme signed cells; probe `lattices`
   values for the six keys at reachable adjacent positive and negative cells match the code proof,
   with `smokeCurl` showing a negative exponent at Smoke Curl Scale `0.0001`; no new jump at a
   lattice edge or cell change beyond the baseline source motion recorded in (1).
6. `/glsl-review` confirms LOD 0 in the compute reads, the same scalar potential at all four curl
   taps with per-tap triangle descriptions, unchanged curl normalization and clamp, wind Low/High
   strengths blended by `WindMagnitudeFactor`, and a wind lattice independent of the per-texel
   swirl scale.
7. Smoke enable/disable and clear, gameplay pause and resume, camera cell remap and draining
   occupancy match the baseline contracts (tiles still retire to exact zero); the diff shows no
   frame, network, save or replay change.
8. Fetch counts and `query_profile` smoke and wind spread-pass GPU cost at zero, half and full
   against (1), reported without a speedup claim.
9. Shader pack, BES Debug client and server, and ModelViewer Debug x64 (it compiles the smoke and
   wind uniforms, wrappers and Tweaks sources) build; the probe documentation matches the response.
10. A call-site inventory of texture reads under `Engine/Data/Shaders` on the final diff shows the
    seven smoke noise reads and the wind swirl read routed through the shared helpers and every
    other read unchanged against this session's baseline commit.

## Coordination

Stage 1 landed with this Plan and has no Plan file. Seed indices passed to
`PopulatePatchLattice` are fixed: water 0 to 7 (`PopulateWaterReducedUv` order), smoke and wind 22
to 27.

## Notes

The water stage's lattice frequencies, exponent clamp and coarse `k<0` path are recorded in the
code it landed; consult it rather than re-deriving them.
