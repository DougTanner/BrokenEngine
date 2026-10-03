<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-03T02:44:06.091Z","dependsOn":[]} -->
# Fix: StyleGuideWholeFileSweep runbook — SmartGit launch command not on PATH

## Context
`Documents/Investigations/ChangeWorkflow/StyleGuideWholeFileSweep.md`
`## Setup` step 2 tells the session to open SmartGit with
`smartgit.exe --open <worktree root>`. During a `/next-plan` run on
`Documents/Plans/Engine/StyleGuideSweepEngine.md`, launching that command with
`Start-Process` failed with "The system cannot find the file specified":
`smartgit.exe` is not on PATH on this machine. The main session then probed
install folders and relaunched from `C:\Program Files\SmartGit\bin\smartgit.exe`.
Every stage that follows the runbook repeats that probe.

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
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/StyleSweepSmartGitLaunchPath.md`, but a periodic
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
recommends naming the launch so it works without SmartGit on PATH (for example
the default install path, with the PATH name as the alternative), because the
observed failure was PATH resolution only. If root-causing shows the fix lies
outside that boundary, surface it for re-planning instead of expanding scope.

## Critical files
- `Documents/Investigations/ChangeWorkflow/StyleGuideWholeFileSweep.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `## Setup` step 2 of the runbook

## Out of scope
- The landed change the session produced
- Every other runbook section, including `## Phase 1 — per-unit pipeline` and
  `## Appendix — prompts and coordinator`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 1 (documentation: runbook prose only). Escalate if the fix reaches a
script. Never embed transcript paths or home paths.

## Acceptance criteria
- Following `## Setup` step 2 as written opens SmartGit on the worktree on a
  machine whose PATH lacks `smartgit.exe`
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
- Observed during the `Engine/` stage run; the `Projects/` stage
  (`Documents/Plans/Game/StyleGuideSweepProjects.md`) reuses the same step.
