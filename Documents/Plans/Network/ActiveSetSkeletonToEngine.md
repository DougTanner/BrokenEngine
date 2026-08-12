<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-11T00:00:14.000Z","dependsOn":[]} -->
# Move the server active-set skeleton into ServerSessionRuntime

## Context

Which cells the server simulates each update is decided by three functions in `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp`, and the skeleton is engine policy documented in the game hub `Network/Server/AGENTS.md` ("The active set combines subscriptions and player coordinates, always including origin"):

- `AddSubscribedCoords` (`:339-356`) — union of every client slot's coord whose subscription carries `engine::SubscriptionFlags::kActive`, deduplicated into `mActiveCoords`.
- `SyncActiveFrames` (`:372-395`) — create a frame at each active coord that has none, then erase every `mCoordFrames` entry outside the active set.
- `ComputeActiveSet` (`:397-412`) — clear under `ScopedSuppressAllocationTracking`, add subscribed coords, add game-required coords, force-add `engine::kOriginCoord`, then sync.

Two game-specific steps are interleaved: `EnsurePlayerCoords` (`:358-370`) adds every coord holding at least one Player, and `SyncActiveFrames` hands each retiring frame to `mGameSaveLoad.RetainReplayEndFrame` (`:392`) before erasing it. Everything else is engine: `engine::ClientConnection`, `SubscriptionFlags`, `GridCoord`, `kOriginCoord`, and `GameBase::mCoordFrames` plus the game members the engine already reaches through `game::gpGame` (`mActiveCoords`, `CreateFrameAtCoord` — same access `Engine/Source/GameBase.cpp:183`, `:699-716` uses today).

## Design

Move the three skeleton functions into `engine::ServerSessionRuntime` (`ServerSessionRuntime.h`/`.cpp`): `ComputeActiveSet` public, `AddSubscribedCoords` and `SyncActiveFrames` private. Mechanical respellings only: `engine::gpServer->mClients` becomes `mpServer->mClients`, `gpGame->` becomes `game::gpGame->` (the `.cpp` gains `#include "Game.h"` if a sibling move plan has not already added it). Two explicit game hooks replace the interleaved game steps, both `game::ServerSession` members called through the existing `mrSession` reference (the runtime is already a friend):

- `void AddGameRequiredCoords()` — called from `ComputeActiveSet` between `AddSubscribedCoords` and the origin force-add; the game implements it with the current `EnsurePlayerCoords` body, which stays in the game because it reads `PlayersPostRender`.
- `void OnFrameRetiring(engine::GridCoord coord, std::unique_ptr<game::Frame>& rpFrame)` — called from `SyncActiveFrames` immediately before each `erase`; the game implements it as the current `RetainReplayEndFrame(coord, rpFrame)` call. The parameter is the owning `std::unique_ptr` reference because `GameSaveLoad::RetainReplayEndFrame` (`Save/GameSaveLoad.h:67`) takes it by non-const reference and may take ownership of the frame.

Ordering is preserved exactly: clear, subscribed, game-required, origin, sync — and inside sync, create-then-erase with the retiring hook before each erase.

Caller updates, exhaustive: `ServerSession::PrepareTick` (`ServerSession.cpp:52`) calls `mpRuntime->ComputeActiveSet()`; `Game::ComputeActiveSet`'s `BT_SERVER` arm (`Game.cpp:172`) and the two `GameSaveLoad` sites (`GameSaveLoad.cpp:454` load path, `:471` `ServerReset`) call `gpServerSession->mpRuntime->ComputeActiveSet()`. `ServerSession.h` loses the `ComputeActiveSet` declaration (`:39`) and the three helper declarations (`:70-72`), gaining the two hook declarations (private; the runtime friend declaration already grants access). No file is added or deleted, so `/update-vcxproj` is not triggered.

## Critical files

- `Engine/Source/Network/Server/ServerSessionRuntime.h`, `ServerSessionRuntime.cpp` — new home and hook call sites
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.h`, `ServerSession.cpp` — removals, the two hook implementations, and the `PrepareTick` call at `ServerSession.cpp:52`
- `Projects/BrokenEngineSandbox/Source/Game.cpp:172` — `gpServerSession->ComputeActiveSet()` in the `BT_SERVER` arm of `Game::ComputeActiveSet`, retargeted at the runtime
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:454`, `:471` — the two `game::gpServerSession->ComputeActiveSet()` calls, retargeted at the runtime

## In scope

- Moving `AddSubscribedCoords`, `SyncActiveFrames`, and `ComputeActiveSet` into the runtime with unchanged order, dedup behavior, and allocation-suppression scope
- Adding the two hooks named in Design and implementing them in the game from the existing `EnsurePlayerCoords` body and the existing `RetainReplayEndFrame` call
- Updating every `ComputeActiveSet` caller to the moved function's new home: `ServerSession.cpp:52`, `Game.cpp:172`, and `GameSaveLoad.cpp:454` and `:471`
- Any `Engine/Source/Network/Server/AGENTS.md` or game `Network/Server/AGENTS.md` sentence that names the old owner (the game hub's active-set bullet quoted in Context)

## Out of scope

- Subscription servicing in the same file — owned by `Documents/Plans/Network/ServerSubscriptionServicingToEngine.md`
- The game-packet admission gate in the same file — owned by `Documents/Plans/Network/GamePacketAdmissionGateToEngine.md`
- `CreateFrameAtCoord` and frame construction
- `GameSaveLoad` and replay-writer semantics beyond calling the existing retain function at the existing point
- The client-side arm of `Game::ComputeActiveSet` and `GameBase::PrepareActiveSet`
- Any change to which coords qualify, including the origin force-add and the 3x3 authorized ring

## Risk tier and invariants

Tier 3 — the active set decides which cells tick, so it drives deterministic simulation, save/replay coverage, and what clients can receive. Invariants: origin is always in the set; `mActiveCoords` stays deduplicated; frames are created before any erase pass; a retiring frame reaches the game's replay retention before destruction, or a recording writer loses its final complete frame; the erase loop keeps its iterator-safe `it = erase(it)` form; the whole computation stays inside `ScopedSuppressAllocationTracking` because `mActiveCoords` may reallocate inside the armed main loop; a subscription only counts while `kActive`; the game-required hook runs between the subscribed union and the origin force-add, exactly where `EnsurePlayerCoords` runs today.

## Acceptance criteria

- Server compiles; the three functions exist once, in the engine.
- A harness run with a client moving between cells shows the same set of cells ticking before and after, verified from active-coord logging or an agent query.
- A recorded replay ends with the same final frame per coord as a replay recorded before the change, and replays back deterministically.
- With no clients connected, only origin ticks.

## Scores

Effort 2 / Impact 3 / Risk 2
