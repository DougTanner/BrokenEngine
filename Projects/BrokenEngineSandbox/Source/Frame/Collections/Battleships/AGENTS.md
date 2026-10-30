# Battleships - Beam-Armed Heavy Ships

Battleships are a copy of Fighters with larger, slower constants (`Battleships.h`): every Fighters invariant (`../Fighters/AGENTS.md`) holds with the Battleship names. Keep the two collections parallel; only the beam and fleet health below and the Fighters' client-only climb tilt differ.

## Weapon Invariants

- One hit-scan beam replaces the Fighter's fixed forward shot, so the
  PostRender Spawn hook only emits the staggered death explosions. The beam
  fires in the PostRender PostCollision hook, after every other collection's
  PostCollision, in one sequential pass over the rows in ascending order. A row
  fires when it is neither dying nor transferring, its arrival grace has run
  out, it stores a flight plane, and its `pNextBeamFireTimes` cooldown is below
  zero; a shot resets the cooldown to `kBattleshipBeamCooldown`.
- The beam hits the nearest enemy within `kfBattleshipBeamRange` of 3D distance, with no visibility test, whose nearest flight plane is plane 0 (`../Ships/AGENTS.md`): a Battleship if one qualifies, else an Omni, else a Fighter, and only when no ship qualifies, a flying missile that is not leaving the cell. An exact distance tie keeps the lowest row, and a later shooter sees an earlier shooter's kill.
- A hit deals `kfBeamDamage` at once through the target type's death path, and a Fighter's or Battleship's death knockback pushes it away from the shooter. A hit missile explodes as on an entity impact.
- The cooldown counts down under the Fighters arrival-grace rule and transfers with the ship.
- The beam line is client-only: the shot syncs the `pBeamAreaLights` area light
  toward the stored `pVecBeamEnds`, and Interpolate Update fades it from the
  shared cooldown alone. PostRender Update releases that light and clears its
  client-only Interpolate handle `pBeamAreaLights` once the fade ends, an
  exception to the mirrored Fighters rule that the position is the only other
  Interpolate field PostRender Update writes (`../Fighters/AGENTS.md`). A
  transfer arrival draws no line until its next shot.

## Fleet Health Invariants

- A fleet-controlled Battleship never dies, a harness-injected one included: when collision, area, or beam damage takes its health to 0 or below, PostRender holds its health at 0 and starts no explosion, and it otherwise behaves as any Battleship. Its health is never refilled.
