<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:39:56.467Z","dependsOn":[]} -->
# Size a server client's slot list to kiCoordinateSlots

## Context

Paths are under `Engine/Source/Network/` unless they start elsewhere. Line
numbers are those of the tree this Plan landed with.

`Server::Connect` sizes `ClientConnection::slots` to
`NetworkManager::kiMaximumEnetCoordinateSlots` (`Server/Server.cpp:166`), the
transport ceiling of 64 (`NetworkManager.h:21`). Every slot the server ever
activates comes from `ClientConnection::AllocateSlot` (`Server/Server.h:103`),
which searches only the first `kiCoordinateSlots` entries
(`NetworkProtocol.h:86`), and the client's pool uses `kiCoordinateSlots` too
(`Client/Client.cpp:24-25`). `Server/AGENTS.md` `## Session Invariants` states
that allocation stays within `kiCoordinateSlots`.

So each client carries slot states past `kiCoordinateSlots` that can never
become active, and every loop over `slots` — `SendUpdate`, `SendResends`,
`FindSlotForCoordinate`, `IsDestinationLive`, the load reset, the Server
Monitor count — walks them each time.

Nothing needs the larger size. The sites that take a slot index from client
data test it against `std::ssize(rClient.slots)` before indexing
(`ServerBufferedFrames::ApplyAckStream`, `Server/ServerBufferedFrames.cpp:426`;
`Server::ClientUnsubscribe`, `Server/Server.cpp:796`;
`Server::SendSubscribeAccept`, `Server/Server.cpp:906`). The ack-stream entry
array and the channel count keep using the transport ceiling
(`Server/Server.cpp:508-512`, `NetworkManager.h:22`) and are unaffected.

Also, `Server/Server.cpp` uses `game::gpServerSession` in `RemoveClient` but
includes only the game `Network/Server/ServerClientManager.h`, not the game
`Network/Server/ServerSession.h` that declares it.

Origin: an architecture review (data-shape lens) of
`Engine/Source/Network/Server`, independently verified. Unmet criterion: the
server sizes a client's slot list by the slot count it can allocate.

## Design

Recommended: resize `connection.slots` to `kiCoordinateSlots` in
`Server::Connect`. In the same change, add the direct include of the game
`Network/Server/ServerSession.h` to `Server.cpp`, beside the existing game
include.

## Critical files

- `Engine/Source/Network/Server/Server.cpp`

## In scope

- `Server::Connect`: the `connection.slots.resize` argument.
- The game include block of `Server.cpp`: one added include.

## Out of scope

- `kiCoordinateSlots`, `kiMaximumEnetCoordinateSlots`, the channel count, and
  the three `static_assert`s at the top of `Server.cpp`.
- The ack-stream entry capacity in `Server::ClientAcknowledgementStream` and
  on the client.
- Every loop over `slots`: each already uses the list's own size.
- Other unused or missing includes in this directory.

## Risk tier

Tier 2 — scoped behavior (the author's classification). Trigger: it changes
the size of a container that client-supplied slot indexes are checked
against, inside one subsystem. The checks, the wire format, and what the
server trusts are unchanged: an index at or above `kiCoordinateSlots` was
already ignored as an inactive slot and is now ignored by the range test. No
determinism/CRC, serialization, or threading change.

## Acceptance criteria

- `ClientConnection::slots` holds `kiCoordinateSlots` entries.
- Every index into `slots` that comes from client data is range-checked
  against the list's size before use (code reading).
- The server builds.
- Through `/agent-harness`, a client connects, subscribes, and receives
  updates and resends as before. The supplied evidence records no harness
  setup.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
