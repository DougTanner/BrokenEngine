# Agent Client Coordinate Subscription

## Context

The `ReceivedUpdateAdoptionToEngine` acceptance requires subscribing to a neighbouring cell during a live client session and proving that the cell's full state is adopted without resetting the existing simulation tick. The closest documented harness recipe could not exercise the client side of that transition.

The live attempt on 2026-08-17 (`Temp/runtime-acceptance-neighbor.jsonl`) reset the server, injected a flagship at `[0,0]` with `fleetWantedCoord: [1,0]`, and eventually observed server `activeCoords` as `[[0,0],[1,0]]`. The client remained at `clientGridCoord: [0,0]` with `subscribedCoords: [[0,0]]`; `neighborObserved` stayed `false` through the run. The server-side fleet target made the neighbour active, but it did not change the client's desired-coordinate list.

The current client command surface has no explicit desired-coordinate or client movement command. `Game::SetClientGridCoord()` updates the client cell and invalidates its visible-neighbour cache. `ClientSession::UpdateDesiredCoords()` then derives desired subscriptions from that cell and its cached visible neighbours, and the existing client runtime performs the subscription handshake and full-state adoption. A harness-only client input is needed to reach this path deterministically.

This is a new agent-operation capability, so it belongs in `Documents/Features` rather than the executable scheduler. The current refactor's acceptance remains separate; this document records the follow-up capability needed by a later live cross-cell acceptance.

## Design

Add one client-only game command, `set_client_grid_coord`, to the project command surface. It is an agent input, not a new gameplay or network protocol operation.

### Command schema

Request:

```json
{"cmd":"set_client_grid_coord","params":{"coord":[x,y]}}
```

`params` must be an object containing only `coord`. `coord` must be an array of exactly two JSON integers representable as signed 32-bit `GridCoord` components. Missing, extra, non-integer, wrong-length, and out-of-range input fails before the client cell is changed. The command requires a connected client with a valid assigned player; before assignment, the existing subscription policy deliberately falls back to the origin, so accepting a request in that state would silently fail to request the chosen cell.

Successful result:

```json
{"clientGridCoord":[x,y]}
```

The result confirms only the local coordinate change. The command must not claim that the network subscription or full state is complete; the recipe below polls the existing scene, state, and log surfaces for that.

### Dispatch and subscription path

Dispatch the command through `ExecuteAgentCommandClient` on the client build. `AgentCommandServer::Drain()` already transfers command work from the socket thread to the executable's main thread; no game or network state may be changed on the socket thread.

After validation, perform the operations in this order:

1. Call `gpGame->SetClientGridCoord(coord)`. This setter must run first because it clears the visible-neighbour cache keyed by the old cell.
2. Call `gpClientSession->UpdateDesiredCoords(...)` immediately so the existing desired-coordinate path sees the new cell without waiting for the next poll. Use the existing `SubscriptionChangeReason::kPollTick` reason; no reason is exposed in the command schema.
3. Leave subscription ordering, sticky retention, slot allocation, server adjacency checks, static-data delivery, full-state adoption, and reconciliation to the existing paths. The ordinary frame loop will synchronize the updated desired list.

Do not call the fleet-selection persistence path for this transient agent input. In particular, this command must not turn a verification-only coordinate into a new persisted client preference. A valid coordinate that the server does not authorize remains an ordinary server subscription rejection; the command must not duplicate or bypass the server's adjacency/ownership policy.

### Live recipe

Use the launch, ownership, and release procedure in [BrokenEngineSandbox Agent Harness](../../../Projects/BrokenEngineSandbox/Documents/AgentHarness.md). Keep one connected server/client pair and use a fresh server reset.

1. Set the client Network log level to `Verbose`.
2. On the server, send:

   ```json
   {"cmd":"reset"}
   ```

   Then send:

   ```json
   {"cmd":"inject_status_changes","params":{"changes":[{"coord":[0,0],"type":"SpawnPlayer","isFlagship":true,"fleetWantedCoord":[1,0]}]}}
   ```

   Wait for the normal assigned-player/full-state state. Capture a client Network log baseline, a server `status`, and client `describe_scene {"includeUnits":false}` baseline. Record both ticks and require the client to begin at `clientGridCoord: [0,0]` with `[0,0]` in `subscribedCoords`. The server may make `[1,0]` active from the fleet target, but that is only the server-side fixture and does not satisfy the client criterion.
3. On the client, send:

   ```json
   {"cmd":"set_client_grid_coord","params":{"coord":[1,0]}}
   ```

   Require `ok: true` and result `clientGridCoord: [1,0]`. Poll `describe_scene {"includeUnits":false}` and server `status` until the client reports `clientGridCoord: [1,0]` and `subscribedCoords` contains `[1,0]`, while the server reports both `[0,0]` and `[1,0]` active. Require an appended client Network line of the existing `Client::ServerCoordFullState ... Coord: (1,0)` form.
4. Prove full-state/ring adoption with the existing `client_full_state_fixture`: send `{"cmd":"client_full_state_fixture","params":{"action":"arm_stall"}}` after the target cell is visible, and require its `coordState` for `[1,0]` to report `present: true`, `confirmedTick >= 0`, `lastFullStateTick >= 0`, `snapshotCount > 0`, and `ringValid: true`. Immediately send the fixture's `clear` action so the live session continues normally.
5. Require the post-command client tick to be greater than the pre-command baseline (or, at minimum, never lower than it after the normal tick has advanced); it must not return to zero or be reseeded by the neighbour's first full state. Compare only log lines appended after the baseline and require no new `LogDifferences CRC Client`, `CONFIRMED DESYNC`, checksum, or full-state read/decompression failure.
6. Send one malformed request, for example `{"cmd":"set_client_grid_coord","params":{"coord":[1]}}`. Require the normal agent error envelope (`ok: false` with a nonempty `error`), and verify that `clientGridCoord` and the running tick are unchanged by the rejected input. Sending this command to the server remains a normal client-only `unknown command` error.

The recipe proves the distinction that the failed 2026-08-17 attempt could not: server activation alone is insufficient; the client must explicitly move its desired cell, receive and adopt `[1,0]` full state, retain a valid ring, and continue the same session without a tick reset or desync.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp` — client-only command validation and `ExecuteAgentCommandClient` dispatch.
- `Projects/BrokenEngineSandbox/Source/Game.h` — existing `Game::SetClientGridCoord()` setter and visible-neighbour-cache invalidation.
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.h` and `ClientSessionSubscriptions.cpp` — existing `ClientSession::UpdateDesiredCoords()` policy and subscription-change reason.
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp` — existing desired-coordinate synchronization and received full-state adoption; reference behavior only unless implementation evidence requires a caller adjustment.
- `Engine/Source/Network/Client/ClientSend.cpp` and `Engine/Source/Network/Server/ServerReceive.cpp` — existing subscribe packet and server adjacency/authorization boundary; no protocol changes.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` — project command schema and live verification recipe.
- `Documents/Plans/Network/ReceivedUpdateAdoptionToEngine.md` — the acceptance that supplies the concrete cross-cell full-state trigger.

## In scope

- One validated client-only `set_client_grid_coord` harness command with the schema above.
- Main-thread routing through the existing game command dispatcher and the existing setter → desired-coordinate update path.
- Project harness documentation for the command, its error behavior, and the live `[0,0]` → `[1,0]` subscription/full-state recipe.
- Evidence that the existing client tick, ring, CRC, and reconciliation behavior remains intact while the new agent input is used.

## Out of scope

- A general gameplay movement, camera, fleet-selection, or teleport API.
- Any server-side counterpart, new wire message, protocol field, authorization rule, slot policy, sticky-subscription change, or subscription algorithm rewrite.
- Direct mutation of server simulation state, Frame collections, CRC inputs, replay/save state, or `kiVersion`.
- Persisting the agent-requested coordinate as client settings.
- Changes to `describe_scene` or a new generic client state-query surface; use the existing scene, log, and `client_full_state_fixture` evidence.
- Changes to the current `ReceivedUpdateAdoptionToEngine` implementation or its acceptance beyond recording this follow-up capability.
- Unit tests or a new harness transport.

## Risk tier and invariants

Future implementation is Tier 3. Although the input is client-only, it integrates Agent command dispatch with the game client's network-subscription policy and the received full-state/ring path.

- The command is available only on the client build and executes on the main thread after validation. Invalid input cannot partially change `mClientGridCoord`.
- `SetClientGridCoord` runs before `UpdateDesiredCoords`; otherwise the desired list can be built from the stale visible-neighbour cache.
- The normal `ClientSessionRuntime` ordering remains authoritative: stale coordinates, transitional slots, desired queue, and subscription sends keep their existing order.
- The server remains the authority for adjacency and ownership. The command does not make an unauthorized coordinate legal.
- A neighbour full state must seed that coordinate's ring without reseeding the already-running client tick. The ring must remain valid and CRC/reconciliation logs must remain clean.
- The command changes only existing client subscription intent and local render/simulation inputs. It adds no deterministic server state, wire shape, CRC field, save/replay format, or backward-compatibility path.
- The command must not persist the transient coordinate through fleet-selection/client-state save code.

## Acceptance criteria

- The documented command schema is implemented on the client only; valid `[x,y]` input returns the local coordinate, and malformed/out-of-range input uses the normal `ok:false`/`error` envelope without mutation.
- In a live server/client run, the command changes `[0,0]` to `[1,0]`, the server and client both expose the target as active/subscribed, and the client receives/adopts a `[1,0]` full state through the existing path.
- Existing `client_full_state_fixture` evidence for `[1,0]` reports a present confirmed state, a non-empty snapshot ring, and `ringValid:true`; clearing the fixture leaves the session usable.
- The client tick continues past the pre-command baseline without a reset or neighbour-induced reseed, and the bounded post-command logs contain no new CRC mismatch, confirmed desync, checksum, or full-state read failure.
- The server still rejects the client-only command as `unknown command`, and valid-but-unauthorized subscription requests remain governed by the existing server policy.
- The project Agent Harness document lists the schema, normal error behavior, and the live recipe with explicit server/client/tick/ring checks.

## Notes / Revisit When

- `describe_scene.subscribedCoords` reports local coordinate-frame entries. Treat it as the client visibility signal only alongside the existing full-state log and `client_full_state_fixture` ring evidence; server `activeCoords` alone does not prove client subscription.
- The concrete trigger is: **Revisit when a change requires live verification of client cross-cell subscription/movement (already observed in the `ReceivedUpdateAdoptionToEngine` acceptance).**
- This document is a manual Feature record under `Documents/Features`; it intentionally has no executable-plan metadata or scheduler claim.
