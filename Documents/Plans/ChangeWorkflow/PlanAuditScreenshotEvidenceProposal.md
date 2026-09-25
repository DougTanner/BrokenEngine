<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-25T00:04:52.273Z","dependsOn":[]} -->
# Fix: /plan-audit worker — proposes screenshot evidence for values no harness query exposes

## Context
Observed symptom: while `/next-plan` was running a claim of
`Documents/Plans/Engine/ProfileFramesMapBounds.md`, a `/plan-audit` finding (PA-F-001)
proposed a harness screenshot as the positive evidence for Frames-profile overlay text
that no harness query exposes. Preparation's execution card had already said, correctly,
that pixels could not settle that text. The audit's replacement check was adopted.
After implementation, the `/agent-harness` run returned criteria C1-C4 `BLOCKED` because
the pixel-evidence prohibition forbids settling a criterion from a screenshot:
`.agents/skills/agent-harness/SKILL.md` `## Handoff` (the "Never fake state with pixel
guessing" paragraph), with the claimed-Plan case in `.agents/skills/next-plan/SKILL.md`
`## Rules` (the screenshot-only deferral bullet).

Cost: one full harness run (about 89k subagent tokens, 183 s) settled 1 of 5 criteria.
It also forced an extra user decision round and manual screenshot inspection. The gap
appeared after implementation, not during preparation.

Emitter: `.agents/skills/plan-audit/references/worker.md` `## Steps`. Step 21 checks
that each acceptance criterion has a decisive check. Step 22 requires "an achievable
replacement" for an unverifiable criterion. Neither step, and no `## Rules` line,
blocks a screenshot or pixel reading as the replacement for a value no harness query
exposes. The misbehaving skill is outside the claimed Plan's `## In scope`, which
covers only `FormatFramesMap` and the Frames-screen publication path.

Related lead, not in this Plan's scope: the harness worker named the narrowest
capability gap. Client `query_profile` (or `describe_ui`) could also return the current
profile page and the profile text-area strings (for example `kTextProfileFps` and
`kTextProfileMemory`), which would make overlay-text criteria query-settleable. That is
a harness capability addition, not this friction fix. It is recorded here only as
context for the main agent to route.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: bb605412-936e-48fa-8d9a-8c25b8d2e2c4
- Worktree/branch UUID: ce29ea70-2c0b-4ab2-95da-b80f1b956183
- Session branch: claude/ce29ea70-2c0b-4ab2-95da-b80f1b956183
- Worktree: .claude\worktrees\BrokenEngine\ce29ea70-2c0b-4ab2-95da-b80f1b956183
- Landing ref: claude/ce29ea70-2c0b-4ab2-95da-b80f1b956183 (the observing
  session records and lands this Plan itself; that branch's tip is the session's
  final commit and survives exactly as long as the worktree recorded above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/PlanAuditScreenshotEvidenceProposal.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above. OpenCode transcript review remains unsupported regardless of worktree
  retention.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/ce29ea70-2c0b-4ab2-95da-b80f1b956183` in bounded friction
mode, supplying client `claude` and conversation session ID
`bb605412-936e-48fa-8d9a-8c25b8d2e2c4`. Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

Recommended direction (the author's recommendation, not binding): in step 22 of the
worker, or in a single `## Rules` line, reject a screenshot or pixel reading as an
achievable replacement for a runtime value no harness query exposes. Such a criterion
then gets a must-fix finding that names the pixel-evidence prohibition and offers a
user decision, not a screenshot check. Link to the `/agent-harness` `## Handoff`
prohibition instead of restating it, so the rule has one owner.

## Critical files
- `.agents/skills/plan-audit/references/worker.md` — the emitter; the authorized fix
  boundary

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/plan-audit/references/worker.md`
  `## Steps` items 21-22 and its `## Rules` list

## Out of scope
- The landed change the session produced, including
  `Documents/Plans/Engine/ProfileFramesMapBounds.md` and its implementation
- `.agents/skills/agent-harness/SKILL.md` and `.agents/skills/next-plan/SKILL.md`,
  whose pixel-evidence rules are correct as written
- Any harness query extension, such as `query_profile` or `describe_ui` returning profile
  page and text-area strings
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one skill worker's audit rule); escalate if the
fix reaches build/bootstrap coordination. Never embed transcript paths or home paths.
The pixel-evidence prohibition keeps one owner, `/agent-harness` `## Handoff`.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: under the
  revised worker, a `/plan-audit` of a plan whose criterion depends on overlay text that
  no harness query exposes does not propose a screenshot as the replacement check
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing
