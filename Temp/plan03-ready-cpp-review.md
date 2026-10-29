# Plan03 capture readiness C++ review

## Scope and result

PASS. Independently reviewed the pending HEAD c1aa4847eb49a70ebb3df5c143fc7b300913c90b
correction in all three paths from Temp/plan03-ready-targets.json. Session baseline is
cd6d410c064a5a5f905bf68efaf28493ab6fc33a. No source edits or harness operations occurred.
No correctness, scope, API-verification or project-membership finding survived inspection.

## Count and ordering proof

- Islands.cpp:199-214 refreshes current-frame indirect mesh ranges before restoration.
- Islands.cpp:259-291 counts every active placement, independent of mesh visibility.
- Islands.cpp:332-360 emits visible prefixes and offscreen tails; :377 publishes their total.
- Terrain.cpp:133,147-191 walks those same contiguous runs and counts every eligible placement.
  Only nonresident or zero-index runs are skipped; visibility instanceCount is not a gate.
- IslandTerrainResidency.cpp:122-135 requests reload for referenced nonresident geometry.
  RestorationSweep mutates mesh residency on the main thread, not the async loader thread.
  Its :447-458 requires positive mesh counts, records the upload, then marks resident.
- Therefore prepared count cannot exceed desired count or compensate for an omitted run.
  Zero desired placements gives zero prepared placements. Inactive templates add neither.
  Fully culled resident templates still count because indexCount remains positive.
- A mesh restored after UpdateActiveIslands still has zero current indirect indexCount.
  The gate waits through that publication and admits the next refreshed framebuffer record.
- BeginRender selects the current framebuffer; EndRender resets and recomputes terrainCounts.
  Reused framebuffer counters therefore cannot satisfy readiness using a previous publication.
- GameBase.cpp:947-971 orders settings, placement writes, Global restoration, then Main.
  Graphics.cpp:207-214 synchronously performs BeginRender, publication and EndRender before
  SubmitMainCommandBuffer. CommandBufferManager.cpp:195-204 records preparation synchronously.
- LightingComparison.cpp:222 gates snapshot and receiver resource/pipeline preparation.
  LightingCapture.cpp:31 gates receiver/reference access with the same predicate and frame.
  No intervening placement writer or async mesh-residency publisher changes the predicate.
- Lighting.cpp:341-372 records geometry builds, scene build and environment before receivers.
  A waiting frame never touches uncreated receiver resources; ordinary publication still runs.
- LightingModels.cpp:127-132 obtains scene and geometry from eager chunks. BufferManager.cpp:37-91
  creates their GPU buffers during construction. LightingModels::Prepare creates required
  instance resources synchronously; RecordPreparation builds them before receiver recording.
  There is no analogous async model-geometry completion omitted by the terrain-only predicate.

## Cancellation and ordinary behavior

- ApplyComparisonSettings retains receivers in IsCaptureRunning. Unpause, hidden Tweaks and
  changed settings fail waiting requests before publication, even if settings otherwise match.
- uiServicedCapture is retired before fresh-request prerequisites are evaluated; reset clears
  tiles/errors. A failed request cannot restart without a different request value.
- Graphics recreation fails receivers through DestroyComparisonResources.
- The tracing memcmp against capturePublication is unchanged and still rejects later count changes.
- Disabled/noncapture paths exit before readiness; ModelViewer refreshes its placements before
  Global/Main too. No new public phase, state, simulation data or resource lifecycle was added.
- Scope authorization, minimality and KISS pass: one private shared predicate plus its two guards
  directly implement the authorized correction and preserve existing cancellation contracts.

## Checks and limitations

- git diff --check passed. No build was requested by this findings-only review.
- Measure-Tokens: LightingComparison.cpp 342 lines/5352 tokens; LightingCapture.cpp 128/1430.
  The small changed predicate/guards expose no cohesive size-reduction follow-up.
- Final runtime retest remains manager-owned and pending; this review claims source proof only.
- Existing async mesh failures or exhausted arena capacity can leave desired geometry absent.
  The accepted correction does not promise capture completion under those failures.
