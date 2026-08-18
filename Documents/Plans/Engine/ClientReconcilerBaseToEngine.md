<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T18:48:44.566Z","dependsOn":["Documents/Plans/Engine/ReconcileReplayChainToEngine.md"]} -->
# Client Reconciler Dispatch Boundary

## Context

The parallel per-cell client reconciler in
Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp:35-228
is mostly generic.
The confirmed-client-state and visual-error smoothing region at :164-203
still assumes game-specific player/focus data. ClientReconciler.h:34-113 also
contains structs carrying game Frame pointers and status payloads, so it is not
a pure base boundary.

The unresolved D2 questions are whether following one entity is an engine
concept and how a spectator or god-view game supplies confirmed state and
visual smoothing. The current focus seam returns an XMVECTOR position and a
boolean success result. The user-approved conversion may retain bounded
alternatives for that policy.

## Design

Move only the generic parallel per-cell dispatch and leave the game-policy
region behind an explicit required compile-time seam. The implementation must
choose one of these bounded options: preserve the existing position-plus-bool
contract, or pass a game-owned focus/confirmed-state record to a generic
engine routine. A virtual session base, optional callback, and registration
registry are not selected. Preserve game-specific visual-error smoothing
unless the chosen record proves that its semantics are domain-neutral.

## Critical files

- Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp:35-228
- Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.h:34-113
- Projects/BrokenEngineSandbox/Source/Network/Client/ReconcileReplayClientState.cpp
- Projects/BrokenEngineSandbox/Source/Game.h
- Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.h

## In scope

- Generic parallel per-cell dispatch and its engine-owned scheduling boundary.
- The confirmed-client-state/focus and visual-error policy seam at
  ClientReconciler.cpp:164-203.
- Required game-facing focus/confirmed-state data needed by a non-player game.
- Existing dispatch order, synchronization, and smoothing semantics.

## Out of scope

- CRC, ring, replay tick, or transfer ordering owned by
  ReconcileReplayChainToEngine and ChangeListTransportContract.
- Desync reporting/reset/fixture policy owned by ClientDesyncPolicyToEngine.
- Client session poll timing and clock-snap recovery.
- Camera ownership, visible-neighbor policy, or Player header cleanup.

## Risk tier and invariants

Expected Change Workflow Tier 3: parallel simulation/reconciliation state and
CRC-adjacent client policy cross a subsystem boundary. Preserve cell dispatch
order, worker synchronization, confirmed-tick semantics, visual-error
application, vector W lanes, no main-loop allocations, and deterministic
PostRender state. The engine must not require a Player entity.

## Coordination

Metadata depends on Documents/Plans/Engine/ReconcileReplayChainToEngine.md.
The replay Plan supplies the ring/tick boundary. The desync and polling Plans
must consume this policy seam rather than moving or duplicating the focus
logic. Existing D14 agent-command ownership remains game-owned for coordinate
commands.

## Acceptance criteria

- Per-cell dispatch produces the same ordered state and worker completion
  behavior under the existing client scenario.
- The selected focus/confirmed-state option supports both the current focused
  game and a game with no player entity without Player names in engine code.
- Visual-error smoothing and missing-focus behavior are unchanged for the
  current game, or the selected new policy has an explicit observable rule.
- No new CRC, replay, wire, allocation, or thread-affinity regressions appear;
  client and server compilation passes.

## Notes

The exact client-policy hook was intentionally unresolved in the investigation;
the option list is bounded by the current position-plus-bool seam and an
explicit game-owned state record.
