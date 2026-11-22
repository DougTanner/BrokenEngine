# PointLights - Circular Point Lights

Client-only point lights support owner-driven and fire-and-forget controlled
lifecycles.

## Invariants

- Visible sprites cull against the live visible area and stay at the original
  world position; deposits pack into one shared deposit buffer, pipeline, and
  cursor (`kCrc`), independent of the sprite cursor.
- Deposits bypass visible-sprite culling (see `PointLightsRender.cpp` for
  off-area clipping): when `giLightingRefreshLayer` (`Render.h`) is active, each
  light with positive lighting area whose nearest flight plane
  (`NearestPlaneIndex`) matches that layer projects onto that plane and emits
  into the deposit buffer. When no layer is active, deposit buffer resizing and
  emission are skipped and the deposit indirect count is zero, while visible
  sprites keep their full rendered count.
- The visible sprite may be world-axis-aligned or camera-facing by type. The
  deposit is always world-axis-aligned and samples the blurred texture; the
  sprite samples the unblurred texture.
- Deposit area uses the selected lighting area directly in world-space units.
- Controller wrappers apply when controlled lights are seeded and animated. A
  light with no controller takes its visible intensity, lighting area, and
  lighting intensity from its type at render, through the type's tuning wrapper
  when one is set; owner `Sync` writes only position, visible area, and
  rotation.
- `PersistentMembers()` carries type/controller/start-time metadata and base
  rotation through `AllocateAndCopyMembers()`; identity is owned and copied by
  the paired PostRender ID storage. Position and animated values are rewritten
  by owner sync or controller update, so any new persistent state must follow
  one of those lifecycles explicitly.
- Future controller start times remain at the first keyframe until animation
  begins.
