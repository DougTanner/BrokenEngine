<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-09T12:58:49.689Z","dependsOn":[]} -->
# Keep a pending disconnect until the game layer consumes it

## Context

`engine::Server::mPendingDisconnects` (`Engine/Source/Network/Server/Server.h:145`) is how the game layer learns that a client record was removed. Its only consumer is `ServerClientManager::Disconnects` (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp:149-172`), which `ServerSession::AfterNetworkPoll` (`Network/Server/ServerSession.cpp:260-263`) runs after each of the two `Server::Poll` calls of an update (`Engine/Source/Network/Server/ServerSessionRuntime.cpp:66-69`, `:81-82`). `Disconnects` clears the client's id from `ServerFleetManager::mGuidToClientId` (`OnClientDisconnected`, `ServerFleetManager.cpp:381-399`), purges its queued fleet requests, and removes it from the spawn queue `mClientsWaitingForSpawn`.

`Server::Poll` clears the list unconditionally at its start (`Engine/Source/Network/Server/Server.cpp:91`). A removal that happens inside a poll is consumed by that poll's `AfterNetworkPoll`. A removal that happens between polls is lost: the server agent drain runs between the update-start poll and the tick-boundary poll (`Engine/Source/GameBase.cpp:314-320`, then `:330`). `engine_packet_fault_harness_rig` (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFaultHarnessRigs.cpp:120-180`) calls `Server::Receive` from that drain (`:174`). Its fourth corrupt packet reaches `kiCorruptViolationDisconnectCount` (`Engine/Source/Network/NetworkProtocol.h:149`), so `RecordContractViolation` pushes the pending disconnect and removes the record (`Server.cpp:443-455`). The tick-boundary `Server::Poll` then clears the entry before `Disconnects` sees it. The later ENet DISCONNECT event finds no record and publishes nothing (`Server.cpp:192-201`). The game layer therefore never runs `Disconnects` for that client: its stale id stays in `mGuidToClientId` and in the spawn queue. `server_pre_handshake_ack_harness_rig` also calls `Server::Receive` from the drain (`ServerFaultHarnessRigs.cpp:259`).

That stale id is the only reason three send functions still null-check `FindClient`. `SendFleetSync` (`Network/Server/ServerFleetSerialization.cpp:15-21`) is reached through `SendFleetSyncToClient` with ids from `mGuidToClientId` and from the spawn queue. `ServerSession::SendAssignPlayer` (`ServerSession.cpp:298-304`) and `ServerSession::SendPlayerState` (`:315-321`) are reached from `ServerClientManager::SpawnWaitingClients` (`ServerClientManager.cpp:139-140`), which reads the spawn queue that `ServerFleetManager::TickRespawnTimers` fills from `mGuidToClientId`. Each function dereferences `pClient->pPeer` at its end.

`game_packet_fault_harness_rig` already works around the lost entry. It clears the list before parsing (`ServerFaultHarnessRigs.cpp:76-78`) and calls `Disconnects` itself after parsing (`:108-109`), and `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:8` documents both steps. `engine_packet_fault_harness_rig` has no such workaround. `Engine/Source/Network/Server/AGENTS.md:16` states that `RecordContractViolation` pushes the pending disconnect "so the game layer still gets the notification", which is false for a removal during the drain.

This was found while removing the fleet-request null checks in the game server session: those checks could be removed, but these three could not, because their ids do not come from the current poll's admitted packets.

## Design

Recommended: the consumer clears the list after it handles it. Then every pushed entry reaches `Disconnects` exactly once, whenever the push happened. This matches how `Server::Poll` already treats `mPendingNewSubscriptions` and `mPendingResynchronizationClientIds`, whose consumers service and clear them (`Server.cpp:92-96`).

1. `Server::Poll`: delete `mPendingDisconnects.clear();` (`Server.cpp:91`). Extend the comment that follows so it also names `mPendingDisconnects`, consumed and cleared by `ServerClientManager::Disconnects`.
2. `ServerClientManager::Disconnects`: after the loop, clear `gpServerSession->mpRuntime->mpServer->mPendingDisconnects`. A removal during the agent drain is then handled by the tick-boundary poll's `AfterNetworkPoll` in the same update. A removal inside a poll is handled exactly as today.
3. `game_packet_fault_harness_rig`: delete the `mPendingDisconnects.clear()` and its two-line comment (`ServerFaultHarnessRigs.cpp:76-78`), because the list is empty when the drain runs. Keep the explicit `Disconnects()` call (`:109`): it now consumes and clears the entry, so nothing is processed twice. Reword its comment (`:108`) so it no longer says the next poll clears the list.
4. Delete the `if (pClient == nullptr) { return; }` branch in `SendFleetSync`, `ServerSession::SendAssignPlayer`, and `ServerSession::SendPlayerState`. Keep the `FindClient` call, whose result each function dereferences. Do not add an `ASSERT`: the `pClient->pPeer` dereference at the end of each function is the crash point, and `.agents/references/cpp-conventions.md` (No useless ASSERTs) rules out an `ASSERT` there.
5. Update `commands-server.md:8` to match step 3. Remove the sentences about clearing the already-consumed pending disconnects and about the "next poll's normal clear", and keep the rest of the line.

Before deleting the three branches, re-prove step 4 against current code: list every caller of the three functions and every source of the client ids they receive (`mGuidToClientId` writers, including `OnResetForLoad` and `NewClients`; spawn-queue writers; and any direct `SendFleetSyncToClient` call). Show that each id belongs to a live client, because every removal path (`Server::Disconnect`, `RecordContractViolation`, `RejectHello`) pushes a pending disconnect for a handshaken client and `Disconnects` now always consumes it before the next tick's sends. If a source is found that this does not cover, keep that function's null branch and report the `path:line`.

An alternative is to clear in the engine instead, in `ServerSessionRuntime::Poll` and `PollTickBoundary` right after `mrSession.AfterNetworkPoll()`. It needs two clears instead of one, and `game_packet_fault_harness_rig`'s explicit `Disconnects()` call would then process the entry a second time unless it is also deleted. The author therefore recommends the consumer clear above.

Risk tier: Tier 3 (`.agents/references/risk-tiers.md`). Trigger: the change spans independently owned subsystems. It changes the lifetime of an engine-owned queue (`Engine/Source/Network/Server`) that the game server session (`Projects/BrokenEngineSandbox/Source/Network/Server`) consumes and now clears. Exposure: no wire, serialization, `kiVersion`, threading, or trust-boundary change. Disconnect handling for a real ENet disconnect, a contract violation inside a poll, and `RejectHello` keeps its current timing, so the per-tick CRC and replays of ordinary sessions are unchanged. Only a removal during the agent drain changes: its spawn-queue entry is now purged, so spawn status changes no longer enter the frame input for a removed client on that developer-tool path.

## Critical files

- `Engine/Source/Network/Server/Server.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFaultHarnessRigs.cpp`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`

## In scope

- `engine::Server::Poll`: the `mPendingDisconnects.clear()` statement and the comment block after it that lists the persisting pending lists.
- `game::ServerClientManager::Disconnects`: the clear after its loop.
- `game::SendFleetSync`, `game::ServerSession::SendAssignPlayer`, `game::ServerSession::SendPlayerState`: the `FindClient` null branch only.
- `CommandGamePacketFaultHarnessRig` in `ServerFaultHarnessRigs.cpp`: the `mPendingDisconnects.clear()` with its comment, and the comment on the `Disconnects()` call.
- `commands-server.md`: the `game_packet_fault_harness_rig` line's pending-disconnect sentences.

## Out of scope

- Any other `FindClient` null check, including the engine-side ones `Documents/Plans/Engine/NetworkServerOverEngineeringCleanup.md` covers.
- `RecordContractViolation`, `RemoveClient`, `RejectHello`, and `Server::Disconnect`, apart from what step 1 removes from `Poll`.
- The order of `AfterNetworkPoll`'s steps, and the clearing of `mReceivedGamePackets`.
- `engine_packet_fault_harness_rig` and `server_pre_handshake_ack_harness_rig` behavior beyond what the fix gives them.
- `Engine/Source/Network/Server/AGENTS.md:16`: its claim becomes true with the fix and needs no edit.

## Acceptance criteria

- `Server::Poll` no longer clears `mPendingDisconnects`; `ServerClientManager::Disconnects` clears it after its loop; nothing else clears it.
- The three send functions have no `pClient == nullptr` branch, or a kept branch is reported with the uncovered id source that keeps it.
- Server Debug and Release builds succeed through `/compile`.
- Live, through `/agent-harness`, with one connected client: four `engine_packet_fault_harness_rig` `{"case":"truncated"}` calls disconnect the client. With the `kNetwork` runtime log level lowered to `kVerbose`, the server log shows `ServerClientManager::Disconnects Client: <that clientId>` in that update. If `kVerbose` is compiled out for `kNetwork` in the build used, report that instead of adding a log line. The server keeps running, and each `game_packet_fault_harness_rig` case returns its documented response for a newly connected client.

## Notes

- Line numbers are as of the change that removed the game server session's fleet-request null checks; locate each site by symbol.
- `Documents/Plans/Game/ShipTypeSplit.md` renames `SendAssignPlayer` and `SendPlayerState` to `SendAssignShip` and `SendShipState`. If it lands first, apply step 4 to the renamed functions.
