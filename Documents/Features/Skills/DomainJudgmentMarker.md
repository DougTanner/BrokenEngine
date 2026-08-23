# Domain judgment marker

Revisit When: A Tier 2 or Tier 3 change reaches a legal/security risk-tolerance
or player-facing taste decision that ordinary repository evidence cannot settle
and a user can name the owner who should decide it.

## Context

The repository's reviews settle code, workflow, and observable acceptance
questions, but some decisions are intentionally human judgments. The current
contracts do not give a manager a precise, auditable marker for the narrow case
where evidence is insufficient and a user-designated owner must decide. A
generic approval note would be too broad and could be mistaken for a review
result. This Feature adds a conditional marker without weakening existing
reviews.

## Design

Add an optional `Domain judgment` marker to the execution-card and review
handoff vocabulary. It is valid only when all of these fields are present:

- `Domain` — `legal`, `security`, or `player-facing taste`.
- `Decision` — the concrete choice still open.
- `Why ordinary evidence cannot settle it` — the evidence gap, not a generic
  request for preference.
- `User-designated owner/role` — the user-named person or role authorized to
  decide.
- `Evidence` — the repository or review evidence already considered, with
  unresolved alternatives kept visible.

The marker records a pending human decision and routes it to the named owner;
it does not decide the value, substitute for a correctness review, suppress a
security/legal check, or close an acceptance or landing gate. No other domain
may use this marker. Existing plan audit, correctness, scope, acceptance,
security, and landing reviews continue to run and retain their authority.

## Critical files

- `.agents/references/subagent-reporting.md` — marker fields and handoff state.
- `.agents/skills/plan-audit/SKILL.md` — execution-card eligibility.
- `.agents/skills/external-grill-plan/SKILL.md` — unresolved judgment routing.
- `.agents/skills/verify-changes/SKILL.md` — gate behavior while a judgment is
  pending.

## In scope

- Define the conditional marker and its five required fields in the existing
  card, review, and handoff contracts.
- Restrict the marker to legal/security risk tolerance and player-facing taste.
- Route the marker to the user-designated owner/role while leaving existing
  reviews and gates active.

## Out of scope

- Automatic decisions, model preferences, generic product management, staffing
  approvals, or a marker for ordinary technical uncertainty.
- Bypassing correctness, scope, security, acceptance, build, runtime, or
  landing reviews.
- Persistent owner databases, scores, telemetry, credentials, or external
  service mutation.
- Unit tests, engine/runtime code, determinism/CRC, replay, wire,
  serialization, data layout, shaders, or build/bootstrap changes.

## Risk tier and invariants

Expected future Change Workflow Tier 2 — scoped planning/review behavior. The
marker is conditional, evidence-linked, and user-owned; it cannot be emitted
without all five fields, cannot be used outside the three named domains, and
cannot close an existing review or gate.

## Acceptance criteria

- A complete marker records domain, decision, evidence gap, user-designated
  owner/role, and evidence; an incomplete marker is rejected or reported as
  ordinary unresolved work.
- Legal/security risk-tolerance and player-facing taste scenarios route to the
  named owner, while a technical question outside those domains cannot use the
  marker.
- Existing reviews and acceptance/landing gates still run and remain open
  while the marker is pending.
- The marker never invents a decision, owner, or evidence, and existing static
  reference checks pass. No unit tests are added.
