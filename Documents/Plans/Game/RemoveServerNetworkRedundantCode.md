<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:43:32.700Z","dependsOn":[]} -->
# Remove redundant code from the server game Network files

## Context

Found by a deep analysis of `Projects/BrokenEngineSandbox/Source/Network`
(architecture phase), verified by code read against commit `4cf9d607`; nothing
was compiled. Paths are under
`Projects/BrokenEngineSandbox/Source/Network/Server/` unless they start at the
repository root. Three small pieces of redundant code, none a defect:

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

1. `WriteSaveState` (`ServerFleetSerialization.cpp`): call the free function
   directly with the fleet manager's `mFleets` and `mRandomEngine`, as
   `ReplayFleetRecordBytes` does. `std::fstream&` binds to its
   `std::ostream&` parameter, and the arguments are the ones the method
   passes, so the save bytes are unchanged.
2. Delete `ServerSession::WriteFleetData`: the definition in
   `ServerSession.cpp` and the declaration in `ServerSession.h`.
3. Delete the `ShipBehaviours.h` include line from `ServerSession.cpp` and
   `ServerFleetSerialization.cpp`.
4. `ServerFleetManager::ResetState`: replace the four queue `clear()` lines
   with one `ClearPendingRequests();` call. Its comment and the `mFleets` and
   `mGuidToClientId` clears stay.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.h`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp`

## In scope

- `ServerSession::WriteFleetData`: declaration and definition, deleted.
- `WriteSaveState`: its one statement.
- The `ShipBehaviours.h` include line in `ServerSession.cpp` and
  `ServerFleetSerialization.cpp`.
- The four queue clears in `ServerFleetManager::ResetState`.

## Out of scope

- The free function `WriteFleetData` and every byte it writes.
- The per-client purge in `ServerFleetManager::OnClientDisconnected`, which
  erases by client and cannot use `ClearPendingRequests`.
- Every other include in these files, and the whole-file guard, which
  already sits directly after the first include.
- Turning `FleetNavigationController` into a free function, and moving
  `SortedGuidKeys` (both judged cosmetic).

## Risk tier

Tier 2 — scoped behavior. Trigger: a public method of `ServerSession` on the
grid-save write path is removed, which Tier 1's "no public signature
exposure" excludes. It stays Tier 2 because the save bytes, the request queue
behavior, and every other outcome are unchanged.

## Acceptance criteria

- Code read: no `ServerSession::WriteFleetData` remains; `WriteSaveState`
  passes the same two arguments the method passed; neither file includes
  `ShipBehaviours.h`; `ResetState` clears the request queues only through
  `ClearPendingRequests`.
- `/compile` builds the BrokenEngineSandbox server, which is the proof the
  include is unused.
- The repository static checks pass.

## Coordination

No directional dependencies. Mandatory nondirectional coordination:

- `Documents/Plans/Game/FleetOwnedSquadronState.md` also edits
  `ServerFleetManager::ResetState` (its slice 2 adds a clear of
  `mChangedFleetOwners` there). Whichever lands second keeps the other's
  edit: the function ends up with the `ClearPendingRequests()` call and that
  Plan's added clear.
