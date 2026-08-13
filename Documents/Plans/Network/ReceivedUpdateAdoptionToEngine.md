<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-11T00:00:13.000Z","dependsOn":[]} -->
# Move received full-state and update adoption into ClientSessionRuntime

## Context

`Projects/BrokenEngineSandbox/Source/Network/Client/ClientSessionReceive.cpp` holds the client's adoption of everything the server sent, and almost all of it is engine ring and buffer mechanics:

- `ApplyReceivedFullStates` (`:35-125`) — per received full state: find or create the `engine::CoordFrames` entry; on first full state for a coord seed ring slot 0, stamp `Crcs()`, set `iConfirmedTick`/`iLastFullStateTick`/`iConfirmedOffset`, and — only during initial setup — offset the sim tick back by the `engine::kiJitterSafetyUs` floor, clamped at `iTick` so a fresh post-load server cannot produce a negative tick; on a later full state, reject a stale one (`iTick <= iConfirmedTick`) and otherwise queue it as `pendingFullState` for reconcile injection.
- `ApplyReceivedUpdates` (`:127-186`) — per subscription slot: skip and clear when the slot is not `kActive`; skip updates at or before the confirmed tick; advance `miLatestServerTick`; guard the `engine::kiMaxBufferedFrames` overflow; insert with `try_emplace` so a duplicate tick is first-wins.
- The three queries `GetConfirmedTick`, `GetClientConfirmedTick`, `GetServerUpdateBufferSize` (`:188-222`) — minimum confirmed tick across coords, the client cell's confirmed tick, and the total buffered update count.

One block is genuinely game-specific: `:53-75`, which calls `BlastersInterpolate::ClientInitAll`, `MissilesInterpolate::ClientInitAll`, and `SpaceshipsInterpolate::ClientInitAll` on the incoming frame, then copies smoke-trail smoothed positions from the ring's tail frame to preserve rendering continuity across reconciliation.

Callers today: the runtime itself drains through `mrSession.ApplyReceivedFullStates()` / `ApplyReceivedUpdates()` (`Engine/Source/Network/Client/ClientSessionRuntime.cpp:233-234`), and the three queries are read by `Profile/ProfileManager.cpp:270`, `:283`, `:286` (plus `ApplyReceivedFullStates`'s own `GetConfirmedTick()` use at `:80`). `ApplyReceivedStaticData` (`:13-33`) stays in the game and is not part of this move.

## Design

Move `ApplyReceivedFullStates`, `ApplyReceivedUpdates`, and the three queries with their allocation-suppression scopes into `Engine/Source/Network/Client/ClientSessionRuntime.cpp` as `ClientSessionRuntime` members (public: the queries and nothing else needs game visibility; the two apply functions are private since only `PollAndDrain` calls them), verbatim except for one extracted hook and mechanical respellings: `mpRuntime->mpClient` becomes `mpClient`, `mpRuntime->miLatestServerTick` becomes `miLatestServerTick`, `gpGame->` becomes `game::gpGame->` (the established engine-to-game spelling; `ClientSessionRuntime.cpp` adds `#include "Game.h"` — `GetClientConfirmedTick`'s read of `game::gpGame->mClientGridCoord` follows the same precedent as `GameBase.cpp:481`).

The hook: `void HydrateReceivedFullState(game::Frame& rReceived, const game::Frame* pRingTail)` — a `game::ClientSession` member implemented in `ClientSessionReceive.cpp` with the current `:53-75` body, receiving the tail ring frame pointer (null when the ring is empty) instead of reaching into `CoordFrames` itself. The engine calls `mrSession.HydrateReceivedFullState(...)` at exactly the current point, per received full state, before the confirmed-tick branch.

`kTickNs` and `kfDeltaTime` at `:101` and `:105` remain referenced as the game symbols they are today; the engine already references `game::kTickNs` from `Engine/Source/Frame/TimeStep.cpp`, so this move introduces no new kind of engine-to-game reference. `Documents/Plans/Frame/EngineOwnedFrameConstants.md` removes that qualification independently, in either order.

`PollAndDrain` (`:233-234`) drops the `mrSession.` prefix on the two apply calls; `ProfileManager.cpp:270`, `:283`, `:286` retarget to `gpClientSession->mpRuntime->`. The five declarations leave `ClientSession.h` (`:61-62`, `:65-67`; `:60` `ApplyReceivedStaticData` stays); `HydrateReceivedFullState` is declared there instead. No file is added or deleted, so `/update-vcxproj` is not triggered.

`Documents/Plans/Network/ClientAckAdoptionOverflowGap.md` changes the overflow path inside `ApplyReceivedUpdates`; it lands first (the `dependsOn` edge) so this move carries the fixed body rather than replaying the merge. The two pending plans that edit `ClientSessionRuntime.cpp`'s `ResetForServerLoad` (`Documents/Plans/Network/ClientLoadClockJitterReset.md`, `Documents/Plans/Network/ClientLoadResetGamePacketDrain.md`) touch a different region of the same file and are order-independent with this move.

## Critical files

- `Engine/Source/Network/Client/ClientSessionRuntime.h`, `ClientSessionRuntime.cpp` — new home; prefix drops in `PollAndDrain`; `#include "Game.h"`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSessionReceive.cpp` — reduced to `ApplyReceivedStaticData` plus `HydrateReceivedFullState`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.h` — declaration removals; `HydrateReceivedFullState` declaration
- `Projects/BrokenEngineSandbox/Source/Profile/ProfileManager.cpp` — the three query call sites retargeted (`:270`, `:283`, `:286`)

## In scope

- Moving `ApplyReceivedFullStates`, `ApplyReceivedUpdates`, and the three tick/buffer queries into the runtime, with their comments, log lines, `DEBUG_BREAK()`, and allocation-suppression scopes unchanged
- Extracting `HydrateReceivedFullState` from `:53-75` and calling it from the moved code at the same point
- Rewiring every caller named in Context: the runtime's `PollAndDrain` and the three `ProfileManager.cpp` sites
- Any `Engine/Source/Network/Client/AGENTS.md` or game `Network/Client/AGENTS.md` sentence that names the old owner (the game hub's hydration-ownership sentences describe this split)

## Out of scope

- The overflow-recovery behavior itself — owned by `Documents/Plans/Network/ClientAckAdoptionOverflowGap.md`, which lands first
- `ApplyReceivedStaticData`, which stays a game function
- Reconciliation and pending-full-state injection, which consume `pendingFullState` but live elsewhere
- Engine ACK tracking, resend policy, and the wire format
- Desync detection in `ClientDesyncManager`
- `SendGameRequest` and the client send path — owned by `Documents/Plans/Network/ClientRareRequestSendHelper.md`
- Requalifying `kTickNs`/`kfDeltaTime` — owned by `Documents/Plans/Frame/EngineOwnedFrameConstants.md`

## Risk tier and invariants

Tier 3 — client reconciliation state feeding CRC comparison against server broadcasts. Invariants: stale full states (`iTick <= iConfirmedTick`) are rejected with the same log line; the first full state for a coord seeds ring slot 0 and stamps `sharedCrc` from `Crcs()`; the initial jitter-safety offset applies only on initial setup and only when `TickCounter() < iTick`, and is clamped at `iTick`; updates at or before the confirmed tick are dropped; a non-active slot's updates are cleared, not buffered; the `kiMaxBufferedFrames` guard keeps its `kWarning` log and `DEBUG_BREAK()`; `try_emplace` keeps duplicate ticks first-wins; `bHasNewData` is still set on the overflow path; the smoke-trail copy still uses `min(old, new)` counts so a shrunk collection cannot overrun; the hook runs for every received full state (both the seeding and the pending branch), before the confirmed-tick branch, exactly as the block does today.

## Acceptance criteria

- Client compiles; the five functions exist once, in the engine, and only `ApplyReceivedStaticData` and `HydrateReceivedFullState` remain in the game file.
- A harness connect shows the client seeding from the first full state, starting behind the server by the jitter-safety floor, and reconciling with matching CRCs.
- Subscribing to a neighbouring cell mid-session seeds that cell without disturbing the tick counter.
- Smoke trails do not visibly jump across a reconciliation that injects a full state.
- A replay determinism run is unchanged, and the profile overlay still shows confirmed-tick, rollback, and buffer numbers.

## Scores

Effort 2 / Impact 3 / Risk 2
