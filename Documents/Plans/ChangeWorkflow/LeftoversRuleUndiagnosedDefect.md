<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-29T15:16:36.116Z","dependsOn":[]} -->
# Fix: change-workflow.md Step 8 Leftovers rule — no direction for a proven but undiagnosed leftover

## Context
Observed symptom: during a `/next-plan` run's Implement and propagate step
(pre-edit baseline builds), a pre-existing server Release|x64 build failure
(compiler errors C2220 and C28020) surfaced in the baseline builds. The
Leftovers paragraph of `.agents/references/change-workflow.md`
`#### Step 8 — Verify the acceptance table` (lines 244-263 at commit
b56f20fa) decides between fixing a leftover inside the current change and
routing it to `/create-follow-up-plans` by whether it is small: "Tier 1 or 2,
bounded, decidable from evidence already in hand". It does not say what to do
when the leftover is proven to exist but its root cause is not yet diagnosed,
so neither size nor tier can be judged from evidence in hand.

Main dispatched `/create-follow-up-plans` before diagnosing the failure. The
resulting Plan carried a hypothesized root cause, and `/coherence-review` then
flagged it as not decision-complete (`Documents/AGENTS.md` `## Planning Trees`;
`.agents/skills/create-follow-up-plans/references/worker.md` `### 1. Prove and
consolidate proposals` step 2 requires a confirmed root cause). Main then ran
`/external-diagnose-bug`, which found a proven one-line fix; main applied the
Leftovers rule to fix it inside the change and `git rm`-ed the superseded
follow-up Plan. Rework forced: one wasted `/create-follow-up-plans` dispatch,
one review finding, one Plan deletion, and extra continuation-capsule notes.
No acceptance criterion of the active change was left unmet.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: a352993b-e7e5-4aef-8478-c81c4cef70cf
- Worktree/branch UUID: d208ab4f-a588-4e2d-aee1-1dbfc8e3f4ce
- Session branch: claude/d208ab4f-a588-4e2d-aee1-1dbfc8e3f4ce
- Worktree: .claude\worktrees\BrokenEngine\d208ab4f-a588-4e2d-aee1-1dbfc8e3f4ce
- Landing ref: claude/d208ab4f-a588-4e2d-aee1-1dbfc8e3f4ce, the observing
  session's branch, whose tip is that session's final commit and which survives
  exactly as long as the worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/LeftoversRuleUndiagnosedDefect.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref —
supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author's recommendation, for the fix session to confirm: the Leftovers
paragraph states that a proven leftover whose root cause is not yet confirmed
is diagnosed through `/external-diagnose-bug` before the fix-now versus
`/create-follow-up-plans` choice is made, because both outcomes need the
confirmed root cause — the size test cannot be applied without it, and a
follow-up Plan must carry it.

## Critical files
- `.agents/references/change-workflow.md` — the authorized fix boundary

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to
  `.agents/references/change-workflow.md` `#### Step 8 — Verify the acceptance
  table`: its Leftovers paragraph, and the `/create-follow-up-plans` order line
  and dispatch bullet above it only if the fix makes them incorrect

## Out of scope
- The landed change the session produced
- `.agents/skills/create-follow-up-plans/` and `.agents/skills/external-diagnose-bug/`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation:
  the Leftovers paragraph tells main whether and how to diagnose a proven but
  undiagnosed leftover before choosing between fixing it in the current change
  and routing it to `/create-follow-up-plans`
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
Change Workflow tier trigger: scoped tool behavior (workflow instruction prose
for one rule), Tier 2 per `.agents/references/risk-tiers.md`.
