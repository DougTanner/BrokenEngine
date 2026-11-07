<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-06T23:46:55.726Z","dependsOn":[]} -->
# Fix: /compile — the builder-only route omits checks that interleave file steps with builds

## Context

Observed symptom: `.agents/skills/compile/SKILL.md:12-13` says the skill runs
solely inside one delegated `builder`, and
`.agents/references/change-workflow.md:24`, `:172`, and `:185` give the same
builder-only route. `.agents/references/subagent-reporting.md:115` sends every
`Build required` row to a `builder`. Acceptance runs interleave file steps with
builds, and none of that text routes such a check.

In one `/next-plan` run, five `implementer` briefs set `Skill: /compile` as a
workaround, one of them stating that no builder is available between steps. An
`/implement-plan` worker also raised a role contradiction for its own DataPacker
build, because `.agents/skills/implement-plan/references/worker.md:73-75`
(step 12) and `:82` return compilation and separate-role work to the manager.
The cost was a repeated role workaround across five dispatches and one
contradiction residual.

The role limit is prose only. Under `.agents/scripts`, the skill script
directories, and `.claude`, `.codex`, and `.opencode`, the word `builder` appears
only in the role's own definition and permission entries and in one comment in
`.agents/skills/optimize-build-time/scripts/Measure-BuildTrace.ps1`, so nothing
ties `/compile`'s scripts to that role.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 62a5259c-d255-49c5-9589-a2e7c87d022e
- Worktree/branch UUID: bce2711e-800d-4a71-886c-78431dc6cb86
- Session branch: claude/bce2711e-800d-4a71-886c-78431dc6cb86
- Worktree: .claude\worktrees\BrokenEngine\bce2711e-800d-4a71-886c-78431dc6cb86
- Landing ref: claude/bce2711e-800d-4a71-886c-78431dc6cb86
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/RouteBuildsInsideInterleavedChecks.md`,
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
recorded client and conversation session ID. Then make the smallest fix inside
the `## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

The author recommends keeping `builder` as the default route and naming one
exception: a dispatch whose assigned check interleaves file steps with builds
runs `/compile` itself under its assigned role and reports `/compile`'s
`## Handoff` rows inside its own handoff. The rationale is that the check's
ordered steps stay in one context, where alternating `implementer` and `builder`
dispatches would each need a continuation capsule, and that five dispatches
already worked this way. An `/implement-plan` slice with no such check keeps
returning `Build required`, which settles the recorded contradiction without
changing its worker text.

## Critical files

- `.agents/skills/compile/SKILL.md`
- `.agents/references/change-workflow.md`

## In scope

- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `/compile` `## Purpose` and the
  `builder` row of `### Delegation roles` and the `builder` bullets of
  `#### Step 4 — Implement and propagate` and
  `#### Step 5 — Run targeted pre-review checks` in `change-workflow.md`

## Out of scope

- The landed change the session produced
- `/compile` build steps, scripts, targets, and data-mode behavior
- `/implement-plan` worker text and the `Build required` route to a `builder`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2. Trigger: one subsystem's tool behavior, namely which role runs
`/compile` for one kind of check. Escalate to Tier 3 if the fix reaches
build/bootstrap coordination that can block other sessions. No determinism,
CRC, serialization, replay, wire, threading, affinity, allocation, or shader
exposure. Never embed transcript paths or home paths.

## Acceptance criteria

- `/compile` `## Purpose` and `change-workflow.md` state the same route for a
  check that interleaves file steps with builds, so a brief for such a check
  needs no role workaround and a worker running it has no role contradiction to
  raise
- A `Build required` handoff still routes to a `builder`
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
