# Ocean Impact Splashes and Ripples

Revisit When: water impacts start to matter visually — ship wreckage falling into the sea (`Documents/Features/Graphics/ShipWreckage.md`), a weapon that commonly misses over water, or playtest feedback that the ocean feels inert.

## Context

Nothing reacts to the sea surface today:

- A falling missile that reaches the sea is removed with no effect. In the terrain-hit branch of `MissilesPostRender::PostCollision` (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesUpdate.cpp`), a `kFalling` missile whose hit is at or below z = 0 gets `kSilentDespawn` instead of `Explode`. `TracePointAgainstTerrain` traces against the elevation grid, so in open water that hit is the sea floor (`kfSeaBottomMeters`), not the water surface.
- `ExplosionsPostRender::Spawn` (`Engine/Source/Frame/Collections/Explosions/ExplosionsSpawn.cpp`) adds the same lights, puffs, smoke trails, wind radials, and GPU particles over water as over land. Players and spaceships fly at `gBaseHeight` (default 6 m), so their explosions over water happen at that height.
- The water surface renders at `gWaterHeight` (default 0). Its displacement is GPU-only: `Engine/Data/Shaders/Water/WaterDisplacement.comp` bakes Gerstner displacement into a texture whose texels match the active LOD's visible-area vertex grid (`Water.vert`), and Low water quality bakes none. There is no CPU wave-height query.
- The client runs the same deterministic tick as the server, so both trigger sites already execute on the client. Reconciliation replays ticks marked `FrameFlags::kRecalculated`: `AudioManager::PlayOneShot3d` ignores those frames itself, and `ExplosionsPostRender::Spawn` skips its `ParticleManager::Spawn` calls on them.

## Design

- **Splash burst.** A short white spray through `ParticleManager::Spawn` plus a splash sound through `AudioManager::PlayOneShot3d`, at the impact point.
- **Ripple ring.** A flat, expanding, fading ring quad on the water. Its vertex shader samples the water displacement texture so the ring rides the swell, and stays flat at Low water quality; the fragment shader thins the ring with age.
- **Sources.** Falling missiles reaching the sea (a splash where the silent despawn is set today), explosions near sea level (a larger splash and a longer-lived ring), and falling wreckage from `Documents/Features/Graphics/ShipWreckage.md`. Blaster misses over water are optional.
- **Client-only trigger.** Spawn under `BT_CLIENT` at the trigger site, as `SynchronizeMissileTrail` is guarded beside it, so there is no new frame member, wire data, or CRC exposure. Splash particles and rings are not frame state, so they skip `kRecalculated` frames like explosion particles do, or every reconcile repeats them. Any randomness in them must not draw from the shared frame random engine inside the client guard.
- **Open choices.**
  - Where the ring lives: a new `ParticleManager` layout, or a separate client-side ring list.
  - The height threshold for "near sea level", which decides whether every unit explosion over water at `gBaseHeight` splashes.
  - The falling-missile splash point: the z = 0 crossing of the traced segment, rather than the sea-floor hit position.

## Out of scope

- Interactive wave simulation (wakes or ripples that propagate and interfere in the water height field).
- Water decals that persist (oil slicks, debris fields). `Documents/Features/Graphics/DecalSystem.md` owns terrain decals.

## Notes

- References:
  - Torque3D `SplashData`, https://reference.torque3d.org/scripting/class/classsplashdata — models a splash with ring lifetime, starting radius, emission frequency, and 0-1 time keys over particle life.
  - Panda3D forum, "WIP dynamic water waves," https://discourse.panda3d.org/t/wip-dynamic-water-waves/15179 — expands rings by ping-ponging between textures. Overlapping rings cancel instead of combining, which is a reason to keep rings as independent sprites.
  - Original note: sprite particles for ripples, https://youtu.be/0x_lIq3FEQE?t=1134.
