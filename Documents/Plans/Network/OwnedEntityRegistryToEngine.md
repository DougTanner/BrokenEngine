<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-11T00:00:12.000Z","dependsOn":[]} -->
# Generalize ClientPlayerRegistry into an engine owned-entity registry

## Context

`game::ClientPlayerRegistry` (`Projects/BrokenEngineSandbox/Source/Network/Server/ClientPlayerRegistry.h:8-35`, `.cpp`) does two separable jobs.

The first is pure bookkeeping with no game concept in it: it keeps a per-client list of `{engine::global_id_t globalId, engine::GridCoord coord}` pairs in `mOwned`, mirrored index-for-index into `engine::ClientConnection::authorizedCoords`. `Add` (`.cpp:15-24`) pushes to both; `RemoveAt`, `UpdateCoord`, `Owned`, `Remove`, and `Clear` (`.cpp:26-93`) maintain that mirror. The word "Player" appears only in the type name — the data is an owned global id plus the cell it lives in, which is what authorizes a client to receive that cell.

The second is game policy: `RelinkFromFrames` (`.cpp:95-144`) scans `gpGame->mCoordFrames` for `PlayersPostRender` entries whose `pClientGuids[i]` matches, sorts by global id, and calls `gpServerSession->SendAssignPlayer` and `SendPlayerState` per entry (`.cpp:131-132`). That is this game's Player collection and this game's packets.

A second game needs the first half exactly and none of the second.

## Design

Move the bookkeeping half to new files `Engine/Source/Network/Server/OwnedEntityRegistry.h`/`.cpp` (whole-file `BT_SERVER` guard, matching the directory rule): `struct engine::OwnedEntity` with the same two members in the same order, and `class engine::OwnedEntityRegistry` with `Add`, `RemoveAt`, `UpdateCoord`, `Owned`, `Remove`, `Clear`, and the `mOwned` map moved verbatim, including `Add`'s null-client tolerance (`FindClient` may return null when the client was already removed). The `.cpp` includes `Network/Server/Server.h`; no game include is needed. Add the header to `Engine.h`'s server span beside `Network/Server/Server.h`, preserving the documented include order.

`RelinkFromFrames` becomes a public `game::ServerSession` member, and the `RelinkContext` enum moves onto `ServerSession` with it. It already runs server-session-side — it calls `gpServerSession->SendAssignPlayer` and `SendPlayerState` per entry — so `ServerSession` is its natural owner. It keeps its scan, its sort by `globalId.iValue`, its two `reserve` calls, its per-entry `Add` + `SendAssignPlayer` + `SendPlayerState` sequence, and both context-dependent log lines; it calls `mClientPlayers.Add` on the engine registry, and its local vector becomes `std::vector<engine::OwnedEntity>`.

`ClientPlayerRegistry.h`/`.cpp` are then deleted. Call-site updates, exhaustive:

- `ServerSession.h` — `:6` include removed; the engine registry type arrives through the `Engine.h` aggregation the PCH already provides, matching how `ServerSession.h` sees every other engine type; `:53` `mClientPlayers` redeclared as `engine::OwnedEntityRegistry` (member name unchanged); `RelinkFromFrames`/`RelinkContext` declared here.
- `ServerSession.cpp:615-616` — `ResetClientsForLoad` calls its own `RelinkFromFrames(..., RelinkContext::kLoad)`.
- `ServerClientManager.cpp:70` — `ClientPlayerRegistry::RelinkContext::kConnect` becomes `gpServerSession->RelinkFromFrames(..., ServerSession::RelinkContext::kConnect)`; `:48`, `:157`, `:179`, `:191`, `:216`, `:240-242`, `:265`, `:272` keep calling `gpServerSession->mClientPlayers` and respell `OwnedPlayer` as `engine::OwnedEntity`.
- `ServerBroadcaster.cpp:205-218`, `ServerTransferManager.cpp:173-177`, `ServerFleetManager.cpp:375`, `:421-431`, `:440` — `OwnedPlayer` respelled `engine::OwnedEntity`.
- `ServerFleetManager.h:12` — the `struct OwnedPlayer;` forward declaration moves into an `engine` namespace block as `struct OwnedEntity;`; `:107` parameter respelled.

`/update-vcxproj` is triggered: `ClientPlayerRegistry.h`/`.cpp` leave `BrokenEngineSandboxServer.vcxproj` and its `.filters`, and the two new engine files join them under the engine filter alongside `ServerSessionRuntime.*`.

## Critical files

- `Engine/Source/Network/Server/OwnedEntityRegistry.h`, `.cpp` — new engine registry
- `Engine/Source/Engine.h` — aggregation of the new header in the server span
- `Projects/BrokenEngineSandbox/Source/Network/Server/ClientPlayerRegistry.h`, `.cpp` — deleted
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.h`, `ServerSession.cpp` — new home for `RelinkFromFrames` and `RelinkContext`; `mClientPlayers` redeclared
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp`, `ServerBroadcaster.cpp`, `ServerFleetManager.h`, `ServerFleetManager.cpp`, `ServerTransferManager.cpp` — requalification of `OwnedPlayer` and the relink entry points at the lines listed in Design
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj`, `.filters` — membership updates

## In scope

- Moving `OwnedEntity` and the six bookkeeping members named in Design into `engine::OwnedEntityRegistry`, unchanged in behavior
- Moving `RelinkFromFrames` and its `RelinkContext` enum onto `game::ServerSession`, rewired onto the engine registry, and deleting `ClientPlayerRegistry.h`/`.cpp`
- Redeclaring `mClientPlayers` as `engine::OwnedEntityRegistry` held directly on `ServerSession`, requalifying every call site listed in Design, adding the header to `Engine.h`, and updating project/filter membership for the files added and removed
- Any `Engine/Source/Network/Server/AGENTS.md` or game `Network/Server/AGENTS.md` sentence that names the old owner — the game hub's "State Ownership" bullet ("`ServerSession` owns per-client player records...") describes this registry and is updated with the move

## Out of scope

- `SendAssignPlayer` and `SendPlayerState` and their packet layouts
- The `RelinkContext` enum's two log messages and their levels
- Any change to when relink runs (connect and load) or to the sort key
- The last-client-leaves reset in the same disconnect path — owned by `Documents/Plans/Network/LastClientLeavesResetToEngine.md` (disjoint lines of `Disconnects`; order-independent)
- Any change to `engine::ClientConnection` beyond reading and writing `authorizedCoords` as today

## Risk tier and invariants

Tier 3 — `authorizedCoords` is the server's authorization surface for which cells a client may receive. Invariants: `mOwned[iClientId]` and `pClient->authorizedCoords` stay index-parallel through every operation, so a removal at index `i` removes index `i` from both; `Add` tolerates a null client without corrupting `mOwned`; `Clear` empties `authorizedCoords` while keeping the map entry, and `Remove` drops the client entirely — the existing distinction between the two is preserved; relink's deterministic ordering by ascending `globalId.iValue` is unchanged, because the assign/state packets are emitted in that order; no engine-owned shared member or signature names a Player concept (the hub rule in `Engine/Source/AGENTS.md`).

## Acceptance criteria

- Client and server compile; no engine file names a Player concept.
- A harness reconnect with a persistent GUID relinks the same owned entities in the same order and re-authorizes the same cells.
- A save/load cycle relinks identically.
- A client that loses one of several owned entities keeps the rest, and its `authorizedCoords` still matches its owned list one-for-one.

## Scores

Effort 2 / Impact 3 / Risk 2
