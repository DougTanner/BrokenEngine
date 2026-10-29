# PointLights - Circular Point Lights

Client-only point lights support owner-driven and fire-and-forget controlled lifecycles. Each visible light renders a sprite; its appearance scalars do not publish physical diffuse sources.

## Invariants

- Culling packs only visible sprites, preserving their camera-cell positions. Types select world-axis-aligned or camera-facing quads and sample the original texture.
- Controller wrappers apply when controlled lights are seeded and animated. An uncontrolled light reads visible intensity from its type or tuning wrapper; owner `Sync` writes position, visible area, and rotation. Do not convert these appearance controls into physical source power.
- `PersistentMembers()` carries type/controller/start-time metadata and base rotation through `AllocateAndCopyMembers()`; identity is owned and copied by the paired PostRender ID storage. Position and animated values are rewritten by owner sync or controller update, so any new persistent state must follow one of those lifecycles explicitly.
- Future controller start times remain at the first keyframe until animation begins.
