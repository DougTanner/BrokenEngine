<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:52:16.760Z","dependsOn":[]} -->
# Retire cancelled subscription responses by slot epoch

## Context

The retained survivor `CAI/shard-0035/001` identifies a cross-channel
subscription race. `Client::CancelSubscription` frees a slot and records only
its coordinate in `Engine/Source/Network/Client/Client.cpp:90-105`.
When a late full state arrives, `ServerCoordFullState` consumes that marker and
sends an epoch-qualified ghost unsubscribe at
`Engine/Source/Network/Client/ClientReceive.cpp:209-218`. A subscribe accept
arriving afterward sees an ordinary `kUnsubscribed` slot and commits it at
`:158-180,570-585`, while `ServerUnsubscribeAck` frees only a slot already in
`kUnsubscribing` at `:588-615`.

The direct evidence is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0035.md:67`
and consolidated selector `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:921`. The frozen/live
source rows match baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`; no
source was changed by routing. Runtime subscription churn explicitly permits
full-state-before-accept across different reliable channels, so the stale slot
is not a malformed-packet case.

## Design

The author's recommendation is to retain cancellation state by local slot and
the response epoch (or an equivalent request generation) until both late
response classes are retired. The full-state ghost path may consume its own
response marker, but the later accept must still classify as belonging to the
cancelled generation and issue/retain the matching unsubscribe instead of
committing `kWaitingFullState`. Preserve the legitimate full-state-before-accept
path for a still-live subscription and the existing retained-epoch reuse guard.

## Critical files

- `Engine/Source/Network/Client/Client.cpp:90-105,117-143` — slot freeing,
  cancellation records, and timeout recovery.
- `Engine/Source/Network/Client/ClientReceive.cpp:88-104,158-218,550-615`
  — full-state/accept/ACK classification and ghost handling.
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp:493-547` — ordinary
  sticky-subscription cancellation caller.
- `Engine/Source/Network/Server/ServerReceive.cpp:395-450` — matching server
  slot free and unsubscribe ACK.
- `Engine/Source/Network/Client/AGENTS.md` and
  `Engine/Source/Network/Server/AGENTS.md` — epoch and subscription contracts.

## In scope

- Local cancellation identity and late accept/full-state classification for a
  freed subscription slot.
- Slot state and ACK transitions needed to retire a cancelled generation
  without leaking a server slot.
- Existing legitimate out-of-order response and slot-reuse behavior.

## Out of scope

- ENet channel format, packet sizes, protocol version, or server subscription
  policy unrelated to a cancelled slot.
- Generic receive bounds, reconciliation CRC, and host construction failure.
- Increasing the client slot pool or changing sticky-subscription duration.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: this changes network slot identity,
epoch handling, cross-channel ordering, and client/server subscription
lifetime.

Preserve these invariants:

- A cancelled request cannot be reactivated by any late response from its old
  epoch/generation.
- A server slot freed by a ghost unsubscribe always has a client-side path that
  consumes its ACK and releases the local slot.
- A live request still admits the documented full-state-before-accept order and
  epoch-heal behavior.

## Acceptance criteria

- With a delayed coord full state arriving before its control-channel accept
  after cancellation, the client does not remain in `kWaitingFullState` and
  the matching server slot/ACK are retired.
- The next desired coordinate can use the released slot without waiting for
  the five-second timeout.
- Accept-first, full-state-first-live, and ordinary slot-reuse paths preserve
  their current behavior.
- Client and server `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0035/001`; source selector is the shard line above and the
consolidated selector is `consolidated-index.md:921`. No source fix or build
was performed during routing.
