<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T23:21:46.067Z","dependsOn":[]} -->
# Complete invalid subscribe-accept cleanup acknowledgments

## Context

`Client::ServerSubscribeAccept` treats a non-reject slot outside the local
client pool as hostile input but still sends an epoch-qualified unsubscribe so
the peer can release any corresponding server state
(`Engine/Source/Network/Client/ClientReceive.cpp:489-516`). The server preserves
idempotency by acknowledging every syntactically valid unsubscribe after client
lookup, while freeing state only for an in-range active slot with the matching
epoch (`Engine/Source/Network/Server/ServerReceive.cpp:410-438`). The resulting
acknowledgment carries the same invalid slot, and
`Client::ServerUnsubscribeAck` currently classifies every out-of-range slot as
corrupt before considering whether the client requested that cleanup
(`Engine/Source/Network/Client/ClientReceive.cpp:573-596`). `Client::Receive`
then asserts on that corrupt-stream result (`Engine/Source/Network/Client/Client.cpp:255-312`).

The reachable exchange is therefore invalid subscribe accept -> client cleanup
unsubscribe -> server idempotent acknowledgment -> client corruption assert.
The initial bounds guard prevents the direct local slot access, but a live
connection cannot complete its own cleanup exchange.

## Design

The author's recommendation is to keep the wire messages and the server's
idempotent acknowledgment behavior unchanged. At the client trust boundary,
retain a fixed-size outstanding-cleanup marker keyed by the invalid `uint8_t`
slot when the out-of-range subscribe-accept branch sends its unsubscribe. While
that marker is outstanding, coalesce further cleanup requests for the same
invalid slot so one request produces one acknowledgment. Consume the matching
out-of-range acknowledgment and clear the marker; continue treating an
out-of-range acknowledgment with no outstanding marker as corrupt server data.
Clear all markers with the existing connection/reset state so identity never
crosses peer lifetimes.

Use fixed-capacity state derived from the wire slot domain. Do not allocate on
receive, add a timeout/retry subsystem, change message layouts, or weaken the
ordinary in-range unsubscribe state check.

## Critical files

- `Engine/Source/Network/Client/Client.h` — bounded outstanding invalid-cleanup
  state owned by the connection.
- `Engine/Source/Network/Client/ClientReceive.cpp:489-596` — invalid accept
  cleanup request and unsubscribe-ack classification.
- `Engine/Source/Network/Client/Client.cpp` — existing connection/reset state
  boundary and corrupt-stream response.
- `Engine/Source/Network/Server/ServerReceive.cpp:410-438` — read-only evidence
  for the server's idempotent acknowledgment contract.
- `Engine/Source/Network/Client/AGENTS.md` — resulting invalid cleanup and
  unsolicited-ack trust-boundary contract.

## In scope

- `Client` connection-lifetime state for one outstanding cleanup per invalid
  wire slot.
- The out-of-range branch of `Client::ServerSubscribeAccept`, including
  duplicate cleanup coalescing.
- The out-of-range branch of `Client::ServerUnsubscribeAck`, limited to
  consuming an acknowledgment for a cleanup the client recorded as sent.
- Resetting the bounded cleanup state at the existing client connection/reset
  boundary and documenting the resulting client receive invariant.

## Out of scope

- `Server::ClientUnsubscribe`, server slot allocation, or server acknowledgment
  policy.
- Wire message layouts, packet sizes, protocol version, slot-epoch allocation,
  or backward compatibility.
- In-range subscription cancellation, ghost response retirement, load
  generations, sticky-subscription policy, or slot pool size.
- Save/replay formats, deterministic simulation state or CRC contents,
  threading, dynamic allocation, and unrelated corrupt-packet classifications.
- New general packet-injection facilities or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: the change alters which invalid
network acknowledgment the client trust boundary accepts, even though the wire
format and server behavior remain unchanged.

- A client-generated cleanup for an invalid accepted slot completes without a
  corruption assert and leaves no outstanding local cleanup marker.
- An unsolicited out-of-range unsubscribe acknowledgment remains corrupt input.
- Duplicate invalid accepts for one slot while cleanup is outstanding produce
  at most one cleanup request and one consumable acknowledgment.
- In-range unsubscribe acknowledgments retain the existing requirement that
  the slot be `kUnsubscribing` before it is freed.
- Connection/reset boundaries discard cleanup identity from the prior peer
  lifetime.

## Acceptance criteria

- A focused live receive scenario delivers an out-of-range non-reject
  subscribe accept and observes the client send cleanup, the server return its
  normal acknowledgment, and the client remain connected without a corrupt
  packet assertion.
- The same scenario delivers the invalid accept twice before the acknowledgment
  and observes one cleanup exchange, followed by no retained outstanding marker.
- A fresh unsolicited out-of-range unsubscribe acknowledgment still follows the
  existing corrupt-server-data response.
- Ordinary in-range unsubscribe, canceled-subscription ghost, and disconnect or
  reset paths retain their current behavior.
- Client and server `Debug|x64` builds pass through `/compile`, and the focused
  live scenario passes through `/agent-harness`.

## Notes

This is a pre-existing engine cleanup defect exposed while preparing runtime
coverage; no production correction is part of that harness work. Duplicate
searches for the receive symbols, invalid-slot terms, and unsubscribe-ack terms
found no Plan owning this root cause. `SubscriptionCancellationEpoch.md` owns
late responses for a valid slot canceled locally, and
`LoadResetGenerationBarrier.md` owns subscription identity across loads; this
Plan owns only cleanup of an invalid slot supplied by a subscribe accept. The
three changes are independently landable, so no dependency or Coordination
edge is required. Re-locate line numbers before implementation.
