# Ships - Shared Ship Code

What the three ship collections — Fighters, Omnis, Battleships — share: the vocabulary (`ShipTypes.h`), the per-row behaviours each type's PostRender script calls (`ShipBehaviours`), the cross-type queries (`ShipQueries`), the fleet coordination fleet Fighters and Omnis steer by (`FleetCoordination`), and the ship status changes (`ShipStatusChanges`). This folder holds no collection; each type's own `AGENTS.md` documents its script.

## Ship Invariants

- Ships have no ship-to-ship collision or ram damage. Pushers provide horizontal
  separation within their 3D range between ships on the same flight-plane
  layer, including an X position
  correction for near-coincident XY; blaster collision and missile explosion
  damage still apply.
  The [engine pusher contract](../../../../../../Engine/Source/Frame/Collections/Pushers/AGENTS.md#unique-aspects)
  owns the layer, distance, correction, and shared-snapshot rules.
- Apply the pusher's X correction once to the current stored Interpolate
  position after terrain resolution, preserving Y/Z/W and stored/target planes.
  Tiny terrain intrusion from this correction is accepted; separation can
  progress across ticks rather than fully resolving a stack in one tick.
- A ship releases and clears its pusher when its natural explosion begins.
  Zero handles skip Sync and push queries; final removal and transfer retain
  ordinary owned-handle cleanup. Fleet Battleships follow their
  [fleet health rule](../Battleships/AGENTS.md#fleet-health-invariants) and
  fleet Omnis theirs (`../Omnis/AGENTS.md`).
- A row's CRC'd `ShipController` selects its script branch and its model. Fleet-controlled rows are the owned entities (`Frame::OwnedEntityCount`) and run the fleet navigation below, except a fleet Fighter or Omni in its flagship's cell (`../Fighters/AGENTS.md`, `../Omnis/AGENTS.md`); random-spawn rows, which the frame's random-spawn waves create, carry global ID 0.
- Stable global IDs provide lookup and transfer identity; flagship status is the `ShipOrders::kIsFlagship` bit, never a privileged SOA index. A fleet row follows its own flagship, found by `pFlagshipGlobalIds` in any of the three types.
- In PostRender Update a ship script reads other ship rows only from the previous frame, so the tuple order never hands it a row this tick has not updated yet; Omni and Battleship Spawn-phase weapon targeting and the Fighter beam in PostCollision read the current frame, after every ship's Update. Changing a read's source frame shifts CRC-visible behavior by one tick.
- Every ship type's `pNavigationDelays` stays within [0, 60] seconds, the range
  `IsNavigationDelayInRange` tests and every navigation-delay admission shares.
  Each type's PostRender `PostRead` rejects a non-finite or out-of-range
  navigation delay, an unknown controller, and a target flight plane the type
  disallows, and each Interpolate `PostRead` rejects a height outside the
  lowest-to-highest flight plane range and a stored flight plane that is neither
  `kuiNoFlightPlane` nor allowed for the type, on every Frame read.
- A ship's target flight plane lives in its orders, set at spawn and changed by
  `kUpdateShip`, by the Fighters and fleet Omnis PostRender Update tactics, and
  by the random-spawn Omni chase (`../Fighters/AGENTS.md`,
  `../Omnis/AGENTS.md`); each type flies only up to its own highest plane
  (`IsFlightPlaneAllowed`). Its current plane is stored in the shared
  Interpolate column `puiFlightPlanes`: the plane index while settled,
  `kuiNoFlightPlane` while climbing between planes. A genuine spawn stores its
  spawn plane and a transfer arrival carries the stored value.
- A ship navigates on one plane, its `NavigationFlightPlane` (`ShipBehaviours.h`): path queries and the terrain-contact start snap use that plane's graph, and terrain steering and avoidance read terrain relative to that plane's height, ignoring it on a plane without terrain collision.
- Interpolate Update owns a ship's height and stored plane:
  `ClimbTowardsFlightPlane` lands on the plane table's own height, so client and
  server match bit-exactly, and stores the target plane on arrival and
  `kuiNoFlightPlane` once a climb toward another plane begins.
  PostRender Update stores velocity Z as 0 as its last velocity write in every
  type, so pushers, knockback, and terrain contact move ships only horizontally.
  Change a ship's height through its target plane, not its velocity.
- Headings stay horizontal: a direction that can aim at another plane, such as the Omni wanted direction or the random-wave heading, is flattened before it becomes or turns a ship's heading.
- Target choice treats a ship as on its nearest flight plane
  (`NearestFlightPlane`), while collision and hits stay normal 3D. Battleship
  missiles and Omni blasters target only an enemy whose nearest plane is the
  shooter's stored plane and stay on the shooter's plane, hitting a climbing
  target only by 3D contact: a missile aims along the horizontal offset to its
  target, and an Omni bolt flies flat along the Omni's hull. The Fighter beam
  has its own plane rule (`../Fighters/AGENTS.md`). A ship storing
  `kuiNoFlightPlane` fires none of them, and a Battleship launches missiles only
  while exactly at its stored plane's height. No Omni targets, chases, or faces
  an enemy above its highest plane. These plane rules govern only target choice
  and aim; blaster and missile collision and explosion damage ignore planes, and
  pushers follow their height-derived layer rule above.

## Fleet Coordination Invariants

- A fleet's stance is recomputed every tick from the cell's own previous frame
  and never stored: weapons free while an enemy of its flagship is within the
  `kfWeaponsFreeDistanceSquared` x/y distance of the flagship
  (`FleetCoordination.cpp`), escort otherwise. Only a flagship row in the cell
  gives its fleet a stance and position; enemies in other cells are not seen.
- Fleet Fighters and fleet Omnis steer by the stance through one shared tactic
  function keyed by ship type (`ChooseFleetTactic`): escort at the ship's own
  slot among its type's fleet rows, Attack and Veer-off while weapons free.
  Attack ignores enemies above the type's highest plane. Each type's `AGENTS.md`
  documents its slots.
- A weapons-free fleet Fighter or Omni not finishing a veer-off attacks its kept
  target, the enemy row in the previous frame whose registry id its
  `pRegistryTargets` stores (`FleetCoordination.cpp`). When the id is zero or
  names no enemy row at or below its type's highest plane there, it picks and
  stores the enemy nearest its flagship in x/y among those, ties keeping the
  first row in `ForEachShipType` order, so fleet ships of one type picking on
  the same tick share one target. Escort and veer-off leave the id untouched, so
  a target that died or left keeps its stale id until the next pick; in Update
  only leaving the flagship's cell clears it. Registry ids come from a
  frame-wide counter (`MakeUuid`,
  `../../../../../../Engine/Source/Frame/FrameBase.h`), so a stale id never
  names another row.

## Fleet Navigation Invariants

- After arrival grace expires, a living fleet Fighter or Battleship accelerates
  only when its horizontal AI bearing is within the inclusive +/-45-degree
  forward cone; a zero AI bearing is outside. Outside the cone it retains drag
  and uses its type's signed maximum turn rate, preserving that
  navigation turn through terrain bounce and skipping terrain avoidance.
  Terrain contact and pusher response still apply. Inside the cone it uses
  ordinary steering smoothing. This rule does not apply to Omnis or random-spawn
  ships.
- Navigation covers roaming, cross-cell movement, island destinations, and flagship following. Mode transitions depend only on shared state.
- Flagship following has no frame-local exit when the flagship row is not in
  the follower's cell, and the server never reassigns the flagship: a fleet's
  flagship is its Battleship, which never dies (`../Battleships/AGENTS.md`), so
  a follower stalls until that row is in its cell again or a fleet wanted cell
  other than its own sends it cross-cell.
- Path queries run on a deterministic cadence staggered by tick and stable global ID, with immediate recomputation for mode, destination, or direction changes.
- The cadence also breaks off-schedule when a navigation polygon contains either
  the lookahead point along the carried steering bearing or the current
  position. Both tests are needed: containment ahead does not imply containment
  at the position, and a ship already driven inside sits there as its steady
  state. Key this on nav-polygon containment, not a terrain-elevation probe —
  the polygon is that elevation contour inflated outward, so a ship stranded in
  the band between them measures clear terrain and never re-paths.
- Random draws, arrival checks, and mode transitions remain outside the
  pathfinding throttle. Modes 6 and 7 must consume the same number of random
  draws on any tick they can flip between, so mode 7 mirrors mode 6's three
  entry draws (island pick, footprint X, footprint Y) on every tick it runs. In
  a cell with no islands neither mode draws: mode 7 skips the mirror and mode 6
  navigates to the cell center instead of an island. Only the draw count matters
  — each `common::Random` advances the shared random stream once no matter what
  bound it is given — so mode 7's mirror draws are load-bearing, not dead code,
  even though it throws their results away.
- Spawning always consumes its cell-change random draw.
- Preserve cached steering and client debug waypoints across ticks where pathfinding is skipped.
