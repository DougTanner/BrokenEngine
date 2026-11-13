# Explosions - Composite Effects

Explosions coordinate point lights, puffs, smoke trails, wind radials, and GPU
particles. The collection is client-only visual state (`../../AGENTS.md`
`## Simulation and Visual State`): it compiles only under `BT_CLIENT`.

## Invariants

- `Spawn` draws only from the `common::RandomEngine` its caller passes, which
  the caller seeds from one `Uuid::GenerateVisual` mint per explosion.
- Explosion state lives in Interpolate; the paired PostRender collection holds
  only the standard lifecycle column and hosts phase operations and the public
  spawn API.
- `PersistentMembers()` carries the fixed `pTrails[]` child-handle arrays
  through `AllocateAndCopyMembers()`; `Update` repopulates the remaining
  columns.
- Tuning values are game content: the engine reads the game's tuning wrapper
  globals directly and stores their addresses in controller scale arrays. These
  reads and controller registrations are client-only; the server never reads the
  wrappers.
- Trail duration tuning scales both travel distance and elapsed trail time so
  head speed remains constant. Cleanup intentionally uses unmultiplied trail
  time, keeping child removal aligned with parent destruction.
- Fixed trail slots are cleared when inactive so row reuse cannot expose stale
  state. A `kDying` explosion marks its row `kDestroy` once its last trail
  ends; any other row stays until something marks it `kDestroy`, and its
  remaining trails leave with it.
- Expiry compares whole nanoseconds: the row destruction, the trail cleanup,
  and the expired-trail sync each convert a trail end with
  `common::FloatSecondsToNanoseconds`, so cleanup and destruction agree on every
  row.
