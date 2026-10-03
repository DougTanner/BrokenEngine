<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-03T02:44:11.105Z","dependsOn":[]} -->
# Fix: compile worker — `invalidatedObjects` listed without its meaning

## Context
`.agents/skills/compile/references/worker.md` step 10 lists the
`broken-engine-build-result/v1` envelope field `invalidatedObjects` (line 194)
without saying what it holds. `Tools/WorktreeCli/BuildCommand.cpp:711` fills it
only through `InvalidateSelectedObjects`, that is, only when the build was
given `selectedFiles`; an ordinary incremental build leaves it empty whatever
MSBuild recompiles. During a `/next-plan` run on
`Documents/Plans/Engine/StyleGuideSweepEngine.md`, a builder reported that the
Server Debug build "compiled nothing (invalidatedObjects empty)" while the
retained MSBuild log showed `ReplayFixtures.cpp` recompiled. The main session
had to open the retained log itself to correct the report.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 6006fbc2-b9e6-4f7a-8f51-c6b6bd4c71d9
- Worktree/branch UUID: db182aab-a512-4244-adc8-91d421db7692
- Session branch: claude/db182aab-a512-4244-adc8-91d421db7692
- Worktree: .claude\worktrees\BrokenEngine\db182aab-a512-4244-adc8-91d421db7692
- Landing ref: claude/db182aab-a512-4244-adc8-91d421db7692 (the observing
  session records and lands this Plan itself).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/CompileInvalidatedObjectsMeaning.md`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode with the landing ref,
supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. The author
recommends one clause in step 10 stating that `invalidatedObjects` lists only
the objects deleted for `selectedFiles` and says nothing about what MSBuild
compiled, which the retained log records; the envelope itself already matches
that behavior, so no WorktreeCli change is expected. If root-causing shows the
fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

## Critical files
- `.agents/skills/compile/references/worker.md`
- `Tools/WorktreeCli/BuildCommand.cpp` (read only: the field's producer)

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the envelope field list in step 10
  of `.agents/skills/compile/references/worker.md`

## Out of scope
- The landed change the session produced
- Any change to `Tools/WorktreeCli/` or the build envelope schema
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 1 (documentation: skill reference prose only). Escalate if the fix
reaches WorktreeCli or the envelope. Never embed transcript paths or home
paths.

## Acceptance criteria
- `.agents/skills/compile/references/worker.md` step 10 states when
  `invalidatedObjects` is filled and that it does not report what was compiled
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
