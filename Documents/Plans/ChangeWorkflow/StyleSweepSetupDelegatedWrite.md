<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-03T02:44:13.585Z","dependsOn":[]} -->
# Fix: StyleGuideWholeFileSweep runbook — setup routes the appendix through the main session

## Context
`Documents/Investigations/ChangeWorkflow/StyleGuideWholeFileSweep.md`
`## Setup` step 1 says to "Write the four appendix blocks verbatim" to
`Temp/StyleSweep/prompts/Find.md`, `Fix.md`, `Propagate.md` and
`Temp/StyleSweep/Run-Sweep.ps1`, without saying who does it. During a
`/next-plan` run on `Documents/Plans/Engine/StyleGuideSweepEngine.md`, the main
session read the whole `## Appendix — prompts and coordinator` section (a Read
result of 17,411 characters, flagged at the run checkpoint) only to pass the
four blocks through to four Writes; it never otherwise used that content.
`Documents/Plans/Engine/StyleGuideSweepEngine.md` `## Design` step 1 also asks
for prompt corrections applied to the temporary copies, which the same pass
handled.

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
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/StyleSweepSetupDelegatedWrite.md`, but a periodic
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
recommends that step 1 have an `implementer` write the four `Temp/StyleSweep/`
files from the runbook path plus the `## Appendix — prompts and coordinator`
selector, with any prompt corrections the claimed Plan names carried in its
brief, and return a receipt (the four paths written), because main uses none
of the appendix text itself. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

## Critical files
- `Documents/Investigations/ChangeWorkflow/StyleGuideWholeFileSweep.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `## Setup` step 1 of the runbook

## Out of scope
- The landed change the session produced
- The content of `## Appendix — prompts and coordinator` and every other
  runbook section
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 1 (documentation: runbook prose only). Never embed transcript paths or
home paths.

## Acceptance criteria
- Following `## Setup` step 1 as written, the four `Temp/StyleSweep/` files
  are written without the appendix content entering the main session
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
