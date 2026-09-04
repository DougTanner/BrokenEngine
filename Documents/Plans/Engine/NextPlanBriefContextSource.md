<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-03T23:08:07.881Z","dependsOn":[]} -->
# Fix: subagent-reporting brief baseline — main runs a second identity call `/next-plan` already answered

## Context
Observed during a `/next-plan` run. Step 1 of
`.agents/skills/next-plan/references/worker.md:5-7` had already been run as one
shell call from the worktree root:

`Import-Module ./.agents/skills/next-plan/scripts/NextPlanWorkflowCommon.psm1; Get-NextPlanContext`

Its output put `Worktree`, `SessionBranch`, `Session`, `PrimaryTip`, and
`Baseline b047dc9a805dae35ef09bd70bedfbe2c8306c4aa` in main's context. Before
writing the first subagent brief, main ran a second shell call,
`Import-Module ./.agents/scripts/AgentWorktreeSession.psm1; Get-AgentWorktreeSessionContext`,
whose output (`Worktree`, `Branch`, `SessionId`, `PrimaryRoot`, `PrimaryBranch`,
`PrimaryTip`, `Baseline`) contained no field the first call had not already
reported. Cost per `/next-plan` run: one repeated shell call plus a duplicate
context block in main.

What forces the repeat is the prose, not the scripts.
`.agents/references/subagent-reporting.md`, `## Task brief`, states that the
session baseline and every other machine-derivable identity value in a brief are
copied from `Get-AgentWorktreeSessionContext` output, naming that one function as
the source, while `Get-NextPlanContext`
(`.agents/skills/next-plan/scripts/NextPlanWorkflowCommon.psm1:18-46`) calls
`Get-AgentWorktreeSessionContext` itself at `:20` and re-emits the same values
under partly different names at `:44` — `Branch` as `SessionBranch`, `SessionId`
as `Session` and `Owner`, `PrimaryRoot` as `Primary`, `PrimaryBranch` as
`TargetBranch`, with `Worktree`, `PrimaryTip`, and `Baseline` unchanged. Reading
the rule literally, a `/next-plan` main has no sanctioned way to fill a brief
from the values it is already holding.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 300f0b7d-1d36-431d-80f1-380695db5bcb
- Worktree/branch UUID: 2d01d5b1-c258-42ec-ac6b-48d4fe06c730
- Session branch: claude/2d01d5b1-c258-42ec-ac6b-48d4fe06c730
- Worktree: .claude\worktrees\BrokenEngine\2d01d5b1-c258-42ec-ac6b-48d4fe06c730
- Landing ref: claude/2d01d5b1-c258-42ec-ac6b-48d4fe06c730
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Engine/NextPlanBriefContextSource.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/2d01d5b1-c258-42ec-ac6b-48d4fe06c730` in bounded
friction mode, supplying client `claude` and the conversation session ID recorded
above. Then make the smallest fix inside the `## In scope` boundary below.

This author's recommendation for that fix, offered as a starting point rather
than a binding decision: keep `Get-AgentWorktreeSessionContext` as the stated
source and add one short passage to the same `## Task brief` paragraph in
`.agents/references/subagent-reporting.md` saying that inside a `/next-plan` run
the `Get-NextPlanContext` output main already holds is an equally valid source
for the same values, with the field-name mapping spelled out
(`SessionBranch` = `Branch`, `Session` = `SessionId`, `Primary` = `PrimaryRoot`,
`TargetBranch` = `PrimaryBranch`; `Worktree`, `PrimaryTip`, and `Baseline`
unchanged), so main never runs both calls. The rationale for preferring this over
the alternatives: it is prose-only, it leaves both scripts and every non-
`/next-plan` caller untouched, and it removes the repeat at its source. The
alternatives considered and not recommended are changing
`Get-NextPlanContext`'s emitted field names to match (a behavior change touching
every caller of that function, for a naming preference) and dropping the named
source from `subagent-reporting.md` entirely (which would reopen the retyping-
from-scrollback hole that sentence exists to close).

If root-causing shows the fix lies outside the boundary below, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/references/subagent-reporting.md`
- `.agents/skills/next-plan/scripts/NextPlanWorkflowCommon.psm1` (read-only
  reference for the field mapping; see `## Out of scope`)

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `## Task brief` session-baseline
  paragraph of `.agents/references/subagent-reporting.md` and its
  `Get-AgentWorktreeSessionContext` invocation block

## Out of scope
- The landed change the observing session produced: `.agents/scripts/Invoke-StaticChecks.ps1`,
  `.agents/references/static-checks.md`, and
  `.agents/skills/implement-plan/references/worker.md`
- Editing `NextPlanWorkflowCommon.psm1`, `AgentWorktreeSession.psm1`, or any
  emitted field name in either
- The `## Task brief` field list itself, the handoff form, and the size caps
- The `SessionId` versus conversation-session-ID distinction stated in the same
  file, which stays as written
- `Documents/Plans/Engine/AgentExecutorHelper.md` (the `Executor` effort lookup)
  and `Documents/Plans/Engine/DirectReviewerBriefAssembly.md` (the reviewer
  brief assembly script), which own different regions of the same file
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (mechanical documentation prose, no public signature or
invariant exposure); this author's classification, to be confirmed at Step 1.
Escalate if root-causing shows the fix must change script behavior. Invariants to
preserve: a brief's machine-derivable values still come from script output and
are never retyped from memory or scrollback; the leading `./` module-import rule
and the same-shell-call rule stay stated; no transcript path or home path enters
the repository.

## Acceptance criteria
- A `/next-plan` main that has run step 1 can fill every machine-derivable brief
  field from the rule as written, without a second identity shell call
- `.agents/references/subagent-reporting.md` still forbids retyping those values
  from memory or scrollback
- `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid` and `code: ok`
