<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-17T15:19:04.039Z","dependsOn":[]} -->
# Fix: Get-TranscriptProjection.ps1 — `use`, `match`, and `result` rows print twice, and the output spills to a file

## Context
Tooling-friction finding from the `/next-plan` run checkpoint
(`/next-plan-checkpoint-review`) of the session recorded below.

The checkpoint reviewer ran the projection as
`.agents/skills/next-plan-checkpoint-review/references/worker.md` step 12
documents it:
`pwsh -NoProfile -File .agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1 -TranscriptPath '<transcript path>'`.
Its output was 44.9 KB and repeated `use`, `match`, and `result` rows, so the
host spilled the result to a file instead of returning it inline.

Re-running the same invocation on the same transcript later in the session
returned 397 rows and 46,284 characters: 88 `use`, 144 `match`, 91 `result`,
44 `assistant-text`, 25 `user`, and 5 `api-request-blob` rows. The pattern
observed:

- Each delegation's `use Agent` row and its `match` rows print once at the
  assistant record and again at a later record of type `api-request-blob`. That
  record type carries no `isSidechain` field and holds the earlier tool calls'
  `tool_use` elements again. The record loop
  (`Get-TranscriptProjection.ps1:37-115`) skips only `isSidechain` records, so
  it prints those elements a second time.
- The same replay adds a second `Read` entry for each read path to `$reads`
  (`:65-67`). Each later delegation that lists the path then prints one more
  `match` row (`:95-101`), and 29 rows came out byte-identical.
- `api-request-blob` records also hold `tool_result` elements that a `user`
  record holds too (transcript line 34 and line 37 carry the same
  `tool_use_id`), so each such result prints a second `result` row
  (`:104-107`).

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 6bdbf3e5-d34d-4e81-a914-5ad25ff31358
- Worktree/branch UUID: b30e196d-9362-434f-8887-7f86ca9a2631
- Session branch: claude/b30e196d-9362-434f-8887-7f86ca9a2631
- Worktree: .claude/worktrees/BrokenEngine/b30e196d-9362-434f-8887-7f86ca9a2631
- Landing ref: claude/b30e196d-9362-434f-8887-7f86ca9a2631 (the observing
  session records and lands this Plan itself; the branch tip is that session's
  final commit and survives exactly as long as the worktree recorded above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/TranscriptProjectionDuplicateRows.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode, using the landing ref
and supplying client `claude` and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author recommends skipping `api-request-blob` records in the record loop the
same way sidechain records are skipped, if root-causing confirms they only
repeat elements other records carry; that one skip removes the repeated `use`
and `result` rows and the repeated `$reads` entries. The header comment (`:1-20`)
states which records print nothing and is updated to name the skipped type.

## Critical files
- `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to
  `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`:
  the header comment (`:1-20`) and the record skip at the top of the record
  loop (`:40`)

## Out of scope
- The landed change the session produced
- `Measure-SessionContext.ps1` and the context-efficiency envelope
- `.agents/skills/next-plan-checkpoint-review/references/worker.md` step 12 and
  `.agents/skills/next-plan-review/references/measurement.md`, which point to
  the header comment for row shapes and stay unchanged unless a row they name
  is removed
- Every row shape, the `$reads` collection, and the `match` and `brief-missing`
  logic
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2. Trigger: scoped tool behavior, because one checkpoint-review
script's output changes. Every non-sidechain `tool_use`, `tool_result`, and
main-assistant text element still yields its row exactly once, at a line number
`Read` can open with `offset` and `limit` 1 (`worker.md` step 12). Every
delegation that lists a previously read path still yields a `match` row for it.
Never embed transcript paths or home paths.

## Acceptance criteria
- On a Claude parent transcript that holds `api-request-blob` records, each
  `tool_use` yields exactly one `use` row, each `tool_result` exactly one
  `result` row, and no `match` row prints twice
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
- `.agents/skills/next-plan/references/run-checkpoint.md`
  `## Follow-up routing` routed this finding here.
