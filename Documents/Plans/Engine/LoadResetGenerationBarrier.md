<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T22:27:07.360Z","dependsOn":[]} -->
# Preserve post-load subscription traffic across channel reordering

## Context

The server sends `kServerLoadNotification` on the control reliable channel, but
subscription static data, full states, and updates use independently ordered
slot channels. `Client::Poll` drains all ready channels before
`ClientSessionRuntime::PollAndDrain` treats the load flag as a reset boundary.
Consequently, a subscription accepted after the server reset can place valid
post-load slot data in the client receive buffers before the notification is
observed, and `ResetForServerLoad` then erases it. The client can remain in
`kWaitingFullState` while the server already considers the slot active and
ignores the repeated subscribe.

The deterministic source signal is
`Engine/Source/Network/Client/Client.cpp:146-215`,
`Engine/Source/Network/Client/ClientSessionRuntime.cpp:179-203,214-258`,
`Engine/Source/Network/Server/ServerSend.cpp:15-118,273-290`, and
`Engine/Source/Network/NetworkSimulation.h:150-186`: receive mutation precedes
the reset decision, the reset clears those mutations, and reliable ordering is
per channel rather than global.

## Design

Add a server-load generation owned by the server connection/session boundary.
Advance it when `ResetClientsForLoad` establishes the new authoritative world,
publish it in `kServerLoadNotification`, and stamp it on subscription accept,
static-data, full-state, update, and resend messages. Add the client's committed
load generation to the receive boundary. Increment `engine::kuiProtocolVersion`
for these incompatible engine-packet layout changes; do not add a compatibility
path. Keep the ACK stream unchanged: its existing slot epoch rejects stale ACK
state after the server frees and reallocates a slot across a load.

Keep the existing slot epoch as the identity for reuse within one load
generation. Key each staged setup tuple by `(load generation, slot index, slot
epoch, coordinate)`. Within one load generation and slot, a wrap-aware newer
epoch atomically replaces every field of the older staged tuple; an older epoch
is rejected, and the same epoch with a different coordinate is rejected. A
same-identity duplicate field keeps the first arrival. Do not stage updates or
resends, and keep the accepted ACK simplification unchanged. A subscription
request received before the load but accepted by the server afterward is valid:
its responses carry the new generation and the client must preserve them until
that generation is committed.

At the client receive boundary, compare stamped slot traffic with the committed
load generation before mutating slots, ACK tracking, timing, or ordinary drain
buffers. Reject older-generation traffic. For the one newest future generation,
stage at most one accept, static-data payload, and full state per slot without
applying or acknowledging them. Drop future-generation update and resend
payloads; after the staged full state commits its ACK floor, the existing gap
and resend path recovers any needed deltas. A still newer generation replaces
older staged setup traffic, because an intervening load makes that older world
obsolete.

When a load notification advances the committed generation,
`ResetForServerLoad` clears the old world and delayed coordinate packets, then
retains incomplete setup tuples for that now-current generation. If a staged
accept is present, apply it first, then reclassify and release matching staged
static/full fields through the existing handlers and drain order. If the accept
arrives later as ordinary committed-generation traffic, apply it and then
release the matching staged static/full fields. This prevents a crossing
static/full from being lost when notification and accept arrive in different
polls. It must not retain traffic for an older generation or apply future
traffic before its notification. Multiple notifications coalesced by one poll
commit the newest observed notification; staged traffic for a still newer
generation remains staged for its matching notification.

Extend the existing client full-state Agent Harness fixture with one focused
load-generation action that feeds current-generation and future-generation
subscription messages through the real receive/reset boundary in the adverse
slot-before-control order. Reuse its connected coordinate and full-state
inspection rather than adding a general packet-injection or network-simulation
configuration surface. Pair that deterministic boundary fixture with an
ordinary live server load/resubscription scenario.

The selected generation design avoids waiting an additional round trip before
the server can publish valid post-load subscriptions. The alternative was a
load-acknowledgement gate that would hold subscription publication until each
client acknowledged the notification. That would also impose an order, but it
was not selected because it delays valid work and adds per-client handshake
state instead of identifying the cross-channel traffic directly.

## Critical files

- `Engine/Source/Network/NetworkProtocol.h` and `NetworkMessages.h` — protocol
  version and load-generation wire fields.
- `Engine/Source/Network/Server/Server.h` and `ServerSend.cpp` — generation
  ownership and response stamping.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp` —
  authoritative load-generation advance.
- `Engine/Source/Network/Client/Client.h`, `Client.cpp`, and
  `ClientReceive.cpp` — committed generation, bounded future-generation
  staging, and packet classification.
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp` — reset and staged
  drain ordering.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp` and
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-client.md` —
  focused adverse-order fixture action and its contract.
- `Documents/Architecture/Network.md` and the affected Network `AGENTS.md`
  files — load-generation, epoch, ACK, and reset contracts.

## In scope

- One server-owned load generation advanced by the authoritative load reset
  and published to connected clients.
- Generation identity on the load notification, subscribe accept, static/full
  subscription state, and update/resend traffic.
- Client classification and bounded staging of one newest future generation,
  with each accept/static/full setup tuple keyed by load generation, slot,
  epoch, and coordinate, followed by reset-before-apply draining for the
  committed generation; future update/resend traffic is dropped for ordinary
  resend recovery.
- Protocol-version bump and paired message serialization/validation updates.
- A focused client full-state fixture action and live load/resubscription
  scenario that exercise the selected cross-channel semantics.
- Documentation of the resulting protocol and client/server ownership.

## Out of scope

- A load-acknowledgement handshake or server-side publication stall.
- Changing slot-epoch allocation, subscription cancellation policy, sticky
  subscription selection, or ordinary same-channel reliable ordering.
- Generic packet injection, runtime-configurable channel delays, packet-buffer
  size increases, or unrelated packet bounds.
- Frame/save/replay formats, `Frame::kiVersion`, simulation CRC contents, or
  backward compatibility with the prior protocol version.

## Risk tier and invariants

Expected Change Workflow Tier 3. The change alters the wire protocol, ACK and
subscription identity, and client/server load ordering across independently
owned transport and game-session boundaries.

- The client resets the old world before it applies any traffic for the
  committed post-load generation.
- A valid subscription accepted after the server reset survives delivery ahead
  of its load notification and reaches full-state adoption.
- Older-generation traffic cannot mutate slots, ACK state, timing, or game
  state; future-generation traffic cannot be acknowledged or applied early,
  and dropped future updates recover through the existing resend path.
- Staged setup identity is complete: a wrap-aware newer epoch replaces the
  older tuple atomically, older epochs and same-epoch coordinate mismatches are
  rejected, and same-identity duplicate fields retain first arrival. After its
  notification, an incomplete current-generation tuple remains available for
  accept-first or accept-later release through the existing handlers.
- Slot epochs continue to reject reuse ghosts within a generation, including
  the cancellation behavior owned by `SubscriptionCancellationEpoch.md`.
- Coordinate-channel purge, initial resubscription, static→full→update drain
  order, same-channel FIFO, and deterministic PostRender CRC behavior remain
  unchanged.

## Coordination

- Coordinate with `Documents/Plans/Engine/SubscriptionCancellationEpoch.md`:
  that Plan owns cancellation identity within a load generation, while this
  Plan owns identity across server loads. Whichever lands second must preserve
  both classifications and the shared slot receive paths; neither Plan depends
  on the other landing first.

## Acceptance criteria

- The focused fixture delivers old-generation and valid new-generation slot
  traffic before the new-generation load notification. No slot, ACK, timing,
  or game state changes early; the old traffic is rejected; after notification
  the valid accept/static/full setup traffic drains in existing order and the
  slot becomes active from the transmitted full state. A future-generation
  update is dropped before notification and recovered through the existing
  ACK-gap/resend path afterward. The fixture asserts that a wrap-aware newer
  epoch replaces every field of an older staged tuple, while an older epoch and
  a same-epoch wrong-coordinate packet are rejected; same-identity duplicate
  fields keep the first arrival. The same fixture also delivers matching
  future-generation static/full setup traffic without an accept, then delivers
  and commits the matching load notification and proves the tuple remains
  unapplied; in a later poll it delivers ordinary committed-generation
  matching accept traffic and proves the accept applies first, followed only
  then by retained static/full release through the existing handlers and drain
  order, leaving the slot active from the transmitted full state.
- A second future generation supersedes staged traffic for the first without
  applying either before its notification, and the newest matching notification
  commits the newest traffic.
- A live save/load run re-establishes the desired subscription without a
  five-second timeout, duplicate-subscribe stall, confirmed desync, or CRC
  mismatch.
- Accept-first, full-state-first, ordinary slot reuse, delayed-coordinate purge,
  and cancelled-subscription ghost handling retain their documented behavior.
- Client and server `Debug|x64` builds pass through `/compile`; the focused and
  live scenarios pass through `/agent-harness`.

## Notes

Origin: `Documents/Investigations/Engine/LoadResetCrossChannelBarrier.md`. The
investigation was confirmed against baseline
`54cb3c36fad34497c6e27a89fbbb506f8709cc7f` before this Plan replaced it.
