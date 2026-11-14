# Battleships - Missile-Armed Heavy Ships

Battleships are a copy of Fighters with larger, slower constants
(`Battleships.h`): every Fighters invariant (`../Fighters/AGENTS.md`) holds with
the Battleship names. Keep the two collections parallel; only the tube missiles,
fleet health, and death below and the Fighters' beam, client-only climb tilt
and turn roll, fleet tactics, `pVecVeerOffPoints`, and flagship chase differ: a
random-spawn Battleship chases an enemy flagship under the flagship rule
(`../Ships/AGENTS.md` `## Ship Invariants`), else the kept attack target
`FindAttackTarget` picks from itself in 3D at or below plane 0, clearing the
return-to-island state while it chases; only with no enemy does it return to an
island, under distance hysteresis, and in a cell with no island candidates it
then clears the return-to-island state and ramps its turn rate to zero. Its
missiles keep the rule below, which already covers the flagship. A dying
Battleship has none of the Fighters' death knockback, shrink, or staged
explosions: it holds still, skips terrain contact, and the client draws its
model as a wreck, which emits its death explosions (`## Wreck Invariants`).

## Weapon Invariants

- Homing tube missiles replace the Fighter's beam: the PostRender Spawn hook
  only launches them, in one sequential pass over the rows in ascending order.
  A row launches when it is not dying, its arrival grace has run out, the
  shared plane and height rule allows it (`../Ships/AGENTS.md`
  `## Ship Invariants`), and its `pNextMissileFireTimes` cooldown is at or
  below zero. A launch resets the cooldown to `kBattleshipMissileCooldown`,
  and the cooldown transfers with the ship, so launches come exactly
  `kBattleshipMissileCooldown` apart. The simulation keeps the cooldown finite
  and at most `kBattleshipMissileCooldown`, with no lower clamp; `PostRead`
  rejects any other row on every Frame read.
- A launching row launches at most one missile per tick, choosing among every
  (enemy, tube) pair the one nearest by 3D distance from the tube, among pairs
  whose enemy is within `kfBattleshipMissileRange` of that tube, passes the
  shared plane and line-of-sight targeting rules taken from that tube
  (`../Ships/AGENTS.md`), and is targeted by fewer missiles than its type's
  `kiMissilesPerTarget` cap; an exact distance tie keeps the first enemy in
  `ForEachEnemyShip` order, then its first authored tube. Missiles do not
  prefer the kept attack target. The count covers every missile targeting that
  enemy, other Battleships' missiles included, and every launch made earlier in
  the same tick, so a later shooter sees an earlier shooter's launches. Each
  missile receives that enemy as its target at launch, with no registry
  acquisition (`../Missiles/AGENTS.md`).
- Tubes are the controller model's packed missile tubes
  ([exporter](../../../../../../DataPacker/Source/ExportJobs/AGENTS.md#model-alignment));
  these armed models require at least one. A launch uses the selected pair's
  yawed tube, its full aligned Z, and a flattened direction with zero Z. A
  launch `MissilesPostRender::Spawn` refuses leaves the cooldown unchanged.

## Fleet Health Invariants

- A fleet-controlled Battleship never dies, a harness-injected one included:
  when collision, area, or beam damage takes its health to 0 or below,
  PostRender holds its health at 0 and starts no explosion, and it otherwise
  behaves as any Battleship. Its health is never refilled.

## Wreck Invariants

- Wrecks are the client-only Wrecks collection (`../Wrecks/Wrecks.h`), visual
  state as `../../../../../../Engine/Source/Frame/AGENTS.md`
  `## Simulation and Visual State` defines; `../../AGENTS.md` `## Invariants`
  owns its registration. Its PostRender Spawn captures, from the row's tick
  pose, each Battleships row on its first dying tick, when `pDestroyedTimes`
  still equals the `kBattleshipDestroyTime` that `BeginExplosion` set, so only a
  combat death makes one; a `kDestroyShip` or harness `kDestroy` removal leaves
  none.
- After capture, the same Spawn emits one Battleship death explosion per tick
  from every wreck whose `pFallTimes` is below `kWreckExplosionDuration`. Its
  anchor is the upright hull's `ShipHullContact` point along a random
  upper-hemisphere direction, and the anchor and direction turn with the
  wreck's current tilt, from the `Wrecks.h` helper the wreck draw also uses.
  An anchor `ExplosionsPostRender::Spawn` refuses, by the rules
  `../../../../../../Engine/Source/Frame/AGENTS.md` `## Architecture` and
  `## Simulation and Visual State` give, is not retried, so that tick emits
  none.
- Its PostRender Destroy marks every sinking wreck whose hull top is below the
  water, and every wreck of a cell whose hexagon overlaps neither the camera's
  cell loaded area (`mf4CellLoadedArea`) nor, during a camera jump, the previous
  cell loaded area (`mf4PreviousCellLoadedArea`), by the client subscription's
  `RectangleOverlapsHex` test; only the main thread writes those members. The
  game Destroy fold compacts the marked rows in the same phase, so no stored or
  render frame holds a marked wreck.
- Every full state the client adopts replaces the cell's whole Frame with one
  whose `ServerRead` filled only the collections both builds share, so the
  cell's wrecks are lost there (resync, reconnect, restarted stream, update
  gap); no code copies them.
- Capture samples the cell's elevation under the hull box's center and four
  corners. A wreck with open seafloor under all five sinks; any other rests its
  box bottom on the highest sample, never above its start height, tilted to the
  terrain normal at the center by the fraction of its fall completed.
- A wreck falls straight down, its gravity ramping up over
  `kBattleshipWreckPowerLossTime`, then brakes hard once its center reaches
  z = 0 to `kfBattleshipWreckSinkSpeed`. The Interpolate Update steps it from
  the previous frame by the frame delta, in the tick and in render
  interpolation, so wrecks follow the simulation behind an opaque map and a
  death wholly inside map view leaves one.
- Dying rows are not drawn. A coordinate and pipeline's visible wrecks take the
  slots after its rows and share its single skinning reservation
  (`../Fighters/AGENTS.md` `## Rendering Invariants`), and the buffer capacity
  adds the Wrecks capacity. The `Wrecks` counter counts drawn wrecks;
  `Rendered` excludes them.
