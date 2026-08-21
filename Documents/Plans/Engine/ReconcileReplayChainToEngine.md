<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T18:48:39.329Z","dependsOn":[]} -->
# Reconciliation Replay Chain to Engine

## Context

Client prediction recovery is split across the CRC fast path,
past-frame/ring orchestration, and the replay tick driver. The current
engine-shaped code is in
Projects/BrokenEngineSandbox/Source/Network/Client/ReconcileReplayCrc.cpp,
Projects/BrokenEngineSandbox/Source/Network/Client/ReconcileReplay.cpp,
Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.h:34-113,
and Projects/BrokenEngineSandbox/Source/Network/Client/ReconcileReplayTick.cpp:24-403.
Remaining game coupling is
status type names, game Frame replay/desync storage, status payloads, transfer
application, and the CRC recomputation point.

The unresolved D2 gap is an engine/game boundary that preserves deterministic
rollback, transfer-after-tick ordering, and diagnostic output without moving
game reset or client-policy code wholesale. Completed historical prerequisites
for full-state injection, debug-frame correlation, and load-clock jitter are
already landed and are not active dependencies.

The user approved conversion while allowing bounded choices to remain in the
Plan. A future implementation must choose the policy seams before editing.

## Design

Move the chain in this order: CRC fast path, ring/orchestration, then replay
tick driver. The researched recommendation is a required game contract for
StatusChangeType::kCount and StatusChangeTypeName, with no callback for one
diagnostic line. The engine owns the rule that transfers are applied after the
tick and the CRC is recomputed at the same comparison point; game functions
provide classification, payload materialization, destination policy, and
summary details.

The implementation must select one bounded client-policy shape for the
remaining seams: retain the current game Frame storage behind required
compile-time game functions, or introduce an explicit game-owned replay-state
record passed to the engine. Do not hide this choice behind optional virtual
functions, registration, std::function, or CRTP. Keep
ReconcileReplayClientState.cpp outside the engine move unless the selected
record boundary proves it can move without game policy.

## Critical files

- Projects/BrokenEngineSandbox/Source/Network/Client/ReconcileReplayCrc.cpp
- Projects/BrokenEngineSandbox/Source/Network/Client/ReconcileReplay.cpp
- Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.h:34-113
- Projects/BrokenEngineSandbox/Source/Network/Client/ReconcileReplayTick.cpp:24-403
- Projects/BrokenEngineSandbox/Source/Network/Client/ReconcileReplayClientState.cpp
- Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h
- Projects/BrokenEngineSandbox/Source/Network/NetworkSessionContract.h

## In scope

- CRC comparison and diagnostic type-name access in ReconcileReplayCrc.
- Ring storage and replay orchestration that do not own game policy.
- Replay tick rerun, transfer application, and post-transfer CRC recompute.
- Required compile-time status count/name and payload/Frame policy seams.
- Existing replay/desync ring and tick ordering, with no new ring format.

## Out of scope

- Server transfer harvest, codec, publication, or agent queue ownership;
  those belong to ChangeListTransportContract.
- Client per-cell dispatch and visual-error/focus policy; those belong to
  ClientReconcilerBaseToEngine.
- ClientDesyncManager reset sequencing and agent fixture/debug policy.
- Client poll timing and clock-snap recovery.
- New replay formats, compatibility readers, or protocol changes.

## Risk tier and invariants

Expected Change Workflow Tier 3: this is deterministic rollback, CRC, replay,
and client/server integration. Preserve frame-ring indexing, tick order,
transfer application before CRC, status payload bytes, checksum comparisons,
desync escalation inputs, replay stream order, allocation behavior, and
thread/dispatch affinity. The engine must not name Player symbols.

## Coordination

Metadata depends on Documents/Plans/Engine/ChangeListTransportContract.md.
That Plan owns the transfer classification/materialization ordering consumed
here; do not reproduce its codec or server harvest. The follow-on
Documents/Plans/Engine/ClientReconcilerBaseToEngine.md and
Documents/Plans/Engine/ClientDesyncPolicyToEngine.md consume the engine-facing
replay boundary and must leave their policy symbols on their side of the seam.

## Acceptance criteria

- Existing recorded replay and rollback scenarios produce identical ordered
  CRC/checksum sequences and no new desync or replay errors.
- The selected required status diagnostic contract compiles for both targets
  and reports the same type names and count.
- Replayed ticks apply transfers before recomputing the CRC, with the same
  destination and comparison tick as the current path.
- The ring and tick driver preserve past-frame retention, confirmed-tick
  advancement, input replay, and failure escalation.
- Client and server compile through the repository build workflow; the
  existing harness replay/desync probes settle without a new error.

## Notes

This is the first D2 chain stage. It consumes the D1 decision but does not
choose the later client-focus or desync ownership shape.
