<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-25T15:46:45.703Z","dependsOn":[]} -->
# Fix: /comment-review — Findings row omits the flagged comment's current text

## Context
Context-efficiency (isolation) finding from a `/next-plan` run checkpoint,
class fixable-defect, emitter `.agents/skills/comment-review/SKILL.md`.

Observed symptom: the `/comment-review` `## Handoff` Findings row form
(`.agents/skills/comment-review/SKILL.md:66`) carries
`<ID> Required|Recommended path:start-end — class — evidence — replacement`,
but not the flagged comment's current text. To accept or decline one returned
row, the main session had to Read `Engine/Source/File/PackChunks.cpp`
(offset 260, limit 10) itself to see the comment the row flagged. That Read
also triggered the PostToolUse hook that injects the AGENTS.md files on the
read path, adding `Engine/Source/AGENTS.md` and `Engine/Source/File/AGENTS.md`
(about 24,835 characters) to the main context. The source Read was work the
dispatched `mechanic` had already done; the injected AGENTS.md content was
unneeded for the decision.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 4647ce8d-d65a-4832-90ea-eef20457ccfb
- Worktree/branch UUID: 4ad65a1d-5838-41ea-83d3-e7380ab86306
- Session branch: claude/4ad65a1d-5838-41ea-83d3-e7380ab86306
- Worktree: .claude\worktrees\BrokenEngine\4ad65a1d-5838-41ea-83d3-e7380ab86306
- Landing ref: claude/4ad65a1d-5838-41ea-83d3-e7380ab86306 — the observing
  session records and lands this Plan itself, so the session branch tip is that
  session's final commit; it survives exactly as long as the worktree above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/CommentReviewFindingsCarryCommentText.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/4ad65a1d-5838-41ea-83d3-e7380ab86306` in bounded
friction mode, supplying client `claude` and conversation session ID
`4647ce8d-d65a-4832-90ea-eef20457ccfb`. Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

Recommended fix (the author's recommendation, not a binding decision): add a
slot to the Findings row form that carries the flagged comment's current text,
so main can weigh the row's evidence and replacement against the actual comment
without reading source. Carrying the text verbatim is recommended over a
summary, because the replacement is judged against the exact wording; even a
`dense`-class block is smaller than a source Read plus the AGENTS.md content the
read hook injects. Update the example row to match the new form.

## Critical files
- `.agents/skills/comment-review/SKILL.md` — the authorized fix boundary

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to
  `.agents/skills/comment-review/SKILL.md` `## Handoff`: the Findings row form
  (line 66), its example row (line 72), and the focused re-review paragraph only
  where the new slot makes it incorrect

## Out of scope
- The landed change the session produced
- `.agents/skills/comment-review/references/worker.md` beyond what the new row
  slot makes incorrect
- The PostToolUse AGENTS.md injection hook itself
- Other review skills' Findings row forms
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one skill's handoff form); escalate if
the fix reaches build/bootstrap coordination. Never embed transcript paths or
home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: a
  `/comment-review` Findings row carries the flagged comment's current text, so
  main can accept or decline the row without reading the source file
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
- `/external-skill-creator` validation applies, because the change edits a
  `.agents/skills/*/` package.
