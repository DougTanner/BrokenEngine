<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-28T23:07:40.064Z","dependsOn":[]} -->
# Fix: change-workflow.md Plan review — no role writes accepted Plan review findings into the snapshot at Tier 1/2

## Context
Observed during a `/next-plan` run (run checkpoint, isolation lens; the
measured context-efficiency envelope verdict was pass with no over-threshold
tool result, so this is not a threshold breach). After round two of
`/plan-audit` findings were accepted, main applied them to the resolved Plan
snapshot itself: one `Read` of the `Temp/` snapshot (offset 395, limit 120)
returned 14930 characters into the main context, followed by four `Edit`
calls on that snapshot. In round one of the same run main instead dispatched a
preparation `implementer` continuation for the same work, which returned only
the snapshot path plus `##` selectors, so the main context took no snapshot
text.

The gap is in the emitter, `.agents/references/change-workflow.md`
`#### Step 3 — Plan review`: its role bullets name who runs `/plan-audit`,
`/plan-simplicity-review`, `/external-grill-plan`, and
`/verify-external-claims`, but no role that writes accepted Plan review
findings into the snapshot. `.agents/skills/next-plan/SKILL.md` step 5 defers
to that step and only requires that accepted findings are resolved. Only the
Tier-3 route, `.agents/skills/next-plan/references/tier3-workflow.md`
`## Plan review`, returns accepted findings to the preparation `implementer`.
At Tier 1/2 the choice falls to main, and main can take it on itself.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: db62e552-1953-4824-b2a4-70b8b1fbfb18
- Worktree/branch UUID: 891f0cc1-fb84-42b0-b0f8-cfc1446abc9c
- Session branch: claude/891f0cc1-fb84-42b0-b0f8-cfc1446abc9c
- Worktree: .claude\worktrees\BrokenEngine\891f0cc1-fb84-42b0-b0f8-cfc1446abc9c
- Landing ref: claude/891f0cc1-fb84-42b0-b0f8-cfc1446abc9c
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/PlanReviewFindingsSnapshotWriter.md`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above. OpenCode transcript review remains unsupported regardless of worktree
  retention.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref —
supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

Recommended fix, with rationale: add one role bullet to
`#### Step 3 — Plan review` stating that, at every tier, a preparation
`implementer` continuation writes the accepted Plan review findings into the
plan snapshot and returns the snapshot path plus its `##` selectors, and that
main does not edit the snapshot. The Change Workflow step owns role
assignments, so one bullet there covers both `/next-plan` and `/prepare-change`
runs, and it matches what the Tier-3 route and round one of the observed run
already do.

## Critical files
- `.agents/references/change-workflow.md` — `#### Step 3 — Plan review`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the role bullets of
  `.agents/references/change-workflow.md` `#### Step 3 — Plan review`

## Out of scope
- The landed change the session produced
- `.agents/skills/next-plan/SKILL.md` (step 5 already defers to the Change
  Workflow step) and `.agents/skills/next-plan/references/tier3-workflow.md`
  (already routes accepted findings to the preparation `implementer`), unless
  root-causing shows either now contradicts the new bullet; then surface it for
  re-planning
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2, trigger: scoped tool behavior — a role assignment in one
subsystem, the Change Workflow instruction set. Escalate if the fix reaches
build/bootstrap coordination. Never embed transcript paths or home paths.

## Acceptance criteria
- `#### Step 3 — Plan review` names the preparation `implementer` continuation
  as the role that writes accepted Plan review findings into the snapshot at
  every tier, returning the path plus `##` selectors
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
- Follow-up class: context-efficiency (isolation lens), recorded at a
  `/next-plan` run checkpoint.
