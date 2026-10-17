<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-17T15:18:56.425Z","dependsOn":[]} -->
# Fix: /external-design-interface — main reads the whole source document to write the design brief itself

## Context
Context-efficiency finding from the `/next-plan` run checkpoint
(`/next-plan-checkpoint-review`) of the session recorded below.

- Tool: `Read`
- Invocation: main read lines 5-114 of the claimed Plan's preparation snapshot,
  a gitignored session scratch file, to write the design brief
- Measured size: 22,468 characters entered the main context
- Checkpoint: the `/next-plan` run checkpoint of the session recorded below

`.agents/skills/external-design-interface/SKILL.md` `### Establish the Design
Brief` step 1 (`:43-51`) has main inspect the repository and record the brief
itself: problem, callers, operations, ownership, existing interfaces, allocation,
SOA layout, threading, build affinity, frame phases, and deterministic versus
non-deterministic state. `## Inputs` (`:33-37`) says main runs the
orchestration and supplies every child the brief. Nothing lets a worker produce
the brief, so main reads the full source material even when a prepared document
already holds it.

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
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/DesignBriefWrittenByImplementer.md`,
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

The author recommends that an `implementer` write the brief. Main passes it the
source path and the user's recorded answers; it inspects the repository as step
1 lists, sorts essential invariants from accidental seams as step 2 states, and
writes the brief to a `Temp/` file. It returns the path plus a short receipt
that main reads in place of the source. Main still owns steps 4 and 5, the
Collection decision and its constraints, and hands the brief file to the
researcher and planners as path plus selector, per
`.agents/references/subagent-handoff.md` `## Handoffs`. Two current statements
must be reconciled with this: `## Inputs` says main runs the orchestration;
`## Handoff` (`:132-133`) calls the workflow read-only and `## Rules`
(`:159-161`) bars the manager and workers from creating scratch source. The fix decides how the brief-writing `implementer` and its one
`Temp/` file fit them, and checks that dispatch against the three-child-slot
limit in `## Rules` (`:167-168`); it runs and finishes before the researcher,
so it never runs beside the three planners.

## Critical files
- `.agents/skills/external-design-interface/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to
  `.agents/skills/external-design-interface/SKILL.md`: `## Inputs`,
  `### Establish the Design Brief` steps 1-3, the `## Handoff` sentence on
  changed files, and the `## Rules` bullet on what the manager and workers may
  create

## Out of scope
- The landed change the session produced
- The researcher's `Pattern evidence` return, owned by
  `Documents/Plans/ChangeWorkflow/ResearcherPatternEvidenceFile.md`
- Steps 4-14, the three design axes, the planner extension, and
  `### Compare and Decide`
- `/next-plan`, `/prepare-change`, and the preparation snapshot's shape
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Coordination
- `Documents/Plans/ChangeWorkflow/ResearcherPatternEvidenceFile.md` edits the
  `## Handoff` and `## Rules` sections of the same skill to let the researcher
  write a `Temp/` evidence file. Whichever lands second keeps the two `Temp/`
  file allowances in one consistent wording.

## Risk tier and invariants
Expected Tier 2. Trigger: scoped tool behavior, because one skill's
orchestration changes. The brief keeps every item step 1 records and the
essential-versus-accidental sort step 2 requires. A requirement the user states
while the brief is being built stays a core constraint of all three designs
(`## Rules`). Never embed transcript paths or home paths.

## Acceptance criteria
- `/external-design-interface` lets main obtain the design brief as a worker's
  `Temp/` file plus receipt, without reading the source document itself
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
- `.agents/skills/next-plan/references/run-checkpoint.md`
  `## Follow-up routing` routed this finding here. The `Read` size is the
  checkpoint's measured figure.
