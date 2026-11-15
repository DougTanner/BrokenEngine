<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:43:44.898Z","dependsOn":[]} -->
# Clean up server fleet serialization and redundant fleet Network code

## Context

Found by a deep analysis of `Projects/BrokenEngineSandbox/Source/Network`
(in-function and architecture phases), verified by code read against commit
`4cf9d607`; nothing was compiled. Paths are under
`Projects/BrokenEngineSandbox/Source/Network/Server/` unless they start at
`Engine/` or `Projects/`.

### Redundant writers of the fleet owner client-id map

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

### Redundant code in the server game Network files

Three small pieces of redundant code, none a defect:

- `ServerSession::WriteFleetData` (`ServerSession.cpp:224-227`, declared at
  `ServerSession.h:20`) is one pass-through call to the free function
  `WriteFleetData`. Its only caller is `WriteSaveState`
  (`ServerFleetSerialization.cpp:378`); a repository-wide search finds no
  other. The sibling entry points in the same file reach the fleet manager
  directly (`ReplayFleetRecordBytes`, `AdoptSaveState`, `ResetSaveState`).
  `Documents/C++StyleGuide.txt` rule 49 forbids a function that is only one
  pass-through call.
- `#include "Frame/Collections/Ships/ShipBehaviours.h"` at `ServerSession.cpp:8`
  and `ServerFleetSerialization.cpp:7` is unused: every function, constant,
  and type that header declares was matched by whole word against both files
  with no hit. `IsNavigationDelayInRange`, which they once took from it, is
  now in `Ships/ShipTypes.h`, reached through `Fleet.h`; commit `b9beb87b`
  moved it and left these two includes behind.
- `ServerFleetManager::ResetState` (`ServerFleetManager.cpp:507`) repeats the
  four queue clears of `ServerFleetManager::ClearPendingRequests` (`:311`),
  same queues, same order, before clearing `mFleets` and `mGuidToClientId`.

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
5. `WriteSaveState` (`ServerFleetSerialization.cpp`): call the free function
   `WriteFleetData` directly with the fleet manager's `mFleets` and
   `mRandomEngine`, as `ReplayFleetRecordBytes` does. `std::fstream&` binds
   to its `std::ostream&` parameter, and the arguments are the ones the
   method passes, so the save bytes are unchanged.
6. Delete `ServerSession::WriteFleetData`: the definition in
   `ServerSession.cpp` and the declaration in `ServerSession.h`.
7. Delete the `ShipBehaviours.h` include line from `ServerSession.cpp` and
   `ServerFleetSerialization.cpp`.
8. `ServerFleetManager::ProcessCreateFleetRequests`: delete the
   `insert_or_assign` line. Its `// Heap:` comment names the `mFleets` growth
   and stays true.
9. `ServerFleetManager::ResetState`: replace the four queue `clear()` lines
   with one `ClearPendingRequests();` call. Its comment and the `mFleets` and
   `mGuidToClientId` clears stay.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.h`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.h`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp`
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveState.h`

## In scope

- `ServerFleetSerialization.h` and `.cpp`:
  - `ReadFleetData`: the `rGuidToClientId` parameter and its two uses.
  - `ReadReplayFleetRecord`: the dropped local and its comment.
  - `ReadSaveState` and `AdoptSaveState`: the argument and the move line.
  - `WriteSaveState`: its one statement.
  - The `ShipBehaviours.h` include line.
- `GameSaveState.h`: `SaveStagedState::guidToClientId`.
- `ServerSession.h` and `.cpp`: `ServerSession::WriteFleetData`, declaration
  and definition, deleted; the `ShipBehaviours.h` include line in
  `ServerSession.cpp`.
- `ServerFleetManager.cpp`: the `insert_or_assign` line in
  `ProcessCreateFleetRequests`; the four queue clears in `ResetState`.

## Out of scope

- `mGuidToClientId` itself and its remaining writers (`OnClientConnected`,
  `OnResetForLoad`, `OnClientDisconnected`, and the `mGuidToClientId` clear
  in `ResetState`).
- The free function `WriteFleetData`, every byte it writes, and every byte
  `ReadFleetData` reads; no version changes.
- The engine grid-save code, which carries `SaveStagedState` by value without
  inspecting it.
- The comment in `ServerSession::OnResetForLoad` and the one in
  `PrepareReplayPlaybackTick`, which stay true.
- The per-client purge in `ServerFleetManager::OnClientDisconnected`, which
  erases by client and cannot use `ClearPendingRequests`.
- Every other include in these files, and the whole-file guard, which
  already sits directly after the first include.
- Turning `FleetNavigationController` into a free function, and moving
  `SortedGuidKeys` (both judged cosmetic).
- The work of `Documents/Plans/Game/FleetOwnedSquadronState.md`.

## Risk tier

Tier 2 — scoped behavior. Triggers: the signature of `ReadFleetData` and the
members of `SaveStagedState`, the game half of the grid-save read, change,
and a public method of `ServerSession` on the grid-save write path is
removed; Tier 1's "no public signature exposure" excludes both. It stays
Tier 2 because no save, replay, checksum, CRC, or wire byte changes, no
reader can observe the removed entries, and the request queue behavior and
every other outcome are unchanged.

## Acceptance criteria

- Code read: `ReadFleetData` has no owner client-id parameter,
  `SaveStagedState` has no `guidToClientId`, and
  `ProcessCreateFleetRequests` does not write `mGuidToClientId`.
- Code read: no `ServerSession::WriteFleetData` remains; `WriteSaveState`
  passes the same two arguments the method passed; neither
  `ServerSession.cpp` nor `ServerFleetSerialization.cpp` includes
  `ShipBehaviours.h`; `ResetState` clears the request queues only through
  `ClearPendingRequests`.
- `/compile` builds the BrokenEngineSandbox server, which is also the proof
  the `ShipBehaviours.h` include is unused.
- The repository static checks pass.
- Harness (`/agent-harness`): with one client connected and owning a fleet,
  `save` then `load`; the client's fleet panel shows its fleets after the
  load (the post-load fleet sync arrived). Then a create-fleet request from
  that client adds a fleet to the panel. The supplied evidence records no
  harness setup beyond these steps.

## Coordination

`Documents/Plans/Game/FleetOwnedSquadronState.md` lands after this Plan and
depends on it:

- That Plan rewrites `ReadReplayFleetRecord` and describes it as
  "`ReadFleetData` alone (still dropping the all-disconnected owner map)".
  After this Plan there is no owner map to drop, so that Plan writes
  `ReadReplayFleetRecord` as the `ReadFleetData` call with this Plan's
  parameter list. That Plan's `MarkAllFleetsChanged` marks every
  `mGuidToClientId` key and its flush sends only to connected owners, so it
  stays correct when the map no longer holds zeroed entries for loaded
  offline owners.
- That Plan also edits `ServerFleetManager::ResetState` (its slice 2 adds a
  clear of `mChangedFleetOwners` there). It keeps this Plan's edit: the
  function ends up with the `ClearPendingRequests()` call and that Plan's
  added clear.
