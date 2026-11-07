<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T23:02:07.284Z","dependsOn":[]} -->
# Fix: Invoke-NextPlanClaim.ps1 — a refused refresh of a held claim reports no claim

## Context
Observed symptom, from one `/next-plan` run: the final claim refresh
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan '<claimed Plan path>'`,
run while the session held that Plan's claim, its head was behind the primary
tip, and a retained dirty path was also changed by the primary movement, was
refused by the overlap branch of
`.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1` (near line 100). It
returned `status: blocked`, `code: claim.worktree-dirty`, a message stating "No
Plan was claimed and the worktree was not touched", and `claim: null` — the same
report a fresh claim gets. The claim was in fact still held: rerunning the same
command right after restoring the file returned `code: reused` with "Existing
Plan claim remains live for this session." Cost: because `claim: null` never
proves claim absence (`.agents/skills/next-plan/SKILL.md`
`### Post-checkpoint outcomes`), main had to treat the claim state as unknown and
tell the user, although the script had already read the held claim.

Code reading at the recording session's baseline (unconfirmed root cause): the
held-claim lookup sets `$heldClaim` near line 78 and reuses it at line 79 only
when the session head equals the primary tip; otherwise it falls through, the
held claim exempts the dirty-path gates near line 90, and the overlap branch then
completes through `Complete-Claim` without consulting `$heldClaim`, so `claim`
keeps its `$null` default and the message denies a claim. The diverged-session
branch near line 115 and the sync-failure branch near line 118 report "no Plan
was claimed" the same way.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 29a75419-ca00-43b1-bc0c-be58309f0d44
- Worktree/branch UUID: 30856220-1bfe-4df6-aa0c-59b1fd51086d
- Session branch: claude/30856220-1bfe-4df6-aa0c-59b1fd51086d
- Worktree: .claude\worktrees\BrokenEngine\30856220-1bfe-4df6-aa0c-59b1fd51086d
- Landing ref: claude/30856220-1bfe-4df6-aa0c-59b1fd51086d, whose tip is the
  observing session's final commit and which survives exactly as long as the
  worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/ReportHeldClaimOnRefusedClaimRefresh.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`
with `/external-diagnose-bug`. Only when the transcript is genuinely needed, in a
new session run `/next-plan-review claude/30856220-1bfe-4df6-aa0c-59b1fd51086d`
in bounded friction mode, supplying client `claude` and the conversation session
ID above. Then make the smallest fix inside the `## In scope` boundary below.
The author recommends that a refusal reached after the held-claim lookup found a
claim report that claim (the `claim` object and a message that says the claim
remains held) instead of denying one, because the script already holds that
fact and the caller then needs no recovery probe. If the result shape changes,
bump the result `schemaVersion` and update its documentation. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

## Critical files
- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1`
- `.agents/skills/next-plan/references/claim-results.md`
- `.agents/skills/next-plan/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `Invoke-NextPlanClaim.ps1`
  overlap branch (near line 100) and, when the investigation confirms the same
  cause, the diverged-session and sync-failure branches reached with a held
  claim (near lines 115 and 118)
- Wording in `claim-results.md` and the `/next-plan` `SKILL.md` outcome rows
  only where the fix makes their description of this result incorrect

## Out of scope
- The landed change the session produced
- WorktreeCli claim behavior and the claim-status lookup itself
- Changing which dirty paths block a claim or the fast-forward rule
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one script's result report); escalate if
the fix reaches build/bootstrap coordination. The refusal must still leave the
worktree and the claim untouched. Never embed transcript paths or home paths.

## Acceptance criteria
- With a held claim, the session head behind the primary tip, and a retained
  dirty path the primary movement also changed, the `-Plan` refresh reports the
  held claim instead of `claim: null` and "No Plan was claimed"
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
