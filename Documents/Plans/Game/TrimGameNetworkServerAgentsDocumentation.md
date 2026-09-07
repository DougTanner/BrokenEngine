<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T22:55:18.035Z","dependsOn":[]} -->
# Trim game Network Server agent documentation

## Context

`Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md` measures 1,830 `bt-token-v1` tokens against a 1,518-token formula budget (14,809 direct code tokens, zero direct child documents), an advisory excess of 312. The document repeats engine server runtime, transfer, replay, and File contracts alongside the game-owned policy.

## Design

The author recommends keeping only the game session's ownership, deterministic hooks, fleet state, lifecycle, and trust obligations here, and replacing engine-owned transport/transfer mechanics with concise integration duties and direct links. Consolidate repeated fleet persistence and reconnect rationale without losing identifier, RNG, or save/replay rules.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md`

## In scope

- Condense the opening and `## State Ownership` by merging repeated engine/game boundaries and fleet-lifetime rationale.
- Tighten `## Timing and Paused Availability` to game-hook obligations, leaving engine pacing mechanics at the linked owner.
- In `## Deterministic Tick Contracts`, remove repeated engine transfer/replay mechanics while retaining game-side active-set, replay-input, ownership-relink, spawn, fleet, and queue duties.
- Condense trust/lifecycle explanations and redundant links.

## Out of scope

- C++, session behavior, deterministic ordering, fleet persistence/RNG, save/replay, transfer, paused availability, trust validation, or lifecycle reset behavior.
- Editing engine Server, File, or Network documentation.

## Acceptance criteria

- Remeasure with `bt-token-v1`; meet the budget or explain unavoidable operative excess.
- Persistent fleet identity/RNG/save contracts, queue drain timing, active-coordinate/replay obligations, transfer relink ordering, advancing-update gates, trust bounds, reconnect notification, and reset behavior remain explicit.
- Engine-owned machinery is referenced once through its existing owners.

## Classification

Tier 1 — documentation-only condensation.

## Coordination

None.

## Notes

Preserve deterministic and persistence contracts even if they require an advisory excess.
