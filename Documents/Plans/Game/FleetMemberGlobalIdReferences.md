<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-26T17:35:07.819Z","dependsOn":[]} -->
# Identify fleet members by global ID instead of list position

## Context

Every stored reference to a fleet member today is its position in
`Fleet::members`. The planning survey found these six (F1-F6); line numbers are
at primary `5527a6d5`:

- F1 `Fleet::iFlagshipIndex` (`Projects/BrokenEngineSandbox/Source/Fleet.h:36`):
  server state, the fleet save/replay stream
  (`Network/Server/ServerFleetSerialization.cpp:39`, `:65`), and FleetSync.
- F2 the FleetSync copy of that field (`Network/GameMessages.h:91`, checked at
  `:151-152`), read by the client at `FleetSelection.cpp:167-169`.
- F3 `PendingRespawnInFleetRequest::iMemberIndex`
  (`Network/Server/ServerFleetManager.h:41`), sent by the HUD from its row
  number (`Ui/Screens/HudScreen.cpp:308`) as the 8-byte field of
  `kClientRespawnInFleetRequest` (`Network/Client/ClientSession.cpp:311-316`,
  decoded at `Network/Server/ServerSession.cpp:140-141`).
- F4 `ClientSpawnInfo::iMemberIndex` (`Network/Server/ServerClientManager.h:16`),
  queued across polls and consumed by the spawn slot overwrite
  (`ServerFleetManager.cpp:306-309`), the flagship assignment (`:327-335`), the
  dedup (`ServerClientManager.cpp:28`), and the broadcaster drop filter
  (`Engine/Source/Network/Server/ServerBroadcaster.cpp:41`).
- F5 `LookupFleetWantedCoord`'s `iMemberIndex` compared with the flagship index
  (`ServerFleetManager.cpp:465`), which sets `SpawnPlayerData::bIsFlagship` and
  so reaches `FrameInput`, the CRC, and replay.
- F6 `FleetSelection::miFocusedPlayerInFleetIndex` (`FleetSelection.h:49`),
  which drives `Game::ClientPlayerId` (`Game.cpp:75-92`), the camera cell, and
  the remembered ship written to `ClientState.bin` (`Game.cpp:674-684`).

Paths without a root are under `Projects/BrokenEngineSandbox/Source/`.

Members are only appended (`ServerFleetManager.cpp:321`) or overwritten in place
on respawn (`:309`) today, so none of these can point at the wrong member yet.
Once members are reordered, inserted, or removed, each one silently names a
different member, and F1/F5 then put the wrong `kIsFlagship` into the
deterministic Frame and the save/replay stream.

No per-member identity exists to replace them. `FleetMember::globalPlayerId` is the ship's
`engine::global_id_t` (`Engine/Source/Frame/Collections/CollectionId.h:10-15`),
minted by the only-increasing `GameBase::GenerateGlobalId`
(`Engine/Source/GameBase.h:233`) and persisted as `iNextGlobalId`
(`Engine/Source/File/GridSave.cpp:24`, `:94-100`). But a respawn mints a fresh ID
(`Network/Server/ServerClientManager.cpp:119`) and overwrites the member
(`ServerFleetManager.cpp:309`), so today it identifies a ship, not a member.

User decisions (this session) that bind this Plan:

1. A fleet member's identity is its ship's existing `engine::global_id_t`. A
   respawned member keeps its global ID across death and respawn instead of
   receiving a newly minted one. Temporary objects and ships outside a fleet are
   unchanged.
2. Use the existing `global_id_t` server counter: no new 128-bit type, no
   per-fleet counter, no fleet-level member GUID.
3. Every fragile stored member position (F1-F6) is replaced by the member's
   `global_id_t`, with lookups by ID. Transient loop indices stay.
4. Bump the save format version so the existing version check rejects old saves;
   no conversion code. Bump the network protocol version for the wire change.
5. Load validation: a nonempty fleet's flagship ID names a member of that fleet,
   an empty fleet's flagship ID is the invalid (0) ID, and member IDs within a
   fleet are valid and unique. Violations are rejected through the existing
   corrupt-save path (`std::ios_base::failure` from `ReadFleet`). This replaces
   the negative-index check at `ServerFleetSerialization.cpp:66-74`.
6. The reorder/add/remove features themselves are out of scope.

### Reusing an ID across respawn: collision check

The planning session checked every holder of a player global ID, to make sure a
reused ID cannot collide with anything that still holds it:

- Frame rows. A member is marked dead only when its row is missing from the
  current Frame of the cell the server has recorded for it:
  `ServerClientManager::DetectPlayerDeaths` (`ServerClientManager.cpp:200-230`),
  `DetectDisconnectedPlayerDeaths` (`ServerFleetManager.cpp:497-519`), and
  `RefreshFleetMembers` against the registry that `RelinkFromFrames` rebuilds
  from Frame rows (`:415-430`). Transfer ownership relinks run before death
  detection, and death detection skips a client mid-transfer
  (`Network/Server/AGENTS.md` `## Deterministic Tick Contracts`;
  `ServerClientManager.cpp:190-194`). A respawn is accepted only for a dead
  member (`ServerFleetManager.cpp:185`) and is minted on a later advancing
  `BuildFrameInputs`. So when the respawn row with ID X is created, no other row
  with ID X exists in any Frame, and at most one row with a given global ID
  exists at any tick. The dead row's copy in an older Frame is found only by its
  Collection uuid (`puiIds`), and a respawn gets a new uuid. The previous-frame
  flagship scan (`Frame/Collections/Players/PlayersNavigation.cpp:115-124`)
  matches on the flag, not on the global ID. `pGlobalPlayerIds` is excluded
  from the CRC (`Frame/Collections/Players/Players.h:305`).
- `engine::OwnedEntityRegistry`. At death, `RemoveAt` runs before
  `OnPlayerDeath` (`ServerClientManager.cpp:227-229`). At respawn, `Add` runs
  in `SpawnWaitingClients` (`:141`). The ID is never registered twice.
- Player event messages. `kDied` (`ServerClientManager.cpp:225`) and
  `kAssigned`/`kSpawned` (`:139-140`) both go on the reliable channel
  (`Network/Server/ServerSession.cpp:296-329`), so the client always applies
  `kDied` X before `kAssigned` X. `ClientSession::ApplyPlayerEvent` removes X,
  then adds it back through `IsClientPlayer`
  (`Network/Client/ClientSession.cpp:111-143`).
- The remembered focused ship ID (`ClientSettings.cpp:66`, `:80`, `:98`;
  `Game::mRememberedFocusedShipId`). It already stores a `global_id_t`, and with
  kept IDs it keeps naming the same member after a respawn. The format does not
  change.
- Save and replay. `iNextGlobalId` only grows. A respawn no longer consumes a
  counter value, which can only make IDs sparser. Replay playback takes spawn
  IDs from the recorded `SpawnPlayerData::iGlobalId`, not from the counter.

The one new hazard: a respawn spawn that sits queued while its member changes.
Minting would then give the ship a reused ID while that member is alive (a
duplicate live ID) or after it has left the fleet. Neither can happen at
baseline: disconnect and load both clear the queue
(`ServerClientManager.cpp:163-167`, `ResetState`), and members are never
removed. The design below re-checks the member at spawn time, using the
broadcaster's existing drop path.

## Design

The author recommends:

- **State.** Replace `Fleet::iFlagshipIndex` with a `global_id_t` field
  (suggested name `flagshipGlobalPlayerId`). Its default `{}` means "no
  flagship" and is the value an empty fleet carries. Replace
  `PendingRespawnInFleetRequest::iMemberIndex` and
  `ClientSpawnInfo::iMemberIndex` with a `global_id_t` (suggested
  `memberGlobalPlayerId`). In `ClientSpawnInfo`, `{}` means "spawn a new
  member" and replaces the `-1` sentinel. Change the
  `LookupFleetWantedCoord` and `QueueSpawnForClient` parameters to match. Find
  a member by ID with `std::ranges::find` on `&FleetMember::globalPlayerId` at
  each site, the pattern already used for `&Fleet::guid` in
  `FleetNavigationController.cpp:128`. No new helper.
- **Keeping the ID on respawn.** `SpawnWaitingClients` uses the queued member ID
  when it is valid and calls `GenerateGlobalId` only for a new member.
  `OnPlayerSpawned` finds the respawned member by ID and sets `bAlive` and
  `coord`, leaving its ID and list position unchanged. A new member is still
  appended under the member cap. A respawn whose member is not found returns
  without changing the fleet. It never appends.
- **Re-checking the member at spawn time.** For a valid member ID,
  `LookupFleetWantedCoord` reports `kFound` only when the fleet exists *and* has
  a dead member with that ID. The existing drop filter in
  `ServerBroadcaster::BuildFrameInputs` then drops a stale respawn before any
  spawn is minted, so a live ID is never duplicated. `bIsFlagship` becomes
  "member ID equals the flagship ID, or a new member of an empty fleet". For
  append-only data this gives the same values as today.
- **Flagship consumers.** Compare by ID in `OnPlayerDeath`,
  `DetectDisconnectedPlayerDeaths`, `OnClientConnected`, `ResetFleetForLoad`,
  `OnPlayerSpawned` (flagship reassignment), `TickFleetTimers`, and
  `ProcessFlagshipUpdates`. A flagship ID that names no member is skipped
  exactly where an out-of-range index is skipped today.
  `ShiftFlagshipAfterDeath` keeps today's rotation order: it looks up the
  current flagship's list position by ID, only as a loop start, scans forward
  from there, and stores the chosen member's ID. When the flagship ID names no
  member, the scan starts at the first member.
- **Respawn request.** `ProcessRespawnInFleetRequests` finds the member by ID
  and ignores the request when the member is missing or alive. The HUD sends the
  dead row's `globalPlayerId`. The request payload stays 16B fleet GUID + 8B
  int64 with the same contract row sizes; only its meaning changes to a member
  global ID. The dedup key becomes (client, fleet, member ID), so a pending
  new-member spawn is still deduplicated as before.
- **Save/replay stream.** `WriteFleet`/`ReadFleet` store the flagship ID's
  `iValue` in the existing int64 slot. After reading the members, `ReadFleet`
  applies user decision 5 and throws `std::ios_base::failure` on any violation:
  a nonzero flagship ID on an empty fleet, a nonempty fleet whose flagship ID
  names no member, an invalid member ID, or a duplicate member ID within the
  fleet. Bump the `Frame::kiVersion` base term (`Frame/Frame.cpp:33`, `130`
  -> `131`). That constant gates both grid saves and the replay grid
  (`Engine/Source/File/GridSave.cpp:16-20`, `:74-78`; `Engine/Source/File/Replay.cpp:670`, `:799`).
  The `FrameInput` byte format does not change, so `FrameInput::kiVersion`
  stays.
- **FleetSync.** The header field carries the flagship ID's int64 in the same
  place, so header size (36) and member size (9) are unchanged. `ReadPayload`
  replaces its position check with the matching ID relation, checked after the
  member loop: an empty fleet has flagship `{}`, and a nonempty fleet's flagship
  ID names one of its members. It keeps throwing through `ThrowCorruptStream`.
  Bump `engine::kuiProtocolVersion` (`Engine/Source/Network/NetworkProtocol.h:63`,
  `16` -> `17`), because FleetSync and the respawn request change meaning while
  keeping their byte layout. The Engine Network rule is that every incompatible
  wire change bumps it (`Engine/Source/Network/AGENTS.md`).
- **Client focus.** Replace `FleetSelection::miFocusedPlayerInFleetIndex` with a
  `global_id_t` (suggested `mFocusedMemberGlobalId`).
  `SelectPlayerInFleet` and its `Game.h` forwarder take the member's ID, and
  the getter `FocusedPlayerInFleetIndex` is replaced by one that returns the ID
  (suggested `FocusedMemberGlobalId`).
  `ClientPlayerId`, `CaptureClientStateIfChanged`, the HUD selection highlight
  and click, `AutoSelectFirstAliveMember`, and `SyncFleets` resolve the member by
  ID. `SyncFleets` keeps its current policy expressed through IDs:
  - restore the remembered alive ship, otherwise the flagship ID;
  - drop a focused ID that names no member of the focused fleet;
  - when the member count grew or nothing is focused, focus the last member's ID;
  - when the focused member is dead, fall back to the first alive member.

  Then an ID that is still present keeps its member through any reorder.
- **Docs.** In `Documents/Architecture/Network.md`, update the
  `kClientRespawnInFleetRequest` row ("member-index lookup" -> member global-ID
  lookup) and the current protocol version sentence (`16` -> `17`, noting that
  fleet member references are now global IDs). Add one line to
  `Network/Server/AGENTS.md` `## State Ownership`: a fleet member is identified
  by its ship's global ID, which a respawn keeps, and stored member references
  carry that ID. `/update-claude-docs` owns the final wording.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Fleet.h` — `Fleet` flagship field.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.h` / `.cpp` — respawn request struct, `ProcessRespawnInFleetRequests`, `OnPlayerDeath`, `OnPlayerSpawned`, `OnClientConnected`, `ResetFleetForLoad`, `LookupFleetWantedCoord`, `DetectDisconnectedPlayerDeaths`.
- `Projects/BrokenEngineSandbox/Source/Network/Server/FleetNavigationController.cpp` — `TickFleetTimers`, `ProcessFlagshipUpdates`, `ShiftFlagshipAfterDeath`.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.h` / `.cpp` — `ClientSpawnInfo`, `QueueSpawnForClient`, `SpawnWaitingClients`.
- `Engine/Source/Network/Server/ServerBroadcaster.cpp:32-46` — queued-spawn drop filter.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp` — `WriteFleet`, `ReadFleet`.
- `Projects/BrokenEngineSandbox/Source/Network/GameMessages.h` — `FleetSyncMessage::VisitFleetHeader`, `ReadPayload`.
- `Projects/BrokenEngineSandbox/Source/Network/GamePacketType.h:40`, `Network/Server/ServerSession.cpp:134-143`, `Network/Client/ClientSession.h` / `.cpp` `SendRespawnInFleetRequest` — respawn request wire.
- `Projects/BrokenEngineSandbox/Source/FleetSelection.h` / `.cpp`, `Game.h:120-121`, `Game.cpp` `ClientPlayerId` / `CaptureClientStateIfChanged`, `Ui/Screens/HudScreen.cpp:270-315` — client focus.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:33` — `Frame::kiVersion` base; `Engine/Source/Network/NetworkProtocol.h:63` — `kuiProtocolVersion`.

## In scope

- `Fleet::iFlagshipIndex` replaced by a `global_id_t` flagship field, and every
  read and write of it in `ServerFleetManager.cpp`,
  `FleetNavigationController.cpp`, `ServerFleetSerialization.cpp`,
  `GameMessages.h`, and `FleetSelection.cpp`.
- `PendingRespawnInFleetRequest::iMemberIndex` and
  `ClientSpawnInfo::iMemberIndex` replaced by a member `global_id_t`. Also the
  matching parameters of `QueueSpawnForClient`, `LookupFleetWantedCoord`, and
  `ClientSession::SendRespawnInFleetRequest`; the `kClientRespawnInFleetRequest`
  decode in `ServerSession::ParseReceivedGamePackets`; and the contract-row
  comment in `GamePacketType.h`.
- `ServerFleetManager::ProcessRespawnInFleetRequests`: find the member by ID and
  ignore the request when the member is missing or alive.
  `ServerFleetManager::ProcessSpawnIntoFleetRequests`: pass the invalid `{}`
  member ID to `QueueSpawnForClient` in place of the literal `-1`.
- `ServerClientManager::SpawnWaitingClients`: reuse the queued member ID
  instead of minting one. `ServerClientManager::QueueSpawnForClient`: dedup on
  the member ID.
- `ServerFleetManager::OnPlayerSpawned`: find the respawned member by ID and
  revive it in place; assign the flagship by ID.
  `ServerFleetManager::LookupFleetWantedCoord`: `kFound` requires a dead member
  with the given ID for a respawn; `bIsFlagship` compares IDs.
- The `ServerBroadcaster::BuildFrameInputs` drop filter's use of the renamed
  `ClientSpawnInfo` field and lookup parameter.
- `ReadFleet`: the user-decision-5 validation, which replaces the
  negative-index check and its comment. `WriteFleet`: write the flagship ID.
- `FleetSyncMessage::VisitFleetHeader`: carry the flagship ID.
  `FleetSyncMessage::ReadPayload`: the ID relation check.
- `FleetSelection::miFocusedPlayerInFleetIndex`, `SelectPlayerInFleet`,
  `FocusedPlayerInFleetIndex`, `AutoSelectFirstAliveMember`, `SyncFleets`,
  `Clear`; the `Game.h` forwarders; `Game::ClientPlayerId`;
  `Game::CaptureClientStateIfChanged`; the HUD member-list selection, click,
  and respawn send in `HudScreen.cpp`.
- `Frame::kiVersion` base term `130` -> `131`; `engine::kuiProtocolVersion`
  `16` -> `17`.
- The `Documents/Architecture/Network.md` respawn row and protocol-version
  sentence, and the one `Network/Server/AGENTS.md` `## State Ownership` line
  described under Design.

## Out of scope

- Reorder, insert, or remove of fleet members, and any UI for them (user
  decision 6).
- `FleetSelection::miFocusedFleetIndex` (`FleetSelection.h:48`). It is a position in the
  fleet list, not the member list, and `SyncFleets` already re-anchors it by
  `FleetGuid` on every sync. Also the harness `index` field in
  `Agent/AgentScene.cpp:96-103`.
- The "member count grew means focus the last member" and "fleet count grew
  means focus the last fleet" heuristics in `SyncFleets`, beyond expressing the
  chosen member by ID. Also the count-keyed HUD toggles
  (`mSpawnIntoFleetToggle`, `mCreateFleetToggle`, `mDeleteFleetToggle`).
  The future member add/remove feature owns those policies.
- Transient loop and search indices, which are recomputed and used within one
  call and never stored across ticks, saves, or the wire:
  - the fleet position from `FindFleetIndexByGuid`
    (`ServerFleetManager.cpp:31-41`) and its immediate uses, including the
    fleet erase at `:105`;
  - server member loop counters (`ServerFleetManager.cpp:262-265`, `:350-353`,
    `:417`, `:486-491`; `FleetNavigationController.cpp:36`, `:142-144`), and
    the `ShiftFlagshipAfterDeath` rotation loop (`:201-204`) apart from its
    starting point;
  - Frame-collection loop counters that look up `globalPlayerId` each tick
    (`FleetNavigationController.cpp:81-83`, `:164-168`;
    `ServerFleetManager.cpp:504-506`;
    `Frame/Collections/Players/PlayersNavigation.cpp:115-124`) and
    `UpdateFleetData::iPlayerUuid`, a Collection uuid resolved in the same
    update;
  - serialization loops in `ServerFleetSerialization.cpp` and `GameMessages.h`,
    which keep list order but store no position;
  - client loop counters in `FleetSelection.cpp`, `HudScreen.cpp`,
    `Agent/AgentScene.cpp`, and `Game.cpp`, and the position-paired
    `Game::mClientPlayerIds`/`mClientPlayerCoords` lists, which every access
    searches by `globalPlayerId`;
  - the HUD "Ship %lld" label and `ImGui::PushID(i)`, rebuilt every frame;
  - the `PendingCreate/Delete/SpawnInto` requests and `PendingFlagshipUpdate`,
    which carry only client and fleet identifiers.
- `ClientState.bin` / `ClientSettings.cpp` format and version: it already
  stores a `global_id_t`.
- `FrameInput::kiVersion`, `StatusChange`/`SpawnPlayerData`/`UpdateFleetData`
  layout, Players collection columns, and CRC membership.
- A new member-ID type, per-fleet counter, fleet-level member GUID, or any
  conversion or compatibility reader for old saves, replays, or clients (user
  decisions 2 and 4).
- Validating member IDs against `iNextGlobalId`, uniqueness across fleets or
  owners, and uniqueness of FleetSync member IDs on the client.
- `ReadFleetData` owner-map handling, including its existing duplicate-owner
  rejection (`ServerFleetSerialization.cpp:175-178`), and `FleetGuid` emptiness
  checks.

## Risk tier and invariants

Expected Change Workflow Tier 3. Triggers: a save/replay format change
(`Frame::kiVersion` bump and new `ReadFleet` validation), a wire/protocol change
(FleetSync and `kClientRespawnInFleetRequest` meaning, `kuiProtocolVersion`
bump), and a change to deterministic `FrameInput` content (the source of
`SpawnPlayerData::bIsFlagship` and the respawned `iGlobalId`), spanning
game-owned fleet code and engine-owned `ServerBroadcaster`.

Invariants to preserve:

- Determinism/CRC. `bIsFlagship` and each `kUpdateFleet` flag get the same
  values as before for append-only fleets. A respawned row's global ID comes only
  from its `kSpawnPlayer` status change, so client, server, and replay agree. No
  CRC member changes.
- One live Player row per global ID at every tick. A respawn is minted only for
  a member that is dead at spawn time, as checked by the broadcaster drop filter.
- Replay. Recording and playback of a run that includes a respawn stay
  bit-identical. Old replay grids are rejected by `Frame::kiVersion`.
- Transfer. `TransferData::globalPlayerId` and `OnPlayerTransferred` lookup by
  ID are unchanged, and a transferred member keeps flagship status.
- Reconnect relink. `OwnedEntityRegistry` removes the dead ID before it is
  re-added. `RelinkFromFrames`/`RefreshFleetMembers` match members by the kept
  ID. The flagship survives disconnect, reconnect, and save/load.
- Trust boundaries. Invalid fleet saves fail through the existing
  post-header corrupt-save path before adoption. Malformed FleetSync fails
  through `ThrowCorruptStream`. Unknown or alive respawn IDs are ignored.

## Acceptance criteria

- No stored fleet-member position remains: `iFlagshipIndex`, `iMemberIndex`,
  `miFocusedPlayerInFleetIndex`, and `FocusedPlayerInFleetIndex` no longer
  appear in `Projects/` or `Engine/`.
- Live (`/agent-harness`), with a fleet of at least two members: after a member
  dies and its HUD row is clicked, the harness fleet JSON shows that member
  alive again with the same `globalPlayerId` and in the same list position. The
  server log's `kSpawnPlayer` line shows that same GlobalId.
- Live: when the flagship dies, the next alive member gets `kIsFlagship`. The
  focused ship stays focused across a client disconnect and reconnect.
- Live: a save/load round trip with a fleet that has a dead member and an empty
  fleet loads successfully and keeps the flagship member. A save written before
  the version bump is rejected with the existing `ReadGrid ... version` error.
- A replay recording that spans a death and a respawn plays back with no
  checksum mismatch.
- The diff alone settles the `ReadFleet` rejection rules, the FleetSync relation
  check, and the protocol rejection of a version-16 client.
- Client and Server `Debug|x64` build clean through `/compile`.

## Coordination

`ReadFleetData` in `ServerFleetSerialization.cpp` already rejects a duplicate
owner `ClientGuid` through a checked `try_emplace` on the same corrupt-save path
(`:175-178`). This Plan edits only `ReadFleet` and `WriteFleet` in that file and
leaves that check unchanged; the new flagship and member-ID checks stay separate
predicates in `ReadFleet`.
