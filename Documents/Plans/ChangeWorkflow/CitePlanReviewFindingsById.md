<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-22T20:14:06.551Z","dependsOn":["Documents/Plans/ChangeWorkflow/FixNextPlanBriefInlineRepetition.md"]} -->
# Fix: .agents/skills/next-plan — cite Plan review findings by ID in Tier-3 briefs

## Context

Two recorded Tier-3 incidents in `WorkflowIncidents.md` under owner
`.agents/skills/next-plan` share the symptom that main paraphrases Plan review
findings inline in `/external-grill-plan` briefs instead of citing `Temp/`
review files by finding ID:

- 2026-11-22 (`agy/bfc232ef-cb23-48db-a353-2bd2f14023ec`): selector
  `/external-grill-plan` brief decisions list; main copied finding text inline;
  bounding: cite `Temp/` audit output files by finding ID with decisions only.
- 2026-11-22 (`agy/690eea56-ca81-49c6-9d35-f7503f68e7e9`): selector
  `/external-grill-plan` continuation brief; main paraphrased audit findings
  inline; bounding: keep decisions by finding ID and have the `planner` read
  cited `Temp/` audit outputs.

A third recorded incident under that key (2026-11-22,
`agy/690eea56-ca81-49c6-9d35-f7503f68e7e9`, selector `planner` briefs applying
`PA-F-001`-`007` and `PSR-F-001`-`006` during a Tier-2 run) belongs to
prerequisite
`Documents/Plans/ChangeWorkflow/FixNextPlanBriefInlineRepetition.md`'s Step 3
work, which owns Tier 1-2 Plan review write-in and difference
`/plan-simplicity-review` citations in `.agents/skills/next-plan/SKILL.md`
(Step 5), `.agents/references/change-workflow.md` (`#### Step 3 — Plan
review`), and `.agents/skills/plan-simplicity-review/SKILL.md`. This Plan stays
focused strictly on Tier-3 `/external-grill-plan` briefs: current source
inspection confirms `.agents/skills/next-plan/references/tier3-workflow.md`
(`## Plan review` `:23-39`) lies outside that prerequisite Plan's scope and
directs main to return accepted Plan review findings to the preparation
`planner` for `/external-grill-plan` without stating to pass finding IDs and
`Temp/` review output paths instead of paraphrasing findings inline.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:

- Client: agy
- Conversation session ID: b4bc361b-7bec-4239-9390-63aa1cea4e5d
- Worktree/branch UUID: 690eea56-ca81-49c6-9d35-f7503f68e7e9
- Session branch: agy/690eea56-ca81-49c6-9d35-f7503f68e7e9
- Worktree: `.agy\worktrees\BrokenEngine\690eea56-ca81-49c6-9d35-f7503f68e7e9`
- Landing ref: `agy/690eea56-ca81-49c6-9d35-f7503f68e7e9`
  The review ref the invoker passes to `/next-plan-review` has two cases. When
  the `Observed in an earlier session` line is present, it is that line's
  landed commit hash, with no lookup. When that line is absent, the recording
  session is the observing session, and worktree reuse moves its branch past
  its landing, so the invoker never reviews the ref itself: it resolves the ref
  to the commit that added this Plan with
  `git log --follow --diff-filter=A -n 1 --format=%H <ref> -- <this plan path>`
  and passes that commit. When the recorded ref is gone or that command prints
  nothing (a later session moved this Plan), it runs the same command with
  `<ref>` omitted. A periodic Plan-history squash can make either command
  return an unrelated aggregate commit, and a landing that also deletes a
  similar file can make it return another session's commit, so review the
  result only when the commit is attributable to the observing session recorded
  above alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before `/cleanup-worktrees` removes the worktree recorded
  above: Codex discovery requires a registered worktree, and Claude or `agy`
  review requires the conversation session ID above.

## Design

First root-cause the friction from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session
run `/next-plan-review <review ref>` in bounded friction mode, with the review
ref the provenance block above defines, supplying the recorded client and
conversation session ID. Then make the smallest fix inside the `## In scope`
boundary below so Tier-3 Plan review and `/external-grill-plan` continuation
briefs pass accepted finding IDs (and any decision overrides) as selectors into
the cited `Temp/` review output files rather than paraphrasing findings inline,
matching the Step 3 citation rule without duplicating prose across layers. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files

- `.agents/skills/next-plan/references/tier3-workflow.md` — `## Plan review`
  (`:23-39`).

## In scope

- Root-cause investigation as `## Design` states.
- The smallest resulting fix, confined to
  `.agents/skills/next-plan/references/tier3-workflow.md` (`## Plan review`).

## Out of scope

- Tier 1-2 Plan review write-in, difference `/plan-simplicity-review`, and
  Step 4 pre-edit rules in `.agents/skills/next-plan/SKILL.md` (Step 5, Step 8,
  and `## Handoff`), `.agents/references/change-workflow.md` (`#### Step 3 —
  Plan review` and `#### Step 4 — Implement and propagate`), and
  `.agents/skills/plan-simplicity-review/SKILL.md` (owned by
  `Documents/Plans/ChangeWorkflow/FixNextPlanBriefInlineRepetition.md`).
- The landed change the session produced.
- Unrelated skills/scripts; any transcript path or transcript text in the repo.
- Any C++, shader, Visual Studio project, or `WorktreeCli` file.

## Risk tier and invariants

Expected Tier 2 (scoped tool/workflow behavior: Tier-3 Plan review and grill
briefing rules in `.agents/skills/next-plan/references/tier3-workflow.md`);
escalate if the fix reaches build/bootstrap coordination. No determinism/CRC,
wire, serialization, `.pack`/`kiVersion`, replay, affinity, threading,
allocation, shader, build, trust-boundary, or live `/agent-harness` exposure.
Never embed transcript paths or home paths.

## Acceptance criteria

- The recorded Tier-3 symptom no longer reproduces under the documented
  invocation: `.agents/skills/next-plan/references/tier3-workflow.md`
  `## Plan review` directs main to cite `Temp/` review output paths plus
  finding IDs and decisions in Tier-3 `/external-grill-plan` briefs rather than
  paraphrasing findings inline.
- `pwsh -NoProfile -File .agents/skills/external-skill-creator/scripts/Validate-Skill.ps1 -Path .agents/skills/next-plan`
  exits `0` (`VALID`) when `.agents/skills/next-plan` is edited.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing.

## Notes

- Live `/agent-harness` verification does not apply.
