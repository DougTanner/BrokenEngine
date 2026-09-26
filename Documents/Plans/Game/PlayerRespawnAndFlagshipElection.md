<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-26T19:30:13.021Z","dependsOn":["Documents/Plans/Game/FleetMemberGlobalIdReferences.md"]} -->
# Respawn dead fleet Players automatically and keep one flagship per fleet

## Context

User direction (verbatim intent): "we should have a dead Player respawn at 0,0
and move to rejoin the fleet (1 second delay?), if flagship dies the fleet
should elect a new one, and respawning Player will not be flagship anyway (can
only have one)". The user chose this work over a Plan that proposed only
rejecting the reserved tag in replays; that Plan was rejected by user direction
on 2026-09-26, deleted in the landing that adds this one, and is recoverable
from Git history. This Plan therefore also owns closing the reserved
`kRespawnPlayer` hole that Plan described.

What the code already provides at baseline `28b74695`:

- **Death.** A Player row is removed by the Destroy sweep once it is exploding
  and its destroyed timer runs out, or by a `kDestroyPlayer` status change
  (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:228-264`).
  The server notices the missing row after the tick in
  `ServerClientManager::DetectPlayerDeaths` (`Network/Server/ServerClientManager.cpp:173-239`)
  for connected owners and `ServerFleetManager::DetectDisconnectedPlayerDeaths`
  (`Network/Server/ServerFleetManager.cpp:472-524`) for disconnected owners.
  Both run from `ServerSession::FinalizeTickClients`
  (`Network/Server/ServerSession.cpp:268-275`) and set `FleetMember::bAlive`
  false (`Fleet.h:25-30`); the member stays in the fleet list. "Dead" in this
  Plan means that state.
- **Manual respawn.** Clicking a dead HUD row (`Ui/Screens/HudScreen.cpp:299-313`)
  sends `kClientRespawnInFleetRequest`; `ServerFleetManager::ProcessRespawnInFleetRequests`
  (`ServerFleetManager.cpp:153-195`) queues `ServerClientManager::QueueSpawnForClient`
  with the member, deduplicated per (client, fleet, member)
  (`ServerClientManager.cpp:17-34`). On the next advancing update,
  `ServerBroadcaster::BuildFrameInputs` (`Engine/Source/Network/Server/ServerBroadcaster.cpp:34-63`)
  runs `SpawnWaitingClients` (`ServerClientManager.cpp:112-148`), which mints a
  fresh global ID and emits a `kSpawnPlayer` into `engine::kOriginCoord`
  (`Engine/Source/Frame/GridCoord.h:42`, cell (0,0)) at the default
  `SpawnPlayerData` offset of (45, -12) m from that cell's center
  (`Frame/StatusChange.h:57-69`). The payload carries the owner GUID, the
  fleet's wanted coord and pending ticks, and `bIsFlagship` from
  `LookupFleetWantedCoord` (`ServerFleetManager.cpp:450-470`).
  `OnPlayerSpawned` (`ServerFleetManager.cpp:288-340`) then replaces the dead
  member.
- **Rejoining.** A non-flagship whose fleet wanted coord differs from its cell
  steers across cells toward it, and one in the same cell follows the flagship
  (`Frame/Collections/Players/PlayersNavigation.cpp:65-149`). A spawn at the
  origin cell carrying the fleet's wanted coord therefore already travels back
  to the fleet.
- **Flagship election.** `FleetNavigationController::ShiftFlagshipAfterDeath`
  (`Network/Server/FleetNavigationController.cpp:198-219`) moves the flagship
  to the next alive member in list order and queues a flagship update, which
  `ProcessFlagshipUpdates` (`:118-186`) turns into `kUpdateFleet` status
  changes. It runs from `OnPlayerDeath` (`ServerFleetManager.cpp:254-286`), the
  disconnected-owner reap, `OnClientConnected` (`:364-385`), and
  `ResetFleetForLoad` (`:432-448`). The Players Navigation Invariants in
  `Frame/Collections/Players/AGENTS.md` describe the D+2 recovery after a
  reassignment. When no member is alive the flagship index stays on the dead
  member.
- **Respawned member becoming flagship.** `OnPlayerSpawned` makes the spawned
  member flagship whenever the fleet has no alive flagship other than it
  (`ServerFleetManager.cpp:326-339`), and `LookupFleetWantedCoord` marks the
  spawn flagship when its member index equals the flagship index (`:465`).
  With another member alive, `ShiftFlagshipAfterDeath` has already moved the
  flagship, so a respawn is not flagship today; only an all-dead fleet hands
  the flagship to the respawned member.
- **Reserved `kRespawnPlayer`.** The tag is a reserved, payload-less arm mapped
  to an empty `TransferData` (`Frame/StatusChange.h:13`, `:171-189`); no
  producer emits it. `ProcessSpawnStatusChanges` enters its creation branch for
  it but only fills global ID, owner, and fleet fields for `kSpawnPlayer`
  (`Players.cpp:266-317`), so such a record creates a row with global ID zero
  that no ownership or fleet scan can address. The replay reader accepts the
  tag (`Frame/FrameInput.cpp:33-54`), the network codec treats it as a known
  zero-byte arm (`Engine/Source/Network/NetworkSerialization.cpp:142`, `:184`,
  `:346`), and a carried replay difference can re-create the row every tick.

The gap against the user direction: no respawn happens without a HUD click,
there is no respawn delay, and the reserved tag can still create an
unaddressable row. Whether an all-dead fleet keeps handing the flagship to the
first respawn is an open decision (design item 6).

## Design

Every choice below is the author's recommendation with its rationale; the
claiming session confirms or replaces each one with the user before
implementation, per `.agents/references/authority-order.md`.

1. **Automatic respawn trigger (server-only).** When a fleet member becomes
   dead, arm a per-member respawn countdown of 1 second; when it expires on an
   advancing update, queue the existing respawn path
   (`QueueSpawnForClient` with that member). Rationale: the existing path
   already mints a real global ID, sets ownership, and carries the fleet's
   wanted coord, so no new frame-side spawn logic is needed. A respawn queued from the
   fleet-timer tick lands after the broadcaster's stale-spawn drop filter
   (`ServerBroadcaster.cpp:34-49`) and is consumed by `SpawnWaitingClients`
   (`:63`) in the same update, so it bypasses that filter; this is safe because
   the queuing pass has just found the fleet.
2. **Delay and timer home.** Store the countdown in the server fleet state
   (`FleetMember`), in seconds, decremented by `gpGame->mfLastDeltaTime` in a
   new `ServerFleetManager` function called from
   `ServerFleetManager::TickFleetTimers` (`ServerFleetManager.cpp:197-203`),
   which already runs only on advancing updates in tick lockstep
   (`ServerBroadcaster.cpp:54-58`). Not inside
   `FleetNavigationController::TickFleetTimers`: its per-fleet loop skips a
   fleet whose flagship index is out of range or dead
   (`FleetNavigationController.cpp:39-48`), so a countdown there would never
   expire for an all-dead fleet.
   One second is `engine::kiTickRate` (32) ticks (`Engine/Source/Frame/TimeStep.h:6`).
   Seconds rather than a tick counter because one update can advance several
   ticks (`Engine/Source/GameBase.cpp:277`). Keep the check before
   `SpawnWaitingClients` so ordering inside `BuildFrameInputs` stays fixed.
3. **Save/load of a pending countdown.** Recommendation: do not persist it.
   `ResetFleetForLoad` re-arms a full delay for every dead member, so the fleet
   save layout and `Frame::kiVersion` are unchanged. The alternative, writing
   the countdown in `WriteFleet`/`ReadFleet`
   (`Network/Server/ServerFleetSerialization.cpp:33-53`, `:55-108`), requires a
   `Frame::kiVersion` base bump (`Frame/Frame.cpp:33`) because the fleet payload
   has no version of its own (`Save/AGENTS.md`).
4. **Where "0,0" is.** Recommendation: the existing spawn point, cell
   `kOriginCoord` at the default `SpawnPlayerData` offset. Positions are
   cell-local (`Engine/Source/Frame/AGENTS.md`), so "0,0" can only name a cell
   plus a local offset; reusing the spawn point keeps one spawn location. The
   alternative is offset (0,0) — the origin cell's center — which changes only
   the two offsets the respawn passes.
5. **Disconnected owners.** `SpawnWaitingClients` needs a connected client id
   for assignment and ownership (`ServerClientManager.cpp:139-141`).
   Recommendation: the countdown runs only for connected owners; dead members
   of a disconnected owner respawn after the full delay once `OnClientConnected`
   re-arms them.
6. **Flagship rule.** A respawned member is never flagship while any other
   member is alive, and a fleet has at most one flagship. Election on flagship
   death keeps `ShiftFlagshipAfterDeath` as it is. Recommendation for the
   all-dead fleet: the first member to respawn becomes flagship, because a
   fleet with no alive flagship stops `TickFleetTimers` from moving it
   (`FleetNavigationController.cpp:44-48`); the user's "can only have one"
   holds since no other member is alive. Confirm this with the user.
7. **HUD manual respawn.** Recommendation: keep it. It feeds the same
   deduplicated queue, and a click during the countdown only respawns sooner.
   The alternative is removing the HUD click and `kClientRespawnInFleetRequest`,
   which is a wire change.
8. **Reserved `kRespawnPlayer`.** Recommendation: respawns keep using
   `kSpawnPlayer`; the enumerator stays (append-only), is removed from the
   Players creation branch so it can never create a row, and the replay
   `FrameInput` reader rejects it through the existing corrupt-replay recovery
   (`Engine/Source/File/Replay.cpp:747-760`). Wire bytes, `FrameInput::kiVersion`
   (`Frame/FrameInput.h:11`), and `engine::kuiProtocolVersion`
   (`Engine/Source/Network/NetworkProtocol.h:63`) stay unchanged. The
   alternative gives `kRespawnPlayer` a `SpawnPlayerData` payload and emits it
   for respawns; that changes the variant arm, the codec size/read/write arms,
   and requires bumping both `FrameInput::kiVersion` and `kuiProtocolVersion`,
   for no simulation difference from `kSpawnPlayer`.
9. **Replay playback.** The claiming session establishes whether the fleet
   hooks (`TickFleetTimers`, `SpawnWaitingClients`, death detection) run while
   `gpGame->mbReplaying` is set, and ensures an automatic respawn cannot mint
   IDs or change fleet state during playback beyond what the recorded
   `FrameInput` stream carries.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Fleet.h:25-41` — `FleetMember` countdown field.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.h` / `.cpp` — `TickFleetTimers`, `OnPlayerDeath`, `DetectDisconnectedPlayerDeaths`, `OnClientConnected`, `ResetFleetForLoad`, `OnPlayerSpawned`, `LookupFleetWantedCoord`.
- `Projects/BrokenEngineSandbox/Source/Network/Server/FleetNavigationController.h` / `.cpp` — `TickFleetTimers`, `ShiftFlagshipAfterDeath`.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp:17-148` — spawn queue and `SpawnWaitingClients`.
- `Engine/Source/Network/Server/ServerBroadcaster.cpp:17-63` — advancing-update ordering in `BuildFrameInputs`.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:266-317` — spawn creation branch.
- `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.cpp:33-54` — replay status tag admission.
- `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:9-30`, `:171-189` — reserved tag and variant mapping.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:33-108` — only if design item 3 persists the countdown.

## In scope

- A per-member respawn countdown on `FleetMember`, armed where a member becomes
  dead for a connected owner (`OnPlayerDeath`) and re-armed in
  `OnClientConnected` and `ResetFleetForLoad`, cleared when the member respawns
  in `OnPlayerSpawned`, per design item 5.
- Countdown drain and expiry in a new `ServerFleetManager` function called from
  `ServerFleetManager::TickFleetTimers`, per design item 2, queuing the
  existing respawn through `QueueSpawnForClient`.
- The all-dead flagship decision in `OnPlayerSpawned` and
  `LookupFleetWantedCoord`, only as far as design item 6 requires.
- Removing `kRespawnPlayer` from the creation branch in
  `ProcessSpawnStatusChanges` and rejecting it in `FrameInput::operator>>`, per
  design item 8, with the matching comment in `StatusChange.h`.
- Fleet save read/write and the `Frame::kiVersion` base term, only if design
  item 3 is decided as persisted.
- `Network/Server/AGENTS.md` and `Frame/Collections/Players/AGENTS.md` lines
  describing automatic respawn and the flagship rule, through `/update-claude-docs`.

## Out of scope

- Changing `ShiftFlagshipAfterDeath` order or the flagship-follow navigation in
  `PlayersNavigation.cpp`.
- Changing member identity from list position to global ID; that belongs to
  `Documents/Plans/Game/FleetMemberGlobalIdReferences.md`, which this Plan
  depends on.
- New status tags, new `SpawnPlayerData` fields, `NetworkSerialization.cpp`
  size/read/write arms, `FrameInput::kiVersion`, and `kuiProtocolVersion`,
  unless design item 8 is decided for the payload alternative.
- HUD countdown display, FleetSync wire changes, and removal of the manual
  respawn request, unless design item 7 is decided for removal.
- Enemy spaceship spawning, Player combat, and the Destroy sweep.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: the change alters which `kSpawnPlayer`
status changes enter the deterministic `FrameInput` stream and when, spans
independently owned subsystems (game server fleet management, the engine
`ServerBroadcaster` ordering, and the Frame Players spawn consumer), and
touches replay admission of a status tag; the persisted-countdown and
payload alternatives add save format and wire/protocol changes.

Invariants to preserve:

- No path creates a Player row without a real, stable, nonzero global ID; a
  `kRespawnPlayer` record never creates a row and a carried replay difference
  cannot re-create one each tick.
- At most one alive flagship per fleet; a respawned member is not flagship
  while another member is alive.
- Spawns enter `FrameInput` only on advancing updates, in the existing
  `BuildFrameInputs` order, so client, server, and replay CRCs match.
- One live Player row per global ID; a respawn is minted only for a member that
  is still dead when the spawn is built.
- Replay playback consumes the recorded `FrameInput` exactly; automatic respawn
  adds no spawn during playback.

## Acceptance criteria

- Live (`/agent-harness`), fleet of at least two members, flagship alive: after
  a wingman dies, the server log shows a `kSpawnPlayer` for that member about
  32 ticks later at cell (0,0) without any HUD click, the harness fleet JSON
  shows the member alive again and not flagship, and its row later reaches the
  fleet's cell.
- Live: when the flagship dies, the next alive member gets `kIsFlagship` in the
  following ticks, the dead flagship respawns about 1 second later without
  `kIsFlagship`, and no tick shows two alive members flagged.
- Live: when every member of a fleet is dead, respawns follow design item 6 as
  decided.
- A replay containing a `kRespawnPlayer` record fails through the existing
  corrupt-replay path and leaves no global-ID-zero row.
- A replay recording spanning a death and an automatic respawn plays back with
  no checksum mismatch.
- Client and Server `Debug|x64` build clean through `/compile`.

## Notes

- The originating decision-blocked items are design items 3 through 8; the
  claiming session resolves them with the user before implementation.
- The rejected Plan's invariant (no global-ID-zero row from a reserved tag) is
  carried here as the first invariant.
