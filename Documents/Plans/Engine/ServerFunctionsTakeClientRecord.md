<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:39:17.129Z","dependsOn":[]} -->
# ApplyAckStream and SendOwnedEntityState use the client record they are given

## Context

Paths are under `Engine/Source/Network/Server/`. Line numbers are those of the
tree this Plan landed with.

Two functions take a client identifier although every caller holds the
`ClientConnection`:

- `ServerBufferedFrames::ApplyAckStream` (`ServerBufferedFrames.cpp:414`,
  declared at `ServerBufferedFrames.h:83`) takes `ClientConnection& rClient`
  and, beside it, `int64_t iClientId`. Its one caller,
  `Server::ClientAcknowledgementStream` (`Server.cpp:525`), passes
  `rClient.iClientId`. The identifier is only logged
  (`ServerBufferedFrames.cpp:448`, `:494`, `:504`).
- `ServerSessionBase::SendOwnedEntityState` (`ServerSessionBase.cpp:270-283`)
  takes `int64_t iClientId`, finds the record again with the linear
  `Server::FindClient` (`:272`), and dereferences the result unchecked
  (`:282`). Its only callers, in `ServerSessionBase::RebuildOwnedEntities`
  (`:233`, `:242`, `:246`), hold the client as `const ClientConnection&
  rClient`.

`ServerBufferedFrames::SendCoordinateFullState` has the same shape and is
already recorded in
`Documents/Plans/Engine/SendCoordinateFullStateTakesClient.md`.

Origin: an architecture review (cohesion lens) and an in-function refactor
review of `Engine/Source/Network/Server`, independently verified. Unmet
criterion: a function whose every caller holds the client record takes the
record.

## Design

Recommended:

- `ApplyAckStream`: delete the `iClientId` parameter and log
  `rClient.iClientId` at the three sites.
- `SendOwnedEntityState`: take `const ClientConnection& rClient` in place of
  the identifier, delete the `FindClient` line, log `rClient.iClientId`, and
  send to `rClient.pPeer`. The three callers pass `rClient`.

The same packets go to the same peers in the same order, and the log lines
are unchanged.

## Critical files

- `Engine/Source/Network/Server/ServerBufferedFrames.h`
- `Engine/Source/Network/Server/ServerBufferedFrames.cpp`
- `Engine/Source/Network/Server/Server.cpp`
- `Engine/Source/Network/Server/ServerSessionBase.h`
- `Engine/Source/Network/Server/ServerSessionBase.cpp`

## In scope

- The `ApplyAckStream` declaration and definition: the last parameter and the
  three log arguments.
- The `ApplyAckStream` call in `Server::ClientAcknowledgementStream`.
- The `SendOwnedEntityState` declaration and definition: the first parameter,
  the `FindClient` line, and the uses of the identifier and `pClient`.
- The three `SendOwnedEntityState` calls in `RebuildOwnedEntities`.

## Out of scope

- `SendCoordinateFullState` and its callers (the Plan named above).
- `Server::FindClient` and every other caller of it.
- The acknowledgement logic in `ApplyAckStream`.
- `ServerOwnedEntityStateMessage`, the channel, and the packet flags.
- `AGENTS.md` files: none names either signature.

## Risk tier

Tier 2 — scoped behavior (the author's classification). Trigger: a public
member signature of `ServerBufferedFrames` changes, which Tier 1 excludes; the
change stays inside the Network Server subsystem and preserves behavior. No
determinism/CRC, wire, serialization, threading, or trust-boundary change.

## Acceptance criteria

- `ApplyAckStream` has no identifier parameter.
- `SendOwnedEntityState` contains no `FindClient` call and takes
  `const ClientConnection&`.
- The server builds. The diff is decisive: each caller passes the client the
  removed lookup would have returned.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
