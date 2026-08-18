# `/Engine/Source/Network/Client/` - Client Networking

## Overview

Client-side ENet peer (`Client`, `gpClient`) plus composed `ClientSessionRuntime`. The peer owns wire dispatch, slot state, ACK/RTT/jitter/bandwidth tracking, and receive buffers. The runtime owns GUID persistence, discovery, subscription orchestration, clock correction, generic received full-state/delta adoption, received-update ring/buffer/confirmation mechanics, static→full-state→delta drain order, ACK/flush, and generic reset state. The concrete game client session (`../../../../Projects/BrokenEngineSandbox/Source/Network/Client/AGENTS.md`) owns static-data application, per-frame gameplay hydration and smoke continuity, reconciliation, desync policy, UI, and gameplay policy. Collection-slot hydration rules are in `../../../../Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md`. Client-only (`BT_CLIENT`).

## Receive Paths

Server-update kinds land in separate buffers: per-tick coord delta updates (one buffer per slot), full state (placeholder-clear + slot activate), static data (sent once per subscription; carries NavData), debug frames (one-shot, captured for desync inspection), and a one-shot load-notification flag. `ClientSessionRuntime` drains static data, full states, and deltas in that order, adopting full-state/delta data generically while the game session applies static data and hydrates gameplay state. Channel/ACK/drain-per-poll conventions are the hub's — see parent.

## Subscription Receive Invariants

Most receive handlers (full-state, coord-update, subscribe-accept) classify into a flag set (commit / clear-placeholder / epoch heal / reject-as-ghost) before mutating slot state, the choke point for the epoch (the reuse counter) and ghost logic below; static data applies the same logic inline.

- Epoch check (drop rationale: hub's slot ACK model) applies only where the slot has a server-assigned epoch — a `kSubscribing` placeholder has none yet.
- Retained epoch: clearing a client slot (load reset, unsubscribe ack, cancel, timeout, placeholder clear) keeps the slot's most recent server-assigned epoch, retained for the connection lifetime, so slots without a server-assigned epoch (`kSubscribing`, `kUnsubscribed`) still admit full state and static data only when the packet epoch is strictly newer than the retained one (wrap-aware). The server only increments the epoch per allocation, so genuine post-reset traffic always passes and a packet from a retired subscription is dropped instead of reactivating the slot.
- Out-of-order full state (before subscribe-accept) claims the coord for the slot only when a matching `kSubscribing` placeholder still exists, clearing that placeholder at whichever slot holds it. Without that placeholder, the full state is a ghost and triggers an epoch-qualified unsubscribe.
- Static data applies the same coord-identity check as full state on `kWaitingFullState`/`kSubscribing` slots (silent drop on coord mismatch — the full-state path owns the ghost unsubscribe), with the exact-match guard against the server-assigned epoch only on `kWaitingFullState`; `kSubscribing` and `kUnsubscribed` admit out-of-order static data (buffers only, no slot mutation) only past the strictly-newer retained-epoch guard above.
- Pre-full-state buffering: a `kWaitingFullState` slot accepts delta updates but does not advance its ACK tick floor (only `kActive` slots track received ticks).
- Cancelled-subscription ghosts: locally-dropped `kSubscribing` slots record the coord; a late accept/full-state triggers an unsubscribe carrying the epoch from that packet. One epoch heal case covers legitimate re-subscribe to an already-active slot.
- Resync re-commit: a full state on a still-`kActive` slot commits only when its coord and epoch both match the slot — the server resends full state on live slots to re-sync a desynced client, re-baselining the slot's ACK floor and buffering the fresh frame for runtime adoption and game hydration; any other `kActive` full state (coord/epoch mismatch) or an `kUnsubscribing` slot is dropped. Resync and reconciliation are different mechanisms: resync is the server re-sending full state after a CRC mismatch, while reconciliation is the client's own rollback and replay of local ticks (`../../../../Documents/Architecture/GameReconciliation.md`).
- Out-of-range accept guard: a subscribe-accept with a non-`kuiSubscribeRejectSlot` index beyond the client slot pool is a trust-boundary violation — the client immediately unsubscribes to prevent a server-side slot from leaking.
- Gap beyond `kiNetworkBufferSize` on a single slot forces disconnect.
- Activation and hydration are same-frame: a full state mutates slot ACK/epoch/state immediately at receive time (`ClientReceive.cpp` `ServerCoordFullState`), while the runtime adopts its generic state and invokes game hydration in the ordered drain. Because `Poll()` clears all receive buffers at entry (drain-per-poll), skipping that drain loses the frame yet keeps the activated slot — so activation and hydration must both happen in the same frame.

## Transport Timing Inputs

The transport seeds pipeline RTT from the handshake wall-clock delta, then refines it from a client timestamp echoed in each coord update; a monotonic guard prevents duplicate processing during multi-frame ticks. Resends skip RTT/jitter processing because off-cadence arrivals would corrupt the interarrival jitter estimate. The game session consumes these measurements for clock correction; formulas live in `../../../../Documents/Architecture/Network.md`.

Server-load reset starts a new timing-measurement epoch: clear the jitter history and arrival baseline, then discard the first interval after that baseline is re-established so pre-load wall time cannot affect post-load clock correction. Keep this reset at the load boundary; otherwise the new full-state tick seed can be paired with stale pacing data.

When the clock check finds no `kActive` slot at all, it resets the latest-server-tick baseline to "no clock yet" and returns no correction. Keep that reset with the active-slot check: the baseline feeds the ceiling on how far the client may simulate, so a stale value left behind after the last subscription drops would clamp the simulation and freeze it until a new subscription catches up.

## Other

- Desync debug mode: real desync handling enters this mode only in game builds with `kbDesyncDebugFrames` enabled; it freezes the ACK floor (received-tick tracking becomes a no-op) so the server keeps resending while the captured debug frame is inspected. Disabled builds never request a debug frame or freeze ACK tracking for a real desync. The synthetic agent full-state fixture explicitly enters the same engine mode regardless of that compile flag and freezes ACK tracking until the fixture is cleared or reset.
- Network simulation: fast-forward (time multiply > 1) bypasses the delay queue and flushes pending; slot reuse, unsubscribe-ack, and load-reset paths purge delayed packets on the affected slot's coord channels, and load reset leaves the control channel queued because that channel carries the load notification and handshake traffic. If ENet reports disconnect before a delayed reliable rejection is released, the client delivers that queued rejection first so teardown cannot suppress its reason.

## See Also

- Game session + reconciliation: `../../../../Projects/BrokenEngineSandbox/Source/Network/Client/AGENTS.md`
- `../../../../Documents/Architecture/Network.md`
