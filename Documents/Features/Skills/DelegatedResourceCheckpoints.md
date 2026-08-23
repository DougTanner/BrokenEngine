# Delegated resource checkpoints

Revisit When: A user explicitly asks to cap a delegated slice or a repeated
workflow has a concrete need to stop at a user-stated time, attempt, or
evidence boundary.

## Context

Execution cards and delegated briefs already carry scope, acceptance checks,
and a return contract, but they have no optional user-controlled checkpoint or
hard ceiling. Agents therefore cannot report a user-selected boundary in the
same record without inventing a quota or treating a timeout as a workflow
verdict. This Feature adds a capability to the workflow layer; it does not
change engine execution or create agent telemetry.

## Design

Add two optional, user-supplied fields to execution cards and delegated briefs:

- `Checkpoint` — the exact user-supplied boundary at which the worker reports
  progress and the evidence it has, without inferring a new limit.
- `Hard ceiling` — the exact user-supplied stop boundary. It is absent unless
  the user supplies it; there is no default based on tokens, wall time, tool
  calls, or model quota.

When a worker reaches a hard ceiling, it stops the assigned slice and returns
`NEEDS_ACTION` with the checkpoint/ceiling text and observed evidence. The
manager may ask the user whether to continue or revise the boundary. A
checkpoint or ceiling never closes a review, build, runtime, acceptance, or
landing gate, and it never turns missing evidence into a pass or a residual.

The fields travel verbatim through the existing card, brief, and handoff
contracts. No token telemetry, agent-time accounting, tool quota, persistence,
score, or automatic resource enforcement is added.

## Critical files

- `.agents/references/subagent-reporting.md` — optional field and handoff shape.
- `.agents/skills/plan-audit/SKILL.md` — card validation and boundary review.
- `.agents/skills/implement-plan/SKILL.md` — worker stop and `NEEDS_ACTION`
  behavior.
- `.agents/skills/verify-changes/SKILL.md` — gate preservation when a ceiling
  is reached.

## In scope

- Add the optional `Checkpoint` and `Hard ceiling` fields to cards, briefs, and
  their closed handoffs.
- Define the user-supplied, verbatim semantics and the `NEEDS_ACTION` response
  at a reached hard ceiling.
- Preserve all existing review, build, runtime, acceptance, and landing gates.

## Out of scope

- Agent token telemetry, automatic quotas, model/service-tier controls,
  persistent budgets, scoring, or time-to-first metrics.
- Changes to engine code, runtime behavior, worktree or landing locks, or
  credential handling.
- Treating a checkpoint or ceiling as an acceptance result or a reason to skip
  an independent review.
- Unit tests, build/bootstrap changes, determinism/CRC, replay, wire,
  serialization, data layout, shaders, or game harness behavior.

## Risk tier and invariants

Expected future Change Workflow Tier 2 — scoped workflow behavior. The marker
is conditional and user-owned: absent fields add no behavior, supplied text is
not reinterpreted, `NEEDS_ACTION` leaves gates open, and no agent telemetry or
tool quota is introduced.

## Acceptance criteria

- A card and delegated brief can carry exact optional `Checkpoint` and `Hard
  ceiling` fields, and a closed handoff preserves them without inventing
  defaults.
- A worker that reaches a supplied hard ceiling returns `NEEDS_ACTION` with its
  observed evidence; it does not claim pass/fail, close a gate, or silently
  continue past the boundary.
- Existing review, build, runtime, acceptance, and landing gates remain open
  until their own evidence settles them.
- A scenario with no supplied fields behaves byte-for-byte as the current
  workflow, and a scenario with a supplied checkpoint reports it without
  telemetry, scoring, persistence, or quota output.
- Existing static/reference checks pass. No unit tests are added.
