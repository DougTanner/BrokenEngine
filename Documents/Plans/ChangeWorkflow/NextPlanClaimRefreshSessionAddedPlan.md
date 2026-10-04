<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-04T15:39:14.885Z","dependsOn":[]} -->
# Fix: Invoke-NextPlanClaim.ps1 — off-tip final claim refresh blocks on a session-added follow-up Plan with a contradictory result

## Context
Observed symptom: during a `/next-plan` run that held the claim on
`Documents/Plans/Game/OneCppPerClass.md`, the run had deliberately created a
follow-up Plan, `Documents/Plans/Game/TweaksScreenRenderFreeFunctions.md`
(session-added, staged). The step 6 final claim refresh ran
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1`
after the primary branch had moved, and it exited 2 with
`code: claim.worktree-dirty` naming that Plan path. Earlier refreshes at the
primary tip had returned `reused` with the same staged file present.

The current script explains the tip/off-tip difference: the held-claim
shortcut returns `reused` only when `SessionHead` equals `PrimaryTip`
(`Invoke-NextPlanClaim.ps1`, the `$heldClaim` block before the `git status`
read); off the tip the request falls through to the dirty-worktree gate, whose
scheduler-input branch (the `$schedulerPaths` check, around lines 63-67)
refuses any modified or untracked path under `Documents/Plans/`, a
session-added follow-up Plan included. That refusal's result carried
`nextAction: stop-report-to-user`, yet its `message` prescribed a self-serve
route — `git stash push -u -m '<tag>' -- <paths>`, rerun, then
`git stash apply --index <entry>` — so the result contradicted itself. Main
worked around it by using the shared stash stack by hand to sync, then
restoring the staged Plan. `.agents/skills/next-plan/references/claim-results.md`
`## Session baseline and the sync object` documents the same refusal.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: f8588a3d-3d69-46e9-b773-e4506a6f3b5a
- Worktree/branch UUID: 38e0f674-d9b5-4b35-b12a-a9c583aa6d5e
- Session branch: claude/38e0f674-d9b5-4b35-b12a-a9c583aa6d5e
- Worktree: .claude\worktrees\BrokenEngine\38e0f674-d9b5-4b35-b12a-a9c583aa6d5e
- Landing ref: claude/38e0f674-d9b5-4b35-b12a-a9c583aa6d5e, whose tip is that
  session's final commit and which survives exactly as long as the worktree
  recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/NextPlanClaimRefreshSessionAddedPlan.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above. OpenCode transcript review remains unsupported regardless of worktree
  retention.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`:
establish why a session-added Plan that the fast-forward cannot touch must
block the off-tip refresh of an already-held claim, and whether the result's
`nextAction` or its `message` is the intended route. Only when the transcript
is genuinely needed, in a new session run `/next-plan-review <review ref>` in
bounded friction mode — the landing ref — supplying the recorded client and
the recorded conversation session ID. Then make the smallest fix inside the
`## In scope` boundary below, so the result is self-consistent and the
documented step 6 refresh no longer needs a hand-run stash for a follow-up Plan
the run created itself. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

## Critical files
- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1`
- `.agents/skills/next-plan/references/claim-results.md`
- `.agents/skills/next-plan/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the held-claim shortcut and the
  dirty-worktree gate's scheduler-input branch in `Invoke-NextPlanClaim.ps1`,
  the matching paragraph of `claim-results.md`
  `## Session baseline and the sync object`, and `SKILL.md` `## Steps` step 6
  only if the refresh route it states changes

## Out of scope
- The landed change the session produced
- WorktreeCli's `plan claim-next`, `plan validate`, and claim storage
- The `-ResumeRetained` route for non-Plan retained work
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one skill's claim script); escalate if
the fix reaches build/bootstrap coordination. Never embed transcript paths or
home paths. Validation and Plan completion must still never run against
uncommitted scheduler input that could change selection.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation:
  an off-tip step 6 refresh with a session-added, staged follow-up Plan either
  completes without a hand-run stash or stops with a result whose `nextAction`
  and `message` name the same route
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
Recorded from the `/next-plan` run checkpoint (`/next-plan-checkpoint-review`)
of the run that claimed `Documents/Plans/Game/OneCppPerClass.md`. The
misbehaving script is outside that Plan's `## In scope`. No live Plan covered
this script and symptom when this Plan was written. No dependencies or
Coordination constraints.
