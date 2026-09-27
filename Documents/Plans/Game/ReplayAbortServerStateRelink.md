<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T13:53:53.377Z","dependsOn":[]} -->
# Relink server client and fleet state after an aborted replay playback

## Context

Audit finding PA-F-002 was raised while the Plan
`Documents/Plans/Game/PlayerRespawnAndFlagshipElection.md` was being implemented.
The manager chose to record it here and not fix it in that change. The defect
already exists: it does not depend on that Plan's diff. That Plan's automatic
respawn is what lets the defect happen without a click, which is why this Plan
depends on it.

Replay playback and its abort exist only when `kbDebugInput` is true, which is
Debug builds only (`Projects/BrokenEngineSandbox/Source/Pch.h:34,51,68`).
`Replay::SaveLoadReplay` (`Engine/Source/File/Replay.cpp:403`) and
`Replay::SyncReplayTick` (`:771`) are both guarded by it, and so are the F7/F8
replay keys (`Engine/Source/Input/Input.cpp:59-66`).

Verified root cause:

- **Adoption relinks; abort does not.** When playback is adopted,
  `game::OnStateReplaced()` runs (`Engine/Source/File/Replay.cpp:729`). That is
  `ServerSession::ResetClientsForLoad`
  (`Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:44-47`), which
  rebuilds `mClientPlayers` from the current frames (`RelinkFromFrames`) and
  refreshes every fleet member's `FleetMemberFlags::kIsDead` flag against them
  (`ServerFleetManager::OnResetForLoad` -> `ResetFleetForLoad` ->
  `RefreshFleetMembers`). Quickload and new game go through the same hook
  (`GameSaveLoad.cpp:83,100`). `Replay::ClearReplayAbortState`
  (`Replay.cpp:299-305`) keeps the current frames, inputs, and active set so
  that live simulation resumes from the replayed state. It clears only
  replay-owned state, transfers, and broadcast status changes. It never relinks
  server client or fleet state. It is called on a second F8 during playback
  (`Replay.cpp:417-421`), on a failed replay load while replaying (`:752`), and
  on every `abortReplay()` return in `SyncReplayTick` (`:1013-1018`, used at
  `:1039-1106`). When playback ends normally, the loop reloads the replay
  (`:1119-1123`), so adoption relinks again. Only an abort misses the relink.
- **Death detection runs during playback; the respawn bookkeeping does not.**
  `ServerSession::FinalizeTickClients`
  (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:268-271`)
  runs `ServerClientManager::DetectPlayerDeaths` and
  `ServerFleetManager::DetectDisconnectedPlayerDeaths` against the replayed
  frames. A recorded death therefore removes the `mClientPlayers` entry and sets
  the member's `kIsDead` flag (`ServerFleetManager::OnPlayerDeath`,
  `ServerFleetManager.cpp:292`). Replay input re-creates the recorded
  respawn row with the same global ID. `kIsDead` is cleared on two paths.
  `ServerFleetManager::OnPlayerSpawned` (clear at `ServerFleetManager.cpp:353`)
  is called only from the live spawn path
  `ServerClientManager::SpawnWaitingClients` (`ServerClientManager.cpp:112-150`).
  `ServerFleetManager::RefreshFleetMembers` (clear at `:469`) clears it for a
  member whose global ID is in the client's `mClientPlayers` entry, and runs
  from `OnClientConnected` (after the `kConnect` `RelinkFromFrames` at
  `ServerClientManager.cpp:71-72`) and from `ResetFleetForLoad` (the `game::OnStateReplaced()` path above).
  Neither runs after the replayed respawn for a client that stays connected:
  its `RefreshFleetMembers` ran at adoption, before that respawn, and a
  replay-created row reaches `mClientPlayers` only through
  `RelinkFromFrames`. After an abort, the member can have `kIsDead` set while
  its Player row exists in the current frame.
- **The stale dead member can be respawned.** A HUD respawn request passes
  `ServerFleetManager::ProcessRespawnInFleetRequests`
  (`ServerFleetManager.cpp:161`), and `LookupFleetWantedCoord`
  (`:501`) still finds the member dead. So `SpawnWaitingClients` emits a
  `kSpawnPlayer` for the respawned member's existing global ID. The Players
  spawn consumer (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:266-299`)
  and `PlayersPostRender::Spawn(SpawnInfo)` (`:405`) do not check for duplicate global
  IDs, so a second row with the same global ID is created. Once the prerequisite
  Plan's automatic respawn lands, the same stale dead member is respawned
  without any click.

## Design

Author's recommendation: in `Replay::ClearReplayAbortState`, call the existing
state-replacement hook `game::OnStateReplaced()` after the current clears. The
abort adopts the replayed frames as the live state, which is the same kind of
state replacement that adoption, quickload, and new game already handle with
that hook. Reusing it also rebuilds `mClientPlayers`, fleet liveness, and
flagship assignment, clears pending client, transfer, broadcaster, and fleet
request queues, advances the load generation, and sends clients the load
notification, just as adoption does. `ClearReplayAbortState` already calls one
game hook (`game::OnReplayStreamsInvalidated()` through
`ClearReplayTransientState`), so the Engine-to-game call pattern already
exists.

Rejected alternative: a narrower relink that clears and rebuilds
`mClientPlayers` and calls `ServerFleetManager::OnResetForLoad` per client,
without the load generation and notification. It would duplicate most of
`ResetClientsForLoad` and add a second relink path to keep in sync.

Exposure:

- Determinism/CRC: fleet state is not in the CRC. It decides which
  `kSpawnPlayer` status changes enter frame input, and so what the frame
  contains after an abort. The change should not change the CRC of any tick
  that runs without an abort.
- Wire: existing load-notification, assign-player, player-state, and fleet-sync
  messages are sent at one more point, the abort. No message or format changes.
- Replay/serialization: no replay, save, or `.pack` format change and no
  `kiVersion` bump.
- Build: Debug only in effect. The call sits inside code guarded by
  `kbDebugInput`.

## Critical files

- `Engine/Source/File/Replay.cpp` — `Replay::ClearReplayAbortState`, its three
  call sites, and the adoption call to `game::OnStateReplaced()`.
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp` —
  `game::OnStateReplaced`.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp` —
  `ResetClientsForLoad`, `RelinkFromFrames`, and `FinalizeTickClients`.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp` —
  `OnResetForLoad`, `ResetFleetForLoad`, `RefreshFleetMembers`,
  `ProcessRespawnInFleetRequests`, and `LookupFleetWantedCoord`.

## In scope

- `Replay::ClearReplayAbortState` in `Engine/Source/File/Replay.cpp`: after an
  aborted playback, relink server client and fleet state to the retained
  frames. The Design section gives the recommended mechanism.

## Out of scope

- Any change to `ServerSession::ResetClientsForLoad`, `RelinkFromFrames`, or
  `ServerFleetManager` functions. The abort reuses them as they are.
- Fleet members of disconnected owners. `OnResetForLoad` runs only for
  connected clients, both at adoption and after this change; that gap is
  separate.
- Rows that replay input spawned for new fleet members.
  `RefreshFleetMembers` updates only members the fleet already lists.
- A duplicate-global-ID check or ASSERT in the Players spawn consumer or
  `PlayersPostRender::Spawn`.
- Normal playback end and loop reload, which already relink through adoption.
- Release and Profile builds, where playback does not exist.

## Risk tier

Change Workflow Tier 3. Trigger: the change spans independently owned
subsystems (the Engine replay lifecycle in `Engine/Source/File/` and the game
server session and fleet state in
`Projects/BrokenEngineSandbox/Source/Network/Server/`). It also changes which
spawn status changes enter frame input after an abort, which is a
determinism-input surface.

Invariants:

- After any abort, every fleet member's `kIsDead` flag is clear exactly when its
  global ID has a row in the current frame at its owner's coords, for connected owners.
- A global ID never has more than one Player row.
- Ticks that run without an abort keep a byte-identical CRC sequence.

## Acceptance criteria

Verified live in a Debug build through `/agent-harness`:

1. Record a replay in which a connected client's fleet member dies and is then
   respawned. Play it back and abort playback after the recorded respawn tick.
   The member reads as alive in server fleet state and is owned by its client.
2. After that abort, a respawn request for that member, whether from the HUD or
   automatic under the prerequisite Plan, does not emit a `kSpawnPlayer`. No
   global ID has more than one row in any Players collection.
3. Replay determinism checks for a recording that plays through without an
   abort still pass.

## Notes

Recorded from audit finding PA-F-002 during
`Documents/Plans/Game/PlayerRespawnAndFlagshipElection.md`. The HUD-click path
exists at baseline `3a7a24829b22d2658d27742b9595ba64b04d1a25`.
