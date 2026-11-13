# Fighters - Heading-Steered Combat Ships

Fighters combine heading-based steering, health and damage, a hit-scan beam, and
client skeletal rendering. Battleships mirror this collection
(`../Battleships/AGENTS.md`).

## Behavior Invariants

- A fleet-controlled fighter in its flagship's cell steers by its fleet's
  stance (`../Ships/AGENTS.md` `## Fleet Coordination Invariants`) on the
  highest Fighter plane: in escort it closes on and holds its own slot on the
  fleet's escort clock (`../Ships/FleetCoordination.cpp`), a point
  `kfEscortDistance` from the flagship. The fleet's non-dying Fighters in the
  cell's previous frame share the circle evenly in global-ID order, and the
  clock turns clockwise once per `kiEscortLapTicks` as a pure function of the
  frame tick, never stored. It aims ahead of its slot along the slot's path
  under the escort tactic's own maximum speed, `ShipTactic::fMaximumSpeed`
  from `ChooseFleetTactic`, which rises while it trails its slot and falls
  while it is ahead; every other tactic keeps `kFighterMovement`'s. Weapons
  free it attacks. Outside that cell it keeps the shared fleet navigation on the
  highest Fighter plane. A random-spawn fighter attacks whenever its cell's
  previous frame holds an enemy ship, finishes a veer-off in progress even when
  none remains, and otherwise heads for the nearest island center, setting the
  return-to-island state with distance hysteresis, or only ramps its turn rate
  to zero in a cell with no island candidates.
- Attack heads for its target and targets that enemy's nearest plane. Its
  target is its kept attack target (`../Ships/AGENTS.md` `## Ship Invariants`),
  picked from itself in x/y for a random-spawn fighter, or the enemy flagship
  for a `kTargetFlagship` row. Within the
  `kfVeerOffStartDistanceSquared` x/y distance it sets `ShipFlags::kVeerOff`,
  stores the enemy position in `pVecVeerOffPoints`, and routes away from that
  point (`FleeRouteWaypoint`) on the point's nearest plane until the
  `kfVeerOffEndDistanceSquared` x/y distance from it, then attacks again while
  an enemy remains. An attack, veer-off, or escort tick writes its target plane
  into the current row's orders; only a fleet fighter's such tick also leaves
  the row roaming (navigation direction -1). `kVeerOff` clears in escort,
  outside the flagship's cell, and on spawn and transfer arrival.
- Steering turn rate crosses cell boundaries with the ship rather than
  restarting on arrival.
- Direct blaster hits arrive through collision; missile splash damage arrives
  through the area-damage phase; a beam hit arrives in the shooting Fighter's
  PostCollision hook (`## Weapon Invariants`). Destruction applies horizontal
  knockback and a staged explosion sequence. Separation and pusher death
  ownership follow the [shared ship policy](../Ships/AGENTS.md#ship-invariants).
- The nose climb tilt is client-only render state: Interpolate Update derives
  it from ordinary plane travel before terrain rise, and Render pitches the
  model by it, while the simulated heading stays yaw-only. Terrain displacement
  contributes no tilt.
- Body and effect sizing derive from `kfFighterRadius`; collision dimensions
  follow the [shared ship policy](../Ships/AGENTS.md#ship-invariants).
- The simulation keeps the Interpolate `pDestroyedTimes` at the -1 spawn
  sentinel or in [0, `kFighterDestroyTime`]. The Interpolate `PostRead` rejects
  any other row on every Frame read.
- Arrival grace and the beam cooldown count down unconditionally before
  behavior gating. While the decremented grace remains positive, a non-exploding
  fighter retains carried motion, pusher response, and the shared terrain
  response while skipping navigation and AI steering, acceleration, drag,
  terrain avoidance, and firing. Ordinary behavior resumes in the same tick that
  grace reaches zero; exploding fighters continue through the existing death
  path.
- The PostRender Spawn phase hook never creates fighters, despite the name: it
  only emits the staggered death explosions. Creation is the separate
  `SpawnInfo` overload of `Spawn`, called from the frame's random-spawn waves
  and from the post-tick batch's spawn and transfer status changes. Creation
  logic belongs in that overload, not in the phase hook, which runs per tick per
  ship.
- Cross-phase field ownership: the steering delta rotation (this tick's turn
  amount) lives in the Interpolate struct and is read by Interpolate Update and
  by Render, but it is ramped and written from PostRender Update. That crossing
  is deliberate — do not "fix" it by moving the field to PostRender, which would
  change the CRC member set and the render read path. The position and stored
  plane, corrected under the shared ship policy, and the client-only beam light
  handle `pBeamAreaLights`, which it releases and clears
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
  transfers with the ship. The simulation keeps the cooldown finite and at most
  `kFighterBeamCooldown`, with no lower clamp; `PostRead` rejects any other row
  on every Frame read.
- The beam hits the shooter's kept attack target when it qualifies, else the
  nearest qualifying enemy ship by 3D distance. A qualifying ship is inside the
  cell, its nearest flight plane is the shooter's stored plane, it is in line
  of sight (`../Ships/AGENTS.md` `## Ship Invariants`) and within
  `kfFighterBeamRange`, and its horizontal offset is within
  `kfFighterBeamAimAngle` of the heading; with none the beam holds fire. A
  `kTargetFlagship` shooter's kept attack target is the enemy flagship. An
  exact distance tie keeps the first row in `ForEachShipType` order, and a
  later shooter sees an earlier shooter's kill.
- A hit deals `kfFighterBeamDamage` at once through the target type's
  `ApplyBeamDamage` death path, and a Fighter's death knockback pushes it away
  from the shooter.
- The beam line is client-only: the shot syncs the `pBeamAreaLights` area light
  toward the stored `pVecBeamEnds`, and Interpolate Update fades it from the
  shared cooldown alone. PostRender Update releases that light and clears its
  handle once the fade ends, before the next shot. A transfer arrival draws no
  line until its next shot.
- Target qualification and aim use the target center. The client captures the
  current yawed baked outline contact before damage, using the model's outline
  and Z slab even for simpler projectile hitbox settings. An inside start uses
  the center-directed ray's exit surface.
  The stored endpoint stays
  fixed during the fade. A separate self-destroying impact point light places
  a rotating particle-library star and local illumination at that contact.

## Rendering Invariants

- Rendering across active coordinates is sequential on the main thread because
  the running total used for what is displayed is process-wide rather than
  thread-local. Do not parallelize the outer per-coordinate calls.
- Each controller draws through its own model pipeline. Within one coordinate
  and pipeline, culling and the controller partition happen first, followed by
  one up-front bulk reservation each for the mesh-data and joint-matrix blocks;
  workers then write disjoint mesh-data and joint-matrix slabs. Preserve those
  non-overlapping reservations when changing dispatch.
