<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-03T02:44:08.606Z","dependsOn":[]} -->
# Fix: StyleGuideWholeFileSweep runbook — batch wait ends at once on the previous batch's status

## Context
`Documents/Investigations/ChangeWorkflow/StyleGuideWholeFileSweep.md`
`## Phase 1 — per-unit pipeline` has main launch the detached coordinator and
then wait "with an until-loop on `Temp/StyleSweep/Status.txt` leaving
`RUNNING`". The coordinator in `## Appendix — prompts and coordinator` writes
`RUNNING <batch> ...` to `Status.txt` only once it starts, and `## Phase 2 —
per-batch close` leaves `Status.txt` reading `BATCH-DONE` for the batch just
closed. During a `/next-plan` run on
`Documents/Plans/Engine/StyleGuideSweepEngine.md`, the documented wait
therefore ended immediately for every batch after the first: it read the
previous batch's `BATCH-DONE` line before the new coordinator overwrote it. The
main session rewrote the wait to also match the new batch name, and repeated
that rewrite for each later batch.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 6006fbc2-b9e6-4f7a-8f51-c6b6bd4c71d9
- Worktree/branch UUID: db182aab-a512-4244-adc8-91d421db7692
- Session branch: claude/db182aab-a512-4244-adc8-91d421db7692
- Worktree: .claude\worktrees\BrokenEngine\db182aab-a512-4244-adc8-91d421db7692
- Landing ref: claude/db182aab-a512-4244-adc8-91d421db7692 (the observing
  session records and lands this Plan itself).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/StyleSweepBatchStatusWait.md`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode with the landing ref,
supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. The author
recommends stating the wait condition as `Status.txt` naming the launched batch
and no longer reading `RUNNING`, because that is the condition the session
worked around with; clearing `Status.txt` in the coordinator is the
alternative if root-causing favors it. If root-causing shows the fix lies
outside that boundary, surface it for re-planning instead of expanding scope.

## Critical files
- `Documents/Investigations/ChangeWorkflow/StyleGuideWholeFileSweep.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the wait sentence of
  `## Phase 1 — per-unit pipeline`, or the `Status.txt` write in the coordinator
  block of `## Appendix — prompts and coordinator`

## Out of scope
- The landed change the session produced
- Every other runbook section and every other coordinator behavior
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 1 (documentation: runbook prose); Tier 2 if the fix changes the
coordinator script block's behavior. Never embed transcript paths or home
paths.

## Acceptance criteria
- Following `## Phase 1 — per-unit pipeline` as written, the wait for a second
  batch does not end until that batch's coordinator finishes
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
