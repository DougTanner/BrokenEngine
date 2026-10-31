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

- The engine session base rebuilds the `engine::OwnedEntityRegistry` from the Frame ownership layers and reports each entity whose row is gone from every cell to `OnOwnedEntityDied`, which the game leaves empty. Game code uses those per-client global ID and coordinate records, not the engine-authoritative `authorizedCoordinates`, for owned-entity lookup.
- Fleets are keyed by persistent `ClientGuid` so they survive disconnect: disconnect clears that GUID's client id and that client's queued fleet requests, never the fleet, and its ships keep running. `DetectFleetMemberDeaths` is the one member death pass, for connected and offline owners alike: after each tick it marks a live member dead when its recorded cell is no longer active or no longer holds its row.
- Fleet RNG is seeded once, serialized with fleet state, and consumes exactly two 64-bit draws per generated fleet identifier.
- Fleet serialization supplies the game half of an engine grid save through the four entry points the engine reader requires (`../../../../../Engine/Source/File/AGENTS.md`) — write the fleet payload into the save stream, read it into an isolated staged value the engine carries uninspected, adopt that value into the live fleet manager, and reset fleet state for a fresh game.
- Fleets are permanent: no request deletes one, so a fleet's position within its client's list never shifts. Requests and queued work still carry the generated fleet identifier and resolve it at consumption.
- A fleet member is identified by the global ID minted when the member is added, unique across every owner's fleets, which every spawn of it keeps; stored member references — the flagship, a queued spawn, and client focus — carry that ID, never a list position.
- Each client's owned-entity records are in global-ID order; because each member's ID is minted when it is added, that order is the members' add order.
- Every member, the fleet's Battleship included, is added dead with an expired countdown, so its first spawn and every respawn take the same path. A dead member respawns automatically when its type's countdown ends — 0 s for a Fighter, 1 s (`kRespawnDelay`) for an Omni or Battleship — and the countdown drains whether or not its owner is connected.
- The [game Agent](../../Agent/AGENTS.md#contracts) owns the session-bound harness rig slot and its clearing semantics; teardown detaches it before destroying the runtime.

## Timing and Paused Availability

- Poll transport and LAN discovery run before simulation work on every update, including while paused: a zero-tick update still builds navigation data for pending subscriptions and services and flushes the resync and new-subscription full-state queues, so a newly connected client receives initial state without a simulation tick.
- An update polls more than once and `AfterNetworkPoll` runs at every poll, so each handler must empty the request queue it processes; a request left queued is applied a second time later in the same update. `BeforeNetworkPoll` clears only queues whose requests are meant to expire unprocessed.

## Deterministic Tick Contracts

- The engine owns the active set, which keeps every coordinate holding at least one fleet-controlled ship, because those rows are the owned entities (`Frame::OwnedEntityCount`). Retention after the departing-Frame handoff, and generation activation, belong to the [File Replay contract](../../../../../Engine/Source/File/AGENTS.md#replay-streams).
- Normal update prepares tick inputs; replay uses coordinates with live readers and consumes its recorded `FrameInput` order exactly, without sorting it again.
- A replay transfer recorded in the post-dispatch channel at event tick `E` is applied and published after dispatch at `E`; the newly activated destination first dispatches at `E + 1`.
- A spawned fleet ship row is born owned by its owner's GUID carried in the spawn the per-tick fleet step queues; its first tick's owned-entity rebuild registers it and reports it `Spawned`, and no pairing pass runs. That GUID is the persistent ownership key.
- An Omni or Battleship spawns at the first clear candidate `FindFleetSpawnPoint` returns, with no random draw: the
  search is a pure function of the candidates, the origin's committed frame, and the pass's taken points, so replay
  playback reaches the recording's spawn-or-wait decision, as the fleet checksum (`../../Save/AGENTS.md`) requires.
  With none clear the member stays dead and is retried on the next tick. `ServerClientManager.cpp` owns the candidate
  set, the clearance, and the blocked-episode logging. A Fighter launches from inside its fleet's
  Battleship: it spawns in that row's cell with its heading, velocity, and stored plane,
  `kfBattleshipRadius - kfFighterRadius` ahead of its position along the heading, so that Battleship's own pusher
  pushes it out the front under the [shared ship policy](../../Frame/Collections/Ships/AGENTS.md#ship-invariants). At most
  one Fighter launches per fleet per `kfShipSpawnInterval`, timed by `ServerClientManager::mNextFighterLaunchTicks`,
  which every load reset clears; a skipped Fighter stays dead and is tried again on the next tick.
- `ServerSession::BuildAdvancingFrameInputs`, which the engine
  `ServerBroadcaster::BuildFrameInputs` calls after its frame-input reset on
  advancing updates, is empty: no queued client request becomes a
  once-per-update `StatusChange`.
- The fleet step runs once per tick: fleet timers, then respawn timers and the
  spawns they make, each draining one tick's `kfDeltaTime`. Both timer passes
  walk owners in GUID order, because each fleet's direction draw consumes the
  one fleet RNG and each fleet spawn search keeps clear of the points earlier
  spawns of its pass took, so replay playback must walk the recording's order. In normal play the
  engine's `PrepareTick` runs it after the tick's frame inputs exist and before
  the agent status-change drain and the broadcast snapshot, so the `StatusChange`s
  the fleet timers write into the frame input when a flagship picks a new cell
  enter that tick's broadcast and recorded input, and each spawn queued on the
  engine transfer manager joins that tick's post-tick batch. On a later tick of
  a multi-tick update, `ComputeActiveSet` can erase a cell whose frame input
  survives until the next `BuildFrameInputs`; the step still reads a frame
  input's cell with `mCells.at` unchecked because it reaches frame inputs only
  through a live member's cell, which holds that member's owned row (or, for a
  just-queued spawn, is the origin or its flagship's cell) and so stays active.
  Replay playback reruns the step on every dispatching tick with every
  bookkeeping effect but no frame change and no queued spawn, because playback
  takes every frame change from the recording.
- Agent-injected `StatusChange`s remain queued until the first normal tick at or after the entry's target tick whose coordinate is active and has a committed current Frame: a destroy or update entry then enters that tick's input at `PrepareTick`, and a spawn entry joins that tick's post-tick batch at its harvest. Consumed changes then follow the engine-owned status-change ordering.
- Transfer validation, deterministic ordering, destination materialization and
  CRC recompute, and publication assembly are engine-owned
  ([engine Server contract](../../../../../Engine/Source/Network/Server/AGENTS.md#transfers-and-publication)).
  The game half is the transfer payloads, fleet member spawn coordination,
  and fleet notification; injected-status harness rig state
  follows the [game Agent contract](../../Agent/AGENTS.md#contracts).

## Trust and Lifecycle Boundaries

- Engine `Server` owns packet admission and violation accounting, and the engine session base drains admitted packets and records handler throws; this layer supplies game contracts and handles each admitted game packet in `HandleGamePacket`. Side-specific bounds validate navigation timing, fleet sizes, and save/replay fleet data without partially applying a request.
- While the server plays a replay, `HandleGamePacket` refuses every game request before dispatch with a warning, because playback takes every simulation change from the recording and a request sent before the client learned of playback still arrives. A refusal for timing is not corrupt data, so it records no contract violation.
- Connect-time new-client processing begins only after the engine accepts `ClientHello` and marks the handshake complete, and runs after the owned-entity rebuild has registered the client's surviving owned entities. Notify the fleet manager even when the client has no owned entities: it makes the GUID's first fleet, with that fleet's Battleship, when the GUID has none, and a persistent fleet whose members are all dead still needs synchronization on reconnect.
- During replay playback the fleet manager skips only that first-fleet creation,
  since playback takes every fleet from the recording; it still maps the client
  and sends its fleet sync. When playback ends by abort, load, or reset, the
  load reset hands a client still without a fleet back to new-client
  processing, which makes its first fleet. Accepted limit: a loop re-read that
  fails on replay files changed during playback ends playback with no load
  reset, so such a client stays without its first fleet until the next load
  reset or reconnect.
- The engine load reset (`engine::ServerSessionBase::ResetForLoad`) advances
  the debugging-load generation before notification, slot clearing, or
  transient reset, and rebuilds client ownership before it calls
  `OnResetForLoad`. That function rebuilds
  every owner's fleet members from the cells' ownership rows
  (`RebuildFleetMembersForLoad`), and leaves every fleet's saved course, then
  clears client and request transient state after restored fleet state is read;
  the engine then resets its two managers, which clear their own queues (engine
  Server contract), so no second clear belongs here. Do not erase restored
  fleet RNG.

## See Also

- `../../../../../Documents/Architecture/Network.md`
