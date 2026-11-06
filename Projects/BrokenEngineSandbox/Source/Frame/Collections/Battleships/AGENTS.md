# Battleships - Missile-Armed Heavy Ships

Battleships are a copy of Fighters with larger, slower constants
(`Battleships.h`): every Fighters invariant (`../Fighters/AGENTS.md`) holds with
the Battleship names. Keep the two collections parallel; only the tube missiles,
turning, fleet health, and death below and the Fighters' beam, client-only climb tilt
and turn roll, fleet tactics, `pVecVeerOffPoints`, and lack of flee differ: a
random-spawn Battleship, when island candidates are available, prioritizes
returning to an island, fleeing its enemy ship when nearby, then chasing it; in
a cell with no island candidates, it clears the return-to-island state and uses
flee/chase steering for an enemy ship or ramps its turn rate to zero otherwise.
Return and flee transitions use distance hysteresis. That enemy is its kept
attack target (`../Ships/AGENTS.md` `## Ship Invariants`), picked from itself in
3D at or below plane 0. A `kTargetFlagship` row's kept attack target is the
enemy flagship, which it chases with neither return nor flee
(`../Ships/AGENTS.md`); its missiles keep the rule below, which already covers
the flagship. A dying Battleship has neither the Fighters' death knockback nor
their shrink: it holds still, skips terrain contact, and the client draws its
model as a wreck (`## Wreck Invariants`).

## Weapon Invariants

- Homing tube missiles replace the Fighter's beam: the PostRender
  Spawn hook launches them first, then emits the staggered death explosions. It
  runs in one sequential pass over the rows in ascending order. A row launches
  when it is not dying, its arrival grace has run out, the shared plane and
  height rule allows it (`../Ships/AGENTS.md` `## Ship Invariants`), and its
  `pNextMissileFireTimes` cooldown is at or below zero. A launch resets the
  cooldown to `kBattleshipMissileCooldown`, and the cooldown transfers with the
  ship, so launches come exactly `kBattleshipMissileCooldown` apart.
- A launching row launches at most one missile per tick, at the enemy nearest
  it by 3D distance among those within `kfBattleshipMissileRange` that the
  shared plane and line-of-sight targeting rules allow (`../Ships/AGENTS.md`),
  and that fewer missiles than its type's `kiMissilesPerTarget` cap target; an
  exact distance tie keeps the first in `ForEachEnemyShip` order. Missiles do
  not prefer the kept attack target. The count covers every missile targeting
  it, other Battleships' missiles included, and every launch made earlier in the
  same tick, so a later shooter sees an earlier shooter's launches. Each missile
  receives that enemy as its target at launch, with no registry acquisition
  (`../Missiles/AGENTS.md`).
- Tubes are the controller model's packed missile tubes
  ([exporter](../../../../../../DataPacker/Source/ExportJobs/AGENTS.md#model-alignment));
  these armed models require at least one. A launch uses the yawed tube nearest
  the selected enemy in XY (exact tie: first authored), its full aligned Z, and a
  flattened direction with zero Z. A launch `MissilesPostRender::Spawn` refuses
  leaves the cooldown unchanged.

## Turning Invariants

- Every Battleship turns at one maximum rate, `kfBattleshipMaxTurnRate`.
  Each tick PostRender Update moves the turn rate toward a wanted rate by at
  most `kfBattleshipTurnAcceleration` times the tick time
  (`RampBattleshipTurnRate`), and terrain bounce and terrain avoidance pass
  through that same ramp instead of setting the rate directly. The wanted rate
  is zero within the heading capture angle of the wanted heading.
- Every row stores its horizontal wanted heading in `pVecAiDirections`: fleet
  rows their navigation bearing, random-spawn rows the normalized horizontal
  direction to their destination, negated while fleeing and not returning, or
  zero without a destination, in which case the wanted rate is zero.
- Interpolate Update, while the previous turn rate is exactly 0 and the
  previous wanted heading is nonzero and within the capture angle, rotates the
  direction a fixed percent toward it each tick.

## Fleet Health Invariants

- A fleet-controlled Battleship never dies, a harness-injected one included: when collision, area, or beam damage takes its health to 0 or below, PostRender holds its health at 0 and starts no explosion, and it otherwise behaves as any Battleship. Its health is never refilled.

## Wreck Invariants

- Wrecks are client-only render state, the `sWrecks` list in
  `BattleshipsRender.cpp`, outside every Frame, CRC, save, and the wire, keyed
  by `(GridCoord, battleship_t)`. `BeginRender` captures, once per key, each row
  whose interpolated `pDestroyedTimes` is at or above zero in a visible cell, so
  only a combat death makes one; a `kDestroyShip` or harness `kDestroy` removal
  leaves none.
- A cell is visible while it is active, has a render interpolate, and its
  hexagon overlaps the non-map camera's `mf4WorldVisibleArea` by the client
  subscription's `RectangleOverlapsHex` test. Each `BeginRender` drops every
  wreck whose cell is not visible and every sinking wreck whose hull top is
  below the water.
- Capture samples the cell's elevation under the hull box's center and four
  corners. A wreck with open seafloor under all five sinks; any other rests its
  box bottom on the highest sample, never above its start height, tilted to the
  terrain normal at the center by the fraction of its fall completed.
- A wreck falls straight down, its gravity ramping up over
  `kBattleshipWreckPowerLossTime`, then brakes hard once its center reaches
  z = 0 to `kfBattleshipWreckSinkSpeed`, stepped by the sim-scaled render delta
  and not at all while paused. Capture, motion, and prune run only in
  `BeginRender`, so map view's world-hidden frames freeze wrecks, and a death
  wholly inside map view leaves none.
- `Game::ResetGameState` and `ClientSession::OnServerLoad` clear the list,
  because cell-local Ids restart there.
- Dying rows are not drawn. A coordinate and pipeline's visible wrecks take the
  slots after its rows and share its single skinning reservation
  (`../Fighters/AGENTS.md` `## Rendering Invariants`), and the buffer capacity
  adds the wreck count. The `Wrecks` counter counts drawn wrecks; `Rendered`
  excludes them.
