# Omnis - Armored Shielded Ships

Omnis are the armored, shielded ship type; fleet navigation, flagship identity,
collision dimensions and the other shared ship rules are in
[the shared ship policy](../Ships/AGENTS.md#ship-invariants). Terrain-push sizing
derives from the shared omni radius.

## Behavior Invariants

- Omnis fire only blasters, at the kept attack target when it is visible, within `kfBlasterTargetRange` on the Omni's stored plane, and in line of sight (`../Ships/AGENTS.md` `## Ship Invariants`), else at the nearest such enemy, one flat shot per `kBlasterFireInterval`; a climbing Omni holds fire.
- A fleet-controlled Omni in its flagship's cell steers by its fleet's stance
  through the fleet tactic it shares with fleet Fighters
  (`../Ships/AGENTS.md` `## Fleet Coordination Invariants`): in escort the
  tactic gives it no destination and it patrols on the lowest plane
  (`../Ships/AGENTS.md` `## Fleet Navigation Invariants`): it picks a random point within
  `kfOmniPatrolRadius` of the flagship's previous-frame position in x/y,
  snapped clear of terrain, keeps it only when it lies `kfOmniPatrolCellInset`
  inside the cell, else draws again next tick, flies to it, and picks another on
  arrival. A stored point stays until arrival even when the flagship moves, or
  when it lies inside the flagship's navigation rectangle, which that query then
  leaves out (`../Ships/AGENTS.md` `## Ship Invariants`). A route around a
  rectangle can still pass the inset and transfer the Omni out.
  Weapons free it attacks its kept attack target (`../Ships/AGENTS.md`
  `## Ship Invariants`) on that enemy's nearest plane, ignoring enemies above
  the highest Omni plane, routes to it while it is hidden, and while it is in line of sight within
  `kfOmniCirclingDistance` of it in x/y circles it clockwise by chasing a point
  on that circle ahead of itself
  (`OmniCirclingPoint`), with no coordination among Omnis. Outside its
  flagship's cell it keeps the shared fleet navigation on the lowest plane.
  Outside transfer lock every tick writes its tactic's target plane into its
  orders, overriding a spawn plane and a `kUpdateShip` plane.
- A random-spawn Omni chases its kept attack target (`../Ships/AGENTS.md`
  `## Ship Invariants`), picked from itself in 3D among enemies on an Omni
  plane, circling it while it is in line of sight as a fleet Omni circles its
  target unless returning to an island, and, outside transfer lock, writes that
  enemy's plane into its orders; with no such enemy it targets the lowest plane. A
  `kTargetFlagship` row (`../Ships/AGENTS.md`) whose enemy flagship is at or
  below the highest Omni plane keeps that flagship as its kept attack target
  instead and chases and circles it the same way, with no flee or return to an
  island.
- An Omni enters a cell, by spawn or transfer arrival, with its heading as its
  AI direction and `ShipFlags::kNoAiDirection` set (`../Ships/AGENTS.md`
  `## Ship Invariants`). The first tick outside transfer lock that gives it a
  bearing clears the flag for good: any fleet tick, a flagship chase, or a
  random-spawn tick with a destination. A later random-spawn tick with no
  destination keeps the previous AI direction.

## Fleet Health Invariants

- A fleet-controlled Omni survives combat damage, a harness-injected one included: when
  collision, area, or beam damage takes its armor to 0 or below, it holds its
  armor at 0, sets `ShipFlags::kHealing`, and starts no explosion; it still
  marks itself for transfer when it leaves the cell. A random-spawn Omni
  explodes, and the random-spawn waves replace it. Explicit destruction follows
  the shared destroy-status contract (`../Ships/AGENTS.md` `## Ship Invariants`).
- While `kHealing` is set an Omni fires nothing and patrols whatever its
  fleet's stance. On a tick outside transfer lock on which its flagship is in
  the cell and it is within `kfOmniPatrolRadius` of that flagship in x/y, its
  armor and its shield each rise by `kfOmniHealRate` per second up to
  `kfOmniArmor` and `kfOmniShield`; both full clears `kHealing`, and it fires
  again in the same tick's Spawn phase. `kHealing` carries across a transfer.
  The shield also keeps its passive regeneration.

## Heal Beam

- The heal beam is client-only and outside the CRC. PostRender Update, after
  the heal step, adds the `pHealBeamAreaLights` area light on a row that healed
  this tick and still heals, and releases it on every other row and tick. That
  write of the client-only Interpolate handle is an exception, like the
  Fighter beam handle (`../Fighters/AGENTS.md`), to the PostRender writes of
  Interpolate position and stored plane under the shared ship policy. Interpolate
  Update draws the line from the flagship's previous-frame position, about one
  tick behind, to the Omni, its brightness pulsing as a pure function of the
  interpolated time; with no flagship in the previous frame the light keeps its
  last corners until PostRender Update releases it.

## State Boundaries

- Shared CRC excludes client animation time, the local records tracking each pusher, and server-assigned global identity. Some excluded state still participates in save or transfer.
- The simulation keeps `pNextBlasterFireTimes` in [0, `kBlasterFireInterval`], which `SpawnBlasters` asserts. `PostRead` rejects out-of-range rows on every Frame read.
- The simulation keeps the Interpolate `pDestroyedTimes` in [0, `kDestroyTime`]. The Interpolate `PostRead` rejects out-of-range rows on every Frame read.
- `OmnisPostRender::Spawn(SpawnInfo)` is the asserting overload of the engine bounds rule (`../../../../../../Engine/Source/Frame/AGENTS.md`): an omni outside its cell is a bug, so it asserts instead of refusing and always reports success.
