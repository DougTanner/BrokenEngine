<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-05T14:08:28.017Z","dependsOn":[]} -->
# Fix: subagent-reporting.md — brief composition prints `## Handoff` text no brief uses

## Context

Observed symptom: `.agents/references/subagent-reporting.md:81-85`, the `## Task
brief` guidance for composing a brief, suggests a section-bounded search such as
`Grep '^## (Inputs|Handoff)'` with trailing context. Main ran that search for
`/plan-audit`, `/plan-simplicity-review`, `/external-grill-plan`, and
`/adversarial-review` and printed both sections of each, 19,550 characters in
all. At the time of recording those four `## Handoff` sections hold 9,119
characters against 4,763 in their `## Inputs` sections.

No rework followed; the cost is main-session context. The brief form at
`subagent-reporting.md:22-35` needs no `## Handoff` text: `Return:` and `Required
sections:` can cite the skill's `## Handoff` by name, as briefs already do.
`## Handoff` opens by path only when routing a returned result needs it, for
instance the `## Handoff` fallback rule in `.agents/skills/codex-review/SKILL.md`,
which applies after a returned failure. `## Section placement` in
`.agents/references/skill-skeleton.md` keeps `Handoff` public for that reason.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 8efba1af-3a94-4fc9-834c-ff190a2db7f1
- Worktree/branch UUID: 059d7be2-5d08-4321-9713-a6986b2e0d61
- Session branch: claude/059d7be2-5d08-4321-9713-a6986b2e0d61
- Worktree: .claude\worktrees\BrokenEngine\059d7be2-5d08-4321-9713-a6986b2e0d61
- Landing ref: claude/059d7be2-5d08-4321-9713-a6986b2e0d61
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/BriefCompositionReadsInputsOnly.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design

First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying the
recorded client and the recorded conversation session ID. Then make the smallest
fix inside the `## In scope` boundary below. If root-causing shows the fix lies
outside that boundary, surface it for re-planning instead of expanding scope.

The author recommends narrowing the example to `^## Inputs` and stating in the
same sentence that `## Handoff` opens by path only when routing a returned result
needs it, because a section the brief never uses should not enter main's context.

## Critical files

- `.agents/references/subagent-reporting.md` — `## Task brief`

## In scope

- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the brief-composition sentence of
  `## Task brief` in `.agents/references/subagent-reporting.md`

## Out of scope

- The landed change the session produced
- `.agents/references/skill-skeleton.md` and any skill's `## Inputs` or
  `## Handoff` content
- The brief form and the `## Handoffs` table in `subagent-reporting.md`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 1: the highest trigger is documentation with no public signature or
invariant exposure, since one sentence of guidance changes and no script or skill
contract does. Determinism, wire, serialization, replay, affinity, threading,
allocation, shader, and build surfaces are not exposed; no live verification
applies. Never embed transcript paths or home paths.

## Acceptance criteria

- The recorded symptom no longer reproduces under the documented invocation: the
  guidance's search prints `## Inputs` alone
- `.agents` holds no remaining `(Inputs|Handoff)` search suggestion
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
