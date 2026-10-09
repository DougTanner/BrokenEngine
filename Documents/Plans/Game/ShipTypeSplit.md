<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T23:54:23.561Z","dependsOn":["Documents/Plans/Engine/FlightPlaneData.md","Documents/Plans/Engine/NetworkServerOverEngineeringCleanup.md","Documents/Plans/Engine/RemoveEngineRuntimeOverEngineering.md","Documents/Plans/Game/CollectionTimesToChrono.md","Documents/Plans/Game/ControllerTypeIndexStaticsToInt64.md","Documents/Plans/Game/ExplosionParticleCountsToInt64.md","Documents/Plans/Game/FrameEditRead.md","Documents/Plans/Game/FrameEditRows.md","Documents/Plans/Game/RemoveSandboxOverEngineering.md","Documents/Plans/Game/SpaceshipZDrift.md"]} -->
# Replace Players and Spaceships with the Fighters, Omnis, and Battleships ship types

Tier 3 (`.agents/references/risk-tiers.md`): the change alters CRC'd gameplay state, the wire format, the save and replay formats, and spans the game and engine. Line numbers cite baseline `c1420821` unless marked `90fa6c85`; the prerequisite Plans rename the float time columns to `std::chrono` and other members and drop dead parameters, checks, and return values, so carry their names and signatures forward where a site cited here has changed. Where a statement here and the code disagree, the code wins; report the contradiction instead of matching one side to the other. Paths below `Frame/`, `Network/`, `Agent/`, and `Ui/` mean `Projects/BrokenEngineSandbox/Source/<dir>/`; `Engine/` paths are repository-relative.

`/add-collection` owns the mechanical wiring of each of the three new Collections and invokes `/add-collection-member` for every SOA column they declare, including every column this Plan adds to a copied column list.

## Context

Today two collections carry ships. `Players` (`Frame/Collections/Players/Players.h:55-347`) are the human fleets: a flagship flag, stable global IDs, client GUIDs, armor plus shields plus client hex shields, accelerated direction-driven movement (`PlayersNavigation.cpp:440-445`), fleet navigation (`ComputeNavigation`), flagship following by the first `kIsFlagship` row in the cell (`PlayersNavigation.cpp:105-138`), blasters and missiles. `Spaceships` (`Frame/Collections/Spaceships/Spaceships.h:34-183`) are the randomly spawned enemies: health only, heading-driven turn-and-thrust movement (`SpaceshipsNavigation.cpp:116-136`), flee/return/chase steering (`SpaceshipsNavigation.cpp:43-114`), Spawn-phase blaster fire at the nearest alive Player (`Spaceships.cpp:413-470`), a registry id column so Missiles can home on them. Nineteen explicit `Players*` dispatch sites in `Frame/Frame.cpp` exist because engine `ForEachPostRenderSpawn`/`ForEachPostRenderUpdate` carry no `FrameInput` (`Engine/Source/Frame/FrameUtils.h:263,333`); Spaceships ride the `FrameCollections.h` tuples.

The behaviours are welded to the collection: a Spaceship cannot join a fleet and a Player cannot be a random spawn. The user decided (this session, binding) to replace both with three per-type collections — Fighters, Omnis, Battleships — whose rows are driven by one of two controllers, Fleet AI or random-spawn AI, through order flags, so that fleets can mix types and the shared-versus-per-type behaviour split can be moved by experiment. The names Player and Spaceship are dropped everywhere.

Evidence the design rests on:

- Fleet membership is by global ID (`Fleet.h:28-43`); `FleetNavigationController.cpp:144-170` scans `pGlobalPlayerIds` per member and pushes `kUpdateFleet`; `ServerClientManager.cpp:109-146` builds `kSpawnPlayer` for waiting clients; `ServerFleetManager.cpp:300-353` adds the member and elects the flagship.
- The engine ownership layer is single-layer by design (`Engine/Source/Frame/FrameRegistry.h:89-97`); `Frame::OwnershipLayer` (`Frame.cpp:538-549`) binds the Players columns. Engine `ServerTransferManager.cpp:26-35,80-82,104-108,239-283,340-346` and `ServerBroadcaster.cpp:222-267` consume it and test `== kTransferPlayer`; engine `ReconcileReplayTick.cpp:124-163` counts transfers per type for logs. Both engine managers already name `game::StatusChange` under the documented exception (`Engine/Source/Network/Server/AGENTS.md` `## Transfers and Publication`).
- The registry query windows bind one Spaceship source layer (`Frame.cpp:462-536`); the engine ranking supports several layers in fixed order (`Engine/Source/Frame/AGENTS.md` `## Frame Registry`).
- `TransferData` (`Frame/StatusChange.h:93-153`) is one union of all transfer payloads; the engine codec writes per-type field subsets (`Engine/Source/Network/NetworkSerialization.cpp:12-120`). The replay reader rejects `kSpawnPlayer` with global ID 0 (`Frame/FrameInput.cpp:68-73`).
- Render model selection is a set of `#if` blocks (`PlayersRender.cpp:17-41`, `SpaceshipsRender.cpp:13-21`), one model per collection.
- Spaceships read previous-frame Players in Update but current-frame Players in `AvoidTerrain` and in the Spawn-phase blaster fire (`Spaceships.cpp:578-580,643`, `Frame/Collections/Spaceships/AGENTS.md:11`). Update order is Players, then Blasters, Missiles, Spaceships (`Frame.cpp:197-200`, `FrameCollections.h:11-19`).
- Same-type collision is unsupported by the engine (`Documents/Investigations/Engine/SameCollectionCollision.md`); Pushers keep same-type ships apart.
- `Documents/Plans/Engine/FlightPlaneData.md` (prerequisite, written in parallel) replaces `gBaseHeight`; this Plan leaves room for flight planes but adds none.
- Terrain contact already goes through the engine: `PlayersPostRender::ResolveTerrainContact` (`Players.h:245`, `PlayersNavigation.cpp:478-493`, both `90fa6c85`) slides and `SpaceshipsPostRender::ApplyTerrainBounce` (`SpaceshipsNavigation.cpp:138-157`, `90fa6c85`) bounces, each around one `engine::ResolveDiscAgainstTerrain` call. `Documents/Plans/Game/SpaceshipZDrift.md` (prerequisite) pins Spaceship Z to the base height in `SpaceshipsInterpolate::Update`; Fighters and Battleships keep that pin.

## Decisions

Each decision is recorded as made; the alternatives are not open. The design is the user's choice "C (caller ergonomics) + A's orders/flags split".

1. **Three separate collections, mirrored by name.** `FightersInterpolate`/`FightersPostRender` (`Frame/Collections/Fighters/`), `OmnisInterpolate`/`OmnisPostRender` (`Frame/Collections/Omnis/`), `BattleshipsInterpolate`/`BattleshipsPostRender` (`Frame/Collections/Battleships/`). No base struct and no embedded block: each header declares its own columns, and a column shared by two or three types has the same name and element type in each. Fighters are today's Spaceships (files and flight physics), Omnis are today's Players, Battleships are a copy of Fighters with the constants in decision 3. A type carries only the columns it uses: Omnis keep armor, shields, shield cooldowns, hex shields, transfer lock, missiles; Fighters and Battleships keep health, damage directions, and delta rotations.
2. **Common columns** (same name in all three `PostRender` structs unless marked): `pIds` (every Interpolate uses `CollectionFlags::kIdToIndex`, so uuid lookups work for all three), `pController` (`ShipController`), `pOrders` (`ShipOrders_t`), `pFlags` (`ShipFlags_t`), `pAlignments`, `pVecVelocities`, `pfArrivalGracePeriods`, `pfDestroyedExplosionTimes`, `pGlobalIds` (`engine::GlobalId`, CRC-excluded, persisted), `pClientGuids` (CRC-excluded, persisted), `pFlagshipGlobalIds` (`engine::GlobalId`, CRC'd), `pFleetWantedCoordinates`, `puiPendingFleetWantedCoordinateTicks`, `pfNavigationDelays`, `pfCellChangeTimers`, `pVecAiDirections`, `pVecIslandDestinations`, and client-only `pVecDebugNavigationWaypoints`; in every Interpolate: `pVecPositions`, `pVecDirections`, `pfDestroyedTimes`, `pPushers`, `pRegistryIds` (`engine::registry_id_t`, persisted, so every type is a Missile target), and client-only `pWindTrails`. Omni-only: `pfArmors`, `pfShields`, `pfShieldCooldowns`, `pfShieldDownSoundCooldowns`, `pfNextBlasterFireTimes`, `pfNextSecondarySpawnTimes`, `pVecWantedDirections`, `pfTransferLockTimers`, `puiPendingWeaponModeTicks`, and the hex-shield, shield-rotation, and rotation-acceleration client columns. Fighter and Battleship only: `pfHealths`, `pVecDamageDirections`, `pfNextBlasterSpawnTimes`, Interpolate `pfDeltaRotations`, client-only `pfAnimationTimes`. Omnis keep `pfAnimationTimes` in `SharedMembers` (CRC-excluded), as Players do today; that asymmetry is today's and stays.
3. **Battleship constants** (`Battleships.h`, replacing the Fighter values in the copied header): `kfBattleshipRadius = 4.5f` (three times the Fighter's 1.5), `kfBattleshipAcceleration = 4.0f`, `kfBattleshipDrag = 0.25f`, `kfBattleshipMaxSpeed = 15.0f`, `kfBattleshipMaxTurnRate = 1.0f`, `kfBattleshipChaseTurnRate` and `kfBattleshipFleeTurnRate` a quarter of the Fighter values, `kfBattleshipHealth = 60.0f` (in `HealthDamage.h`, next to `kfFighterHealth = 10.0f`), pusher radius, intensity, and power derived from the radius exactly as the Fighter's are, model scale three times the Fighter's for both placeholder models, explosion and blaster sizes derived from the radius through the copied formulas. Battleships fire the Fighter blaster type at the Fighter cadence.
4. **Two flag words per ship**, both `common::Flags` over `uint16_t`, declared in `Frame/Collections/Ships/ShipTypes.h` and used by all three types. `ShipOrders` is written only by controllers: `kIsFlagship 0x0001`, `kEscortFlagship 0x0002`, `kHuntNearby 0x0004`, `kUseMissiles 0x0008`, `kPendingUseMissiles 0x0010`; bits 8-15 are reserved for a target flight plane a later Plan adds and stay zero. `ShipFlags` is ship-internal state written by the ship's own phases: `kExploding 0x0001`, `kTransfer 0x0002`, `kFireBlaster 0x0004`, `kBlasterSpawnLeft 0x0008`, `kFireMissile 0x0010`, `kMissileSpawnLeft 0x0020`, `kFlee 0x0040`, `kReturnToIslandCenter 0x0080`, navigation direction in bits 8-10 and navigation waypoint index in bits 12-13 with today's packing (`Players.h:149-185`), moved onto `ShipFlags_t`. `kFlee`, `kReturnToIslandCenter`, and the navigation bits are hysteresis and mode state the ship's own steering writes, so they live in `ShipFlags`; the orders word holds only what a controller sets.
5. **Controllers select behaviour and model.** `ShipController` is a per-row CRC'd column. `kFleet` rows are human-fleet ships: they carry a minted global ID and an owning client GUID, navigate to the fleet wanted coordinate, and follow their flagship. `kRandomSpawn` rows are the director's ships: global ID 0, empty GUID, `kHuntNearby`, today's Spaceship flee/return/chase steering. Each type's `Update` is a per-row script that branches on `pController` and `pOrders` and calls shared per-row behaviours; moving a behaviour between types means moving one call line. The explicit marker for "owned by a fleet" is `pController == kFleet` everywhere (`OwnedShipCount`, `IsOwnedTransfer`); a global ID of 0 is a consequence, never the test.
6. **Shared behaviours** are free functions in `Frame/Collections/Ships/ShipBehaviours.h/.cpp`, each operating on one row's values (today's dominant style, `Players.h:242-250`): `CountdownPendingOrders`, `ComputeNavigation` (today's `PlayersNavigation.cpp` body including `NavigateToFleetCoordinate`), `FollowFlagship` (replaces the first-`kIsFlagship`-in-cell scan at `PlayersNavigation.cpp:105-138`: resolve `pFlagshipGlobalIds[i]` through `FindShipByGlobalId` on the previous frame, same cell only, keeping the follow and close distances and modes 4/5), `ComputeFleeOrReturnDestination` (today's `ComputeSteering` destination and hysteresis logic returning a destination and updating `kFlee`/`kReturnToIslandCenter`), `SteerHeadingTowards` (the turn-rate lerp and decay of `ComputeSteering`), `ApplyAcceleratedMovement` (today's Players `ApplyMovement`), `ApplyHeadingMovement` (today's Spaceships `ApplyMovement`), `ApplyTerrainPush`, `ResolveTerrainContact` (today's `PlayersPostRender::ResolveTerrainContact`, used by Omnis), `ApplyTerrainBounce` (today's `SpaceshipsPostRender::ApplyTerrainBounce`), `ApplyPusherPush`, `AcquireTarget` (today's Players target acquisition, scanning every enemy-aligned ship of every type through the queries below), `RegenerateShield`, `RegenerateHealth`, and `IsNavigationDelayInRange` (moved out of `PlayersPostRender`, shared by the three `PostRead`s, `ServerSession.cpp:74`, `ServerFleetSerialization.cpp:78`, and the harness rigs). Per-type scripts: a `kFleet` Fighter or Battleship runs `CountdownPendingOrders`, `ComputeNavigation` with `FollowFlagship`, then `SteerHeadingTowards(position + AI direction)` and `ApplyHeadingMovement`; a `kRandomSpawn` Fighter or Battleship runs `ComputeFleeOrReturnDestination`, `SteerHeadingTowards`, `ApplyHeadingMovement` with today's constants; a `kFleet` Omni runs today's Players script; a `kRandomSpawn` Omni runs `ComputeFleeOrReturnDestination` and sets its AI direction toward the destination, then `ApplyAcceleratedMovement`. Weapons stay per type: Omnis keep `SpawnBlasters`/`SpawnMissiles` (missiles when `kUseMissiles`); Fighters and Battleships keep the Spawn-phase blaster fire at the nearest enemy ship. Fighters and Battleships gate fleet navigation on arrival grace, as they gate steering today; Omnis keep the transfer lock.
7. **Cross-collection queries** in `Frame/Collections/Ships/ShipQueries.h/.cpp` are the only place that switches over the three types: `ForEachShipType` (the fixed order Fighters, Omnis, Battleships), `FindShipByGlobalId`, `FindNearestEnemyShip` (alive, past arrival grace, enemy alignment), `OwnedShipCount`, `ShipUuidByGlobalId`, `ShipPosition`, `ShipHull` (armor for Omnis, health otherwise; feeds the client armor tracking at `Game.cpp:323-331` and `ClientReconciler.cpp:113`), and `BuildShipRegistryWindow` with three source layers in that order. Both registry query windows keep their shape (`Documents/Architecture/FrameUpdatePipeline.md` `## Frame Registry Query Windows`); acquisition order and count are unchanged apart from the extra layers.
8. **Cross-type reads in PostRender Update take the previous frame** for both Interpolate and PostRender columns, so the tuple order never makes a type read an unwritten current column. This moves Fighters' `AvoidTerrain` ship read from the current frame to the previous frame (one tick of lag, CRC-visible, covered by the version bump). Spawn-phase reads (Fighter and Battleship blaster targeting) use the current frame, because every Update has finished. Update `Frame/Collections/Fighters/AGENTS.md` (today's `Spaceships/AGENTS.md:11`) to say so.
9. **Status changes** (`Frame/StatusChange.h`): `enum class StatusChangeType : uint8_t { kSpawnShip, kTransferFighter, kTransferOmni, kTransferBattleship, kTransferBlaster, kTransferMissile, kDestroyShip, kUpdateShip, kUpdateFleet, kCount }`; `IsTransferType` covers `kTransferFighter..kTransferMissile` with the matching `static_assert`. Payloads carry `ShipType`: `SpawnShipData { ShipType eType; int64_t iGlobalId; bool bIsFlagship; engine::GlobalId flagshipGlobalId; engine::GridCoord fleetWantedCoordinate; uint8_t uiPendingFleetWantedCoordinateTicks; float fSpawnOffsetX, fSpawnOffsetY; engine::ClientGuid clientGuid; }`, `DestroyShipData { ShipType eType; int64_t iUuid; }`, `UpdateShipData { ShipType eType; int64_t iUuid; bool bUseMissiles; std::chrono::duration<float> navigationDelaySeconds; uint8_t uiPendingWeaponModeTicks; }`, `UpdateFleetData { ShipType eType; int64_t iUuid; bool bIsFlagship; engine::GlobalId flagshipGlobalId; engine::GridCoord fleetWantedCoordinate; uint8_t uiPendingFleetWantedCoordinateTicks; }`. `TransferData`: `globalPlayerId` becomes `globalId`, `uiPlayerFlags` becomes `uiOrders` plus `uiFlags`, plus `ShipController eController` and `engine::GlobalId flagshipGlobalId`; every ship transfer type serializes `globalId`, `eController`, `uiOrders`, `uiFlags`, `flagshipGlobalId`, `fleetWantedCoordinate`, `uiPendingFleetWantedCoordinateTicks`, and `navigationDelaySeconds`, because any type can now be fleet-owned; the Omni transfer alone adds the shield, armor-timer, animation, and weapon-mode fields, and the Fighter and Battleship transfers share one layout (`fHealth`, `nextBlasterSpawnTimeSeconds`, `fDeltaRotation`). `kSpawnShip` always spawns a `kFleet` row; the director never goes through a status change.
10. **Frame dispatch.** All three types register in the `FrameCollections.h` tuples in the order Blasters, Missiles, Fighters, Omnis, Battleships (Blasters and Missiles first because they consume the collision and area-damage queues the game phase overrides clear last, `Frame/AGENTS.md`; the three ship types then in `ShipType` order so tuple order and `ShipType` order agree). The only explicit ship dispatch left in `Frame.cpp` is `ApplyUpdateStatusChanges` and `ApplySpawnStatusChanges` (`Frame/Collections/Ships/ShipStatusChanges.h/.cpp`), called where `PlayersPostRender::ProcessUpdateStatusChanges` and the Players spawn hook are called today, and `SpawnShip(Frame&, const ShipSpawnInfo&)` switching on `eType`. The `Frame::kiVersion` sum drops the two Players terms and adds nothing explicit: the tuples carry the three types.
11. **Status change dispatch order within a tick** is unchanged: `kUpdateShip` and `kUpdateFleet` in Update after every ship Update; `kDestroyShip` and `kSpawnShip` in Spawn before the per-type Spawn hooks; arriving transfers through `SpawnTransfer` as today.
12. **Fleets mix types.** `FleetMember` gains `ShipType eType` and its fields drop `Player`: `globalId`, `Fleet::flagshipGlobalId`. `ClientSpawnInformation` gains `ShipType eType`; `kClientSpawnIntoFleetRequest` carries one `uint8_t` ship type after the fleet GUID (contract 17 → 18 bytes, `GamePacketType.h`, `Documents/Architecture/Network.md` contract table); a byte at or above `ShipType::kCount` is corrupt under the bad-value rule. The respawn path reuses the member's stored type. The HUD `[+]##Player` button becomes three buttons `[+F]##Fighter`, `[+O]##Omni`, `[+B]##Battleship` sending that type. The FleetSync wire member gains the type byte (`GameMessages.h` fleet `Visit`), and the HUD member label shows the type name. The fleet save payload (`ServerFleetSerialization.cpp`) writes the member type; grid saves are gated by `Frame::kiVersion` (`Engine/Source/File/GridSave.cpp:17`, `Projects/BrokenEngineSandbox/Source/Save/AGENTS.md`); its bump (decision 22) covers the fleet payload change.
13. **Spawn paths.** `ServerClientManager::SpawnWaitingClients` builds `kSpawnShip` with the requested type (today's offsets, flagship election, and fleet wanted coordinate unchanged) and `flagshipGlobalId` from the fleet lookup. The random-spawn director (`SpawnRandomShipGroup`, today's `SpawnSpaceshipGroup`) uses the first non-exploding `kFleet` ship of any type as its center and spawns Fighters only, `kRandomSpawn`, orders `{kHuntNearby}`, through `SpawnShip`. A `kFleet` spawn carries orders `{kIsFlagship}` for the flagship and `{kEscortFlagship}` otherwise; `kUseMissiles` comes from `kUpdateShip` as today.
14. **Engine seams.** `Frame::OwnershipLayer(const Frame&, ShipType)` binds that type's `pIds`, `pGlobalIds`, `pClientGuids`, and count; the engine `RegistryOwnershipLayer` stays single-layer. Engine `ServerTransferManager` stores the transferred ship's `game::ShipType` in its client-transfer record and passes it to `OwnershipLayer`; destination liveness is `game::OwnedShipCount(frame) > 0`; every `== kTransferPlayer` test (engine `ServerTransferManager.cpp:82,104,137,183,340`, `ReconcileReplayTick.cpp:139`; game `ReconcileReplayClientState.cpp:73`, `ServerSimulationHarnessRigs.cpp:434,502`) becomes `game::IsOwnedTransfer(eType, data)` (a ship transfer type with `eController == kFleet`). Engine `ServerBroadcaster` resolves the update request's uuid and type through `game::ShipUuidByGlobalId`. The game active-set hook (`ServerSession.cpp:292-296`) contributes every coordinate with `OwnedShipCount > 0`. These are the documented engine/game exception sites; `Engine/Source/Network/Server/AGENTS.md` `## Transfers and Publication` names `OwnedShipCount`, `IsOwnedTransfer`, `ShipUuidByGlobalId`, and the typed `OwnershipLayer` as the seam.
15. **Alignments.** `FramePostRender::playerAlignment`/`enemyAlignment` become `fleetAlignment`/`randomAlignment`; `Game::mPlayerAlignment`/`mEnemyAlignment` become `mFleetAlignment`/`mRandomAlignment`. `MissileFlags::kTargetPlayer`/`kTargetEnemy` become `kTargetFleet`/`kTargetRandom`.
16. **Collision.** `CollisionCategory` becomes `kuiBlaster 0x0001`, `kuiFighter 0x0002`, `kuiOmni 0x0004`, `kuiBattleship 0x0008`, `kuiMissile 0x0010`. `CollidesWith`: a ship type collides with the other two ship types, blasters, and missiles; blasters and missiles collide with all three ship types; no type collides with itself (the engine asserts this; same-type separation stays with Pushers). Every ship takes `kfShipCollisionDamage = 5.0f` from a ship collision, whatever the other type (today's Player-only collision damage becomes symmetric; the alignment gate keeps allies from colliding at all). Blaster damage is unchanged. Missile area damage stays on Fighters and Battleships only; Omnis keep taking missiles by direct collision only, as Players do today.
17. **Models per controller.** Each `*Render.cpp` declares two model CRCs and scales, `kFighterFleetModel`/`kFighterRandomModel` and so on, creates two dynamic buffers and two model pipelines per type (names `"FightersFleet"`, `"FightersRandom"`, …), and partitions rows by `pController` in `Render`; `BeginRender` reserves per pipeline. Placeholders, all three types: fleet model `data::kModelsspaceship2scenegltfCrc` (today's Player model and scale), random model `data::kModelsSpaceshipscenegltfCrc` (today's Spaceship model and scale), Battleships three times both scales. The `#if 0` model blocks are deleted. Animation clocks look up the row's own model.
18. **Client focus.** `Game::ClientPlayerIdentifier()` becomes `ClientShipGlobalId()`, `ClientPlayerIndex(const PlayersPostRender&)` becomes `std::optional<ShipLocator> ClientShip(const Frame&)`, `GetClientPlayerPosition` becomes `ClientShipPosition`, `mClientPlayerIdentifiers`/`mClientPlayerCoordinates` become `mClientShipGlobalIds`/`mClientShipCoordinates`, `AddClientPlayer`/`RemoveClientPlayer` become `AddClientShip`/`RemoveClientShip`; camera, HUD, `ClientReconciler`, `ReconcileReplayClientState`, and `FleetSelection::SelectPlayerInFleet` (→ `SelectShipInFleet`, `kSelectPlayer` → `kSelectShip`) follow.
19. **Network vocabulary rename** (game Network, same wire semantics): `kServerAssignPlayer` → `kServerAssignShip`, `kServerPlayerState` → `kServerShipState`, `kClientUpdatePlayerRequest` → `kClientUpdateShipRequest`, `PlayerEvents.h/.cpp` → `ShipEvents.h/.cpp` with `ShipStateWireType`, `ShipEventType`, `ReceivedShipEvent`, `ParseShipEvents`; `AssignPlayerMessage`/`PlayerStateMessage` → `AssignShipMessage`/`ShipStateMessage`; `ServerSession::SendAssignPlayer`/`SendPlayerState` → `SendAssignShip`/`SendShipState`; `mClientPlayers` → `mClientShips`; `ServerClientManager::DetectPlayerDeaths` → `DetectShipDeaths`; `ServerFleetManager::OnPlayerDeath`/`OnPlayerSpawned`/`OnPlayerTransferred`/`DetectDisconnectedPlayerDeaths` → `OnShipDeath`/`OnShipSpawned`/`OnShipTransferred`/`DetectDisconnectedShipDeaths`; `PendingUpdatePlayerRequest`/`mPendingUpdatePlayerRequests`/`ProcessUpdatePlayerRequests` → `…UpdateShip…`; `ClientSession::SendUpdatePlayerRequest`/`ApplyPlayerEvent`/`UpdatePlayerCoordinate` → `SendUpdateShipRequest`/`ApplyShipEvent`/`UpdateShipCoordinate`; `memberGlobalPlayerId` → `memberGlobalId`; `iPlayerUuid` → `iUuid`. Engine identifier names that are not ship concepts (`CameraBase`'s last-known-target members, `RawInputManager`, `Engine/Source/Ui/Screens/AGENTS.md` "Players see the audio menu") are not ship names and stay.
20. **Other renames.** Profile: `kCpuCounterPlayers` → `kCpuCounterOmnis`, `kCpuCounterSpaceships(Rendered)` → `kCpuCounterFighters(Rendered)`, add `kCpuCounterBattleships(Rendered)`; timers `…Spaceships` → `…Fighters` and add the Battleships rows; `kCpuTimerRenderPlayer` → `kCpuTimerRenderOmnis`; name tables follow. `ServerCellStatistics::iPlayers`/`iSpaceships` → `iFighters`, `iOmnis`, `iBattleships`; `PublishServerEntityCounts` and `ServerDisplay.cpp` print the three. UI wrappers: `gPlayer*` → `gOmni*`, `gSpaceship*`/`gEnemy*`/`gWindDepositSpaceships*` → `gFighter*`/`gWindDepositFighters*`; Battleships reuse the Fighter wrappers (placeholder, like the model); `gHexShield*` stay. `TerrainUtils.h` derives from `kfOmniRadius`. Engine comments and assert text naming the old types (`Engine/Source/Graphics/AnimationData.h:42`, `Engine/Source/Network/NetworkProtocol.h:99`, `Engine/Source/File/Replay.cpp:283`, `Engine/Source/Frame/Collections/HexShields/AGENTS.md:3`, `Engine/Source/Frame/Collections/Pushers/AGENTS.md:8`) are reworded; `Engine/Source/Graphics/Render/MainUniforms.cpp:557` calls all three `DebugRender`s. Explosion, hit-flash, and blaster type registrations keep today's values per type: Omnis register the Player ones, Fighters the Spaceship ones, Battleships their own copies scaled through the radius-derived formulas.
21. **Harness.** `query_players` is removed; `query_collection` accepts `fighters|omnis|battleships|missiles|blasters` and the three ship extractors emit the common fields `index`, `uuid`, `globalId`, `controller`, `orders`, `flags`, `alignment`, `local`, `dir`, `vel`, `arrivalGrace`, `flagshipGlobalId`, plus `armor` and `shield` for Omnis and `health` otherwise. `query_frame` counts carry `fighters`, `omnis`, `battleships`, `missiles`, `blasters`. `edit_frame` collections are `fighters`, `omnis`, `battleships`, with their column tokens. `inject_payload` types are `SpawnShip` (fields `eType`, `fSpawnOffsetX`, `fSpawnOffsetY`, `bIsFlagship`, `flagshipGlobalId`, `fleetWantedCoordinate`), `DestroyShip` (`eType`, `iUuid`), `UpdateShip` (`eType`, `iUuid`, `bUseMissiles`, `navigationDelaySeconds`), `UpdateFleet` (`eType`, `iUuid`, `fleetWantedCoordinate`, `bIsFlagship`, `flagshipGlobalId`), `TransferFighter`, `TransferOmni`, `TransferBattleship`, `TransferBlaster`, `TransferMissile`; `eType` is the enumerator name without `k` (`Fighter`, `Omni`, `Battleship`). `describe_scene` `unitTypes` are `fighter`, `omni`, `battleship`, `blaster`; flag names come from `ShipOrders` and `ShipFlags`. `replay_transfer_capture` counts gain `battleshipCount` and rename the other two. The docs under `Projects/BrokenEngineSandbox/Documents/AgentHarness/` follow.
22. **Versions and compatibility.** Bump `Frame::kiVersion`'s base (`Frame.cpp:38`) by one from where the prerequisites leave it; the three new collections start at `kiVersion = 1` and Blasters and Missiles bump theirs by one; bump `FrameInput::kiVersion` and `engine::kiProtocolVersion` by one each; grid saves are gated by `Frame::kiVersion` (`Engine/Source/File/GridSave.cpp:17`, `Projects/BrokenEngineSandbox/Source/Save/AGENTS.md`); its bump covers the fleet payload change. No backward compatibility: `StatusChangeType` is renumbered, and the comment at `StatusChange.h:9` and the `## Status-Change Wire Format` rule in `Network/AGENTS.md` change from "append-only" to "enumerator order is wire order; any change to the set or order is an incompatible wire change and takes the `engine::kiProtocolVersion` and `FrameInput::kiVersion` bumps". The `GamePacketType` "new values append" rule is untouched because this Plan renames packet types without reordering them.

## Design

### Shared vocabulary (`Frame/Collections/Ships/ShipTypes.h`)

Included by `Frame/StatusChange.h`, so it includes nothing beyond what the game PCH already provides ahead of `Engine.h`.

```cpp
namespace game
{

enum class ShipType : uint8_t { kFighter, kOmni, kBattleship, kCount };
inline constexpr bool IsKnownShipType(ShipType eType) { return eType < ShipType::kCount; }
const char* ShipTypeName(ShipType eType); // "Fighter", "Omni", "Battleship"

// CRC'd per-row column: which AI drives the row, and which of the type's two models the client draws.
enum class ShipController : uint8_t { kFleet, kRandomSpawn };

// Written only by controllers: the fleet path through kSpawnShip/kUpdateShip/kUpdateFleet, the director at spawn.
enum class ShipOrders : uint16_t
{
	kIsFlagship         = 0x0001,
	kEscortFlagship     = 0x0002,
	kHuntNearby         = 0x0004,
	kUseMissiles        = 0x0008,
	kPendingUseMissiles = 0x0010,
	// Bits 8-15 reserved for the target flight plane (later Plan); zero until then.
};
using ShipOrders_t = common::Flags<ShipOrders>;

// Written by the ship's own phases; consumed by Spawn, Transfer, and Destroy.
enum class ShipFlags : uint16_t
{
	kExploding            = 0x0001,
	kTransfer             = 0x0002,
	kFireBlaster          = 0x0004,
	kBlasterSpawnLeft     = 0x0008,
	kFireMissile          = 0x0010,
	kMissileSpawnLeft     = 0x0020,
	kFlee                 = 0x0040,
	kReturnToIslandCenter = 0x0080,
	kNavigationDirectionBit0 = 0x0100, kNavigationDirectionBit1 = 0x0200, kNavigationDirectionBit2 = 0x0400,
	kNavigationWaypointBit0  = 0x1000, kNavigationWaypointBit1  = 0x2000,
};
using ShipFlags_t = common::Flags<ShipFlags>;
int64_t GetNavigationDirection(ShipFlags_t flags);            // today's Players.h:163-185 on ShipFlags_t
void SetNavigationDirection(ShipFlags_t& rFlags, int64_t iDirection);
int64_t GetNavigationWaypointIndex(ShipFlags_t flags);
void SetNavigationWaypointIndex(ShipFlags_t& rFlags, int64_t iIndex);

// Row address across the three ship collections, valid for one frame and never across a tick or a transfer.
struct ShipLocator
{
	ShipType eType {};
	int64_t iRow = 0;
};

} // namespace game
```

### Per-type collections

Each of `Fighters.h`, `Omnis.h`, `Battleships.h` keeps today's shape (`Players.h`, `Spaceships.h`): `kiVersion`, `kName`/`kCrc`, `Register`, `GraphicsResources`, `AllocateAndCopy`, Interpolate `Update`, `BeginRender`/`Render`/`EndRender`/`DebugRender`, `LogDifferences`, `PostRead`, the PostRender phase hooks `Update`, `PreCollision`, `PostCollision`, `AreaDamage` (Fighters, Battleships), `Transfer`, `Destroy`, `Spawn(Frame&, const CellStaticData&)`, and the full-state `SpawnInfo` plus `Spawn(Frame&, const SpawnInfo&)` that transfers use. Private per-row helpers move to `ShipBehaviours.h` as decision 6 lists; what remains private is type-specific weapon and effect spawning. Column lists are decision 2; `SharedMembers`, `ClientMembers`, `Members`, `PersistentMembers`, and `SharedCrcMembers` keep today's partition rules (`Engine/Source/Frame/Collections/AGENTS.md`), with `pGlobalIds` and `pClientGuids` persisted and CRC-excluded, `pRegistryIds` and `pPushers` persisted, and `pController`, `pOrders`, `pFlags`, `pFlagshipGlobalIds` CRC'd.

### Shared behaviours (`Frame/Collections/Ships/ShipBehaviours.h/.cpp`)

```cpp
void CountdownPendingOrders(int64_t& riPendingFleetWantedCoordinateTicks); // Fighters, Battleships
void CountdownPendingOrders(ShipOrders_t& rOrders, int64_t& riPendingFleetWantedCoordinateTicks, int64_t& riPendingWeaponModeTicks); // Omnis: adds the weapon-mode countdown
void XM_CALLCONV ComputeNavigation(Frame& rFrame, const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, ShipLocator self, FXMVECTOR vecPosition, FXMVECTOR vecCellCenter, engine::GridCoord fleetWantedCoordinate, int64_t iPendingFleetWantedCoordinateTicks, ShipOrders_t orders, engine::GlobalId flagshipGlobalId, float fDeltaTime, int64_t& riNavigationDirection, int64_t& riNavigationWaypointIndex, XMVECTOR& rVecAiDirection, XMVECTOR& rVecIslandDestination, float& rfCellChangeTimer);
[[nodiscard]] XMVECTOR XM_CALLCONV ComputeFleeOrReturnDestination(std::span<const XMFLOAT4> islandCandidates, FXMVECTOR vecPosition, bool bEnemyFound, FXMVECTOR vecNearestEnemy, ShipFlags_t& rFlags);
void XM_CALLCONV SteerHeadingTowards(FXMVECTOR vecPosition, FXMVECTOR vecDirection, FXMVECTOR vecDestination, float fTurnRate, float fSmoothing, float fDecay, float fDeltaTime, float& rfDeltaRotation);
void XM_CALLCONV ApplyAcceleratedMovement(int64_t iNavigationDirection, FXMVECTOR vecAiDirection, float fDeltaTime, float fAccelerationMultiplier, float fDecayMultiplier, const AcceleratedMovementConstants& rConstants, XMVECTOR& rVecVelocity);
void XM_CALLCONV ApplyHeadingMovement(FXMVECTOR vecDirection, ShipFlags_t flags, float fDeltaTime, const HeadingMovementConstants& rConstants, XMVECTOR& rVecVelocity);
void XM_CALLCONV ApplyTerrainPush(const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, float fRadius, XMVECTOR& rVecVelocity);
void XM_CALLCONV ResolveTerrainContact(/* today's PlayersPostRender::ResolveTerrainContact signature, plus fRadius */);
void XM_CALLCONV ApplyTerrainBounce(const engine::CellStaticData& rStaticData, int64_t i, FXMVECTOR vecPreviousPosition, XMVECTOR& rVecPosition, FXMVECTOR vecDirection, float fRadius, float fDeltaTime, float& rfDeltaRotation, XMVECTOR& rVecVelocity); // today's SpaceshipsPostRender::ApplyTerrainBounce body (SpaceshipsNavigation.cpp:138-157, 90fa6c85) on one row: i for the log, vecPreviousPosition the sweep start, rVecPosition the current Interpolate position, vecDirection the current heading
void XM_CALLCONV ApplyPusherPush(const Frame& rFrame, FXMVECTOR vecPosition, engine::pusher_t pusher, float fMaximumPushVelocity, XMVECTOR& rVecVelocity);
void XM_CALLCONV AcquireTarget(const Frame& rPreviousFrame, FXMVECTOR vecPosition, engine::AlignmentIdentifier alignment, float fBlasterSpeed, ShipFlags_t& rFlags, bool& rbLookTargetFound, XMVECTOR& rVecLookPosition);
void RegenerateShield(std::chrono::duration<float> deltaTime, std::chrono::duration<float> shieldCooldown, float& rfShield);
void XM_CALLCONV RegenerateHealth(FXMVECTOR vecPosition, bool bEnemyFound, FXMVECTOR vecNearestEnemy, ShipFlags_t flags, std::chrono::duration<float> deltaTime, float fMaximumHealth, float& rfHealth);
bool IsNavigationDelayInRange(float fDelay);
```

`AcceleratedMovementConstants` and `HeadingMovementConstants` are small aggregates of the per-type tuning constants (acceleration, catch-up acceleration, drag, maximum speeds; or acceleration per steering state, drag, maximum speed), declared in `ShipBehaviours.h` and instantiated as `inline constexpr` values in each type header, so a behaviour reads its numbers from its caller and never names a type.

Representative script (`FightersPostRender::Update`, per row, after the loads from the previous row):

```cpp
CountdownPendingOrders(iPendingFleetWantedCoordinateTicks);
if (!(flags & ShipFlags::kExploding)) [[likely]]
{
	if (fArrivalGracePeriod > 0.0f)
	{
		ApplyPusherPush(rFrame, vecPosition, rPreviousInterpolate.pPushers[i], kfFighterMaxPusherPushVelocity, vecVelocity);
	}
	else if (rPrevious.pController[i] == ShipController::kFleet)
	{
		ComputeNavigation(rFrame, rPreviousFrame, rStaticData, {ShipType::kFighter, i}, vecPosition, vecCellCenter, fleetWantedCoordinate, iPendingFleetWantedCoordinateTicks, orders, rPrevious.pFlagshipGlobalIds[i], fDeltaTime, iNavigationDirection, iNavigationWaypointIndex, vecAiDirection, vecIslandDestination, fCellChangeTimer);
		SteerHeadingTowards(vecPosition, vecDirection, XMVectorAdd(vecPosition, vecAiDirection), kfFighterChaseTurnRate, kfFighterSteeringSmoothing, kfFighterSteeringDecay, fDeltaTime, fDeltaRotation);
		ApplyHeadingMovement(vecDirection, flags, fDeltaTime, kFighterMovement, vecVelocity);
	}
	else
	{
		XMVECTOR vecNearestEnemy = XMVectorZero();
		bool bEnemyFound = FindNearestEnemyShip(rPreviousFrame, vecPosition, alignment, vecNearestEnemy);
		RegenerateHealth(vecPosition, bEnemyFound, vecNearestEnemy, flags, deltaTime, kfFighterHealth, fHealth);
		XMVECTOR vecDestination = ComputeFleeOrReturnDestination(islandCandidates, vecPosition, bEnemyFound, vecNearestEnemy, flags);
		SteerHeadingTowards(vecPosition, vecDirection, vecDestination, (flags & ShipFlags::kFlee) ? kfFighterFleeTurnRate : kfFighterChaseTurnRate, kfFighterSteeringSmoothing, kfFighterSteeringDecay, fDeltaTime, fDeltaRotation);
		ApplyHeadingMovement(vecDirection, flags, fDeltaTime, kFighterMovement, vecVelocity);
	}
}
else
{
	vecVelocity = XMVectorScale(XMVectorNegate(rPrevious.pVecDamageDirections[i]), kfDeathKnockbackSpeed);
}
ApplyTerrainBounce(...);
```

The Omni script is today's `PlayersPostRender::Update` body (`Players.cpp:656-779`) with `ComputeNavigation` under `kFleet` and `ComputeFleeOrReturnDestination` feeding `vecAiDirection` under `kRandomSpawn`; `AcquireTarget` and `UpdateFacing` run under both controllers.

### Cross-collection queries (`Frame/Collections/Ships/ShipQueries.h/.cpp`)

```cpp
// Visits (ShipType, Interpolate&, PostRender&) in the fixed order Fighters, Omnis, Battleships. The one three-way switch.
template <typename FRAME, typename VISITOR> void ForEachShipType(FRAME& rFrame, VISITOR&& rVisitor);

[[nodiscard]] std::optional<ShipLocator> FindShipByGlobalId(const Frame& rFrame, engine::GlobalId globalId);
[[nodiscard]] bool XM_CALLCONV FindNearestEnemyShip(const Frame& rFrame, FXMVECTOR vecFrom, engine::AlignmentIdentifier alignment, XMVECTOR& rVecResult); // alive, past arrival grace, enemy alignment
[[nodiscard]] int64_t OwnedShipCount(const Frame& rFrame); // rows with pController == kFleet
struct ShipUuid { ShipType eType {}; int64_t iUuid = 0; };          // iUuid 0 when not found
[[nodiscard]] ShipUuid ShipUuidByGlobalId(const Frame& rFrame, engine::GlobalId globalId);
[[nodiscard]] XMVECTOR ShipPosition(const Frame& rFrame, ShipLocator locator);
[[nodiscard]] float ShipHull(const Frame& rFrame, ShipLocator locator); // armor for Omnis, health otherwise
[[nodiscard]] RegistryWindow BuildShipRegistryWindow(const Frame& rFrame, const Frame* pPreviousFrame, const MissilesPostRender& rSubscribers); // three source layers, Fighters, Omnis, Battleships
```

`BuildShipRegistryWindow` keeps today's single reservation and eligibility rule per layer (`Frame.cpp:473-521`); `pPreviousFrame` is non-null for the Missile-Update window and null for the Omni-Spawn window, replacing the two raw pointers.

### Status changes and spawning (`Frame/Collections/Ships/ShipStatusChanges.h/.cpp`, `Frame/StatusChange.h`)

```cpp
struct ShipSpawnInfo // genuine spawns only; transfers call each type's own full-state Spawn(SpawnInfo) through SpawnTransfer
{
	ShipType eType {};
	ShipController eController {};
	XMVECTOR vecPosition, vecDirection, vecVelocity;
	engine::AlignmentIdentifier alignment {};
	ShipOrders_t orders {};
	engine::GlobalId globalId {};
	engine::ClientGuid clientGuid {};
	engine::GlobalId flagshipGlobalId {};
	engine::GridCoord fleetWantedCoordinate {};
	int64_t iPendingFleetWantedCoordinateTicks = 0;
	float fArrivalGracePeriod = 0.0f;
};
bool SpawnShip(Frame& rFrame, const ShipSpawnInfo& rInfo);                                   // switches on eType
void ApplyUpdateStatusChanges(Frame& rFrame, const FrameInput& rFrameInput);                 // kUpdateShip, kUpdateFleet
void ApplySpawnStatusChanges(Frame& rFrame, const FrameInput& rFrameInput);                  // kDestroyShip, kSpawnShip
[[nodiscard]] bool IsOwnedTransfer(StatusChangeType eType, const TransferData& rData);       // ship transfer type and eController == kFleet
```

`kUpdateShip` and `kUpdateFleet` resolve `eType` to the collection and `iUuid` through its `idToIndexMap`, then write `pOrders` (`kUseMissiles`/`kPendingUseMissiles`, `kIsFlagship`), `puiPendingWeaponModeTicks` (Omnis), `pFlagshipGlobalIds`, `pFleetWantedCoordinates`, `puiPendingFleetWantedCoordinateTicks`, and `pfNavigationDelays`, exactly as `Players.cpp:285-345` does today. `kUpdateFleet` with `bIsFlagship` false also sets `kEscortFlagship`. `SpawnTransfer` (`Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp`) gains the `kTransferFighter`/`kTransferOmni`/`kTransferBattleship` arms and `IsAdoptableStatusChange` also rejects a ship payload whose `eType` or `eController` is out of range; the replay reader keeps rejecting `kSpawnShip` with global ID 0.

### Integration sites

- `Frame/Frame.h/.cpp`: collection members `pFighters`, `pOmnis`, `pBattleships`; `fleetAlignment`/`randomAlignment`; `OwnershipLayer(const Frame&, ShipType)`; `kiVersion`; the two status-change calls; `SpawnRandomShipGroup`; profile counter publication; `BuildShipRegistryWindow` moves to `ShipQueries.cpp`; `Values()` renames.
- `Frame/FrameCollections.h`: tuples in decision 10's order.
- `Frame/HealthDamage.h`: decision 16 masks, `kfOmniArmor`/`kfOmniShield`/`kfOmniShieldRegeneration`, `kfFighterHealth`, `kfBattleshipHealth`, `kfShipCollisionDamage`.
- `Frame/Collections/Missiles/`: `kTargetFleet`/`kTargetRandom`, the Missile-Update window call, collision mask.
- `Frame/Collections/Blasters/`: collision mask.
- `Projects/BrokenEngineSandbox/Source/SpawnTransfer.h/.cpp`, `Game.h/.cpp`, `Fleet.h`, `FleetSelection`.
- `Network/`: `StatusChange.h` through `GameMessages.h`, `GamePacketType.h`, `ShipEvents.h/.cpp`, `NetworkSessionContract.h`, `Client/ClientSession`, `Client/ClientReconciler.cpp`, `Client/ReconcileReplayClientState.cpp`, `Server/ServerSession`, `Server/ServerClientManager`, `Server/ServerFleetManager`, `Server/FleetNavigationController.cpp`, `Server/ServerFleetSerialization.cpp`; `Network/AGENTS.md` wire rule; `Documents/Architecture/Network.md` contract rows and protocol version; `Documents/Architecture/GameReconciliation.md` event names.
- Engine: `Engine/Source/Network/NetworkSerialization.cpp` (per-type serializers: `SerializeOmniTransfer`, one `SerializeHeadingShipTransfer` for Fighters and Battleships, Blaster and Missile unchanged plus the group dispatch), `Engine/Source/Network/Server/ServerTransferManager.cpp`, `ServerBroadcaster.cpp`, `Engine/Source/Network/Client/ReconcileReplayTick.cpp`, `Engine/Source/Server/ServerDisplay.cpp`, `Engine/Source/Graphics/Render/MainUniforms.cpp`, `Engine/Source/Network/NetworkProtocol.h` (`kiProtocolVersion` and the assert text), the comment sites in decision 20.
- `Agent/`: `AgentCommandsServer.cpp`, `AgentCommandsServerQueries.h/.cpp`, `AgentScene.cpp`, `Commands/ServerFrameEdit.cpp`, `Commands/ServerSimulationHarnessRigs.cpp`, `Agent/AGENTS.md`, and `Projects/BrokenEngineSandbox/Documents/AgentHarness/*.md`.
- `Ui/`: `Screens/HudScreen.cpp`, `LightingWrappers`, `SoundWrappers`, `WindDepositsWrappers`, the `TweaksScreen` files naming the wrappers.
- `Profile/ProfileManager.h`, `Frame/ServerCellStats.h/.cpp`, `Frame/TerrainUtils.h`.
- Project membership and filters for the renamed and new files: `/update-vcxproj`.
- `AGENTS.md`: `Frame/AGENTS.md` (explicit dispatch invariant, registry windows, ownership layer, counters), `Frame/Collections/AGENTS.md` and the four per-collection `AGENTS.md` (three ship folders plus `Ships/AGENTS.md` for the shared headers), `Network/AGENTS.md`, `Network/Server/AGENTS.md`, `Agent/AGENTS.md`, `Graphics/AGENTS.md`, `Projects/BrokenEngineSandbox/Source/AGENTS.md`, `Engine/Source/Network/Server/AGENTS.md`, `Engine/Source/Frame/AGENTS.md`, `Engine/Source/Frame/Collections/HexShields/AGENTS.md`, `Engine/Source/Frame/Collections/Pushers/AGENTS.md`, `Documents/Architecture/FrameUpdatePipeline.md`.

### Stages

One Plan, one landing; the stages order the work so each builds and runs.

1. **Vocabulary and the two renames.** Add `ShipTypes.h`, `ShipQueries`, `ShipStatusChanges`; rename Players to Omnis and Spaceships to Fighters (files, types, constants, wrappers, counters, network vocabulary, harness, docs); split `PlayerFlags` into `ShipOrders`/`ShipFlags`; add the common columns to Fighters; the `StatusChangeType` and `TransferData` changes, codec, versions, engine seams, collision categories (two ship types), tuples. The director spawns `kRandomSpawn` Fighters and fleets spawn `kFleet` Omnis, so behaviour equals today's apart from decision 8 and decision 16's symmetric collision damage.
2. **Behaviours and controllers.** Extract `ShipBehaviours`; write the per-controller scripts for Fighters and Omnis; per-controller models; typed spawn request and HUD buttons; `FollowFlagship` by global ID.
3. **Battleships.** Copy Fighters, apply decision 3, register the third type everywhere the integration list names a per-type site, add `kTransferBattleship`, the harness names, counters, and docs.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipTypes.h`, `ShipBehaviours.h/.cpp`, `ShipQueries.h/.cpp`, `ShipStatusChanges.h/.cpp` (new)
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/`, `Omnis/`, `Battleships/` (renamed from `Spaceships/`, `Players/`; copied)
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.h`, `Frame.cpp`, `FrameCollections.h`, `StatusChange.h`, `FrameInput.cpp`, `HealthDamage.h`
- `Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp`, `Game.h`, `Game.cpp`, `Fleet.h`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp`, `ServerFleetManager.cpp`, `FleetNavigationController.cpp`, `ServerSession.cpp`
- `Engine/Source/Network/NetworkSerialization.cpp`, `Engine/Source/Network/Server/ServerTransferManager.cpp`, `ServerBroadcaster.cpp`

## In scope

- Everything the Decisions and the Integration sites list name: the three collections, the four shared `Ships/` headers and sources, the status-change set and payloads, `TransferData`, the codec, the spawn paths, the fleet member type, the typed spawn request and HUD buttons, the engine seams, the collision categories and symmetric ship collision damage, the per-controller models with placeholder assets, the alignment and network vocabulary renames, the harness commands and their documents, profile counters and cell statistics, UI wrapper names, the version bumps, and the `AGENTS.md` and `Documents/Architecture` lines `/update-claude-docs` finds stale, including the wire-rule rewording of decision 22.
- The decision 8 change of Fighters' `AvoidTerrain` and steering reads to the previous frame.
- `PlayersPostRender::ResolveTerrainContact` (`Players.h:245`, `PlayersNavigation.cpp:478-493`, both `90fa6c85`): it moves to `Frame/Collections/Ships/ShipBehaviours.h/.cpp` as the shared per-row `ResolveTerrainContact`, called by the Omni script; it and the shared `ApplyTerrainBounce` keep their `engine::ResolveDiscAgainstTerrain` calls, which this Plan does not change.

## Out of scope

- Flight-plane columns, climbing, cross-plane navigation and weapons, smoke, and lighting per plane (later Plans; only the reserved `ShipOrders` bits leave room). The plane access list (Battleships low; Omnis low and medium; Fighters all) is awareness only.
- New 3D model assets; the placeholder models in decision 17 stand until a later Plan.
- Same-type collision (`Documents/Investigations/Engine/SameCollectionCollision.md`); Pushers remain the only same-type separation.
- Any fleet behaviour beyond today's (per-type escort versus hunt choices, formation shapes, fleet composition rules); the director's mix stays Fighters only.
- Changes to Blaster or Missile flight, damage values, or the explosion and hit effects beyond renaming and the Battleship copies.
- The engine `RegistryOwnershipLayer`, `OwnedEntityRegistry`, collision, and registry mechanics.
- Backward compatibility of saves, replays, or the wire; `ClientState.bin` and `TweaksSettings.bin` layouts (unchanged).
- Engine identifiers that are not ship concepts (decision 19).

## Risk triggers and invariants

- CRC'd PostRender and Interpolate state changes everywhere ships are: every bump in decision 22 rejects older saves and replays, and client and server must build from the same change.
- Bit determinism under `/fp:strict` (`Documents/FloatingPointDeterminism.txt`): the extracted behaviours keep today's operation order; the per-type scripts call them in the order the Decisions give; cross-type reads come from the previous frame (decision 8); registry acquisitions keep their order and are never hoisted or cached (`Frame/AGENTS.md`); random draws per row per mode are unchanged (`Frame/Collections/Omnis/AGENTS.md`, today's `Players/AGENTS.md:11-12`).
- Tuple order is deterministic and fixed (decision 10); the three `ShipType` enumerators, the tuple, `ForEachShipType`, and the registry layers agree on the order.
- `SharedCrcMembers()` ⊆ `SharedMembers()` per collection; `pGlobalIds` and `pClientGuids` stay out of the CRC; the engine ownership layer binds CRC-excluded GUID columns only (`Engine/Source/Frame/AGENTS.md` `## Frame Registry`).
- `Read()`/`ServerRead()` require Interpolate/PostRender count parity for all three pairs (`Frame/AGENTS.md`).
- Transfer arrivals restore carried state verbatim except arrival grace, navigation state, Fighter and Battleship previous-frame reaction flags, and client-only visuals (`Frame/Collections/AGENTS.md`); the explicit transfer marker is each type's `SpawnInfo::bTransfer`.
- Spawn and transfer sites keep `common::ValidateVector` and the refuse-outside-the-cell rule.
- No main-loop allocation beyond today's: the registry window stays one reservation; the three extractors and the HUD buttons add none.
- The server validates every client record: the spawn request's type byte and every ship payload's `eType`/`eController` are range-checked under the bad-value rule; the client trusts server data.
- Engine code names game ship types only inside the documented exception sites of decision 14; no engine shared/public member names a game-only concept elsewhere (`Engine/Source/AGENTS.md` `## Hub Conventions`).
- The whole-file build affinity of every renamed and new file matches project membership; `/update-vcxproj` validates.
- The user's "no backward compatibility" decision overrides `Network/AGENTS.md`'s append-only wire rule: record as a residual that the rule text is rewritten under that authority (decision 22).

## Acceptance

| Criterion | Expected observation | Method |
|---|---|---|
| Client and server builds | Both succeed after each stage | `/compile` BrokenEngineSandbox client and server |
| No `Player`/`Spaceship` ship names remain | A repository search for `Players`, `Spaceships`, `PlayerFlags`, `SpaceshipFlags`, `kSpawnPlayer`, `kTransferPlayer`, `kTransferSpaceship`, `globalPlayerId`, `ClientPlayer`, `playerAlignment`, `enemyAlignment`, `query_players` under `Projects/`, `Engine/`, `Documents/Architecture/`, and the harness documents finds only the decision 19 engine exceptions | Search |
| Fleets spawn each type | After `[+F]`, `[+O]`, `[+B]`, `query_collection` for `fighters`, `omnis`, `battleships` at the origin each shows one row with `controller` `kFleet`, a nonzero `globalId`, and `orders` containing `kIsFlagship` for exactly one member of the fleet | `/agent-harness`: launch, UI clicks, `query_collection`, `describe_scene` |
| Mixed-type fleet follows its own flagship | With a Battleship flagship and two Fighter wingmen, after 10 s the wingmen's `local` positions lie within the follow distance of the flagship's, and a second fleet in the same cell is not followed (wingmen `flagshipGlobalId` equals their own fleet's flagship `globalId`) | `/agent-harness`: `query_collection` samples over time |
| Random spawns fight fleets | Within 30 s of a fleet spawn, `query_collection` `fighters` shows rows with `controller` `kRandomSpawn`, Blasters exist, and at least one fleet ship's `armor`/`health` or one random ship's `health` has dropped | `/agent-harness`: `query_collection`, `query_frame` |
| Cross-cell transfer of each type | Driving one ship of each type across a cell edge (`UpdateFleet` with a neighbouring `fleetWantedCoordinate`) logs one `TransferFighter`, `TransferOmni`, and `TransferBattleship` arrival each in `replay_transfer_capture`/`get_logs`, and the ship reappears in the destination with the same `globalId` and `controller` | `/agent-harness`: `inject_payload`, `query_collection` in both cells (`cross-cell.md`) |
| Save and load | Quicksave then quickload restores the three collections' counts, every `globalId`, `controller`, and `flagshipGlobalId`, and the fleet member types in the HUD | `/agent-harness`: save/load requests, `query_collection`, HUD snapshot |
| Repeated-run replay determinism | No `LogDifferences CRC` mismatch across repeated playback loops of a recording containing all three types, a transfer of each, and random spawns | `/agent-harness` replay (`replay.md`) |
| Status-change and transfer rejection | A `SpawnShip` with global ID 0, an `eType` at or above `kCount`, or a spawn request type byte at or above `kCount` is rejected: replay read throws, `inject_payload` reports the error, the server counts one contract violation | `/agent-harness` `inject_payload`, `game_packet_fault_harness_rig` or code reading for the packet path |
| Collision masks | Each ship type collides with the other two and never with itself; the engine assert passes at startup; ramming between enemy types reduces both ships' hull | Code reading plus the random-spawn fight run above |
| Per-controller models | A fleet Fighter and a random Fighter in one cell render through different pipelines (screenshot shows both placeholder models) | `/agent-harness` screenshot |
