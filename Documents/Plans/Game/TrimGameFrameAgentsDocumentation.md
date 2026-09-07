<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T22:54:59.613Z","dependsOn":[]} -->
# Trim game Frame agent documentation

## Context

`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` measures 1,749 `bt-token-v1` tokens against a 1,607-token formula budget (13,070 direct code tokens, one direct child document), an advisory excess of 142. Several invariants include detailed mechanics already owned by engine Frame, File replay, Network, or the pipeline architecture.

## Design

The author recommends keeping game-owned deterministic and serialization obligations here while shortening engine-owned phase, registry, replay, and monitoring explanations to concise obligations plus direct links. Merge overlapping `FrameInput` version and variant-read prose where their separate guards remain clear.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`

## In scope

- Condense engine-owned phase/CRC and registry mechanics in `## Invariants` to game-side obligations and authority links.
- Tighten `FrameInput` ownership, versioning, replay-difference, and variant-read bullets without losing compatibility gates or trust validation.
- Shorten monitoring-counter ownership explanations and redundant `## See Also` links.

## Out of scope

- C++, phase order, CRC/determinism, serialization layouts/versions, wire versions, replay, registry behavior, or server monitoring behavior.
- Editing engine Frame, File, Network, or architecture documentation.

## Acceptance criteria

- Remeasure with `bt-token-v1`; meet the current budget or explain unavoidable operative excess.
- Game-owned deterministic inputs, collection order, version gates, corrupt-read handling, registry acquisition order, queue clears, and monitoring writer ownership remain explicit.
- Engine-owned mechanics are referenced without being restated.

## Classification

Tier 1 — documentation-only condensation.

## Coordination

None.

## Notes

Serialization and deterministic-phase contracts take priority over the advisory target.
