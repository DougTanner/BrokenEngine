<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T21:46:43.784Z","dependsOn":[]} -->
# Single Flight Height for Ships

## Context

BrokenEngineSandbox (BES) ships fly on a table of flight planes:
`game::kFlightPlanes`
(`Projects/BrokenEngineSandbox/Source/Frame/FlightPlaneTable.h:7-11`) holds
plane 0 at 3 m with `engine::FlightPlaneFlags::kTerrainCollision` and plane 1
at 70 m without it. Every ship stores the plane it is on
(`puiFlightPlanes`, `kuiNoFlightPlane` while climbing) and a target plane in
bits 8-15 of its `ShipOrders`; Fighters default to the 70 m plane, climb and
descend between planes, target and fire only at enemies on their own plane,
and navigate on per-(clearance tier, plane) navigation graphs, the 70 m
plane's graphs being empty.

The user asked for the multi-layer concept to be removed entirely, delivered
as a series of Plans. This Plan is the gameplay stage: after it every ship
flies one height and nothing in BES gameplay reads a plane index. The
engine's flight-plane table, world lighting layers, smoke planes, and pusher
layers are later stages. The 70 m table entry stays in this stage only
because the client still requires exactly two planes
(`static_assert` at
`Engine/Source/Graphics/Managers/RenderTargetTexturesLighting.cpp:14`,
`fLightingLayerHeightInverse` at
`Engine/Source/Graphics/Render/GlobalUniforms.cpp:632`, and the object-shadow
height range at `GlobalUniforms.cpp:463-464`); no gameplay code reads it
after this change.

Decisions the user made (binding). Quoted text is the user's own words,
except the option text in decision 1, which the author worded and the user
selected by number:

1. Fighters always navigate to their targets. The user asked that "fighters
   will need to navigate to their targets always" and "No more
   no-navigation-layer", then selected the option worded: "Fighters fly
   straight at a visible target and route around islands to a hidden one.
   Escorting fighters still head straight for their slot beside the flagship,
   and turn or bounce if an island is in the way."
2. A live ship meeting an island: "Option 2 for fighters, have them pushed up
   like they currently do, have omnis and battleships be pushed by terrain
   (bounce if actually hit it)". The user's reason for the Fighter rise:

   > "fighters rise over islands" is a safety for if they mess up navigation,
   > they fly pretty fast
3. The single height: "Do 10m to start, I will tweak later". BES uses 10 m
   for gameplay, navigation contours, and graphics.

## Design

### One height

Change BES plane 0 in `FlightPlaneTable.h` from 3.0 to 10.0 m (decision 3).
Plane 1 stays at 70 m without `kTerrainCollision`, for client graphics only.
Every BES base-height read already goes through
`engine::gpFlightPlanes->Lowest()`, so terrain, water, lighting, smoke, the
camera, and the navigation contour bake all follow the new height without
further edits; those reads stay until the later stage that removes the table.

ModelViewer keeps its own table at 2 m
(`Projects/ModelViewer/Source/Frame/FlightPlaneTable.h`). The user did not
mention ModelViewer, and each project supplies its own table header, so the
code forces no shared value; the author's recommendation is to leave it
unchanged.

`EffectiveShipFlightHeight(eType, eController)` drops its plane argument and
returns the larger of the base height and the controller model's
`fMinimumOriginZ`, as today for plane 0.

### Heights per ship type

- Omnis and Battleships never change height after spawn: their Interpolate
  Updates lose `ClimbTowardsFlightPlane`, and Omnis lose
  `RaiseAboveTerrain`.
- Fighters keep today's rise (decision 2). Interpolate Update, for every row
  as today, steps Z toward `EffectiveShipFlightHeight` by at most
  `kfFighterClimbRate` times the delta with `StepHeightTowards`, then, for a
  row that is not dying, applies `RaiseAboveTerrain`
  (`ShipBehaviours.cpp:139-148`: terrain under the center plus the base
  height). PostRender Update keeps its `RaiseAboveTerrain` after the pusher
  correction (`Fighters.cpp:685-688`). A Fighter over an island therefore
  follows the terrain up and drifts back down at the climb rate once the
  terrain falls away. Today's descent hold — a descent blocked by the lower
  plane's graph climbing toward the plane above
  (`ShipBehaviours.cpp:125-129`) — goes with the upper plane.
- The client-only `pfClimbTilts` stays and keeps its rule: the tilt follows
  the descent step taken before the terrain rise, so terrain displacement
  adds no tilt.

### Terrain response per type (decision 2 mapped to existing mechanisms)

| Row | Response after this change | Existing mechanism |
|-----|---------------------------|--------------------|
| Live Fighter | Rises over terrain, no horizontal contact, terrain avoidance steering | `RaiseAboveTerrain` (`ShipBehaviours.cpp:139-148`), called at `Fighters.cpp:283-286` and `:685-688`; `AvoidTerrain<kFighter>` (`ShipBehaviours.cpp:455-459`) |
| Dying Fighter | Bounces at its height | `ApplyTerrainBounce` (`ShipBehaviours.cpp:545-565`), `Fighters.cpp:679-683`, now ungated |
| Live Omni | Soft push away from terrain, bounce on contact; no rise | `ApplyTerrainPush` (`ShipBehaviours.cpp:504-516`) and `ApplyTerrainBounce` |
| Dying Omni | Soft push and slide, as today | `ApplyTerrainPush` and `ResolveTerrainContact` (`ShipBehaviours.cpp:527-543`), now ungated |
| Live Battleship | Steers away and bounces on contact, as today | `AvoidTerrain<kBattleship>` (`ShipBehaviours.cpp:461-465`, `ComputeTerrainAvoidance` at `TerrainUtils.cpp:87-136`) and `ApplyTerrainBounce` (`Battleships.cpp:506-510`), now ungated |
| Dying Battleship | Holds still, no contact, as today | none |

Battleships already behave as decision 2 describes, so only the
`SkipsTerrainContact` gate goes. For Omnis the push mechanism is
`ApplyTerrainPush`, the soft push dying Omnis use today
(`OmnisUpdate.cpp:400-403`); Omnis have no turn rate, so `AvoidTerrain` does
not apply to them. "Bounce if actually hit it" maps to `ApplyTerrainBounce`,
the contact response Fighters and Battleships use: it reflects the velocity
off the contact normal and adds the normal kick. Its turn-rate output has no
Omni column, so the Omni call passes a local the caller discards (author's
recommendation: the smallest change that gives Omnis the bounce the user
named rather than today's dying-Omni slide). Live Omnis today take no
horizontal contact at all (`OmnisUpdate.cpp:398` gates it to dying rows) and
rise over terrain instead (`OmnisUpdate.cpp:60-63`, `:411-414`).

New Omni PostRender Update order for the terrain block
(`OmnisUpdate.cpp:396-416`): regenerate the shield; `ApplyTerrainPush` for
every row; `ApplyPusherPush`; then `ResolveTerrainContact` for a dying row or
`ApplyTerrainBounce` (hull direction `pVecDirections[i]`) for a live one; the
pusher X correction; no terrain rise; velocity Z set to 0 last.

### Navigation and line of sight

- One navigation graph per clearance tier, and the graph index is the
  clearance tier itself: `CellStaticData::navigationData` index i is tier i's
  graph. Every tier is used — Omnis and `kiFiringClearanceTier` on tier 0,
  Fighters on tier 1, Battleships on tier 2 (`kShipTypeClearanceTiers`,
  `TerrainUtils.h:58-70`, from `kfOmniRadius` 1.1, `kfFighterRadius` 1.5, and
  `kfBattleshipRadius` 4.5) — so no tier lacks a graph. `NavigationGraph`,
  `ClearanceTierFlightPlaneCount`, `kiNavigationGraphCount`,
  `kNavigationGraphs`, its plane `static_assert`, and `NavigationGraphIndex`
  are deleted. Every graph is baked on the base height.
- `NavigationPlaneData` becomes `NavigationTierData(rStaticData,
  iClearanceTier)`: `navigationData` at index `iClearanceTier`, or the empty
  graph in a client cell whose static data has not arrived (an empty
  `navigationData`); that is its only guard.
  `NavigationFlightPlane`, `TerrainContactNavData`, and every plane argument
  of `NextRouteWaypoint`, `FleeRouteWaypoint`, `ComputeReturnDestination`,
  `ResolveTerrainContact`, `ApplyTerrainBounce`, `ComputeAiSteering`, and
  `ComputeTerrainAvoidance` go. `ComputeAiSteering` and
  `ComputeTerrainAvoidance` always read terrain (`HasTerrainCollision` and
  `FlightPlaneElevationOffset` go; the offset is 0 on the base height, so the
  tuning constants keep their meaning).
- `HasLineOfSight` and `HasShipLineOfSight` drop the plane argument; the
  Battleship box walk drops its occupied-plane filter.
- `NavRectangleBoard` drops `BoardRectangle::iLowestPlane`/`iHighestPlane`,
  `CappedOccupiedPlanes`, and the plane filter in `Select`.

How decisions 1 and 2 hold together: today a Fighter on the empty 70 m graph
has line of sight to everything and flies straight at every target. After
this change every Fighter, raised or not, tests sight on its tier's graph at
the base height, with both ends snapped clear of terrain as today
(`ShipBehaviours.cpp:583-587`). A target in sight gets `AttackTactic`
(straight, its path crossing no navigation polygon); a hidden one gets
`RouteTactic`/`NextRouteWaypoint` round the islands, the same path rule
Battleships and plane-0 Omnis use today; escort keeps flying straight at its
slot (`FleetCoordination.cpp:273-283`). A Fighter that does pass over terrain
— escorting across an island, or clipping a corner on a route — is pushed up
by `RaiseAboveTerrain`, the safety decision 2 records, and a Fighter raised
over an island snaps its position out of the polygon for its sight and route
queries exactly as a raised plane-0 Fighter does today. The two decisions
therefore do not conflict, and `AvoidTerrain<kFighter>`, previously inactive
on the 70 m plane, now also steers Fighters off terrain ahead.

### Targeting and weapons

- No plane filter remains in target choice: `FindAttackTarget` drops
  `uiFlightPlane`, `iHighestFlightPlane`, and the kept-target-above-highest
  clearing; `AcquireTarget` drops `uiFlightPlane`, `iHighestFlightPlane`, and
  the same-plane condition on its firing candidate; `FindSquadronTarget`
  drops `iHighestFlightPlane`; the random-spawn Omni flagship chase drops its
  `NearestFlightPlane(vecFlagship) <= kiOmniHighestFlightPlane` term.
- The Fighter beam drops the stored-plane gate (`FightersCombat.cpp:215-219`)
  and the nearest-plane match in `FireBeam`; the Battleship launch drops the
  stored-plane and exact-effective-height gates
  (`BattleshipsCombat.cpp:182-194`) and the enemy/tube nearest-plane match.
  A Fighter raised over terrain may therefore fire; range and line of sight
  still gate it (author's recommendation: the gate existed only because a
  climbing ship had no plane to match against).
- `FindLowestPlaneShipByRegistryId` becomes `FindShipByRegistryId` without the
  nearest-plane test; missiles keep a homing target whatever its height.
- `ShipTactic::iTargetFlightPlane`, `HighestFlightPlane`,
  `DefaultFlightPlane`, `NearestFlightPlane`, `IsFlightPlaneAllowed`,
  `SkipsTerrainContact`, `TakesDeathTerrainContact`, `FlightPlaneAtHeight`,
  `ClimbTowardsFlightPlane`, `GetTargetFlightPlane`, `SetTargetFlightPlane`,
  `kuiNoFlightPlane`, `kShipTypeHighestFlightPlanes`, and the per-type
  `ki<Type>HighestFlightPlane` constants are deleted with their callers.
  `ShipOrders` keeps its `uint16_t` width; bits 8-15 become unused.

### Data, wire, spawn, and agent surfaces

- `puiFlightPlanes` leaves the Fighters, Omnis, and Battleships Interpolate
  (`SharedMembers()` and every other tuple naming it, `LogDifferences`,
  `PostRead`), and `SpawnInfo::uiFlightPlane` leaves each `SpawnInfo` and its
  `Fields()` list. `UpdateShipData::uiTargetFlightPlane` and
  `kuiKeepTargetFlightPlane` go; `kUpdateShip` keeps its navigation delay.
- Interpolate `PostRead` for all three types requires a finite height at or
  above the base height (Battleships lose their upper bound at the highest
  plane); the PostRender target-plane check goes.
  `IsAdoptableStatusChange` loses its plane tests.
- The random wave (`SpawnRandomShipWave`) spawns every type at the base
  height and always runs the terrain clearance test. Fleet spawn, fleet rows,
  and the Fighter bay launch drop plane fields; the bay terrain admission
  calls `ComputeTerrainAvoidance` without a plane.
- Agent: `query_collection` ship rows drop `targetFlightPlane`; the
  `edit_frame`/`read_frame` column lists drop `puiFlightPlanes`;
  `inject_payload` drops `SeatShipSpawnHeight` (a ship spawn's z stays the
  base height the position parser seats, and `Spawn` applies the effective
  height) and the `UpdateShip` plane check, so `uiFlightPlane` and
  `uiTargetFlightPlane` become unknown keys.
- Engine readers of the game's graph list: the server bake loop
  (`Engine/Source/Main.cpp:229-239`) builds one `NavigationGraphBake` per
  tier, looping `iTier` over `game::kiNavigationClearanceTierCount`, each with
  plane 0 and the base height; the client nav debug draw
  (`Engine/Source/Graphics/Render/MainUniforms.cpp:166-176`) takes graph i's
  tier as i and draws every graph at the base height, its loop keeping its
  bound on the cell's `navigationData` size, which is empty until static data
  arrives. The engine field `NavigationGraphBake::iFlightPlane` stays until
  the stage that removes the table.

### Versions

No backward compatibility: bump `FightersInterpolate::kiVersion` 8 to 9,
`OmnisInterpolate::kiVersion` 18 to 19, `BattleshipsInterpolate::kiVersion`
5 to 6 (each feeds `Frame::kiVersion`, so saves and replays recorded before
are rejected), `FrameInput::kiVersion` 39 to 40 (status-change payloads), and
`engine::kiProtocolVersion` 57 to 58, with a history sentence in
`Documents/Architecture/Network.md`: version 58 removed `uiFlightPlane` from
the three ship `SpawnInfo` `Fields()` lists and `uiTargetFlightPlane` from
`UpdateShipData`, and made `kServerCoordinateStaticData` carry one navigation
graph per clearance tier, indexed by tier. PostRender layouts do not change,
so their versions stay.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/FlightPlaneTable.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.{h,cpp}`,
  `ShipTypes.h`, `ShipQueries.{h,cpp}`, `FleetCoordination.{h,cpp}`,
  `ShipNavigationUtils.cpp`, `ShipStatusChanges.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/{Fighters,Omnis,Battleships}/`
- `Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.{h,cpp}`,
  `Frame.cpp`, `StatusChange.{h,cpp}`, `FrameInput.h`
- `Engine/Source/Network/NetworkProtocol.h`, `Engine/Source/Main.cpp`,
  `Engine/Source/Graphics/Render/MainUniforms.cpp`

## In scope

- `Projects/BrokenEngineSandbox/Source/Frame/FlightPlaneTable.h`: plane 0
  height 10.0f.
- `Frame/Collections/Ships/ShipBehaviours.h` and `.cpp`: delete
  `HighestFlightPlane`, `DefaultFlightPlane`, `IsFlightPlaneAllowed`,
  `FlightPlaneAtHeight`, `NearestFlightPlane`, `NavigationFlightPlane`,
  `SkipsTerrainContact`, `TakesDeathTerrainContact`,
  `ClimbTowardsFlightPlane`, `CappedOccupiedPlanes`,
  `TerrainContactNavData`; rename `NavigationPlaneData` to
  `NavigationTierData(rStaticData, iClearanceTier)`; drop the plane argument
  of `EffectiveShipFlightHeight`, `RaiseAboveTerrain` (also its
  `ruiFlightPlane` output), `NextRouteWaypoint`, `FleeRouteWaypoint`,
  `ComputeReturnDestination`, `ResolveTerrainContact`, `ApplyTerrainBounce`,
  `HasLineOfSight`, `HasShipLineOfSight`; `NavRectangleBoard::BoardRectangle`
  plane members, the constructor's plane assignment, and `Select`'s plane
  filter; `AvoidTerrainRows`' sight and avoidance calls; the declaration
  comments of every changed function and the `DebugRenderNavigation` comment
  naming a higher plane (`:696-697`). The `Frame/FlightPlanes.h` include
  stays for the remaining `Lowest()` reads.
- `Frame/Collections/Ships/ShipTypes.h`: the `ShipOrders` comments naming the
  target plane (`:51-52`, `:67`), `GetTargetFlightPlane`,
  `SetTargetFlightPlane`, `kuiNoFlightPlane`, `IsFlightPlaneAllowed`
  declaration. `kfShipClimbSpeedFraction` stays (Fighter descent, missiles).
- `Frame/Collections/Ships/ShipQueries.h` and `.cpp`: `FindAttackTarget`,
  `AcquireTarget` signatures and plane conditions; rename
  `FindLowestPlaneShipByRegistryId` to `FindShipByRegistryId` and drop its
  plane test; their declaration comments.
- `Frame/Collections/Ships/FleetCoordination.h` and `.cpp`:
  `ShipTactic::iTargetFlightPlane`; `VeerOffTactic`, `AttackTactic`,
  `RouteTactic`, `AttackOrStartVeerOff` (graph lookup via
  `NavigationTierData`), `FindSquadronTarget`, `ChooseFleetTactic`,
  `ChooseFighterFleetTactic`, `ChooseAttackTactic` plane parameters, locals,
  and return fields; their declaration comments.
- `Frame/Collections/Ships/ShipNavigationUtils.cpp`: `ComputeNavigation`'s
  `iNavigationPlane`/`rNavData` lookup (`:260-261`) and its
  `ComputeAiSteering` call (`:457`); the comments naming flight planes
  (`:119`, `:196`, `:216`, and the stored-destination snap comment at
  `:409-411`, whose code stays).
- `Frame/Collections/Ships/ShipStatusChanges.cpp`: the `kUpdateShip` target
  plane write (`:78-81`).
- `Frame/Collections/Fighters/Fighters.h`: `kiFighterHighestFlightPlane` and
  its `static_assert`, the `Frame/FlightPlaneTable.h` include, `PostRead`
  plane check and comment, `puiFlightPlanes` member and tuples,
  PostRender `PostRead` target-plane check and comment,
  `SpawnInfo::uiFlightPlane` and its `Fields()` entry, `kiVersion` 8 to 9.
- `Frame/Collections/Fighters/Fighters.cpp`: `FightersInterpolate::Update`
  climb, rise, and tilt block (`:261-316`); `Transfer` `.uiFlightPlane`;
  `Spawn` height and plane writes (`:481-486`); `PostRender::Update` tactic
  calls, `SetTargetFlightPlane` calls, `ComputeReturnDestination` call, the
  dying bounce gate and calls, and the `RaiseAboveTerrain` call
  (`:606-688`); `LogDifferences` `puiFlightPlanes` line.
- `Frame/Collections/Fighters/FightersCombat.cpp`: `FireBeam` signature, plane
  match, and sight call; the stored-plane gate in the PostCollision loop.
- `Frame/Collections/Omnis/Omnis.h`: `kfOmniClimbRate`,
  `kiOmniHighestFlightPlane` and its `static_assert`, the
  `Frame/FlightPlaneTable.h` include, Interpolate `PostRead` plane check and
  its comment (`:106-107`, the terrain-rise and stored-plane sentences),
  `puiFlightPlanes` and every tuple naming it, PostRender `PostRead`
  target-plane check and its comment (`:233-234`),
  `SpawnInfo::uiFlightPlane` and its `Fields()` entry, `kiVersion` 18 to 19.
- `Frame/Collections/Omnis/Omnis.cpp`: `Spawn` height and plane writes
  (`:238-247`), `LogDifferences` line.
- `Frame/Collections/Omnis/OmnisUpdate.cpp`: `OmnisInterpolate::Update` climb
  and rise (`:53-63`, `:83`); `OmnisPostRender::Update` tactic,
  flagship-chase, `FindAttackTarget`, `AcquireTarget`, and
  `SetTargetFlightPlane` lines (`:292-357`) and the terrain block
  (`:396-416`) as the Design orders it.
- `Frame/Collections/Omnis/OmnisNavigation.cpp`: `.uiFlightPlane` (`:45`).
- `Frame/Collections/Battleships/Battleships.h`:
  `kiBattleshipHighestFlightPlane` and its `static_assert`, the
  `Frame/FlightPlaneTable.h` include, Interpolate `PostRead` height and plane
  checks and their comment (`:70-71`), `puiFlightPlanes` and tuples,
  PostRender `PostRead` target-plane check and its comment (`:182`),
  `SpawnInfo::uiFlightPlane` and `Fields()` entry, `kiVersion` 5 to 6.
- `Frame/Collections/Battleships/Battleships.cpp`: Interpolate climb
  (`:167`, `:174-175`, `:204`), `Transfer` `.uiFlightPlane`, `Spawn` writes
  (`:342-347`), PostRender sight, target, route, and return calls
  (`:465-489`), the bounce gate (`:506-510`), `LogDifferences` line.
- `Frame/Collections/Battleships/BattleshipsCombat.cpp`: the launch gates and
  plane matches (`:182-224`) and the sight call (`:254`).
- `Frame/Collections/Missiles/MissilesUpdate.cpp`: the target-retention
  comment and call (`:144-150`).
- `Frame/TerrainUtils.h` and `.cpp`: `ComputeAiSteering`,
  `ComputeTerrainAvoidance` (parameters, `HasTerrainCollision` and offset
  uses, comments), `FlightPlaneElevationOffset`, `HasTerrainCollision`,
  `kShipTypeHighestFlightPlanes`; delete `NavigationGraph`,
  `ClearanceTierFlightPlaneCount`, `kiNavigationGraphCount`,
  `kNavigationGraphs`, the plane `static_assert` (`:129-133`), and
  `NavigationGraphIndex` (`:77-147`).
- `Frame/Frame.cpp`: `SpawnRandomShipWave` plane locals, terrain test, wave
  orders, and `.uiFlightPlane` (`:273-290`, `:331`, `:415-449`).
- `Frame/StatusChange.h` and `.cpp`: `kuiKeepTargetFlightPlane`,
  `UpdateShipData::uiTargetFlightPlane` and its `Fields()` entry, the
  `IsAdoptableStatusChange` comment and plane tests.
- `Frame/FrameInput.h`: `kiVersion` 39 to 40.
- `Network/Server/ServerClientManager.cpp`: `SpawnFleetMember` plane lines
  (`:163`, `:187`, `:208-209`, `:218`).
- `Network/Server/FleetRows.cpp`: the `SetTargetFlightPlane` line (`:54`).
- `Network/Server/FleetNavigationController.cpp`: `FlagshipReachesEdge`
  graph lookup (`:26-32`).
- `Agent/AgentCommandsServerQueries.cpp`: the `targetFlightPlane` column
  (`:62`).
- `Agent/Commands/ServerFrameEdit.cpp`: `puiFlightPlanes` in
  `FightersFrameColumns`, `OmnisFrameColumns`, `BattleshipsFrameColumns`.
- `Agent/Commands/ServerSimulationHarnessRigs.cpp`: `SeatShipSpawnHeight` and
  its callers, the `vecPosition` parse comment (`:111-112`), the `UpdateShip`
  adoptability block (`:370-377`).
- `Engine/Source/Main.cpp`: the server bake loop (`:229-239`), one entry per
  clearance tier.
- `Engine/Source/Graphics/Render/MainUniforms.cpp`: the nav debug loop's
  graph lookup, height, and tier blend (`:171-175`).
- `Engine/Source/Network/NetworkProtocol.h`: `kiProtocolVersion` 57 to 58.
- `Documents/Architecture/Network.md`: the version-history sentence for 58.
- Prose this stage makes wrong:
  - `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` `## Invariants`:
    the navigation bullet's "the planes each tier has graphs on" (`:72-74`;
    the `FlightPlaneTable.h` sentence stays) and the admission bullet's
    plane clause (`:105-110`).
  - `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md`
    `## Ship Invariants`, `## Fleet Coordination Invariants`,
    `## Fleet Navigation Invariants`: every rule about stored or target
    planes, plane travel, the descent hold, nearest-plane target choice,
    plane-capped rectangles, sight on the stored plane, highest-plane
    exclusion, terrain-response gating, and "its plane's polygons"; rewrite
    the effective-height, terrain-rise (Fighters only), and terrain-contact
    bullets to the Design. The pusher sentence about occupied flight-plane
    layers stays (engine-owned, later stage).
  - `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/AGENTS.md`
    `## Behavior Invariants` and `## Weapon Invariants`: highest-plane,
    nearest-plane, target-plane, and stored-plane clauses; the climb-tilt
    bullet (descent toward the base height).
  - `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/AGENTS.md`
    `## Behavior Invariants` and `## Heal Beam`: stored-plane, climbing,
    lowest-plane, nearest-plane, highest-plane, and target-plane clauses.
  - `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/AGENTS.md`:
    "at or below plane 0" (`:10`) and the plane wording in
    `## Weapon Invariants`.
  - `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/AGENTS.md`
    `:15-17`: the nearest-plane target-retention clause.
  - `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md`
    `## Deterministic Tick Contracts`: "and stored plane" in the Fighter
    launch sentence.
  - `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`
    `query_collection` (`:32`, `:42-44`) and `inject_payload` (`:232-234`,
    `:245-251`, `:263-264`, `:277-292`).
- Stale non-Plan documents:
  - Delete `Documents/Features/Frame/HighLayerHeightTweaks.md` (tuning the
    high plane ships fly; no ship flies it after this stage).
  - Delete `Documents/Investigations/Engine/ShipForcesInThreeDimensions.md`
    (its premise is ships moving between flight planes).
  - Update `Documents/Investigations/Engine/ShipLineOfSightRealShapes.md`
    `:37-40`: one polygon set per clearance tier instead of per plane.
  - Update `Documents/Investigations/Engine/SameCollectionCollision.md` `:4`:
    drop "ships fly on three flight planes".

## Out of scope

- The engine flight-plane table and its consumers: `engine::FlightPlanes`,
  `kTerrainCollision`, `NavigationGraphBake::iFlightPlane`, the pusher layers,
  `IslandTerrain`/`NavCellData` bake gating, every
  `gpFlightPlanes->Lowest()` base-height read, and the render
  `NearestPlaneIndex`/`FillModelLayout` plane arguments in the ship and
  missile renderers.
- World lighting layers, smoke planes, object-shadow plane channels, camera,
  and audio heights.
- The 70 m table entry and the ModelViewer table (2 m) values.
- `ShipOrders` width; terrain steering and avoidance tuning constants;
  `ComputeNavigation`'s stored-destination snap code; Battleship and dying-ship
  terrain behaviour beyond removing the plane gates; the Omni hull-turn model.
- `CollisionLayer`, `ShipHitboxes` `iLayerIndex`, frame-registry ownership
  layers, and other uses of the word "layer" unrelated to flight planes.
- Unit tests.

## Risk triggers and invariants

Tier 3: CRC-visible ship behaviour, the Interpolate layouts and status-change
payloads (save, replay, and wire), and spans the game and two engine files.

- Client and server stay bit-identical: Fighter descent lands exactly on the
  effective height through `StepHeightTowards`, and every removed branch is
  removed on both builds.
- Every PostRender script still stores velocity Z as 0 last; height changes
  step the position directly.
- `pfClimbTilts` stays client-only and outside the CRC.
- A ship's position stays inside its cell; spawn sites keep their
  `IsInsideCell` tests.
- Path queries keep their cadence and random-draw counts; no new random draw.
- `AdmitFrameInputStatusChange` still rejects every other inadmissible payload.

## Acceptance criteria

1. `/compile` builds the BrokenEngineSandbox client and server and the
   ModelViewer.
2. No `puiFlightPlanes`, `uiFlightPlane`, `uiTargetFlightPlane`,
   `kuiNoFlightPlane`, `kuiKeepTargetFlightPlane`, `GetTargetFlightPlane`,
   `SetTargetFlightPlane`, `IsFlightPlaneAllowed`, `NearestFlightPlane`,
   `NavigationFlightPlane`, `ClimbTowardsFlightPlane`, `HighestFlightPlane`,
   `DefaultFlightPlane`, `kShipTypeHighestFlightPlanes`, or
   `iTargetFlightPlane` identifier remains under
   `Projects/BrokenEngineSandbox/Source`, and no `kNavigationGraphs`,
   `kiNavigationGraphCount`, `NavigationGraphIndex`,
   `ClearanceTierFlightPlaneCount`, or `NavigationPlaneData` identifier and
   no `game::NavigationGraph` struct remains under
   `Projects/BrokenEngineSandbox/Source` or `Engine/Source`.
3. Versions: `FightersInterpolate` 9, `OmnisInterpolate` 19,
   `BattleshipsInterpolate` 6, `FrameInput` 40, `kiProtocolVersion` 58, and
   the Network.md history sentence.
4. `/agent-harness`, after a server `reset` with a connected client and the
   fleet and random waves spawned: `query_collection` ship rows carry no
   `targetFlightPlane`; every Omni and Battleship row's local z is the same
   value per controller and at least 10; every Fighter row's z is at least 10.
5. Hidden target: using `cell_coordinate_probe` navigation samples to place
   them, `inject_payload` a random-spawn Fighter and an enemy-aligned ship on
   opposite sides of an island so the straight segment between them is
   blocked; after unpausing, sampled `query_collection` rows show the Fighter
   closing to within 50 m (`kfFighterBeamRange`) of the target, and its z at
   least 10 throughout.
6. Omni terrain: `inject_payload` a random-spawn Omni with no enemy in the
   cell and a velocity toward an island; across samples its z never changes
   and `cell_coordinate_probe` elevation at each sampled Omni x/y stays below
   its z.
7. `inject_payload` fails with an unknown-key error for a `SpawnFighter`
   entry carrying `uiFlightPlane` and for an `UpdateShip` entry carrying
   `uiTargetFlightPlane`.
8. The `/agent-harness` replay determinism check (`replay_record`, then
   `replay_play`) over a run with fleet and random-wave combat reports no CRC
   mismatch.
