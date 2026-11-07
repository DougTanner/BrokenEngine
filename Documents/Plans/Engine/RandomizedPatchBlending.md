<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T23:33:08.488Z","dependsOn":[]} -->
# Randomized Patch Blending for Tiled Visual Textures

## Context

The user selected randomized patch blending and requested an actionable implementation Plan, covering tiled
game textures with strength sliders in the existing effects. This document proposes the implementation;
author-selected formulas, grouping and defaults below are design recommendations, not quotations or
implementation approval. Plan authorship and landing do not authorize implementing it.

Use existing texture assets and three offset-only samples at triangular-lattice vertices. This is a
deliberately simplified adaptation of [Practical Real-Time Hex-Tiling, Mikkelsen (2022), sections
1–2](https://jcgt.org/published/0011/03/05/paper-lowres.pdf): that paper supports triangular sampling and
normalized exponentiated weights without preprocessing. Its additional sample-dependent metrics and histogram
treatment are not included. No equivalence to an unavailable 2026 GGG algorithm is asserted.

Preparation baseline: `9149a99e37a4ec40a590f8b126e8144011029758`. No duplicate executable Plan was found by
searching `Documents/Plans` for de-tiling, randomized patch blending or stochastic textures. Runtime
implementation is Tier 3: shared CPU/GLSL scalar-layout fields (`Engine/Data/Shaders/ShaderGlobalLayout.h:6`),
camera precision (`Engine/Source/Graphics/Render/WaterUniforms.cpp:120`) and UI/render/shader integration.
Writing this Plan is Tier 1.

## Design

### Sampling and strength

Add shared patch-description and Grad/Lod sampling helpers to `ShaderFunctions.h`, reusing `ShaderRandom.h`
for integer seed mixing. Helpers receive source UV, explicit gradients or LOD, stationary lattice
coordinates/identity, a material seed and strength. They must not infer derivatives in a branch.

For axial lattice coordinates `a`, take `i=floor(a)` and `f=fract(a)`. When `f.x+f.y <= 1`, use vertices `i`,
`i+(1,0)`, `i+(0,1)` and barycentrics `(1-f.x-f.y,f.x,f.y)`. Otherwise use `i+(1,1)`, `i+(0,1)`, `i+(1,0)` and
`(f.x+f.y-1,1-f.x,1-f.y)`. Normalize the cubes of the nonnegative barycentrics: `w=b*b*b / sum(b*b*b)`. The
denominator is positive throughout a triangle. Cubing is an internal fixed choice; expose no exponent control.

Hash each canonical vertex identity and the material seed into two independent offsets in `[0,1)`, using the
existing seed mixer and the upper 24 random bits times `2^-24` to avoid rounding an offset to 1. Sample
`sourceUV+offset` with the source's current sampler/address behavior; water's repeat-normal/noise path applies
its existing `fract` after adding the offset, while mirrored foam does not. No random rotations, scaling,
distortion or nearest-patch selection. Blend corresponding channels with the same weights and offsets.

Strength `s=0` branches directly to the unchanged original sampling and decoding expression, before building a
patch description or making extra fetches. At `0<s<1`, return `mix(original, patchBlend, s)`; at `s=1`, omit
the original fetch. Scalar/vector reads blend their sampled values; normals blend decoded vectors and use the
existing composition/normalization owner afterward. Water retains its sign-inverted BC5 decoding and band
rotation; terrain retains its standard BC5 decoding, swizzles and unequal octave weights. Do not average
encoded RG and reconstruct Z afterward. Foam RGB retains its current post-sample gamma decoding, and red
breakup/spawn remain linear.

Each original read costs one fetch at zero, four at intermediate strength and three at full strength; inactive
existing bands/effects still skip their reads. This is an accepted cost of the proposed design, not a
performance optimization. No histogram or contrast preservation guarantee is made. Review visible seams,
ghosting and contrast before accepting the implementation; do not add histogram assets or a second algorithm
if quality is inadequate—return to the Plan decision.

### Stable stationary lattice

Keep source UV and the lattice separate. Water source UVs continue using the existing rotated modulo-ten
origins/time and modulo-two foam origins (`WaterUniforms.cpp:49–175`, `WaterNormalSampling.h:38`,
`Water.frag:90`). Never hash those reduced values, animated phases, screen coordinates, retained-area UVs or
floating absolute world positions. Thus their integral repeat/mirror wrap shifts leave all patch reads
unchanged while the lattice stays stationary. A moving normal/noise pattern scrolls under fixed weights; its
advected shape can change during blending. Continuity is required, exact transport of its original shape is
not.

Derive the lattice from the existing axial cell basis `B=[kCellDirections.f2BasisQ,kCellDirections.f2BasisR]`
(`Common/Math/MathUtils.h:260`, `Engine/Source/Frame/GridCoord.h:45`). Compute its analytic 2x2 inverse on the
CPU, from the same stored float basis used for rebasing, in double precision; publish float finished
transforms. No GLSL `inverse`, float64 feature, absolute GPU position or new phase accumulator is needed.

Choose one dyadic density per sampling group: `k=floor(log2(length(B.q)*F)+0.5)` and `D=2^k`, with `F` from
the table below. `F=0` uses `k=0`; this is needed by the existing zero color-frequency setting. This keeps
patches approximately one representative source tile across, instead of using one world size that would either
preserve many fine repeats inside a patch or subdivide a low-frequency texture into unrelated small blotches.
Densities and transforms change only when their source-frequency settings change; changing these settings may
change patch identity, just as changing a texture may. No new patch-size setting or temporal tracking is
added.

| Lattice group | Representative frequency F, using already resolved/published values |
|---|---|
| Each water normal band | That band's base size after Water Scale; its three octave calls share the lattice |
| Water color noise | Color frequency times max of the two nonnegative octave multipliers |
| Foam color / breakup / spawn, separately | Each published inverse tile size |
| Sand / rock, separately | The material color's size; its three detail normals share its lattice |
| Smoke natural/swirl noise, shared by both spread passes | max of the two spread noise scales times max(1, natural-wind noise scale) |
| Smoke curl | Published curl scale |
| Wind swirl | max of Low/High swirl scales, independent of per-texel magnitude |

Do not form `D*cellIndex` in a signed 32-bit integer or convert a cell index to float. Publish a small
descriptor per group: the precomputed local-position-to-lattice transform, the anchor's unsigned integer
identity, fractional coarse-cell remainder where needed, exponent and fixed group seed. Reuse the current
camera basis for every effect, including held smoke areas expressed in that basis.

For `k>=0`, shader-local coordinates are `D*B^-1*p`, where `p` is the current camera-cell position. For a
lattice vertex with small local integer coordinates `n`, canonicalize each component using mathematical floor
division: `cell=uint(cameraCell)+uint(floorDiv(n,2^k))`, `sub=n-floorDiv(n,2^k)*2^k`. Hash the full cell bits
and nonnegative subcell indices plus the group seed and exponent. Unsigned addition provides specified modulo
arithmetic; no signed overflow or truncation of the high cell bits occurs. For `k<0`, let `M=2^-k`; CPU
floor-divides each signed camera cell index by M, publishes the coarse quotient as unsigned bits and
remainder/M as float. Shader coordinates are `B^-1*p/M + remainder/M`, and vertex identity is the unsigned
coarse quotient plus the small local vertex. Hash both coordinate components; hashing only the low word of
`GridCoord::ToKey` would lose Q (`GridCoord.h:92`). Hash collisions are ordinary random reuse, not coordinate
truncation.

Use 64-bit CPU integers for quotient/remainder preparation and 32-bit unsigned shader hashing. Use the
authored internal density range `k in [-6,15]`: clamp the nearest exponent to this range before shifting, with
zero frequency selecting k=0. The upper end covers the listed effect-frequency maxima; the lower end limits a
patch to 64 cell spacings even when the color-frequency slider approaches zero. These are algorithm scale
limits, not a source-UV clamp or a guarantee inferred from a positive wrapper minimum. Floor division must
handle negative n/cells. Water uses its undisplaced camera-cell position `f2InInitialPosition+f2WaterOrigin`
for normal/foam lattices and its displaced position for color noise; no world-sized float is introduced. Other
shaders use their current camera-cell positions. Keep patch coordinates in the same physical orientation for
sand's `.yx` and rock's elevation-adjusted source UVs; these are source-UV choices, not different world
locations.

Across a cell rebase by integer axial delta, local lattice coordinates shift by `-D*delta` and the canonical
integer identity adds the corresponding delta. The represented vertices and barycentrics therefore agree,
within existing local float precision, for positive/negative cells and distant signed cell indices. Adjacent
triangles share the same two vertex samples on an edge, with the third weight zero. Cubed weights remove the
entering/exiting sample's first-order contribution; they do not promise globally continuous derivatives of the
whole field, since normalized edge weights can have different transverse derivatives on the two triangles.
Explicit source gradients avoid wrap-derived mip spikes; they do not filter the spatial weight variation
itself. Visual acceptance must check both visible triangle boundaries and distance shimmer.

For curl, evaluate the same scalar randomized field independently at all four existing potential taps,
including recomputing weights and vertex IDs at the tap's physical position. A UV offset `(+h,0)` corresponds
to physical offset `(+h/fSmokeCurlScale,0)` and similarly for Y; curl scale's existing wrapper minimum is
positive (`SmokeWrappersBase.cpp:25`). All taps share seed and algorithm; never independently salt taps or
hold center weights fixed. Preserve the current finite-difference normalization/clamp and magnitude-dependent
curl amount. Wind's per-texel scale affects only source UV, not its lattice density or identity.

Source UVs of terrain/smoke/wind already use camera-cell positions (`Terrain.frag:99`,
`SmokeSpreadCommon.h:17`, `WindSpreadCommon.h:48`). This Plan preserves those original expressions and does
not claim to fix their pre-existing rebasing or long-time precision behavior. Added patch identities must
remain stable, and no additional discontinuity beyond the source path is acceptable. Separate baseline source
motion from a changed lattice with code/continuity evidence; a new defect in the lattice is an in-scope
failure, not an excuse to widen into unrelated UV fixes.

### Controls and publication

Add ordinary runtime `Wrapper` strengths with range `[0,1]`, default `0`, normal continuous slider snapping
and distinct labels. The proposed defaults preserve existing appearance. Put each in the existing effect, with
matching declaration/UI order (`Engine/Source/Ui/Screens/TweaksScreen/AGENTS.md:16`): water normal-band
One/Two/Three; water Color Noise; foam Color/Breakup/Spawn Noise; terrain Sand and Rock; smoke Noise and Curl;
wind Swirl Low/High. One sand/rock control covers that material's color and all detail normals. One smoke
Noise control covers natural wind, swirl X/Y and displacement noise in both passes. Blend Wind Low/High with
the existing `WindMagnitudeFactor`, as required by `Engine/Data/Shaders/Wind/AGENTS.md:14`. No wrapper-value
persistence or Tweaks settings version change is needed.

Publish strengths and lattice descriptors through the shared `GlobalLayout`, with CPU/GLSL scalar types/order
aligned. Global owns the common cell basis/identity and terrain/curl fields; Water owns normal/color/foam
fields; Smoke and Wind own their groups. Publish every descriptor before its dispatch can read it. Mirror
newly guaranteed published lattice values into the existing `PresentationContinuitySnapshot` and existing
read-only probe serialization only; add no harness command. Give correlated channels of a read the same
descriptor. The current sand/rock color and normal sources are independently scaled, not an authored
UV-correlated PBR set; share their material lattice/hash offsets without inventing asset correlation or
replacing those scales.

## In scope

- `Engine/Data/Shaders/ShaderFunctions.h`: shared triangle description, canonical vertex hash, Grad/Lod
  scalar/vector helpers and normal-sampling integration; include the existing `ShaderRandom.h` once in the
  appropriate shared include chain. No new shader file is required.
- `Engine/Data/Shaders/Water/WaterNormalSampling.h`: all nine `SAMPLE_NORMAL_PRECISE` reads, including reuse
  by extra skybox sampling. `Water/Water.frag`: both color-noise layers and stationary foam RGB/breakup reads.
  `Water/WaterFoam.comp`: only `NativeSource`'s spawn-noise read.
- `Engine/Data/Shaders/Terrain/Terrain.frag`: rock/sand color reads and all six detail normal calls,
  preserving masks/height gating; update the shared `SampleNormal` interface only as required by these callers
  and propagate every caller.
- `Engine/Data/Shaders/Smoke/SmokeSpreadCommon.h`: natural wind, swirl X/Y noise and the four curl potential
  taps; generated field reads are unchanged. `Engine/Data/Shaders/Wind/WindSpreadCommon.h`: swirl noise only.
- `Engine/Data/Shaders/ShaderGlobalLayout.h`: strengths and scalar-layout lattice descriptors.
  `Engine/Source/Graphics/Render/GlobalUniforms.cpp`: common descriptor preparation/publication, terrain
  controls and curl descriptor. `WaterUniforms.cpp`, `SmokeUniforms.cpp`, `WindUniforms.cpp`: owner
  control/descriptor publication, leaving existing source origin/time reductions intact. `Render.h`: minimal
  common CPU helper declarations/types and continuity snapshot additions.
- `Engine/Source/Ui/{Water,Terrain,Smoke,Wind}WrappersBase.h/.cpp` and corresponding
  `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen{Water,Terrain,Smoke,Wind}.cpp`: controls listed above.
  `Engine/Source/Agent` existing presentation-continuity probe response builder: serialize only newly
  published guarantee fields.
- Owning shader/Render AGENTS.md contract updates required by the actual diff. Project/filter reconciliation
  only if implementation introduces a file, which this design does not require.

## Out of scope

- UV-authored model materials, cookies, sprites, billboards, skyboxes and atlases; repeat address mode alone
  does not make them tiled world materials. Island baked color/normal/AO/masks,
  elevation/lighting/shadow/history/displacement, smoke/wind field targets and lookup tables are authored
  images or data, not randomized visual inputs.
- Asset changes, Wang-tile preprocessing, histogram/LUT assets, new samplers/descriptors for textures, random
  rotations or coordinate distortion; a second algorithm; algorithm tuning sliders; persistent settings;
  quality presets; simulation, CRC, network/save/replay formats; collection changes; unit tests; unrelated
  tuning or baseline UV fixes.

## Implementation order

1. Before the first edit, use `/compile` and `/agent-harness` to capture fixed-input baseline scenes covering
   ocean normals/color, stationary foam with each selected material, spawn-noise emission, sand/rock and
   active smoke/wind. Record camera basis/position, pause/time, effect values, quality, resolution and source
   texture selections. Capture cell/reduction/phase transitions needed below and baseline GPU timings.
   Preserve reusable scenario instructions and artifacts under Temp.
2. Implement the CPU descriptor representation, analytic basis transform and negative-coordinate
   quotient/remainder preparation with matching `GlobalLayout` fields. Use the planned exponent bounds and
   verify them against the existing source wrappers; inspect extreme signed cell identities and local vertex
   arithmetic at k=15 algebraically without adding unit tests. Populate every render owner and continuity
   snapshot/probe field before wiring shaders.
3. Implement shared patch/hash/Grad/Lod and decoded-normal paths, with exact zero and full-strength branches.
   Wire water, terrain, smoke and wind at the scoped owners. Existing derivatives are captured before
   divergent branches, masks and discarded fragments; each offset sample receives the original unwrapped
   gradients. Compute retains explicit mip 0. Source sampler choices, especially foam mirroring, stay intact.
4. Add the listed wrappers/sliders and owner publication. Propagate affected signatures and shared include
   dependencies using `/update-affected-code`; synchronize contract documentation with `/update-claude-docs`.
   Complete triggered C++/GLSL, style/comment, over-engineering, coherence and adversarial reviews under the
   Change Workflow. Resolve acceptance failures within scope before marking complete.
5. Build shader assets and BES Debug client/server through `/compile` in its documented runtime-data mode;
   then perform runtime verification through `/agent-harness`. Use existing controls, captures, profile query
   and continuity probe; add no verification-only runtime system. Record per-criterion evidence and observed
   cost; obtain fresh acceptance verification before completion.

## Acceptance and evidence

| Criterion | Required evidence |
|---|---|
| All tiled visual consumers covered | Call-site inventory/diff covering every read in In scope; explicit unchanged-source exclusions above. Runtime controls for each scoped effect are absent from the baseline and present afterward. |
| Zero preserves baseline | Fixed-input zero-strength captures match the before-the-first-edit scenes at the same time/state; shader read confirms unchanged sample/decode expressions, no patch construction/fetches, and unchanged generated-field handling. Do not use screenshots alone to prove fetch count. |
| Adjustable seamless breakup | At zero, half and full strength, compare every material/effect with the recorded baseline; full strength shows less obvious repeated source motifs, half strength gives an intermediate result, and patch boundaries do not create visible seams, conspicuous triangles or severe contrast washout. Capture close and RTS-height views while moving slowly for shimmer. These are visual acceptance judgments, not histogram guarantees. |
| Coordinates and animation | Code proof of canonical identities plus published-descriptor/probe evidence at positive/negative adjacent camera cells and distant signed coordinates; paused-time captures keep the lattice anchored. Exercise water normal rotations away from axis multiples, all bands/extra skybox reads, Water Scale extremes, color multipliers including zero, old origin reductions and accumulated phase wraps. Animated source detail may morph under fixed weights but must not newly jump at a lattice edge, cell transition or water wrap. Compare existing terrain/smoke/wind source motion against baseline, not an invented absolute-UV guarantee. |
| Sampling/channel invariants | GLSL review confirms pre-branch explicit source gradients, fixed compute LOD, shared channel offsets/weights, correct normal decode/rotation and shared continuous curl potential. Runtime all-zero normal weights remain finite; source opacity/material masks and foam history remain independent of material controls. |
| Lifecycle and visual-state isolation | Exercise water Low/Medium/High, foam disable/enable and rewind, smoke/wind pause/resume, camera remap and draining occupancy; compare with baseline contracts. CPU/diff evidence confirms no deterministic frame/network/save/replay changes. |
| Cost disclosed | Record fetch counts above and GPU observations for the same zero/half/full settings and scene coverage. Follow `/agent-harness` profile cohort caveats and Immediate/fixed-clock policy; report measured per-pass cost without claiming speedup or inventing a frame-time threshold. A usability-blocking slowdown is a failed visual feature acceptance requiring a Plan decision, not an invitation to add an unplanned renderer quality system. |
| Integration | Successful shader pack and BES Debug client/server build results; all applicable correctness review findings resolved or explicitly classified as pre-existing/out of scope with evidence. |

## Execution card

### What does this plan do?

Add randomized patch blending to the tiled visual inputs in water, terrain, smoke and wind. Strength controls
live in the existing effects, with zero preserving current appearance and the original sample path. A
stationary axial lattice supplies stable random offsets while existing source animation and addressing remain
owned by their effects.

### Why this is good for the codebase

The requested repetition breakup uses existing textures and one shared shader mechanism. CPU preparation
preserves integer cell identity and provides finished transforms, while the current render owners keep their
controls, phase math and lifecycle contracts.

- Goal: Adjustable repetition breakup for every tiled visual consumer listed in scope.
- Out of scope: The Out of scope section, including authored finite images/data and unrelated source-UV fixes.
- Tier trigger: Future implementation Tier 3: shared CPU/GLSL layout, camera precision and cross-subsystem
  integration; Plan-only authorship Tier 1.
- Interfaces and invariants: Shared scalar layout; full signed-cell identity in unsigned hashing; small local
  coordinates; original water reduction/address periods; explicit gradients/LOD; decoded normals and channel
  correlation; same scalar potential for curl taps; client visual state outside CRC; existing
  pause/remap/occupancy/history rules.
- Acceptance checks: Execute every Acceptance and evidence row, including baseline captures and
  transition/profile runs before the first edit, then zero/half/full-strength comparisons, full inventory,
  coordinate proof/probe evidence, source addressing and finite-value checks, lifecycle cases, recorded cost
  and required builds. New controls and repetition breakup must be observable where the baseline has neither.
- Roles: Main owns future approval and Change Workflow orchestration; prepare implementer supplies the draft;
  plan simplicity/audit and Tier-3 grill review the concrete design before execution; implementer performs
  approved work and assumption audit; affected-code/docs workers propagate contracts; C++ and GLSL reviewers
  plus conditional cleanup/coherence/adversarial reviewers resolve correctness; compile and harness operators
  supply evidence; fresh acceptance reviewer settles completion. No implementation runs during Plan
  authorship.
