<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T18:27:23.891Z","dependsOn":[]} -->
# Fleet-Owned Squadron and Fleet State

## Context

Paths are under `Projects/BrokenEngineSandbox/Source/` unless they start at the
repository root; `## Critical files` gives every edited path from the
repository root. Sites are named by symbol; locate them by search. The baseline
this Plan was resolved against is commit `213d2577`.

The user asked for this follow-up in these words (the authoring agent's
record): "I think we should store fleet data with the fleet ... we may also
want to look at other 'fleet-level' settings and move them there? Should we
create a 'Fleet' collection that owns the 'Squadrons' and those own the ships?
(File as followup plan)".

### Problem

- A squadron is only the pair `(FleetMember::eType, FleetMember::uiSquadron)`
  (`Fleet.h`); no squadron record exists. The Fighter squadron settings (Patrol
  or Seek and Destroy, Same Target) and the Omni squadron settings (Escort or
  Patrol Cell, Patrol Together) are `FleetMemberFlags` bits repeated on every
  member of the squadron, and three separate pieces of code keep them equal:
  `ServerFleetManager::ProcessSquadronOrdersRequests` sets them on each member,
  `ProcessSpawnIntoFleetRequests` copies a squadronmate's onto a joining
  member, and `ReadFleet` (`Network/Server/ServerFleetSerialization.cpp`)
  rejects a squadron whose members disagree. The member flags use a different
  encoding from the row's `ShipOrders` bits (Same Target is the inverted
  `kSeparateTargets`), converted by `SquadronOrders` in `Fleet.h`.
- Code that copies fleet values onto ship rows is written by hand in three
  classes with different checks: the spawn lambda in
  `ServerClientManager::SpawnFleetMember`, the `kUpdateFleet` member loop in
  `FleetNavigationController::TickFleetTimers`, the `kUpdateSquadronOrders`
  loop in `ProcessSquadronOrdersRequests`, and the `kDestroyShip` row lookup in
  `ProcessDestroyFleetMemberRequests`.
- The Fighter launch cadence is fleet state kept outside `Fleet`, in the side
  map `ServerClientManager::mNextFighterLaunchTicks` keyed by fleet GUID. Only
  the replay fleet record carries it, as a GUID-keyed tail after the grid-save
  bytes, beside a second tail of member respawn countdowns.
- Every fleet change has to remember its own `SendFleetSyncToClient` call;
  there are nine hand-placed calls in the server fleet code and one loop in
  `Save/GameSaveLoad.cpp`.
- Commit `de0c3c54` changed the replay fleet record's member flags without
  bumping `ReplayMeta::kiVersion` (`Game.h`).

### Inventory (resolved)

The investigation this Plan first carried is done; its result, confirmed from
the baseline tree:

- The squadron settings on `FleetMember::flags` are not needed per member.
  They are in FleetSync, the grid save, replay fleet records, and the fleet
  checksum, never in the Frame CRC. Their row copies (the `ShipOrders` setting
  bits) must stay per row and are in the CRC.
- The row columns `pClientGuids`, `pFlagshipGlobalIds`,
  `pFleetWantedCoordinates`, and `puiPendingFleetWantedCoordinateTicks`, and
  the row bit `ShipOrders::kIsFlagship`, must stay per row: in-frame navigation,
  `FleetBoard`, and the owned-entity registry read them inside the cell-local
  tick. None causes a defect.
- `pNavigationDelays` is not a copy of `Fleet::navigationDelaySeconds` and has
  no simulation reader; only the harness rig produces `kUpdateShip`.
  `pCellChangeTimers` is a per-row navigation timer. Neither is fleet state.
- `mNextFighterLaunchTicks` is a server-only side map keyed by fleet; it
  belongs on `Fleet`.
- Fleet stance (`FleetBoard`, `FleetView`), the Same Target shared target, and
  the Patrol Together leader are derived per tick, never stored.
- Client `FleetSelection` holds only pending-UI state for the focused fleet,
  no fleet or squadron value.

### Decisions recorded for this Plan

The authoring agent's record of what the user chose in the resolving session:

- The authoritative fleet and squadron structure stays on the server, outside
  the Frame, because ships of one fleet can be in different cells; ship rows
  keep cell-local copies. No frame Collection is added.
- The scope is widened from the squadron settings to the whole fleet-to-row
  copy design in `## Design`, including moving the launch tick onto `Fleet` and
  automatic per-owner fleet syncs.
- No backward compatibility: the protocol, Frame, and replay metadata versions
  are bumped.
- Grid saves also store each member's respawn countdown and the fleet's next
  Fighter launch tick, so a replay fleet record is exactly the grid-save
  bytes and the replay-only tail is deleted. The save reader rejects a
  non-finite countdown.
- Both slices in `## Design` are one Plan, implemented in order in one session
  and landed together. This departs from the one-Plan-per-stage rule in
  `Documents/Plans/AGENTS.md` `## Plan files` on the user's direct instruction.
- The user directed that this Plan file be rewritten to hold the resolved
  design.

Resolved from evidence by the authoring agent, not asked of the user:

- `Fleet::uiPendingFleetWantedCoordinateTicks` is set at each pick and never
  counted down on the server, so a member that respawns long after a pick
  spawns with the full count. No code shows a defect from it, and changing it
  would change simulation behaviour, so it stays unchanged and out of scope.
- `mChangedFleetOwners` growth in `DetectFleetMemberDeaths` and
  `UpdateFleetNavigation` needs no allocation-tracking scope of its own (the
  reason is in `### Invariants`).

## Design

One design: the flat add-order member list stays, `Fleet` gains a per-squadron
settings table and the launch tick, a new server-only `FleetRows` module is the
only code that copies fleet values to ship rows, and fleet syncs are sent from
change marks.

### Final interface

`Fleet.h` (both builds):

```cpp
enum class FleetMemberFlags : uint8_t
{
	kIsDead = 1 << 0,
};

// The squadron setting bits a squadron of each type may hold, indexed by ShipType.
inline constexpr ShipOrders_t kpSquadronOrderBits[] {{ShipOrders::kSeekAndDestroy, ShipOrders::kSharedTarget}, {ShipOrders::kPatrolCell, ShipOrders::kPatrolTogether}, {}};
// The settings an empty squadron's first member starts with, indexed by ShipType.
inline constexpr ShipOrders_t kpDefaultSquadronOrders[] {{ShipOrders::kSharedTarget}, {}, {}};

struct Fleet
{
	// ... existing members unchanged ...
	// One squadron's settings, as the bits its members' rows hold, indexed by ShipType and squadron number.
	ShipOrders_t pSquadronOrders[std::to_underlying(ShipType::kCount)][kiMaximumFleetSquadronsPerType] {};
	// Server-only: the first spawn tick at which the Battleship may launch another Fighter batch.
	int64_t iNextFighterLaunchTick = 0;
};
```

`Network/Server/FleetRows.h` (new, whole-file `BT_SERVER`, namespace `game`),
implemented in the new `Network/Server/FleetRows.cpp`:

```cpp
// Completes spawnInfo with the member's fleet values and queues it for this tick's post-tick batch in coordinate.
template <typename SPAWN_INFO>
void QueueFleetSpawn(const engine::ClientGuid& rClientGuid, const Fleet& rFleet, const FleetMember& rMember, engine::GridCoord coordinate, SPAWN_INFO spawnInfo);
// Queues the fleet's wanted cell and pending ticks for every live member's row.
void QueueUpdateFleet(const Fleet& rFleet);
// Queues the squadron's settings for every live member row of that squadron.
void QueueUpdateSquadronOrders(const Fleet& rFleet, ShipType eType, uint8_t uiSquadron);
// Queues staged destruction of a live member's row.
void QueueDestroyShip(const FleetMember& rMember);
```

`Network/Server/ServerFleetManager.h`:

```cpp
struct PendingSquadronOrdersRequest
{
	int64_t iClientId = 0;
	engine::GlobalId globalId {};
	ShipOrders_t orders {};
};

struct FleetMemberLocation
{
	Fleet* pFleet = nullptr;
	FleetMember* pMember = nullptr;
};

// public
FleetMemberLocation FindFleetMember(const engine::ClientGuid& rClientGuid, engine::GlobalId globalId);
void MarkFleetChanged(const engine::ClientGuid& rClientGuid);
void MarkAllFleetsChanged();
void FlushFleetSyncs();
std::vector<engine::ClientGuid> mChangedFleetOwners;

// private
FleetMember& AddFleetMember(const engine::ClientGuid& rClientGuid, Fleet& rFleet, ShipType eType, uint8_t uiSquadron);
void SendFleetSyncToClient(const engine::ClientGuid& rClientGuid);
```

`MarkFleetChanged`, `MarkAllFleetsChanged`, `FlushFleetSyncs`, and
`mChangedFleetOwners` arrive with slice 2, which also makes
`SendFleetSyncToClient` private; slice 1 leaves it public. The union of every
squadron setting bit is the existing `kSquadronOrderBits`
(`Frame/Collections/Ships/ShipTypes.h`), unchanged.

Other changed signatures:

```cpp
// Network/Server/ServerClientManager.h — the spawn's cell, or nullopt when the member stays dead.
std::optional<engine::GridCoord> SpawnFleetMember(const engine::ClientGuid& rClientGuid, const Fleet& rFleet, const FleetMember& rMember, int64_t& riNextFighterLaunchBay, common::ScopedWorkbufferArena& rTakenSpawnPoints);
// Network/Client/ClientSession.h
void SendSquadronOrdersRequest(engine::GlobalId globalId, ShipOrders_t orders);
// Network/GameMessages.h — SquadronOrdersRequestMessage
engine::GlobalId globalId {};
uint16_t uiOrders = 0;
```

### Key call sites

The examples show the tree after slice 2. Every `MarkFleetChanged` line in them
is slice 2's: slice 1 writes `SendFleetSyncToClient` with the same argument at
that line.

Request handler (`ProcessSquadronOrdersRequests`, inside its existing
allocation scope and request loop):

```cpp
FleetMemberLocation location = FindFleetMember(pClient->clientGuid, rRequest.globalId);
if (location.pMember == nullptr)
{
	continue;
}
ShipType eType = location.pMember->eType;
uint8_t uiSquadron = location.pMember->uiSquadron;
ShipOrders_t& rSlot = location.pFleet->pSquadronOrders[std::to_underlying(eType)][uiSquadron];
// A setting of another type, or no change, is ignored.
if (rRequest.orders.Mask(kpSquadronOrderBits[std::to_underlying(eType)]) != std::to_underlying(rRequest.orders.meFlags) || rRequest.orders == rSlot)
{
	continue;
}
rSlot = rRequest.orders;
QueueUpdateSquadronOrders(*location.pFleet, eType, uiSquadron);
MarkFleetChanged(pClient->clientGuid);
```

Navigation pick (`FleetNavigationController::TickFleetTimers`, replacing its
member loop and its own `mbReplaying` test):

```cpp
rFleet.wantedCoordinate = destination;
rFleet.uiPendingFleetWantedCoordinateTicks = static_cast<uint8_t>(engine::kiTickRate);
rFleet.cellChangeTimerSeconds = rFleet.navigationDelaySeconds;
QueueUpdateFleet(rFleet);
```

Destroy (`ProcessDestroyFleetMemberRequests`):

```cpp
FleetMemberLocation location = FindFleetMember(pClient->clientGuid, rRequest.globalId);
if (location.pMember == nullptr || location.pMember->eType == ShipType::kBattleship)
{
	continue;
}
QueueDestroyShip(*location.pMember);
location.pFleet->members.erase(location.pFleet->members.begin() + (location.pMember - location.pFleet->members.data()));
MarkFleetChanged(pClient->clientGuid);
```

Spawn placement (`SpawnFleetMember`) and its caller (`TickRespawnTimers`):

```cpp
// SpawnFleetMember, Fighter bay found:
QueueFleetSpawn(rClientGuid, rFleet, rMember, flagshipCoordinate, spawnInfo);
return flagshipCoordinate;

// TickRespawnTimers:
std::optional<engine::GridCoord> optionalSpawnCoordinate = gpServerSession->mpClientManager->SpawnFleetMember(rGuid, rFleet, rMember, iNextFighterLaunchBay, takenSpawnPoints);
if (optionalSpawnCoordinate)
{
	rMember.flags.Set(FleetMemberFlags::kIsDead, false);
	rMember.coordinate = *optionalSpawnCoordinate;
	MarkFleetChanged(rGuid);
}
```

HUD (`HudScreen::RenderFocusedOwnedEntityPanel`):

```cpp
ShipOrders_t orders = pFleet->pSquadronOrders[std::to_underlying(pSquadronMember->eType)][pSquadronMember->uiSquadron];
bool bSameTarget = orders & ShipOrders::kSharedTarget;
// ... each control changes one bit of orders ...
gpClientSession->SendSquadronOrdersRequest(pSquadronMember->globalId, orders);
```

### Slice 1: fleet-owned state and FleetRows

Slice 1 keeps the hand-placed `SendFleetSyncToClient` calls and writes no
`MarkFleetChanged` line. After it the tree builds and criteria 1 to 4, 6 to 8,
10, 11, and the `FleetRows` half of 5 can pass.

1. Squadron slots. `Fleet::pSquadronOrders` holds each squadron's settings
   once, in the row's `ShipOrders` encoding, with `kpSquadronOrderBits` and
   `kpDefaultSquadronOrders` per type (Battleship: none). The array is
   zero-initialized. `FleetMemberFlags` keeps only `kIsDead`. Delete from
   `Fleet.h`: the four setting bits, `kFighterOrderFlags`, `kOmniOrderFlags`,
   `kSquadronOrderFlags`, `SquadronOrderFlags`, `SetSquadronOrderFlags`, and
   `SquadronOrders`. `Fleet::members`, `FleetMember::eType`, and
   `FleetMember::uiSquadron` are unchanged.
2. `ServerFleetManager::AddFleetMember` (private) mints the global ID, appends
   the member dead at the origin, and, when the squadron had no member, resets
   the slot to its type's `kpDefaultSquadronOrders`. That is the only place a
   default is set: a slot would otherwise keep an emptied squadron's settings,
   and a zeroed slot is not the Fighter default (Same Target on is a set bit).
   `CreateFleet` (for the Battleship) and `ProcessSpawnIntoFleetRequests` use
   it. The squadronmate copy in `ProcessSpawnIntoFleetRequests` is deleted.
3. `ServerFleetManager::FindFleetMember` returns the owner's fleet and member
   for a global ID, both null when absent. The destroy and squadron-orders
   handlers use it.
4. `FleetRows` (new files) is the only fleet-to-row code. `QueueFleetSpawn`
   fills the whole fleet block of the `SpawnInfo`: alignment,
   `ShipController::kFleet`, global ID, owner, flagship ID, wanted cell,
   pending ticks, and orders built from the squadron slot, `kSquadron1` for a
   Fighter or Omni in squadron 1, `kIsFlagship` for the member the fleet's
   flagship ID names (compared by ID, as today, not by ship type), and the
   type's default target flight plane. It then queues the spawn on the transfer
   manager at `gpGame->miTickCounter`. The template is defined in
   `FleetRows.cpp` with explicit instantiations for the Fighter, Omni, and
   Battleship `SpawnInfo` types. `QueueUpdateFleet`,
   `QueueUpdateSquadronOrders`, and `QueueDestroyShip` share one file-local
   live-member loop: skip a dead member, resolve the row with
   `ShipUuidByGlobalId` in the member's cell, one `ASSERT` that it exists, push
   into that cell's `FrameInput::statusChanges`. All four functions queue
   nothing while `gpGame->mbReplaying`. Like `FleetNavigationController.cpp`,
   `FleetRows.cpp` is a server-project-only file with a whole-file `BT_SERVER`
   guard; add both files to the server project through `/update-vcxproj`.
5. Callers move onto `FleetRows`. Delete the spawn lambda in
   `SpawnFleetMember` (with its `kIsFlagship` overwrite), the member loop in
   `FleetNavigationController::TickFleetTimers`, the fan-out in
   `ProcessSquadronOrdersRequests`, and the row lookup in
   `ProcessDestroyFleetMemberRequests`. `SpawnFleetMember` becomes placement
   only: it reads the fleet and member, never writes them, never calls the
   fleet manager, and returns the spawn's cell (the origin, or the flagship's
   cell for a Fighter) or `std::nullopt`. `TickRespawnTimers` writes the
   member's liveness and coordinate and sends the spawn's fleet sync, which is
   the `SendFleetSyncToClient` call moved out of `SpawnFleetMember`. The other
   hand-placed `SendFleetSyncToClient` calls stay where they are. The
   navigation pick log loses its `MembersUpdated` count, since
   `QueueUpdateFleet` returns nothing; nothing reads that field.
6. Launch tick. Delete `ServerClientManager::mNextFighterLaunchTicks`;
   `Fleet::iNextFighterLaunchTick` replaces it (a value at or below the
   current tick, zero included, means ready). `RebuildFleetMembersForLoad`
   resets it beside the respawn countdowns, which replaces the clear in
   `ServerClientManager::ResetState`. Delete
   `ReplayFleetRecord::nextFighterLaunchTicks` and its adoption line in
   `PrepareReplayPlaybackTick` (`Save/GameSaveLoad.cpp`).
7. Formats.
   - FleetSync (`FleetSyncMessage`, `Network/GameMessages.h`):
     `VisitFleetHeader` appends every slot as a 16-bit value;
     `kiFleetHeaderSize`, its `static_assert`, and the trailing-size argument
     of the member `BoundedCount` grow by the slot bytes. The member record
     keeps its flags
     byte. FleetSync carries neither the countdown nor the launch tick.
   - Grid save (`WriteFleet`/`ReadFleet`): the slots follow the hold
     coordinate and the fleet's launch tick follows the slots; each member
     record ends with its respawn countdown. `ReadFleet` rejects a slot holding
     a bit outside its type's `kpSquadronOrderBits`, a member flag other than
     `kIsDead`, and a non-finite countdown. The per-member size passed to the
     member-count bound grows by one float. The squadron-agreement check is
     deleted. The launch tick takes no range check: every load resets it, and
     replays are exempt from the trust policy (root `AGENTS.md`
     `## Key Patterns`).
   - Replay fleet record: `ReplayFleetRecordBytes` returns the
     `WriteFleetData` bytes alone and `ReadReplayFleetRecord` is
     `ReadFleetData` alone (still dropping the all-disconnected owner map). The
     countdown tail and the GUID-keyed launch-tick list with its count are
     deleted.
   - `RebuildFleetMembersForLoad` still resets every countdown and launch tick
     on every load, so a grid save's stored values are never used by a load;
     they exist so the two layouts are one.
   - `SquadronOrdersRequestMessage` carries `uiOrders`; `kiSize` and its
     `static_assert` follow.
   - The `ReadFleetData` fleet-GUID uniqueness check stays (fleet lookup takes
     the first match); only the launch-timer reason in its comment goes. The
     `FleetMember::respawnTimerSeconds` comment in `Fleet.h` now says grid
     saves and replay fleet records carry the value and the wire does not.
8. Request path. `ServerSession::HandleGamePacket` throws a corrupt stream for
   a global ID at or below zero or a `uiOrders` bit outside
   `kSquadronOrderBits`. The handler ignores a bit outside the member type's
   `kpSquadronOrderBits` and a request equal to the slot; no separate "type
   without settings" test is needed, since a Battleship's allowed bits are
   empty and its slot is always empty.
   `ClientSession::SendSquadronOrdersRequest` and the HUD send `ShipOrders_t`.
9. Versions. Bump `engine::kiProtocolVersion`
   (`Engine/Source/Network/NetworkProtocol.h`; FleetSync header and request),
   the base term of `Frame::kiVersion` (`Frame/Frame.cpp`; grid save), and
   `ReplayMeta::kiVersion` (`Game.h`; replay fleet records). The last also
   closes the gap commit `de0c3c54` left. `FrameInput::kiVersion` is unchanged:
   no `StatusChange` payload changes, because `UpdateSquadronOrdersData`
   already carries `ShipOrders`.
10. Docs and leftovers, through `/update-claude-docs` for the `AGENTS.md`
    files: the regions `## In scope` names. Update comments naming deleted
    symbols in `Frame/Collections/Ships/ShipTypes.h`. Remove this work's row
    from `Documents/Investigations/Engine/RefactorCandidates.md`.

### Slice 2: automatic fleet syncs

Slice 2 runs after slice 1 in the same session and settles the
`SendFleetSyncToClient` half of criterion 5.

1. `MarkFleetChanged` appends the owner to `mChangedFleetOwners` unless it is
   already there. `MarkAllFleetsChanged` marks every `mGuidToClientId` key.
   `FlushFleetSyncs` sends one full fleet sync per marked connected owner and
   clears the list. `ResetState` clears the list.
2. Every write of a value FleetSync carries (member list, `kIsDead`, a slot,
   delay, behaviour, hold) calls `MarkFleetChanged`: each hand-placed
   `SendFleetSyncToClient` call slice 1 kept or moved is replaced by one, and
   `AddFleetMember` gains one. `OnClientConnected` marks its owner.
3. On a load reset, `ServerSession::OnResetForLoad` calls
   `MarkAllFleetsChanged` after its per-client loop and `FlushFleetSyncs` last,
   so a connected client with no fleet still gets its empty fleet sync; the
   per-client `ServerFleetManager::OnResetForLoad` no longer sends.
4. `ServerSession` calls `FlushFleetSyncs` last in `AfterNetworkPoll`,
   `BuildAdvancingFrameInputs`, `TickGameTimers`, `FinalizeTickClients`, and
   `OnResetForLoad`. A navigation request needs no flush of its own:
   `DispatchDeferredClientPackets` runs just before `AfterNetworkPoll` in both
   polls. `PrepareReplayPlaybackTick` replaces its send loop with
   `MarkAllFleetsChanged` and `FlushFleetSyncs` after its record adoption,
   because that adoption runs after `TickGameTimers` has flushed.
5. `SendFleetSyncToClient` becomes private and is called only from
   `FlushFleetSyncs`.

Syncs coalesce per phase: several spawns in one fleet step, or a connect and a
create in one poll, send one fleet sync instead of one each. The spawn's sync
still leaves in the phase that queues the spawn, which the client's
newest-member focus relies on (`FleetSelection::SynchronizeFleets`). One
accepted visible difference: when two members are added to a fleet with no
focused member in one poll, focus goes to the last instead of the first.

### Invariants

- `Fleet::members` stays in add order; a member keeps one squadron for life;
  squadron choice stays `FindOpenFleetSquadron`.
- An emptied squadron's next member starts at the default; a member joining a
  non-empty squadron, and every respawn, spawns with the slot's value.
- Each request queue is still emptied by the handler that processes it.
- `TickRespawnTimers` keeps its walk (owners in GUID order, then list order)
  and so the order of the tick's post-tick spawn batch; the bytes of every
  queued `StatusChange` and spawn are the ones the baseline code queues, so no
  frame CRC changes.
- Rows are written only by spawn, transfer, and the named update records;
  transfer and load never consult the fleet.
- Record bytes stay a pure function of fleet state in the fixed walk; the
  fleet checksum stays replay-only and outside `Frame::Crc`.
- Every main-loop growth site sits in a `ScopedSuppressAllocationTracking`
  scope with a `// Heap:` note. `mChangedFleetOwners` growth in
  `DetectFleetMemberDeaths` and in `UpdateFleetNavigation` needs no scope of
  its own: the engine callers `ServerSessionRuntime::CompleteTick`, `Poll`, and
  `PollTickBoundary` (`Engine/Source/Network/Server/ServerSessionRuntime.cpp`)
  already hold a scope around them, which is why the fleet sync those
  functions send today is allowed.
- Every fleet manager and `FleetRows` entry point runs on the main thread
  between ticks.
- The squadron-orders `ASSERT` in the shared live-member loop holds: the
  handler drains in `BuildAdvancingFrameInputs`, after the previous tick's
  `DetectFleetMemberDeaths`, so a member not marked dead has its row, which the
  destroy path already relies on. Only `kUpdateFleet` is reachable during
  playback (requests are refused in `HandleGamePacket`), so the shared
  `mbReplaying` test changes nothing for destroy and squadron orders.
- The server validates the request and every grid-save fleet read without
  partially applying it; the client trusts FleetSync.

### Risk tier

Change Workflow tier: 3. Triggers: wire/protocol (FleetSync header, the
squadron-orders request) and save/replay format (`WriteFleet`/`ReadFleet`,
replay fleet records). No Collection or row column is added, so
`/add-collection` and `/add-collection-member` do not apply.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Fleet.h`
- `Projects/BrokenEngineSandbox/Source/Game.h`
- `Projects/BrokenEngineSandbox/Source/Network/GameMessages.h`
- `Projects/BrokenEngineSandbox/Source/Network/Server/FleetRows.h` (new)
- `Projects/BrokenEngineSandbox/Source/Network/Server/FleetRows.cpp` (new)
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.h`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.h`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.h`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Server/FleetNavigationController.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.h`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp`
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp`
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipTypes.h`
- `Engine/Source/Network/NetworkProtocol.h`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj.filters`
- `Documents/Architecture/Network.md`
- `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Save/AGENTS.md`
- `Documents/Investigations/Engine/RefactorCandidates.md`

Verified unaffected, because they read only `kIsDead`, `eType`, `uiSquadron`,
and the member list: `FleetSelection.cpp`, `Game.cpp`
`ClientOwnedEntityIdentifier`, `Agent/AgentScene.cpp` `BuildFleets`, and the
HUD fleet panel boxes.

## In scope

Slice 1:

- `Fleet.h`: `FleetMemberFlags` and the deleted flag sets and helpers
  (`## Design` slice 1 item 1); new `kpSquadronOrderBits`,
  `kpDefaultSquadronOrders`, `Fleet::pSquadronOrders`,
  `Fleet::iNextFighterLaunchTick`; the `FleetMember::respawnTimerSeconds`
  comment.
- `Network/GameMessages.h`: `SquadronOrdersRequestMessage` (member, `Visit`,
  `kiSize`, `static_assert`); `FleetSyncMessage` `kiFleetHeaderSize`, its
  `static_assert`, `VisitFleetHeader`, and whatever of its write and read
  functions the deleted member flag bits touch.
- `Network/Server/FleetRows.h` and `Network/Server/FleetRows.cpp`: new, the
  four functions and their file-local loop.
- `Network/Server/ServerFleetManager.h`/`.cpp`:
  `PendingSquadronOrdersRequest`, new `FleetMemberLocation`, `FindFleetMember`,
  `AddFleetMember`; `CreateFleet`, `ProcessSpawnIntoFleetRequests`,
  `ProcessDestroyFleetMemberRequests`, `ProcessSquadronOrdersRequests`,
  `TickRespawnTimers`, `RebuildFleetMembersForLoad`.
- `Network/Server/ServerClientManager.h`/`.cpp`: `SpawnFleetMember`,
  `ResetState` (the launch-tick clear), the `mNextFighterLaunchTicks` member.
- `Network/Server/FleetNavigationController.cpp`: `TickFleetTimers` (the
  member loop, its `mbReplaying` test, the pick log).
- `Network/Server/ServerFleetSerialization.h`/`.cpp`: `WriteFleet`,
  `ReadFleet`, `ReplayFleetRecordBytes`, `ReadReplayFleetRecord`,
  `ReplayFleetRecord`, the `ReadFleetData` comment.
- `Network/Server/ServerSession.cpp`: `HandleGamePacket`
  (`kClientSquadronOrdersRequest` arm).
- `Network/Client/ClientSession.h`/`.cpp`: `SendSquadronOrdersRequest`.
- `Ui/Screens/HudScreen.cpp`: the squadron settings controls in
  `RenderFocusedOwnedEntityPanel`.
- `Save/GameSaveLoad.cpp`: the launch-tick adoption line in
  `PrepareReplayPlaybackTick`.
- `Game.h` `ReplayMeta::kiVersion`; `Frame/Frame.cpp` the `Frame::kiVersion`
  base term; `Engine/Source/Network/NetworkProtocol.h` `kiProtocolVersion`.
- `Frame/Collections/Ships/ShipTypes.h`: comments on the `ShipOrders` setting
  bits that name deleted symbols; no code.
- `BrokenEngineSandboxServer.vcxproj` and its `.filters`: membership of the
  two new files, through `/update-vcxproj`.
- `Documents/Architecture/Network.md`: the `kClientSquadronOrdersRequest`
  contract row, the FleetSync member-flags description, and a new protocol
  version history entry.
- `Network/Server/AGENTS.md`: the squadron-settings bullet in
  `## State Ownership`; the launch-tick wording and the squadron-orders drain
  wording in `## Deterministic Tick Contracts`.
- `Frame/Collections/Ships/AGENTS.md`: the spawn bullet in
  `## Ship Invariants`.
- `Save/AGENTS.md` `## Replay Contract`: the record contents (the record is
  the grid-save fleet bytes) and the playback adoption bullet.
- `Documents/Investigations/Engine/RefactorCandidates.md`: remove the
  `## Entries` row that names this Plan.

Slice 2:

- `Network/Server/ServerFleetManager.h`/`.cpp`: new `MarkFleetChanged`,
  `MarkAllFleetsChanged`, `FlushFleetSyncs`, `mChangedFleetOwners`;
  `SendFleetSyncToClient` made private; every `SendFleetSyncToClient` call
  site (`ProcessCreateFleetRequests`, `ProcessSpawnIntoFleetRequests`,
  `ProcessDestroyFleetMemberRequests`, `ProcessSquadronOrdersRequests`,
  `TickRespawnTimers`, `OnClientConnected`, `OnResetForLoad`,
  `DetectFleetMemberDeaths`, `UpdateFleetNavigation`); `AddFleetMember`;
  `ResetState`.
- `Network/Server/ServerSession.cpp`: `AfterNetworkPoll`,
  `BuildAdvancingFrameInputs`, `TickGameTimers`, `FinalizeTickClients`,
  `OnResetForLoad`.
- `Save/GameSaveLoad.cpp`: the fleet sync loop in `PrepareReplayPlaybackTick`.
- `Network/Server/AGENTS.md` `## Deterministic Tick Contracts` and
  `Save/AGENTS.md` `## Replay Contract`: fleet-sync wording.

Naming a file grants only the named regions plus the includes and declarations
they need.

## Out of scope

- Adding new squadron orders or behaviours; only existing settings move.
- User control of squadron membership, and squadron-aware escort layout.
- Changing what any moved setting does in simulation.
- `FleetStance` and `FleetBoard`, which stay derived per tick.
- Random-spawn rows and enemy waves.
- A frame Collection, any row column, `SpawnInfo`, or `StatusChange` payload
  change, and `FrameInput::kiVersion`.
- A nested `Squadron` type, any change to `Fleet::members` order, and any
  change to how a request names a squadron (one member's global ID).
- `kUpdateShip`, `pNavigationDelays`, and
  `Fleet::uiPendingFleetWantedCoordinateTicks` behaviour.
- Row-against-fleet agreement checks on load, and a range check on the saved
  launch tick.
- Using a grid save's stored countdowns or launch tick on load; every load
  still resets them.
- Unit tests, and backward compatibility with older peers, saves, or replays.

## Acceptance criteria

No harness run backs this Plan; its source was a code read.

1. `/compile` builds the BrokenEngineSandbox client and server.
2. The repository static checks pass.
3. Code read: `Fleet.h` holds one storage site per squadron
   (`Fleet::pSquadronOrders`) and `FleetMemberFlags` holds only `kIsDead`. The
   baseline fails this.
4. Code read: `AddFleetMember` resets an empty squadron's slot to its default,
   and no code copies a squadronmate's settings.
5. Code read: under `Network/Server/`, only `FleetRows.cpp` queues a fleet
   spawn or pushes `kUpdateFleet`, `kUpdateSquadronOrders`, or `kDestroyShip`;
   and `SendFleetSyncToClient` is called only from `FlushFleetSyncs`. The
   baseline fails both.
6. Code read: `engine::kiProtocolVersion`, the `Frame::kiVersion` base, and
   `ReplayMeta::kiVersion` are each above their baseline values 53, 188, and 7.
7. Code read: `ReadFleet` throws for a slot holding a bit outside its type's
   `kpSquadronOrderBits`, for a member flag other than `kIsDead`, and for a
   non-finite member countdown; `ServerSession::HandleGamePacket` throws a
   corrupt stream for a `uiOrders` bit outside `kSquadronOrderBits`.
8. Code read: `ReplayFleetRecordBytes` writes nothing after the
   `WriteFleetData` bytes, `ReadReplayFleetRecord` reads nothing after
   `ReadFleetData`, and `mNextFighterLaunchTicks` no longer exists.
9. Harness (`/agent-harness`): with two or more live members in a Fighter
   squadron and in an Omni squadron, change each squadron's settings in the
   HUD. Every live member row of that squadron (the server ship query's order
   names) and the HUD show the new values, the other squadron's rows do not
   change, and a spawn-into and a destroy each show in the HUD fleet panel.
10. Harness: set the settings, `save`, change them, `load`; rows and the HUD
    show the saved values. Then destroy a Fighter row and an Omni row with a
    harness `inject_payload` `DestroyShip` status-change entry (not the
    destroy-member request, which removes the member); each respawns with the
    saved settings.
11. Harness: the replay determinism acceptance in
    `.agents/skills/agent-harness/references/replay.md`
    `### Replay determinism acceptance`, over a recording that changes a
    setting, lets a fleet pick a new cell, and respawns a Fighter, shows no
    frame CRC or fleet checksum mismatch.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
