<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T18:47:53.038Z","dependsOn":[]} -->
# Per-Tick Change-List Transport Contract

## Context

The current engine, server, client, and replay paths all consume a flat array
of tagged game status changes. The contract is visible in
Projects/BrokenEngineSandbox/Source/Network/NetworkSerialization.cpp:176-461,
Projects/BrokenEngineSandbox/Source/Network/Server/ServerTransferManager.cpp:29-349, and
Projects/BrokenEngineSandbox/Source/Network/Server/ServerBroadcaster.cpp:82-191 and 267-291.
The game supplies the change enum, payload variants, codecs, transfer policy,
SpawnTransfer, TransferRequest, and the transferRequests member on its
FramePostRender. The four current producers are in the Blasters, Missiles,
Spaceships, and PlayersNavigation collection files.

A second game would otherwise have to reproduce the desync-sensitive
batch codec, cross-cell transfer ordering, deferred agent injection, and
per-cell publication machinery. The unresolved D1 condition is which part of
that shape is a universal engine contract and which part remains game policy.
The governing invariants are wire identity, post-tick transfer-before-CRC
ordering, deterministic type ordering, and replay parity.

The user approved conversion of this investigation into an executable Plan
while allowing bounded implementation options to remain visible. The future
implementation must select one option before changing source.

## Design

Use the existing tagged-item array as the primary bounded option. The
researched recommendation, not an implementation approval, is to make that
shape mandatory and move generic transport, validation, transfer harvest,
deferred injection, and publication assembly into the engine. The game keeps
its enum ending in kCount, concrete payloads, per-type NetworkSessionContract
operations, transfer classification and destination policy, SpawnTransfer,
TransferRequest, transferRequests, and the four type-specific producers.

The implementation must resolve these bounded choices explicitly:

1. Select the mandatory tagged array contract, or return the work for
   replanning if a game-specific payload format is required. An opt-in
   collection registry is not an implicit alternative because deterministic
   collection order is CRC and wire load-bearing.
2. Preserve the current bounded receive cursor and hostile-prefix clamp, or
   define a documented whole-batch rejection contract before changing it.
   Malformed-input behavior is currently not an all-or-nothing batch rule.
3. For transfer requests, retain four game producers, add only a game-side
   helper that preserves type-specific construction and diagnostics, or
   introduce a payload-blind required engine seam. Do not add a template or
   move the transient transferRequests buffer without choosing that boundary.
4. Implement the codec first, then transfer harvest, deferred agent injection,
   and per-cell publication. The transfer-after-tick then destination-CRC
   ordering is an engine rule; game code supplies classification,
   materialization, liveness, and payload policy.

## Critical files

- Projects/BrokenEngineSandbox/Source/Network/NetworkSerialization.cpp:176-461
- Projects/BrokenEngineSandbox/Source/Network/Server/ServerTransferManager.cpp:29-349
- Projects/BrokenEngineSandbox/Source/Network/Server/ServerBroadcaster.cpp:82-191 and 267-291
- Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.cpp:184-221
- Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp:348-394
- Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp:394-444
- Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp:251-303
- Projects/BrokenEngineSandbox/Source/Network/NetworkSessionContract.h
- Projects/BrokenEngineSandbox/Source/Frame/Frame.h

## In scope

- The tagged status-change batch contract and its codec envelope.
- Server cross-cell transfer collection, validation, sorting, application,
  and destination CRC recomputation.
- Deferred agent-injected changes and per-cell publication assembly.
- The bounded transfer-producer ownership choice and the four current
  producer loops, without changing their payload semantics.
- Existing replay/broadcast/CRC channel routing for injected changes.
- Byte identity and malformed-input behavior for the current protocol.

## Out of scope

- Variable-length payload framing or a future protocol extension.
- The client rollback/replay classes, except for the transfer-ordering seam
  consumed by the separate reconciliation Plan.
- Save/replay lifecycle ownership, except that this Plan supplies its
  transfer dependency.
- The D12 time-scaling packet promotion and its protocol version bump.
- Player dependency removal, target collection registration, or unrelated
  collection extraction.

## Risk tier and invariants

Expected Change Workflow Tier 3: this changes wire-facing transport,
deterministic CRC ordering, replay input, and cross-cell integration.
Preserve the current game payload bytes, type grouping, transfer adjacency
and liveness checks, post-tick ordering, destination materialization,
destination CRC point, publication order, and allocation and thread-affinity
rules. No compatibility reader or alternate wire format is added without an
explicit decision.

## Coordination

No directional prerequisite is required.
Documents/Plans/Engine/ReconcileReplayChainToEngine.md and
Documents/Plans/Engine/ReplayLifecycleToEngine.md consume this Plan's
selected tagged-array and transfer-ordering contract. Their implementation
must not duplicate codec, transfer harvest, or publication ownership.
Documents/Architecture/Network.md and
Documents/Architecture/GameReconciliation.md are the behavior authorities to
reread before implementation; they are not source files owned by this Plan
unless the current contract changes.

## Acceptance criteria

- The selected contract is stated in the implementation record and covers
  every engine/game seam listed above; no producer or transfer buffer is moved
  by implication.
- Encoded batches before and after the move are byte-identical for the
  existing fixtures and type order.
- Malformed input, hostile compressed prefixes, capacity failures, adjacency
  rejection, destination application, publication, replay capture, and CRC
  ordering retain their current observable results, or the selected new
  contract has an explicit acceptance fixture.
- Deferred agent changes travel through the same broadcast, CRC, and replay
  path as ordinary changes.
- Both client and server compile through the repository build workflow, and
  the existing harness/replay scenarios show no new transfer, CRC, replay, or
  desync errors.

## Notes

This Plan records D1 and its four transport candidates as one ordered
contract slice. It does not schedule the recommended keep-in-game
FleetSelection mechanism, and it does not claim that the researched
recommendation has user approval.
