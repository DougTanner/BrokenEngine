<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T17:36:07.408Z","dependsOn":[]} -->
# Fix: /next-plan preparation handoff — returned handoffs exceed the handoff character cap

## Context
Observed symptom: during one `/next-plan` run, six preparation-continuation
`implementer` handoffs (dispatches briefed "Role: implementer Skill: none
(preparation continuation for a claimed /next-plan Plan)", returning the form
`.agents/skills/next-plan/SKILL.md` `## Handoff` declares) each exceeded the
character cap `.agents/references/subagent-handoff.md` `## Handoffs` sets,
measuring 4246 to 5269 characters. None applied that section's overflow rule
(move the full material of the over-cap fields to a file with one `##` heading
per section and cite each under `Evidence`). Measured field sizes per handoff:

- `Decisive checks`: 599 to 1767 characters, mostly line-cited read results
  (`path:line` reads of the snapshot and source) rather than command output.
- `Evidence`: roughly 650 to 1150 characters, mostly the one-row-per-`##`
  selector citations of the preparation snapshot that
  `.agents/skills/next-plan/references/preparation-brief.md` `## Snapshot`
  requires.
- `Verification`: roughly 290 to 1200 characters; `Slices`: roughly 510 to
  1400 characters. `Slices` is the only source of the slice partition main
  dispatches step 8 from, so it is effectively required inline, and the shared
  overflow rule never moves inline-required text.
- Two handoffs also carried free prose before the fenced form, and one carried
  an undeclared extra field; both are worker deviations from a correctly
  declared form and are not part of this Plan.

Removing `Decisive checks` alone would bring only some of the six under the
cap; the rest stay over on `Evidence`, `Verification`, and `Slices`. The
reviewer bounding proposal was to move the line-cited `Decisive checks` read
results into a snapshot section and cite them by path plus `##` heading under
the shared overflow rule. The same run's checkpoint review also observed ten
other skills' handoffs over the cap without the overflow rule applied
(`/plan-alternatives` researchers, `/plan-audit`, `/repo-code-review`,
`/adversarial-review`, an `/external-grill-plan` implementer, a sync
researcher, `/implement-plan`, `/update-affected-code`), which suggests the
root cause may sit in the shared reference rather than in `/next-plan` alone.
The rework forced: main consumed every over-cap handoff in full in its own
context instead of a bounded summary.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 9b0c4970-d298-4d92-bfa4-f2253d3517cd
- Worktree/branch UUID: 35f31600-f877-4044-86d4-1d7c6ccb9721
- Session branch: claude/35f31600-f877-4044-86d4-1d7c6ccb9721
- Worktree: .claude\worktrees\BrokenEngine\35f31600-f877-4044-86d4-1d7c6ccb9721
- Landing ref: claude/35f31600-f877-4044-86d4-1d7c6ccb9721
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a
  periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`: decide whether the cause is the `/next-plan` preparation handoff
shape (its declared fields plus the snapshot-selector `Evidence` rows leave no
room under the cap), or the shared overflow rule in
`.agents/references/subagent-handoff.md` `## Handoffs` (for example, workers
have no prompt to measure the handoff or no clear rule for read-result
`Decisive checks` rows), which the ten other over-cap skills point toward.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/35f31600-f877-4044-86d4-1d7c6ccb9721` in bounded
friction mode, supplying client `claude` and the conversation session ID
recorded above. Then make the smallest fix inside the `## In scope` boundary
below.

The author recommends, when the cause is the `/next-plan` shape, the reviewer
bounding above: line-cited `Decisive checks` read results go into a snapshot
section and the handoff cites it by path plus `##` heading, keeping only
recorded command results inline; and, when the cause is the shared rule, one
fix in the shared reference rather than per-skill edits, because ten other
skills show the same symptom. If root-causing shows the fix lies outside the
`## In scope` boundary (for example, per-skill `## Handoff` edits in the other
ten skills), surface it for re-planning instead of expanding scope.

## Critical files
- `.agents/skills/next-plan/SKILL.md` (`## Handoff`)
- `.agents/skills/next-plan/references/preparation-brief.md` (`## Snapshot`)
- `.agents/references/subagent-handoff.md` (`## Handoffs`)

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/next-plan/SKILL.md`
  `## Handoff`, `.agents/skills/next-plan/references/preparation-brief.md`
  `## Snapshot`, and `.agents/references/subagent-handoff.md` `## Handoffs`

## Out of scope
- The landed change the session produced
- `## Handoff` sections of the other ten over-cap skills; surface any needed
  per-skill change for re-planning
- Worker deviations from a correctly declared form (prose before the fenced
  form, undeclared extra fields)
- Changing the cap value itself
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2, trigger: scoped tool behavior (the `/next-plan` preparation
handoff contract and the shared handoff rule); escalate if the fix reaches
build/bootstrap coordination. `Slices` and `Candidate zero` stay inline, as
main dispatches from them. Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: a
  `/next-plan` preparation or preparation-continuation handoff for a Plan of
  comparable size stays under the cap, or applies the shared overflow rule
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
