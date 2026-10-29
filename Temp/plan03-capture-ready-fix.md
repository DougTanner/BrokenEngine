# Plan03 capture readiness correction

## Scope and artifacts

- Accepted HARNESSF001; conformance/non_structural correction to the approved minimum prototype.
- Worktree HEAD c1aa4847eb49a70ebb3df5c143fc7b300913c90b; session base
  cd6d410c064a5a5f905bf68efaf28493ab6fc33a.
- Exact before bytes: Temp/plan03-capture-ready-before-LightingComparison.cpp,
  Temp/plan03-capture-ready-before-LightingCapture.cpp, Temp/plan03-capture-ready-before-Lighting.h,
  Temp/plan03-capture-ready-before-AGENTS.md.
- Exact owned diff: Temp/plan03-capture-ready-fix.patch; three C++ files and the Graphics AGENTS.md
  capture paragraph only.
- Concurrent plan/reference prose remained owned by the other worker and was not edited.
- No runtime, build, unit test, staging, commit, primary checkout or residency modification
  occurred.

## Confirmed cause and source order

- Diagnosis evidence: Temp/plan03-restoration-diagnosis.md ## Proven cause and recorded signal. Late
  two-template restoration added exactly three placements and 250903 triangles, tripping the valid
  post-snapshot guard.
- GameBase.cpp:947 applies comparison settings; :966 writes current island placement/indirect
  records; :967 runs RenderGlobal restoration; :971 enters Main rendering.
- Islands.cpp:210-214 writes indexCount from pre-restoration residency. Restored meshes therefore
  remain absent from that framebuffer's indirect record for one frame.
- Islands.cpp:332-360 emits both visible prefixes and offscreen tails; :377 publishes the full
  desired count. This count includes all desired actual placements, independent of visibility.
- Terrain.cpp:133 reads that exact count; :147-170 spans every template run; :151-157 skips
  nonresident or zero-index runs; :171-191 counts every remaining placement once.
- IslandTerrainResidency.cpp:447-448 requires positive legitimate mesh vertex/index counts. A valid
  empty/model-only scene has desired0/prepared0 and passes immediately; an offscreen template has a
  positive indexCount even when its visible instanceCount is zero.
- Graphics.cpp:207-209 begins, fills and ends Lighting publication synchronously. Lighting.cpp:262
  prepares terrain before :323 publishes counts and :330 prepares comparison pipelines.
- LightingComparison.cpp:182-187 defines one private predicate comparing prepared and desired counts
  for pState->iFramebuffer. :218 returns while receivers are pending; :222 snapshots only after
  equality.
- Graphics.cpp:214 calls SubmitMainCommandBuffer; CommandBufferManager.cpp:202 synchronously records
  preparation before waking its submit worker. Lighting.cpp:341-372 records current geometry, scene
  and environment before RecordCapture.
- LightingCapture.cpp:31-34 checks the same predicate before touching receiver/reference images. No
  terrain/placement writer runs between the two checks, so the publication snapshot and receiver
  image refer to the same current framebuffer and frame.
- When unequal, diagnostics stay receivers with completedTiles0; ordinary terrain/model/fixture
  publication still runs. When equal, the existing receiver recording transitions to tracing, and
  the unchanged tracing publication guard detects subsequent count changes.

## Propagation and self audit

- Readiness semantic trigger: receivers can wait for terrain before capturing; searched both
  receivers/tracing/composing status consumers and capturePublication readers in Graphics/Lighting,
  Ui, Agent and ModelViewer.
- RecordCapture was the dependent consumer requiring the second gate. PrepareComparisonPipelines is
  the snapshot producer; both now use the same private predicate. No new status, field, enum,
  timeout, diagnostic, layout, identity or serialization format exists.
- Existing IsCaptureRunning includes receivers; ApplyComparisonSettings retains unpause, hidden
  Tweaks and settings-change cancellation, request servicing, fresh-request errors and reset logic.
  DestroyComparisonResources retains recreation cancellation.
- TweaksScreen.cpp:312,406 already disables receiver-input controls during receivers; no UI or
  harness API propagation is required. WriteProfile remains valid and reports existing receivers
  progress.
- Disabled/noncapture publications never call the predicate because their status exits before the
  receiver gate. Tracing never uses the readiness predicate; its existing scene-change guard is
  unchanged.
- Current terrain count producer searches: git grep mLastWrittenCounts and capturePublication under
  Graphics/Lighting and Islands source/header; all hits accounted for. IsCaptureTerrainReady has one
  declaration, one definition and exactly two consumers.
- Get-SessionChangeInventory completed against fix-entry HEAD;
  Temp/plan03-capture-ready-inventory.json has no vcxprojCandidates. No project-membership operation
  is needed.
- Get-AffectedAgentsDocs completed for the three owned C++ files: root, Engine/Source and Graphics
  governing documents read; Graphics capture paragraph now states the readiness ordering. Other
  guidance remains valid.
- Governing docs have pre-existing advisory size excess: Engine/Source 3846/3531 tokens; Graphics
  4241/2657 before this small sync. No unrelated trimming was performed.
- Self audit: a fresh request cannot reuse an old capturePublication while waiting because no
  receiver recording occurs until the new frame snapshot has been prepared; request reset still
  zeros tiles and errors first.
- Self audit: pending restoration cannot trip the tracing guard because status remains receivers
  until the same-frame snapshot and receiver pass complete. Later count changes still fail.
- External/API verification requests: none; all conclusions use repository producers and call order.

## Recorded checks

- git diff --check passed after final source/doc edits.
- Temp/plan03-capture-ready-static.json: Invoke-StaticChecks passed markdown-links, file-hygiene and
  line-length; validate-skill skipped. No runtime/build result is claimed.
- All added C++ lines are at most100 characters; static runner's hard500 line limit passed. Before
  snapshots and patch preserve the exact correction boundary for independent review.

## Required verification and builds

- /compile BrokenEngineSandbox client Debug|x64: selected LightingComparison.cpp and
  LightingCapture.cpp plus Lighting.h aggregate/PCH consumers.
- /compile BrokenEngineSandbox server Debug|x64: header is client-guarded; requested paired
  regression build validates the shared aggregate/PCH boundary.
- /compile ModelViewer Debug|x64: same Lighting source/header consumers as the client.
- Fresh independent correctness, style/comment, over-engineering and
  coherence/progressive-disclosure review must inspect the owned diff; no such review is claimed
  here.
- Fresh /agent-harness retest must use the same Local packed data and patched save/identity, paused
  tick400/six-cell setup, Point/Close/event0/256 samples and request immediately enough to exercise
  pending restoration.
- Preserve query_profile/describe_scene progression and Graphics Debug log: pending preparation must
  retain completedTiles0; readiness must proceed naturally to tracing and complete129600 tiles/seven
  exports without retry. For the identical fixture, compare current complete counts against90
  instances/7109498 triangles, explaining any setup difference.
- Retest a ready repeat and empty/model-only scene for prompt start; settings change, unpause and
  hidden Tweaks during waiting for failure and request retirement; actual post-snapshot scene change
  for failure. Verify fresh-request prerequisite error states still work.
- Verify disabled comparison, ordinary rendering and ModelViewer remain unaffected; then finish the
  plan03 point/density/wide acceptance captures and radiometry/desync/validation checks owned by
  main.
- Arbitrary chunk failure/arena exhaustion may leave readiness pending; no completion guarantee,
  loader change or extra failure machinery is introduced. Material texture readiness remains outside
  the accepted geometry correction.
