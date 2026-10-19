# Fighters - Heading-Steered Combat Ships

Fighters combine heading-based steering, health and damage, a fixed forward blaster, and client skeletal rendering. Battleships mirror this collection (`../Battleships/AGENTS.md`).

## Behavior Invariants

- A fleet-controlled fighter steers its heading along the shared fleet
  navigation (`../Ships/AGENTS.md`). A random-spawn fighter, when island
  candidates are available, prioritizes returning to an island, fleeing a
  nearby enemy ship, then chasing the nearest enemy ship; in a cell with no
  island candidates, it clears the return-to-island state and uses flee/chase
  steering for an enemy ship or only decays the current turn otherwise. Return
  and flee transitions use distance hysteresis. Only random-spawn fighters
  regenerate health.
- Steering turn rate crosses cell boundaries with the ship rather than restarting on arrival.
- Direct blaster hits and ship rams arrive through collision; missile splash damage arrives through the area-damage phase. Destruction applies horizontal knockback and a staged explosion sequence.
- Interpolate Update owns a ship's height: each tick it moves position Z toward the ship's target height — the lowest flight plane's height for every ship — by at most the maximum flight speed times the tick delta, and lands exactly on it. PostRender Update stores velocity Z as 0 after every other velocity write, so pusher response, knockback, and terrain bounce move ships only horizontally. Change a ship's height through its target height, not its velocity.
- Body, collision, effect, and model sizing derive from `kfFighterRadius`.
- The simulation keeps the Interpolate `pDestroyedTimes` at the -1 spawn sentinel or in [0, `kFighterDestroyTime`]. The Interpolate `PostRead` rejects any other row on every Frame read.
- Under the [shared arrival-grace contract](../AGENTS.md#game-specific-rules),
  arrival grace and the blaster cooldown count down unconditionally before
  behavior gating. While the decremented grace remains positive, a non-exploding
  fighter retains carried motion, pusher response, and terrain bounce while
  skipping health regeneration, navigation and AI steering, acceleration, drag,
  terrain avoidance, and firing. Ordinary behavior resumes in the same tick that
  grace reaches zero; exploding fighters continue through the existing death
  path.
- The PostRender Spawn phase hook never creates fighters, despite the name: it emits the staggered death explosions and fires blasters. Creation is the separate `SpawnInfo` overload of `Spawn`, called from the frame's random-spawn waves and from the post-tick batch's spawn and transfer status changes. Creation logic belongs in that overload, not in the phase hook, which runs per tick per ship inside the blaster-fire loop.
- Cross-phase field ownership: the steering delta rotation (this tick's turn
  amount) lives in the Interpolate struct and is read by Interpolate Update and
  by Render, but it is decayed and written from PostRender Update. That crossing
  is deliberate — do not "fix" it by moving the field to PostRender, which would
  change the CRC member set and the render read path. The terrain-resolved
  position is the only other Interpolate field PostRender Update writes; do not
  treat any other Interpolate field as writable from PostRender Update.

## Rendering Invariants

- Rendering across active coordinates is sequential on the main thread because the running total used for what is displayed is process-wide rather than thread-local. Do not parallelize the outer per-coordinate calls.
- Each controller draws through its own model pipeline. Within one coordinate and pipeline, culling and the controller partition happen first, followed by one up-front bulk reservation each for the mesh-data and joint-matrix blocks; workers then write disjoint mesh-data and joint-matrix slabs. Preserve those non-overlapping reservations when changing dispatch.
