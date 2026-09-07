<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T22:54:40.235Z","dependsOn":[]} -->
# Trim engine UI agent documentation

## Context

`Engine/Source/Ui/AGENTS.md` measures 2,034 `bt-token-v1` tokens against a 1,925-token formula budget (22,152 direct code tokens, one direct child document), an advisory excess of 109. Small repeated ownership and persistence explanations can be consolidated without removing UI contracts.

## Design

The author recommends merging repeated client/server guard and direct-include explanations, shortening settings-version history to current-format behavior, and replacing screen-specific duplication with links to `Screens/AGENTS.md`. Preserve wrapper consumption semantics, localization layout, persistence versions and rejection behavior, shared type boundaries, and the deliberate curve A/B state.

## Critical files

- `Engine/Source/Ui/AGENTS.md`

## In scope

- Condense repeated aggregation/guard ownership in `## Wrapper Contracts`, `## Localization`, `## Menu and Panel Helpers`, and the paragraph after `## Shared Types`.
- Tighten `## Settings Persistence` version-history rationale while retaining current versions, fields, rejection, defaults, and finite-value rules.
- Shorten cross-document ownership descriptions and `## See Also` entries.

## Out of scope

- C++, UI behavior, settings layouts/versions, localization, wrapper semantics, client/server guards, or renderer quality behavior.
- Editing Screens or game UI documentation.

## Acceptance criteria

- Remeasure with `bt-token-v1`; meet the budget or explain any unavoidable operative excess.
- Wrapper change tracking, persistence contracts, localization layout, shared/client-only type boundaries, network controls, and deliberate curve duplication remain explicit.
- Screen composition detail is linked to its child owner rather than repeated.

## Classification

Tier 1 — documentation-only condensation.

## Coordination

None.

## Notes

The modest target should be met through deduplication, not by removing current-format contracts.
