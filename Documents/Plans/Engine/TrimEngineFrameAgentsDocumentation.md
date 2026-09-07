<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T22:54:04.442Z","dependsOn":[]} -->
# Trim engine Frame agent documentation

## Context

`Engine/Source/Frame/AGENTS.md` measures 3,047 `bt-token-v1` tokens against a 2,831-token formula budget (48,016 direct code tokens, one direct child document), an advisory excess of 216. Several long bullets restate detail owned by `Documents/Architecture/FrameUpdatePipeline.md`, `Collections/AGENTS.md`, or the game Frame document.

## Design

The author recommends trimming repeated phase, registry-query, and navigation implementation detail to concise invariants plus direct authority links. Preserve deterministic phase order, CRC boundaries, thread-local allocation rules, collection serialization order, terrain sampling separation, navigation trust-boundary validation, and all CPU/GPU or client/server distinctions.

## Critical files

- `Engine/Source/Frame/AGENTS.md`

## In scope

- Condense `## Overview` and the phase/ownership portions of `## Architecture` that repeat the linked pipeline document.
- Condense `## Frame Registry` query-window mechanics already specified by the pipeline document while retaining lifetime, allocation, ranking, and write restrictions.
- Shorten implementation inventories and explanatory examples in `## Terrain and Navigation`, preserving its operative sampling, winding, validation, and determinism rules.
- Remove redundant `## See Also` links already present at point of use.

## Out of scope

- C++, GLSL, phase ordering, CRC/determinism, serialization, allocation, terrain, navigation, or collection behavior.
- Editing linked architecture or child documentation.

## Acceptance criteria

- Remeasure with `bt-token-v1`; meet the current formula budget or explain which remaining operative rules require advisory excess.
- Determinism, CRC, thread affinity/allocation, collection order, registry lifetime/ranking, terrain sampling, and navigation validation contracts remain explicit or directly linked to their owner.
- The Frame pipeline and child collection documents remain the sole owners of their detailed mechanics.

## Classification

Tier 1 — documentation-only condensation with no invariant or runtime change.

## Coordination

None.

## Notes

Do not trade away an operative deterministic-simulation rule for the advisory budget.
