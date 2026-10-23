# Battleships - Two-Gun Heavy Ships

Battleships are a copy of Fighters with larger, slower constants (`Battleships.h`): every Fighters invariant (`../Fighters/AGENTS.md`) holds with the Battleship names. Keep the two collections parallel; only the guns below and the Fighters' client-only climb tilt differ.

## Weapon Invariants

- Two guns, front and rear, replace the Fighter's fixed forward shot. In the PostRender Spawn hook each gun whose own cooldown has run out (`pNextBlasterSpawnTimes` front, `pNextRearBlasterSpawnTimes` rear) shoots at the nearest visible enemy ship the shared plane rule (`../Ships/AGENTS.md`) allows whose horizontal offset lies within the gun's 90-degree arc about the heading or the reversed heading. The front gun fires before the rear, so a battleship fires at most two shots per tick.
- Both cooldowns count down under the Fighters arrival-grace rule and transfer with the ship.
