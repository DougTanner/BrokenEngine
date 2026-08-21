<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-20T18:28:45.951Z","dependsOn":[]} -->
# Client Ack Floor Stuck After Full-State Resync

## Context

A client can disconnect itself after successfully recovering from a desync,
because its per-slot acknowledgement floor does not advance past the pre-event
tick even though the full-state adoption path resets it.

Observed once in five over-capacity harness trials during a Change Workflow
session, on a cold first run after server launch only:

1. The server dropped an over-cap status-change payload at tick 407 under the
   existing send-side cap rule in `Server::BufferFrame`
   (Engine/Source/Network/Server/Server.cpp:344-357, `kiMaxStatusChangesPerCell`).
2. The client hit `CONFIRMED DESYNC`, resynced once, and the server sent
   `Server::SendCoordFullState` at Frame 417.
3. At tick 531 the client logged
   `Client::TrackReceivedTick Too many missing frames, disconnecting Slot: 0 Gap: 129`,
   followed by `Server::Disconnect Client: 1` at tick 547.

Mechanism: `Client::TrackReceivedTick`
(Engine/Source/Network/Client/Client.cpp:333-338) disconnects when
`iTick - iAckFloor - 1 >= kiNetworkBufferSize`, where `kiNetworkBufferSize`
is 128 (Engine/Source/Network/NetworkProtocol.h:187). A gap of 129 at tick 531
means `iAckFloor` was still near the pre-event tick (~402), even though the
full-state adoption path sets `rSlot.ackState.iAckFloor = iTick` and clears
both received bitfields (Engine/Source/Network/Client/ClientReceive.cpp:269),
under the epoch guard region at ClientReceive.cpp:115, 133, and 146.

Four subsequent identical over-cap trials and a 1280-player control run
recovered and stayed connected for 400+ ticks, so the failure is
cold-start-conditional and not reproduced on demand yet.

Pre-existing: the session that observed this changed only the receive decode
path, which never executed here because the payload was dropped on the server
before transmission.

## Design

Root-cause first with /external-diagnose-bug; do not fix from the hypothesis
above. The diagnosis must establish which of these is true before any edit:

- the full-state adoption at ClientReceive.cpp:269 never ran for the affected
  slot (epoch guard, slot state, or coord mismatch at ClientReceive.cpp:115,
  133, 146),
- it ran but for a different slot or a stale slot record, or
- it ran and a later path reverted `iAckFloor`.

Only then make the smallest fix that keeps a recovered client connected, and
keep the existing disconnect rule itself intact unless the diagnosis proves
the rule is the defect.

Repro attempt: harness Debug build, reset, pause,
`spawn_players {"coord":[0,0],"count":256}` five times, unpause, on the first
run after server launch. If the repro does not reproduce, use added targeted
logging of `iAckFloor`, `uiEpoch`, and slot state across the resync rather
than speculative hardening.

## Critical files

- Engine/Source/Network/Client/Client.cpp (`Client::TrackReceivedTick`)
- Engine/Source/Network/Client/ClientReceive.cpp (full-state adoption and its
  epoch guard region)
- Engine/Source/Network/NetworkProtocol.h (`kiNetworkBufferSize`)

## In scope

- Diagnosing why `iAckFloor` did not advance after full-state adoption.
- The smallest resulting fix inside the ack-floor / full-state adoption /
  epoch-guard logic in the files above.
- Temporary diagnostic logging needed to reproduce, removed or demoted to
  `kVerbose` before completion.

## Out of scope

- The server-side over-capacity drop rule and `kiMaxStatusChangesPerCell`.
- Raising `kiNetworkBufferSize` or removing the disconnect rule as a way to
  hide the symptom.
- Wire format, protocol version, resync policy ownership, and the desync
  detection path itself.
- The collision zone capacity limit tracked separately in
  Documents/Plans/Engine/CollisionZoneCapacityPolicy.md.

## Risk tier and invariants

Expected Change Workflow Tier 3: this is wire/protocol-facing client receive
state feeding resync and replay behavior, which the root AGENTS.md Tier-2
exclusion list names. Preserve PostRender bit determinism, the epoch guard's
stale-payload rejection, and existing connect/disconnect semantics for a
genuinely lagging client.

## Acceptance criteria

- The root cause is stated with `path:line` evidence before the fix.
- After the fix, a client that adopts a full state resumes with an ack floor
  at the adopted tick, verified by log evidence.
- A harness run reproducing the original scenario keeps the client connected
  for at least 400 ticks past the resync, with no
  `Too many missing frames, disconnecting` line.
- A client that is genuinely more than `kiNetworkBufferSize` ticks behind with
  no full-state adoption still disconnects.
- Client and server both build.

## Notes

The single observation is cold-start-conditional; if the diagnosis cannot
prove a root cause, report that outcome rather than applying a defensive
patch.
