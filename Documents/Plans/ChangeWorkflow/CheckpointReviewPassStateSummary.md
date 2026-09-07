<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T17:29:16.520Z","dependsOn":[]} -->
# Fix: /next-plan-checkpoint-review — summary line undefined for a supplied `pass` context state

## Context

During a `/next-plan` run, main measured the context-efficiency envelope, got a
`pass` verdict with `breachRowsTruncated` false, and supplied the state `pass` to
`/next-plan-checkpoint-review` exactly as the `pass` row of the
`## Measurement states` table in
`.agents/skills/next-plan/references/run-checkpoint.md` directs.

The reviewer's handoff summary line has no defined value for that case:

- `.agents/skills/next-plan-checkpoint-review/SKILL.md` `## Handoff` declares
  `Rows at or over threshold: <count | skipped (<supplied state>)>`, and its
  neighbouring rule says to use the `skipped` form "when the context-efficiency
  lens is skipped".
- `.agents/skills/next-plan-checkpoint-review/SKILL.md` `## Inputs` skips that
  lens only for a missing or non-envelope context input.
- `.agents/skills/next-plan/references/run-checkpoint.md` states "The reviewer
  skips the context-efficiency lens in every blocked and error case."

A supplied `pass` state is neither missing, non-envelope, blocked, nor an error,
so under `## Inputs` the lens is not skipped — yet a bare `pass` state carries no
`topResults` rows, so there is no row set to count either. The state matches
neither branch of the declared line.

The cost: main's dispatch brief had to hedge the handoff form itself, instructing
the reviewer to use "the `skipped (pass)` form if the worker contract says so,
otherwise `pass`". That pushed a handoff-form decision onto the reviewer, who
resolved it by writing `0` — a third form the contract does not declare, and one
that is indistinguishable from a measured envelope with zero breaching rows.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the session
`/next-plan-review` must reach — while the `Landing ref` line names a ref whose
tree actually contains this Plan:
- Client: claude
- Conversation session ID: 3656f895-74c5-4b8a-9101-1c1e80d69753
- Worktree/branch UUID: 412415d0-8439-4ca9-8967-7690feb857d2
- Session branch: claude/412415d0-8439-4ca9-8967-7690feb857d2
- Worktree: .claude\worktrees\BrokenEngine\412415d0-8439-4ca9-8967-7690feb857d2
- Landing ref: claude/412415d0-8439-4ca9-8967-7690feb857d2
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so review
  its result only when the commit is attributable to one session alone (its diff
  limited to that session's files); never review an aggregate or multi-session
  squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact conversation session ID above.

## Design

First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying the recorded
client and conversation session ID. Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

The author's recommendation is to state in `## Handoff` what the summary line
carries for a supplied `pass` state, and to make `## Inputs` and the
`## Measurement states` prose agree with it, so the reviewer never has to choose a
form. Whether the resolved answer is a distinct token (for example
`pass (no envelope rows)`), the existing `skipped (pass)` form, or `0`, is a
wording choice for the fix session to settle; the requirement is that exactly one
of them is declared and that the two supporting documents state the same thing.
Recommended because the defect is an undeclared case in an existing three-line
contract, so the fix is wording in the documents that already own it, not a new
measurement path or a new reviewer lens.

Change Workflow tier: expected Tier 2, scoped tool behavior — the handoff form one
skill emits and one caller records. Escalate only if root-causing shows the fix
must change what main measures or supplies rather than what the reviewer reports.
Never embed transcript paths or home paths.

## Critical files

- `.agents/skills/next-plan-checkpoint-review/SKILL.md`
- `.agents/skills/next-plan/references/run-checkpoint.md`

## In scope

- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `## Handoff` and `## Inputs`
  sections of `.agents/skills/next-plan-checkpoint-review/SKILL.md` and the
  `## Measurement states` section of
  `.agents/skills/next-plan/references/run-checkpoint.md`

## Out of scope

- The landed documentation change the observing session produced
- The friction and isolation lenses, their finding row forms, and the reviewer's
  precision guards
- The envelope-measuring script, its schema, and its thresholds
- `/next-plan-checkpoint-review` `references/worker.md` beyond a change the
  corrected summary-line contract makes necessary
- Unrelated skills and scripts; any transcript path or transcript text in the
  repository

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior); escalate per `## Design`. The reviewer
still never runs the measuring script and never reads `CLAUDE_CODE_SESSION_ID`;
`Build required` and `Residuals` stay last in the extended handoff; the summary
line stays a single line; no transcript path or home path enters the repository.

## Acceptance criteria

- `## Handoff` declares exactly one value the summary line carries for a supplied
  `pass` state, and that value is distinguishable from both a skipped lens and a
  measured envelope with zero breaching rows
- `## Inputs` in the reviewer skill and `## Measurement states` in
  `run-checkpoint.md` state the same skip condition as `## Handoff`, with no case
  left unmatched
- A dispatch brief for a `pass` state can name the summary-line form directly
  instead of hedging between two forms
- `/validate-skill` passes for `next-plan-checkpoint-review` and `next-plan`, and
  `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid`, `code: ok`

## Notes

The reviewer's `0` was a reasonable reading of an underspecified contract rather
than a deviation from a correctly declared form, so the skill text is the defect.
