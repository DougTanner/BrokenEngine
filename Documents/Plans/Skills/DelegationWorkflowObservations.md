<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-23T18:10:23.123Z","dependsOn":[]} -->
# Canonical workflow observations before residuals

## Context

At the session baseline `613687a376bde229f3d734112161dcaeb2d977fc`, delegated
handoffs place `Residuals` last (`.agents/references/subagent-reporting.md`,
`## Handoffs`), and the Change Workflow requires main to route worker results
and reconcile them. The shared contract does not provide a canonical
observation record before residuals, so a command result, path, effect, and
category can be restated in prose or inferred during reconciliation. That
creates avoidable discovery and makes it unclear which observations were
actually reported by the worker.

This is a pre-existing, out-of-scope workflow gap. Its originating criterion is
the manager's required routing/reconciliation of closed handoffs; no active
acceptance failure is being moved out of the current change.

## Design

The recommended handoff shape adds `Workflow observations` immediately before
`Residuals`. It is either the literal state `none` or one row per observed
item. Each row carries exactly these fields: category, command or repository
path, observed result, and effect. A row records what was seen, not an
interpretation of an unrun check.

Main consumes and routes those rows as evidence. Reconciliation accepts only
closed handoff schemas that contain the required fields; it does not create a
new observation by combining partial prose, guessing a command, or converting
an absent result into `none`. Existing status, build, executor, and residual
fields remain unchanged, and observations never bypass an independent review,
build, harness, or landing gate.

## Critical files

- `.agents/references/subagent-reporting.md` — canonical handoff field order
  and observation schema.
- `.agents/skills/implement-plan/SKILL.md` — worker observation production.
- `.agents/skills/verify-changes/SKILL.md` — read-only acceptance evidence
  reconciliation.
- `.agents/skills/create-follow-up-plans/SKILL.md` — residual routing after a
  closed handoff.

## In scope

- Define the `Workflow observations` section and its `none`/row form in the
  shared handoff contract.
- Require the four fields (category, command/path, observed result, effect) for
  every row and place the section before `Residuals`.
- Define main's routing and reconciliation rule so only closed, complete
  observation rows are consumed; preserve existing gates and required handoff
  fields.

## Out of scope

- Changing which worker performs a check, adding telemetry, or inventing a new
  workflow category.
- Changing residual eligibility, Plan duplicate rules, review verdicts, build
  semantics, harness behavior, or landing locks.
- Credential policy, model routing, worktree containment, or acceptance tuple
  design.
- Unit tests, builds, runtime game behavior, determinism/CRC, replay, wire,
  serialization, data layout, or shaders.

## Risk tier and invariants

Expected future Change Workflow Tier 2 — scoped workflow/handoff behavior. The
trigger is a new structured observation field consumed by managers and
follow-up routing. Observations must be worker-reported evidence, must be
complete or explicitly `none`, must precede residuals, and must never be used
to close a gate that its evidence does not settle.

## Acceptance criteria

- Every applicable handoff places `Workflow observations` before `Residuals` and
  emits either `none` or complete rows.
- Every row contains category, command/path, observed result, and effect; a
  missing field blocks reconciliation instead of being silently inferred.
- Main routes observations as evidence and reconciles only closed schemas;
  existing build/review/harness/landing gates remain authoritative.
- Static link/reference checks pass and Plan validation exits `0` with
  `status: valid` and `code: ok`.
- No unit tests are added.

## Coordination

- None. This owns handoff observation shape and does not depend on the separate
  acceptance-evidence, credential, or routing-parity changes.

## Notes

`Residuals` remains the final handoff field. An observation that proves an
accepted gap is still routed through the manager's existing follow-up workflow;
the new section only preserves the evidence that led there.
