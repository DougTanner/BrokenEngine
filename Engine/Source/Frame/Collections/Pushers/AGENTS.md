# Pushers - Physics Force Fields

Radial repulsion force fields for entity separation (anti-stacking), owned by
game entities via the hub's Sync pattern. Pushers has no render path.

## Unique Aspects

- Carry-forward Update: Interpolate `Update` copies the full `Members()` tuple
  forward inside the existing Update phase — pushers have no self-driven
  simulation; last-synced state stays valid until the owner re-syncs. Do not
  move this copy into `AllocateAndCopy`.
- Zone acceleration: single fixed XY spatial grid (`kiPusherZones` per axis in
  `PushersUpdate.cpp`) spanning the cell's bounding rectangle
  (`kfCellHalfWidth`, `kfCellHalfHeight`, `GridCoord.h`), rebuilt by
  `SetupZones`, which the engine calls at the tail of
  `FramePostRenderBase::Update` after the base collection Updates — the game
  runs that parent call before its own collection Updates, so zones are valid
  from the rest of that same Update phase onward, on the same thread only (a
  game collection's `Update` is a same-phase consumer). Stored `thread_local` so
  each Dispatch worker and reconcile thread owns its copy. Each zone caps at
  `kiMaxPushersPerZone` rows in ascending row order (`PushersUpdate.cpp`);
  overflow drops excess registrations (`PushersInterpolate::SetupZones` owns
  overflow diagnostics), and pushers wholly outside the rectangle are silently
  skipped. A pusher spanning multiple zones is registered in every overlapping
  XY zone; `PushersInterpolate::ApplyPush` reads the query's zone in ascending
  row order, clamping outside queries to the border zone.
- A query requires a registered self handle and uses its registered position,
  excluding that row. Keep registered positions and zones unchanged throughout
  the game collection Updates; the next ordinary Interpolate Sync refreshes
  positions, rather than each consumer changing the shared query snapshot.
- Push falloff: `(1 - d²/r²)^power * intensity`, using full 3D distance
  within the source pusher's sphere, directed horizontally pusher-to-query and
  summed across the query's zone. Include/exclude masks on `PusherFlags_t`
  default to `kTypeDefault` and `kTypeMines`; new pusher types opt in/out
  through those flags.
- An included peer within the source sphere whose XY distance is at
  most `kfPusherCoincidentDistance` contributes no force, only a signed
  `kfPusherCoincidentCorrection` X position correction: negative when the self
  row index is lower, positive otherwise.
  Every other peer's XY offset is
  safe to normalize. Force has zero Z/W; the correction is added to the
  caller's value as transient output, never a snapshot-position write.
- ApplyClampedPush (`Pushers.h`): inline impulse that caps the velocity
  component in push direction, preventing stacking beyond caller-supplied max.
  General helper — game code also uses it for non-pusher impulses
  (terrain-normal push).
