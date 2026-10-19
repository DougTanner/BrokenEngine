<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-19T03:03:53.081Z","dependsOn":[]} -->
# Verify the three-ship-type split at runtime

## Context

The change that split the game's ships into three per-type collections —
Fighters (renamed from Spaceships), Omnis, and Battleships, each row driven by
a CRC'd `pController` (`ShipController::kFleet` or `kRandomSpawn`,
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipTypes.h`) —
landed by the user's direction with its remaining acceptance checks skipped.
Find that landing with
`git log --diff-filter=A --format=%H -- Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.h`;
its parent is the pre-split baseline for every "before" comparison below.

Already verified before landing (context only, not repeated):

- Debug client and server built clean.
- Random spawns of every type fight fleets, with client and server CRC agreement
  over 30 s (origin `confirmedTick` advancing, never -1; no `CONFIRMED DESYNC`
  or `RecoverFromDesync` client log line).
- Battleship front and rear guns: every shot inside the front or rear 45-degree
  arc, a front and a rear shot on one tick, each gun 33 ticks between shots.
- The fleet-follows-own-flagship red run on the pre-split code (a wingman
  tracked the other fleet's flagship).
- Code, comment, coherence, progressive-disclosure, and adversarial reviews;
  the adversarial fix limited the HUD weapon toggle to Omnis.
- A server crash on a fleet-Omni spawn: `Assert failed: "rpCursor - pItemStart
  <= kiMaxStatusChangeBytesPerItem"` at
  `Engine/Source/Network/NetworkSerialization.cpp:33` (`engine::SerializeGroup`),
  fixed by raising `engine::kiMaxStatusChangeBytesPerItem` from 120 to 128
  (`Engine/Source/Network/NetworkSerialization.h:19`). That raise has had no
  focused review and no runtime retest.

Unverified: the runtime checks and code readings below.

## Design

Run each check as written. A check that passes needs no edit. A check that
fails is a defect in the landed split: diagnose it with
`/external-diagnose-bug`, fix it inside `## In scope`, and rerun that check plus
every check its fix can reach. The author recommends running the cap review
first, because the fleet-Omni spawn in check 1 and check 8 both pass through
the status-change batch the cap bounds.

Fixed decision (the user's, after the split's plan was approved): Fighter and
Battleship escorts keep the designed heading steering and may trail loosely, so
check 2 carries no "within 50 m" bound.

Harness notes: once a ship is focused, the HUD FleetPanel hides off-screen, so
move the mouse to (120,200) before every UI poll or click on it; fleet member
labels read `<Type> <n> (x,y) #<globalId>`; `query_omnis` no longer exists —
`query_collection` takes `fighters`, `omnis`, or `battleships`. A server crash
during a check is a failed check, not a retry: record the crash report headline.

Director freeze (used by checks 2 and 8): `pause` per
`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`;
`query_collection` `fighters`, `omnis`, `battleships` at the origin; one
`edit_frame` holding a `frame` write of `fSpawnTimer` -3600 (finite and below
`kfShipSpawnInterval` 0.5, so `AdmitSpawnTimer` in
`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp` accepts it; the director
runs only at or above 0.5 s and the timer gains one tick's delta per tick, so
no wave spawns for an hour) plus the kill-state write of every `RandomSpawn`
row; advance one tick so its Destroy phase removes them; `query_collection`
again shows no `RandomSpawn` row. Every `edit_frame` precedes every
`inject_payload`, because with a handshaken client an edit drops queued
entries. Set timescale back to 1/1 while paused before unpausing.

### Cap review

Focused `/repo-code-review` of `engine::kiMaxStatusChangeBytesPerItem` 128.
Per-payload serialized totals (field widths per `WriteStatusChangeField` in
`Engine/Source/Network/StatusChangeFields.h`; the per-type group header lies
outside the per-item ASSERT):

- `OmnisPostRender::SpawnInfo` 124 (pre-split 113): 3 vectors 48, alignment 4,
  `eController` 1, orders 2, armor and shield 8, 5 durations 20, flags 2,
  3 durations 12, `globalId` 8, `flagshipGlobalId` 8, `fleetWantedCoordinate`
  8, 2 uint8 2, `bTransfer` 1.
- `BattleshipsPostRender::SpawnInfo` 105; `FightersPostRender::SpawnInfo` 101.
- `MissilesPostRender::SpawnInfo` 98; `engine::ExplosionsPostRender::SpawnInfo`
  77; `BlastersPostRender::SpawnInfo` 57.
- `UpdateFleetData` 27; `UpdateShipData` 15; `DestroyShipData` 9
  (`Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h`).

Every dependent derives from the constant: `MaxSerializedStatusChangeBatchBytes`,
`kiMaxCompressStatusChangeWorkbufferBytes` (consumed by the workbuffer preflight
in `Engine/Source/Network/Server/ServerBroadcaster.cpp`),
`MaxCompressedStatusChangeBatchBytes` (compression scratch in
`Engine/Source/Network/Server/ServerBufferedFrames.cpp`), and the serialize
reservation in `NetworkSerialization.cpp`. Expected: 128 bounds every payload,
no dependent states the old value, and no derived buffer is undersized.

### Runtime checks (`/agent-harness`)

1. Fleets spawn each type: create a fleet with `[+]##Fleet`, click
   `[+F]##Fighter`, `[+O]##Omni`, `[+B]##Battleship`, then `query_collection`
   each collection at the origin. Expected: one row each with `controller`
   `Fleet`, nonzero `globalId`, and `orders` bit `0x0001` (`kIsFlagship`) on
   exactly one of the three; `describe_scene` lists `fighter`, `omni`,
   `battleship` units; no server crash.
2. Fleet follows its own flagship: `reset`; HUD `[+]##Fleet`,
   `[+B]##Battleship`, `[+F]##Fighter` twice (fleet 1: Battleship flagship,
   two Fighter wingmen), `[+]##Fleet`, `[+F]##Fighter` twice (fleet 2: Fighter
   flagship, Fighter wingman); director freeze; one `edit_frame` moving both
   fleet-2 rows as a group about 200 m west, so the two flagships start at
   least 150 m apart; unpause; ten `query_collection` samples at 1 s. Expected
   on every sample: each wingman's `flagshipGlobalId` equals its own fleet
   flagship's `globalId`; each wingman's `local` lies nearer its own flagship
   than the other fleet's; each wingman more than 50 m from its own flagship is
   in follow-flagship mode (navigation direction 5, `flags` bits 8-10 = 6, per
   `GetNavigationDirection` in `ShipTypes.h`). A wingman whose flagship is in
   another cell is settled by a code read of `FollowFlagship`
   (`Frame/Collections/Ships/ShipBehaviours.cpp`).
3. Cross-cell transfer of each type, per
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/cross-cell.md`:
   `inject_payload` `SpawnFighter`, `SpawnOmni`, `SpawnBattleship` with
   `eController` `Fleet`, then `UpdateFleet` with a neighbouring
   `fleetWantedCoordinate`. Expected: `replay_transfer_capture` or `get_logs`
   shows one `TransferFighter`, `TransferOmni`, `TransferBattleship` arrival,
   and `query_collection` in the destination shows each with the same
   `globalId` and `controller` `Fleet`.
4. Save and load: `save`, then `load`. Expected: the three collections' counts
   and every `globalId`, `controller`, and `flagshipGlobalId` match before and
   after (not `load` success alone — a load whose frame read throws resets
   fresh and still succeeds), and the HUD fleet panel shows each member's type
   name.
5. Replay determinism, per `replay.md`: record a run with all three types, a
   transfer of each, random spawns of all three, and fleet missiles; play it
   back in repeated loops. Expected: no `LogDifferences CRC` mismatch, and on
   Debug no new `Full frames file read failed` or `SaveLoadReplay aborted` line.
6. Rejections: (a) `inject_payload` `SpawnFighter` with an unknown
   `eController` token, and `UpdateShip` with an unknown `eType` token.
   Expected: `ok:false` naming the field from the field parser, and no new
   `fighters` row and no changed row after the next tick. (b) client `send_raw_packet` with 18 bytes:
   `kClientSpawnIntoFleetRequest` (`kGamePacketStart + 2`,
   `Projects/BrokenEngineSandbox/Source/Network/GamePacketType.h`), 16
   fleet-GUID bytes, type byte 3. Expected: server log
   `Server::RecordContractViolation First` (reason `deferred packet handler
   threw`) and no new ship row in `query_frame` counts after the next ticks.
7. Per-controller models on a Debug client (Vulkan debug object names on):
   `query_collection` `fighters` in a cell holding one `Fleet` and one
   `RandomSpawn` Fighter, then `renderdoc_capture` analysed with
   `rdc_summary.py` and `rdc_pipeline.py`
   (`.agents/skills/agent-harness/references/renderdoc.md`). Expected: both
   rows present and model draws bound to the pipelines `FightersFleet` and
   `FightersRandom` (`FightersRender.cpp` `kpcPipelineNames`), one instance
   each; without object names, two model draws with distinct pipelines, one
   instance each. No pixel evidence.
8. Missiles chase and hit Omnis: one fleet Omni flagship from the HUD, HUD
   weapon toggle to `Missiles`; director freeze; `inject_payload` `SpawnOmni`
   with `eController` `RandomSpawn`, `alignment` `random`, default
   `arrivalGracePeriod` 0, at the fleet Omni's `local` plus 30 m east (inside
   the 65 m by 45 m visibility box of `IsVisible` in
   `Engine/Source/Frame/FrameBase.h`, the 160 m `kfMissileTargetRange`, and the
   45 m `kfMissileTargetAcquireRange`); unpause and sample `query_collection`
   `missiles` and `omnis` for 10 s. Expected: a `missiles` row whose
   `registryTargetId` equals the random Omni's `registryId`, and the random
   Omni's `shield` or `armor` lower than at spawn.

### Code readings

- Names gone: `git grep -n -i -E` for `spaceship`, `OmniFlags`, `DestroyOmni`,
  `UpdateOmni`, `iOmniUuid`, `omniAlignment`, `enemyAlignment`, `mOmniAlignment`,
  `mEnemyAlignment`, `kTargetOmni`,
  `kTargetEnemy`, `query_omnis`, `kOwnedEntityTransferType`,
  `ClientOwnedEntityIndex`, `puiPushers` over `Projects/BrokenEngineSandbox`,
  `Engine/Source`, `Documents/Architecture`, and
  `.agents/skills/add-collection*`. Expected: only `kModelsSpaceshipscenegltfCrc`,
  `kModelsspaceship2scenegltfCrc`, and protocol-history sentences in
  `Documents/Architecture/Network.md`.
- Collision masks (`Projects/BrokenEngineSandbox/Source/Frame/HealthDamage.h`):
  each ship type collides with the other two, Blasters, and Missiles, with no
  self-collision; a ram between enemy types lowers both hulls in each
  PostCollision; a missile explosion registers `kuiFleetFired` or
  `kuiRandomFired` from its row's alignment and each of the three
  `AreaDamage` hooks queries only the opposing side's category; the Omni hook
  starts the explosion at armor at or below zero.
- Version gate: the composed `Frame::kiVersion` in `Frame.cpp` strictly greater
  than the baseline's, `FrameInput::kiVersion` 23, `engine::kiProtocolVersion`
  30; the replay admission check rejects an out-of-range `eController` or
  `eType` and a `kFleet` ship spawn with global ID 0, and grid-save load rejects
  an out-of-range saved type byte or `pController` (no harness command reaches
  these).
- Constants and read frames: `kfBattleshipRadius` 4.5, acceleration 4.0, drag
  0.25, max speed 15.0, max turn rate 1.0, chase and flee turn rates 8.0,
  chase 4.0, flee 6.0, return 4.0; `kfBattleshipHealth` 60 beside
  `kfFighterHealth` 10; a Battleship blaster type of size
  `kfBattleshipRadius * 0.15f`; every cross-type ship read in a PostRender
  `Update` (Fighters' `AvoidTerrain` included) takes the previous frame and the
  Spawn-phase blaster targeting the current frame; the director draws one
  `common::Random(2i64, …)` after its no-fleet-ship return and before its
  position search; `engine::SharedCollectionCrc`
  (`Engine/Source/Frame/Collections/Collection.h`) CRCs `SharedCrcMembers()`
  when declared.
- Pusher owners: `ValidatePusherOwners` in `Frame.cpp` compares the Pushers count
  against Fighters plus Omnis plus Battleships, claims each `pPushers` handle
  once, throws `std::ios_base::failure` on a mismatch, and runs after the pair
  checks and before the destination is replaced.

## Critical files

- `Engine/Source/Network/NetworkSerialization.h`, `NetworkSerialization.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/{Fighters,Omnis,Battleships,Ships,Missiles}/`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`, `HealthDamage.h`, `StatusChange.h`, `FrameInput.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp`, `ServerFleetSerialization.cpp`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/`

## In scope

- Running every check above and recording its result.
- A fix for a failed check, limited to the files the split changed (the
  landing commit's file list) and to restoring the expected observation that
  check states.

## Out of scope

- Fleet steering, follow distance, or catch-up tuning for heading-steered ships
  (the fixed decision above).
- New behaviour, tuning, or assets; the transferred-blaster wind-trail tuning
  (its own Plan, `Documents/Plans/Game/TransferredBlasterWindTrailTuning.md`).
- Backward compatibility with pre-split saves, replays, or protocol.

## Acceptance criteria

Every check above meets its expected observation on the current build, and the
cap review reports no defect.

## Notes

- Change Workflow tier: Tier 1 while the change edits no file (verification
  only). A fix for a failed check takes the tier of the surface it touches;
  any fix to CRC'd ship state, a `SpawnInfo` `Fields()` list, the status-change
  cap, grid-save records, or replay input is Tier 3 (determinism/CRC,
  wire/protocol, serialization).
- Live verification exposure: `/agent-harness` client and server, Debug build
  for checks 5 and 7.
