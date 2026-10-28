# Lighting foundation: unfinished implementation handoff

## Snapshot identity and status

This is a work-in-progress handoff, not a verified lighting foundation. The original completion condition—an
accepted, verified commit on the reusable `lighting-foundation` ref—was not reached. The user now requests
preservation of the current changes on a new handoff branch after rebasing onto main, with no primary-branch
mutation or push. This snapshot does not authorize beginning the five candidate implementations.

The handoff ref is `lighting-foundation-handoff-20261028`; its final document commit is identified by `git log` for
this file rather than a self-referential hash. Source commits below already exist. At document authoring the
wrapper remains checked out; final ref creation accompanies the document commit.

| Field | Result |
| --- | --- |
| Wrapper branch retained | `codex/c74db6e7-ad2a-4189-ac32-93d768f4975f` |
| Rebase target main commit | `c161367603653b54e470c94cb67a3efcef36fc17` |
| Foundation WIP commit | `b5f903a6e59c917691917654d5120e13128b2154` |
| Separate experimental instrumentation commit | `49cbb2ff7e4df1454974b41260f0f6bff82ed358` |
| Pre-rebase recovery branch | `lighting-foundation-before-rebase-20261028` at `a75591d40dab3012d81edf9846179262cd0351e4` |
| Primary ref before and after rebase preparation | `c161367603653b54e470c94cb67a3efcef36fc17`; no session primary writes or push |
| Post-rebase build/data generation/runtime | Not performed; current source is unbuilt WIP |
| Rebase preservation checks | Snapshot/path accounting, range-diff, ancestry and diff checks passed |
| Project/filter checks | Sandbox client/server and relocated ModelViewer: all three pairs passed |
| Independent integration/static/document review | Not completed at document authoring; attribute later results separately |

The rebase accounted for all 206 original path intents, including 48 untracked production files and 17 deletions.
It preserved 188 non-overlap entries byte-for-byte or as deletions; 18 overlap entries were adapted, including six
ModelViewer path moves to `Projects/ModelViewer`. The experiment replayed unchanged as its own seven-file commit.

Manual conflicts occurred only in `Projects/ModelViewer/AGENTS.md` and
`Projects/ModelViewer/Source/ModelViewerApplication.cpp`. Resolution preserves upstream flight/alignment/shadow
behavior and adds Lighting registration after final transforms. Assignments to two session-deleted shader fields
were removed. Runtime GraphicsFlags draw order/recreation, packed model alignment for visible and offscreen
instances, and upstream PBR specular defaults 5.964/0.559 remain.

Upstream main changed ModelHeader from 24 to 64 bytes and raw data version from 55 to 56, with matching
exporter/loader changes. Those changes survive this rebase. Historical Output binaries and data are stale: use the
current-format /compile build and generation workflow before any future launch. No post-rebase runtime result is
claimed here.

Historical pre-rebase base was `d84c16e97cac594fcd58c9a09c9755caf75e1955`. The original P0 session-start main was
`7697f59e8187918c5a73e956ee7a721a18a6fe2d`; main moved externally during the session. The working tree was
`C:/Users/dougt/.codex/worktrees/BrokenEngine/c74db6e7-ad2a-4189-ac32-93d768f4975f`. Historical evidence below
identifies the old source/binaries, never a new post-rebase baseline.

## Original request and purpose

Build the complete common lighting foundation needed to compare five independent techniques fairly: [clustered
direct lighting](LightingPrototypeClustered.md), [DDGI](LightingPrototypeDDGI.md), [directional
cache](LightingPrototypeDirectionalCache.md), [ReGIR/ReSTIR](LightingPrototypeReGIRReSTIR.md), and [split radiance
cascades](LightingPrototypeSplitRadianceCascades.md). Implementing those five algorithms was outside this change.
They must eventually branch independently from the same verified foundation, using identical fixture/reference
inputs rather than depending on one another.

The foundation removes the legacy projected local-light deposit/spread/combine/history/preblur system and its
exclusive settings/resources. Gameplay's new physical local diffuse stays finite and exactly zero; artistic lights
do not acquire invented physical powers. Visible sprites/shields, sun/IBL, retained specular/emissive, shadows,
smoke and water retain their ordinary behavior. Removal intentionally also removes the old local specular
approximation, distinct from retained environmental specular.

The replacement supplies quantitative physical source records, resident terrain and rigid/skinned triangle/material
queries, one frozen comparison fixture, an exhaustive direct-diffuse reference, raw/image export and common
diagnostics. The original constraint preserved cell-local coordinates, deterministic simulation/CRC, RNG, Frame
layout, network/save/replay formats and packed data formats. The user-requested rebase now includes upstream raw
format version 56; historical version-55 packs are incompatible and must be regenerated. The lighting change itself
adds no packed format revision. No extra streaming or infinite-world visibility is promised: trace all currently
resident active-cell terrain and published models, including raster-culled instances, plus fixture solids; declare
omissions outside that domain.

The implementation must synchronously borrow Frames and write completed GPU records into the current framebuffer.
Resolve final GPU model/mesh/joint/instance ranges after growth. Do not retain Frame/collection pointers, worker
captures, CPU scene/geometry/matrix snapshots, duplicate pose evaluation or CPU asset blobs. Captures hold existing
GPU resources and publication generations through their final consumer rather than copying a CPU scene.

## Implemented structure and source map

Slices A–F and much of G are present in source: legacy removal; Graphics-owned Lighting/capabilities/submission
integration; terrain and model geometry producers; AS/query/lifetime integration; fixture/controls;
receiver/reference/capture/export; investigation and owning-document updates. Source presence and earlier
reviews/builds do not establish full acceptance. The following are starting points, not proof that every changed
site is correct:

- [Lighting public contract](../../../Engine/Source/Graphics/Lighting/Lighting.h), [private
publication/lifecycle](../../../Engine/Source/Graphics/Lighting/Lighting.cpp), [private
state](../../../Engine/Source/Graphics/Lighting/LightingState.h).
- [Terrain production and validation](../../../Engine/Source/Graphics/Lighting/Terrain.cpp), [model
geometry](../../../Engine/Source/Graphics/Lighting/LightingModels.cpp), [AS
allocation](../../../Engine/Source/Graphics/Lighting/LightingAccelerationStructure.cpp).
- [Fixture](../../../Engine/Source/Graphics/Lighting/LightingFixture.cpp), [comparison
controls](../../../Engine/Source/Graphics/Lighting/LightingComparison.cpp),
[capture/export](../../../Engine/Source/Graphics/Lighting/LightingCapture.cpp), [scene
diagnostics](../../../Engine/Source/Graphics/Lighting/LightingSceneDiagnostics.cpp).
- [Shared shader contract](../../../Engine/Data/Shaders/Lighting/LightingCommon.h), [original terrain
producer](../../../Engine/Data/Shaders/Terrain/Terrain.vert), [buffer allocation
policy](../../../Engine/Source/Graphics/Objects/Buffer.cpp), [terrain
residency](../../../Engine/Source/Frame/IslandTerrainResidency.cpp).

The selected resource approach uses shared pipelines and frame-indexed descriptors (the previously selected Option
1). Observed startup descriptor-pool failures were corrected; the full descriptor census remains outstanding. The
point-list witness correction now reports originalMissing=0 in the retained validation; the historical old count 42
was not exactly reproduced. Omni publication lifecycle checks passed; the corresponding raster live/dying/expired
lifecycle remains unproven.

## Frozen comparison and reference contract

Fixture RNG seed is `0x51A7`, separate from simulation RNG. Canonical little-endian
source/sample/geometry/camera/event blobs have explicit zero pads and SHA-256 identities. One stationary anchor
cell is rebased through RenderBasis; moving the camera never moves the fixture. The receiving square is 512 m, XY
[-256,256], Z=0, diffuse albedo (0.5,0.5,0.5). Panels center at (-80,0,6), (0,0,50), (80,0,100), each 16 m square,
with vertical/sloped/downward cases. Indexed solid blockers: main wall center (0,40,40), size (160,2,80); thin wall
(-80,0,40), (1,80,80); overhang (80,0,80), (80,80,2); colored wall (120,40,40), (2,80,80), albedo (0.8,0.1,0.1).

Generate 1000 ordered sources, XY uniformly mapped using frozen RNG bytes, Z cycling 6/50/100 m. Every fifth source
(indices 4,9,...) is a downward 4x4 m rectangle with U=(2,0,0), V=(0,-2,0); others are points. Each has RGB power
(1000,1000,1000) linear radiant watts and range 150 m. Prefixes 100/300/1000 share exact entries. GPU source
records are four 16-byte groups: center/range, U/type, V/pad, power/pad; signed finite coordinates, nonnegative
finite power/range, zero point edges/padding.

Point intensity is P/(4*pi); rectangular radiance is P/(pi*A), A=4*length(cross(U,V)). Reflected diffuse uses
albedo/pi exactly once, receiver cosine, inverse-square distance and segment visibility; rectangle samples
additionally use source cosine and equal A/S weights. Range window is exactly distance<150 m, with no artistic
fade. Numerical distance floor is 0.01 m; avoid singular fixture contacts. Ray origin offset and emitter-segment
shortening are each 0.001 m. Occluders are double-sided; MASK is opaque and disclosed,
BLEND/water/smoke/sprites/shields/billboards do not occlude. Water receives light. Hit textures use explicit LOD 0;
actual raster receiver material/albedo and full normals are retained.

Area samples j=0..511 are Halton (radicalInverseBase2(j+1), radicalInverseBase3(j+1)), mapped to
center+(2u-1)U+(2v-1)V. The 256/512 sets have the same first 256 entries and no temporal jitter. The moving opaque
box, visibly deforming packed skinned mesh and changing water are real rendering inputs. Choose the first resident
packed skinned scene in stable path order with a nonconstant clip; record scene CRC/clip/time, fail setup if none
exists.

All 120 frozen events use t=i/119: source 0 height 6+94t, panel tracking with recorded nonsingular offset, blocker
X traversing [-20,20], animation sampling the clip's first second. Events 0..59 interpolate eye (0,-600,900) to
(0,-4000,6000), target (0,0,6); events 60..119 pan eye/target east 900 m together to cross a cell boundary. Fixed
close/wide stills preserve those exact poses.

Reference captures are exhaustive additive linear HDR direct diffuse, with no indirect bounce, candidate estimator,
importance sampling or temporal accumulation. Capture actual displaced model/terrain/water receiver
positions/normals/albedo, then process fixed row-major 4x4-pixel compute tiles, one fenced tile in flight. Each
pixel loops all ordered sources/samples and writes its final float result once. Pump existing window/agent
servicing between fence polls. Measure a first tile and set a practical full-capture wall budget; a
watchdog/feasibility failure blocks acceptance, not sample/resolution reduction. At 1000 lights the unrejected
upper bound is about 108 billion queries at 256 samples and 214 billion at 512.

Freeze camera/basis, render/animation/water times and all captured GPU inputs/descriptors from preparation through
receivers, tiles, original frozen Main composition, raw/final export and last-consumer fences. Withhold ordinary
publication/adoption/growth/retirement during the epoch. Mutations cancel before applying or wait for release.
Preserve the preparation/Main/UI/particle/image-available dependency chain; never strand binary signals or abandon
submitted Global work. Invalidate the reference and restore zero live local diffuse before ordinary rendering
resumes. Cancellation produces no completed artifact. The final reference must never overlay later live frames.

## L1–L14 requirement and evidence matrix

Rows describe the original completion ceiling. None is waived by this WIP snapshot; partial results below are
historical unless explicitly reverified after rebase.

### L1

Required: No legacy local passes/resources/descriptors/settings/diagnostic names/project entries; surviving sprite
shaders and texture requests work. Final symbol/consumer search and GPU capture.

Evidence/open state: Removal implemented and built; final exhaustive search and capture proof remain open.

### L2

Required: Disabled/gameplay/empty/no-camera paths have finite exactly zero new local diffuse, no stale work;
preserve sprites/shield/environment/shadows/water/smoke. P0 qualitative imagery is historical, not matched frozen
imagery.

Evidence/open state: Bounded A runtime checkpoint passed 3/3; final comprehensive visual/raw receiver verification
remains open.

### L3

Required: Synchronous borrowing only, final current GPU ranges after capacity growth, complete offscreen
publication, no retained CPU snapshot/pointer and no avoidable per-frame allocation.

Evidence/open state: Implemented/audited in slices; late XYZ growth observed. Final complete range/allocation audit
and capture remain open; current experiment unreviewed.

### L4

Required: Selected-device enabled Vulkan 1.2 BDA/AS/ray-query/dependencies, formats/usages/alignment, reflected
descriptors/coherent memory, R16 sampled/linear/color-attachment/blend and raster→compute→AS dependencies. No
validation error/reset/watchdog in prescribed scenarios.

Evidence/open state: Prior capability/build and bounded clean runs exist; descriptor census and full scenario
coverage open. RenderDoc replay loss remains unresolved, distinct from clean live execution. Unsupported hardware
blocks acceptance.

### L5

Required: Real closest-hit positions/normals/material/identity and occlusion: blocked/unblocked, ridge, 1 m wall,
overhang, moving/rotating rigid, visible/offscreen skinned, zero-joint, U16/U32/empty ranges; real textured model
and rotated terrain material.

Evidence/open state: Query/producer code present, builds passed historically; complete scene/ray/material proof
remains open. No fixture-only substitute.

### L6

Required: Actual original raster equivalence, XYZ error <=0.001 m; every used raw tap exactly equal. For every
evaluated offscreen tile reraster all four separate (+2X,-2X,+2Y,-2Y) integer-texel shifts using same scratch
target, compare overlapping guarded interiors and alternative seam ownership. Compare scratch taps with Global and
XYZ with original implicit Terrain.vert. Cover rotated/overlapping/abutting placements, edges/corners, four taps,
negative tiles, cutoff equality/below, offscreen contributors/disconnected cells; no trace-domain border clamp.
Full retained topology, including rotated crop-overhang with lower-tap owner outside footprint tile range, exactly
one XYZ write and all indexed triangles in BLAS.

Evidence/open state: **FAIL/BLOCKED:** >0.001 m witness and retained shifted raw/filtered failures. Current saved
validation has once970219, duplicate0, untouched0, nonfinite0, invalidIndices0, mappingMismatches0,
originalMissing0; these coverage counters do not establish numerical equivalence or all prescribed topology
scenarios. Do not loosen tolerances or omit geometry.

### L7

Required: All 120 FixtureZero event diagnostics with fixed hashes/nested prefixes, actual XYZ/full normal,
stationary anchor/cell crossing, deforming animation/offscreen blockers, signed edges/nonnegative power/pads and
downward emitter front/back. No sim/RNG mutation; selected references include ordinary motion/skinning/water.

Evidence/open state: Fixture/control code present; exhaustive event and visibility/deformation/source-decode
evidence incomplete. No requirement to exhaustively bake 120 full references.

### L8

Required: Independently evaluated point formula within 0.1% relative OR 1e-6 absolute radiance; two-source sum
within 0.1%; blocked exactly zero except declared epsilon boundary; area energy/sidedness and albedo/pi once;
empty/overlap/range and finite output.

Evidence/open state: Estimator implemented; complete independent radiometry evidence not established. Another
invocation of the same shader is insufficient.

### L9

Required: Six 1920x1080 256-sample exports (100/300/1000 × close/wide), plus two 1000-light close/wide 512-sample
exports; raw float and final PNG. Same frozen geometry/material/exposure/environment/animation/water bytes
throughout each capture and convergence pair. RGB RMS difference/max(RMS512,1e-6)<=1%; P99 absolute
difference/max(P99 reference magnitude,1e-6)<=5%; identify shadow/high-gradient crops and tile costs.

Evidence/open state: Full eight-export/convergence acceptance not completed; resolve geometry oracle and practical
tile/capture feasibility first. Failed convergence requires diagnosis or reviewed refinement, never exposure/sample
changes disguised as success.

### L10

Required: Resize/fullscreen/minimize/restore, zero→reference→zero, cancel/invalidation, capacity growth/residency
churn, adjacent/multi-cell jump and Graphics recreation safely retain generations through last capture/export
consumer. Freeze ordinary moving/skinned/water inputs; cancel before mutation, resume without stale overlay.
Include negative origins/scratch reuse/empty/nonresident/overhang eviction-restoration-recreation.

Evidence/open state: Some lifecycle/publication/growth/normal-release evidence exists. Full hold/cancel/recreate
matrix, raster live/dying/expired and overhang generation cases remain open. Unreachable device-loss paths need
read+build, no new injection verb.

### L11

Required: Reject/reset old graphics settings with current version 19→20 absent a genuine concurrent version change;
no compatibility reader. Viewer local diffuse zero, static/skinned selection and recreation, no comparison fixture.
Preserve upstream Viewer-owned World background, seeded terrain/water, v3 presets/20 participants, outside
presets/Q comparison.

Evidence/open state: Settings/Viewer source and historical builds exist; complete old-settings restart and
supported Viewer capture/control proof remain open, especially after rebase.

### L12

Required: DataPacker Release x64 and Local shader data; Sandbox client/server Debug x64; ModelViewer Debug x64;
project/filter reconciliation and required static/style/reviews. Original lighting constraint: no pack format
change or re-bake. Rebase now requires upstream version-56 data generation, as explained above.

Evidence/open state: Selected production passed client/server/Viewer plus prior DataPacker Release. Current
seven-file experiment has never compiled; no post-rebase build proof.

### L13

Required: Replay the same complete hashed original P0 crossing generation with final binaries; complete
loop/client-server agreement and new log segments free of CRC/desync/read failures. Compare
coordinate/activation-keyed checksum streams; audit Frame/IDs/RNG/schema/CRC membership.

Evidence/open state: P0 generation/evidence retained locally; final-binary replay not established. Unqualified CRC
log lines across cells are not a valid comparison. Render animation proof belongs to L7/L9/L10.

### L14

Required: Final complete manifest, actual persistent/peak bytes, trace/reference/build/tile/preparation costs and
command operation counts; coherent five investigations; clean verified commit/ref on lighting-foundation with
wrapper branch retained and no session primary write. Include hashes, hardware/features, fixed
1920x1080/exposure/environment, counts/omissions, frame/tick/epoch/time/generations and
completion/export/cancel/resume ordering. Actual-topology N/P/A/T and overhang expansion; no omitted topology or
unsupported speedup claim.

Evidence/open state: Many diagnostics/receipts/docs present, full accounting/performance/manifest/final-clean
verified ref absent. This new WIP handoff branch satisfies only the user's preservation request.


## Terrain numerical blocker and native-capture limits

Retained exact target: placement p0, template `188901052726349361`, vertex24356. Witness Z=6.483987808227539,
compute Z=6.487268447875977, delta=0.0032806396484375 m. Shifted filtered/raw cases also retain failures. Coverage
improvements do not cure this mechanism gate. No numerical fix has been selected.

On RX9070XT, native replay of the preparation witness/compute case traced and measured a split multiply/add versus
FMA arithmetic difference in the actual words. This is not an established oracle for the original normal raster
terrain path. Preparation RDC SHA-256 is `79AE56E3393A2DF523E2B2BF06E8B0294D17F0311EFE61BD58D444E4FC85CB27`;
qualified replay GPU device30032, driver8389003. Lighting framebuffer generation10 and terrain generation9 are
distinct owners at the same publication, not an identity contradiction.

The full normal live frame captured cleanly, but RenderDoc replay internally failed `vkQueueSubmit` with deviceLost
at selected event3053. Its original bound terrain shader/pipeline and normal raster result were not established.
There is no evidence for an exclusive OOM or shader-driver cause. Do not present the 49-byte Terrain.vert source
wrapper as the shader module: the retained original compiled module is 11748 bytes, SHA-256
`7ADAA8971644BF7EB19460B752B789AA72798D97AD72110AEC57B4F92A910566`.

Historical old P50/max slot94 and current P22 had different upstream input; publication-time camera/eligible-cell
state is missing for that old case. The old count42 and p67 raw target were not recovered. Do not silently equate
them or require reproduction of an unspecified old scene as proof of current equivalence.

## Memory-policy change and bounded verification

The user selected memory Option 2: explicit compatible non-device-local, coherent mapped temporary buffers, and one
shared Buffer helper recovery path for compatible helper-managed device-local buffers. On OUT_OF_DEVICE_MEMORY it
warns, DEBUG_BREAKs, retries once in system RAM preserving other requirements, then retains final CHECK_VK failure
handling. Direct image allocations and raw ImGui allocation paths are outside this helper policy. Zero VMA
memoryTypeBits means unrestricted, not an empty allowed set. Existing broader resource lifetime/work duplication is
not solved by placement alone.

The instrumented acquisition recorded 4074 correct RAM-first allocations; code/build and eight compatibility
classes were verified. Natural fallback was not exercised: no fabricated recovery PASS and no fault injection. The
final run removed only the new RAM_REQUIREMENTS/RAM_COMPATIBILITY/ACTUAL_PLACEMENT probes; older
numeric/native/allocation TEMP instrumentation remains in source. It was not a fully uninstrumented run.

That final saved-workload run reproduced exact target records16/17 (128 words each), retained numeric failures,
observed normal XYZ demand77694696 bytes exceeding old69142332, continued ordinary rendering and ended with clean
owned release, no survivors/debug breaks/VUID/CHECK_VK. M1–M5/M7 were accepted by the caller on combined evidence;
M6 remained open. This does not mean natural OOM recovery ran or every allocation class was freshly probed after
stripping.

Historical selected-production identities before the experiment/rebase:

| Artifact | SHA-256 / result |
| --- | --- |
| Buffer.cpp | `C90CB94981CBA6F3691D2AA59E95D8C553243104924B8A4649FA0DFFEF4FF0E3` |
| Client | `7F7F968FD77F101BF4BE1CDA37C7179D8B642B8139AD9CD5DBA55FC14E1EFE80` |
| Shader.pack | `63663AFE53F786BDC2D1BFDFA930F5059444B250991796D3D64804870E0613C1` |
| Compile envelope | `Temp/AgentBuildEnvelopes/compile-20261028T181512667Z-44408.md`, Compile results: client/server/Viewer PASS; prior DataPacker Release evidence separate |

Final raw terrain production/validation times were150.84108/12.76596 ms versus old14.87916/4.03108 ms. Both had
N970219 vertices/P22 placements/A1932291 candidate visits/T28 tiles and many equal inputs/counts/operations, but
complete height-contributor/binding/content identity was unproven. Whole buffer hashes contain volatile
addresses/time; thirteen extra retained slots were not selected terrain. No causal tenfold slowdown claim is
supported. Export snapshot payload/allocated categories were1689339630/1689492352 bytes; these differ from peaks,
cumulative allocations, process bytes and heap budgets. Required complete matched costs remain unsettled.

## Current unbuilt experimental state and scope drift

An agent expanded the memory-cost investigation into a TEMP matched-cost R/G experiment. This was not an original
foundation deliverable. The original foundation called for descriptive costs, no broad timing repeats and no
speedup claim. The user's memory-policy choice does not by itself endorse every later agent-authored benchmark
detail. Apply the repository [authority order](../../../.agents/references/authority-order.md) and reassess the
necessity and scope ceiling before adopting those extra gates as obligations.

Experimental R remains selected (`kbMatchedMemoryCostR=true` in Buffer.cpp). Seven live source files contain the
experiment: Buffer.cpp; LightingCapture.cpp; Terrain.cpp; Terrain.h; LightingState.h; Lighting.cpp;
LightingAccelerationStructure.cpp (all except Buffer under Graphics/Lighting). It adds treatment selection,
allocation/CPU-duration logs, command/input/publication receipts and export fields. Source was written and
self-audited before preservation, but the experiment has **not** been compiled or executed. Independent
review/propagation results for the rebased tree were not complete at document authoring; attribute later results
separately. Current Output executables are the last verified pre-experiment, pre-rebase production binaries and do
not match the current source or upstream packed format. Never label a launch of those binaries experimental R/G or
a build of this snapshot.

Local archive `Temp/memory-matched-cost/experimental-R/manifest.json` SHA-256 is
`6CB0BAE03AC2779AC69203941ABD30CC3678210CE8E56CF7432D8FAFE495F218`. Preedit/capsule archives retain851 source
identities, nine original inputs and eight binaries/PDBs. These artifacts and any exact toggle/restoration scripts
are local provenance, not portable deliverables. Do not rerun the one-shot implementation/finalization scripts. If
continuing the experiment, first inspect the current reviewed source and rebuild via the repository skills; prior
script/manifest identities can become invalid after rebase. If a future owner chooses not to continue it, inspect
and revert only the separate experiment commit, resolving any intervening conflicts against current source. Never
restore archived whole files after rebase. User currently requires all changes preserved; a separate experiment
commit makes a future explicit removal reviewable.

The process lesson is to distinguish acceptance failures from newly invented diagnostic ambitions. Long diagnosis
and nested cost-proof work displaced the original common-fixture/reference completion while the decisive terrain
oracle remained unresolved. This observation creates no new mandatory workflow and assigns no cause to a model
identity.

## Recommended continuation

1. Inventory the actual handoff branch, rebase/conflict result, source/Output identities, active processes and
claims. No build or harness was started for rebase preservation; latest normal harness and replay cleanup were
reported released. Do not stop foreign processes. Treat pre-rebase receipts as history and retain original inputs/receipts.
2. Decide whether the unbuilt matched-cost experiment is necessary within the authorized ceiling. Preserve it in
the requested WIP snapshot; do not blindly inherit its extra benchmarking gates. The next owner must re-evaluate
that optional scope before running it. Continuing requires propagation/review/build; choosing not to continue
permits inspecting and reverting only the separate experiment commit, then rebuilding the selected production
policy. This preservation handoff does not authorize automatically starting a benchmark.
3. Resolve the actual terrain raster oracle, raw/shift consistency and full-topology feasibility. Establish what
the original normal raster executes, distinguish native arithmetic evidence from unproved normal-pipeline
association, and return measured impossibility/required decisions without weakening L6. Resolve
first-tile/full-reference practicality before scheduling exhaustive exports.
4. Complete the remaining query/material, 120-event, independent radiometry, hold/cancel/recreate, Viewer/settings,
eight-export/convergence and original P0 replay requirements. Run required repository review/build/harness skills,
and remove temporary instrumentation only with preserved evidence and final source/binary attribution.
5. Complete bytes/timings/manifest/docs, then produce the originally requested clean verified foundation commit/ref
if still authorized. Handle any existing `lighting-foundation` collision explicitly; never overwrite someone else's
ref. Record start/end primary identities and external movement. This handoff is not that completion.

## Optional local provenance

Critical requirements and limitations are written above so a fresh worktree does not need ignored Temp files to
understand the task. The following machine-local paths locate richer evidence when this worktree remains available;
they are not committed dependencies. Large RDC/native captures remain local and must not be committed as gigabyte
artifacts.

| Local path | Content |
| --- | --- |
| `Temp/eod-resume.md` | Earlier session history; superseded where later evidence differs. |
| `Temp/lighting-foundation-integration-plan.md` | Consolidated foundation authority, slices and original L1–L14 acceptance. |
| `Temp/lighting-resume-progress-20261028.md` | Earlier checkpoints and compiled terrain identity; later memory decisions supersede pending-choice text. |
| `Temp/native-arithmetic-diagnosis.md`, `Temp/native-replay-supplement.md` | Preparation arithmetic/native-word evidence and limits. |
| `Temp/lighting-normal-terrain-native-acquisition.md`, `Temp/normal-replay-loss-diagnosis.md` | Clean live acquisition, unproved original bound pipeline and replay loss. |
| `Temp/memory-policy-cost-diagnosis.md` | Matched-count timing observations and missing causal/content proof. |
| `Temp/lighting-memory-policy-final-control.md` | Probe-stripped saved-workload identity, target/growth/lifecycle and incomplete costs. |
| `Temp/memory-matched-cost/implementation-handoff.md` and selected matched-cost plan | Seven-file unbuilt experiment, schema, archives and proposed—not completed—R/G work. |

Residual authority conflict: the original feature completion goal is a verified `lighting-foundation` endpoint; the
latest user instruction requests preservation on a rebased WIP handoff branch now. Trust the latest user
instruction for this snapshot while retaining the original goal and unfulfilled criteria above. No executable Plan
is claimed by this handoff.
