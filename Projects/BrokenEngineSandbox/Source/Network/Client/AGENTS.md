# Network Client - Session and Reconciliation

Client-only game networking. `ClientSession` is the game-policy wrapper over `engine::ClientSessionRuntime`. The reconciliation replay chain — CRC fast path, rollback, replay, catch-up — desync reporting and escalation, and all detailed rollback-base, clock, and full-state behavior are engine-owned (`../../../../../Engine/Source/Network/Client/AGENTS.md`, `../../../../../Documents/Architecture/GameReconciliation.md`). Collection-slot hydration rules are in `../../Frame/Collections/AGENTS.md`.

## Ownership

- Engine-owned: connection/GUID/discovery, subscription and clock state, generic full-state/delta adoption, the received-update ring/buffer/confirmation mechanics and drain order, clock correction, the per-coordinate reconcile work list and its parallel dispatch, and the desync record's declaration, reported-desync selection, and remaining core.
- `ClientSession` supplies desired coordinates, applies static data (also requesting matching terrain textures), hydrates gameplay state with smoke continuity, and owns reconciliation policy, resync reset sequencing, the client/server frame-difference logging the engine desync core calls back into, and the agent full-state fixture policy (`../../Agent/AGENTS.md`).
- `ClientReconciler` runs one engine reconcile dispatch per client update and owns what stays game-side: confirmed client-state tracking, filling in the engine-owned desync record handed back to the session, and the visual-error aggregation and cross-coordinate player-transfer migration it computes from the dispatch result.

## Session Policy

- Persist the client GUID in versioned app-data `ClientGuid.bin`: load it before connecting, then let the transport accept callback write it atomically so an interrupted write cannot orphan persistent server state.
- Disconnect clears transport and discovery, coordinate and subscription state, pacing and correction state, reconciliation, and desync recovery. Engine-side fixture teardown follows the [Engine Agent lifecycle contract](../../../../../Engine/Source/Agent/AGENTS.md#architecture); the game callback then resets packet and full-state fixture state. `ClientSession` destruction detaches every game Agent fixture binding before destroying its runtime.
- Preserve subscription orchestration order: remove stale coordinates, recover timed-out transitional slots, rebuild the desired queue, then fill available slots.
- Ticks are acknowledged before adoption and the server resends only unacknowledged gaps, so an update discarded during adoption is never resent and strands reconciliation at that tick forever. When a coordinate's buffered server updates reach their ceiling, the runtime requests resync, synchronously resets game coordinate, reconciler, and unwanted-timestamp state through a narrow game hook, clears all received update vectors, and returns false.
- LAN discovery stops after recording a found address. A scan timeout records the timeout, replaces the scanner, and immediately starts a fresh scan.

## Reconciliation Invariants

- A server-validated tick is frozen and never simulated again; preserve render-behind history when advancing confirmed state. Replay and catch-up stay within the coordinate ring budget: the retained authoritative base holds one ring slot, so combined replay and catch-up writes are one less than the ring size and never wrap onto the base.
- Server-load notification clears coord, clock, identity, fleet, subscription, and reconciler state before the client accepts post-load data.
- Player-event and fleet-sync handlers have independent exception boundaries, each splitting corrupt data from an ordinary local failure under the hub's corrupt-input policy (`../../../../../Engine/Source/Network/AGENTS.md`). Static-data application and per-frame gameplay hydration remain outside those catches.
- Desired subscriptions stay sticky to reduce visible churn. During a real debug-frame wait or the synthetic full-state fixture stall, transport polling and receive-buffer drains continue while subscription updates, simulation, and reconciliation remain stalled.

Speculative and provisional CRC mismatches log at `kDebug`. Only a mismatch that survives full rollback/replay is a confirmed desync: it logs at `kError` with the differing CRCs and triggers recovery policy. With `kbDesyncDebugFrames` enabled, the client requests the server snapshot, stalls until a response matching the requested tick and coordinate or `kDesyncDebugTimeout`, compares it, then recovers or disconnects. Disabled, it never requests or stalls for a real desync and immediately follows `kbDesyncRecovery`; repeated recoveries within the configured window escalate to disconnect.
