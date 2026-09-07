<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T18:20:45.958Z","dependsOn":[]} -->
# Fix: /next-plan-checkpoint-review — a second checkpoint in one session re-reviews the span the first already covered

## Context

Observed symptom. In one `/next-plan` run the claim stopped on a prerequisite,
so main ran the run checkpoint once as
`.agents/skills/next-plan/references/worker.md` step 9 requires. The user then
retried `/next-plan` in the same conversation, and main ran the checkpoint a
second time. The second reviewer selected records with
`pwsh -NoProfile -File .agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1 -TranscriptPath <transcript path>`,
which prints one row per tool call and tool result from transcript line 1, so
its rows again covered the whole span the first checkpoint had already reviewed
— including the first checkpoint's own dispatch and its returned handoff, about
100 projection rows of overlap. Cost: the second review re-derived a friction
item the first checkpoint had already reported and main had already decided not
to route, so the review effort was spent twice and an already-decided finding
was proposed again as a follow-up Plan.

Emitter wording. `.agents/skills/next-plan-checkpoint-review/references/worker.md`
step 5 currently reads: "Cover friction observable in the supplied transcript at
any point in the run up to this dispatch, including a stop before or without a
claim." Nothing in that step, or in step 11's isolation lens, bounds the start of
the covered span, so the start is always the beginning of the transcript.
`.agents/skills/next-plan/references/worker.md` `## Rules` carries the matching
sentence: "The checkpoint review covers friction observable in the transcript up
to its own dispatch; `/next-plan-review` covers the rest after landing."

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: e6cc1c50-2e68-4765-b975-1f605361b911
- Worktree/branch UUID: ba8b404f-ed0e-4453-a2ea-05b19d046331
- Session branch: claude/ba8b404f-ed0e-4453-a2ea-05b19d046331
- Worktree: .claude\worktrees\BrokenEngine\ba8b404f-ed0e-4453-a2ea-05b19d046331
- Landing ref: claude/ba8b404f-ed0e-4453-a2ea-05b19d046331, the recording
  session's own branch, whose tip is that session's final commit and which
  survives exactly as long as the worktree recorded above.
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
`/next-plan-review claude/ba8b404f-ed0e-4453-a2ea-05b19d046331` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

Recommended mechanism, chosen because it needs no new script parameter and no
new scope-file field: state the span boundary in the reviewer's own worker
reference and let the reviewer find it in the projection it already runs. The
author recommends wording step 5 so the covered span starts immediately after
the previous `/next-plan-checkpoint-review` dispatch in the same transcript when
one exists, and at the transcript start otherwise, and having step 11 select
records only from that span. The reviewer locates the boundary from the
projection's `use Task` rows, opening candidates with `Read` at the row's line
number as `offset` with `limit` 1 exactly as step 11 already prescribes; the
last such dispatch record is this review's own and is excluded, so the boundary
is the most recent one before it. The author recommends aligning the one
matching sentence in `.agents/skills/next-plan/references/worker.md` `## Rules`
with the same boundary, because that sentence is the emitter's public statement
of the same coverage rule and would otherwise contradict the fixed step.

The author considered and rejects, for this Plan, adding a start-line parameter
to `Get-TranscriptProjection.ps1` or a boundary field to the scope-file template
in `.agents/skills/next-plan/references/run-checkpoint.md`: main does not know
the previous dispatch's transcript line number, so either route would still need
the reviewer-side search this mechanism performs, plus new inputs to carry it.

The measured context-efficiency envelope is session-wide telemetry from
`Measure-SessionContext.ps1`, not a transcript span, and this Plan does not
change it.

## Critical files
- `.agents/skills/next-plan-checkpoint-review/references/worker.md` — steps 5
  and 11
- `.agents/skills/next-plan/references/worker.md` — the `## Rules` sentence
  quoted in `## Context`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the two files named above: the span
  boundary and how the reviewer locates it in step 5, the matching record
  selection in step 11, and the one-sentence alignment in the `/next-plan`
  worker `## Rules`

## Out of scope
- `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`,
  `.agents/skills/next-plan-checkpoint-review/scripts/Measure-SessionContext.ps1`,
  and every other bundled script
- `.agents/skills/next-plan/references/run-checkpoint.md`, including its
  scope-file template, and `.agents/skills/next-plan/references/worker.md`
  step 9
- How often the checkpoint runs, the post-checkpoint outcome table, and
  `/next-plan-review`'s post-landing coverage
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one skill's workflow contract); escalate
if the fix reaches build/bootstrap coordination or a bundled script's inputs.
The checkpoint must still cover every record since the previous checkpoint,
including a run that stops before or without a claim, and must still run exactly
once per `/next-plan` run. Never embed transcript paths or home paths.

## Acceptance criteria
- `.agents/skills/next-plan-checkpoint-review/references/worker.md` states the
  span boundary and how the reviewer locates it, and step 11 selects records
  only from that span
- The `/next-plan` worker `## Rules` sentence states the same boundary, with no
  remaining wording that says coverage starts at the beginning of the transcript
- A second checkpoint dispatched in a conversation that already ran one reviews
  only records after the previous checkpoint dispatch row, so the earlier
  checkpoint's dispatch and returned handoff are outside its findings
- /validate-skill passes wherever the root AGENTS.md Apply the triggered cleanup
  step triggers it; plan validate exits 0

## Notes
Recorded from the run checkpoint of the `/next-plan` run holding
`Documents/Plans/ChangeWorkflow/RemoveSiblingAgentsDocumentationSupport.md`; the
friction lies outside that Plan's `## In scope`, which changes
`/update-claude-docs` only. Main accepted the finding and chose the follow-up
route because the fix lands in a different skill package than that session's
change.
