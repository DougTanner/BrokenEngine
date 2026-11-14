<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:43:44.898Z","dependsOn":[]} -->
# Remove the redundant writers of the fleet owner client-id map

## Context

Found by a deep analysis of `Projects/BrokenEngineSandbox/Source/Network`
(in-function phase), verified by code read against commit `4cf9d607`. Paths
are under `Projects/BrokenEngineSandbox/Source/Network/Server/` unless they
start at `Engine/` or `Projects/`.

`ServerFleetManager::mGuidToClientId` maps a fleet owner's GUID to its
connected client id; 0 means disconnected. Two of its writers carry no
information.

The load path. `ReadFleetData` (`ServerFleetSerialization.cpp:249`) takes an
out-parameter `rGuidToClientId`, clears it, and fills it only with 0, one
entry per loaded owner (`:303`). `ReadReplayFleetRecord` passes a local it
then drops (`:346`). `ReadSaveState` fills
`SaveStagedState::guidToClientId`
(`Projects/BrokenEngineSandbox/Source/Save/GameSaveState.h:14`), and
`AdoptSaveState` moves it into the live map (`:432`). Every reader treats an
entry holding 0 like an absent key:

- `ServerFleetManager::FindClientIdForGuid` returns 0 for an absent key; its
  only caller, `SendFleetSyncToClient`, returns on 0.
- `ServerFleetManager::OnClientDisconnected` sets a present entry to 0 and
  does nothing for an absent one.
- `PrepareReplayPlaybackTick`
  (`Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp`) iterates the
  keys only to call `SendFleetSyncToClient`.
- No `contains`, `size`, agent, harness, or map-marker reader exists.

After a load with clients connected, the real mapping comes from
`ServerSession::OnResetForLoad`, which calls
`ServerFleetManager::OnResetForLoad` for every connected client. Both
adoption paths run it synchronously after `AdoptSaveState`
(`Engine/Source/File/ServerSaveLoad.cpp`, `Engine/Source/File/Replay.cpp`),
and no `SendFleetSyncToClient` caller sits between. `Autoload` adopts without
that reset but runs in the `Game` constructor before any client exists.
`WriteFleetData` never writes the map.

The create path. `ServerFleetManager::ProcessCreateFleetRequests`
(`ServerFleetManager.cpp:65`) writes
`mGuidToClientId.insert_or_assign(guid, rRequest.iClientId)` for a GUID that
is always already mapped to that client:

- A queued create names a handshaken, still-present client
  (`OnClientDisconnected` purges its queued requests).
- Within a poll, `ServerSession::AfterNetworkPoll` runs
  `ServerClientManager::NewClients` before `ProcessCreateFleetRequests`, and
  `NewClients` maps the client through `OnClientConnected`, so a Hello and a
  create arriving in one poll are mapped first.
- A load reset remaps every connected client and then clears the pending
  queues; during playback, requests are refused before they queue.
- A reconnect with the same GUID is a new, unprocessed client record that
  `NewClients` maps; a duplicate-GUID Hello is rejected before the GUID is
  stored.
- The three sibling request handlers do not repeat the write.

`.agents/references/cpp-conventions.md` forbids a check for a state no caller
or writer can produce; this write is the same kind of re-assertion.

## Design

1. `ReadFleetData` (`ServerFleetSerialization.h` and `.cpp`): remove the
   `rGuidToClientId` parameter, its `clear()`, and the per-owner
   `insert_or_assign` with its "start as disconnected" comment. The bytes
   read are unchanged.
2. `ReadReplayFleetRecord`: delete the dropped local and its comment; pass
   the remaining arguments.
3. `ReadSaveState`: drop the argument. `GameSaveState.h`: delete
   `SaveStagedState::guidToClientId`.
4. `AdoptSaveState`: delete the line that moves the staged map into
   `mGuidToClientId`. Recommended over replacing it with a `clear()`: the
   live map then holds only entries for connected clients, which
   `OnResetForLoad` reassigns, and zeroed entries, which read like absent
   keys, so leaving it and clearing it are equivalent and leaving it is the
   smaller change.
5. `ServerFleetManager::ProcessCreateFleetRequests`: delete the
   `insert_or_assign` line. Its `// Heap:` comment names the `mFleets` growth
   and stays true.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.h`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp`
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveState.h`

## In scope

- `ReadFleetData`: the `rGuidToClientId` parameter and its two uses.
- `ReadReplayFleetRecord`: the dropped local and its comment.
- `ReadSaveState` and `AdoptSaveState`: the argument and the move line.
- `SaveStagedState::guidToClientId`.
- The `insert_or_assign` line in `ProcessCreateFleetRequests`.

## Out of scope

- `mGuidToClientId` itself and its remaining writers (`OnClientConnected`,
  `OnResetForLoad`, `OnClientDisconnected`, `ResetState`).
- Every byte `WriteFleetData` writes and `ReadFleetData` reads; no version
  changes.
- The engine grid-save code, which carries `SaveStagedState` by value without
  inspecting it.
- The comment in `ServerSession::OnResetForLoad` and the one in
  `PrepareReplayPlaybackTick`, which stay true.

## Risk tier

Tier 2 — scoped behavior. Trigger: the signature of `ReadFleetData` and the
members of `SaveStagedState`, the game half of the grid-save read, change,
which Tier 1's "no public signature exposure" excludes. It stays Tier 2
because no save, replay, checksum, CRC, or wire byte changes and no reader
can observe the removed entries.

## Acceptance criteria

- Code read: `ReadFleetData` has no owner client-id parameter,
  `SaveStagedState` has no `guidToClientId`, and
  `ProcessCreateFleetRequests` does not write `mGuidToClientId`.
- `/compile` builds the BrokenEngineSandbox server.
- The repository static checks pass.
- Harness (`/agent-harness`): with one client connected and owning a fleet,
  `save` then `load`; the client's fleet panel shows its fleets after the
  load (the post-load fleet sync arrived). Then a create-fleet request from
  that client adds a fleet to the panel. The supplied evidence records no
  harness setup beyond these steps.

## Coordination

No directional dependencies. Mandatory nondirectional coordination:

- `Documents/Plans/Game/FleetOwnedSquadronState.md` rewrites
  `ReadReplayFleetRecord` and describes it as "`ReadFleetData` alone (still
  dropping the all-disconnected owner map)". If this Plan lands first, there
  is no owner map to drop and that Plan's wording describes nothing; it then
  writes `ReadReplayFleetRecord` as the `ReadFleetData` call with this Plan's
  parameter list. If that Plan lands first, this Plan removes the dropped
  local from the function as that Plan left it. That Plan's
  `MarkAllFleetsChanged` marks every `mGuidToClientId` key and its flush
  sends only to connected owners, so it stays correct when the map no longer
  holds zeroed entries for loaded offline owners.
