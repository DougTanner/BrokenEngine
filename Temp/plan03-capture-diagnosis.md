# Plan03 capture diagnosis

## Result

BLOCKED for exact provenance of the 87-to-89 publication transition. Proven setup defect: Point0.ps1
requests capture while subscribed cell [0,1] remains unconfirmed and excluded from terrain
publication. That cell becomes confirmed/rendered later despite pause. Independently, asynchronous
mesh restoration can add terrain instances while paused. Existing records do not timestamp the cell
transition or identify restored templates, so they cannot distinguish these two contributors to the
exact failing frame. No source defect or source remedy is proven. Keep the publication cancellation
guard unchanged.

## Reproducing signal

Existing driver: Temp/plan03-patched-runtime/Point0.ps1, Capture click followed by query_profile
every three seconds. Existing red result: point0-progress.jsonl at 2026-10-29T19:49:43.0896531Z,
elapsed 69.6543708 seconds, failed at 15960/129600 tiles, error Published scene changed during
capture. Instances changed 87 to 89; triangles changed 6858595 to 7143790. Last preceding sample at
19:49:39.8843594Z was still tracing with the old counts. First tracing sample at 19:48:33.9410009Z
already had those same old counts. This is more than 65 seconds of observed stability followed by a
change.

## Proven setup change

Read fixture-scene.json and final-scene.json under Temp/plan03-patched-runtime. At capture setup, cellStates entry coord [0,1] has confirmedTick=-1, snapshotCount=1, lastRenderedTick=-1. At final observation it has confirmedTick=400, snapshotCount=16, lastRenderedTick=413. Other five cells already had confirmedTick=400 at setup. Camera, unit records, subscribedCoords, tick415 and gameFlags[kPaused] match between these records.

There are 142 static island placements in both records; only 118 have footprint fields initially, all142 finally. The extra24 footprints belong to [0,1]. DescribeScene.cpp:88-100 emits footprints only when the render placement cache exists; static placements alone do not prove the cell participates in rendering. FrameBase.cpp:281 constructs that cache during client simulation/reconciliation work.

Engine/Source/GameBase.cpp:1160 excludes non-client cells until iConfirmedTick>=0 and iSnapshotCount!=0 when assembling UpdateActiveIslands input. Thus [0,1] was definitely absent from that terrain input initially and eligible later. Publication growth also continued after failure: fixture-profile.json was 69 instances/5317683 triangles before request, first tracing87/6858595, failure89/7143790, final-profile.json96/9072900. A paused global tick is insufficient readiness evidence.

## Producer and guard evidence

Engine/Source/Graphics/Lighting/LightingComparison.cpp:201 compares the current LightingPublication against the receivers-frame publication; line204 emits the recorded error. Lighting.cpp:267 and317 sum model, terrain and fixed fixture counts. BeginRender at215-222 exposes counts only after the corresponding framebuffer fence; reported diagnostics therefore lag CPU publication and do not identify the failing producer.

Terrain.cpp:150 walks every placement in each template run, including the offscreen suffix.
Lines151-155 skip nonresident templates and zero index counts; line187 adds the template triangle
count per admitted placement. Islands.cpp:210-214 derives indexCount from mesh residency, while its
visibility predicate only controls prefix ordering and raster instanceCount. Camera LOD does not
select a different mesh in this lighting count path. IslandTerrainResidency.cpp:458-459 sets
kResident and logs the template CRC/vertices/indices after restoration. This is a concrete
asynchronous route to additional terrain counts while the same camera and sim tick remain fixed.

Model assets are obtained synchronously from eager pack chunks (LightingModels.cpp:119-130). Counts
depend on publication ranges and fixed primitive counts (329,341); animation pose changes alone do
not increase them. The fixture has constant13 solids plus one animation instance. Before/final units
and available model counters match. These facts exclude simple pose/LOD explanations, but absence of
per-frame model ranges and restoration records prevents assigning the exact extra two instances to
named templates or a particular cell.

## Existing settling signals and next check

Minimal authorized next step is harness setup/checking, with no source edit or rebuild. After selecting the final fixture mode/camera and retaining pause/Tweaks/1920x1080, poll describe_scene until every subscribed cell has confirmedTick>=0, snapshotCount>0, lastRenderedTick>=0 and no pendingFullStateTick; retain the whole cellStates timeline. This closes the proven incomplete-cell precondition. Do not rely on the player cell alone or the global tick.

Poll query_profile.lighting counts as an observation of completed publications, but do not claim an arbitrary quiet interval guarantees residency completion: the failed run was quiet for over65 seconds. No existing documented query exposes all mesh residency states or a pending-restoration count. describe_scene islands are static placements, not resident GPU instances; footprint availability is a client cache signal, not GPU restoration completion.

For an exact causal retest, use existing set_log_level {category:Graphics,level:Debug} and require
its returned effective level to be Debug; the current EngineSwitches.h:78-80 floor permits this
without a build. Preserve the explicit process log, and poll describe_scene alongside query_profile
through setup and capture. Existing Restored island mesh lines identify CRC/indices; correlate their
triangle contributions and the cell-confirmation transition with the first changed publication.
Existing full logs contain no such restoration lines; retrospective recovery is unavailable because
the harness processes were released. If per-frame attribution is still ambiguous, return the
remaining gap rather than adding new instrumentation in this diagnosis.

Acceptance remains a completed capture with all seven files and unchanged consistency guard; retain first failing capture and its timeline if it fails again. Apply the cell-readiness gate before each camera/event capture. Full residency readiness cannot be certified from a few unchanged counts; a successful full capture is the check of uninterrupted publication consistency.

## Timing contract

Temp/lighting-plans/03-runtime-acceptance.md ## Steps explicitly requires 1920x1080 captures and says to read Lighting Terrain during that run with comparison enabled and islands present to decide whether the cost is far above the15ms order. Documents/Investigations/Engine/LightingFoundationHandoff.md ## Acceptance table likewise mandates1920x1080 for rows4-7; LightingComparison.cpp:160 rejects other extents. No plan03 timer threshold says4K.

.agents/skills/agent-harness/references/worker.md ### Performance investigations requires
fullscreen3840x2160 for GPU performance investigations. That is a separate general measurement
contract. It does not make mandatory1080p captures invalid or silently add a4K condition to this
explicitly specified plan03 during-run timer observation. The plan03 timer observation can be
reported with its actual1920x1080 context (final-profile Lighting Terrain
current675.723ms/average588.736ms); it is not a4K performance baseline. A separately requested
general GPU performance investigation must satisfy the4K rule. Authority retained: explicit plan03
scenario for plan03; general skill rule for general GPU investigations. The previous runtime report
conflated these scopes when it called the plan03 timer ineligible solely for lack of4K.

## Fix scope and build requirements

No source edit justified. Minimal setup correction: add the existing describe_scene cellStates
readiness gate to the next harness driver and record the paired scene/publication timeline, plus
existing Graphics Debug restoration logs. Build required: none for that procedure on the current
fresh Sandbox Debug binaries. A later proven source defect would need a new scoped resolution brief;
if Lighting or residency C++ changes, rebuild BrokenEngineSandbox client Debug|x64 and ModelViewer
Debug|x64 through /compile (and affected data targets only if shader/assets change). No server
rebuild is implied by a client harness setup correction.

## Residuals

Exact missing evidence: cellStates and template-restoration events at the first differing CPU
publication between the last tracing sample and failure, with enough producer identity to
distinguish cell admission from mesh restoration. No intermediate describe_scene samples or Graphics
Debug restoration logs exist in the supplied run. The confirmed-cell gate fixes a proven setup
deficiency, but its sufficiency for a complete capture remains untested. No instrumentation, runtime
operations, source edits, or tracked-file edits performed by this diagnosis; only this requested
Temp evidence file was written.
