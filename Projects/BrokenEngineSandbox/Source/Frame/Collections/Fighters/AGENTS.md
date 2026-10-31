# Fighters - Heading-Steered Combat Ships

Fighters combine heading-based steering, health and damage, a hit-scan beam, and client skeletal rendering. Battleships mirror this collection (`../Battleships/AGENTS.md`).

## Behavior Invariants

- A fleet-controlled fighter in its flagship's cell steers by its fleet's
  stance (`../Ships/AGENTS.md` `## Fleet Coordination Invariants`) on the
  highest Fighter plane: in escort it heads for its own slot on the fleet's
  escort clock (`../Ships/FleetCoordination.cpp`), a point `kfEscortDistance`
  from the flagship. The fleet's non-dying Fighters in the cell's previous
  frame share the circle evenly in global-ID order, and the clock turns
  counter-clockwise once per `kiEscortLapTicks` as a pure function of the frame
  tick, never stored; weapons free it attacks. Outside that cell it keeps the
  shared fleet navigation on the highest Fighter plane. A random-spawn fighter
  attacks whenever its cell's previous frame holds an enemy ship, finishes a
  veer-off in progress even when none remains, and otherwise heads for the
  nearest island center, setting the return-to-island state with distance
  hysteresis, or only decays its turn in a cell with no island candidates.
- Attack heads for its target and targets that enemy's nearest plane. A
  random-spawn fighter's target is the enemy ship nearest it in x/y, picked
  every tick with no target lock; a fleet fighter's is its kept
  `pRegistryTargets` target (`../Ships/AGENTS.md`
  `## Fleet Coordination Invariants`). Within the
  `kfVeerOffStartDistanceSquared` x/y distance it sets `ShipFlags::kVeerOff`,
  stores the enemy position in `pVecVeerOffPoints`, and flies directly away
  from that point on the point's nearest plane until the
  `kfVeerOffEndDistanceSquared` x/y distance from it, then attacks again while
  an enemy remains. An attack, veer-off, or escort tick writes its target plane
  into the current row's orders; only a fleet fighter's such tick also leaves
  the row roaming (navigation direction -1). `kVeerOff` clears in escort,
  outside the flagship's cell, and on spawn and transfer arrival.
- Steering turn rate crosses cell boundaries with the ship rather than restarting on arrival.
- Direct blaster hits arrive through collision; missile splash damage arrives
  through the area-damage phase; a beam hit arrives in the shooting Fighter's
  PostCollision hook (`## Weapon Invariants`). Destruction applies horizontal knockback and a
  staged explosion sequence. Separation and pusher death ownership follow the
  [shared ship policy](../Ships/AGENTS.md#ship-invariants).
- The nose climb tilt is client-only render state: Interpolate Update derives it from the tick's height change and Render pitches the model by it, while the simulated heading stays yaw-only.
- Body, collision, effect, and model sizing derive from `kfFighterRadius`.
- The simulation keeps the Interpolate `pDestroyedTimes` at the -1 spawn sentinel or in [0, `kFighterDestroyTime`]. The Interpolate `PostRead` rejects any other row on every Frame read.
- Under the [shared arrival-grace contract](../AGENTS.md#game-specific-rules),
  arrival grace and the beam cooldown count down unconditionally before
  behavior gating. While the decremented grace remains positive, a non-exploding
  fighter retains carried motion, pusher response, and terrain bounce while
  skipping navigation and AI steering, acceleration, drag,
  terrain avoidance, and firing. Ordinary behavior resumes in the same tick that
  grace reaches zero; exploding fighters continue through the existing death
  path.
- The PostRender Spawn phase hook never creates fighters, despite the name: it only emits the staggered death explosions. Creation is the separate `SpawnInfo` overload of `Spawn`, called from the frame's random-spawn waves and from the post-tick batch's spawn and transfer status changes. Creation logic belongs in that overload, not in the phase hook, which runs per tick per ship.
- Cross-phase field ownership: the steering delta rotation (this tick's turn
  amount) lives in the Interpolate struct and is read by Interpolate Update and
  by Render, but it is decayed and written from PostRender Update. That crossing
  is deliberate — do not "fix" it by moving the field to PostRender, which would
  change the CRC member set and the render read path. The position, resolved
  against terrain and then corrected under the shared ship policy, and the
  client-only beam light handle `pBeamAreaLights`, which it releases and clears
  (`## Weapon Invariants`), are the only other Interpolate fields PostRender
  Update writes; do not treat any other Interpolate field as writable from
  PostRender Update.

## Weapon Invariants

- The beam fires at the end of the PostRender PostCollision hook, in one
  sequential pass over the rows in ascending order. Fighters' PostCollision
  runs after the Blasters' and Missiles' and before the Omnis' and Battleships'
  (`../../FrameCollections.h`), so a shot sees neither this tick's collision
  results for Omnis and Battleships nor this tick's area damage. A row fires
  when it is neither dying nor transferring, its arrival grace has run out, it
  stores a flight plane, and its `pNextBeamFireTimes` cooldown is below zero; a
  shot resets the cooldown to `kFighterBeamCooldown`, and the cooldown
  transfers with the ship.
- The beam hits the nearest enemy ship by 3D distance, with no visibility test,
  among those inside the cell whose nearest flight plane is the shooter's
  stored plane (`../Ships/AGENTS.md`), within `kfFighterBeamRange`, and whose
  horizontal offset is within `kfFighterBeamAimAngle` of the heading; with none
  it holds fire. An exact distance tie keeps the first row in `ForEachShipType`
  order, and a later shooter sees an earlier shooter's kill.
- A hit deals `kfFighterBeamDamage` at once through the target type's
  `ApplyBeamDamage` death path, and a Fighter's or Battleship's death knockback
  pushes it away from the shooter.
- The beam line is client-only: the shot syncs the `pBeamAreaLights` area light
  toward the stored `pVecBeamEnds`, and Interpolate Update fades it from the
  shared cooldown alone. PostRender Update releases that light and clears its
  handle once the fade ends, before the next shot. A transfer arrival draws no
  line until its next shot.

## Rendering Invariants

- Rendering across active coordinates is sequential on the main thread because the running total used for what is displayed is process-wide rather than thread-local. Do not parallelize the outer per-coordinate calls.
- Each controller draws through its own model pipeline. Within one coordinate and pipeline, culling and the controller partition happen first, followed by one up-front bulk reservation each for the mesh-data and joint-matrix blocks; workers then write disjoint mesh-data and joint-matrix slabs. Preserve those non-overlapping reservations when changing dispatch.
