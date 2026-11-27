# Ships - Shared Ship Code

What the three ship collections — Fighters, Omnis, Battleships — share: the
vocabulary (`ShipTypes.h`), the per-row behaviours each type's PostRender script
calls (`ShipBehaviours`), the fleet navigation (`ShipNavigationUtils`), the
cross-type queries (`ShipQueries`), the fleet coordination fleet Fighters and
Omnis steer by (`FleetCoordination`), the ship status changes
(`ShipStatusChanges`), the collision hitboxes (`ShipHitboxes`), and the
client-only model of each type and controller (`ShipModels`). This folder holds
no collection; each type's own `AGENTS.md` documents its script. The ramped
heading turn in `ShipBehaviours` follows the
[turning rule](#turning-invariants).

## Ship Invariants

- Ships have no ship-to-ship collision or ram damage; blaster collision and
  missile explosion damage still apply. Pushers provide separation under the
  [engine pusher contract](../../../../../../Engine/Source/Frame/Collections/Pushers/AGENTS.md#unique-aspects),
  which owns the distance, correction, and shared-snapshot rules.
- Apply the pusher's X correction once to the current stored Interpolate
  position after terrain resolution, preserving Y/Z/W and target orders, then
  enforce the live terrain-rise rule below. For ships without that rule, tiny
  terrain intrusion from the correction is accepted. Separation can progress
  across ticks rather than fully resolving a stack in one tick.
- A ship releases and clears its pusher when its explosion begins. Fighter and
  Omni `kDestroyShip` status changes use the same initializer as combat death,
  ignore an already dying row, and retain the existing delayed removal; an
  explicit Fighter destruction has zero damage direction. Battleship destroy
  status changes retain immediate removal.
  Zero handles skip Sync and push queries; final removal and transfer retain
  ordinary owned-handle cleanup. Fleet Battleships follow their
  [fleet health rule](../Battleships/AGENTS.md#fleet-health-invariants) and
  fleet Omnis theirs (`../Omnis/AGENTS.md`).
- A ship's collision and missile-damage shape uses its model's already aligned
  baked `common::ModelOutline` directly, at the depth its sidecar hitbox setting
  names: the box and radius come from its convex hull and the outline level from
  its outline polygon, so an outline inset moves neither radius nor pusher. Ship
  models require uniform authored scale. `kShipHitboxTypes` must name each
  controller's Render model. Each ship PreCollision writes every row's radius
  and posed hitbox from its current pose
  (`../../../../../../Engine/Source/Frame/AGENTS.md` `## Architecture`), and
  each type's Render draws it in debug rendering; the hitbox ignores roll,
  climb tilt, and death shrink. Only the row's own cell tests its hitbox, so a
  hull part past the cell edge is never hit from the neighbour cell; this is
  accepted by design. A row's pusher radius is its hitbox sphere radius times
  its type's `kShipHitboxTypes` pusher radius scale. Each `kShipHitboxTypes`
  model's Z range also contains its local origin, which the bake keeps inside
  its outline and so its box: `ShipHullContact`'s center-directed ray asserts
  that it meets that prism, and the bake tests the origin against the outline
  only (`../../../../../../DataPacker/Source/ExportJobs/AGENTS.md`
  `## Model Alignment`), so a sidecar `offset` must not move the Z range off it.
- Shared startup caches each controller model's minimum origin height from its
  aligned upright geometry, keeping the hull at least 1 meter above water. A
  ship's effective flight height (`EffectiveShipFlightHeight`) is the maximum
  of that minimum and `kfBaseHeightMeters`; bank and pitch do not affect it.
  Genuine spawns apply this height once before initializing owned objects;
  transfers preserve their carried position.
- A row's CRC'd `ShipController` selects its script branch and its model.
  Fleet-controlled rows are the owned entities (`Frame::OwnedEntityCount`) and
  run the fleet navigation below, except a fleet Fighter in its flagship's cell
  or in Seek and Destroy, and a fleet Omni in its flagship's cell on a tick its
  tactic gives a destination (`../Fighters/AGENTS.md`, `../Omnis/AGENTS.md`);
  random-spawn rows, which the frame's mirror-fleet waves create
  (`../../AGENTS.md`), carry global ID 0, and a transfer arrival does not keep
  a row's mirror-fleet membership.
- A random-spawn row whose orders carry `ShipOrders::kTargetFlagship`, won by
  its spawn coin toss (`../../AGENTS.md`), targets the enemy flagship: on a tick
  its cell's previous frame holds an enemy `kIsFlagship` Battleship row past
  arrival grace, the 3D-nearest one becomes its kept attack target (below) in
  place of `FindAttackTarget`'s pick, even while it is hidden, and the row
  chases it as each type document describes (`../Fighters/AGENTS.md`,
  `../Omnis/AGENTS.md`, `../Battleships/AGENTS.md`). With no such flagship the
  row steers as an unflagged one that tick. The bit stays and transfers with the
  row. A random-spawn Battleship follows this flagship rule whatever the bit.
- Stable global IDs provide lookup and transfer identity; flagship status is the
  `ShipOrders::kIsFlagship` bit, never a privileged SOA index. A fleet row
  follows its own flagship, found by `pFlagshipGlobalIds` in any of the three
  types.
- A fleet Fighter or Omni row's spawn sets `ShipOrders::kSquadron1` from its
  member's squadron and its squadron setting bits from its squadron's slot
  (`Fleet::pSquadronOrders`, `../../../Fleet.h`;
  `../../../Network/Server/AGENTS.md` `## State Ownership`): `kSeekAndDestroy`
  and `kSharedTarget` on a Fighter (`../Fighters/AGENTS.md`), `kPatrolCell` and
  `kPatrolTogether` on an Omni (`../Omnis/AGENTS.md`). A transfer carries them,
  and afterwards only `kUpdateSquadronOrders` changes the setting bits.
- In PostRender Update a ship script reads other ship rows only from the
  previous frame, so the tuple order never hands it a row this tick has not
  updated yet, Omni weapon targeting included; Battleship Spawn-phase weapon
  targeting and the Fighter beam in PostCollision read the current frame, after
  every ship's Update. Changing a read's source frame shifts CRC-visible
  behavior by one tick.
- Each ship type's PostRender `PostRead` rejects an unknown controller, a
  nonzero `pClientGuids` or `pGlobalIds` on a row that is not
  `ShipController::kFleet`, and an AI direction that fails
  `IsShipHeadingValid`; Interpolate `PostRead` rejects a heading that fails
  `IsShipHeadingValid`. On build-local `Frame` reads (`ValidateShipKinematics`,
  `Frame.cpp`), every ship row's horizontal position and velocity must be
  finite, Omni and Battleship heights must equal their controller's
  `EffectiveShipFlightHeight`, Fighter heights must be finite and at or above
  it (allowing terrain-raised Fighters), and every ship row's velocity Z must
  be zero.
- Path queries and the terrain-contact start snap use the ship type's clearance
  tier graph (`ShipTypeClearanceTier`, looked up via `NavigationTierData`),
  while `ComputeAiSteering` compares raw cell-elevation samples and
  `ComputeTerrainAvoidance` compares weighted averages of raw cell elevations
  against their steering thresholds.
- Path queries route around large ships. `NavRectangleBoard`, built once per
  ship type's PostRender Update, holds one rectangle per previous-frame row
  `IsNavRectangleRow` accepts, so a navigation-obstacle ship's rectangle exists
  exactly while its pusher does. A rectangle is the row's box inflated by the
  asking ship's clearance, posed at the row's previous-frame position, under the
  engine's navigation rectangle contract
  (`../../../../../../Engine/Source/Frame/AGENTS.md`
  `## Terrain and Navigation`). A way round cheaper than the crossing penalty
  wins; otherwise the ship crosses and pushers separate them. A ship can also
  cross that way when it or its target sits in the margin between a model box
  and its rectangle: line of sight can read the target as hidden while the
  route, without the rectangle `NavRectangleBoard::Select` left out, ignores
  that hull.
- A query sees only the rectangles `NavRectangleBoard::Select` picks, so no
  exception code exists for a ship's own rectangle, a flagship, or an attack
  target. Pushers move a ship out of a box it starts in. A stored destination is
  never snapped out of a box, because a point left beside a ship that moves on
  might never be reached.
- Only path queries (`ComputeNavigation`, `NextRouteWaypoint`) see rectangles,
  except the veer-off side test (`AttackOrStartVeerOff`,
  `../Fighters/AGENTS.md`). Veer-off (`FleeRouteWaypoint`) and return to an
  island (`ComputeReturnDestination`) steer at a `NextRouteWaypoint` every tick,
  as a hidden target's route does. In-sight attack, chase, and Omni circling
  steering, roaming, escort, and terrain snaps all ignore rectangles, and a ship
  in a neighbouring cell makes none.
- Line of sight reads no rectangle. `HasShipLineOfSight` tests the unmodified
  model box of every `IsNavRectangleRow` Battleship in the frame its caller
  reads. That Battleship-only walk matches the rows `NavRectangleBoard` holds
  only while `navigationObstacle` stays on the two Battleship models.
- A live Fighter rises automatically above terrain (`RaiseAboveTerrain`), and
  in Interpolate Update steps Z back toward `EffectiveShipFlightHeight` at
  `kfFighterClimbRate` via `StepHeightTowards` so client and server land on it
  bit-exactly. Apply `RaiseAboveTerrain` after that descent step and after the
  final PostRender pusher correction, without a rise-rate ceiling, preserving
  XY/W, target orders, and the effective model-origin floor. Live Fighters
  bypass horizontal terrain push and contact. Omnis and Battleships stay at
  their spawn height with no terrain rise. PostRender Update stores velocity Z
  as 0 as its last velocity write in every type. Height changes step position
  directly, never through velocity.
- Ships taking horizontal terrain contact in PostRender Update sweep their disc
  from the previous to the current Interpolate position against the cell's
  elevation grid, blocked at the current position's height, and write the
  resolved position back to the current Interpolate row. Live Omnis take
  terrain push, pusher push, and `ApplyTerrainBounce`; dying Omnis take
  terrain push, pusher push, and `ResolveTerrainContact` (sliding once); live
  Battleships and dying Fighters bounce via `ApplyTerrainBounce`; dying
  Battleships skip contact (`../Battleships/AGENTS.md`). A start already inside
  terrain logs a warning; spawn sites test at most the body's center against
  terrain, so a warning right after a spawn is expected.
- Headings stay horizontal: a 3D direction, such as the Omni wanted direction or
  the random-wave heading, is flattened before it becomes or turns a ship's
  heading. Every simulation write and every ship spawn stores a horizontal unit
  vector in `pVecAiDirections`, and spawn stores the heading; the agent
  `edit_frame` command writes raw values, bounded only by the read check
  (`IsShipHeadingValid`). An Omni row carrying `ShipFlags::kNoAiDirection` has
  no AI direction it wants, so the Omni thrust ignores it; the
  [turning rule](#turning-invariants) covers Fighter and Battleship rows.
- All three weapons fire only at an enemy in line of sight
  (`HasShipLineOfSight`): the Omni blaster and Fighter beam from the shooter's
  origin, and a Battleship missile from the launching tube
  (`../Battleships/AGENTS.md` `## Weapon Invariants`). The test is 2D against
  the Battleship model boxes above and tier 0's inflated navigation polygons
  for every shooter (`kiFiringClearanceTier`), so it blocks slightly early near
  terrain. An Omni faces only an enemy in line of sight, its bolt flies flat
  along its hull, and a missile aims along the horizontal offset to its target
  (flight behavior: [Missiles Invariants](../Missiles/AGENTS.md#invariants)).
- Every Fighter, Omni, and random-spawn Battleship keeps one attack target, the
  enemy row in the previous frame whose registry id its `pRegistryTargets`
  stores, chosen by `FindAttackTarget` (`ShipQueries.h`) or, for a
  `kTargetFlagship` row or a random-spawn Battleship, the flagship rule above.
  A fleet ship picks from its flagship (`## Fleet Coordination Invariants`), a
  random-spawn ship from itself as its type's `AGENTS.md` states, except where
  a fleet Fighter's squadron settings pick for it (`../Fighters/AGENTS.md`). In
  `FindAttackTarget`'s pick an enemy in line of sight always wins: a hidden
  kept target is kept, and routed to, only while no enemy is in sight, except
  under `FindAttackTarget`'s `AttackTargetFlags::kIgnoreLineOfSight`, used only
  for those squadron settings. The pick and the imposed-target chases (flagship
  and squadron settings) test `HasShipLineOfSight`, a chase on the ship's own
  clearance tier. Movement decisions after a pick retest terrain only
  (`HasLineOfSight`) on the ship's own clearance tier, which can hide a target
  the tier-0 pick saw; the box test does not depend on the tier. While the
  target is hidden the ship steers at its `NextRouteWaypoint` every tick, an
  exception to the path-query cadence (`## Fleet Navigation Invariants`), and
  starts no veer-off; a random-spawn Fighter or Omni returning to an island
  skips that route. Fleet Battleships pick none.

## Fleet Coordination Invariants

- A fleet's stance is recomputed every tick from the cell's own previous frame
  and never stored: weapons free while an enemy of its flagship is within the
  `kfWeaponsFreeDistanceSquared` x/y distance of the flagship
  (`FleetCoordination.cpp`), escort otherwise. Only a flagship row in the cell
  gives its fleet a stance and position; enemies in other cells are not seen.
- Patrol fleet Fighters and fleet Omnis steer by the stance through one shared
  tactic function keyed by ship type (`ChooseFleetTactic`): escort at a
  Fighter's own slot among the fleet's Fighters, and no destination for an
  Omni, which then patrols (`../Omnis/AGENTS.md`); while weapons free, Attack,
  which an Omni's script turns into circling the target (`../Omnis/AGENTS.md`).
  A Patrol fleet Fighter calls it through `ChooseFighterFleetTactic`, which adds
  Veer-off. `../Fighters/AGENTS.md` documents the slots and Seek and Destroy.
- A weapons-free fleet Omni, or Patrol fleet Fighter not finishing a veer-off,
  attacks its kept attack target (`## Ship Invariants`), picked from its
  flagship in x/y; a Same Target Fighter instead takes its squadron's shared
  target first (`../Fighters/AGENTS.md`). Escort and veer-off leave the id
  untouched, except for a Fighter's squadron-setting picks
  (`../Fighters/AGENTS.md`), so a target that died or left keeps its stale id
  until the next pick; in Update only leaving the flagship's cell clears it.
  Registry ids come from a frame-wide counter (`MakeUuid`,
  `../../../../../../Engine/Source/Frame/FrameBase.h`), so a stale id never
  names another row.

## Fleet Navigation Invariants

- After arrival grace expires, a living fleet Fighter or Battleship accelerates
  only when its horizontal AI bearing is within the inclusive +/-45-degree
  forward cone. Outside the cone it retains drag and skips terrain avoidance;
  the shared terrain response and pusher response still apply. This rule does
  not apply to Omnis or random-spawn ships; every Fighter and Battleship turns
  as `## Turning Invariants` states.
- Navigation covers roaming, cross-cell movement, island destinations, flagship
  following, and a fleet Omni's patrol around its flagship
  (`../Omnis/AGENTS.md`). The patrol runs in the island-destination mode (mode 6)
  and, on a tick the Omni's flagship is in its cell, replaces the fleet
  override, flagship following, and the cell-change timer. Mode transitions
  depend only on shared state.
- A Patrol Cell Omni (`kPatrolCell`, `## Ship Invariants`) picks a random
  point on a random centred island instead of a point around its flagship, or,
  in a cell with no centred island, a point uniform within
  `engine::kfCellHalfWidth` of the cell center; either pick is snapped and kept
  only inside `kfOmniPatrolCellInset`, else dropped as a patrol pick is. With
  `kPatrolTogether` too, its squadron's leader is the lowest-global-ID
  non-dying, non-healing Omni in the previous frame with the same flagship,
  `kSquadron1`, and both setting bits; the leader picks, and every other such
  row takes the leader's previous-frame destination whenever it is set and
  differs from its own, recomputing its path on that change, and skips the
  blocked-destination snap and the arrival reset.
- Flagship following has no frame-local exit when the flagship row is not in
  the follower's cell, and the server never reassigns the flagship: a fleet's
  flagship is its Battleship, which never dies (`../Battleships/AGENTS.md`), so
  a follower stalls until that row is in its cell again or a fleet wanted cell
  other than its own sends it cross-cell.
- Path queries run on a deterministic cadence staggered by tick and stable
  global ID, with immediate recomputation for mode or destination changes. A
  tick a fleet Omni's patrol pick (`../Omnis/AGENTS.md`) or an island
  destination is dropped, or a Patrol Together follower and its leader both
  have no destination, keeps the cached steering with no recompute.
- The cadence also breaks off-schedule when a navigation polygon contains either
  the lookahead point along the carried steering bearing or the current
  position. Both tests are needed: containment ahead does not imply containment
  at the position, and a ship already driven inside sits there as its steady
  state. Key this on nav-polygon containment, not a terrain-elevation probe —
  the polygon is that elevation contour inflated outward, so a ship stranded in
  the band between them measures clear terrain and never re-paths. A navigation
  rectangle (`## Ship Invariants`) never breaks the cadence, so its reach must
  stay above what the two ships can close between recomputes.
- Random draws, arrival checks, and mode transitions remain outside the
  pathfinding throttle. Modes 6 and 7 must consume the same number of random
  draws on any tick they can flip between, so mode 7 mirrors mode 6's three
  entry draws (island pick, footprint X, footprint Y) on every tick it runs. In
  a cell with no centred islands neither mode draws: mode 7 skips the mirror and
  mode 6 navigates to the cell center instead of an island. A fleet Omni's
  Escort patrol pick instead draws two values per attempt in any cell, a Patrol
  Cell pick three on a centred island and two otherwise, and a Patrol Together
  follower none; a fleet Omni never enters mode 7, so none of its ticks can
  flip between the modes. Only the draw count matters — each `common::Random`
  advances the shared random stream once no matter what bound it is given — so
  mode 7's mirror draws are load-bearing, not dead code, even though it throws
  their results away.
- Every ship's island decisions, destination picks and returns alike, read the
  cell's `CentredIslands`, never its full list
  (`../../../../../../Engine/Source/Frame/AGENTS.md`
  `## Terrain and Navigation`), so no ship heads for an island centred in
  another cell. A centred island's footprint can still pass the cell edge and a
  navigation snap can push a point past it, so a fresh island destination, or a
  stored one snapped when its clearance tier's polygons cover it, that fails
  `IsInsideCell` after the snap is dropped: the destination clears, and the next
  tick draws again. A dropped largest or smallest pick still advances to the
  next waypoint.
- Spawning always consumes its cell-change random draw.
- Preserve cached steering and client debug waypoints across ticks where
  pathfinding is skipped.

## Turning Invariants

- Every Fighter and Battleship turns at one maximum rate, its type's
  `kFighterMovement` or `kBattleshipMovement` `fMaxTurnRate`. Each tick
  PostRender Update moves the turn rate toward a wanted rate by at most its
  `fTurnAcceleration` times the tick time (`RampHeadingTurnRate`), and terrain
  bounce and terrain avoidance pass through that same ramp instead of setting
  the rate directly. The wanted rate is zero within the heading capture angle
  of the wanted heading.
- Every row stores its horizontal wanted heading in `pVecAiDirections`: fleet
  rows their navigation or fleet tactic bearing, random-spawn rows the
  normalized horizontal direction to their destination. Without a destination a
  random-spawn row, or an idle Seek and Destroy fleet Fighter
  (`../Fighters/AGENTS.md`), keeps its previous wanted heading with
  `ShipFlags::kNoAiDirection` set, and its wanted rate is zero; the flag is
  clear while it has one. The idle fleet Fighter's kept wanted heading still
  gates its terrain avoidance (`## Fleet Navigation Invariants`).
- Interpolate Update, while the previous turn rate is exactly 0, the previous
  row does not carry `ShipFlags::kNoAiDirection`, and the previous wanted
  heading is within the capture angle, rotates the direction a fixed percent
  toward it each tick.
