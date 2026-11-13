<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-12T23:51:25.302Z","dependsOn":[]} -->
# Stop a Fleeing Random-spawn Battleship Bouncing at the Shore

## Context

Originating gap: the change that routed ship flee, island return, and veer-off
goals around terrain had the acceptance criterion "a fleeing random-spawn
Battleship with an island on its away side stays clear of the island with no
bounce". The user kept that change's code and judged the criterion by its
wanted heading, which now follows the route; the shore bounces it did not
remove are recorded here.

Paths below are relative to
`Projects/BrokenEngineSandbox/Source/Frame/`; line numbers are from the session
that landed the route change, on top of `9f3923d4`.

Observed symptom. In the setup under `## Acceptance criteria`, the
random-spawn Battleship holds `ShipFlags::kFlee` for about 134 ticks after the
spawn. While it is set, its westward (away from the threat) velocity reverses
to eastward, back toward the threat, at the shore near local x -74 to -78 four
times (`ApplyTerrainBounce`, `Collections/Ships/ShipBehaviours.cpp:854`). Its
away progress stalls at 3.4 to 8.3 m and it slides north along the shore. The
same four reversals, at the same positions and velocities to two decimals,
occur before and after the route change. Only the stored wanted heading
(`pVecAiDirections`) differs between the two builds. A Battleship flees only
while within `kfFleeEndDistance` of its threat, so flee near land happens only
when it is already beside a shore.

Steering path, read from code. The random-spawn branch of
`BattleshipsPostRender::Update` aims at `FleeRouteWaypoint`
(`Collections/Battleships/Battleships.cpp:546-549`,
`Collections/Ships/ShipBehaviours.cpp:496-515`) while `kFlee` is set and
`kReturnToIslandCenter` is clear, turns toward it with `SteerHeading` (`:566`),
and ramps the result with `RampHeadingTurnRate` (`:584`). After the row loop,
`BattleshipsPostRender::AvoidTerrain` (`:605`,
`Collections/Battleships/BattleshipsNavigation.cpp:16-60`) runs for every row
not dying, not in arrival grace, not a fleet row outside its heading cone, and
not facing a near enemy in sight. It replaces the stored turn rate with
`ComputeTerrainAvoidance` (`TerrainUtils.cpp:90-139`), which near terrain
returns 0.995 times the current rate plus 0.005 times a wanted rate of
magnitude 16 to 32 (`:128-135`), then ramps that from the previous tick's rate.
That pull of at least 0.08 per tick exceeds the ramp limit of
`fTurnAcceleration` (0.25) times the 1/32 s tick, 0.0078125
(`Collections/Battleships/Battleships.h:39-51`), so wherever it acts the
avoidance side sets the turn direction.

Root cause: not confirmed. The diagnosis behind this Plan names that override
as the cause. The recorded samples do not separate it from two other
explanations:

- Turn limits. In those samples the stored turn rate steps by exactly the ramp
  limit each tick from the spawn and then holds at `-fMaxTurnRate` (-0.25).
  `SteerHeading` gives the same negative sign for both recorded wanted headings
  at the recorded heading, so steering alone would produce the same rate. The
  Battleship spawns 9 m from the shore facing it. At these turn limits it
  reaches the shore in about 1.3 s, well before it can turn 90 degrees.
- Waypoint toward the threat. At one sample under `kFlee` the stored wanted
  heading pointed toward the threat, (0.992, 0.130). One possible source is the
  inside-polygon escape in `engine::NavQueryDirection`
  (`Engine/Source/Frame/NavQuery.cpp:717-728`): from a start inside an inflated
  navigation polygon, the next waypoint is the nearest polygon edge, whatever
  the destination.

## Design

1. Confirm the root cause with `/external-diagnose-bug` in the setup under
   `## Acceptance criteria`. Settle all three explanations in `## Context`. For
   the override, compare a probe build that skips `AvoidTerrain` for the
   fleeing row against the unchanged build. For the turn limits, check whether
   any turn within `fMaxTurnRate` and `fTurnAcceleration` avoids each contact.
   For the waypoint, record `pVecAiDirections` and whether the position lies
   inside a navigation polygon.
2. Make the smallest fix inside `## In scope`. The author recommends, if the
   override is confirmed, that `BattleshipsPostRender::AvoidTerrain` skip a row
   while `kFlee` is set and `kReturnToIslandCenter` is clear. Rationale: that is
   exactly when the row steers at `FleeRouteWaypoint`, a route that already
   goes around terrain. It mirrors the existing skip for a fleet row outside
   its heading cone. It leaves `ComputeTerrainAvoidance` alone, which Fighters
   and the server launch check
   (`Network/Server/ServerClientManager.cpp:269`) share. Fighters have no flee,
   so the Fighter/Battleship parallel keeps its documented flee difference.
   Alternative for `/plan-alternatives`: have avoidance adjust the wanted
   heading instead of replacing the turn rate. It is not recommended, because
   it changes the shared `ComputeTerrainAvoidance` contract and every caller.
3. If the turn limits are confirmed, no steering change can avoid the first
   contact. A fix to the waypoint source in `NavQuery` lies outside the scope
   too. In either case surface the finding for re-planning instead of widening
   scope or weakening the acceptance criterion.
4. Bump the `Frame::kiVersion` base (`Frame.cpp`) by one from the value found
   at landing: the fix changes CRC'd steering without a collection bump.
5. Update `Collections/Battleships/AGENTS.md` for the new avoidance skip.
   `Collections/Ships/AGENTS.md` `## Fleet Navigation Invariants` documents
   avoidance skips; update it only if its wording becomes false.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsNavigation.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`

## In scope

- The root-cause investigation in `## Design` step 1.
- The fix region: the row skip conditions of
  `BattleshipsPostRender::AvoidTerrain`, and the random-spawn flee steering in
  `BattleshipsPostRender::Update` from the `FleeRouteWaypoint` override
  through its `SteerHeading` call.
- The `Frame::kiVersion` base, and the Battleships and Ships `AGENTS.md`
  wording that the fix makes false.

## Out of scope

- `ComputeTerrainAvoidance` and its constants, `RampHeadingTurnRate`,
  `SteerHeading`, `ApplyTerrainBounce`, and `kBattleshipMovement` turn limits.
- `FleeRouteWaypoint`, `NextRouteWaypoint`, `engine::NavQueryDirection`, and
  the navigation bake.
- Fighters, Omnis, fleet Battleships, and island-return steering.
- Flee start and end distances.

## Acceptance criteria

- The recorded symptom no longer reproduces. With the `/agent-harness` setup
  below, while `kFlee` is set, the Battleship's velocity never reverses from
  away from the threat to toward it at the shore.
- Setup, as the observing run recorded it, on a fresh Debug server: `pause`
  true; `timescale` faster false twice; `create_cell` [10,-13];
  `inject_payload` of a `SpawnBattleship` at [-70,70], direction [-1,0],
  velocity [0,0], alignment random, `RandomSpawn`, orders 0, and a
  `SpawnOmni` at [-58,70], direction [-1,0], velocity [0,0], alignment fleet,
  `Fleet`, orders 0, `transferLockTimer` 600; `pause` false. Then, until 320
  ticks pass, `status` and `query_collection` battleships and omnis each
  sample (about every 6 ticks), and after any sample where the fleet Omni is
  more than 0.25 m from [-58,70] or moves faster than 0.01 m/s horizontally,
  find its row with `read_frame` by `pIds` and `edit_frame` its
  `pVecPositions` to [-58,70,3] and `pVecVelocities` to [0,0,0]. Read
  `read_frame` battleships each sample for `pVecAiDirections`.
- With the same setup, the Battleship still stays at least as far from the
  threat as before and clears `kFlee` by distance hysteresis.

## Notes

- Tier 3. Trigger: determinism/CRC, because the fix changes CRC-visible
  Battleship steering.
- Exposure: Frame CRC and replay compatibility through the `Frame::kiVersion`
  bump; no serialization layout, wire, `.pack`, threading, allocation, shader,
  or build change.
- No directional prerequisites.
