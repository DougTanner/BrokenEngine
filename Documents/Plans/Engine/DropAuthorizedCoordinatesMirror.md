<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:39:47.066Z","dependsOn":[]} -->
# Drop the authorizedCoordinates mirror of the owned-entity registry

## Context

`REG` is `Engine/Source/Network/Server/OwnedEntityRegistry.cpp`, `SSB` is
`Engine/Source/Network/Server/ServerSessionBase.cpp`, and `BES` is
`Projects/BrokenEngineSandbox/Source`. Verified against commit `4cf9d607`.

Each client's authorized coordinates are held twice:

- `OwnedEntityRegistry::mOwned`, a per-client list of
  `{globalId, coord}` records, written at `REG:14`, `REG:25`, `REG:36`,
  erased on disconnect (`BES/Network/Server/ServerClientManager.cpp:316`),
  and read at `SSB:221`, `SSB:313`, and in `ServerClientManager.cpp`.
- `ClientConnection::authorizedCoordinates`
  (`Engine/Source/Network/Server/Server.h:54`), written at `REG:19`,
  `REG:30`, `REG:41` and read in two places: the subscribe authorization
  check in `Server::ClientSubscribe`
  (`Engine/Source/Network/Server/Server.cpp:740`) and the Server Monitor
  client count (`Engine/Source/Server/ServerMonitorHost.cpp:149`).

The registry is called only from `SSB:250`, `SSB:251`, `SSB:254`, and
`SSB:305`. Each of its three methods looks the client up again with
`Server::FindClient` and tests for null only to maintain the mirror.
`Engine/Source/Network/Server/AGENTS.md` (lines 14-16) says the registry keeps
the mirror "index-parallel", but no reader indexes the two lists together, so
that invariant has no consumer.

Reading the registry gives both readers the same answer: the two stores
change together, client identifiers are never reused (`miNextClientId`,
`Server.cpp:165`), and subscribe handling precedes the rebuild within a poll
(`ServerSessionRuntime::Poll`).

Origin: an architecture review (data-shape and cohesion lenses) of
`Engine/Source/Network/Server`, independently verified. Unmet criterion: one
store holds each client's authorized coordinates.

## Design

Recommended:

- Delete `ClientConnection::authorizedCoordinates`.
- In `OwnedEntityRegistry::Add`, `Clear`, and `Reserve`, delete the
  `FindClient` lookup and the mirror write. Keep `Clear`'s `try_emplace`:
  `ServerSessionBase::ResetForLoad` reads `mOwned.at(...)` (`SSB:313`) and
  relies on the entry existing.
- The two readers look the client up in `mOwned` and walk its records' `coord`
  members, treating a missing entry as an empty list.
- Update the documents and comments that name the mirror.

The alternative of making the map private or folding the registry into
`ServerSessionBase` is not part of this Plan.

## Critical files

- `Engine/Source/Network/Server/Server.h`
- `Engine/Source/Network/Server/Server.cpp`
- `Engine/Source/Network/Server/OwnedEntityRegistry.cpp`
- `Engine/Source/Network/Server/ServerSessionBase.cpp`
- `Engine/Source/Server/ServerMonitorHost.cpp`
- `Engine/Source/Network/Server/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md`

## In scope

- `ClientConnection::authorizedCoordinates` in `Server.h`.
- `OwnedEntityRegistry::Add`, `Clear`, `Reserve`: the `FindClient` half of
  each, and the `Server.h` include if it is then unused.
- `Server::ClientSubscribe`: the source list of the adjacency `any_of`.
- The client-count loop in `ServerMonitorHost.cpp` that walks
  `authorizedCoordinates`. It reads the registry as
  `game::gpServerSession->mClientOwnedEntities.mOwned`, which that file does
  not use today; the `Game.h` it already includes brings in the game
  `Network/Server/ServerSession.h` in the server build, the route `Server.cpp`
  also takes, so no include is added.
- The two "Heap:" comments in `SSB` (`RebuildOwnedEntities`, `ResetForLoad`)
  that name `authorizedCoordinates`.
- `Engine/Source/Network/Server/AGENTS.md`: the opening paragraph's
  `OwnedEntityRegistry` sentence.
- `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md`
  `## State Ownership`: the sentence naming the engine-authoritative
  `authorizedCoordinates`.

## Out of scope

- The adjacency rule itself (origin, or grid distance 1 of an authorized
  coordinate) and its overflow-safe distance test.
- `OwnedEntityRegistry::mOwned`'s type and visibility, and its game readers.
- `ServerSessionBase::RebuildOwnedEntities` beyond its comment.
- The monitor wire format and what the monitor counts.

## Risk tier

Tier 3 — invariant/integration (the author's classification). Trigger: it
changes the data the subscribe authorization check reads, a trust boundary for
hostile client input, and spans two independently owned subsystems
(`Network/Server` and the Server Monitor host). The check's result is meant to
be unchanged. No wire, determinism/CRC, serialization, or threading change.

## Acceptance criteria

- `authorizedCoordinates` no longer exists in code or documents.
- A subscribe is accepted for the origin or a coordinate within grid distance
  1 of one of the client's owned-entity coordinates and rejected otherwise,
  as before; a client with no registry entry authorizes only the origin.
- The server builds.
- Through `/agent-harness`, a connected client subscribes to its owned
  entity's cell and neighbours and receives updates, and the Server Monitor
  client count per cell is unchanged. The supplied evidence records no
  harness setup.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
