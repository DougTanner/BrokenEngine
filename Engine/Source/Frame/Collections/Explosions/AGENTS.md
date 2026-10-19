# Explosions - Composite Effects

Explosions coordinate point lights, puffs, smoke trails, wind radials, and GPU particles. They compile in both builds, while visible child effects are client-only.

## Invariants

- Keep every shared-random draw outside client guards. Client-only effects consume the results, but both builds must advance the frame random engine identically.
- Recalculated client frames suppress GPU-particle spawning without suppressing its random draws, preventing duplicate visuals during reconciliation while preserving stream lockstep.
- Explosion state lives in Interpolate; the paired PostRender collection holds only the standard lifecycle column and hosts phase operations and the public spawn API. Only shared members participate in deterministic CRC.
- On clients, `PersistentMembers()` carries the fixed `pTrails[]` child-handle arrays through `AllocateAndCopyMembers()`; the server persistent tuple is empty. `Update` repopulates the shared columns and remaining client trail state.
- Tuning values are game content: the engine reads the game's tuning wrapper globals directly and stores their addresses in controller scale arrays. These reads and controller registrations are client-only; the server never reads the wrappers.
- Trail duration tuning scales both travel distance and elapsed trail time so head speed remains constant. Cleanup intentionally uses unmultiplied trail time, keeping child removal aligned with shared parent destruction.
- Fixed trail slots are cleared when inactive so row reuse cannot expose stale shared state. A `kDying` explosion marks its row `kDestroy` once its last trail ends; any other row stays until something marks it `kDestroy`, and its remaining trails leave with it.
- Expiry compares whole nanoseconds: the shared destruction, the client trail cleanup, and the client expired-trail sync each convert a trail end with `common::FloatSecondsToNanoseconds`, so cleanup and destruction agree on every row.
- `PostRead` rejects any row whose start time, time percent, or used trail time lies outside the ranges `Spawn` produces (`Explosions.h`).
