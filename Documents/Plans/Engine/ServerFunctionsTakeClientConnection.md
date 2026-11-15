<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:39:56.467Z","dependsOn":[]} -->
# Server functions take the ClientConnection they are given, and client slots are sized to kiCoordinateSlots

## Context

Paths are under `Engine/Source/Network/` unless they start elsewhere. Line
numbers are those of the tree this Plan landed with.

### Functions that take an identifier although every caller holds the record

Three server functions take a client identifier although every caller holds
the `ClientConnection`:

- `ServerBufferedFrames::ApplyAckStream` (`Server/ServerBufferedFrames.cpp:414`,
  declared at `Server/ServerBufferedFrames.h:83`) takes
  `ClientConnection& rClient` and, beside it, `int64_t iClientId`. Its one
  caller, `Server::ClientAcknowledgementStream` (`Server/Server.cpp:525`),
  passes `rClient.iClientId`. The identifier is only logged
  (`Server/ServerBufferedFrames.cpp:448`, `:494`, `:504`).
- `ServerSessionBase::SendOwnedEntityState`
  (`Server/ServerSessionBase.cpp:270-283`) takes `int64_t iClientId`, finds the
  record again with the linear `Server::FindClient` (`:272`), and dereferences
  the result unchecked (`:282`). Its only callers, in
  `ServerSessionBase::RebuildOwnedEntities` (`:233`, `:242`, `:246`), hold the
  client as `const ClientConnection& rClient`.
- `ServerBufferedFrames::SendCoordinateFullState`
  (`Server/ServerBufferedFrames.cpp`) takes a client identifier, calls
  `Server::FindClient` on it, and dereferences the result without a null
  check. `Server::FindClient` (`Server/Server.cpp`) is a linear search of
  `mClients` that returns `nullptr` for an unknown identifier. Every caller
  already holds the `ClientConnection` when it calls:
  - `ServerBufferedFrames::SendUpdate` has `ClientConnection& rClient` and
    passes `rClient.iClientId`.
  - `ServerSessionRuntime::HandleResyncRequests`
    (`Server/ServerSessionRuntime.cpp`) looks the client up itself, skips a
    null result, and then passes `iClientId` inside the slot loop.
  - `ServerSessionRuntime::SendNewSubscriptionFullStates` (same file) looks the
    client up as `const engine::ClientConnection* pClient`, erases the entry
    when it is null or the slot no longer matches, and then passes
    `rSubscription.iClientId`.

  There are no other callers; no game project names the function. The second
  lookup is therefore redundant, and the unchecked dereference is safe only
  because of what the callers did — the signature does not say so. The sibling
  `ServerBufferedFrames::SendCoordinateStaticData` already takes
  `const ClientConnection& rClient`.

### Slot list sized past what the server can allocate

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

Origin: an architecture review (cohesion and data-shape lenses) and an
in-function refactor review of `Engine/Source/Network/Server`, independently
verified, and a refactor candidate logged in
`Documents/Investigations/Engine/RefactorCandidates.md`, validated against
commit `213d2577`. Unmet criteria: a function whose every caller holds the
client record takes the record; the server sizes a client's slot list by the
slot count it can allocate.

## Design

Run the steps in this order. Step 4 relies on the `ApplyAckStream` range check
against `std::ssize(rClient.slots)`, which step 1 leaves in place while editing
that function's signature and log arguments.

1. `ApplyAckStream`: delete the `iClientId` parameter and log
   `rClient.iClientId` at the three sites. `Server::ClientAcknowledgementStream`
   stops passing the identifier.
2. `SendOwnedEntityState`: take `const ClientConnection& rClient` in place of
   the identifier, delete the `FindClient` line, log `rClient.iClientId`, and
   send to `rClient.pPeer`. The three callers in `RebuildOwnedEntities` pass
   `rClient`.
3. `SendCoordinateFullState`: change the first parameter from
   `int64_t iClientId` to `ClientConnection& rClient`, non-const because the
   function writes `rSlot.iPendingFullStateTick`. Inside the function, delete
   the `mrServer.FindClient` line, read the slot and the peer from `rClient`,
   and log `rClient.iClientId` where the identifier was logged, as
   `SendCoordinateStaticData` does. This is recommended over keeping the
   identifier and adding a null check, because the callers already own the
   revalidation (`Engine/Source/Network/Server/AGENTS.md` `## Buffered State`)
   and a reference removes the missing-client case instead of handling it.
   Callers pass what they hold: `rClient` in `SendUpdate`, `*pClient` in
   `HandleResyncRequests`, and `*pClient` in `SendNewSubscriptionFullStates`,
   whose local becomes `engine::ClientConnection* pClient` so it can bind to
   the non-const reference.
4. Resize `connection.slots` to `kiCoordinateSlots` in `Server::Connect`. In
   the same change, add the direct include of the game
   `Network/Server/ServerSession.h` to `Server.cpp`, beside the existing game
   include.

Steps 1-3 preserve behavior: the same packets go to the same peers in the same
order, the same slot write happens, and the log lines are unchanged. Step 4
only shortens the slot list; an index at or above `kiCoordinateSlots` that was
already ignored as an inactive slot is now ignored by the range test.

## Critical files

- `Engine/Source/Network/Server/ServerBufferedFrames.h`
- `Engine/Source/Network/Server/ServerBufferedFrames.cpp`
- `Engine/Source/Network/Server/Server.cpp`
- `Engine/Source/Network/Server/ServerSessionBase.h`
- `Engine/Source/Network/Server/ServerSessionBase.cpp`
- `Engine/Source/Network/Server/ServerSessionRuntime.cpp`

## In scope

`ApplyAckStream`:

- The declaration and definition: the last parameter and the three log
  arguments.
- The call in `Server::ClientAcknowledgementStream`.

`SendOwnedEntityState`:

- The declaration and definition: the first parameter, the `FindClient` line,
  and the uses of the identifier and `pClient`.
- The three calls in `RebuildOwnedEntities`.

`SendCoordinateFullState`:

- The declaration in `ServerBufferedFrames.h`.
- The definition in `ServerBufferedFrames.cpp`: its first parameter, the
  removed `FindClient` line, and the uses of `pClient` and the identifier
  inside it.
- The call in `ServerBufferedFrames::SendUpdate`.
- The call in `ServerSessionRuntime::HandleResyncRequests`.
- The call in `ServerSessionRuntime::SendNewSubscriptionFullStates` and the
  constness of that function's `pClient` local.

`Server.cpp` slot sizing:

- `Server::Connect`: the `connection.slots.resize` argument.
- The game include block of `Server.cpp`: one added include.

## Out of scope

- `Server::FindClient` and every other caller of it.
- The callers' own lookups and their client, slot, and coordinate revalidation.
- The acknowledgement logic in `ApplyAckStream`, including its range check.
- `ServerOwnedEntityStateMessage`, `ServerCoordFullStateMessage`, the channels,
  the packet flags, and anything else on the wire;
  `engine::kiProtocolVersion` does not change.
- The other `ServerBufferedFrames` send functions and their signatures.
- `kiCoordinateSlots`, `kiMaximumEnetCoordinateSlots`, the channel count, and
  the three `static_assert`s at the top of `Server.cpp`.
- The ack-stream entry capacity in `Server::ClientAcknowledgementStream` and
  on the client.
- Every loop over `slots`: each already uses the list's own size.
- Other unused or missing includes in this directory.
- `Documents/Plans/Engine/DropAuthorizedCoordinatesMirror.md` and
  `Documents/Plans/Engine/RemoveOutOfRangeSubscribeAcceptBranch.md`: separate
  Tier 3 Plans.
- `AGENTS.md` files and `Documents/Architecture/Network.md`: none names these
  signatures.

## Risk tier and invariants

Tier 2 under `.agents/references/risk-tiers.md`. Triggers: public member
signatures of `ServerBufferedFrames` change, which Tier 1 excludes; the size of
a container that client-supplied slot indexes are checked against changes.
Everything stays inside the Network Server subsystem. The range checks, the
wire format, and what the server trusts are unchanged. No determinism/CRC,
serialization, saves, replays, threading, or trust-boundary change. The changed
functions stay server-only and main-thread-only.

## Acceptance criteria

- `ApplyAckStream` has no identifier parameter.
- `SendOwnedEntityState` contains no `FindClient` call and takes
  `const ClientConnection&`.
- `SendCoordinateFullState` contains no `FindClient` call and takes
  `ClientConnection&`.
- `ClientConnection::slots` holds `kiCoordinateSlots` entries.
- Every index into `slots` that comes from client data is range-checked
  against the list's size before use (code reading).
- The server project builds through `/compile`; the changed functions are
  `BT_SERVER`-only, so the client build is unaffected. For the three signature
  changes the diff is decisive: each caller passes the client the removed
  lookup would have returned.
- Through `/agent-harness`, one run in which a client connects, subscribes, and
  receives updates and resends as before. The supplied evidence
  records no harness setup.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
