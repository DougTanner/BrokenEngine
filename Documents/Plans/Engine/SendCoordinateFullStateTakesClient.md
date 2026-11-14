<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T23:03:18.818Z","dependsOn":[]} -->
# SendCoordinateFullState takes the client it already has

## Context

`ServerBufferedFrames::SendCoordinateFullState`
(`Engine/Source/Network/Server/ServerBufferedFrames.cpp`) takes a client
identifier, calls `Server::FindClient` on it, and dereferences the result
without a null check. `Server::FindClient`
(`Engine/Source/Network/Server/Server.cpp`) is a linear search of `mClients`
that returns `nullptr` for an unknown identifier.

Every caller already holds the `ClientConnection` when it calls:

- `ServerBufferedFrames::SendUpdate` has `ClientConnection& rClient` and passes
  `rClient.iClientId`.
- `ServerSessionRuntime::HandleResyncRequests`
  (`Engine/Source/Network/Server/ServerSessionRuntime.cpp`) looks the client up
  itself, skips a null result, and then passes `iClientId` inside the slot
  loop.
- `ServerSessionRuntime::SendNewSubscriptionFullStates` (same file) looks the
  client up as `const engine::ClientConnection* pClient`, erases the entry when
  it is null or the slot no longer matches, and then passes
  `rSubscription.iClientId`.

There are no other callers; no game project names the function. The second
lookup is therefore redundant, and the unchecked dereference is safe only
because of what the callers did — the signature does not say so. The sibling
`ServerBufferedFrames::SendCoordinateStaticData` already takes
`const ClientConnection& rClient`.

Origin: a refactor candidate logged in
`Documents/Investigations/Engine/RefactorCandidates.md`, validated against
commit `213d2577`.

## Design

Recommended: change the first parameter from `int64_t iClientId` to
`ClientConnection& rClient`, non-const because the function writes
`rSlot.iPendingFullStateTick`. Inside the function, delete the
`mrServer.FindClient` line, read the slot and the peer from `rClient`, and log
`rClient.iClientId` where the identifier was logged, as
`SendCoordinateStaticData` does. This is recommended over keeping the
identifier and adding a null check, because the callers already own the
revalidation (`Engine/Source/Network/Server/AGENTS.md` `## Buffered State`) and
a reference removes the missing-client case instead of handling it.

Callers pass what they hold: `rClient` in `SendUpdate`, `*pClient` in
`HandleResyncRequests`, and `*pClient` in `SendNewSubscriptionFullStates`,
whose local becomes `engine::ClientConnection* pClient` so it can bind to the
non-const reference.

Behavior is unchanged: the same packet, the same slot write, and the same log
line are produced for the same client.

## Critical files

- `Engine/Source/Network/Server/ServerBufferedFrames.h`
- `Engine/Source/Network/Server/ServerBufferedFrames.cpp`
- `Engine/Source/Network/Server/ServerSessionRuntime.cpp`

## In scope

- The `SendCoordinateFullState` declaration in `ServerBufferedFrames.h`.
- The `SendCoordinateFullState` definition in `ServerBufferedFrames.cpp`: its
  first parameter, the removed `FindClient` line, and the uses of `pClient` and
  the identifier inside it.
- The call in `ServerBufferedFrames::SendUpdate`.
- The call in `ServerSessionRuntime::HandleResyncRequests`.
- The call in `ServerSessionRuntime::SendNewSubscriptionFullStates` and the
  constness of that function's `pClient` local.

## Out of scope

- `Server::FindClient` and every other caller of it.
- The callers' own lookups and their client, slot, and coordinate revalidation.
- `ServerCoordFullStateMessage`, the channel, the packet flags, and anything
  else on the wire; `engine::kiProtocolVersion` does not change.
- The other `ServerBufferedFrames` send functions and their signatures.
- `AGENTS.md` files and `Documents/Architecture/Network.md`: none names this
  signature.

## Risk tier and invariants

Tier 2 under `.agents/references/risk-tiers.md`: the change is
behavior-preserving but alters a public member signature of
`ServerBufferedFrames`, which Tier 1 excludes, and stays inside the Network
Server subsystem. It does not touch determinism/CRC, the wire format,
serialization, saves, replays, threading, or the trust boundary. The function
stays server-only and main-thread-only.

## Acceptance criteria

- `SendCoordinateFullState` contains no `FindClient` call and takes
  `ClientConnection&`.
- The server project builds through `/compile`; the function is
  `BT_SERVER`-only, so the client build is unaffected.

## Notes

- No live verification is needed beyond the build: the diff is decisive, since
  each caller passes the same client the removed lookup would have returned.
