<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-22T20:14:15.227Z","dependsOn":[]} -->
# Fix: .agents/skills/next-plan — bound delegated SKILL.md reads in main

## Context

Three recorded incidents in `WorkflowIncidents.md` under owner
`.agents/skills/next-plan` share the symptom that main reads a full delegated
skill `SKILL.md` instead of only `## Inputs` or its dispatch reference:

- 2026-11-22 (`agy/690eea56-ca81-49c6-9d35-f7503f68e7e9`): selector
  `.agents/skills/plan-audit/SKILL.md`; full skill read in main; bounding: main
  reads `## Inputs` for the brief and the `reviewer` reads the full skill.
- 2026-11-22 (`agy/690eea56-ca81-49c6-9d35-f7503f68e7e9`): selector
  `.agents/skills/plan-simplicity-review/SKILL.md`; 5,846 chars entered main;
  bounding: main reads `## When to use` and `## Inputs` and the `reviewer`
  reads the full skill.
- 2026-11-22 (`agy/690eea56-ca81-49c6-9d35-f7503f68e7e9`): selector
  `.agents/skills/next-plan-checkpoint-review/SKILL.md`; full checkpoint skill
  read in main; bounding: use `references/run-checkpoint.md` `## Dispatch` and
  leave the full skill to the `reviewer`.

Current source inspection confirms `.agents/skills/next-plan/SKILL.md`
explicitly bounds main's reads in Step 4 (`:84-92`) for the claimed Plan and
`## Task brief`, while Step 5 (`:133-138`) and Step 9 (`:202-212`) plus
`.agents/skills/next-plan/references/run-checkpoint.md` (`## Dispatch` `:9-16`)
do not surface the section-bounded skill-read rule in
`.agents/references/subagent-reporting.md` (`## Task brief` `:87-92`) or state
that `run-checkpoint.md` `## Dispatch` alone suffices without opening
`next-plan-checkpoint-review/SKILL.md`.

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
boundary below so `/next-plan` dispatches read only the required public
sections (`## When to use`/`## Inputs`) or `run-checkpoint.md` `## Dispatch`
instead of pulling full delegated `SKILL.md` files into main. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

## Critical files

- `.agents/skills/next-plan/SKILL.md` — Steps 4, 5, and 9 (`:64-138`,
  `:202-212`).
- `.agents/skills/next-plan/references/run-checkpoint.md` — `## Dispatch`
  (`:9-16`).
- `.agents/references/subagent-reporting.md` — `## Task brief` (`:81-97`), if
  root-cause analysis shows the general section-bounded skill-read rule needs
  tightening alongside `/next-plan`.

## In scope

- Root-cause investigation as `## Design` states.
- The smallest resulting fix, confined to `.agents/skills/next-plan/SKILL.md`
  (Steps 4, 5, and 9),
  `.agents/skills/next-plan/references/run-checkpoint.md` (`## Dispatch`), and
  `.agents/references/subagent-reporting.md` (`## Task brief`).

## Out of scope

- The landed change the session produced.
- Unrelated skills/scripts; any transcript path or transcript text in the repo.
- Any C++, shader, Visual Studio project, or `WorktreeCli` file.

## Risk tier and invariants

Expected Tier 2 (scoped tool/workflow behavior: delegated-skill briefing and
read-bounding rules in `.agents/skills/next-plan` and
`.agents/references/subagent-reporting.md`); escalate if the fix reaches
build/bootstrap coordination. No determinism/CRC, wire, serialization,
`.pack`/`kiVersion`, replay, affinity, threading, allocation, shader, build,
trust-boundary, or live `/agent-harness` exposure. Never embed transcript paths
or home paths.

## Acceptance criteria

- The recorded symptom no longer reproduces under the documented invocation,
  distinguished from baseline: the edited in-scope instructions close the
  baseline omission at the Plan review and checkpoint dispatch sites, and a
  recorded verification of the reads main actually performs when assembling
  both the Plan review briefs (`/plan-audit`, `/plan-simplicity-review`) and
  the `/next-plan-checkpoint-review` brief under the edited steps shows only
  `^## ` heading lookup plus `## When to use` and `## Inputs` range reads for
  Plan review skills and zero reads of `next-plan-checkpoint-review/SKILL.md`
  (no full-file delegated `SKILL.md` reads in main).
- `pwsh -NoProfile -File .agents/skills/external-skill-creator/scripts/Validate-Skill.ps1 -Path .agents/skills/next-plan`
  exits `0` (`VALID`) when `.agents/skills/next-plan` is edited.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing.

## Notes

- No live Plan dependencies or reciprocal `## Coordination` constraints apply.
- Live `/agent-harness` verification does not apply.
