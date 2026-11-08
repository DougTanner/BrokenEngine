<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-08T13:52:04.099Z","dependsOn":[]} -->
# Randomized Patch Blending for Terrain Materials

## Context

Stage 2 of three stages of randomized patch blending for tiled world textures. The user chose
randomized patch blending on a stationary triangular lattice (over two-layer superposition,
noise-masked rotated fetch and a shared domain warp) and chose to land it in stages: stage 1, the
shared mechanism plus water, landed in the same commit that added this Plan; stage 2 (this Plan)
covers terrain; stage 3 (`Documents/Plans/Engine/RandomizedPatchBlendingSmokeAndWind.md`) covers
smoke and wind. This record of those choices is the authoring agent's, not the user's words.

Current terrain material reads repeat visibly at RTS height. `Terrain/Terrain.frag` reads rock color
(`rockSampler`, line 99) and sand color (`sandSampler`, line 114) with `textureGrad` at
`size * f3InPosition.xy`, and six detail normals through `SampleNormal` (`ShaderFunctions.h:73`,
the only caller being `Terrain.frag:94,106-108`): three rock normals at `f3InPosition.xy +
f3InPosition.z`, sand normal One at `.xy` and sand normals Two and Three at `.yx` with swizzled
gradients. Gradients are taken before the material branches (`Terrain.frag:85-88`). The sizes come
from `gTerrainRockSize`, `gTerrainRockNormalsSize{One,Two,Three}`, `gTerrainBeachSandSize` and
`gTerrainBeachNormalsSize{One,Two,Three}` (`Ui/TerrainWrappersBase.cpp:18-30`), published by
`PopulateTerrainParameters` (`Render/GlobalUniforms.cpp:513-536`).

Observation, not scope: `f3InPosition.xy` is camera-cell-local (`VisibleAreaToWorld`,
`Terrain.frag:56`) and the terrain material source UVs have no CPU-reduced origin, unlike water
(`Engine/Source/Graphics/Render/AGENTS.md` `## Precision and History`). A camera cell change shifts
the position by the cell-centre offset, whose product with a material size is generally not a whole
number of tiles, so the source pattern may jump on a camera cell change. This is unverified at
runtime; this Plan preserves the source expressions and must not add a jump of its own.

## Design

### Shared mechanism this stage reuses

Stage 1 added, and this Plan reuses without changing its behavior:

- Shader helpers in `ShaderFunctions.h` that build a triangle description on the stationary lattice
  (axial `floor`/`fract` split on `f.x+f.y<=1`; weights are the cubed nonnegative barycentrics
  normalized by their sum, less `GlobalLayout::fPatchWeightThreshold` capped just below the largest normalized cube, floored at 0 and renormalized), hash each
  canonical vertex identity with the descriptor's seed, which `PopulatePatchLattice` has already
  mixed with the exponent, through one pcg3d integer hash into two offsets in `[0,1)` (upper 24
  bits times `2^-24`), and blend up to three offset-only reads with the caller's original
  gradients, skipping a read whose weight is 0. Strength 0
  branches to the original sample expression with no patch construction and no extra fetch;
  `0<s<1` returns `mix(original, blend, s)`; `s=1` omits the original fetch. Normals blend decoded
  vectors, never encoded RG.
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

### Terrain reads

The user chose one lattice per texture read, at that read's own frequency, with correlated channels
of one read sharing it. Terrain therefore publishes eight descriptors, each `F` the read's published
size: rock color (`fTerrainRockSize`), rock normals One/Two/Three
(`fTerrainRockNormalsSize{One,Two,Three}`), sand color (`fTerrainBeachSandSize`) and sand normals
One/Two/Three (`fTerrainBeachNormalsSize{One,Two,Three}`). Each descriptor passes its own fixed seed
index to `PopulatePatchLattice`: 14 to 21 in the order just listed (water uses 0 to 7; see
`## Coordination`).

The author recommends:

- Lattice position for every terrain read is the physical `f3InPosition.xy`. Source UVs keep their
  current expressions: rock normals keep `xy + z`, sand normals Two and Three keep `.yx` with
  swizzled gradients; these are source-UV choices, not different world locations.
- Color reads blend RGB with one set of weights and offsets per read.
- `SampleNormal` gains only the parameters the blended path needs (lattice position, descriptor,
  strength) and keeps its standard BC5 decode (`2x-1`, Z reconstructed per sample) inside each of
  the three samples. The callers keep their sums, the unequal sand weights `2.0/0.5/1.0`, the sand
  `z=0`, the rock `normalize`, the mask thresholds and the `f*NormalsBlend > 0` height gating.
  The unused `fSpeed`/`f2Offset` parameters stay.
- Reuse the stage 1 helper variant matching each read's form; add a variant to `ShaderFunctions.h`
  only for a form stage 1 lacks (expected: the standard-decode normal path).
- Two strengths, Sand and Rock: `Wrapper`s with range `[0,1]`, default 1.0 like the water strengths,
  continuous snapping, in
  `TerrainWrappersBase`, named after the pattern stage 1 used for its water strengths, labels per
  the wrapper-label rule in `Engine/Source/Ui/AGENTS.md` `## Wrapper Contracts`, declaration and
  slider order aligned (`TweaksScreen/AGENTS.md` line 12): Sand after the Beach group, Rock after
  the Rock group. One control covers its material's color and its three detail normals. No settings
  persistence or Tweaks settings version change.
- `PopulateTerrainParameters` publishes both strengths and the eight descriptors, computing each
  descriptor through the common CPU helper from the wrapper's current size, and writes the eight
  `LatticeReport`s there, which precedes the water publication that advances `iPublishedFrames`.
  Probe keys: `rockColor`, `rockNormalOne`, `rockNormalTwo`, `rockNormalThree`, `sandColor`,
  `sandNormalOne`, `sandNormalTwo`, `sandNormalThree`.
- The Graphics screen's Break Repetition toggle (`gBreakRepetition`, persisted, default on) is
  the player switch for the whole repetition-breaking feature: `PopulateTerrainParameters`
  publishes each terrain strength as its wrapper value when `gBreakRepetition` is on, and 0
  otherwise, without writing the wrappers. If the cost measurement (criterion 8) suggests a
  cheaper subset of terrain reads, return it to the user. Strength 0, through the toggle or a
  Tweaks slider, is the zero-strength path of criterion 2.

At the default sizes and `length(B.q)=900` m (`kfCellApothem` 450, `GridCoord.h:45`) the exponents
are rock color 8, rock normals 5/5/8, sand color 6, sand normals 6/2/6; the wrapper ranges keep
every terrain exponent within `[0,9]`, so terrain never takes the `k<0` path.

Cost: each enabled read costs one fetch at zero strength, at most four at intermediate and at most
three at full; masked-out materials and height-gated normals still skip their reads. No histogram
or contrast guarantee; if quality is inadequate, return to the user rather than adding a second
algorithm. Context, not a requirement: for cost, stage 1 blends only octave One of each water
normal band; the executing session weighs the same for the three-octave rock and sand detail
normals against its cost measurement (criterion 8), and returns a restriction to the user because
it changes this Plan's eight-read scope.

## Critical files

- `Engine/Data/Shaders/Terrain/Terrain.frag`
- `Engine/Data/Shaders/ShaderFunctions.h`
- `Engine/Data/Shaders/ShaderGlobalLayout.h`
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`

## In scope

- `Terrain/Terrain.frag`: the rock and sand color `textureGrad` reads and the six `SampleNormal`
  calls, preserving masks, height gating, swizzles and weights.
- `ShaderFunctions.h`: `SampleNormal`'s interface and body for the blended path, and any helper
  variant only terrain needs.
- `ShaderGlobalLayout.h`: the two terrain strengths and eight terrain descriptors beside the
  existing terrain fields, CPU/GLSL scalar types and order aligned.
- `Render/GlobalUniforms.cpp` `PopulateTerrainParameters`: strength and descriptor publication and
  the terrain `LatticeReport` writes. `Render/Render.h`: the eight terrain `LatticeReport` members
  and the ownership comment naming `GlobalUniforms.cpp` as their writer.
- `Ui/TerrainWrappersBase.h/.cpp` and `Ui/Screens/TweaksScreen/TweaksScreenTerrain.cpp`: the two
  strengths and sliders.
- `Agent/Commands/PresentationContinuityProbe.cpp`: the eight terrain keys in `"lattices"`.
  `.agents/skills/agent-harness/references/command-reference.md`: the same keys in the
  `presentation_continuity_probe` `lattices` description.
- Owning shader/Render `AGENTS.md` updates the diff requires.

## Out of scope

- Island baked color, normal, ambient-occlusion and mask textures, elevation, lighting, shadow and
  smoke reads in `Terrain.frag`; water, smoke and wind reads; stage 3's controls and descriptors.
- Changing the stage 1 mechanism's behavior, its water reads or its water descriptors.
- Adding a reduced origin or any other fix for the observed terrain source-UV behavior on a camera
  cell change; moving derivative sites; removing `SampleNormal`'s unused parameters.
- Asset changes, histogram/LUT assets, new samplers or texture descriptors, random rotations,
  coordinate distortion, a second algorithm, tuning sliders beyond the two strengths, persistence,
  quality presets, simulation/CRC/network/save/replay formats, unit tests.

## Risk triggers and invariants

Change Workflow Tier 3: `GlobalLayout` CPU/GLSL scalar layout change, camera-cell identity
precision, and integration across independently owned shader, Render, Ui and Agent subsystems.
Writing this Plan is Tier 1. Invariants: CPU/GLSL layout aligned; full signed camera-cell identity
hashed as unsigned bits, never `D*cell` in int32 and never a cell index as float; small local
lattice coordinates; gradients at their current pre-branch sites; standard terrain decode,
swizzles and octave weights; render state stays outside the CRC; probe schema matches the
command reference. The command-reference edit sits in a skill package, so a fresh `reviewer` runs
`/external-skill-creator` validate mode at the cleanup step.

## Acceptance criteria

The supplied evidence records no harness setup; the executing session records its own commands.

1. Before the first edit, `/compile` and `/agent-harness` capture fixed-input sand and rock scenes
   (close and RTS height) with camera basis and position, pause and sun angle, terrain values,
   quality, resolution, and per-pass GPU timings, saved under `Temp/`.
2. Decisive: a shader read shows each terrain read's zero-strength path is its unchanged sample and
   decode expression with no patch construction or extra fetch. Supporting: zero-strength captures
   judged visually equivalent to (1).
3. Zero, half and full strength captures per material, close and RTS height, moving slowly: full
   shows fewer repeated motifs, half intermediate, no visible seams, conspicuous triangles, severe
   contrast loss or new shimmer.
4. A read at this session's baseline commit shows neither terrain control; the changed client shows
   both.
5. Code proof of canonical identities including distant and extreme signed cells; probe `lattices`
   values for the eight terrain keys at reachable adjacent positive and negative cells match the
   code proof (exponents as listed above at default sizes); no new jump at a lattice edge or cell
   change beyond the baseline source behavior recorded in (1).
6. `/glsl-review` confirms pre-branch gradients, standard decode per sample, swizzles and weights
   preserved, shared RGB offsets and weights per color read; all-zero normal weights stay finite.
7. The diff shows no frame, network, save or replay change.
8. Fetch counts and `query_profile` terrain-pass GPU cost at zero, half and full against (1),
   reported without a speedup claim.
9. Shader pack, BES Debug client and server, and ModelViewer Debug x64 (it compiles the terrain
   wrappers, Tweaks and `GlobalUniforms.cpp`) build; the probe documentation matches the response.
10. A call-site inventory of texture reads under `Engine/Data/Shaders` on the final diff shows the
    eight terrain reads routed through the shared helpers and every other read unchanged against
    this session's baseline commit.

## Coordination

Stage 1 landed with this Plan and has no Plan file. Reciprocal
coordination with `Documents/Plans/Engine/RandomizedPatchBlendingSmokeAndWind.md`: both extend
`ShaderGlobalLayout.h`, `PresentationContinuitySnapshot` and its ownership comment, the probe's
`"lattices"` object, its command-reference description and possibly `ShaderFunctions.h`; whichever
lands second refreshes those regions and reuses any helper variant the first added. Seed indices
passed to `PopulatePatchLattice` are fixed regardless of landing order: water 0 to 7
(`PopulateWaterReducedUv` order), terrain 14 to 21, smoke and wind 22 to 27. Each Plan keeps its own
acceptance.

## Notes

The water stage's lattice frequencies, exponent clamp and coarse `k<0` path are recorded in the
code it landed; consult it rather than re-deriving them.
