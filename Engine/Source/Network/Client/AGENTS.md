# `/Engine/Source/Network/Client/` - Client Networking

## Overview

Client-only (`BT_CLIENT`) ENet peer and composed `ClientSessionRuntime`. The peer owns wire dispatch, slots, ACK/timing/bandwidth state, and receive buffers. The runtime owns connection and GUID persistence, discovery, subscription and clock state, generic full-state and delta adoption, received-update buffering and confirmation, ordered receive draining, ACK/flush, and resets. This directory also owns replay and desync machinery. The [game client session](../../../../Projects/BrokenEngineSandbox/Source/Network/Client/AGENTS.md) owns static-data application, gameplay hydration and smoke continuity, reconciliation/desync policy, UI, and gameplay policy. Collection hydration rules are in the [game collection detail](../../../../Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md).

## Receive Paths

Server updates use separate delta, full-state, static-data, debug-frame, and load-notification buffers. `ClientSessionRuntime` drains static data, full states, then deltas, adopting generic state while the game session applies static data and hydrates gameplay.

## Subscription Receive Invariants

`ClientReceive.cpp` centralizes full-state, coord-update, and subscribe-accept decisions in `ClassifyFullState`, `ClassifyCoordUpdate`, and `ClassifySubscribeAccept`; static data applies the same checks inline.

- Exact epoch checks apply only after the server assigns an epoch. Slot clearing retains the last assigned epoch for the connection lifetime; `kSubscribing` and `kUnsubscribed`, which have no assigned epoch, admit out-of-order full/static state only when its epoch is wrap-aware newer than the retained one. This prevents retired traffic from reactivating a slot.
- Out-of-order full state may claim a coord only while its matching `kSubscribing` placeholder exists; otherwise it is a ghost and sends an epoch-qualified unsubscribe. Locally cancelled subscriptions likewise unsubscribe late accepts/full states, while a same-coord active slot may heal its epoch on a legitimate re-subscribe.
- Static data requires matching coord identity on `kWaitingFullState`/`kSubscribing`, exact epoch on `kWaitingFullState`, and a newer retained epoch on `kSubscribing`/`kUnsubscribed`. A coord mismatch drops silently because full state owns ghost unsubscribe. Static data buffers without slot mutation and rejects the whole payload if an island CRC lacks a loaded template. Its frame area is always recomputed from the payload coord ([Frame](../../Frame/AGENTS.md)).
- `kWaitingFullState` accepts deltas without advancing the ACK floor; only `kActive` tracks received ticks. A gap beyond `kiNetworkBufferSize` disconnects.
- Active-slot resync full state commits only on matching coord and epoch, resets the ACK floor, and buffers the new frame; mismatched active or `kUnsubscribing` state is dropped. Resync is server full-state replacement; reconciliation is client rollback/replay ([Game Reconciliation](../../../../Documents/Architecture/GameReconciliation.md)).
- A non-reject subscribe-accept slot outside the client pool immediately unsubscribes so the server slot cannot leak. Full-state activation in `Client::ServerCoordFullState` and runtime adoption/game hydration must complete in the same frame because `Client::Poll` clears receive buffers at its next entry; skipping adoption loses the frame after activating the slot.

## Transport Timing Inputs

`ClientReceive.cpp` seeds pipeline RTT from the handshake, refines it once per monotonic echoed client timestamp, and excludes resends because off-cadence arrivals would corrupt interarrival jitter. At load, `ClientSessionRuntime.cpp` resets timing history and skips the first restored interval so pre-load wall time cannot affect the new clock. When no active slot remains, it clears `miLatestServerTick` because that baseline sets the simulation ceiling. Signed clock correction floors the time-step remainder at zero so it cannot produce a negative target tick. Keep these behaviors at their existing boundaries; formulas and simulation-ceiling behavior are in [Network](../../../../Documents/Architecture/Network.md).

## Reconciliation Replay Chain

This directory owns CRC fast-path, rollback/ring orchestration, replay, per-coord parallel dispatch, profiling merge, and first-in-slot-order unresolved mismatch selection. Each worker writes only its coord. The game supplies payloads, transfer materialization, and reconciliation/desync policy; see [Game Reconciliation](../../../../Documents/Architecture/GameReconciliation.md).

- `ReconcileReplayTick.cpp` applies transfer `StatusChange`s after each replay tick in server Destroy/Spawn order and recomputes CRC after transfers; this ordering is required for the broadcast CRC ([server detail](../Server/AGENTS.md)).
- `ReconcileReplay.h` and `ClientDesyncCore.h` deliberately name game types because their payloads are game-defined while their machinery is engine-owned. They stay out of `Engine.h`, and consumers include them directly, matching the exception in the [server detail](../Server/AGENTS.md).

## Other

- `ClientDesyncCore` owns debug-frame correlation and repeated-desync escalation to disconnect. Real desync freezes ACK tracking and requests a debug frame only when `kbDesyncDebugFrames` is enabled; disabled builds do neither. The synthetic agent fixture enters the same engine mode regardless of the flag until cleared/reset. The game-side seam is documented in the game client-session leaf.
- Network fast-forward bypasses delayed delivery and flushes pending packets. Slot reuse, unsubscribe ACK, and load reset purge affected coord channels; load reset preserves the control channel ([`ClientSessionRuntime::ResetForServerLoad`](ClientSessionRuntime.cpp)). A delayed reliable rejection is delivered before ENet disconnect teardown ([`Client::Poll`](Client.cpp)).
