<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-11T00:00:07.000Z","dependsOn":[]} -->
# Move the three server subscription-servicing functions into ServerSessionRuntime

## Context

Three functions in `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp` service engine-owned subscription queues and contain no game symbol other than the `gpGame` accessors that `engine::GameBase` already provides:

- `PreparePausedSubscriptions` (`:318-337`) — walks `mPendingNewSubscriptions`, lazily builds nav data for each subscribed cell via `engine::BuildCellNavData`, under `ScopedSuppressAllocationTracking`.
- `SendNewSubscriptionFullStates` (`:517-545`) — revalidates each pending subscription's slot against `engine::SubscriptionFlags::kActive` and its coord, sends `SendCoordStaticData` + `SendCoordFullState`, then clears the queue (the documented persist-until-served rule).
- `HandleResyncRequests` (`:547-589`) — walks `mPendingResyncClientIds`, and for every active slot of each still-present client sends a full state, then clears the queue.

Every type they touch is engine: `engine::PendingNewSubscription`, `engine::ClientConnection`, `engine::SubscriptionFlags`, `engine::GridCoord`, `engine::FrameStaticData`, `engine::BuildCellNavData`, `Server::SendCoordStaticData`/`SendCoordFullState`, and `GameBase::mCoordFrames`/`TickCounter()`. They are game-side only by where they were written.

Their callers today are the runtime itself — `ServerSessionRuntime::CompleteTick` (`Engine/Source/Network/Server/ServerSessionRuntime.cpp:101`) and `CompleteUpdate` (`:125-127`) call all three through `mrSession.` — plus one game-side call: `ServerSession::SubscriptionUpdates` calls `SendNewSubscriptionFullStates()` at `ServerSession.cpp:499`.

## Design

Move all three verbatim into `Engine/Source/Network/Server/ServerSessionRuntime.cpp` as `engine::ServerSessionRuntime` members, declared in `ServerSessionRuntime.h`: `SendNewSubscriptionFullStates` public (the game's `SubscriptionUpdates` still calls it), `PreparePausedSubscriptions` and `HandleResyncRequests` private (called only from `CompleteTick`/`CompleteUpdate`). No signature changes: all three take no parameters and return `void`.

Inside the runtime, `mpRuntime->mpServer` becomes `mpServer`, and `gpGame->` becomes `game::gpGame->` — the existing engine-to-game contract spelling (`Engine/Source/GameBase.cpp` and `Engine/Source/Frame/IslandTerrain.cpp` already reach `game::gpGame->mCoordFrames` this way). `ServerSessionRuntime.cpp` adds `#include "Game.h"`, the same include the other engine TUs that touch `game::gpGame` carry. Nothing else in the bodies changes; the file sits inside the existing whole-file `BT_SERVER` guard.

Call-site updates:

- `CompleteTick`/`CompleteUpdate` drop the `mrSession.` prefix on all three calls.
- `ServerSession::SubscriptionUpdates` (`ServerSession.cpp:499`) calls `mpRuntime->SendNewSubscriptionFullStates()`.
- The three declarations leave `ServerSession.h` (`:48-49` public, `:67` private).

No file is added or deleted, so `/update-vcxproj` is not triggered.

## Critical files

- `Engine/Source/Network/Server/ServerSessionRuntime.h`, `ServerSessionRuntime.cpp` — new home; prefix-drop at `CompleteTick`/`CompleteUpdate`; `#include "Game.h"`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.h`, `ServerSession.cpp` — declaration removals; `SubscriptionUpdates` retargeted at `mpRuntime->SendNewSubscriptionFullStates()`

## In scope

- Moving the three functions listed in Context, with their comments, allocation-suppression scopes, and queue-clearing statements unchanged
- Removing their declarations from `ServerSession.h` and rewiring the callers named in Design (the runtime's internal calls and `SubscriptionUpdates`)
- Any `Engine/Source/Network/Server/AGENTS.md` or game `Network/Server/AGENTS.md` sentence that names the old owner (the game hub's "Timing and Paused Availability" bullet describes this servicing; the engine hub already claims "persistent queue servicing" for the runtime)

## Out of scope

- The game-packet contract gate and throw boundary in `ParseReceivedGamePackets` — owned by `Documents/Plans/Network/GamePacketAdmissionGateToEngine.md`
- `AddSubscribedCoords`, `EnsurePlayerCoords`, `SyncActiveFrames`, `ComputeActiveSet` — owned by `Documents/Plans/Network/ActiveSetSkeletonToEngine.md`
- `SubscriptionUpdates` itself beyond the one retargeted call, `ResetClientsForLoad`, `SendAssignPlayer`, `SendPlayerState`, and the fleet and timespeed functions in the same file
- Any change to what is sent, in what order, or to when a queue is cleared

## Risk tier and invariants

Tier 3 — the functions sit on the client/server wire and on the paused-server servicing path. Invariants: the persist-until-served rule (both queues are cleared here, not in `Server::Poll`, so a subscribe or resync accepted while the server is paused survives across polls until serviced); slot revalidation still checks client presence, slot index bound, `kActive`, and coord equality before sending; the static-data-then-full-state send order per subscription; nav data is built at most once per cell (`bNavDataBuilt`) and only when the cell has islands; both allocation-suppression scopes (in `PreparePausedSubscriptions` and `HandleResyncRequests`) stay with the code they cover; `SubscriptionUpdates` still services new-subscription full states before draining its pending assign/state updates.

## Acceptance criteria

- Client and server compile; the three functions no longer appear in the game layer.
- A harness run where a client subscribes to a new neighbouring cell shows the cell's static data and full state arriving and the cell rendering, unchanged from before.
- A subscribe issued while the server is paused is still served once the server resumes, and a forced resync still restores every active slot.

## Scores

Effort 1 / Impact 3 / Risk 1
