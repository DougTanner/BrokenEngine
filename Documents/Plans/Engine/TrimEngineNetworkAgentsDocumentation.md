<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T22:54:21.555Z","dependsOn":[]} -->
# Trim engine Network agent documentation

## Context

`Engine/Source/Network/AGENTS.md` measures 2,169 `bt-token-v1` tokens against a 1,866-token formula budget (16,162 direct code tokens, two direct child documents), an advisory excess of 303. The Network document currently repeats Client and Server child-document ownership and architecture detail, especially in client/server admission, discovery, status batching, and session-runtime descriptions.

## Design

The author recommends retaining the Network document's wire, corrupt-input, affinity, and ownership boundaries while replacing detailed Client and Server mechanics with concise links to `Client/AGENTS.md`, `Server/AGENTS.md`, and the Network architecture. Merge repeated protocol-version and packet-admission explanations where one statement can preserve both rules.

## Critical files

- `Engine/Source/Network/AGENTS.md`

## In scope

- Condense `## Transport Contracts` around versioning, fixed/variable packet validation, rate limits, and paired endpoint settings.
- Tighten `## Corrupt Input Policy` without changing its corruption/state distinction or asymmetric client/server response.
- In `## Ownership`, replace discovery, runtime, transfer, and batch-codec mechanics already owned by the Client and Server child documents or Network architecture with concise ownership boundaries and direct links.
- Remove redundant `## See Also` descriptions.

## Out of scope

- C++, wire layout/versioning, packet validation, trust policy, timing, affinity, serialization, or status-change ordering.
- Editing child or architecture documentation.

## Acceptance criteria

- Remeasure with `bt-token-v1`; meet the current budget or explain the operative rules behind unavoidable advisory excess.
- The protocol bump gate, corrupt-input policy, main-thread affinity, polling semantics, and engine/game ownership boundaries remain explicit.
- Detailed client/server and flow mechanics are stated once at their existing owner and directly linked from the Network document.

## Classification

Tier 1 — documentation-only condensation.

## Coordination

None.

## Notes

Preserve all trust-boundary and wire-compatibility rules regardless of the advisory target.
