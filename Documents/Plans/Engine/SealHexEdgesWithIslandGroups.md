<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-06T00:41:04.290Z","dependsOn":[]} -->
# Seal some hex edges with deliberate island groups

## Context

Originating gap: the region island chain change gave a fleet a pick-time edge check
(`NavQueryEdgeReachable`, reached through `FlagshipReachesEdge` in
`Projects/BrokenEngineSandbox/Source/Network/Server/FleetNavigationController.cpp`), but the
generator never produces an edge that check refuses. Its acceptance criteria for a sealed edge and
for a replay spanning a refusal could not pass, so the refusal branch of `TickFleetTimers` has been
read and never run, live or in playback.

Observed on that change's tree, by a server `cell_coordinate_probe` sweep of radius 20 with
`navigation` on graph 4 (Battleship clearance tier, plane 0):

- 0 of 7,566 cell and direction pairs over 1,261 cells report `reachableEdges` false.
- An independent walk of the returned polygons at 5 m steps finds 2,854 edges holding a blocked
  sample and none fully blocked. The tightest longest open run is 25 m, at [-3,-17] direction 4,
  [-4,9] direction 3, and [-5,9] direction 0.

Root cause, from `Engine/Source/Frame/IslandChainPlacement.cpp`:

- `BuildRoutes` walks from cell to cell through jittered cell centres, so a route crosses a hex edge
  instead of running along it, and `LayGroups` lays its groups on that route.
- `Collides` holds groups `kfGroupGapMeters` apart and `IsInsideRegion` holds regions twice
  `kfRegionPaddingMeters` apart, so two groups can never meet. Only one group astride an edge could
  close it.
- A shared edge is `kfCellHalfWidth` long, about 520 m. The largest hull at scale spans 358 m, and
  group members touch along the route tangent, not along the edge.

The originating session's brief reports that the user approved this direction: sealed passages come
from the generator making some chains deliberately close their gaps across a hex edge, so that what
is visible matches what ships can do, and not from raising `kfOpenStretchMeters`. That record is
relayed, not quoted. Every other choice below is this Plan author's recommendation.

Harness limit the old sealed-edge recipe missed: an `inject_payload` `UpdateFleet` writes ship rows
only. `TickFleetTimers` drains and draws only while the flagship is at `Fleet::wantedCoordinate`, and
it is that field's only writer after creation, apart from restoring a save (`ServerFleetManager.cpp`
sets it to the origin at creation). A fleet ordered into a cell therefore holds no timer there and
never reaches the draw.
A new fleet does hold one, at the origin, which is why the design below seals an origin edge.

Measured for the values below, at the shipped island scale:

- Hull extents: Large 315-351 m long and 114-183 m wide, all 7 templates at aspect 1.5 or more; Huge
  345-358 m on both axes, 3 templates.
- Walking the long axis through an island's centre against graph 4 polygons, over the 10 Large or
  Huge islands in the radius 12 sweep with no other island near either end: land stops short of the
  hull's end by a median of 5 m and at most 30 m, and the longest open run inside the land is 10 m.
  Ten islands is a small sample, so `kfSealOverlapMeters` below is a starting value that acceptance
  settles.

## Design

1. A region lays at most one seal group, in `GrowRegion`, before the first `LayGroups`. Route groups
   laid after it keep `kfGroupGapMeters` from it through the existing `Collides`, so nothing already
   placed needs moving. A cell belongs to one region and an edge between regions cannot be sealed, so
   a cell has at most one sealed edge and a flagship cannot be walled in on all six.
2. A new `common::RandomEngine` stream seeded with `kuiSealSeedMultiplier` makes the choices:
   - Region (0,0) always lays one, on an edge of cell (0,0). Every other region lays one at
     `kfSealChance` = 0.5, which gives about 40 sealed edges in radius 20, or about 1% of directed
     pairs, close to the 0.85% of blocked pairs in the approved layout's prototype.
   - Outside region (0,0) the cell is one of the region's four central cells, both anchor-relative
     axial coordinates in [-1, 0]. Each has all six neighbours in the region, and its edges sit far
     enough inside the padded rhombus that a member cannot leave it.
   - The direction is uniform over `common::kiHexDirectionCount`.
3. New `LaySealGroup` lays the members along the edge in one new group:
   - The line runs through the edge midpoint, half of the direction's `f2CenterOffset` from the
     cell's offset, along the perpendicular `NavQueryEdgeReachable` walks. It sits
     `kfSealSideOffsetMeters` = 5 toward the cell's centre, because a centre on an edge can pass
     `IsInsideCell` in no cell or in two, which would lose or double the island on the map.
   - Each member is Large, or Huge when a `kfMemberLargeChance` draw fails, posed by
     `OrientForTangent`.
   - The first hull's near end sits `kfSealOvershootMeters` = 30 before the edge's first vertex, past
     the 30 m worst shortfall measured above.
   - Each next member comes from `TouchPosition` with a gap of minus `kfSealOverlapMeters` = 60, twice
     that worst shortfall. `TouchPosition` gains an `fGap` parameter for this, and its other callers
     pass `kfTouchGapMeters`.
   - The group ends when the last hull's far end passes the second vertex by the overshoot, which
     takes 2 or 3 members.
   - Members go straight to `CommitPlacement`, because they overlap on purpose and `Collides` rejects
     overlap inside a group. They are the region's first placements, so nothing else can collide, and
     an `ASSERT` on `IsInsideRegion` keeps the confinement invariant visible.
4. The walked edge needs no other code. A member crossing an edge overlaps both cells' margin
   hexagons, so `ListRegionPlacements` lists it in both, and the `static_assert` in `TerrainUtils.h`
   keeps inflated polygons inside that margin, so each side's graph holds the land and the refusal
   holds in both directions.
5. Bump the `Frame::kiVersion` base literal, under the generator rule in
   `Engine/Source/Frame/AGENTS.md`.

If an edge a seal group was laid on still reports reachable, raise `kfSealOverlapMeters`. A leak
leaves a real gap that ships really use, so it never breaks what the player sees, but it does cost
the origin edge the live refusal needs.

Alternatives rejected: a harness write to `Fleet::wantedCoordinate`, because
`.agents/references/change-workflow.md` Step 5 forbids a new harness command only to observe a path,
and a write outside `FrameInput` would also break the fleet checksum in a recording. A generation
time check of land along the edge adds a second closure rule for no gain, given that a leak is
harmless.

## Critical files

- `Engine/Source/Frame/IslandChainPlacement.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`
- `Engine/Source/Frame/AGENTS.md`

Read only: `Engine/Source/Frame/NavQuery.cpp`,
`Projects/BrokenEngineSandbox/Source/Network/Server/FleetNavigationController.cpp`.

## In scope

- `Engine/Source/Frame/IslandChainPlacement.cpp`: `kuiSealSeedMultiplier`, `kfSealChance`,
  `kfSealSideOffsetMeters`, `kfSealOvershootMeters`, `kfSealOverlapMeters`, new `LaySealGroup`, the
  `fGap` parameter of `TouchPosition` and its two callers, and the call in `GrowRegion`.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`: the `Frame::kiVersion` base literal.
- `Engine/Source/Frame/AGENTS.md` `## Terrain and Navigation`: state that a region may seal one edge
  with a deliberate group, and that region (0,0) always seals an edge of cell (0,0).

## Out of scope

- `kfOpenStretchMeters`, or any other change to `NavQueryEdgeReachable` or
  `FleetNavigationController.cpp`.
- A harness write to `Fleet::wantedCoordinate`, any change to `UpdateFleet`, and any new agent
  command or probe field.
- More than one seal group per region, sealing an edge between two regions, and ringing a seal group
  with `SurroundIsland`.
- Stored per-edge data, any `NavData`, `IslandPlacement`, or wire layout change, `kiNavDataVersion`,
  and `kiProtocolVersion`.
- Re-baking islands, and any change to `kiIslandScalePermille` or to the route, group, or filler
  constants.
- Replacing the rectangle test in `ComputeFleetSpawnCandidates`, unless the fresh-start criterion
  fails.

## Risk triggers and invariants

Tier 3. Trigger: determinism and save and replay compatibility, because the change alters
`GenerateIslandChain` output, which feeds `placements.crc`, `elevation.crc`, and every position in
the cell, and it moves `Frame::kiVersion`.

- Determinism: float math under `/fp:strict`, trigonometry only through
  `common::DeterministicSinCos`, and the tangent through `DeterministicAngle`.
  `GenerateIslandChain` stays a pure function of coordinate and templates, in any order, on both
  endpoints.
- Saves and replays: grid saves store no placements and regenerate them, so the `Frame::kiVersion`
  move rejects older saves and replays. `IslandPlacement` layout and the wire are unchanged.
- Bounds: `kiMaximumIslandsPerCell` keeps its existing `ASSERT`. Every centre passes `IsInsideCell`
  in exactly one cell, to rounding, which `kfSealSideOffsetMeters` preserves.
- Threading and allocation: generation stays on the main thread under the existing allocation rules.
- Build: no new file, so no project-membership change. No shader or `.pack` change.

## Acceptance criteria

No recorded run has reproduced a refusal, so the live setup below is unproven. What was recorded is
the sweep (`cell_coordinate_probe {"coord":[q,r],"navigation":{"graphs":[4],"points":[]}}` over each
cell) and the replay order of criterion 5. Graph 4 is the Battleship graph at this writing; confirm
it through `NavigationGraphIndex` first, because `graphs` carries no ship-type mapping.

1. Sealed edges: a radius 20 server sweep finds at least 20 directed pairs with `reachableEdges`
   false. Each one's reverse pair is also false, and a second probe with `points` every 5 m along
   that edge shows no open run of 20 m or more on that graph.
2. Origin seal: exactly one direction of cell (0,0) reports false, the unedited tree has none, and
   the same probe confirms it.
3. Visible match: on each pair from criterion 1, the same `points` show no run of 20 m or more with
   `elevation` at or below 0. A `/agent-harness` screenshot of the origin seal and of the map shows
   one continuous landmass along that edge.
4. Regression, as the region chain change ran them:
   - radius 12 sweeps give equal `placements.crc` and `elevation.crc` on server and client;
   - a reversed sweep on a fresh server process gives the same CRCs;
   - a radius 20 sweep finds 0 cells with no centred row and no list over `kiMaximumIslandsPerCell`;
   - each island more than 1 mm from every cell edge is centred in exactly one cell;
   - a fresh start connects and spawns a fleet in cell (0,0);
   - an `UpdateFleet` crossing at least 4 cells arrives, with no `A* found no path` and no terrain
     start overlap away from spawn.
5. Live refusal in recording and playback, through `/agent-harness`, with a client connected,
   following `.agents/skills/agent-harness/references/replay.md` `### Replay determinism acceptance`:
   1. Take k, the sealed direction of cell (0,0), from criterion 2.
   2. `set_log_level {"category":"Network","level":"Debug"}`, plus that recipe's baselines.
   3. `reset`, then wait for the `Spawned` line of the new fleet's Battleship. A new fleet sits at the
      origin with `wantedCoordinate` there and its timer under `navigationDelaySeconds`, and
      `ResetState` does not reseed the fleet RNG, so each reset gives fresh draws.
   4. `replay_record {"start":true}`, then inject one edge-coast Omni across the edge opposite k, as
      `SpawnOmni` with `eController` `RandomSpawn`, `transferLockTimer` 1, a position 96.4% of the way
      to that edge, and a speed of 33.33 m/s straight at it. This run form recorded a transfer in the
      earlier replay acceptance.
   5. Wait for the first draw. A `TickFleetTimers` line setting `WantedCoord` is a departure: stop
      recording and repeat from step 3, at most 30 times, since each draw refuses at 1 in 6. A
      `TickFleetTimers refused move` line naming Coord (0,0) and Direction k is the refusal.
   6. Keep recording through the next draw, about one delay later. Require that the fleet stayed in
      cell (0,0) for that delay and that no `TickFleetTimers` line set `WantedCoord` to the neighbour
      in direction k. Then stop recording.
   7. Finish the recipe. Playback reaches a new `End replay` marker, the refusal line repeats
      identically in playback, there is no new CRC, checksum, desync, or read-failure line, and a
      client CRC run after cancel shows no desync.
6. A code read shows the `Frame::kiVersion` base literal moved, and that `LaySealGroup` uses no
   `std::sin`, `std::cos`, `std::atan2`, or `std::hypot`.
7. `.agents/skills/agent-harness/references/model-viewer.md` `## Capture` returns code `ok` with the
   world background rendered, because ModelViewer compiles the generator.

## Notes

- Criterion 5 takes about 3 minutes of failed attempts on average, plus about 4 for the kept
  recording and its playback, so its harness brief needs a run budget above the 10-minute default.
- A cell's list can grow by a seal group; the radius 20 maximum on the unsealed tree was 28 rows,
  far under the cap.
- Fleet spawn: `ComputeFleetSpawnCandidates` rejects whole footprint rectangles, and a seal group on
  an origin edge adds 2 or 3 of them near the cell's rim. The search walks out from the centre, so
  it should be unaffected, and criterion 4 settles it.
- `commands-server.md`'s edge-coast north-east example `[28.87,16.67]` measures 33.34 m/s, which
  `inject_payload` rejects as over the Omni speed limit, so step 4 of criterion 5 uses 33.33 m/s.
- A `Render regressed` debug break at client connect is not this Plan's failure:
  `Documents/Plans/Engine/ClientRenderedTickRegression.md` owns it.
