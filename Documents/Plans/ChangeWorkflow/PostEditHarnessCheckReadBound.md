<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-16T01:37:20.368Z","dependsOn":[]} -->
# Fix: change-workflow.md — unbounded card read for post-edit harness checks

## Context
Observed symptom (isolation finding, class fixable-defect, emitter
`.agents/references/change-workflow.md`): in a `/next-plan` run, before
dispatching the `runner` for the post-edit `/agent-harness` acceptance checks,
main searched for the first line of every acceptance check in the execution
card and then read the full text of the two harness checks (32 lines of the
card) into its own context. The `runner` brief it then sent already cited
those same checks by the card's path plus its `## Execution card` selector and
passed the run-duration judgment (a check expected to take about 10 minutes)
to the `runner`, so main's read was never used to decide anything: the
`runner` re-read the same text itself.

`.agents/references/change-workflow.md` `#### Step 4 — Implement and propagate`
bounds main's reading of the card only for checks marked
`before the first edit`: one line search for that literal within the card's
`Acceptance checks` field, then reading only each marked check.
`#### Step 5 — Run targeted pre-review checks` says full builds and harness
scenarios "remain acceptance-table work", and
`#### Step 8 — Verify the acceptance table` gives no rule for how main finds or
dispatches the post-edit `/agent-harness` acceptance checks, so main had no
bound on how much of the card it reads for them.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: f5cfa349-2285-40f7-aed4-60c2a4d34d59
- Worktree/branch UUID: 041e27c6-7b5f-4879-840f-1df103133526
- Session branch: claude/041e27c6-7b5f-4879-840f-1df103133526
- Worktree: .claude\worktrees\BrokenEngine\041e27c6-7b5f-4879-840f-1df103133526
- Landing ref: claude/041e27c6-7b5f-4879-840f-1df103133526, the session branch
  above, which survives exactly as long as the worktree recorded above.
  The review ref the invoker passes to /next-plan-review has two cases. When
  the `Observed in an earlier session` line is present, it is that line's
  landed commit hash, with no lookup. When that line is absent, the recording
  session is the observing session, and worktree reuse moves its branch past
  its landing, so the invoker never reviews the ref itself: it resolves the ref
  to the commit that added this Plan with
  `git log --follow --diff-filter=A -n 1 --format=%H <ref> -- <this plan path>`
  and passes that commit. When the recorded ref is gone or that command prints
  nothing (a later session moved this Plan), it runs the same command with
  `<ref>` omitted. A periodic Plan-history squash can make either command
  return an unrelated aggregate commit, and a landing that also deletes a
  similar file can make it return another session's commit, so review the
  result only when the commit is attributable to the observing session recorded
  above alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`: confirm which step of `.agents/references/change-workflow.md`
owns the dispatch of post-edit `/agent-harness` acceptance checks and that no
rule there bounds main's reading of the execution card for them. Only when the
transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode, with the review ref
the provenance block above defines, supplying the recorded client and the
recorded conversation session ID. Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

The author recommends mirroring the Step 4 rule rather than adding a new
mechanism: main finds the `/agent-harness` acceptance checks with one line
search limited to the card's `Acceptance checks` field (as Step 4 does for
checks marked `before the first edit`), and the dispatched `runner` reads each
check's text itself from the card path plus its `## Execution card` selector,
so main never reads a post-edit check's body. Rationale: Step 4 already
establishes the bound, and the `runner` already receives the duration judgment.

## Critical files
- `.agents/references/change-workflow.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to
  `.agents/references/change-workflow.md` `#### Step 4 — Implement and propagate`
  (its card-reading rule, if the fix generalizes it),
  `#### Step 5 — Run targeted pre-review checks`, and
  `#### Step 8 — Verify the acceptance table`

## Out of scope
- The landed change the session produced
- The `/agent-harness` skill, the `runner` agent definition, and the execution
  card template
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: how main dispatches one kind of
acceptance check); escalate if the fix reaches build/bootstrap coordination.
Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation:
  `.agents/references/change-workflow.md` bounds main's reading of the
  execution card for post-edit `/agent-harness` acceptance checks to one line
  search, and the `runner` reads each check's text from the cited card path
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
