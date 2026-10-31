# Battleships - Missile-Armed Heavy Ships

Battleships are a copy of Fighters with larger, slower constants
(`Battleships.h`): every Fighters invariant (`../Fighters/AGENTS.md`) holds with
the Battleship names. Keep the two collections parallel; only the tube missiles
and fleet health below and the Fighters' beam, client-only climb tilt, fleet
tactics, `pVecVeerOffPoints`, `pRegistryTargets`, and lack of flee differ: a
random-spawn Battleship, when island candidates are available, prioritizes
returning to an island, fleeing a nearby enemy ship, then chasing the nearest
enemy ship; in a cell with no island candidates, it clears the return-to-island
state and uses flee/chase steering for an enemy ship or only decays the current
turn otherwise. Return and flee transitions use distance hysteresis.

## Weapon Invariants

- Homing tube missiles replace the Fighter's beam: the PostRender
  Spawn hook launches them first, then emits the staggered death explosions. It
  runs in one sequential pass over the rows in ascending order. A row launches
  when it is not dying, its arrival grace has run out, and the shared plane and
  height rule allows it (`../Ships/AGENTS.md` `## Ship Invariants`). There is no
  cooldown.
- A launching row visits its enemies in `ForEachEnemyShip` order and launches
  at each one within `kfBattleshipMissileRange` of 3D distance, with no
  visibility test, that the shared plane targeting rule allows
  (`../Ships/AGENTS.md`), while fewer than `kiMissilesPerTarget` missiles
  target that enemy. The count covers every missile targeting it, other
  Battleships' missiles included, and every launch made earlier in the
  same tick, so a later shooter sees an earlier shooter's launches. Each missile
  receives that enemy as its target at launch, with no registry acquisition
  (`../Missiles/AGENTS.md`).
- `puiNextMissileTubes` holds the next launch slot k: tube k / 2, on the left
  side when k is even and the right when odd, so launches alternate sides and
  step through the `kiBattleshipMissileTubesPerSide` tubes from front to back,
  wrapping from the last right tube to the first left one. A missile leaves its
  tube outward from that side at the ship's exact height, so it starts on the
  ship's plane. A launch `MissilesPostRender::Spawn` refuses takes no slot and
  ends launches at that enemy for the tick.
- The tube slot transfers with the ship, and the PostRender `PostRead` rejects a
  slot at or above `kiBattleshipMissileTubeSlots` on every Frame read.

## Fleet Health Invariants

- A fleet-controlled Battleship never dies, a harness-injected one included: when collision, area, or beam damage takes its health to 0 or below, PostRender holds its health at 0 and starts no explosion, and it otherwise behaves as any Battleship. Its health is never refilled.
