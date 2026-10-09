<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-09T00:14:22.326Z","dependsOn":["Documents/Plans/Engine/RemoveEngineRuntimeOverEngineering.md","Documents/Plans/Game/FlightPlaneClimbing.md"]} -->
# Build navigation per terrain-collision flight plane and path ships on the plane they navigate

Tier 3 (`.agents/references/risk-tiers.md`): the change alters the server-to-client static-data wire layout and `Frame::kiVersion`, changes CRC'd navigation and steering results for ships above the lowest plane, and spans the engine Frame, Main, Graphics debug render, and the game Frame. Line numbers cite baseline `c1420821` unless marked `90fa6c85`; the prerequisite Plans rename the files, functions, and columns named below, and SplitIslandTerrainCollision supplies `Engine/Source/Frame/TerrainCollisionUtils.h/.cpp`, so the post-Plan names are used throughout and each citation names the current-tree site it stands for. Where a statement here and the code disagree, the code wins; report the contradiction instead of matching one side to the other. Paths below `Frame/` mean `Projects/BrokenEngineSandbox/Source/Frame/`; `Engine/` paths are repository-relative. Flight-plane series; the term is "flight plane".

## Context

`Documents/Plans/Engine/FlightPlaneData.md` hands the engine the game's flight-plane table at startup (`engine::gpFlightPlanes`: `Count()`, `Plane(i)`, `Lowest()`, `Highest()`, `FlightPlaneFlags::kTerrainCollision`; low 6 m and medium 50 m collide with terrain, high 100 m does not). `Documents/Plans/Game/ShipTypeSplit.md` moves navigation into `Frame/Collections/Ships/ShipBehaviours.cpp` `ComputeNavigation` and gives every ship `pOrders`. `Documents/Plans/Game/FlightPlaneClimbing.md` puts the target plane in `ShipOrders` bits 8-15 (`GetTargetFlightPlane`), derives the current plane from Z (`FlightPlaneAtHeight`), and, as a stopgap, flattens every `NavQuery*` argument to `Lowest()` so ships on every plane path around the lowest plane's obstacles; it leaves `ComputeTerrainAvoidance` and `ComputeArtificialIntelligenceSteering` on absolute elevation thresholds. This Plan removes that stopgap.

Current-tree evidence this design rests on:

- Server startup bakes one contour per island template: `Engine/Source/Main.cpp:342` calls `gpIslandTerrain->WaitForElevationMaps(game::NavigationThresholdElevation(<lowest height>), game::NavigationClearanceMeters())` (`Frame/TerrainUtils.h:26-39`, threshold = height minus radius minus push margin); `IslandTerrain::WaitForElevationMaps` (`Engine/Source/Frame/IslandTerrain.cpp:210-226`) dequantizes each heightmap once and calls `BuildNavContour` (`NavBuild.cpp:317-421`: marching squares at the threshold, Clipper2 union, inflate by the clearance in meters, simplify, store in UV) into `IslandTemplate::navContour` (`IslandTerrain.h:55`), under `kBootTimerIslands`.
- Each cell's visibility graph is built on its first tick (`FrameBase.cpp:205-216`) or before a paused subscription is served (`ServerSessionRuntime.cpp:119-138`) by `BuildCellNavigationData(NavData&, placements)` (`NavCellData.cpp:292-369`), which places every template's contour into cell-local meters, then builds acceleration, visibility, and adjacency; a cell whose contours are all empty keeps the cleared derived state. `CellStaticData::navigationData` (`CellStaticData.h:32`) is `mutable`, outside the CRC and the grid save (`GridSave.cpp:43,117` pass `bIncludeNavigationData=false`), and shipped to clients inside the static-data packet (`CellStaticData.cpp:17-20,37-43`; `ServerBufferedFrames.cpp:213-243` serializes into a growable scratch and sends it reliable with an `int32_t` size; no cap beyond that); `NavData::Write/Read` (`NavCellData.cpp:439-498`) serialize vertices, polygon offsets, and visibility edges and rebuild acceleration on read. `kiNavDataVersion = 15` (`NavBuild.h:15`) is summed into `Frame::kiVersion` (`Frame/Frame.cpp:38`), which the Hello handshake, grid save, and replay gate.
- `NavQuery` (`NavQuery.cpp:558-703`) reads x/y only; `NavQuerySnapToNavigable` and `NavQueryDirection` assert the position and destination Z equal the lowest plane (`:574-575,608-610`) and write snap and waypoint Z at that height (`:459,589,660,676`); an empty graph returns the straight-line direction (`:624-627`). `NavQueryPointBlocked` places no requirement on Z (`NavQuery.h:15-17`).
- `engine::ResolveDiscAgainstTerrain` resolves an overlapped start with `NavQuerySnapToNavigable(XMVectorSetZ(vecStart, <lowest height>), rStaticData.navigationData)`, using only the snapped XY (`IslandTerrain.cpp:943`, `90fa6c85`; `Documents/Plans/Engine/SplitIslandTerrainCollision.md`, a prerequisite of FlightPlaneData, moves the body unchanged into `Engine/Source/Frame/TerrainCollisionUtils.cpp` and the declaration from `IslandTerrain.h:203` into `TerrainCollisionUtils.h`). Its two callers after ShipTypeSplit are the shared `ResolveTerrainContact` (the Omni slide, from `OmnisPostRender::Update`) and `ApplyTerrainBounce` (from `FightersPostRender::Update` and `BattleshipsPostRender::Update`) in `ShipBehaviours.cpp`; FlightPlaneClimbing decision 11 makes each pass its position Z as `fThresholdHeight` and skip the call when `SkipsTerrainContact` holds.
- Game call sites (post-ShipTypeSplit all in `ShipBehaviours.cpp` `ComputeNavigation` and its static helpers; today `PlayersNavigation.cpp`): `ShouldRecomputeNavigation` (`:175`, two `NavQueryPointBlocked`), `RecomputeNavigationPath` (`:194/196`), the island destination built at the lowest height and snapped (`:356,359`), the mode 0-3 query (`:414/416`), the 3D arrival test (`:374-375`), and the roam branch calling `ComputeArtificialIntelligenceSteering` (`:446`). Fighter terrain avoidance calls `ComputeTerrainAvoidance` from the `AvoidTerrain` phase hook (`SpaceshipsNavigation.cpp:191`; Battleships copy it under ShipTypeSplit).
- `TerrainUtils.cpp` compares sampled elevation (meters above sea level) with absolute constants that assume a ship on the lowest plane: `kfPreferredElevation = 0.2f` (`:8,38`), `kfHighElevationThreshold = 0.5f` (`:12,45`), `kfAvoidTerrainMinimum = 0.5f` and `kfAvoidTerrainMaximum = 2.5f` (`:71-72,109-111`).
- The client debug nav render draws every cell's `navigationData` at the lowest height (`Engine/Source/Graphics/Render/MainUniforms.cpp:153-195`, client-only, outside the CRC).
- Boot timers are logged by `ProfileManagerBase::BootLog` as `<name>: <ms> ms` when profiling is enabled (`Engine/Source/Profile/ProfileManagerBase.cpp:565-581`).

## Decisions

Each decision is recorded as made; the alternatives are not open.

1. **The game hands the engine one threshold per plane at startup.** `IslandTerrain::WaitForElevationMaps` (server) takes `std::span<const float> navigationThresholds, float fNavigationClearanceMeters`; the span is index-parallel with `gpFlightPlanes` and its size must equal `Count()` (`ASSERT`). `Engine/Source/Main.cpp` fills a `std::vector<float>` with `game::NavigationThresholdElevation(gpFlightPlanes->Plane(i).fHeightMeters)` for every plane (startup may allocate) and passes it; `NavigationThresholdElevation` and `NavigationClearanceMeters` keep their signatures. The engine still receives both inputs from the game and reaches into no game constant.
2. **Per-plane contours on the template.** `IslandTemplate::navContour` becomes `std::vector<NavContour> navContours`, sized `Count()` and index-parallel with the planes. The bake loop dequantizes each heightmap once, then for every plane whose `flags` carry `kTerrainCollision` calls `BuildNavContour(rTemplate.navContours.at(i), ..., navigationThresholds[i], fNavigationClearanceMeters, ...)`; a plane without the flag keeps an empty contour. The engine reads the flag from its own `gpFlightPlanes`. Indexing by plane index (not a compact collision-only array) is the simplest: no mapping between the two numberings anywhere.
3. **Per-plane graphs on the cell.** `CellStaticData::navigationData` becomes `std::vector<NavData>`, index-parallel with the planes. `BuildCellNavigationData(std::vector<NavData>& rNavigationData, const std::vector<IslandPlacement>&)` resizes it to `Count()` and runs today's body once per plane from `navContours.at(iPlane)` (a `static` per-plane helper in `NavCellData.cpp`); a no-collision plane yields an empty `NavData` with the cleared derived state, exactly what an island-free cell yields today. The two build sites (`FrameBase.cpp:213`, `ServerSessionRuntime.cpp:134`) and `bNavigationDataBuilt` are unchanged. `CellStaticData::Write(true)` writes an `int32_t` plane count then each `NavData::Write`; `Read(true)` reads the count, resizes, and reads each (acceleration rebuilt per plane as today), trusting the server under the corrupt-input policy; `Read(false)` clears the vector. The `BuildCellNavData` log line gains `plane={}`.
4. **`NavQuery` becomes height-agnostic.** The two Z asserts are removed; `NavQuerySnapToNavigable`, the escape and destination waypoints, and `AStarPath`'s waypoint take their Z from the query position's Z instead of a plane height (the `fBaseHeight` parameter of `AStarPath` becomes that value). Signatures are unchanged and `NavQuery` reads no flight-plane data: the caller chooses the plane by choosing which `NavData` it passes. This deviates from the guidance "NavQuery takes a plane index" because passing the index beside the matching `NavData` would be two values that must agree; the empty-graph straight-line path (`:624-627`) is untouched and serves the no-collision planes.
5. **Which plane a ship navigates.** `ShipBehaviours.h` gains `NavigationFlightPlane(float fHeight, int64_t iTargetPlane)`: `iBelow` is the highest plane index with `Plane(i).fHeightMeters <= fHeight` (exists because Z is never below `Lowest()`, FlightPlaneClimbing decision 4), and the result is `min(iBelow, iTargetPlane)`. On a plane this is that plane unless the ship is about to descend, in which case it is the lower target; between planes it is the lower of the plane below and the target, which is the lower of the two planes bracketing Z whenever the target is above and the target itself when descending. A lower plane's blocked area contains a higher one's, so the choice is always conservative.
6. **`ComputeNavigation` queries one plane per call.** It computes `iNavigationPlane = NavigationFlightPlane(XMVectorGetZ(vecPosition), GetTargetFlightPlane(orders))` once and `const engine::NavData& rNavData = rStaticData.navigationData.at(iNavigationPlane)`; the two `NavQueryPointBlocked` calls, both `NavQueryDirection` calls, and `NavQuerySnapToNavigable` pass `rNavData`, and the FlightPlaneClimbing `XMVectorSetZ(v, Lowest())` flattening before each is deleted. The island destination is built at the ship's own Z (`XMVectorSet(fX, fY, XMVectorGetZ(vecPosition), 1.0f)`) and the arrival test uses `XMVector2LengthSq`, so a ship that climbs after choosing a destination still arrives. For a ship on the lowest plane every one of these values is bit-identical to today (Z equals the lowest height, and the squared 3D distance added an exact 0.0f).
7. **Steering and avoidance thresholds are relative to the navigated plane.** `ComputeArtificialIntelligenceSteering` and `ComputeTerrainAvoidance` gain a trailing `int64_t iFlightPlane`. When `Plane(iFlightPlane).flags` lacks `kTerrainCollision` they return their input unchanged (the normalized current direction; `fCurrentDeltaRotation`), which is the straight-line steering the no-collision plane uses. Otherwise every `Sample` result has `FlightPlaneElevationOffset(iFlightPlane) = Plane(iFlightPlane).fHeightMeters - Lowest().fHeightMeters` (a `static` in `TerrainUtils.cpp`) subtracted before it meets the unchanged constants, so "0.5 m above the lowest plane's reference" becomes "0.5 m above this plane's reference"; on the lowest plane the offset is exactly 0.0f and the subtraction is exact. `ComputeNavigation`'s roam branch passes `iNavigationPlane`; `FightersPostRender::AvoidTerrain` and `BattleshipsPostRender::AvoidTerrain` pass `NavigationFlightPlane(XMVectorGetZ(pVecPositions[i]), GetTargetFlightPlane(pOrders[i]))`.
8. **Debug nav render per plane.** `DebugRenderNavigationData` loops the planes and draws `navigationData.at(i)` at `Plane(i).fHeightMeters`. Client-only, outside the CRC.
9. **Versions.** `kiNavDataVersion` 15 → 16 (the static-data wire layout and the graph set changed); `Frame::kiVersion` follows and already gates the handshake, grid saves, and replays, so no other version is bumped: the engine packet envelope is unchanged (`kiProtocolVersion` stays), the grid save carries no navigation data, and no collection layout changes. No backward compatibility.
10. **Cost is accepted, not capped.** Contour bake, per-cell graph build, and the static-data payload scale with the number of terrain-collision planes (two today; the medium plane's graph is smaller because fewer texels pierce 50 m). No cap exists on the static-data payload and none is added.
11. **The terrain-contact start-overlap snap uses the navigated plane's graph.** `engine::ResolveDiscAgainstTerrain` gains `const NavData& rNavData` after `rStaticData` and snaps with `NavQuerySnapToNavigable(vecStart, rNavData)`; the `XMVectorSetZ` wrapper and the comment clause saying the snap asserts the Z go, because decision 4 removes that assert and only the snapped XY is used. The shared `ResolveTerrainContact` and `ApplyTerrainBounce` each gain a trailing `int64_t iTargetFlightPlane`, compute `iNavigationPlane = NavigationFlightPlane(<the sweep-end Z (FlightPlaneClimbing decision 11)>, iTargetFlightPlane)`, `ASSERT` that `Plane(iNavigationPlane).flags` carries `kTerrainCollision`, and pass `rStaticData.navigationData.at(iNavigationPlane)`; `OmnisPostRender::Update`, `FightersPostRender::Update`, and `BattleshipsPostRender::Update` pass `GetTargetFlightPlane(orders)`. This is the same plane the ship paths on, consistent with transiting ships using the lower plane's navigation (decision 5); plane 0 is not used for every ship because it would snap a medium-plane ship out to the much wider outline traced for the 6 m plane. The snap never indexes an empty no-collision graph: a ship on a no-collision plane skips terrain contact entirely (FlightPlaneClimbing `SkipsTerrainContact`), and otherwise every plane at or below the ship's Z collides with terrain in today's table, which the `ASSERT` guards. A ship on the lowest plane passes plane 0, today's graph, and the snapped XY is unchanged, so its result is bit-identical.

## Design

`Engine/Source/Frame/IslandTerrain.h`:

```cpp
std::vector<NavContour> navContours;   // one per flight plane, index-parallel with gpFlightPlanes; empty without kTerrainCollision
#if defined(BT_SERVER)
void WaitForElevationMaps(std::span<const float> navigationThresholds, float fNavigationClearanceMeters); // size == gpFlightPlanes->Count()
#endif
```

`Engine/Source/Frame/NavBuild.h`:

```cpp
inline constexpr int64_t kiNavDataVersion = 16;
void BuildCellNavigationData(std::vector<NavData>& rNavigationData, const std::vector<IslandPlacement>& rPlacements); // one NavData per flight plane
```

`Engine/Source/Frame/CellStaticData.h`: `mutable std::vector<NavData> navigationData;` with the comment stating the per-plane indexing. `Write(true)`/`Read(true)` per decision 3.

`Engine/Source/Main.cpp` (server branch, replacing `:342`):

```cpp
std::vector<float> navigationThresholds(static_cast<size_t>(gpFlightPlanes->Count()));
for (int64_t i = 0; i < gpFlightPlanes->Count(); ++i)
{
	navigationThresholds.at(static_cast<size_t>(i)) = game::NavigationThresholdElevation(gpFlightPlanes->Plane(i).fHeightMeters);
}
gpIslandTerrain->WaitForElevationMaps(navigationThresholds, game::NavigationClearanceMeters());
```

`Engine/Source/Frame/NavQuery.cpp`: delete the asserts at `:574-575,608-610`; `float fWaypointHeight = XMVectorGetZ(vecPosition);` replaces each `fBaseHeight` read.

`Engine/Source/Frame/TerrainCollisionUtils.h` (decision 11; the declaration comment names `rNavData` as the graph the overlapped start snaps against):

```cpp
DiscTerrainResult XM_CALLCONV ResolveDiscAgainstTerrain(const CellStaticData& rStaticData, const NavData& rNavData, FXMVECTOR vecStart, FXMVECTOR vecEnd, float fRadius, float fThresholdHeight, bool bSlide);
```

`Frame/Collections/Ships/ShipBehaviours.h`:

```cpp
// The plane whose graph a ship paths and steers on: min(highest plane at or below its height, target plane).
[[nodiscard]] int64_t NavigationFlightPlane(float fHeight, int64_t iTargetPlane);
```

`ResolveTerrainContact` and `ApplyTerrainBounce` in the same header keep their ShipTypeSplit parameters and add a trailing `int64_t iTargetFlightPlane` (decision 11).

`Frame/TerrainUtils.h`:

```cpp
AiSteeringResult XM_CALLCONV ComputeArtificialIntelligenceSteering(const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, FXMVECTOR vecCurrentDirection, FXMVECTOR vecCellCenter, float fDeltaTime, bool bAlternateContour, int64_t iFlightPlane);
float XM_CALLCONV ComputeTerrainAvoidance(const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, FXMVECTOR vecDirection, float fCurrentDeltaRotation, int64_t iFlightPlane);
```

Per-call sequence in `ComputeNavigation`: compute `iNavigationPlane` and `rNavData` after the direction reseed and before `UpdateFleetAndFlagshipNavigation`; pass `rNavData` to `ShouldRecomputeNavigation` and `RecomputeNavigationPath` (replacing their `rStaticData.navigationData` reads), to the island-destination snap, and to the mode 0-3 query; pass `iNavigationPlane` to the roam branch.

## Critical files

- `Engine/Source/Frame/NavBuild.h`, `NavCellData.cpp`, `NavQuery.cpp`, `IslandTerrain.h`, `IslandTerrain.cpp`, `TerrainCollisionUtils.h`, `TerrainCollisionUtils.cpp`, `CellStaticData.h`, `CellStaticData.cpp`
- `Engine/Source/Main.cpp`, `Engine/Source/Graphics/Render/MainUniforms.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.h`, `TerrainUtils.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.h`, `ShipBehaviours.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/` and `Battleships/` (the `AvoidTerrain` hooks and the `ApplyTerrainBounce` calls), `Omnis/` (the `ResolveTerrainContact` call)

## In scope

- `IslandTerrain.h/.cpp`: `navContours`, the `WaitForElevationMaps` signature and per-plane bake loop (decisions 1, 2).
- `NavBuild.h`: `kiNavDataVersion`, the `BuildCellNavigationData` declaration and the `NavData` comment naming the per-plane indexing. `NavCellData.cpp`: the per-plane `BuildCellNavigationData` and its log line (decision 3).
- `CellStaticData.h/.cpp`: the vector member, `Write(true)`, `Read(true)`, `Read(false)` (decision 3).
- `NavQuery.cpp`: the assert deletions and the waypoint and snap Z source in `NavQuerySnapToNavigable`, `NavQueryDirection`, and `AStarPath` (decision 4).
- `Main.cpp`: the threshold vector and the `WaitForElevationMaps` call (decision 1).
- `MainUniforms.cpp` `DebugRenderNavigationData` (decision 8).
- `ShipBehaviours.h/.cpp`: `NavigationFlightPlane`; in `ComputeNavigation` and its helpers `ShouldRecomputeNavigation` and `RecomputeNavigationPath`: the plane selection, the `NavData` reference threaded to the five `NavQuery*` calls, deletion of the FlightPlaneClimbing flattening, the island destination Z, the horizontal arrival test, and the roam-branch argument (decisions 5-7).
- `TerrainUtils.h/.cpp`: the two signatures, `FlightPlaneElevationOffset`, the no-collision early returns, and the offset at each `Sample` site (decision 7).
- `FightersPostRender::AvoidTerrain` and `BattleshipsPostRender::AvoidTerrain`: the `ComputeTerrainAvoidance` argument (decision 7).
- `TerrainCollisionUtils.h/.cpp` (`IslandTerrain.h:203` and `IslandTerrain.cpp:943` at `90fa6c85`, before SplitIslandTerrainCollision): the `ResolveDiscAgainstTerrain` `rNavData` parameter, its declaration comment, and the start-overlap snap call (decision 11). `ShipBehaviours.h/.cpp` `ResolveTerrainContact` and `ApplyTerrainBounce`: the `iTargetFlightPlane` parameter, the plane selection, the `ASSERT`, and the `rNavData` argument; their calls in `OmnisPostRender::Update`, `FightersPostRender::Update`, and `BattleshipsPostRender::Update` (decision 11).
- The `AGENTS.md` lines `/update-claude-docs` finds stale, at least `Engine/Source/Frame/AGENTS.md` `## Terrain and Navigation` (per-plane contours and graphs; thresholds are a per-plane list), `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` (the navigation-inputs bullet), and the ship navigation invariants ShipTypeSplit leaves in `Frame/Collections/Ships/AGENTS.md` or `Omnis/AGENTS.md` (the plane a ship navigates).

## Out of scope

- An offline nav bake, any DataPacker or pack change, and any cap or compression for the larger static-data payload.
- `kiProtocolVersion`, the static-data packet envelope, grid-save or replay formats beyond what `Frame::kiVersion` already gates.
- `NavQuery` signature changes, A* or visibility-graph algorithm changes, the shared predicates in `NavBuildInternal.h`, `kiNavZonesX/Y`, and any per-plane tuning of clearance or thresholds beyond `NavigationThresholdElevation(height)`.
- Terrain contact (`ApplyTerrainPush`, `ApplyTerrainBounce`, `ResolveTerrainContact`), which FlightPlaneClimbing already keys to the ship's own Z, apart from the graph the start-overlap snap uses (decision 11): the thresholds, skip rule, sweeps, slide, and bounce are unchanged.
- Fighter and Battleship flee/return island-center distances (`ComputeFleeOrReturnDestination`, 3D against candidates at the lowest height), `FollowFlagship`, the mode 0-3 cell-center destination Z, weapons, spawn validity grids, smoke, lighting, camera, and the client navigation debug waypoint render in the ship render files.
- Fleet AI choosing planes, any HUD or harness command beyond those FlightPlaneClimbing adds, and `FlightPlaneTable.h`.
- Client-side validation of the per-plane count or graph contents (the client trusts server static data).

## Risk triggers and invariants

- Wire and version: the static-data payload layout changes and `kiNavDataVersion` carries it into `Frame::kiVersion`, so older clients, saves, and replays are rejected at the existing gates; the engine packet envelope and `kiProtocolVersion` are unchanged.
- CRC: navigation directions, island destinations, and steering for ships above the lowest plane change (intended, covered by the bump). For ships on the lowest plane every value is bit-identical: the plane-0 contour uses today's threshold, the island destination Z equals the lowest height, the horizontal arrival test equals the 3D one when the Z difference is 0, the terrain-contact snap reads plane 0's graph and only its XY, and the steering offset is exactly 0.0f (`Documents/FloatingPointDeterminism.txt`: same operation order on both builds).
- Client and server agree because the client receives the server's per-plane graphs and rebuilds acceleration per plane exactly as today, and `NavigationFlightPlane` reads only CRC'd Z and orders plus the immutable `gpFlightPlanes`.
- Determinism of the build: the per-plane loop runs in ascending plane order on one thread per cell, each plane's body identical to today's; the `gridEdges` cursor order is preserved per plane.
- Navigation and elevation stay derived data outside the CRC and the persisted payload (`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`); `Read(false)` clears the vector so a save reload rebuilds.
- Engine/game contract: the engine takes the thresholds as caller-supplied values and reads only its own `gpFlightPlanes`; `Main.cpp` remains the only engine TU naming `game::NavigationThresholdElevation` (`Engine/Source/AGENTS.md` `## Hub Conventions`).
- Trust: the client adds no checks on server static data (`Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy`); nothing client-to-server changes.
- Threading: the per-plane build runs where the single build runs today (per-cell tick thread or the main thread before a paused subscription is served) under the same allocation-tracking suppression; `navContours` is written only during boot before `Game` exists.
- No main-loop allocation beyond today's: `BuildCellNavigationData` grows vectors under the existing suppression; `NavigationFlightPlane` and the offset helper allocate nothing.
- Conservative plane rule: the navigated plane is never above the plane at or below the ship's Z and never above its target, so a transiting ship paths around every obstacle that could block it on arrival. Terrain contact's start-overlap snap uses that same plane, and the plane it indexes always carries `kTerrainCollision` (`ASSERT`, decision 11).

## Acceptance

| Criterion | Expected observation | Method |
|---|---|---|
| Client and server builds | Both succeed | `/compile` BrokenEngineSandbox client and server |
| Per-plane build and wire | Server `get_logs` shows `BuildCellNavData` lines with `plane=0`, `plane=1`, and `plane=2` per cell, with `plane=2` reporting `vertices=0`, `plane=1` fewer vertices than `plane=0`, and the `plane=0` vertex and polygon counts equal to the pre-change counts for the same cell; a client connects and receives static data for every subscribed cell without a `LogDifferences CRC Client` or `CONFIRMED DESYNC` line | `/agent-harness` server and client launch, `get_logs` baseline then diff; the pre-change counts from a baseline run of the same cell |
| Medium plane paths around tall terrain only | A fleet ordered to plane 1 (`set_fleet_flight_plane`) with a wanted coordinate across an island: `query_collection` `local` samples show it steering around terrain that pierces the medium threshold (about 48 m) and flying straight over lower hills that the same fleet on plane 0 detours around; the client debug nav render shows the plane-1 polygons at 50 m inside the plane-0 polygons | `/agent-harness`: `set_fleet_flight_plane`, `inject_payload` `UpdateFleet`, timed `query_collection`, `screenshot` with debug render |
| High plane flies straight over peaks | A Fighter ordered to plane 2 and driven across an island peak keeps `dir` on the straight bearing to its destination (no detour, no avoidance turn) and crosses without a terrain-contact log | `/agent-harness`: `set_fleet_flight_plane` plane 2, `query_collection` `dir`/`local` samples, `get_logs` |
| Climbing low to medium uses the low graph until arrival | A ship on plane 0 beside a hill that only the plane-0 graph blocks, ordered to plane 1 with a destination behind the hill: while `local[2]` is between 6.0 and 50.0 its `dir` detours as on plane 0; within one recompute interval after `local[2]` reaches exactly 50.0 it turns to the straight bearing | `/agent-harness`: `set_fleet_flight_plane`, timed `query_collection` |
| Lowest-plane behaviour unchanged | With every ship on plane 0, a scripted fleet run's `query_collection` `local`/`dir` samples match a baseline run of the same script and seed on the pre-change build | `/agent-harness` scripted run on both builds, sample comparison |
| Boot cost | The server boot log's `Islands: <ms> ms` line (profiling build) is recorded before and after; the after value is about twice the before value (two terrain-collision planes) and never more than the number of collision planes times the before value | Server log, before and after |
| Repeated-run replay determinism | No `LogDifferences CRC` mismatch across repeated playback loops of a recording containing ships on all three planes, a climb, and a transfer | `/agent-harness` replay (`replay.md`) |
