<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T18:35:09.129Z","dependsOn":[]} -->
# Fix: /next-plan-checkpoint-review projection — errored results and main's text are invisible

## Context

`.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`
is the only route the checkpoint reviewer may use to read a `/next-plan` run's
transcript: `.agents/skills/next-plan-checkpoint-review/references/worker.md`
step 11 states "Read the transcript only through the bundled script, never
whole-file". Steps 1 and 6 of the same worker ask that reviewer to find friction
— a bundled script that errored or returned a malformed result — and to name the
observed output and the workaround it forced.

Observed symptom, confirmed against the current script:

- Line 31 emits a tool result as `'{0} result {1} len {2}'` from `$n`,
  `$element.tool_use_id`, and the serialized content length only. The
  `tool_result` element's `is_error` field is never read, so a failed tool call
  and a successful one project to the same row shape and the reviewer cannot tell
  them apart without opening records one at a time.
- Lines 26-32 iterate the content array and emit a row only for `tool_use` and
  `tool_result` elements. An assistant `text` block in that array falls through
  both branches and emits nothing. The `$content -is [string]` branch on line 25
  catches only a whole-string content record — the script header describes it as
  "a task-notification handoff or an `attachment` record's prompt" — not the
  array-shaped assistant messages the main session actually produces.

Effect on the checkpoint run that observed this: main's own narration of what it
did after a tool failed (the workaround, the retry, the decision to skip a step)
never appears in the projection, and neither does the failure itself. The
friction lens therefore rested only on projected script results and on the
worker handoffs main received, and the reviewer had to state that limitation
rather than search for errors. The workaround available inside the documented
invocation — opening candidate records one by one with `Read` at each row's line
number — cannot be applied, because with no error marker and no text rows there
is nothing to select the candidate records by.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 529bb0e3-18c4-4967-b3b2-cda22677c6ee
- Worktree/branch UUID: 96b53c91-8027-443f-91aa-d19401360a20
- Session branch: claude/96b53c91-8027-443f-91aa-d19401360a20
- Worktree: .claude\worktrees\BrokenEngine\96b53c91-8027-443f-91aa-d19401360a20
- Landing ref: claude/96b53c91-8027-443f-91aa-d19401360a20
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design

First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying the
recorded client and conversation session ID above. Then make the smallest fix
inside the `## In scope` boundary below.

The author's recommendation, to be confirmed by that root-cause pass:

- In the `tool_result` branch, read the element's `is_error` field and mark the
  row when it is true, so an errored result is distinguishable from a successful
  one at a glance. The recommended shape keeps the existing columns and appends a
  fixed marker word rather than adding a new column order, so a reader of the
  header comment can still parse every row positionally.
- Add a branch, taken for a non-sidechain record whose `type` is `assistant`,
  that emits one row per `text` element in that record's content array. The row
  shape is `<line> assistant-text len <chars> <text>`: the second token is the
  fixed literal `assistant-text`, which no existing row uses in that position
  (`use`, `result`, and the record types the string-content row prints are all
  distinct from it), so the new row is positionally distinguishable from the
  existing `<line> <record type> text len <chars>` string-content row. `<chars>`
  is the element's full text length before truncation, and `<text>` is that text
  bounded by the same 160-character cap the `tool_use` branch already applies
  with `Substring` plus the same `\s+` whitespace collapse, so a projection of a
  long run does not grow without bound.
- Update the script's header comment, which is the documented row-shape contract
  the worker's step 11 points readers to, so it lists the new row shapes and the
  error marker.

No new parameters, switches, or modes: the projection has one documented
invocation and the fix keeps it. If root-causing shows the fix lies outside the
boundary below, surface it for re-planning instead of expanding scope.

## Critical files

- `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`
  — the header comment, the `tool_result` branch, and the content-element loop
- `.agents/skills/next-plan-checkpoint-review/references/worker.md` — steps 1, 6,
  and 11, updated only if the new row shapes make its wording wrong

## In scope

- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the header comment, the `tool_result`
  row emission, and the content-element loop of
  `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`
- The minimal wording follow-through in
  `.agents/skills/next-plan-checkpoint-review/references/worker.md` steps 1, 6,
  and 11, only where the new row shapes make the existing text incorrect

## Out of scope

- The landed change the session that observed this friction produced
- The lenses, classes, and precision guards of
  `.agents/skills/next-plan-checkpoint-review/references/worker.md` beyond the
  wording follow-through named above
- Any new parameter, switch, output mode, or alternate transcript reading route
- The context-efficiency measurement scripts and the `/next-plan` run-checkpoint
  routing
- Unrelated skills and scripts; any transcript path or transcript text in the
  repository

## Risk tier and invariants

Expected Tier 2: the change alters what one skill's bundled script outputs, which
is that script's scoped tool behavior. Escalate if the fix reaches
build/bootstrap coordination. Invariants: the script stays read-only over the
transcript; sidechain records still print nothing; every row stays one line and
positionally parseable against the header comment; the `assistant-text` row keeps
the shape `## Design` states, so it stays distinguishable from the existing
string-content row; text rows stay bounded by the existing truncation rule so the
projection cannot flood a reviewer's context; no
`Set-StrictMode`, because transcript records omit fields freely and a missing
field must read as null. Never embed transcript paths or home paths in the
repository.

## Acceptance criteria

- Running the documented invocation over a transcript containing a failed tool
  call prints a row for that result that is visibly distinct from a successful
  result's row, and the difference is described in the script's header comment.
- Running it over a transcript containing assistant text prints one
  `assistant-text` row, in the shape `## Design` states, per `text` element of
  every non-sidechain `assistant` record, each row's text capped at the same
  character limit the `tool_use` rows use, with whitespace collapsed.
- Rows for sidechain records, blank lines, and whole-string content records are
  unchanged from the current behavior.
- `/validate-skill` passes for the `next-plan-checkpoint-review` package, and
  `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid` with `code: ok`.
