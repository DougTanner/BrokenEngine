# Omnis - Armored Shielded Ships

Omnis are the armored, shielded ship type; fleet navigation, flagship identity, and the other shared ship rules are in `../Ships/AGENTS.md`. Collision, terrain-push, and pusher sizing all derive from the shared omni radius, so tuning size means changing that one constant.

## Behavior Invariants

- Omnis fire only blasters, at the kept attack target when it is visible, within `kfBlasterTargetRange` on the Omni's stored plane, and in line of sight (`../Ships/AGENTS.md` `## Ship Invariants`), else at the nearest such enemy, one flat shot per `kBlasterFireInterval`; a climbing Omni holds fire.
- A fleet-controlled Omni in its flagship's cell steers by its fleet's stance
  through the fleet tactic it shares with fleet Fighters
  (`../Ships/AGENTS.md` `## Fleet Coordination Invariants`) on the highest
  Omni plane: in escort it holds its own fixed slot `kfOmniEscortDistance` from the
  flagship. The fleet's non-dying Omnis in the cell's previous frame share the
  circle evenly in global-ID order, slot 0 due East, and the slots never turn.
  Weapons free it attacks its kept attack target (`../Ships/AGENTS.md`
  `## Ship Invariants`), ignoring enemies above the highest Omni plane, routes
  to it while it is hidden, and while it is in line of sight within
  `kfOmniCirclingDistance` of it in x/y circles it clockwise by chasing a point
  on that circle ahead of itself
  (`OmniCirclingPoint`), with no coordination among Omnis; it never
  circles its escort slot. An
  escort tick routes around terrain on its navigated plane's graph
  (`NextRouteWaypoint`) to the slot, or to the nearest clear point when the slot
  lies inside terrain, and accelerates along a damped arrival on that final
  waypoint, or at full acceleration toward an earlier route waypoint, instead of
  along the unit AI direction. The escort route query runs every escort tick,
  an exception to the shared path-query cadence (`../Ships/AGENTS.md`
  `## Fleet Navigation Invariants`). Outside its flagship's cell it keeps the
  shared fleet navigation. Outside transfer lock every tick writes its tactic's
  target plane into its orders, overriding a spawn plane and a `kUpdateShip`
  plane.
- A random-spawn Omni chases its kept attack target (`../Ships/AGENTS.md`
  `## Ship Invariants`), picked from itself in 3D among enemies on an Omni
  plane, circling it while it is in line of sight as a fleet Omni circles its
  target unless returning to an island, and, outside transfer lock, writes that
  enemy's plane into its orders; with no such enemy its target plane stays. A
  `kTargetFlagship` row (`../Ships/AGENTS.md`) whose enemy flagship is at or
  below the highest Omni plane keeps that flagship as its kept attack target
  instead and chases and circles it the same way, with no flee or return to an
  island.

## Fleet Health Invariants

- A fleet-controlled Omni never dies, a harness-injected one included: when
  collision, area, or beam damage takes its armor to 0 or below, it holds its
  armor at 0, sets `ShipFlags::kHealing`, and starts no explosion; it still
  marks itself for transfer when it leaves the cell. A random-spawn Omni
  explodes, and the random-spawn waves replace it.
- While `kHealing` is set an Omni fires nothing and escorts whatever its
  fleet's stance. On a tick it is within `kfOmniEscortArrivalDistance` in x/y of
  its slot, its armor and its shield each rise by `kfOmniHealRate` per second up
  to `kfOmniArmor` and `kfOmniShield`; both full clears `kHealing`, and it
  fires again in the same tick's Spawn phase. An Omni waiting at the nearest
  clear point to a slot inside terrain heals only when that point lies within
  the arrival distance of the slot itself. `kHealing` carries across a
  transfer. The shield also keeps its passive regeneration.

## Heal Beam

- The heal beam is client-only and outside the CRC. PostRender Update, after
  the heal step, adds the `pHealBeamAreaLights` area light on a row that healed
  this tick and still heals, and releases it on every other row and tick. That
  write of the client-only Interpolate handle is an exception, like the
  Fighter beam handle (`../Fighters/AGENTS.md`), to the rule that the
  position is the only Interpolate field PostRender Update writes. Interpolate
  Update draws the line from the flagship's previous-frame position, about one
  tick behind, to the Omni, its brightness pulsing as a pure function of the
  interpolated time; with no flagship in the previous frame the light keeps its
  last corners until PostRender Update releases it.

## State Boundaries

- Shared CRC excludes client animation time, the local records tracking each pusher, and server-assigned global identity. Some excluded state still participates in save or transfer.
- The simulation keeps `pNextBlasterFireTimes` in [0, `kBlasterFireInterval`], which `SpawnBlasters` asserts. `PostRead` rejects out-of-range rows on every Frame read.
- The simulation keeps the Interpolate `pDestroyedTimes` in [0, `kDestroyTime`]. The Interpolate `PostRead` rejects out-of-range rows on every Frame read.
- `OmnisPostRender::Spawn(SpawnInfo)` is the asserting overload of the engine bounds rule (`../../../../../../Engine/Source/Frame/AGENTS.md`): an omni outside its cell is a bug, so it asserts instead of refusing and always reports success.
