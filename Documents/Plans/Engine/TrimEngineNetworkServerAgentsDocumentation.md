<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T22:54:30.690Z","dependsOn":[]} -->
# Trim engine Network Server agent documentation

## Context

`Engine/Source/Network/Server/AGENTS.md` measures 2,471 `bt-token-v1` tokens against a 1,822-token formula budget (23,494 direct code tokens, zero direct child documents), an advisory excess of 649. Long session and buffered-state bullets combine local invariants with explanatory flow already available in Network architecture and the game server document.

## Design

The author recommends reducing narrative examples and repeated ownership prose while keeping hostile-input lifetime safety, poll-window behavior, slot/epoch/ACK rules, transfer publication, CRC recomputation, buffer contiguity, and deferred-send revalidation explicit. Point game-policy and replay-retention details to their existing authorities.

## Critical files

- `Engine/Source/Network/Server/AGENTS.md`

## In scope

- Condense the opening ownership inventory and `## Session Invariants` explanations, especially polling, active-coordinate, ACK, and violation-accounting narration.
- Tighten `## Transfers and Publication` by separating durable engine/game boundaries from examples of named fields and queues.
- Condense `## Buffered State` scenario prose while retaining gap restart, full-state hold, contiguity, pruning, and deferred-send rules.
- Remove repeated links and rationale already owned by Network architecture or game Server documentation.

## Out of scope

- Server code, protocol, trust boundaries, threading, session timing, subscription/ACK behavior, transfer ordering, CRC, replay, or buffering behavior.
- Editing the game server or architecture documents.

## Acceptance criteria

- Remeasure with `bt-token-v1`; meet the formula budget or explain unavoidable operative excess.
- The use-after-free boundary after `RecordContractViolation`, two-poll budget semantics, slot/epoch/ACK invariants, transfer CRC recomputation, publication lifetimes, ring continuity, restart hold, and send-time revalidation remain explicit.
- Game-owned policy details appear only as a boundary and direct authority link.

## Classification

Tier 1 — documentation-only condensation.

## Coordination

None.

## Notes

Trust, compatibility, and deterministic-publication rules take precedence over the advisory budget.
