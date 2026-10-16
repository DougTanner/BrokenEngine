<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-16T17:28:18.437Z","dependsOn":[]} -->
# Fix: /compile Shared shader stamp — primary Shared data goes stale mid-session and blocks client builds

## Context
Observed symptom: during a `/next-plan` run, the claim script
`.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1` (`:92-106`)
fast-forwarded the session branch from `3744e4b5` to the primary tip
`a1c6ec0c`. That range changes the shader trees
(`Engine/Data/Shaders/ShaderGlobalLayout.h`,
`Engine/Data/Shaders/Lighting/LightingTemporal.comp`,
`Engine/Data/Shaders/Shadow/ShadowTemporal.comp`,
`Engine/Data/Shaders/Shadow/ShadowHistoryCopy.comp`). The next
`.agents/skills/compile/scripts/Invoke-CompileBuild.ps1` run for
`BrokenEngineSandbox` Client Debug x64 stopped before MSBuild with exit code
2 and code `data-mode.shared-shader-unverified`. The primary Shared stamp
`SharedDataShaderStamp.txt` held fingerprint `2734a15a…`, but the worktree's
`HEAD` fingerprint was `2a1ddffa…` (check at `Invoke-CompileBuild.ps1:316-355`).

The stamp is written only by the per-game Shared-data refresh in
`.agents/scripts/Bootstrap-AgentTools.ps1` (`:201-268`), which runs only at
wrapper session start (`.agents/scripts/Start-AgentWorktreeSession.ps1:172`).
`.agents/skills/compile/references/runtime-data-mode.md`
`## Wrapper bootstrap Shared-data refresh` documents that rule. For this case,
where the worktree's `HEAD` matches primary main, the block names two remedies:
a new wrapper session start, or Local generation authorization
(`-DataBuildMode Local -RunDataPacker`, up to about 1 GB, and every later game
build in the session must pass `-DataBuildMode Local`). Neither can be done
from inside the running session without a user decision. Cost: the client
was never compiled, the live client/server desync check could not run, the
user had to make a decision, and the change landed without client checks.
The block is correct; the gap is that a landing which changes the shader
trees leaves the primary Shared export stale until some later session start.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: ddfe68e6-7f06-49e2-9221-04256d05625f
- Worktree/branch UUID: 42851aa0-9153-4aa4-9832-6c5906c50b20
- Session branch: claude/42851aa0-9153-4aa4-9832-6c5906c50b20
- Worktree: .claude\worktrees\BrokenEngine\42851aa0-9153-4aa4-9832-6c5906c50b20
- Landing ref: claude/42851aa0-9153-4aa4-9832-6c5906c50b20, the session
  branch above, whose tip is that session's final commit and which survives
  exactly as long as the worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/RefreshSharedShaderDataMidSession.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/42851aa0-9153-4aa4-9832-6c5906c50b20` in bounded
friction mode, supplying client `claude` and conversation session ID
`ddfe68e6-7f06-49e2-9221-04256d05625f`. Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

Candidate fixes. The evidence does not yet show which one is smallest:
- A. On-demand refresh: make the bootstrap's per-game Shared-data refresh and
  stamp block runnable on its own, under the same bootstrap mutex and the same
  `-WaitSeconds` game-process wait. The `data-mode.shared-shader-unverified`
  remedies that now name a new session start would name that command instead.
- B. Refresh at landing: after `/finalize-changes` lands a commit that changes
  the shader-source fingerprint, run the same refresh, so primary Shared data
  tracks primary main.
- C. Refresh at claim: after the claim script fast-forwards across a range that
  changes the shader-source fingerprint, run the same refresh, or report it in
  the claim result.

Author recommendation: A. It turns the documented "new session start" remedy
into a step the running session can take, and adds no DataPacker export to
every shader landing. B would make a landing wait on peers' running games,
which could block other sessions. C covers only the `/next-plan` path, while A
also covers a manual rebase. This is a recommendation for the fix session to
confirm, not a binding decision.

## Critical files
- `.agents/scripts/Bootstrap-AgentTools.ps1` (the per-game Shared-data refresh
  and stamp block)
- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1` (the
  `data-mode.shared-shader-unverified` remedy text)
- `.agents/skills/compile/references/runtime-data-mode.md`
  (`## Wrapper bootstrap Shared-data refresh`)
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1` (option B only)
- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1` (option C only)

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the files named above that the chosen
  option needs: the refresh block in `Bootstrap-AgentTools.ps1`, the remedy
  branches in `Invoke-CompileBuild.ps1` (`:324-354`), the matching prose in
  `runtime-data-mode.md` `## Wrapper bootstrap Shared-data refresh`, and for
  option B or C only the landing or claim fast-forward site

## Out of scope
- The landed change the session produced
- The stamp's fingerprint definition and the Shared Client check itself, which
  correctly detected the mismatch
- Local generation mechanics (`## Local generation`)
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 3: the Change Workflow trigger is build/bootstrap coordination
that can block other sessions, because every option runs the primary
DataPacker export outside session start, moves primary Data under peer
sessions, and takes the bootstrap mutex. A fix confined to the remedy text
would be Tier 2. Invariants: the stamp is written only when the shader
sources, `AGENTS.md` files aside, were clean and unchanged across the export;
the refresh never runs while a game process outlives the wait budget; a
failed stamp delete still stops the refresh before any export. Never embed
transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation:
  after a shader-changing commit lands on primary and the session reaches the
  primary tip, the session can clear `data-mode.shared-shader-unverified` for
  a Shared Client build without a new wrapper session start or Local
  generation authorization
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
