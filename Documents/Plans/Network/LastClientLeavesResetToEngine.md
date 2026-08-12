<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-11T00:00:11.000Z","dependsOn":[]} -->
# Move the last-client-leaves reset into ServerSessionRuntime

## Context

At the end of `ServerClientManager::Disconnects` (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp:194-208`) the server unpauses and restores 1x timescale when the last client leaves, so the next connection is served by a normally ticking server. The edge is state-tracked through `mbHadClients` (`ServerClientManager.h:38`, with its explanatory comment at `:36-37`).

Every symbol involved is engine: `engine::gpServer->mClients`, `engine::GameFlags::kPaused`, and `GameBase::mTimeStep` with `miTimeMultiply`/`miTimeDivide`/`SetTimeScale`. Nothing about it is specific to this game, and a second game that omitted it would leave its server paused forever after an agent-paused session emptied out.

`Disconnects` runs inside `ServerSession::AfterNetworkPoll` (`ServerSession.cpp:297`), which the runtime calls once per poll from both `ServerSessionRuntime::Poll` (`Engine/Source/Network/Server/ServerSessionRuntime.cpp:49`) and `PollTickBoundary` (`:62`) — so today the edge check runs once per `AfterNetworkPoll` call.

## Design

Move the block and the state into `engine::ServerSessionRuntime`: a private member `void ResetOnLastClientLeave();` in `ServerSessionRuntime.cpp` plus a `bool mbHadClients = false;` member carrying the moved `ServerClientManager.h:36-37` comment. The condition, the two resets, and the trailing `mbHadClients = bHasClients;` assignment move verbatim, with two mechanical respellings: `engine::gpServer->mClients` becomes `mpServer->mClients` (same object), and `gpGame->` becomes `game::gpGame->` (the established engine-to-game spelling; `ServerSessionRuntime.cpp` gains `#include "Game.h"` if this plan lands before the sibling move plans that also add it).

The runtime calls `ResetOnLastClientLeave()` immediately after `mrSession.AfterNetworkPoll()` in both `Poll` and `PollTickBoundary`, preserving both the current relative order (after the game's per-disconnect cleanup, which runs inside `AfterNetworkPoll`) and the current once-per-`AfterNetworkPoll` cadence. The `mbHadClients` member and the reset block are removed from `ServerClientManager.h`/`.cpp`. No file is added or deleted, so `/update-vcxproj` is not triggered.

## Critical files

- `Engine/Source/Network/Server/ServerSessionRuntime.h`, `ServerSessionRuntime.cpp` — new home; the two call insertions in `Poll` and `PollTickBoundary`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.h`, `ServerClientManager.cpp` — removals

## In scope

- Moving the reset block at `ServerClientManager.cpp:194-208` and the `mbHadClients` member at `ServerClientManager.h:36-38` into the runtime, with the comments intact
- Calling `ResetOnLastClientLeave()` from `Poll` and `PollTickBoundary` directly after `mrSession.AfterNetworkPoll()`
- Any `Engine/Source/Network/Server/AGENTS.md` or game `Network/Server/AGENTS.md` sentence that names the old owner — the game hub's "Trust and Lifecycle Boundaries" bullet ("When the last client leaves, clear network-driven pause and timescale...") describes this behavior and moves or is repointed with the code

## Out of scope

- The rest of `Disconnects` — the disconnect drain, spawn-queue erase, and `mClientPlayers.Remove` stay in the game
- The owned-entity registry the game removal path uses — owned by `Documents/Plans/Network/OwnedEntityRegistryToEngine.md` (that plan touches other lines of `Disconnects`; the two edits are disjoint and order-independent)
- Any change to pause or timescale semantics elsewhere: agent pause commands, `StepTimescale`, and `BroadcastTimespeedIfChanged` are untouched
- `ServerClientManager::NewClients`, `FinalizeNewClients`, `DetectPlayerDeaths`, `RefreshPreSpawnSnapshot`, `ResetState`

## Risk tier and invariants

Tier 3 — server session state that gates whether the simulation ticks at all. Invariants, all documented in the comment being moved: the reset fires on the non-empty to empty transition only, never on a plain `mClients.empty()` test, which would clobber commanded pause and timescale on every update; it catches the edge regardless of removal path, including a ClientHello reject that calls `Server::RemoveClient` without minting a `PendingDisconnect`; a server that was already empty when an agent paused it keeps its commanded pause and timescale; the timescale is written only when it is not already 1:1; the edge tracker updates unconditionally at the end; the check still runs after the game's disconnect processing in the same poll.

## Acceptance criteria

- Server compiles; `mbHadClients` no longer exists in the game layer.
- A harness scenario where a client connects, the server is paused, and the client disconnects leaves the server unpaused at 1x, and a subsequent connection ticks normally.
- An agent pauses an already-empty server; a later update does not clear that pause.
- A rejected ClientHello on an otherwise-empty server still triggers the reset.

## Scores

Effort 1 / Impact 2 / Risk 1
