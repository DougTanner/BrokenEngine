<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T22:55:09.498Z","dependsOn":[]} -->
# Trim game Network Client agent documentation

## Context

`Projects/BrokenEngineSandbox/Source/Network/Client/AGENTS.md` measures 1,518 `bt-token-v1` tokens against a 1,219-token formula budget (6,271 direct code tokens, zero direct child documents), an advisory excess of 299. The document repeats engine reconciliation flow and architecture detail while also stating the game policy that belongs here.

## Design

The author recommends preserving game-owned hydration, reset, subscription, and desync policy while reducing engine-owned rollback/ring mechanics to the precise obligations the game layer must observe plus links to engine Client and GameReconciliation authorities. Consolidate the three opening ownership bullets, which currently overlap.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Client/AGENTS.md`

## In scope

- Merge overlapping ownership statements in `## Ownership` into concise engine/game/reconciler boundaries.
- Condense `## Session Policy` flow explanations while retaining reset completeness, subscription order, adoption-gap recovery, and discovery restart behavior.
- Shorten `## Reconciliation Invariants` where engine Client or architecture already owns the replay/ring mechanics; preserve the game-side obligations and confirmed-desync policy.
- Remove redundant concluding and See Also prose.

## Out of scope

- C++, network behavior, hydration, reset order, subscriptions, reconciliation, CRC/desync policy, recovery, or debug-frame behavior.
- Editing engine Network Client or architecture documents.

## Acceptance criteria

- Remeasure with `bt-token-v1`; meet the formula budget or explain unavoidable operative excess.
- Game-owned hydration/reconciler/desync boundaries, full reset, subscription ordering, adoption overflow recovery, replay budget obligation, corrupt-handler boundaries, and confirmed-desync policy remain explicit.
- Detailed engine replay flow appears only at its linked authority.

## Classification

Tier 1 — documentation-only condensation.

## Coordination

None.

## Notes

Do not weaken recovery or desync escalation rules to reach the advisory number.
