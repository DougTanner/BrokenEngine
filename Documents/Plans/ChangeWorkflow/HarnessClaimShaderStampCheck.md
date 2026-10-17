<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-17T21:01:36.168Z","dependsOn":[]} -->
# Fix: agent-harness Invoke-HarnessClaim.ps1 — a launch never checks the Shared shader stamp

## Context
Observed symptom: in one `/next-plan` run, two consecutive Debug client launches
through `/agent-harness` crashed on the first boot frame with
`CheckVk failed: vkQueueSubmit(...)` returning `VK_ERROR_DEVICE_LOST` at
`Engine/Source/Graphics/Managers/ImGuiManager.cpp:642`
(`engine::ImGuiManager::Submit`, reached from
`CommandBufferManager::SubmitUiCommandBuffer` and
`Graphics::RenderMainPresentAcquire`). Both launches came after
`pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1 -RepositoryRoot '<worktree>' -Session '<label>' -GameDataDirectory '<primary Data path>'`
returned exit 0. An earlier launch of the same client binary had passed the
boot render.

Cause recorded by the diagnosis in that run: a peer session's wrapper bootstrap
Shared-data refresh
(`.agents/skills/compile/references/runtime-data-mode.md`
`## Wrapper bootstrap Shared-data refresh`) re-exported the primary
`Shader.pack`/`Shader.manifest` from a newer primary main between the passing
launch and the first crashing one, and rewrote `SharedDataShaderStamp.txt` to
shader-source fingerprint `197dca53…`. The client had been built and checked
by `/compile` at worktree `HEAD` fingerprint `5c6e468a…`; the newer primary
commit had removed `fSmokeDepositTileScale` from `GlobalLayout` in
`Engine/Data/Shaders/ShaderGlobalLayout.h`, so the client's compiled layout no
longer matched the shaders it loaded.

`/compile` compares the stamp with the worktree fingerprint only at build time
(`.agents/skills/compile/scripts/Invoke-CompileBuild.ps1:316-323`,
`data-mode.shared-shader-unverified`). The harness path never compares it:
`Invoke-HarnessClaim.ps1` proves only pack-version pairing before the lock
(`:302-328`, `claim.pack-version-mismatch`). The one guard is the prose rule in
`runtime-data-mode.md` `## Wrapper bootstrap Shared-data refresh` ("The check
runs only at build time, so rerun `/compile` before a launch").

Rework it forced: two lost harness runs, a diagnosis dispatch, a second rebase
onto primary main with 22 conflicted files, an invalidated pre-edit runtime
baseline, and the user seeing the GPU crash.

Recorded at the run checkpoint of the `/next-plan` run that claimed
`Documents/Plans/Engine/SharedSessionProtocol.md`; that Plan's `## In scope`
names no agent-harness or compile file.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: bbf5aa94-d769-4ae5-81b2-ad085e0500cc
- Worktree/branch UUID: 3616ee35-6344-4e8a-96dc-0bdbbbc50103
- Session branch: claude/3616ee35-6344-4e8a-96dc-0bdbbbc50103
- Worktree: .claude\worktrees\BrokenEngine\3616ee35-6344-4e8a-96dc-0bdbbbc50103
- Landing ref: claude/3616ee35-6344-4e8a-96dc-0bdbbbc50103, the session branch
  above, whose tip is that session's final commit and which survives exactly as
  long as the worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/HarnessClaimShaderStampCheck.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref —
supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author recommends a Client-launch pairing check beside the existing
pack-version pairing check in `Invoke-HarnessClaim.ps1`, before provisioning
and before the lock: when the scenario launches the client and the supplied
`GameDataDirectory` is the primary Shared `Output\Data`, compare the
`SharedDataShaderStamp.txt` beside it with
`Get-AgentShaderSourceFingerprint` (`.agents/scripts/AgentScriptCommon.psm1:196`,
already imported by the claim script) of the worktree's `HEAD`, and block on a
missing or differing stamp. Rationale: it is the same source-vs-data comparison
`/compile` makes, run at the moment the data is about to be loaded, and it
leaves no owner token or claim behind. Reusing the `/compile` block's remedy
selection versus naming only "rerun `/compile`" is left to the fix session.

## Critical files
- `.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1`
- `.agents/skills/agent-harness/references/worker.md` (step 5 claim result codes)
- `.agents/skills/compile/references/runtime-data-mode.md`
  (`## Wrapper bootstrap Shared-data refresh`, the build-time-only sentence)

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the files named above: the
  pre-lock pairing checks in `Invoke-HarnessClaim.ps1`, the claim-code list in
  `worker.md` step 5, and the build-time-only sentence in `runtime-data-mode.md`

## Out of scope
- The landed change the session produced
- The stamp format, the bootstrap Shared-data refresh, and the `/compile`
  shader-stamp check itself
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2: the trigger is one subsystem's tool behavior (an added check
at the agent-harness claim's existing pre-lock boundary, stamp format and
trust unchanged). Escalate to Tier 3 if the fix reaches the stamp writer in
`.agents/scripts/Bootstrap-AgentTools.ps1` or other build/bootstrap
coordination. A Server-only claim (`-ServerOnly`) loads no shaders and stays
unchecked. Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation:
  with the primary stamp differing from the worktree `HEAD` shader-source
  fingerprint, a Client claim against the primary Shared data blocks before
  the lock instead of returning exit 0
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
