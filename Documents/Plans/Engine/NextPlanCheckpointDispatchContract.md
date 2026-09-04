<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-04T00:12:34.350Z","dependsOn":[]} -->
# Fix: next-plan-checkpoint-review — the dispatch surface documents output and transcript reading the skill does not define

## Context
Two contract gaps in the `/next-plan-checkpoint-review` dispatch surface made
main write a brief the reviewer could not follow, in one `/next-plan` run's
checkpoint.

1. `.agents/skills/next-plan/references/run-checkpoint.md:47-58` — the
   `Handoff line recorded` column and the paragraph under the table name a
   `Friction follow-ups:` line and a `Context-efficiency follow-ups:` line, but
   `.agents/skills/next-plan-checkpoint-review/SKILL.md` `## Handoff` defines
   only `Run checkpoint:` and `Rows at or over threshold:` in the reviewer's
   summary block. Observed: main's dispatch brief required the reviewer to
   return those two follow-up lines; the reviewer answered that it cannot
   produce them, because Plan authoring belongs to main via
   `/create-follow-up-plans`. Cost: the brief demanded output the skill does not
   define, and the discrepancy had to be reconciled after the review returned.
   The reference never says the two lines are main's own record after the
   `## Follow-up routing` step, so a manager reading only that table writes an
   impossible requirement into the brief.

2. `.agents/skills/next-plan-checkpoint-review/SKILL.md` `## Inputs` names only
   the transcript's absolute path, while
   `.agents/skills/next-plan-checkpoint-review/references/worker.md:61-63`
   step 11 requires reading the transcript only through the bundled
   `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`.
   Observed: main's brief told the reviewer to filter the transcript with Grep
   and PowerShell; the reviewer flagged the conflict with its mandatory step.
   Cost: a contradicted instruction the reviewer had to raise and main had to
   withdraw. The public file is what a dispatching manager reads, and it does
   not say the transcript is consumed through the bundled projection script.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 2d56cede-2173-427c-94d1-a1fe8ae82bc8
- Worktree/branch UUID: 4fefe754-6a58-42f4-8566-57f1e6426963
- Session branch: claude/4fefe754-6a58-42f4-8566-57f1e6426963
- Worktree: .claude\worktrees\BrokenEngine\4fefe754-6a58-42f4-8566-57f1e6426963
- Landing ref: claude/4fefe754-6a58-42f4-8566-57f1e6426963
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Engine/NextPlanCheckpointDispatchContract.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`; both symptoms are visible in the cited files without any
transcript. Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/4fefe754-6a58-42f4-8566-57f1e6426963` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below.

The author's recommendation, not binding on the fix session: in
`run-checkpoint.md`, state once — where the table's `Handoff line recorded`
column is introduced — that the two follow-up lines are main's own record after
`## Follow-up routing`, not reviewer output; and in the checkpoint-review
`SKILL.md` `## Inputs`, say that the transcript is consumed through the bundled
`Get-TranscriptProjection.ps1` and not read or filtered by other means, so a
manager does not issue a conflicting reading instruction. Keep each fact at one
owning layer: the projection script's mechanics stay in
`references/worker.md`, and `## Inputs` only names the reading route.

If root-causing shows the fix lies outside that boundary — for example that the
reviewer's `## Handoff` should itself carry the follow-up lines — surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/next-plan/references/run-checkpoint.md`
- `.agents/skills/next-plan-checkpoint-review/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting prose fix, confined to the two files above:
  `run-checkpoint.md` `## Measurement states` (the table's handoff-line column
  and the paragraph following the table) and the checkpoint-review `SKILL.md`
  `## Inputs` section

## Out of scope
- The landed change the session produced
- `.agents/skills/next-plan-checkpoint-review/references/worker.md`, its
  bundled scripts, and the reviewer's `## Handoff` line shapes
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (mechanical: documentation prose with no public signature or
invariant exposure). Escalate to Tier 2 if the fix changes the reviewer's
handoff contract or any script behavior rather than describing the existing
one. Never embed transcript paths or home paths.

## Acceptance criteria
- `run-checkpoint.md` states, in one place, that the `Friction follow-ups:` and
  `Context-efficiency follow-ups:` lines are main's record after Follow-up
  routing rather than reviewer output
- The checkpoint-review `SKILL.md` `## Inputs` names the bundled projection
  script as the transcript's reading route
- /validate-skill passes for the changed SKILL.md;
  `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  status valid

## Notes
Recorded from the run checkpoint of a `/next-plan` run whose own change touched
`.agents/scripts/` and `/compile` and `/finalize-changes` files, so both files
above are outside that change's scope. Main accepted these two findings and
declined the run's other five checkpoint rows.
