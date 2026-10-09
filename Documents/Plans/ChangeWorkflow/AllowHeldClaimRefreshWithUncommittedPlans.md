<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-09T13:45:30.997Z","dependsOn":[]} -->
# Fix: Invoke-NextPlanClaim.ps1 — held-claim refresh refused when run-created Plans are uncommitted

## Context
Observed symptom: during a `/next-plan` run holding a Plan claim, the step-6
idempotent claim refresh (`.agents/skills/next-plan/SKILL.md` `## Steps` step 6),
run again before approval of a scope addition, invoked
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1`
and exited 2 with `claim.worktree-dirty`, `nextAction: stop-report-to-user`. The
only dirty scheduler paths were two follow-up Plans that same run had created
through `/create-follow-up-plans` (staged new files under `Documents/Plans/Engine/`);
the other dirty paths were the run's own implementation edits under `Engine/` and
`Projects/`, and the session head was behind the primary tip. The refusal message
offered a `git stash push -u` set-aside route, which shares one stash stack with
every other worktree. The refresh step 6 requires before approval could not run,
so the run could not reach that step's done condition.

From reading the current script (not yet confirmed by a fix session):
- Line 65 returns `reused` early only when the session head equals the primary
  tip; off the tip the run falls through to the dirty-worktree gates.
- Line 73 sets `$heldPlansOnly` only when a claim is held and every dirty path is
  under `Documents/Plans/`; any dirty non-Plan path clears it.
- Line 74 then refuses any dirty `Documents/Plans/` path. Its comment (lines
  70-71) gives the reason a held claim is safe: `plan claim-next` returns the
  held claim before selection, and healing reads only primary-tip Plans. That
  reason does not depend on whether other non-Plan paths are dirty.
- With line 74 relaxed alone, the same tree would next stop at line 75: a
  targeted `-Plan` run gets `resume-with-flag`, whose rerun `/next-plan`
  `## Rules` gates on an explicit user resume instruction, and a bare run gets
  `stop-report-to-user`, so a held-claim refresh with implementation work in the
  tree would still not run.
- Lines 76-84 refuse when the fast-forward would touch a dirty path; that check
  is what protects retained work during the sync.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: b8a2e1a8-4453-4ff9-830f-4b5ce81fa393
- Worktree/branch UUID: 967a3660-a0a0-4100-9bd1-44cffe47f82d
- Session branch: claude/967a3660-a0a0-4100-9bd1-44cffe47f82d
- Worktree: .claude\worktrees\BrokenEngine\967a3660-a0a0-4100-9bd1-44cffe47f82d
- Landing ref: claude/967a3660-a0a0-4100-9bd1-44cffe47f82d — the observing
  session records and lands this Plan itself; that branch's tip is the session's
  final commit and survives exactly as long as the worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/AllowHeldClaimRefreshWithUncommittedPlans.md`,
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

Author's recommendation, with rationale: key the exemption on the held claim
alone. When the early lookup reports this session's held claim, treat every
dirty path as retained, skipping the line-74 scheduler-path refusal and the
line-75 clean-worktree refusal, while keeping the lines 76-84 fast-forward
overlap refusal. The script cannot tell which Plans the run created, and the
existing line 70-71 reason already covers every dirty Plan once a claim is held.
A run with no held claim keeps both refusals unchanged, so the guard that keeps
uncommitted scheduler input out of selection and healing is not weakened.

## Critical files
- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1`
- `.agents/skills/next-plan/references/claim-results.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the dirty-worktree gate block of
  `Invoke-NextPlanClaim.ps1` (the `$heldPlansOnly` condition and the two
  `claim.worktree-dirty` / `resume-with-flag` refusals it controls, lines 69-75
  at the time of writing)
- The matching result description in `claim-results.md`: the held-claim
  exception paragraph under `## Session baseline and the sync object`, the
  `retained` mention in the `claim.session-diverged` paragraph, and the
  `resume-with-flag` entry in the `nextAction` section

## Out of scope
- The landed change the session produced
- Behavior of a run with no held claim, including its refusal message and the
  set-aside route that message offers
- The fast-forward overlap refusal (lines 76-84) and the divergence gate
- WorktreeCli
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (trigger: scoped tool behavior of one `/next-plan` script and its
result reference); escalate if the fix reaches build/bootstrap coordination.
Invariant: with no held claim, an uncommitted `Documents/Plans` path never
reaches selection or healing. Never embed transcript paths or home paths.

## Acceptance criteria
- With a held claim, the session behind the primary tip, and uncommitted
  implementation paths plus new Plans under `Documents/Plans/` that the
  fast-forward does not touch, the claim script returns `reused` with
  `nextAction: prepare`, a `sync` object, and those paths under `retained`.
- With no held claim, the same tree is still refused with `claim.worktree-dirty`.
- With a held claim, a dirty path the fast-forward would touch is still refused.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
- The unclaimed-path refusal message already uses a tagged
  `git stash push -u -m '<tag>' -- <paths>` form; it no longer appears for this
  case once the held-claim exemption covers it.
