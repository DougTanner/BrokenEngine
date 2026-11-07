<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T20:01:25.580Z","dependsOn":[]} -->
# Fix: /next-plan — stage-landing drift unchecked before the next stage

## Context
Observed symptom, from one `/next-plan` run whose claimed Engine Plan
(`GetNormalTangentFrameUvSet`) the user ran as two stages in one session:

- The session baseline after step 6 was `7db5da95`. Stage 1 landed through
  `/finalize-changes` as `8fa794fa`, rebased onto primary `97fc2976`; its
  commit message records that the Plan stayed claimed for stage 2. The
  finalizer's primary-movement check
  (`.agents/skills/finalize-changes/scripts/Invoke-FinalizePrimaryMovementCheck.ps1`)
  returned `primary.disjoint-needs-review`, because it compares primary
  movement with stage 1's owned paths only, never with the statements the
  remaining stages rely on.
- Nothing compared the absorbed range `7db5da95..97fc2976` with the execution
  card before stage 2 was implemented. `97fc2976` had already moved
  `ExportScene::kiVersion` from 73 to 74
  (`DataPacker/Source/ExportJobs/ExportScene.h`), so the card's stage-2 item
  bumping it from 73 to 74 was stale.
- The stage-2 implementer found the contradiction mid-implementation, moved
  the version from 74 to 75 on its own judgment, and reported it only as a
  residual; stage 2 landed as `e04ae4e9`.
- `/next-plan` routes primary drift into a sync check only from a claim
  result's `sync` object: `.agents/skills/next-plan/references/claim-results.md`
  `## Session baseline and the sync object` updates the baseline only from a
  claim result, and `## Final-refresh sync brief` covers only step 6's final
  claim refresh. No step names a check for the range a stage landing absorbs
  before the next stage's implementation dispatch.

The change that records this Plan adds a `/finalize-changes` route for primary
drift found at landing (`.agents/skills/finalize-changes/SKILL.md`
`### Landing confirmation`): a change that regenerates tracked outputs is
rebuilt and re-prepared there. That route acts on the landing stage itself; it
does not check the absorbed range against stages that have not run yet, which
is this Plan's gap.

Root cause is not confirmed; the missing check is the observed symptom.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: f8d3b92d-a9e2-4bf3-bd98-6a05a1cf0039
- Worktree/branch UUID: 63a744e9-264f-4640-9d1d-449723b1fe0b
- Session branch: claude/63a744e9-264f-4640-9d1d-449723b1fe0b
- Worktree: .claude\worktrees\BrokenEngine\63a744e9-264f-4640-9d1d-449723b1fe0b
- Observed in an earlier session: the fields above are the observing
  session's; it landed stage 1 as `8fa794fa` and stage 2 as `e04ae4e9`.
- Landing ref: claude/30856220-1bfe-4df6-aa0c-59b1fd51086d, the later session
  that recorded this Plan, because the observing session landed before this
  Plan existed.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review e04ae4e9` in bounded friction mode, supplying the recorded
client and the recorded conversation session ID. Then make the smallest fix
inside the `## In scope` boundary below. If root-causing shows the fix lies
outside that boundary, surface it for re-planning instead of expanding scope.

The author recommends, pending that root cause, reusing the existing sync
`researcher` (`claim-results.md` `## Sync researcher steps`) rather than adding
a new mechanism: after a stage landing that leaves the claim held for a later
stage, main records the landing's rebase target as the session baseline under
`## Session baseline and the sync object`, and dispatches that `researcher`
over the range from the previous baseline to that target against the remaining
stages' card statements before the next stage's implementation dispatch; a
reported touch returns those stages to card correction before implementation,
as step 6's touch rule does.

## Critical files
- `.agents/skills/next-plan/SKILL.md`
- `.agents/skills/next-plan/references/claim-results.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/next-plan/SKILL.md`
  `## Steps` steps 8 through 10 and
  `.agents/skills/next-plan/references/claim-results.md`
  `## Session baseline and the sync object`, `## Final-refresh sync brief`,
  and `## Sync researcher steps`

## Out of scope
- The landed changes the observing session produced
- `/finalize-changes`, including its landing-time drift route, its
  `references/worker.md`, and its scripts such as
  `Invoke-FinalizePrimaryMovementCheck.ps1`
- `Invoke-NextPlanClaim.ps1` and WorktreeCli claim behavior
- The stage split rule (`.agents/skills/next-plan/references/preparation-brief.md`
  `## Stage split`) and `.agents/references/change-workflow.md`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 2, trigger: scoped tool behavior of one subsystem, the `/next-plan`
workflow (`.agents/references/risk-tiers.md` `### Risk tiers`). Escalate if the
fix reaches build/bootstrap coordination. Never embed transcript paths or home
paths.

## Acceptance criteria
- The recorded symptom no longer reproduces: when a stage landing in a
  `/next-plan` run rebases onto a newer primary and leaves the claim held for a
  later stage, `/next-plan` publicly names the check of the absorbed range
  against the remaining stages' card statements, run before the next stage's
  implementation dispatch
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
