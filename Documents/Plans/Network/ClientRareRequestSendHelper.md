<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-11T00:00:09.000Z","dependsOn":[]} -->
# Move the rare-user-request send helper into ClientSessionRuntime

## Context

`ClientSession::SendGameRequest` (`Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:51-74`, declared at `ClientSession.h:103-104`) is the single path every rare user-initiated client request takes. Its body is entirely engine mechanics:

- Guard: return unless `mpRuntime->mpClient` exists, `ClientStateFlags::kConnected` is set, and `mpServerPeer` is non-null.
- Open a `common::LogTickScope` only when `common::gpThreadLocal->miLogTickCounter < 0`, i.e. when the call happens outside a tick (UI thread), so the log line still carries a tick prefix.
- Invoke the caller's log callable, then `engine::gpClient->SendSimplePacket(ePacketType, kuiChannelReliable, ENET_PACKET_FLAG_RELIABLE, rArgs...)`.
- Flush immediately under `ScopedSuppressAllocationTracking`, implementing the documented rule in `Engine/Source/Network/AGENTS.md` that a rare user request departs now rather than waiting for the tick-cadence ack flush.

The only game-specific thing is the packet-type enum, and only because the parameter is typed `GamePacketType` — `Client::SendSimplePacket` (`Engine/Source/Network/Client/Client.h:78-80`) is already templated on any enum type tag.

## Design

Move the template into `Engine/Source/Network/Client/ClientSessionRuntime.h` as a public `ClientSessionRuntime` member template, adding a leading `typename TPacketType` template parameter so the enum stays the caller's:

`template <typename TPacketType, typename TLogFunction, typename... TArgs> void SendGameRequest(TPacketType ePacketType, const TLogFunction& rLogFunction, const TArgs&... rArgs)`

Body unchanged, including both comments and the allocation-suppression scope, with these spellings: `mpRuntime->mpClient` becomes `mpClient` (guard and send both go through the runtime's own member — `engine::gpClient` and `mpClient` are the same object), `mpRuntime->FlushOutgoing()` becomes `FlushOutgoing()`.

One name cannot move into the header verbatim: `gpGame->TickCounter()`. `game::gpGame` is declared in the game's `Game.h`, which no engine header may include, and a non-dependent name in a header template must resolve at definition. Add a small non-template private helper `int64_t CurrentGameTick() const;` declared in `ClientSessionRuntime.h` and defined in `ClientSessionRuntime.cpp` as `return game::gpGame->TickCounter();` (that TU adds `#include "Game.h"`, the established engine-TU pattern). The template's tick-scope line becomes `optionalTickScope.emplace(CurrentGameTick());`.

The six game senders — `SendUpdatePlayerRequest`, `SendCreateFleetRequest`, `SendDeleteFleetRequest`, `SendSpawnIntoFleetRequest`, `SendRespawnInFleetRequest`, `SendFleetNavigationDelayRequest` (`ClientSession.cpp:341-387`) — keep their signatures, payload arguments, and log lambdas, and forward to `mpRuntime->SendGameRequest(...)`. The template declaration at `ClientSession.h:103-104` and the definition at `ClientSession.cpp:51-74` are removed. No file is added or deleted, so `/update-vcxproj` is not triggered.

## Critical files

- `Engine/Source/Network/Client/ClientSessionRuntime.h` — new home; `CurrentGameTick` declaration
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp` — `CurrentGameTick` definition; `#include "Game.h"`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.h`, `ClientSession.cpp` — removal and forwarding at the six senders

## In scope

- Moving `SendGameRequest` into `ClientSessionRuntime` templated on the packet-type enum, with the guard, tick-scope, send, and flush steps unchanged, plus the `CurrentGameTick` helper
- Removing the declaration from `ClientSession.h` and updating the six game senders to call the runtime
- Any `Engine/Source/Network/AGENTS.md` or game `Network/AGENTS.md` sentence that names the old owner

## Out of scope

- The game senders' own signatures, payload arguments, or log text
- Received-packet handling in `ClientSession.cpp` and `ClientSessionReceive.cpp` — adoption is owned by `Documents/Plans/Network/ReceivedUpdateAdoptionToEngine.md`
- Any change to channel, reliability flag, flush timing, or the connected-state guard
- `SendSimplePacket` itself and the ack-flush cadence
- The `Players.h` include at the top of `ClientSession.cpp` — owned by `Documents/Plans/Network/ClientSessionUnusedPlayersInclude.md` (different region of the same file; the two plans are order-independent)

## Risk tier and invariants

Tier 2 — client send path only; nothing here is CRC state and the wire bytes are unchanged. Invariants: the guard short-circuits before any log or send; the tick scope is opened only when there is no ambient tick counter, so a call from inside a tick is not double-scoped; the log callable runs before the send so the log line precedes the packet; the immediate flush stays inside `ScopedSuppressAllocationTracking` because ENet may allocate while flushing, and this runs in the allocation-tracked main loop.

## Acceptance criteria

- Client compiles; the helper exists once, in the engine.
- A harness run that issues a fleet create, spawn, and delete shows each request logged with a tick prefix and reaching the server in the same frame it was issued, as before.
- Issuing a request while disconnected is still a silent no-op with no log line.

## Scores

Effort 1 / Impact 2 / Risk 1
