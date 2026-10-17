<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-17T15:18:51.584Z","dependsOn":[]} -->
# Fix: /create-follow-up-plans — a written Plan body cites untracked `Temp/` scratch files a claiming session cannot read

## Context
Tooling-friction finding from the `/next-plan` run checkpoint
(`/next-plan-checkpoint-review`) of the session recorded below.

A `/create-follow-up-plans` `implementer` wrote a follow-up Plan under
`Documents/Plans/Game/` whose body cited two files in the gitignored `Temp/`
directory: an earlier session's design document and a numbered correction in
this session's preparation snapshot. A later session that claims the Plan works
in a different worktree and cannot read either file, so the citations carry no
evidence for it. Removing them cost an extra `implementer` dispatch to rewrite
the body plus one more review round.

Nothing in the skill bars such a citation:

- `.agents/skills/create-follow-up-plans/references/worker.md` `### 2. Draft
  and classify` (`:20-56`) lists the drafted Plan's sections and content, and
  `## Rules` (`:72-100`) requires recorded evidence. Neither says a Plan body may
  cite only tracked repository paths.
- `.agents/skills/create-follow-up-plans/references/tooling-friction-plan-template.md`
  (`:3-5`) bars transcript paths, transcript text, and absolute paths from a
  tooling-friction body, but not untracked files.
- `.agents/skills/create-follow-up-plans/SKILL.md` `## Inputs` passes the
  caller's `Evidence` to the worker, and a `/next-plan` brief's `Evidence` rows
  are routinely `Temp/` path plus selector (`.agents/references/subagent-handoff.md`
  `## Handoffs`). So the worker receives `Temp/` citations as its evidence and
  copies them into the Plan.

In this run, main added "Plan bodies cite only tracked repository paths" to the
later dispatch brief's `Prohibitions` by hand.

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
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/FollowUpPlansCiteTrackedPathsOnly.md`,
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

The author recommends one rule in the worker's drafting step: a Plan body cites
only paths tracked in the repository. Evidence the brief supplies as a `Temp/`
path plus selector is restated in the body as the durable facts it holds, such
as the tracked `path:line`, the symbol, and the observed behavior, never as the
`Temp/` path. `### 1. Prove and consolidate proposals` step 2 already asks for
durable `path:line` evidence, so the rule extends that requirement to the
written body. The tooling-friction template's opening sentence is the matching
place to repeat the bar for friction bodies, unless the fix finds that the
worker rule alone reaches the template's use.

## Critical files
- `.agents/skills/create-follow-up-plans/references/worker.md`
- `.agents/skills/create-follow-up-plans/references/tooling-friction-plan-template.md`
- `.agents/skills/create-follow-up-plans/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to:
  - `.agents/skills/create-follow-up-plans/references/worker.md` —
    `### 2. Draft and classify` and, only if the rule belongs there instead,
    `## Rules`
  - `.agents/skills/create-follow-up-plans/references/tooling-friction-plan-template.md`
    — the opening paragraph before the fenced template
  - `.agents/skills/create-follow-up-plans/SKILL.md` — `## Inputs`, only if the
    dispatcher-facing contract must state that `Temp/` evidence is restated
    rather than cited

## Out of scope
- The landed change the session produced
- `.agents/references/subagent-handoff.md` and its `Temp/` `Evidence` rows,
  which stay valid between workers inside one session
- `New-PlanFile.ps1`, `Test-PlanCitations.ps1`, WorktreeCli `plan validate`,
  and any new script or lint for the rule
- Existing Plans' bodies
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2. Trigger: scoped tool behavior, because only the
`/create-follow-up-plans` worker's drafting rule changes. A Plan body keeps
citing durable evidence, and a tooling-friction body keeps its existing bars on
transcript paths, transcript text, and absolute paths. Never embed transcript
paths or home paths.

## Acceptance criteria
- The `/create-follow-up-plans` worker instructions bar a Plan body from citing
  an untracked `Temp/` path and say how `Temp/` evidence enters the body instead
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
- `.agents/skills/next-plan/references/run-checkpoint.md`
  `## Follow-up routing` routed this finding here.
