<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T18:48:55.298Z","dependsOn":["Documents/Plans/Engine/ClientReconcilerBaseToEngine.md","Documents/Plans/Engine/ReconcileReplayChainToEngine.md"]} -->
# Client Poll Timing and Clock-Snap Recovery

## Context

The client poll and clock-snap recovery path in
Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:190-244
is called out as
engine-shaped but gated behind the first reconciliation stages. Its current
timing and recovery behavior must remain compatible with the replay,
confirmed-tick, and desync boundaries.

The unresolved item is whether this timing belongs wholly to the engine or
requires a small required game policy seam for a game with different focus or
session semantics. The user-approved conversion permits that choice to remain
bounded in the Plan.

## Design

After the reconciliation chain has landed, move the generic poll cadence,
clock-snap detection, and recovery ordering to the engine. Choose either the
current direct game session contract or a small required game-owned timing
record. Do not add optional virtuals, callbacks, or a second timing loop.

## Critical files

- Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:190-244
- Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.h
- Projects/BrokenEngineSandbox/Source/Network/Client/ReconcileReplay.cpp
- Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp

## In scope

- Client poll timing, timeout/retry ordering, and clock-snap recovery.
- The minimum game-facing timing/focus policy seam selected by the
  implementation record.
- Existing interaction with reconciliation and desync escalation.

## Out of scope

- Replay ring, CRC, or tick-driver extraction.
- Server timescale packet promotion.
- Game input, camera, visible-neighbor, or Player dependency changes.
- New network retry policy or protocol format.

## Risk tier and invariants

Expected Change Workflow Tier 3: network timing and clock recovery interact
with deterministic reconciliation and client/server session state. Preserve
poll cadence, timeout values, clock snap thresholds, retry ordering, tick
identity, and allocation/thread-affinity behavior.

## Coordination

Metadata depends on Documents/Plans/Engine/ReconcileReplayChainToEngine.md and
Documents/Plans/Engine/ClientReconcilerBaseToEngine.md. Implementation must
wait until those seams are landed and must consume their final symbols rather
than resolving against a moving worktree.
`Documents/Plans/Engine/NetworkProfileOwnershipToEngine.md` depends on this Plan,
`Documents/Plans/Engine/ClientDesyncPolicyToEngine.md`, and
`Documents/Plans/Engine/WorldGridStateOwnershipToEngine.md`; it must consume
the settled poll-timing contract rather than duplicate its cadence or recovery
state. This Plan remains independent of that downstream profile extraction.

## Acceptance criteria

- Existing client connection, poll, jitter, clock-snap, and recovery harness
  scenarios show the same settled state and timing logs.
- A second game with no Player entity can satisfy the selected timing seam.
- No new protocol bytes, retry loop, allocation, or desync behavior changes
  are introduced.
- Client and server compile through the repository build workflow.

## Notes

This is the separately gated D2 client-session candidate, not a prerequisite
for the replay chain itself.
