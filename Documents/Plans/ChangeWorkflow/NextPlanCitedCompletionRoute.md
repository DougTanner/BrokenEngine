<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-19T01:12:16.743Z","dependsOn":[]} -->
# Fix: Complete-NextPlan.ps1 — `completion.plan-cited` stops the run instead of routing the citation rewrite, and reports a held claim as `absent`

## Context
Observed symptom: at `/next-plan` step 10, main ran
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Complete-NextPlan.ps1`
(no arguments) for an implemented, verified Plan whose claim this session held.
The result was `status: blocked`, `code: completion.plan-cited`,
`nextAction: stop-report-to-user`, with the message "3 line(s) in other live
Plans name '<held Plan file name>'; nothing was changed. Rewrite each line by
the citation rule in .agents/skills/next-plan/SKILL.md step 4, then rerun."
(emitted at `Complete-NextPlan.ps1:39`). The three citing lines, in two other
Engine and Game Plans, were already present at the run's baseline commit
`e630e6cfc3e9c474afe810be558cb5580b72a6b3`, so the step-4 preparation rule that
puts rewriting such lines on the execution card did not result in their rewrite
before step 10; why is unverified.

The rule the message names does exist (`.agents/skills/next-plan/SKILL.md`
step 4, ~:116-128, inside the preparation brief prose), but step 10 (~:224-226)
only states that the script refuses, and
`.agents/skills/next-plan/references/claim-results.md` `## Claim-exit result fields`
says every non-`ok`, non-`no-claim` completion result returns
`stop-report-to-user`. With no claim-exit route for applying that rule, main
stopped and asked the user, who answered that the fix (rewriting the citing
lines to describe the landed change) should have been done automatically; it
was mechanical and was then dispatched to an implementer.

The same blocked result reported `"claim":{"state":"absent","disposition":"none"}`
although the claim was live for this session (the earlier
`Invoke-NextPlanClaim.ps1` results reported `claimed: true`, state `existing`),
and step 10 says the claim stays held until landing. Observation only: the
result object's initializer at `Complete-NextPlan.ps1:4` sets
`claim.state='absent'` and the only assignment of `claimed` is at :46, after the
citation check; whether that is the whole cause is for the fix session to
confirm.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 2d416fac-baac-4761-9536-54fd1e5f5174
- Worktree/branch UUID: 6f9de302-4d9b-451c-b76e-03c5cacef69d
- Session branch: claude/6f9de302-4d9b-451c-b76e-03c5cacef69d
- Worktree: .claude\worktrees\BrokenEngine\6f9de302-4d9b-451c-b76e-03c5cacef69d
- Landing ref: claude/6f9de302-4d9b-451c-b76e-03c5cacef69d (the observing
  session records and lands this Plan itself; the branch survives exactly as
  long as the worktree recorded above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/NextPlanCitedCompletionRoute.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/6f9de302-4d9b-451c-b76e-03c5cacef69d` in bounded
friction mode, supplying client `claude` and conversation session ID
`2d416fac-baac-4761-9536-54fd1e5f5174`. Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

The author recommends, for the fix session to confirm:
- Give `completion.plan-cited` a claim-exit route main follows without asking:
  step 10 (or `claim-results.md`) tells main to dispatch an `implementer` that
  rewrites each citing line by the step-4 citation rule, judging "landed"
  against the tree after the change, and then rerun the script; the result's
  `nextAction` names that route instead of `stop-report-to-user`, and the
  message points at where the rule lives so it is findable. A conditional
  neither branch of which the tree settles still returns to the user, as step 4
  already says.
- Make every blocked or error completion result report the claim state the
  script actually read, so a held claim is never reported as `absent`.

## Critical files
- `.agents/skills/next-plan/scripts/Complete-NextPlan.ps1`
- `.agents/skills/next-plan/SKILL.md`
- `.agents/skills/next-plan/references/claim-results.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the files named above: the
  `completion.plan-cited` branch and the result object's `claim` field in
  `Complete-NextPlan.ps1`; step 10 (and, only if the rule's location must
  change, the step-4 citation rule) in `SKILL.md`; `## Claim-exit result fields`
  and `## nextAction` in `claim-results.md`

## Out of scope
- The landed change the session produced
- The step-4 preparation flow beyond relocating the citation rule, and why it
  left the citing lines unrewritten in the observing run
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one script's result contract and the
skill prose that consumes it); escalate if the fix reaches build/bootstrap
coordination. The refusal still changes nothing before the citing lines are
rewritten. Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: a
  completion refused for citing lines returns a `nextAction` main can act on
  without asking the user, and the documented route rewrites the lines and
  reruns to `ok`
- A blocked or error completion result for a session holding a claim does not
  report `claim.state` `absent`
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
