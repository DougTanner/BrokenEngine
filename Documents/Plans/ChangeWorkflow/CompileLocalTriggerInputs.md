<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T19:39:40.350Z","dependsOn":[]} -->
# Fix: /compile — Inputs never tells the caller that DataPacker or DataFile.h changes require Local generation authorization

## Context
Observed symptom: the style sweep
`Documents/Plans/Engine/StyleGuideScannerRuleSweepCommonDataPackerTools.md`
changed files under `DataPacker/**` and `Common/DataFile.h`. Its execution card
and plan approval never requested Local generation authorization, because
`.agents/skills/compile/SKILL.md` `## Inputs` asks the caller for "the Local
generation authorization a user-approved plan or acceptance criterion grants
... or the stated basis for Shared" without saying which changed paths make
Local mandatory. That rule lives only in
`.agents/skills/compile/references/runtime-data-mode.md` `## Mode selection`
("Local is mandatory when ... changes ... touch `DataPacker/**`,
`Engine/Data/**`, `Projects/BrokenEngineSandbox/Data/**`,
`Common/DataFile.h` ..."). At build time `Resolve-CompileContext.ps1` derived
`dataBuildMode: Local`, and all four BrokenEngineSandbox client/server builds
failed with C1083 on `Data/DataTypes.h` (no Local data and no generation
authorization). Cost: one build round of 4 failed builds, an extra user
approval round, and a full rebuild.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 529e7113-822c-42a1-ae08-17fe68b76d80
- Worktree/branch UUID: 485a0f59-4094-4ff4-8816-96f7e08e2de2
- Session branch: claude/485a0f59-4094-4ff4-8816-96f7e08e2de2
- Worktree: .claude/worktrees/BrokenEngine/485a0f59-4094-4ff4-8816-96f7e08e2de2
- Landing ref: claude/485a0f59-4094-4ff4-8816-96f7e08e2de2 (the observing
  session records and lands this Plan itself).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/CompileLocalTriggerInputs.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/485a0f59-4094-4ff4-8816-96f7e08e2de2` in bounded
friction mode, supplying client `claude` and the recorded conversation session
ID. Then make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The author recommends one clause on the existing Local-authorization bullet of
`## Inputs` telling the caller that a change touching any `## Mode selection`
path trigger makes Local mandatory, so the authorization must be requested at
plan approval, and pointing at that section for the trigger list rather than
restating it, because `## Mode selection` owns the list.

## Critical files
- `.agents/skills/compile/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/compile/SKILL.md`
  `## Inputs` (the BrokenEngineSandbox Local generation authorization bullet)

## Out of scope
- The landed change the session produced
- `.agents/skills/compile/references/runtime-data-mode.md` and the mode
  selection rules themselves; `Resolve-CompileContext.ps1`
- The targets-and-configurations bullet of the same section
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 2 (scoped tool behavior); escalate if the fix reaches build/bootstrap
coordination. The trigger list stays owned once by `## Mode selection`. Never
embed transcript paths or home paths.

## Acceptance criteria
- A dispatcher reading only `/compile` `## Inputs` learns that a change under a
  `## Mode selection` path trigger requires Local generation authorization
  before the first BrokenEngineSandbox build
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
`Documents/Plans/ChangeWorkflow/CompileReleaseOnlyTargetsInputs.md` edits a
different bullet of the same `## Inputs` section; the two land independently.
