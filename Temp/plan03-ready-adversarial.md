# Plan03 readiness adversarial review

## Scope and verdict

PASS: no reachable defect found in the readiness correction or reference-configuration correction.
Reviewed against HEAD c1aa4847eb49a70ebb3df5c143fc7b300913c90b in the assigned worktree.
Complete inventory: Temp/plan03-ready-adversarial-inventory.json, status pass, truncated false,
nine entries: three C++ artifacts and six documentation artifacts. Existing foundation code is
read only where needed to disprove the new correction's hypotheses. No runtime or build launched.
No source, tracked documentation, reference inputs or primary-checkout files changed.

## Traced clean

1. Capture can snapshot terrain restored too late for its framebuffer's indirect records.
   Refuted: Islands.cpp:210-214 publishes indexCount from residency before RenderGlobal;
   GameBase.cpp:966-971 runs that producer before RenderGlobal and Main. Graphics.cpp:152 runs
   RestorationSweep synchronously. Terrain.cpp:147-191 walks current framebuffer runs and excludes
   even newly resident meshes when their current indirect indexCount is zero. The equality in
   LightingComparison.cpp:182-187 therefore remains false until the subsequent indirect publication.
   LightingComparison.cpp:222 and LightingCapture.cpp:31 independently gate snapshot and recording.

2. Equality can overlook unprepared offscreen placements or match a partial total by compensation.
   Refuted: Islands.cpp:332-360 emits visible prefixes and offscreen tails; :377 records their
   complete count. Terrain.cpp:147-191 iterates identical firstInstance boundaries through that
   complete count, counting each accepted placement once and never counting an omitted run twice.
   It does not filter by visible instanceCount. All desired placements represented is necessary
   for equality; one missing run cannot be compensated by another run counted twice.

3. Legitimate zero indirect draws can leave an accepted capture waiting forever.
   Refuted for valid scene inputs: IslandTerrain.cpp:141-147 rejects nonpositive mesh counts before
   template initialization; residency also asserts positive counts at IslandTerrainResidency.cpp:447-448.
   Offscreen instanceCount zero does not imply indexCount zero. Templates with no desired
   placements contribute zero to both counts. An empty/model-only scene yields prepared0/desired0
   and immediately passes the predicate. IndexCount zero for a desired valid placement means mesh
   is unavailable to that framebuffer and must wait. Failed loads or permanent arena exhaustion
   remain outside the successful-load premise; no unconditional eventual-completion claim is made.

4. Snapshot and receiver draw can use different frames or an old snapshot during waiting.
   Refuted: Graphics.cpp:207-214 synchronously begins/fills/ends Lighting publication and then
   calls SubmitMainCommandBuffer. Lighting.cpp:262 prepares terrain, :323 publishes counts,
   :330 prepares comparison pipelines. CommandBufferManager.cpp:202 records Lighting preparation
   synchronously before waking submit worker. Lighting.cpp:341-372 records geometry/scene/environment
   then capture. No placement/residency writer intervenes. Both predicate calls select
   pState->iFramebuffer. Snapshot is written after readiness and before descriptor preparation;
   RecordCapture cannot touch receivers while waiting, including when receiver resources persist
   from an earlier capture. Tracing still compares each new publication against that snapshot.

5. Delayed readiness bypasses descriptor preparation or reuses earlier model receivers.
   Refuted: LightingComparison.cpp:226 snapshots only on readiness, then creates receiver resources
   when needed and refreshes all current model range pipelines/descriptors at :270-296 before
   RecordReceivers traverses those same ranges at :318. Solid forward descriptors and indirect
   draws remain refreshed before the new early return. Existing receiver images are overwritten
   by the receiver render pass before status advances to tracing. Compute query bindings continue
   to refresh on every tracing framebuffer via the unchanged tracing branch at :215.

6. Waiting suppresses ordinary rendering, disables cancellation, or prevents fresh requests.
   Refuted: Lighting.cpp:242-330 still prepares/publishes the ordinary scene and comparison fixture;
   only capture preparation/recording return. Existing receivers remains included in IsCaptureRunning
   at LightingComparison.cpp:74-76. ApplyComparisonSettings at :111-115 fails on unpause, hidden
   Tweaks, or settings changes before publication. Request servicing resets errors, directory and
   tile counts at :143-164. DestroyComparisonResources fails running requests at :82-86.
   TweaksScreen.cpp:312 and :406 already freeze receiver-input controls in receivers status.
   Disabled/noncapture statuses exit before evaluating the predicate. No field/status was added.

7. Reference Lighting=false removes ordinary sprites/shields or leaves cached legacy illumination.
   Refuted at pinned cd6d410c: GraphicsMenuScreen.cpp:166 toggles gLightingEnabled; its default is
   true in GraphicsSettingsWrappersBase.cpp:28. PointLightsRender.cpp:167-168 gates only projected
   lighting and preserves visible light sprites. HexShieldsRender.cpp:123-124 preserves ordinary
   shields while gating their lighting draw. AreaLightsRender.cpp:169 gates projected area light.
   LightingUniforms.cpp:98-102 triggers temporal reset on checkbox change, and :355-359 describes
   the disabled refresh publishing black. Recipe requires completed frames after toggling.

8. Documentation silently changes visual thresholds, invents radio checked readback, or claims
   completed runtime coverage. Refuted across all six inventoried doc diffs and the referenced
   recipe: exact cd6d410c/save256/client identity/FoV15/sun1.8 remain pinned. over8<=2%, one retake,
   fallback and no added/missing glow remain intact. Recipe uses successful Disabled action plus
   Capture disabled state; TweaksScreenLighting.cpp:59-65 directly derives that state from requested
   mode. Attempted-scenario log passes are distinguished from pending full fixture completion;
   rows3-7 and plan03 remain incomplete. Documentation's pending build/review wording is
   conservative bookkeeping predating the latest completed reviews/builds, not a false runtime pass.

## Decisive checks

- git diff c1aa4847... inspected all nine inventory entries; git diff --check returned exit0.
- Read terrain producer, residency progression and header validation, capture consumers,
  publication/submission order, UI cancellation/input guards and pinned-reference consumers.
- Temp/AgentBuildEnvelopes/compile-20261029T204958125Z-36232.md
  ## Plan03 readiness build results contains three success/exit0 Debug x64 Local builds with
  empty diagnostics: Sandbox client, Sandbox server and ModelViewer. No build rerun in review.
- Findings: none. API verification requests: none; retained conclusions depend on repository
  producers and ordering, not non-obvious external API propositions.

## Residual verification

Runtime was expressly excluded from this review and remains pending. Main must verify immediate
request during restoration, ready repeat, zero terrain, cancellation while waiting, unchanged
post-snapshot cancellation, completed exports and remaining plan03 acceptance rows. Retake the
Lighting=false reference with full matching readbacks. Lazy material readiness and arbitrary
terminal load/arena failures are unchanged limitations outside this geometry correction.
