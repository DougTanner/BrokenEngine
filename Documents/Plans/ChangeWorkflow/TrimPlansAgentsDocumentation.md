<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T22:53:55.253Z","dependsOn":[]} -->
# Trim Plans agent documentation

## Context

`Documents/Plans/AGENTS.md` measures 1,073 `bt-token-v1` tokens against a 1,000-token formula budget (zero direct code tokens and zero direct child documents), an advisory excess of 73. The small excess is concentrated in explanatory scheduler prose that repeats implications already established by the metadata and command contracts.

## Design

The author recommends tightening `## Git-backed scheduler` by merging repeated definitions of exclusion, claim expiry, and completion behavior while preserving every metadata, validation, command, and landing rule. Tighten the area-selection and scope-control prose in `## Plan files` without changing the four area mappings or the distinction among Plans, Features, and Investigations.

## Critical files

- `Documents/Plans/AGENTS.md`

## In scope

- Condense explanatory sentences in `## Git-backed scheduler` around invalid metadata, cycles, claims, and completion.
- Condense `## Plan files` prose around area ties, enforceable scope, dependencies, and decision completeness.

## Out of scope

- Scheduler metadata, selection, claim, validation, completion, rejection, dependency, or area-routing semantics.
- Scripts, WorktreeCli, other guidance, or other Plan files.

## Acceptance criteria

- Remeasure with `bt-token-v1`; reach the then-current budget or explain any unavoidable advisory excess in terms of operative scheduler rules.
- Preserve the byte-zero marker contract, all scheduler commands and outcomes, the four area mappings, required Plan sections, and enforceable scope/dependency rules.
- Run the scheduler-state validation required by this document after the edit.

## Classification

Tier 1 — documentation-only condensation with no scheduler behavior change.

## Coordination

None. The trim does not depend on the code-scaled-budget Plan.

## Notes

The budget is advisory; operative scheduler guidance takes priority over the target.
