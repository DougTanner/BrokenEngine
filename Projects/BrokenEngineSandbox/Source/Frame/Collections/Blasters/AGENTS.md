# Blasters - Fast Energy Projectiles

Blasters move at constant velocity and own client-side light and wind-trail effects. They render no projectile geometry directly.

## Invariants

- Resolve exactly one earliest event over swept entity collision, terrain impact, and frame-boundary transfer. Terrain and boundary events win exact-time ties through the collision layer cutoff.
- Shared impact-jitter and crater-rotation draws remain outside client guards even when only client effects consume them. Moving these draws changes the deterministic random stream.
- Each blaster uses either an area light or a camera-aligned point light. Sync and teardown must preserve that exclusive ownership.
- Cross-cell transfer resets client-only wind-trail tuning to canonical client defaults; trail identity is not carried, so the source trail is removed and the destination client creates a new trail.
- Terrain impacts use the resolved elevation-grid position before spawning their client effects.
- Weapon code creates new projectiles. Keep one-shot muzzle audio at the firing site so transfers do not replay it.
- Blaster-owned terrain effect types register through `BlastersInterpolate::Register()`, which forwards to a helper defined in the update file where the cached type indices live. Add new blaster-owned effect types through that one entry point so registration stays ordered and happens once.
