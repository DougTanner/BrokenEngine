<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-23T18:10:12.358Z","dependsOn":[]} -->
# Canonical execution-card acceptance evidence

## Context

At the session baseline `613687a376bde229f3d734112161dcaeb2d977fc`, the
Change Workflow requires an execution card before Tier 2+ work (`AGENTS.md`,
Step 1), while the delegated handoff contract requires acceptance checks and
expected observations (`.agents/references/subagent-reporting.md`, `## Task
brief`). `/plan-audit` and `/verify-changes` each consume acceptance evidence,
but the shared contract does not define one canonical criterion/check/expected-
result record or the evidence tuple for conditional performance work. The
same acceptance fact can therefore be repeated in cards, briefs, and tables,
or a performance claim can be written without a reproducible stop condition.

This is a pre-existing workflow gap outside the active implementation boundary:
the current change does not require a new acceptance contract. The originating
gap is the execution-card requirement and the Step 7 mapping of every approved
criterion to decisive evidence; the current owners name those obligations but
leave the record shape open.

## Design

The recommended change is a single shared acceptance record used by execution
cards, delegated briefs, review scopes, and acceptance tables. Record each
criterion once as:

`criterion` — `decisive check` — `expected result`.

Other workflow documents reference that record instead of restating it. When a
criterion is conditional performance work, require one tuple containing all of
the following before the work starts: metric, baseline, fixed workload and
configuration, reproducible command or scenario, evidence-grounded threshold,
and the stop rule that ends the attempt when the threshold cannot be settled.
Do not invent a threshold from an unmeasured preference.

Allow an optional pre-review phase that runs existing applicable checks before
an independent review. Record its result once and reuse it unless a relevant
input changes; a changed input invalidates that reuse and requires the check
again. This phase may not replace the independent review or acceptance gate.

## Critical files

- `.agents/references/subagent-reporting.md` — shared task-brief and handoff
  acceptance fields.
- `.agents/skills/plan-audit/SKILL.md` — execution-card review inputs.
- `.agents/skills/verify-changes/SKILL.md` — acceptance-table evidence mapping.
- `.agents/skills/implement-plan/SKILL.md` — implementation brief and check
  handoff.

## In scope

- Define the one criterion/check/expected-result record and the conditional
  performance tuple in the four critical files' existing acceptance-card,
  brief, review-input, and acceptance-table sections.
- Define the optional pre-review reuse rule and its relevant-input invalidation
  rule in the same sections.
- Keep the existing reviewer roles, independent-review requirement, landing
  gate, and user approval semantics unchanged.

## Out of scope

- Runtime or engine performance instrumentation, a new benchmark harness, or a
  new persistence or scoring system.
- Changing acceptance criteria for an individual existing Plan or feature.
- Model routing, credential handling, worktree containment, or landing-lock
  behavior.
- Unit tests, build/bootstrap changes, determinism/CRC, replay, wire,
  serialization, data-layout, shader, or live-game behavior.

## Risk tier and invariants

Expected future Change Workflow Tier 2 — scoped workflow behavior. The trigger
is the new canonical acceptance evidence contract used by the planning and
verification skills; it does not touch engine state, network data, or build
coordination. The criterion/check/expected-result record must remain complete,
performance tuples must remain reproducible and evidence-grounded, and a
pre-review result must never stand in for an independent review or landing
gate.

## Acceptance criteria

- Every execution card and acceptance table has one record per approved
  criterion, with criterion, decisive check, and expected result present once;
  repeated references point to that record rather than creating a second
  authority.
- Every conditional performance criterion carries metric, baseline, fixed
  workload/configuration, repro command or scenario, evidence-grounded
  threshold, and stop rule; an incomplete tuple is rejected before review.
- An optional pre-review records existing-check results once, reuses them only
  when relevant inputs are unchanged, and forces a recheck when one changes;
  the independent review and acceptance gate still run.
- The affected skill checks and static link/reference checks pass, and Plan
  validation exits `0` with `status: valid` and `code: ok`.
- No unit tests are added.

## Coordination

- None. The change is a shared documentation/skill contract and has no
  directional prerequisite or reciprocal implementation dependency.

## Notes

The recommendation is intentionally limited to the current owners of execution
cards and acceptance evidence. If implementation discovers that performance
measurement requires a new executable or build/bootstrap coordination, that
surface must be split into a separately reviewed Tier 3 stage rather than
expanded here.
