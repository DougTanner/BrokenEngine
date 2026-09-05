<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-04T23:59:03.814Z","dependsOn":[]} -->
# Fix: handoff `Evidence` files — main reads the whole file although the selector already names one section

## Context
Observed at a `/next-plan` run checkpoint, as a context-efficiency finding.

Measured envelope:
- Checkpoint: the `/next-plan` run checkpoint of the session recorded below
- Tool: `Read`
- Invocation: whole-file read of the gitignored `Temp/` preparation evidence
  file the `/next-plan` preparation `implementer` cited on its handoff's
  `Evidence` line
- Measured size: 23,379 characters in one tool result, against the
  20,000-character per-result threshold the checkpoint measures against

The handoff's `Evidence` line already named the single section main needed,
`## The six open questions — answers`. Main read the file whole anyway, so the
whole preparation record entered main's context instead of the one section the
decision required.

Nothing in the current prose closes that gap on either side:

- `.agents/references/subagent-reporting.md:95` defines the field as
  `Evidence: <existing or Temp/ path plus selector, or none>`, and `:101-104`
  requires an oversized field to move its material to a file "and cites it under
  `Evidence` as path plus selector". Neither states what a selector must be, so
  a selector need not be addressable by a bounded read.
- `.agents/references/subagent-reporting.md:150` tells main only "Read only when
  a decision needs it" for the `Evidence` field. It does not say to read at the
  cited selector rather than whole.
- `.agents/skills/prepare-change/references/worker.md:17-20` has the preparation
  `implementer` draft the plan "into one file readable from the worktree root (a
  gitignored `Temp/` path is fine)" and requires only the two scope headings, so
  the file it produces is not required to carry a heading per section main will
  later need to present from.
- `.agents/skills/next-plan/references/worker.md:26-49` (step 4) is where a
  `/next-plan` main consumes that preparation handoff, and says nothing about
  how the cited evidence file is read.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 1af20171-b83a-4f30-a4ed-335f78190ee7
- Worktree/branch UUID: e05e48ea-58ba-4b02-908d-5b9c76c49e60
- Session branch: claude/e05e48ea-58ba-4b02-908d-5b9c76c49e60
- Worktree: .claude\worktrees\BrokenEngine\e05e48ea-58ba-4b02-908d-5b9c76c49e60
- Landing ref: claude/e05e48ea-58ba-4b02-908d-5b9c76c49e60
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/e05e48ea-58ba-4b02-908d-5b9c76c49e60` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary — for example if it belongs in
`.agents/skills/next-plan/references/worker.md` step 4 rather than in the shared
reference — surface it for re-planning instead of expanding scope.

The author's recommendation, for the implementer to confirm or replace, is to
fix this once in the shared reference rather than per skill, because every
manager consuming any handoff hits it:

1. In `.agents/references/subagent-reporting.md`, at the `Evidence` field
   definition, require that a `Temp/` evidence file a manager must present or
   decide from carry a `##` heading per presentable section, and that the
   selector cited on the `Evidence` line be one of those headings.
2. In the same file's consumption table row for `Evidence`, state that main
   locates the cited heading — a `^## ` search plus a bounded read at that
   heading — and never reads the file whole.
3. In `.agents/skills/prepare-change/references/worker.md`, add only the pointer
   that the drafted evidence file follows that heading requirement, without
   restating it.

An alternative the implementer may prefer is to place the requirement in
`prepare-change/references/worker.md` alone, which is narrower but leaves every
other evidence-file producer uncovered.

## Critical files
- `.agents/references/subagent-reporting.md`
- `.agents/skills/prepare-change/references/worker.md`

## In scope
- Root-cause investigation as `## Design` states, including deciding which of
  the two files above owns the requirement
- The smallest resulting fix, confined to the `Evidence` field definition, the
  oversized-field overflow paragraph, and the `Evidence` row of the consumption
  table in `.agents/references/subagent-reporting.md`, and to the drafted
  evidence file step in `.agents/skills/prepare-change/references/worker.md`

## Out of scope
- The landed change the session produced
- The handoff form's other fields, its size caps, and the overflow route itself
- `.agents/skills/next-plan/references/worker.md` and any other consuming skill,
  unless root-causing proves the requirement belongs there, which returns to
  re-planning
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (mechanical documentation: instruction prose only, no public
signature or invariant exposure). Escalate if the fix reaches the handoff size
caps or the overflow route, which every role depends on. Never embed transcript
paths or home paths.

## Acceptance criteria
- A handoff's `Evidence` selector is defined as a heading a bounded read can
  address, and main's documented action on the field is a bounded read at that
  heading rather than a whole-file read
- The requirement is stated in exactly one file, with any second site pointing
  to it
- `/validate-skill` passes for any changed skill package; plan validate exits 0
