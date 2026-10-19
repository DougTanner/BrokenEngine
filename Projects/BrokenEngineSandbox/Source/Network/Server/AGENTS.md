# Network Server - Game Session and Managers

Server-only game networking. `ServerSession` inherits
`engine::ServerSessionBase`, which holds the owned-entity registry, the
engine-owned transfer and broadcast managers, and `engine::ServerSessionRuntime`,
the owner of host/discovery/pacing and the fixed poll/tick/paused phase order
(`../../../../../Engine/Source/Network/Server/AGENTS.md`). `ServerSession` owns
the client and fleet managers and supplies the game payloads and policy queues
the engine managers work on; the game functions the engine calls by name mutate
game state around the deterministic Frame tick.

## State Ownership

- The engine session base rebuilds the `engine::OwnedEntityRegistry` from the Frame ownership layers and reports each entity whose row is gone from every cell to `OnOwnedEntityDied`, which notifies the fleet manager. Game code uses those per-client global ID and coordinate records, not the engine-authoritative `authorizedCoordinates`, for owned-entity lookup.
- Fleets are keyed by persistent `ClientGuid` so they survive disconnect: disconnect clears that GUID's client id and that client's queued fleet requests, never the fleet, and its ships keep running. A separate reap pass handles deaths in owner-gone fleets — marking members dead and shifting the flagship — because the connected-client death path skips them by design.
- Fleet RNG is seeded once, serialized with fleet state, and consumes exactly two 64-bit draws per generated fleet identifier.
- Fleet serialization supplies the game half of an engine grid save through the four entry points the engine reader requires (`../../../../../Engine/Source/File/AGENTS.md`) — write the fleet payload into the save stream, read it into an isolated staged value the engine carries uninspected, adopt that value into the live fleet manager, and reset fleet state for a fresh game.
- A fleet's position within its client's list shifts on delete, so requests and queued work that outlive a poll carry the generated fleet identifier and re-resolve it at consumption.
- A fleet member is identified by its ship's global ID, which a respawn keeps; stored member references — the flagship, a respawn request, a queued spawn, and client focus — carry that ID, never a list position.
- Each client's owned-entity records are in global-ID order; because a respawn keeps its ID, that order is each member's first-spawn order.
- A dead fleet member respawns automatically after a countdown that drains only while its owner is connected.
- The [game Agent](../../Agent/AGENTS.md#contracts) owns the session-bound harness rig slot and its clearing semantics; teardown detaches it before destroying the runtime.

## Timing and Paused Availability

- Poll transport and LAN discovery run before simulation work on every update, including while paused: a zero-tick update still builds navigation data for pending subscriptions and services and flushes the resync and new-subscription full-state queues, so a newly connected client receives initial state without a simulation tick.
- An update polls more than once and `AfterNetworkPoll` runs at every poll, so each handler must empty the request queue it processes; a request left queued is applied a second time later in the same update. `BeforeNetworkPoll` clears only queues whose requests are meant to expire unprocessed.

## Deterministic Tick Contracts

- The engine owns the active set, which keeps every coordinate holding at least one fleet-controlled ship, because those rows are the owned entities (`Frame::OwnedEntityCount`). Retention after the departing-Frame handoff, and generation activation, belong to the [File Replay contract](../../../../../Engine/Source/File/AGENTS.md#replay-streams).
- Normal update prepares tick inputs; replay uses coordinates with live readers and consumes its recorded `FrameInput` order exactly, without sorting it again.
- A replay transfer recorded in the post-dispatch channel at event tick `E` is applied and published after dispatch at `E`; the newly activated destination first dispatches at `E + 1`.
- A spawned fleet ship row is born owned by the requesting client's GUID carried in the spawn the advancing update queues; its first tick's owned-entity rebuild registers it and reports it `Spawned`, and no pairing pass runs. That GUID is the persistent ownership key.
- Fleet navigation defers flagship updates through `StatusChange`s. Within
  `ServerSession::BuildAdvancingFrameInputs`, which the engine
  `ServerBroadcaster::BuildFrameInputs` calls after its frame-input reset,
  queued owned-entity updates, fleet timers and pending flagship updates, and
  waiting-client spawn construction run once per update and only
  on advancing updates, staying deferred through paused and other zero-tick
  updates. Waiting-client spawns queue on the engine transfer manager for the
  update's first tick, whose harvest drains them into its post-tick batch. The
  agent status-change drain and the broadcast snapshot run in every normal-play
  tick's `PrepareTick`, after those once-per-update natural changes.
- Agent-injected `StatusChange`s remain queued until the first normal tick at or after the entry's target tick whose coordinate is active and has a committed current Frame: a destroy or update entry then enters that tick's input at `PrepareTick`, and a spawn entry joins that tick's post-tick batch at its harvest. Consumed changes then follow the engine-owned status-change ordering.
- Transfer validation, deterministic ordering, destination materialization and
  CRC recompute, and publication assembly are engine-owned
  ([engine Server contract](../../../../../Engine/Source/Network/Server/AGENTS.md#transfers-and-publication)).
  The game half is the transfer payloads, waiting-client spawn coordination,
  and fleet notification; injected-status harness rig state
  follows the [game Agent contract](../../Agent/AGENTS.md#contracts).

## Trust and Lifecycle Boundaries

- Engine `Server` owns packet admission and violation accounting, and the engine session base drains admitted packets and records handler throws; this layer supplies game contracts and handles each admitted game packet in `HandleGamePacket`. Side-specific bounds validate navigation timing, fleet sizes, and save/replay fleet data without partially applying a request.
- Connect-time new-client processing begins only after the engine accepts `ClientHello` and marks the handshake complete, and runs after the owned-entity rebuild has registered the client's surviving owned entities. Notify the fleet manager even when the client has no owned entities, because a persistent fleet can outlive all its owned entities and still needs synchronization on reconnect.
- The engine load reset (`engine::ServerSessionBase::ResetForLoad`) advances
  the debugging-load generation before notification, slot clearing, or
  transient reset, and rebuilds client ownership before it calls
  `OnResetForLoad`. That function clears pending flagship updates before any
  fleet `OnResetForLoad`, then clears client and request transient state after
  restored fleet state is read; the engine then resets its two managers, which
  clear their own queues (engine Server contract), so no second clear belongs
  here. Do not erase restored fleet RNG or
  the navigation updates fleet `OnResetForLoad` queues.

## See Also

- `../../../../../Documents/Architecture/Network.md`
