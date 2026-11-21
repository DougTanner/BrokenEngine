<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T23:55:11.591Z","dependsOn":[]} -->
# Remove the Delayed Connection Response Size Test

## Context

`Client::Poll`'s `ENET_EVENT_TYPE_DISCONNECT` case
(`Engine/Source/Network/Client/Client.cpp:118-145`), compiled under
`if constexpr (keNetworkSimulation != engine::NetworkSimulationLevel::kDisabled)`,
searches `mDelayedPackets` for a delayed connection rejection and delivers it
through `Receive` before disconnect teardown. Its `find_if` predicate
(`Client.cpp:123-139`) runs three steps:

- a size test against
  `NetworkMessages::ServerConnectionResponseMessage::kiMinSize` (`:126-129`),
  with a rationale comment (`:125`) saying the size and type tests come first
  because `Read` throws and `Receive`'s catches do not cover the predicate;
- a type test on `rPacket.data.at(0)` (`:131-134`);
- `NetworkMessages::Read` and the `uiAccepted == 0` test (`:136-138`).

The size test is a structural check the client adds on server data.
`Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy` says the client
trusts what its server sends, adds no new value or structural checks on server
data, and treats corrupt server data as fatal. A past Plan's Fixed decision kept
the size test as a deliberate exception; the user has now chosen to remove it.

Observed behavior today: a delayed packet shorter than `kiMinSize` is skipped by
the predicate and stays in `mDelayedPackets`. If `ProcessOrFlush` later
releases it, `Client::Receive` (`Client.cpp:199-271`) returns silently on an
empty packet (`:201-204`) and processes a short valid packet of another type
normally; only a non-empty packet whose reader throws `std::ios_base::failure`
logs `Client::Receive dropped corrupt packet` and `ASSERT`s (`:262-266`).

`keNetworkSimulation` is `kDisabled` (`Engine/Source/EngineSwitches.h:97`), one
value for every program, so the block is compiled but unreachable in every
current build; any behavior difference exists only when a developer enables
network simulation.

## Design

Delete the size test and the blank line after it (`Client.cpp:126-130`) and its
rationale comment (`:125`).
Keep the type test first, then `Read`, unchanged. The author recommends no
replacement comment: the remaining code follows the documented policy and needs
no local rationale.

Accepted consequence (the user's choice): with network simulation enabled, an
empty delayed packet, or one whose first byte is `kServerConnectionResponse` but
which is shorter than `kiMinSize`, sitting in the delay queue when ENet reports
DISCONNECT, now throws from the predicate (`std::out_of_range` from
`data.at(0)` on an empty packet, `std::ios_base::failure` from `Read` on a short
one). The throw is outside `Client::Receive`'s try/catch (`Client.cpp:208-270`),
so it escapes `Client::Poll` to the main-loop catch and ends the client through
crash reporting instead of being skipped; the
`Client::Receive dropped corrupt packet` log line is not produced on that path.
This is the fatal corrupt-server-data outcome the policy already defines.

No AGENTS.md or `Documents/Architecture/Network.md` text documents the size
test as an exception, so no prose becomes false.
`Engine/Source/Network/Client/AGENTS.md` `## Other`'s "A delayed reliable
rejection is delivered before ENet disconnect teardown" stays true.

## Critical files

- `Engine/Source/Network/Client/Client.cpp`

## In scope

- `Client::Poll`, the `ENET_EVENT_TYPE_DISCONNECT` case's `find_if` predicate:
  delete the size test and the blank line after it (`:126-130`) and the comment
  (`:125`).

## Out of scope

- The type test, the `Read` call, the `uiAccepted` test, and the delivery and
  erase after the search (`:131-144`).
- `Client::Receive` and its catches; the main-loop catch and crash reporting.
- Routing the predicate through `Receive`'s catch, or adding any try/catch.
- `keNetworkSimulation` and the network simulation delay queue.
- Unit tests.

## Risk triggers and invariants

Tier 2: one subsystem's runtime behavior (client handling of a delayed server
packet, reachable only with network simulation enabled); no wire, protocol,
serialization, CRC, threading, or trust change, because the client already
trusts server data under `## Corrupt Input Policy`. No `kiProtocolVersion`
change.

- Server-to-client data stays trusted; the client adds no new check.
- A well-formed delayed rejection is still delivered before teardown.

## Acceptance criteria

1. The diff removes only `Client.cpp:125-130` (the size test, its comment, and
   the blank line after it).
2. `/compile` builds the BrokenEngineSandbox client.

## Notes

- Live verification is not required: no current build compiles the block in,
  because `keNetworkSimulation` is `kDisabled`.
