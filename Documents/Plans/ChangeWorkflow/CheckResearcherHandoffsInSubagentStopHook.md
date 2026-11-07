<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T23:02:08.375Z","dependsOn":[]} -->
# Fix: Invoke-SubagentStopHook.ps1 — researcher handoffs over the cap are not returned

## Context
Observed symptom, from one `/next-plan` run: two `researcher` subagent handoffs
returned at 4,161 and 5,511 characters, over the shared cap in
`.agents/references/subagent-handoff.md` `## Handoffs`, and reached the main
session without being sent back for trimming. The SubagentStop hook registered
in `.claude/settings.json` runs
`.agents/scripts/Invoke-SubagentStopHook.ps1`, whose
`$script:CheckedAgentTypes` (line 21) lists only `implementer` and `reviewer`;
line 59 allows every other agent type without checking form or length. Each
oversized researcher handoff cost main context the overflow rule exists to
avoid. Implementer handoffs in the same run also exceeded the cap, before the
hook enforced it.

Researchers already return the shared handoff form: for example
`.agents/skills/plan-alternatives/SKILL.md` `## Handoff` and
`.agents/skills/analyze-diagsession/SKILL.md` declare extension fields followed
by the shared lines.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 29a75419-ca00-43b1-bc0c-be58309f0d44
- Worktree/branch UUID: 30856220-1bfe-4df6-aa0c-59b1fd51086d
- Session branch: claude/30856220-1bfe-4df6-aa0c-59b1fd51086d
- Worktree: .claude\worktrees\BrokenEngine\30856220-1bfe-4df6-aa0c-59b1fd51086d
- Landing ref: claude/30856220-1bfe-4df6-aa0c-59b1fd51086d, whose tip is the
  observing session's final commit and which survives exactly as long as the
  worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/CheckResearcherHandoffsInSubagentStopHook.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`
with `/external-diagnose-bug`, including whether every skill that dispatches a
`researcher` has it return the shared handoff form, since the hook also sends
back a message without that form. Only when the transcript is genuinely needed,
in a new session run
`/next-plan-review claude/30856220-1bfe-4df6-aa0c-59b1fd51086d` in bounded
friction mode, supplying client `claude` and the conversation session ID above.
Then make the smallest fix inside the `## In scope` boundary below. The author
recommends adding `researcher` to `$script:CheckedAgentTypes` and updating the
script's header comment to match, because the existing form and length checks
already apply unchanged. If a researcher-dispatching skill returns a different
form, or root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/scripts/Invoke-SubagentStopHook.ps1`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `$script:CheckedAgentTypes` and the
  header comment of `Invoke-SubagentStopHook.ps1`

## Out of scope
- The landed change the session produced
- The cap value and the form and length checks in `Get-HandoffProblem`
- Other agent types (`locator`, `planner`, `mechanic`, `builder`), for which no
  over-cap handoff was observed
- Hook registration in `.claude/settings.json`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: which subagents one hook checks);
escalate if the fix reaches build/bootstrap coordination. The hook still fails
open and sends a subagent back at most once. Never embed transcript paths or
home paths.

## Acceptance criteria
- A `researcher` handoff at or over the cap in
  `.agents/references/subagent-handoff.md` `## Handoffs` is sent back once with
  the hook's over-cap message; one under the cap with the shared form is allowed
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
