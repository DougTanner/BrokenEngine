<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T00:05:39.730Z","dependsOn":[]} -->
# Fix: Change Workflow — main reads source to settle a handoff Residual conflict

## Context
During a `/next-plan` run's Implement and propagate step, an `/implement-plan`
slice handoff returned `Residuals` naming both sides of two conflicts between
the Plan and documented rules: the Tweaks slider order rule in
`Engine/Source/Ui/Screens/TweaksScreen/AGENTS.md` (slider order follows the
declaration order of the wrappers in `Engine/Source/Ui/ShadowWrappersBase.h`)
and the label rule in `Engine/Source/Ui/AGENTS.md`, plus which side the worker
trusted. To decide the order conflict, main itself
ran a `grep` over `ShadowWrappersBase.h` and `TweaksScreen/AGENTS.md`, then a
`Read` of `ShadowWrappersBase.h` (offset 8, limit 20). The result size was not
measured. A `researcher` brief naming that header and the two AGENTS.md rules
could have confirmed the conflict, keeping the source read out of main's
context.

The routing text main follows gives no in-session route for a Residual that
main must decide: `.agents/references/subagent-reporting.md` `## Handoffs`
sends `Residuals` only to the message footer or to `/create-follow-up-plans`,
and `.agents/references/change-workflow.md` `### Resolving Ambiguity` says a
question a code read settles is settled "through the role that runs it" but
is not linked from handoff routing. Which of these, if either, is the root
cause is not yet confirmed.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 8504fefa-c048-4677-b236-bf12a2a1571b
- Worktree/branch UUID: b49ef72a-1587-450b-9dde-f92a459122ce
- Session branch: claude/b49ef72a-1587-450b-9dde-f92a459122ce
- Worktree: .claude\worktrees\BrokenEngine\b49ef72a-1587-450b-9dde-f92a459122ce
- Landing ref: claude/b49ef72a-1587-450b-9dde-f92a459122ce
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a
  periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session
run `/next-plan-review <review ref>` in bounded friction mode — the landing ref
— supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. The author
recommends one sentence in the `Residuals` row of
`.agents/references/subagent-reporting.md` `## Handoffs`: a Residual
contradiction main must decide in-session, which a code read settles, goes to
a `researcher` brief naming the cited files and rules, per
`.agents/references/change-workflow.md` `### Resolving Ambiguity`, instead of
main reading the source. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

## Critical files
- `.agents/references/subagent-reporting.md`
- `.agents/references/change-workflow.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `Residuals` row of
  `.agents/references/subagent-reporting.md` `## Handoffs` and the last bullet
  of `.agents/references/change-workflow.md` `### Resolving Ambiguity`

## Out of scope
- The landed change the session produced
- `.agents/skills/implement-plan/` and the handoff form in
  `.agents/references/subagent-handoff.md`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: main-session routing of handoff
fields); escalate if the fix reaches build/bootstrap coordination. Never embed
transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation:
  the routing text main follows sends a handoff Residual contradiction that a
  code read settles to a delegated `researcher`, not to a main-session read
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
Recorded from a `/next-plan` run checkpoint isolation finding (class
fixable-defect).
