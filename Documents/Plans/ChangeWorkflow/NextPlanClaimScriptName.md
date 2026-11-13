<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T00:05:38.943Z","dependsOn":[]} -->
# Fix: /next-plan — unnamed "claim script" exclusion blocks the harness claim

## Context
During a `/next-plan` run, main dispatched an `/agent-harness` launch worker
whose brief carried the exclusion "never run any claim script". The worker
returned `BLOCKED`: another session's harness server held the single server
slot, and `.agents/skills/agent-harness/references/launch.md`
`## Normal launch` waits such a server out through
`.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1`, which the
worker read the exclusion as forbidding. Main then re-dispatched the launch
with a correction saying the exclusion meant only the `/next-plan` Plan claim
scripts (`Invoke-NextPlanClaim.ps1`, `Complete-NextPlan.ps1`,
`Defer-NextPlan.ps1`). Cost: one wasted launch dispatch.

The `/next-plan` text main paraphrased names the script only indirectly:
`.agents/skills/next-plan/SKILL.md` `## Rules` says "The preparation worker
must never run the mutation-capable claim script.", and step 6 refers to
"step 3's claim script"; only step 3 spells out
`.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1`. The word "claim"
also names the agent-harness claim, so a generalized "claim script" exclusion
collides with `Invoke-HarnessClaim.ps1`. Whether the fix belongs in the
`/next-plan` wording or in how main writes a brief's exclusions is not yet
confirmed.

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
recommends naming the script(s) the `/next-plan` prohibitions mean wherever
that skill says "claim script", so a paraphrased exclusion cannot reach
`Invoke-HarnessClaim.ps1`. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

## Critical files
- `.agents/skills/next-plan/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/next-plan/SKILL.md`
  `## Rules` (the mutation-capable claim script rule) and the step 6 reference
  to step 3's claim script

## Out of scope
- The landed change the session produced
- `.agents/skills/agent-harness/` and `Invoke-HarnessClaim.ps1` behavior
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: wording that governs `/next-plan`
worker briefs); escalate if the fix reaches build/bootstrap coordination.
Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation:
  every `/next-plan` prohibition on a claim script names the script it means,
  and none can be read as covering `Invoke-HarnessClaim.ps1`
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
Recorded from a `/next-plan` run checkpoint finding (class fixable-defect).
