# Network Server - Host, Slots, and Resends

Server-only engine transport. `Server` (`gpServer`) owns the ENet host, client transport records, subscription slots, update rings, and resend state. It also performs client-to-server admission and violation accounting for both engine and game packets, including handler throws. `ServerSessionRuntime` owns host lifetime, fixed-tick pacing, discovery, poll/tick/paused sequencing, persistent queue servicing, broadcast consumption, resend, flush, and transport reset. `OwnedEntityRegistry` owns the per-client owned-entity records and keeps their `authorizedCoords` mirror index-parallel. The concrete game server session (`../../../../Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md`) supplies game packet contracts and dispatch, authorization, transfer payloads and classification, and gameplay hooks.

## Affinity and Lifetime

- This directory is server-project-only and every source/header remains whole-file `BT_SERVER` guarded.
- ENet host service, compression scratch, queues, and delayed-packet simulation are main-thread-owned.
- Disconnect purges delayed packets for that peer before its transport state can be reused.

## Session Invariants

- Accept a client only after protocol, Frame version, and ordered island-manifest identity pass. Repeated Hello preserves the stored GUID; mint a GUID only for a first accepted client that supplied none.
- Run the client-to-server contract gate before dispatch for both engine and game packets. Variable payload handlers parse into bounded local state before mutation, and contract violations, including game handler throws, use the central accounting/disconnect path.
- `RecordContractViolation` can destroy the client record: on reaching `kiContractViolationDisconnectCount` it pushes the pending disconnect itself, so the game layer still gets the notification and can keep that client's fleet state, then disconnects the peer and removes the record. Treat the call as the last use of that `ClientConnection*` — read whatever you still need before it and return immediately after, or the next line reads freed memory on a path hostile client input can reach.
- Each client independently admits one desync report and one debug-frame request per two seconds of `steady_clock` time after contract validation. Immediate repeats within the per-update cap are silent; over-cap traffic retains the contract-violation policy. A disabled-build debug-frame request remains contract-valid, starts its cooldown, then returns without parsing, logging, lookup, compression, or sending.
- After each complete `AfterNetworkPoll()` hook, `ServerSessionRuntime` samples the engine client set and updates its presence tracker unconditionally. It clears network-driven pause and writes 1:1 timescale only when the current ratio differs, and only on the sampled nonempty-to-empty transition; an already-empty server retains its commanded pause/timescale, and the post-hook sample also catches removal paths that do not enqueue a pending disconnect.
- Each update polls twice: once at update start and once at the tick boundary, after the fixed-tick wait, so commands that arrived during the wait enter the imminent tick. The boundary poll deliberately omits `BeforeNetworkPoll` — that hook clears the previous update's pending request queues, so running it again would drop what the first poll queued — and it continues the first poll's admission budget window, giving each client one budget window per update rather than one per poll.
- Poll-scoped requests drain each poll: fleet request queues are drained by the handler that processes them, so a second poll in the same update cannot reprocess them. New-subscription and resync requests persist until serviced by post-tick broadcast or the paused/zero-tick service path; deduplicate them while pending.
- `ServerSessionRuntime` owns the active coordinate set: it combines subscribed coordinates with the coordinates the game session contributes, always including origin. It then creates a Frame for every coordinate that entered the set and erases the Frames of coordinates that left it, handing each departing Frame to the game session first so game-owned retention can keep it.
- A subscription must be origin or adjacent to an authorized coordinate. Subscription, ACK, and resend-log bookkeeping stay together per slot; allocation stays within the client-supported count, and reuse increments rather than resets the epoch (the reuse counter).
- After existing-client lookup, unsubscribe ACKs every syntactically valid request and frees only an in-range active slot whose epoch matches the request.
- Clamp ACK floors to buffered history and otherwise require them to advance; the one exception is the backward floor a client produces by adopting a coord full state, which sending that full state arms per slot and which is admitted only down to that full state's own tick and only while the tick stays within a bounded window of the latest buffered tick. Treat an all-zero ACK field as latency, not a resend gap, and cap resends per slot per tick.

## Transfers and Publication

- `ServerTransferManager` owns cross-cell transfer harvest, adjacency and destination-liveness validation, deterministic type ordering, and destination materialization. `ServerBroadcaster` owns the broadcast snapshot of a tick's status changes, the drain of agent-injected changes held for a later tick, and the per-coordinate publication `ServerSessionRuntime` hands to the delta resend ring at tick completion.
- A transfer destination must be adjacent. A non-player transfer requires a destination that is already live; a player transfer makes its destination live.
- Recompute each destination Frame's `sharedCrc` after applying arrived transfers. The frame tick computes `sharedCrc` before transfers land, and clients check their own simulated frame against the CRC the server broadcast for that tick, so skipping the recompute desyncs every client on the receiving end of a transfer. This is not defensive recomputation and must not be optimized away as redundant.
- Both classes name `game::StatusChange` and read game Frame state — a deliberate exception to the engine/game contract, because the payload crossing a cell boundary is game-defined while the machinery moving it is not. Their headers therefore stay out of the `Engine.h` aggregation and consumers include them directly.
- Game policy stays on `game::ServerSession` and is reached through `game::gpServerSession`: the replay transfer fixtures, the pending subscription updates, and the agent-injected changes awaiting a tick that can consume them. Harvested transfers additionally go to the engine replay owner for recording (`../../File/AGENTS.md`), which is engine-internal and carries no game policy. Each manager's reset clears the queues it drains, so the game layer needs no second clear.

## Buffered State

- Buffer exactly one delta entry per supplied coordinate per tick in monotonically increasing order; normal simulation supplies the active coordinates and replay may additionally supply a publication-only coordinate. Resend lookup depends on contiguous tick offsets. When game `kbDesyncDebugFrames` is enabled, also buffer the separate full-frame diagnostic ring. Clear both rings across save-load tick resets and prune coordinates that leave the active set.
- Replay may publish a transfer-only coordinate at event tick `E` even though that coordinate is not in the simulation dispatch set until `E + 1`. Keep its per-coordinate buffer active for the supplied replay publication at `E`; prune from the coordinates actually supplied to `BufferFrame`, not from simulation membership. This is publication-only bookkeeping and does not add a wire type or change subscription starvation behavior.
- Preserve ring contiguity even when an update payload cannot be encoded; the resulting client CRC mismatch drives resync.
- Revalidate client, slot, epoch, and coordinate immediately before sending a deferred full state because an unsubscribe or disconnect may have invalidated it in the same poll batch.
- Full-state resync and load notification are transport mechanisms; game state reconstruction remains game-owned.

## See Also

- Game Server Session (`../../../../Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md`)
- Network architecture (`../../../../Documents/Architecture/Network.md`)
