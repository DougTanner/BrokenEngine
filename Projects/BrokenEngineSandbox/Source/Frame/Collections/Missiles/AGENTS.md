# Missiles - Guided Area-Damage Projectiles

Guided missiles own client-side exhaust light, smoke trail, and looping sound effects while alive. Their damage is applied through an area-damage explosion rather than direct collision damage.

## Invariants

- Homing begins with a boost ramp, then tracks the target the missile received at launch with smoothed steering; the launch target affects homing from the next tick, preserving previous/current frame semantics. Missiles Update never acquires a target: a lost target is dropped and never replaced, and the missile flies on unguided along its stored direction until its lifetime ends. A missile launched with no target flies unguided the same way.
- A missile flies on the lowest flight plane. It keeps a target only in that
  plane's registry context, which holds ships whose nearest flight plane it is;
  a target leaving that context is dropped. A flying missile — neither falling
  nor dying — moves vertically from its launch height toward the lowest plane
  at the Battleship climb speed, clamping exactly on arrival, while direction
  and propulsion velocity Z remain zero. Falling and dying missiles do not
  perform this return.
- Validate and clear target handles on target loss, falling, explosion, transfer, and destruction. Removal is idempotent.
- Lifetime, velocity, falling state, boost-ramp delay, and turn rate cross cell boundaries; an arrival restores them verbatim and draws no fresh ramp delay. Falling disables propulsion, homing, and exhaust random draws; terrain impact at or below sea level removes the missile without explosion damage or effects.
- Resolve the earliest entity, terrain, or boundary event before applying explosion or transfer behavior. All missile damage enters through the area-damage registration at explosion time.
- Client-only pitch and exhaust values still draw from shared deterministic state. Both builds must execute those draws under the same shared-state conditions.
- Live transfer always receives a new smoke-trail identity on arrival; destination lights and sounds are recreated. Falling arrivals create no client effects.
