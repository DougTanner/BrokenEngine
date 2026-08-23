# Next-plan review outcome evidence

Revisit When: Maintainers need comparable, cited outcome evidence from multiple
`/next-plan-review` runs to prioritize workflow improvements, while keeping the
review read-only and free of scores or persistent state.

## Context

`/next-plan-review` already reconstructs a landing timeline, reports wall-clock
span and active elapsed time, and distinguishes required controls from waits in
its measurement reference. Its output has no small optional outcome block that
consistently records active versus wall/excluded waits, acceptance failures and
rechecks, or runtime/harness outcome with citations. Adding a score or a gate
would turn a retrospective into a scheduler; adding time-to-first would create
an unrelated metric. This Feature fills only the evidence gap.

## Design

Add an optional `Outcome evidence` block to the existing read-only review
report. When the record is available, it contains:

- `Timing` — active elapsed time, wall-clock span, and excluded waits/passive
  intervals, each with its reason or `unverified` state.
- `Acceptance` — the count of observed acceptance failures and the count of
  rechecks, with cited evidence or `unverified`.
- `Runtime/harness` — the observed outcome (`pass`, `fail`, `not run`, or
  `unverified`) with the repository/handoff citation that settles it.
- `Evidence` — the exact cited source for each value, or an explicit
  `unverified` entry when no source exists.

The block remains optional: a review that did not run a harness or cannot
separate a wait reports `unverified` rather than estimating. It does not add
scores, priority values, persistence, scheduler claims, acceptance gates,
landing gates, or time-to-first measurements. Existing verdicts, findings, and
read-only provenance rules remain unchanged.

## Critical files

- `.agents/skills/next-plan-review/SKILL.md` — report shape and evidence rules.
- `.agents/skills/next-plan-review/references/measurement.md` — active,
  wall-clock, and excluded-wait definitions.
- `.agents/references/subagent-reporting.md` — cited handoff/evidence wording.

## In scope

- Add the optional `Outcome evidence` block with timing, acceptance,
  runtime/harness, and citation/unverified fields.
- Use the existing measurement and provenance definitions; report only values
  settled by cited evidence.
- Preserve the current read-only review, verdict, finding, and provenance
  contracts.

## Out of scope

- Scores, ranking, persistence, scheduler/Plan claims, acceptance gates,
  landing gates, or time-to-first metrics.
- New runtime instrumentation, harness commands, telemetry, or external
  service queries.
- Reinterpreting active work, waits, acceptance failures, or rechecks without
  evidence.
- Unit tests, engine/runtime code, determinism/CRC, replay, wire,
  serialization, data layout, shaders, or build/bootstrap changes.

## Risk tier and invariants

Expected future Change Workflow Tier 2 — scoped retrospective/report behavior.
The report stays read-only; every value is cited or `unverified`; excluded
waits remain distinct from active work; and no outcome field becomes a score or
workflow gate.

## Acceptance criteria

- A review with timing evidence records active, wall, and excluded waits with
  reasons; unavailable timing is explicitly `unverified`.
- Acceptance evidence records failure and recheck counts with citations or
  `unverified`, without manufacturing a count from prose.
- Runtime/harness outcomes use the four named states and carry a decisive
  citation when observed.
- The report contains no score, persistence, gate, or time-to-first field, and
  existing read-only provenance and verdict behavior remains unchanged.
- Existing static/reference checks pass. No unit tests are added.
