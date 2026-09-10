# Network Server - Game Session and Managers

Server-only game networking. `ServerSession` is the game-policy wrapper over `engine::ServerSessionRuntime`, which owns host/discovery/pacing and the fixed poll/tick/paused phase order. It owns the client and fleet managers and holds the engine-owned transfer and broadcast managers (`../../../../../Engine/Source/Network/Server/AGENTS.md`), supplying the game payloads and policy queues they work on; game hooks mutate game state around the deterministic Frame tick.

## State Ownership

- `ServerSession` holds the engine-owned `engine::OwnedEntityRegistry` and owns the game relink policy that rebuilds it from Frame state. Game code uses those per-client global ID and coordinate records, not the engine-authoritative `authorizedCoords`, for owned-player lookup.
- Fleets are keyed by persistent `ClientGuid` so they survive disconnect: disconnect clears only that GUID's client id, never the fleet, and its ships keep running. A separate reap pass handles deaths in owner-gone fleets — marking members dead and shifting the flagship — because the connected-client death path skips them by design.
- Fleet RNG is seeded once, serialized with fleet state, and consumes exactly two 64-bit draws per generated fleet identifier.
- Fleet serialization supplies the game half of an engine grid save through the four entry points the engine reader requires (`../../../../../Engine/Source/File/AGENTS.md`) — write the fleet payload into the save stream, read it into an isolated staged value the engine carries uninspected, adopt that value into the live fleet manager, and reset fleet state for a fresh game.
- A fleet's position within its client's list shifts on delete, so requests and queued work that outlive a poll carry the generated fleet identifier and re-resolve it at consumption.
- Entities being re-attached are sorted by global ID before rebuilding client ownership, so reconnect and save-load preserve creation order.
- The [game Agent](../../Agent/AGENTS.md#contracts) owns the session-bound fixture slot and its clearing semantics; teardown detaches it before destroying the runtime.

## Timing and Paused Availability

- Poll transport and LAN discovery run before simulation work on every update, including while paused: a zero-tick update still builds navigation data for pending subscriptions and services and flushes the resync and new-subscription full-state queues, so a newly connected client receives initial state without a simulation tick.
- An update polls more than once and the after-poll hook runs at every poll, so each handler must empty the request queue it processes; a request left queued is applied a second time later in the same update. The before-poll hook clears only queues whose requests are meant to expire unprocessed.

## Deterministic Tick Contracts

- The engine runtime owns the active set and calls two hooks here: contribute every coordinate holding at least one Player, and forward ownership of each departing current Frame to the engine replay owner before the runtime erases the coordinate storage. Retention after that handoff, and generation activation, belong to the [File Replay contract](../../../../../Engine/Source/File/AGENTS.md#replay-streams).
- Normal update prepares tick inputs; replay uses coordinates with live readers and consumes its recorded `FrameInput` order exactly, without sorting it again.
- Apply transfer ownership relinks before death detection: a transferred Player updates its client/GUID ownership record before the tick's death pass, so a live handoff is never treated as a death. A replay transfer recorded in the post-dispatch channel at event tick `E` is applied and published after dispatch at `E`; the newly activated destination first dispatches at `E + 1`.
- Spawn assignment diffs origin player IDs across the tick and pairs new IDs with waiting clients in request order. The client GUID written into Frame state is the persistent relink key.
- Fleet navigation defers flagship updates through `StatusChange`s. Within `BuildFrameInputs`, waiting-client spawn construction, queued player updates, fleet timers and pending flagship updates, plus broadcast and pre-spawn snapshot capture run only on advancing updates, staying deferred through paused and other zero-tick updates.
- Agent-injected `StatusChange`s remain queued until a normal, advancing, frame-ready tick can consume them; consumed changes then follow the engine-owned status-change ordering.
- Transfer validation, deterministic ordering, destination materialization and CRC recompute, and publication assembly are engine-owned ([engine Server contract](../../../../../Engine/Source/Network/Server/AGENTS.md#transfers-and-publication)). The game half is the transfer payloads, the ownership relink above, waiting-client spawn coordination, fleet notification, and pending subscription updates; replay-transfer and injected-status fixture state follows the [game Agent contract](../../Agent/AGENTS.md#contracts).

## Trust and Lifecycle Boundaries

- Engine `Server` owns packet admission and violation accounting, including handler throws; this layer supplies game contracts, dispatches admitted packets, and catches/logs handler failures. Side-specific bounds validate navigation timing, fleet sizes, and save/replay fleet data without partially applying a request.
- Connect-time new-client processing, including its ownership relink, begins only after the engine accepts `ClientHello` and marks the handshake complete. After every eligible relink attempt, notify the fleet manager even when no Players were found, because a persistent fleet can outlive all its Players and still needs synchronization on reconnect.
- A client is dead only after all owned players are gone; skip death handling while a player is mid-transfer.
- Load reset advances the engine-owned debugging-load generation before notification, slot clearing, or transient reset. It then clears client, transfer, and broadcast transient state after restored fleet state is read; resetting the two engine managers clears their game Agent fixture queues through the existing hooks, so no second clear belongs here. Do not erase restored fleet RNG or pending navigation updates.

## See Also

- `../../../../../Documents/Architecture/Network.md`
