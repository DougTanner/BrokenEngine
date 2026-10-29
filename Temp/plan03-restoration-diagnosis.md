# Plan03 capture restoration diagnosis

## Proven cause and recorded signal

Inspected worktree HEAD c1aa4847eb49a70ebb3df5c143fc7b300913c90b, lighting-foundation-work; baseline supplied cd6d410c. No source, runtime, plan or primary changes; diagnosis uses preserved evidence only.

Signal: Temp/plan03-final-runtime/point0-progress.jsonl and failure-transition.json. Eleven tracing
samples, UTC 20:34:38.2862793Z through 20:35:18.0853836Z, report 87 instances / 6858595 triangles,
advancing 360 through 9840 tiles. The next sample at 20:35:21.691224Z reports failed / 9960 tiles /
90 instances / 7109498 triangles / Published scene changed during capture. All twelve samples retain
six cells with confirmedTick400, snapshotCount16, lastRenderedTick413, no pendingFullStateTick.
Sources remain1 and traceOnlyInstances0. This directly falsifies cell synchronization as sufficient
capture readiness.

client.log:121-122 contains precisely two restored-mesh records: CRC13385203107645622817,
indices83526, and CRC11876020618390489856, indices585657. The added three placements and 250903
triangles exactly match two placements of the first mesh plus one of the second:
2*(83526/3)+(585657/3)=250903. Terrain::Prepare adds indexCount/3 once per placement
(Terrain.cpp:175-191); Lighting::EndRender adds those counts to its published model/fixture counts
(Lighting.cpp:317-323). Neither model source count nor cell state changes. The restore log has no
timestamps; the transition is bounded by recorded query timestamps, rather than assigning invented
timestamps to those log lines.

LightingComparison.cpp:164 immediately enters receivers for an accepted request, and :214 snapshots
the incomplete publication. Terrain.cpp:151-157 silently omits nonresident meshes or meshes absent
from current indirect records. IslandTerrainResidency.cpp:355-364 legitimately leaves asyncPending
meshes alone until their ranges finish; :449-459 uploads and marks them resident. A subsequent
publication therefore gains those placements, and LightingComparison.cpp:201-205 correctly cancels
the capture. Root cause: capture snapshots before current desired terrain restoration finishes, not
an incorrect cancellation guard.

The log has only these two restoration lines and no mesh reload failure, arena exhaustion or eviction records. The twelve samples show one late addition after eleven identical publications, not repeated churn. The run stopped at this first failure, so continued stability after restoration is unproven.

## Existing readiness and minimum correction

Use existing producer counts at Lighting::EndRender after terrain.Prepare:
rFramebuffer.terrainCounts.iPlacements must equal
gpIslands->mLastWrittenCounts.at(pState->iFramebuffer) before receiver snapshot. Islands.cpp:377
records every emitted desired placement in the current framebuffer. Terrain.cpp:133-191 walks the
same template runs, including offscreen tails, and counts a run only when its template is resident
and current indirect indexCount is nonzero. Thus equality means all desired current terrain geometry
is represented, rather than merely observing unchanged incomplete counts for a few samples. No new
harness field, counter, scene hash, queue, sleep or blocking loader wait is necessary.

Call order is essential: GameBase.cpp:947 applies settings, :966 writes active island records, :967
runs RenderGlobal/restoration, :971 runs Main. Islands.cpp:210-214 writes indexCount from residency
BEFORE RestorationSweep. A mesh can become resident during RenderGlobal while that framebuffer still
has indexCount0. Checking only eMeshResidency after restoration would capture one frame too soon.
Prepared-placement equality includes this publication delay automatically; next frame
UpdateActiveIslands emits the restored range.

Minimum recommended implementation: accept a valid request into a capture-only waiting status;
include waiting in IsCaptureRunning so settings/unpause/graphics recreation still cancel it; before
PrepareComparisonPipelines in EndRender, promote waiting to receivers only on the count equality.
PrepareComparisonPipelines then snapshots normally, and RecordCapture already skips any status other
than receivers/tracing. Include waiting in the existing status documentation. Keep receiver/tracing
scene-change guard unchanged. Expected source scope: LightingComparison.cpp, Lighting.cpp,
Lighting.h status comment; any strictly dependent status consumers/docs require ordinary
propagation. No residency behavior change is needed.

Do not use AnyRestorationPending as readiness: IslandTerrainResidency.cpp:203-204 returns false
while a range is pending; it means restoration work can run now. It also scans unrelated templates.
IsTextureRestorationPending similarly means ready to adopt, not all loading finished. describe_scene
has footprints/cell synchronization, not mesh state (DescribeScene.cpp:80-105); WriteProfile exposes
actual published counts and capture state, not desired count (LightingCapture.cpp:115-120). Existing
logs lack the active desired-template CRC list, so waiting for two arbitrary log lines or a stable
count cannot prove general readiness.

## Other producers and limits

Model geometry does not share island async range restoration: LightingModels.cpp:119-132 obtains
eager scene/model chunks, creates primitive/material data synchronously, and Prepare (:317 onward)
creates resources for current ranges without an async omission branch. Empty primitive sets are
deliberately excluded consistently. Fixture InitializeAnimation requires eager scene/model and
animation data (LightingFixture.cpp:72-91); fixture solids are created synchronously.
RecordPreparation records model deformation, terrain geometry, fixture BLAS and scene builds before
RecordCapture (Lighting.cpp:333-376). No pending-model geometry gate is justified by this failure.

Lazy model material textures are a separate availability branch: GetAsset requests their loads at
LightingModels.cpp:265, and their later descriptor adoption does not change triangle/instance
counts. The recorded failure proves terrain geometry readiness only; it does not prove all model
material textures were resident when capture started. A broad material/descriptor gate is outside
the proven minimal correction and should not be silently claimed covered. Existing
PackChunks::IsChunkReady and descriptor publication state are available if a separately accepted
full-material criterion needs this checked.

Finite readiness follows for a fixed active placement set whose valid chunk loads succeed and whose
meshes fit the arena: asyncPending -> cpuReady -> resident, followed by next-frame nonzero draw
records, makes the equality true; active referenced templates are excluded from ordinary eviction
(IslandTerrainResidency.cpp:190-191,419-420). No repeated source of additions is present once all
desired placements are represented. Terminal load failure or persistent arena blocking cannot be
proven absent for arbitrary data, and waiting must not be described as guaranteed completion in
those environments. This fixture's logs show neither failure. A missing/empty scene yields
desired0/prepared0 immediately; legitimate templates have positive mesh counts required by
restoration assertions.

## Retest and build request

After the accepted client-only correction, build BrokenEngineSandbox Debug|x64 and ModelViewer Debug|x64 through /compile; no shader/data format/server source change is required by this proposal. No build was run during diagnosis.

Fresh /agent-harness run using the same Local packed data, preserved patched save/identity, paused
tick400 setup and six-cell synchronization gate. Enable fixture Point/Close/event0/256 samples and
request capture immediately after current setup, so slow restoration remains exercised. Preserve
query_profile/describe_scene progression and Graphics Debug log. A pending restoration must leave
completedTiles0 until readiness, then naturally enter receivers/tracing and finish all129600 tiles
with seven exports, without retry or frozen publication. The current desired geometry must be
present at receiver start; compare final counts against the old post-restoration90/7109498 for the
identical fixture, allowing only an explained setup difference. Continue all plan03
point/density/wide captures, including wide duration/radiometry and no-validation/desync checks.

Verify a fully ready repeat starts promptly; an empty/no-island scene does not wait; disabled
comparison keeps ordinary rendering/profile behavior; settings change or unpause during waiting
still cancels; scene change after tracing still cancels. Ordinary/model-viewer rendering must remain
unaffected because the new check runs only for a pending capture and does not alter geometry
production. No unit tests, new instrumentation or runtime actions were added. Retest has not been
executed and remains required.
