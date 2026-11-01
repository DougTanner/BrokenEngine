# Battleships - Missile-Armed Heavy Ships

Battleships are a copy of Fighters with larger, slower constants
(`Battleships.h`): every Fighters invariant (`../Fighters/AGENTS.md`) holds with
the Battleship names. Keep the two collections parallel; only the tube missiles,
turning, and fleet health below and the Fighters' beam, client-only climb tilt
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
the flagship.

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
- `puiNextMissileTubes` holds the next launch slot k: tube k / 2, on the left
  side when k is even and the right when odd, so launches alternate sides and
  step through the `kiBattleshipMissileTubesPerSide` tubes from front to back,
  wrapping from the last right tube to the first left one. A missile leaves its
  tube outward from that side at the ship's exact height, so it starts on the
  ship's plane. A launch `MissilesPostRender::Spawn` refuses takes no slot,
  leaves the cooldown unchanged, and launches nothing that tick.
- The tube slot transfers with the ship, and the PostRender `PostRead` rejects a
  slot at or above `kiBattleshipMissileTubeSlots` on every Frame read.

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
