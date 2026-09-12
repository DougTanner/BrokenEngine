<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-12T17:59:13.481Z","dependsOn":[]} -->
# Fix: create-follow-up-plans — the dispatcher-facing SKILL.md never names the mandated Plan-file writer

## Context
`.agents/skills/create-follow-up-plans/SKILL.md` is the page a dispatching main
reads when composing a brief for this skill. Its `## Purpose`, `## When to use`,
`## Inputs`, and `## Handoff` sections say nothing about how the Plan file is
written; only the private `references/worker.md` (lines 28-34, inside step 2,
and the `New Plan` row of the step 3 table at lines 40-42) names
`.agents/scripts/New-PlanFile.ps1` as the writer, backed by
`.agents/references/new-plan-file.md`, which states that the script's marker,
timestamp, encoding, dependency, filename, overwrite, staging, and validation
operations are never to be reconstructed inline. `SKILL.md` `## References`
marks `references/worker.md` private — read only by the session executing the
skill — so a dispatching main has no cue in the page it reads and can name a
conflicting writer in its brief.

Observed symptom in this session: main's brief for a `/create-follow-up-plans`
dispatch instructed the worker to create the Plan file with the host `Write`
tool. The worker, reading `references/worker.md`, had to deviate from its brief
and spend handoff `Residuals` space justifying the deviation, because the skill
and `.agents/references/new-plan-file.md` mandate `New-PlanFile.ps1` as the only
writer.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: d092f285-4dec-482d-b799-56eaffb06339
- Worktree/branch UUID: 9e7420b2-65cd-4b8a-a796-8fabd4cc0e63
- Session branch: claude/9e7420b2-65cd-4b8a-a796-8fabd4cc0e63
- Worktree: .claude\worktrees\BrokenEngine\9e7420b2-65cd-4b8a-a796-8fabd4cc0e63
- Landing ref: the session branch above. The recording session had not landed its
  change when this Plan was written, so the branch tip named here contains this
  Plan only once that session's work is committed to it; until then the Plan
  exists as tracked content in the worktree above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`;
the cited sections should be sufficient, so the transcript is not expected to be
needed. Only when it genuinely is, in a new session run
`/next-plan-review <landing ref above>` in bounded friction mode, supplying
client `claude` and the recorded conversation session ID. Then make the smallest
fix inside the `## In scope` boundary below. If root-causing shows the fix lies
outside that boundary, surface it for re-planning instead of expanding scope.

The author's recommendation is one sentence of dispatcher-facing prose in
`SKILL.md` stating that the worker creates the Plan file through this skill's
bundled script and that a brief must not name a writer, with the mechanics —
script path, parameters, result handling — staying in `references/worker.md` and
`.agents/references/new-plan-file.md` so the fact keeps living once at its owning
layer. Which existing section carries that sentence, and whether it names the
script path or refers to it as the skill's bundled script, is a wording choice
for the fix session, as long as the mechanics are not restated in `SKILL.md`.

## Critical files
- `.agents/skills/create-follow-up-plans/SKILL.md` — the dispatcher-facing
  sections a composing main reads

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to
  `.agents/skills/create-follow-up-plans/SKILL.md`

## Out of scope
- The landed change the session produced
- `.agents/skills/create-follow-up-plans/references/worker.md` and
  `.agents/references/new-plan-file.md`, which already state the writer mandate
  and its mechanics correctly
- `.agents/scripts/New-PlanFile.ps1` and its behavior
- `.agents/references/subagent-reporting.md` brief form, and any other skill's
  dispatcher-facing prose
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1: the fix is documentation-only skill prose with no runtime, tool,
or script behavior exposure. Escalate if root-causing shows the fix must change
the script or the shared brief form. Never embed transcript paths or home paths.
The added sentence must not restate the mechanics that live in
`references/worker.md` and `.agents/references/new-plan-file.md`.

## Acceptance criteria
- `.agents/skills/create-follow-up-plans/SKILL.md` states, in its
  dispatcher-facing prose, that the worker creates the Plan file through the
  skill's bundled script and that a brief must not name a writer
- The script path, parameters, and result handling remain only in
  `references/worker.md` and `.agents/references/new-plan-file.md`
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing
