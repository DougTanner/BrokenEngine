# Network Server - Host, Slots, and Resends

Server-only engine transport. `Server` (`gpServer`) owns the ENet host, client
transport records, and subscription slots, plus client-to-server admission and
violation accounting for engine and game packets, including handler throws.
`ServerBufferedFrames`, a member of `Server`, owns the update rings and resend
state. `ServerSessionRuntime` owns host lifetime, fixed-tick pacing, discovery,
poll/tick/paused sequencing, queue servicing, broadcast consumption, resend,
flush, and transport reset. `ServerSessionBase`, which the game server session
inherits, holds the owned-entity registry, the transfer manager, the
broadcaster, and the runtime, and owns tick preparation, timescale stepping,
the active set's ownership step, the owned-entity rebuild and its state
notifications, the load reset, and the deferred client-packet drain with the
debug-control request handlers. `OwnedEntityRegistry` owns per-client
owned-entity records and keeps their `authorizedCoordinates` mirror
index-parallel. The game server session (the game project's own
`Source/Network/Server/AGENTS.md`) supplies game packet contracts and handlers,
authorization, transfer payloads and classification, and the gameplay functions
the engine calls by name.

## Affinity and Lifetime

- This directory is server-project-only and every source/header remains whole-file `BT_SERVER` guarded.
- ENet host service, compression scratch, queues, and delayed-packet simulation are main-thread-owned.
- Disconnect purges delayed packets for that peer before its transport state can be reused.

## Session Invariants

- Accept a client only after protocol, Frame version, and ordered
  island-manifest identity pass. Repeated Hello preserves the stored GUID; mint
  a GUID only for a first accepted client that supplied none. A nonempty GUID
  belongs to at most one live handshaken client; a Hello carrying one already
  held by another handshaken client is rejected and disconnected. A client
  record is removed only through `RemoveClient`, which synchronously notifies
  the game client manager (`ServerClientManager::OnClientDisconnected`) with the
  record's id and stored GUID before erasing it, because the later ENet
  DISCONNECT event finds no record.
- `Server` owns the current debugging-load generation and stamps connection
  bootstrap and all load-scoped subscription traffic. It rejects a subscription
  request whose generation differs from the current world after finding the
  handshaken client and before coordinate lookup, slot allocation, or
  publication. `ServerSessionBase::ResetForLoad` advances the generation before
  notification, slot clearing, or reset-state mutation. Wire width, exhaustion,
  and client behavior: [Network
  architecture](../../../../Documents/Architecture/Network.md#debug-load-generation).
- Run the client-to-server contract gate before dispatch for engine and game packets. Variable payload handlers parse into bounded local state before mutation; contract violations, including game handler throws, use the central accounting/disconnect path.
- `RecordContractViolation` never removes the client record: when the recorded
  kind's count reaches its limit, it disconnects the peer and keeps the record,
  so callers may keep using their `ClientConnection` after the call. No record
  is removed inside the deferred client-packet drain:
  `Server::RemoveDisconnectedClients` runs at every poll before
  `RebuildOwnedEntities`, and `RemoveClient` purges the removed client's
  deferred packets. Limits, gates, and the drop and removal mechanics:
  [Client to Server Contract](../../../../Documents/Architecture/Network.md#client--server-contract).
- Each client admits one desync report and one debug-frame request per two seconds of `steady_clock` time after contract validation; immediate repeats within the per-update cap are silent, and over-cap traffic keeps the contract-violation policy. A disabled-build debug-frame request with a valid tick starts its cooldown, then returns without logging, lookup, compression, or sending.
- A stored client crash report is written atomically, and a report dropped by the cap or a failed write shows only in the server log. Per-connection and per-run limits, storage, and the absence of a reply: the `kClientCrashReport` row of [Client to Server Contract](../../../../Documents/Architecture/Network.md#client--server-contract).
- After each complete `AfterNetworkPoll()`, `ServerSessionRuntime` samples the engine client set and updates its presence tracker unconditionally; it clears network-driven pause and writes 1:1 timescale only on the sampled nonempty-to-empty transition and only when the ratio differs, so an already-empty server keeps its commanded pause/timescale.
- Each update polls twice, at update start and at the tick boundary after the fixed-tick wait, so commands that arrived during the wait enter the imminent tick. The boundary poll deliberately omits `BeforeNetworkPoll`, which clears the previous update's pending request queues, and continues the first poll's admission budget window: one budget window per update, not one per poll.
- The fixed-tick wait consumes the carried tick remainder but does not maintain it; `TimeStep::TickRealtime`, which `GameBase` calls immediately after the wait, accumulates and wraps it. Preserve the `siTotalTicks`/`siOvershootTicks` counters and their profiling-gated verbose log when changing this path.
- Poll-scoped requests drain each poll: the handler that processes a game request queue drains it, so a second poll in the same update cannot reprocess it. Deduplicate the new-subscription and resync requests that persist per [Timing and Polling](../AGENTS.md#timing-and-polling) while pending.
- `ServerSessionRuntime` owns the active coordinate set — subscribed
  coordinates plus every cell holding an owned entity
  (`game::Frame::OwnedEntityCount`) or whose current frame is pinned (nonzero
  `FramePostRenderBase::uiPinned`) (`ServerSessionBase::AddOwnedEntityCoordinates`),
  always including origin. It creates a Frame for each entering coordinate and
  hands ownership of each departing current Frame directly to the engine replay
  owner before erasing the coordinate storage (`../../File/AGENTS.md#replay-streams`).
- `ServerSessionBase::RebuildOwnedEntities` is the only writer of the owned-entity
  registry apart from the disconnect erase and the load clear. For each
  handshaken client it collects every row carrying the client's GUID in any
  ownership layer of any cell (`game::Frame::OwnershipLayers`,
  `../../Frame/AGENTS.md`), sorted by global ID, and
  reports the difference against the previous list on the reliable channel the
  load notification also uses: first `Died` for each lost entity, in descending
  global-ID order, each followed by the game's `OnOwnedEntityDied`; then, in
  ascending order, `Spawned` for each new entity and `ChangedCell` for each one
  that moved. It runs at every poll after the deferred client-packet drain and
  before `AfterNetworkPoll`, so new-client handling sees the rebuilt list; in
  `CompleteTick` after resync servicing and before the game's
  `FinalizeTickClients`; and inside the load reset. Transfers land before
  `CompleteTick` and carry the owner's GUID onto the destination row, so a live
  handoff is reported as `ChangedCell`, never as a death. A disconnected owner's
  rows match no handshaken client and register again when that GUID reconnects.
- A subscription must be origin or adjacent to an authorized coordinate. Subscription, ACK, and resend-log bookkeeping stay together per slot; allocation stays within `kiCoordinateSlots`, the slot count the client's pool also uses, and reuse increments rather than resets the epoch.
- Answer every current-generation subscribe from a handshaken client exactly
  once on the control channel — a reject, a new slot, or, for a coordinate that
  already holds an active slot, that existing slot — because the client matches
  each answer to its oldest outstanding request for the coordinate. The
  already-subscribed answer does not queue the request again. A handshaken
  client's subscribe and unsubscribe are exempt from the per-update budget
  ([Client to Server Contract](../../../../Documents/Architecture/Network.md#client--server-contract)).
- After existing-client lookup, unsubscribe ACKs every syntactically valid request and frees only an in-range active slot whose epoch matches the request.
- Clamp ACK floors to buffered history and otherwise require them to advance. The one exception is the backward floor a client produces by adopting a coord full state: sending that full state arms it per slot, and it is admitted only down to that full state's own tick and only while the tick stays within a bounded window of the latest buffered tick. Treat an all-zero ACK field as latency, not a resend gap, and cap resends per slot per tick.

## Transfers and Publication

- `ServerTransferManager` owns cross-cell transfer harvest, adjacency and
  destination-liveness validation, deterministic type ordering, and destination
  materialization. It also holds spawns decided outside the tick (`QueueSpawn`)
  until the first tick at or after their target tick whose coordinate is active
  with a committed current Frame, and adds them to that tick's post-tick batch,
  which is applied right after the tick with its arrivals; adjacency and
  liveness rules apply to transfers only. `ServerBroadcaster` owns the broadcast
  snapshot of a tick's status changes, the drain of agent-injected changes held
  for a later tick, and the per-coordinate publication `ServerSessionRuntime`
  hands to the delta resend ring at tick completion.
- Keep every workbuffer-backed publication view valid until synchronous delta
  and optional full-frame buffering have consumed it. Preflight capacity for the
  complete publication block before forming any view, including the codec's
  nested compression scratch bound, `kiMaxCompressStatusChangeWorkbufferBytes`,
  which is sized at `kiMaximumStatusChangesPerCell`; a batch above that constant
  grows the workbuffer in place through `Workbuffer::Grow`, which keeps every
  view valid. Update that bound whenever status compression adds or enlarges
  workbuffer reservations.
- A transfer destination must be adjacent. A non-owned transfer requires a destination that is already live; an owned-entity transfer makes its destination live.
- Recompute each destination Frame's `uiSharedCrc` after applying its post-tick batch (arrived transfers and queued spawns): the frame tick computes it before that batch lands, and clients check their own simulated frame against the broadcast CRC for that tick, so skipping the recompute desyncs every client receiving a transfer or spawn. This is not defensive recomputation and must not be optimized away as redundant.
- Both classes name `game::StatusChange` and read, create, and pass whole game
  Frames and their state, such as `postRender.transferRequests` — a deliberate
  exception to the engine/game contract, because the payload crossing a cell
  boundary is game-defined while the machinery moving it is not. Owned-entity
  identity is not part of that exception: it goes through the engine ownership
  layer (`../../Frame/AGENTS.md`). Their headers therefore stay out of the
  `Engine.h` aggregation and consumers include them directly. The two managers
  also use these game names directly, naming no gameplay type:
  `game::IsOwnedEntityTransfer` (whether a transfer carries an owned entity),
  `game::Frame::OwnershipLayers`, `game::Frame::OwnedEntityCount`,
  `game::ApplySpawnStatusChange`,
  `game::IsSpawnType`, `game::IsTransferType`, `game::SpawnPosition`,
  `game::OwnedSpawnGlobalId`, `game::OwnedSpawnClientGuid`,
  `game::ServerSession::OnOwnedEntityChangedCell`, and
  `game::ServerSession::BuildAdvancingFrameInputs`.
- The harness rig status-change queue comes from the game Agent's session-bound
  state (the game project's own `Source/Agent/AGENTS.md` `## Contracts`). The
  broadcaster drains injected changes and takes its snapshot once per
  normal-play tick, so an injected change can enter any tick of a multi-tick
  update, and its reset clears that queue. The transfer manager drains its spawn
  queue after natural collection and before deterministic sorting, and its reset
  clears that queue.
  Each accepted transfer batch goes to the engine replay owner before live
  application (`../../File/AGENTS.md#replay-streams`); capture failure
  invalidates recording but never vetoes the accepted batch.

## Buffered State

- Buffer exactly one delta entry per supplied coordinate per tick in monotonically increasing order; resend lookup depends on contiguous tick offsets. With `kbDesynchronizationDebugFrames` enabled, also buffer the separate full-frame diagnostic ring. Clear both rings across save-load tick resets, and prune from the coordinates actually supplied to `BufferFrame`, not from simulation membership, so a replay coordinate published before it joins the dispatch set keeps its buffer.
- When an active slot's coordinate delta ring restarts after a gap, send the current authoritative full state before any delta or resend and hold both update paths until an accepted matching-epoch ACK reaches the pending full-state tick. A further restart while held supersedes the target with its newer tick; reuse an existing same-tick full state instead of sending a duplicate. Initial subscriptions, continuous rings, and ordinary explicit resync keep their existing ACK behavior.
- Revalidate client, the slot's active flag, and coordinate immediately before sending a deferred full state because an unsubscribe or disconnect may have invalidated it in the same poll batch.
- A queued new subscription stays queued until it is served or invalid. It is served only once its coordinate has a Frame whose navigation data is built, because static data always carries navigation data; an entry whose client, slot, or coordinate no longer matches is erased. A resync sends no full state to a slot whose new subscription is still queued ([Subscription Receive Invariants](../Client/AGENTS.md#subscription-receive-invariants)).
- Full-state resync and load notification are transport mechanisms; game state reconstruction remains game-owned.

## See Also

- Network architecture (`../../../../Documents/Architecture/Network.md`) - Protocol flow and ACK timing
