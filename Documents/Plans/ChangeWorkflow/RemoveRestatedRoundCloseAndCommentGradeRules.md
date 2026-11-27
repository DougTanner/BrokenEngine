<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-27T03:32:33.343Z","dependsOn":[]} -->
# Remove two restated review-round and comment-grade rules

## Context

Two instruction files restate a rule another file owns, both added by the
landed review-round change (commit `94fa96136`):

- `.agents/references/change-workflow.md` `#### Step 4 — Implement and
  propagate` says "A round's close row waits until main has decided every
  finding its rows returned, `/verify-external-claims` included." `#### Step 6
  — Review and resolve correctness` owns that sequence: main runs
  `/verify-external-claims` before deciding the dependent finding, decides and
  regrades each finding the round's rows return, and closes the round with the
  `-CloseRound` call its close row prints.
- `.agents/skills/comment-review/references/worker.md` `## Rules` says
  "Introduce no severity beyond the `Medium` and `Low` that
  `comment-classes.md` assigns each class." `references/comment-classes.md`
  assigns each class its severity in its table, and `SKILL.md` `## Handoff`'s
  `Findings` row form lists `Medium|Low`.

## Design

Delete the Step 4 sentence, leaving Step 6 as the owner. Replace the worker
rule with an instruction to give each finding the severity `comment-classes.md`
assigns its class, without naming the grades.

Change Workflow tier: Tier 1 — documentation: two duplicated sentences. No
behavior, determinism, wire, or trust-boundary change; no C++, shader, build,
`.pack`, replay, or live-verification exposure.

## Critical files

- `.agents/references/change-workflow.md` (`#### Step 4 — Implement and
  propagate`)
- `.agents/skills/comment-review/references/worker.md` (`## Rules`)

## In scope

- The Step 4 close-row sentence.
- The worker's severity rule.

## Out of scope

- Step 6, `comment-classes.md`, and `SKILL.md`.

## Acceptance criteria

- Step 4 no longer states when a round's close row runs, and Step 6 still
  does.
- The worker names no grade and points to the severity `comment-classes.md`
  assigns.
- `Validate-Skill.ps1` returns `VALID` for `comment-review`.
- The static-checks runner passes for the changed files.
