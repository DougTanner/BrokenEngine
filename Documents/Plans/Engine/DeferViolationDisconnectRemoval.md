<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-17T21:27:53.639Z","dependsOn":[]} -->
# Defer contract-violation client removal until after the deferred client-packet drain

## Context

Origin: an `/over-engineering-review` design suggestion raised while the
game-agnostic session protocol moved into the engine; the user chose to file it
as a follow-up instead of changing that landing.

Verified current behavior:

- `Server::RecordContractViolation` (`Engine/Source/Network/Server/Server.cpp:411-457`)
  calls `enet_peer_disconnect` and then `RemoveClient` (`:454-455`) as soon as a
  kind's count reaches its limit. `RemoveClient` (`:370-385`) notifies the game
  client manager and swap-erases the `ClientConnection` from `mClients`, so any
  pointer or reference the caller still holds is dangling.
- That forces a "last use" rule on every caller: `Engine/Source/Network/Server/AGENTS.md`
  `## Session Invariants` (the `RecordContractViolation` bullet), the
  `Server.h:163-166` comment, and the in-code reminders at `Server.cpp:217-218`
  and `:472`. Callers holding a live `ClientConnection` are `Server::Receive`
  (`:228-279`, `:359`), `ClientAcknowledgementStream` (`:511`), and
  `ClientCrashReport` (`:832`).
- `ServerSessionBase::DispatchDeferredClientPackets`
  (`Engine/Source/Network/Server/ServerSessionBase.cpp:64-106`) can trigger the
  removal mid-drain through `Server::AdmitGamePacket` (`:74`) or the handler-throw
  catch (`:103`). Every later packet therefore looks its client up again: the
  engine-range `FindClient` skip (`:79-83`) and the null-client skip in
  `Server::AdmitGamePacket` (`Server.cpp:461-466`). The game handler at
  `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:118-119`
  dereferences `FindClient` unchecked and relies on that drain-time check.
- Violation removal is not the only mid-poll removal. In the same
  `enet_host_service` loop (`Server.cpp:110-126`) that defers packets
  (`DeferClientPacket`, `:367`), an ENet DISCONNECT event (`:117-118` ->
  `Server::Disconnect`, `:177-181`) and a rejected repeat Hello
  (`RejectHello`, `:698-704`) also call `RemoveClient`, leaving that client's
  already-deferred packets queued under a removed id. `Server::Disconnect`
  already purges the peer's delayed packets (`:183-189`), but nothing purges
  its deferred packets.
- Both polls of an update run the drain and then `RebuildOwnedEntities`
  (`Engine/Source/Network/Server/ServerSessionRuntime.cpp:65-68`, `:82-84`);
  `Server::Poll` clears `mDeferredClientPackets` at its start (`Server.cpp:82`).
- `ClientConnection::StateFlags` (`Engine/Source/Network/Server/Server.h:72-77`)
  already carries one per-connection flag (`kCrashReportReceived`).
- A send to a peer that `enet_peer_disconnect` moved out of the connected state
  fails in `enet_peer_send`, and `NetworkManager::SendPacket`
  (`Engine/Source/Network/NetworkManager.h:45-57`) frees the packet and returns
  silently.

Change Workflow tier: **Tier 3**. Trigger: client-to-server trust boundary — the
change alters when the violation policy disconnects a hostile client relative to
its queued packets (`Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy`;
`Documents/Architecture/Network.md` `## Client → Server Contract`).

## Design

The author recommends these choices; each is a recommendation with its
rationale, not a user decision.

1. Add `ClientConnection::StateFlags::kDisconnectPending`. At the limit,
   `RecordContractViolation` calls `enet_peer_disconnect`, logs the existing
   `Disconnecting` line, and sets the flag; it no longer calls `RemoveClient`.
   A call for a client already pending returns without counting. Rationale: the
   record stays valid until a single removal point, so no caller has a
   last-use constraint.
2. `Server::Receive` gate 1 (`Server.cpp:204-209`) drops a packet from a pending
   client silently, as it does for an unknown id. Rationale: preserves today's
   behavior that nothing from a client past its threshold is gated, counted, or
   deferred again.
3. `RemoveClient` also erases that client's entries from
   `mDeferredClientPackets`. After this change it runs only during `Server::Poll`
   (ENet DISCONNECT, rejected Hello) and in the post-drain removal below, never
   inside the drain loop, so the erase cannot invalidate the drain's iteration.
   Rationale: without it, the ENet DISCONNECT and rejected-Hello paths still
   leave queued packets for a removed id, and the drain lookups could not be
   made infallible.
4. **Remaining queued packets of a pending client: skipped.** The drain looks up
   each packet's client, `ASSERT`s the record exists, and skips the packet
   without admission, handling, or violation counting when the client is
   pending. `AdmitGamePacket` takes the `ClientConnection&` the drain found and
   drops its null-client branch. Rationale: today a removed client's later
   packets are skipped, so skipping keeps the observable trust-boundary outcome
   unchanged — no request from a client that crossed its disconnect threshold
   runs, including debug save/load or game requests. Handling them instead would
   remove the per-packet lookup entirely but would execute a hostile client's
   queued requests after the server decided to disconnect it. Under this
   recommendation the drain keeps one infallible lookup per packet; the gain is
   removing the use-after-free hazard class, not the lookup.
5. After the drain loop ends, a new public `Server` method removes every pending
   client through `RemoveClient` (which stays private), so the game client
   manager is notified and the records are gone before `RebuildOwnedEntities`
   and `AfterNetworkPoll` in the same poll. Removals in this sweep must tolerate
   `RemoveClient`'s swap-erase.

Between a Receive-path mark and the post-drain sweep a pending record stays in
`mClients`, so drain handlers that iterate clients (for example the load reset)
see it; their sends to its disconnecting peer are dropped by
`NetworkManager::SendPacket`.

Exposure: no determinism/CRC, wire, serialization, `.pack`/`kiVersion`, replay,
shader, or allocation-policy change. Main-thread only. Server build only; every
touched engine file stays whole-file `BT_SERVER` guarded.

## Critical files

- `Engine/Source/Network/Server/Server.h`
- `Engine/Source/Network/Server/Server.cpp`
- `Engine/Source/Network/Server/ServerSessionBase.cpp`
- `Engine/Source/Network/Server/AGENTS.md`
- `Documents/Architecture/Network.md`

## In scope

- `ClientConnection::StateFlags` in `Server.h`: add `kDisconnectPending`.
- `Server::RecordContractViolation`: mark instead of remove; return early for a
  pending client.
- `Server::Receive` gate 1: drop a pending client's packet silently.
- `Server::RemoveClient`: erase the client's `mDeferredClientPackets` entries.
- `Server::AdmitGamePacket`: take the drain's `ClientConnection&`; remove the
  null-client branch; update its declaration in `Server.h`.
- A new public `Server` method that removes every pending client through
  `RemoveClient`, declared in `Server.h`.
- `ServerSessionBase::DispatchDeferredClientPackets`: per-packet lookup with
  `ASSERT`, skip of pending clients, pass the record to `AdmitGamePacket`, and
  the post-loop call to the new removal method.
- Comments made false by the change: `Server.h:163-166`,
  `Server.cpp:204`, `:217-218`, `:464`, `:472`, `ServerSessionBase.cpp:81`.
- `Engine/Source/Network/Server/AGENTS.md` `## Session Invariants`: replace the
  `RecordContractViolation` last-use bullet with the deferred-removal rule.
- `Documents/Architecture/Network.md` violation-policy text that says the server
  calls `enet_peer_disconnect` + `RemoveClient` at the limit.

## Out of scope

- Changing `RecordContractViolation`'s signature or its own `FindClient`
  null-record return.
- `RejectHello`'s immediate removal and its caller's own last-use handling.
- Violation thresholds, decay, kinds, budgets, per-type caps, or log text other
  than what the deferral requires.
- The game handlers (`ServerSession.cpp` and fleet/client managers); the
  unchecked lookup at `ServerSession.cpp:118-119` stays valid because the drain
  only dispatches packets of live, non-pending clients.
- Any wire, protocol-version, or client-side change.

## Acceptance criteria

- No path removes a `ClientConnection` from inside the deferred-drain loop;
  `RecordContractViolation` never calls `RemoveClient`.
- Every packet the drain visits has a live client record (the `ASSERT` holds);
  a pending client's packets are neither admitted, handled, nor counted.
- `AdmitGamePacket` has no null-client branch, and the drain has no
  "client removed mid-drain" skip.
- Every pending client is removed through `RemoveClient`, with the game client
  manager notified, before `RebuildOwnedEntities` in the same poll.
- `Engine/Source/Network/Server/AGENTS.md` no longer states a last-use rule for
  `RecordContractViolation`, and `Documents/Architecture/Network.md` describes
  the deferred removal.
- Client and server build cleanly through `/compile`; an `/agent-harness`
  session connects a client, plays briefly, and disconnects it with the server
  log showing the normal connect and disconnect lines and no assert.

## Notes

- The ENet DISCONNECT event that follows a violation disconnect still reaches
  `Server::Disconnect`, which finds no record after the sweep, as today. If that
  event arrives within the same service loop while the record is still pending,
  `RemoveClient` removes it immediately and purges its deferred packets, which
  the recommended design already covers.
- `Documents/Plans/Engine/RemoveNetworkSessionContractStruct.md` also edits
  `Server.cpp` and the drain's `game::NetworkSessionContract::GamePacket` cast;
  the overlap is textual only.
