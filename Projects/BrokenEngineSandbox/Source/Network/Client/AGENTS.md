# Network Client - Session and Reconciliation

Client-only game networking. `ClientSession` is the game-policy wrapper over `engine::ClientSessionRuntime`; it owns static-data application, per-frame gameplay hydration and smoke continuity, reconciliation, and desync/gameplay policy. Collection-slot hydration rules are in `../../Frame/Collections/AGENTS.md`.

## Ownership

- `engine::ClientSessionRuntime` owns connection/GUID/discovery, subscription state, clock state, generic received full-state/delta adoption, received-update ring/buffer/confirmation mechanics, and static→full-state→delta drain order. `ClientSession` supplies desired coordinates, applies static data, hydrates gameplay state with smoke continuity, and owns reconciliation and desync policy; static-data arrival also requests matching terrain textures.
- `ClientReconciler` dispatches each coordinate once per client update. Workers write only their assigned `CoordFrames`; result merging, first-wins desync selection, visual-error aggregation, and cross-coordinate player-transfer migration run after dispatch.
- `ClientDesyncManager` coordinates debug capture, resync, and repeated-desync disconnect policy.

## Session Policy

- Persist the client GUID in the versioned app-data `ClientGuid.bin`: load it before connecting, then let the transport's accept callback write it atomically so an interrupted write cannot orphan persistent server state.
- Disconnect clears transport and discovery objects, every coordinate's client state, subscription intent, the latest-server pacing baseline, active correction error/target, correction-log cadence, reconciliation, and desync recovery.
- Preserve subscription orchestration order: remove stale coordinates, recover timed-out transitional slots, rebuild the desired queue, then fill available slots. The runtime drains static data before full states and full states before delta updates; stale deltas are skipped and duplicate ticks keep the first arrival.
- The engine acknowledges received ticks before the runtime adopts them, and the server resends only unacknowledged gaps, so an update discarded during adoption is never resent and leaves reconciliation waiting at that tick forever. When a coordinate's buffered server updates reach their ceiling, the runtime requests resync, synchronously resets game coordinate state, reconciler state, and unwanted-timestamp state through a narrow game hook, clears all received update vectors, and returns false.
- LAN discovery stops after recording a found address. A scan timeout records the timeout, replaces the scanner, and immediately starts a fresh scan.

## Reconciliation Invariants

- A server-validated tick is frozen and must not be simulated again. Matching speculative CRCs advance confirmation without replay; unresolved mismatches or due pending authoritative state enter rollback and replay. Future full states stay queued until due; a due state becomes the authoritative ring base wherever it lands — at the confirmed tick, at a matching replayed tick, or beyond an update gap — and stays the base until a later validated replayed frame supersedes it.
- Preserve render-behind history when advancing confirmed state. Replay and catch-up must remain within the coordinate ring budget: the retained base occupies one ring slot, so the combined replay and catch-up write budget is one less than the ring size and writes can never wrap onto the base.
- Apply transfer `StatusChange`s after each tick, matching server Destroy/Spawn order, and recompute the Frame CRC when transfers modify the result. The server recomputes the same way after its own transfers land, so this side's frame only matches the broadcast CRC if both sides recompute; dropping either recompute turns every cross-cell transfer into a reported desync.
- Server-load notification clears coord, clock, identity, fleet, subscription, and reconciler state before the client accepts post-load data.
- Clock correction remains signed, but applying it to the time-step remainder floors that remainder at zero before the next reconcile advance; a correction must never produce a negative target tick.
- Player-event and fleet-sync handlers have independent log-and-continue exception boundaries. Static-data application and per-frame gameplay hydration remain outside those catches.
- Sticky desired subscriptions reduce visible churn; the engine slot queue owns transport throttling. During a real debug-frame wait or the synthetic full-state fixture stall, transport polling and receive-buffer drains continue while subscription updates, simulation, and reconciliation remain stalled.

Speculative and provisional CRC mismatches log at `kDebug`. Only a mismatch that survives full rollback/replay is a confirmed desync, logs at `kError`, and triggers recovery policy. A confirmed mismatch always reports the differing CRCs. With `kbDesyncDebugFrames` enabled, the client requests the server snapshot, stalls until a response matching the requested tick and coordinate or `kDesyncDebugTimeout`, compares the matching response, then recovers or disconnects. With it disabled, the client never requests or stalls for a real desync and immediately follows `kbDesyncRecovery`; repeated recoveries within the configured window escalate to disconnect.

Detailed fast-path, rollback-base, clock, and full-state behavior belongs in the architecture documents rather than this leaf.

## See Also

- `../../../../../Documents/Architecture/GameReconciliation.md`
- `../../../../../Documents/Architecture/Network.md`
