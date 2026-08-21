<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-20T20:25:34.255Z","dependsOn":["Documents/Plans/Agents/HarnessPlacementAndPusherOverflowObservability.md"]} -->
# Cover the whole cell with pusher zones

## Context

The pusher spatial acceleration grid covers a 400 m square that is recentered
every tick on one entity, so pushers and queries elsewhere in the same 900 m
cell are silently ignored.

- `Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp:6-17` declares
  `kfPusherArenaSize = 400.0f`, `kfPusherZoneSize = 8.0f`,
  `kiPusherZones = 50`, `kiMaxPushersPerZone = 512`, the two `thread_local`
  zone tables, and the `thread_local` arena origin
  (`gfPusherArenaLeft`/`gfPusherArenaTop`).
- `PushersUpdate.cpp:50-53` recenters that arena every tick on the point the
  game returns from the required hook
  `game::FrameInterpolate::SpatialAnchor(rFrame.interpolate)`, declared at
  `Projects/BrokenEngineSandbox/Source/Frame/Frame.h:49` and defined at
  `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:176-179`, whose body
  returns the first player's position and falls back to the origin when the
  Players collection is empty. The engine no longer names Players — the
  completed predecessor Plan replaced that dereference and its
  `Frame/Collections/Players/Players.h` include with this hook — but the
  coverage is still a single moving 400 m window, so the hook only relocates
  the choice of which single point the arena follows.
- `PushersUpdate.cpp:67-77` skips any pusher whose radius-expanded bounds fall
  entirely outside the arena and clamps partially overlapping ones to the
  border zones; `PushersUpdate.cpp:105-107` clamps an out-of-arena query to a
  border zone, so such a query reads an unrelated zone's contents.
- `Engine/Source/Frame/GridCoord.h:44-45` sets the cell to
  `kfCellWidth = 900.0f` by `kfCellHeight = 900.0f`, so the arena covers under
  20% of one cell's area.

This is a single-local-player legacy. The game is multiplayer, so one cell can
hold several player perspectives at once: with two players in the same cell
more than roughly 400 m apart, entities near the second player receive no push
forces at all, because both the pushers around them and their own queries fall
outside the arena centered on the first player. Push forces feed deterministic
navigation (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp:507`
and `.../Spaceships/SpaceshipsNavigation.cpp:127`), so the omission is a
correctness gap in CRC'd simulation output rather than a visual one.

The engine already has the shape this needs: `Collision::Collide` takes the
cell's own extents, `rStaticData.vecArea`, at
`Engine/Source/Frame/FrameBase.cpp:349`, and `Collision::SetupZones`
(`Engine/Source/Frame/Collision.cpp:288-294`) derives its `thread_local` origin
and zone dimensions from that value over a fixed zone count
(`Engine/Source/Frame/Collision.h:9-10`). Pushers are the only cell-local
broadphase that still tracks a moving anchor instead.

## Design

Make pusher zone coverage the cell, and delete the anchor concept.

1. Pass the cell extents into zone setup. `FramePostRenderBase::Update`
   already holds `rStaticData` at the existing `SetupZones` call site
   (`Engine/Source/Frame/FrameBase.cpp:231-234`), so `SetupZones` takes
   `rStaticData.vecArea` alongside the frame, exactly as `Collision::Collide`
   does. `vecArea` is world-space and already offset per cell
   (`Projects/BrokenEngineSandbox/Source/Game.cpp:377`), which matches the
   world-space positions stored in the pusher collection.
2. Replace the arena constants and moving origin with the same
   `thread_local` origin and zone-extent values `Collision::SetupZones` keeps:
   minimum X, minimum Y, zone width, and zone height, all derived from
   `vecArea`. `ApplyPush` reads those same values.
3. Fix the zone grid at 50 by 50 zones per cell. That is exactly the current
   `kiPusherZones`, so the two `thread_local` tables keep their current size —
   50 × 50 × 512 × 2 bytes = 2,560,000 bytes per thread — and no additional
   per-thread memory is committed. Deriving the count from the cell size at
   the existing 8 m zone size instead would need 113 × 113 zones and about
   13 MB per thread, multiplied by every dispatch worker and reconcile thread,
   which is not worth paying for a broadphase whose exact radius test already
   rejects the extra candidates.
4. Keep `kiMaxPushersPerZone = 512` and its existing drop-past-the-cap
   `DEBUG_BREAK` handling (`PushersUpdate.cpp:85-91`) unchanged, and adopt the
   resulting density limit as decided policy. Zones become 18 m rather than
   8 m on a side, so roughly five times as many pushers land in one zone and
   each query scans that many more candidates. Below the cap the extra
   candidates are all rejected by the exact distance test and cannot change
   which pushers affect a query; above the cap they can, because entries 1
   through 512 are accepted and every later registration into that zone is
   dropped, so a pusher that would have fit
   in an 8 m zone can now be dropped from an 18 m one. The supported worst
   case is therefore stated as up to 512 pushers whose radius-expanded bounds
   overlap a single 18 m zone. Beyond that the loss stays loud
   (`DEBUG_BREAK`) rather than silent, and stays identical on client and
   server because registration runs in ascending collection index order on
   both, so overflow costs push forces but never diverges the CRC. Raising the
   cap or changing the overflow policy is out of scope; if the harness shows
   overflow at supported densities, stop and re-plan rather than widening this
   change.
5. Keep the existing registration structure: a pusher whose radius-expanded
   bounds fall entirely outside the cell is still skipped, partially
   overlapping bounds still clamp to the border zones, and an out-of-bounds
   query still clamps. Only the bounds those tests use change. This also
   decides the transfer window: an entity that moves past the cell bounds
   during Interpolate still consumes push forces in `PostRender::Update`
   before PostCollision and Transfer relocate it, and during that window its
   queries clamp to the border zone, while its own pushers are skipped only
   once their radius-expanded bounds fall wholly outside the cell and
   otherwise still register into the clamped border zones. That is
   accepted, because such an entity is leaving the cell and the destination
   cell's zones cover it on the next tick; matching the collision broadphase,
   which already treats `vecArea` as the hard cell boundary, is preferred over
   adding a margin band that would reintroduce coverage that depends on where
   entities happen to be.
6. Delete the anchor. Remove `kfPusherArenaSize`, the arena-origin globals,
   and the arena recentering at `PushersUpdate.cpp:50-53`, replacing that call
   site with the cell bounds from `rStaticData.vecArea`; then delete the
   `SpatialAnchor` hook itself — its declaration at
   `Projects/BrokenEngineSandbox/Source/Frame/Frame.h:48-49` including the
   comment naming it an engine requirement, and its definition at
   `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:176-179` including the
   empty-collection fallback in that body. `Frame.cpp` keeps its
   `Frame/Collections/Players/Players.h` include, which many other members of
   that file already require, and the engine pusher path already carries no
   Players include. Nothing may keep supplying an anchor after this change.
7. Bump `PushersInterpolate::kiVersion` from 1 to 2
   (`Engine/Source/Frame/Collections/Pushers/Pushers.h:37`) so that saves and
   replays recorded before this change are rejected by the existing
   `Frame::kiVersion` gate instead of silently failing their CRC comparison,
   and widen that member's comment so it also names a change to push
   behavior. No SOA layout, member tuple, wire format, or serialization code
   changes.

Add no backward-compatibility path, no runtime toggle, and no way to restore
the anchored arena.

## Critical files

- `Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp:6-17,43-109` —
  constants, `thread_local` zone state, the anchor call, zone registration,
  and the `ApplyPush` query.
- `Engine/Source/Frame/Collections/Pushers/Pushers.h:36-37,59` — the collection
  version and the `SetupZones` declaration.
- `Engine/Source/Frame/FrameBase.cpp:231-234` — the existing `SetupZones` call
  site, which already has `rStaticData` in scope.
- `Engine/Source/Frame/Collision.cpp:279-294` and
  `Engine/Source/Frame/Collision.h:9-10` — the existing cell-wide zone pattern
  to mirror; read only, not changed.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.h:48-49` and
  `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:176-179` — the
  `SpatialAnchor` declaration and definition, only to delete them; the rest of
  both files, including `Frame.cpp`'s Players include, is untouched.
- `Engine/Source/Frame/Collections/Pushers/AGENTS.md` — the "Zone acceleration"
  bullet, which states the 400 m arena, 8 m zones, `SpatialAnchor`
  recentering, and out-of-arena skip.

## In scope

- Pass `rStaticData.vecArea` into `PushersInterpolate::SetupZones` from the
  existing `FrameBase.cpp` call site and store the derived origin and zone
  extents in the existing `thread_local` state.
- Replace the arena bounds used by zone registration in `SetupZones` and by
  the query lookup in `ApplyPush` with those cell-derived bounds, keeping the
  50 × 50 zone count, the 512 per-zone cap and its overflow handling, the
  ascending-index registration order, the `thread_local` lifetime, and the
  post-Update phase timing.
- Delete `kfPusherArenaSize`, `kfPusherZoneSize`, the arena-origin globals, the
  arena recentering, and the `SpatialAnchor` hook: its engine call site, its
  declaration in the game `Frame.h`, and its definition in the game `Frame.cpp`
  together with that body's empty-collection fallback.
- Bump `PushersInterpolate::kiVersion` to 2 and update its comment wording.
- Update the Pushers `AGENTS.md` zone-acceleration bullet to the new coverage,
  zone size, and anchor-free behavior.
- Client and server compilation plus the behavior and determinism evidence in
  the acceptance criteria.

## Out of scope

- Push falloff math, `PusherFlags` semantics, include/exclude mask defaults,
  self-ignore or coincident-point handling, and `ApplyClampedPush`.
- Pusher collection SOA layout, members, serialization code, wire format,
  transfer, and ID handling.
- The `Collision` broadphase, its zone counts, its capacity policy, and
  `Documents/Plans/Engine/CollisionZoneCapacityPolicy.md`.
- Player and Spaceship navigation policy, tuning, and any gameplay retuning
  prompted by entities that now receive push forces.
- Cross-cell pusher influence: a pusher never affects a query in another cell,
  matching how per-cell ticks already work.
- Any general-purpose spatial index, shared zone abstraction, or unification
  of pusher and collision broadphases.
- Backward compatibility for pre-change saves and replays, runtime toggles,
  and unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3. The trigger is the root `AGENTS.md`
determinism/CRC surface: pusher forces feed Player and Spaceship navigation,
whose results are stamped into the per-tick shared CRC, and the change
deliberately alters that output for entities that previously fell outside the
arena. Zone state is `thread_local` because per-cell ticks run in parallel, and
the version bump touches the save/replay gate.

Invariants to preserve:

- Client and server produce identical results; the new bounds come from
  replicated static cell data, not from any live entity position or any
  client-only state.
- Zone tables stay `thread_local`, keep their current byte size, and are still
  rebuilt at the tail of `FramePostRenderBase::Update`, remaining valid for
  PreCollision and later phases on the same thread only.
- No main-loop heap allocation is introduced; the tables remain fixed-size
  arrays.
- A pusher is still registered in every zone its radius overlaps, registration
  still runs in ascending collection index order, and a query still sums over
  exactly one zone's list, so the summation order for any given candidate set
  is unchanged.
- `Documents/Architecture/FrameUpdatePipeline.md` phase ordering is unchanged.

## Coordination

`Documents/Plans/Engine/PusherAnchorContract.md` was this Plan's prerequisite
and is now complete: it removed the Player dependency from the engine pusher
path by introducing the `SpatialAnchor` hook, so it has been deleted from the
tree and this Plan's `dependsOn` is empty. This Plan is the direct successor
and deletes that hook outright. The predecessor's stated invariants — the
400 m arena size, 8 m zones, arena recentering, and out-of-arena skip
behavior — were binding only on it and are deliberately superseded here; this
Plan is the authority for pusher zone coverage.

## Acceptance criteria

- The client and server both build.
- A scoped search of the engine pusher path finds no arena constant, no arena
  origin variable, no anchor hook, and no anchor call site anywhere in the
  engine or the game.
- A harness scenario places two players in one cell roughly 600 m apart, each
  with nearby entities: entities near both players receive push forces, and the
  same scenario before the change shows push forces only near the first player.
- In a scenario where, for every tick, every pusher's radius-expanded bounds
  and every query position stay inside both the old 400 m arena around the
  first player and the cell's `vecArea`, and no zone exceeds the 512-entry
  cap, per-tick shared CRCs match the pre-change run exactly, confirming the
  candidate set and summation order did not change where coverage already
  existed. Entities that cross the cell bounds are excluded from this
  comparison by construction, because the transfer-window behavior decided
  above deliberately changes their forces.
- Client and server CRCs match for the whole two-player run, and a replay of
  that run reproduces them.
- No `gpppuiPusherZones` per-zone overflow `DEBUG_BREAK` fires in that run, and
  no allocation-tracking `DEBUG_BREAK` fires in the main loop.
- A worst-case density run concentrates the scenario's units into one 18 m
  zone up to the stated 512-pusher limit and still fires no per-zone overflow
  `DEBUG_BREAK`; pushing past that limit fires it on both client and server at
  the same tick, and client, server, and replay CRCs still match, confirming
  overflow costs forces without diverging determinism.
- A save or replay recorded before the change is rejected by the frame version
  gate rather than replaying with mismatched CRCs.

## Notes

Zone size changing from 8 m to 18 m is a memory-versus-scan-cost decision
below the per-zone cap: because a pusher is registered in every zone its
radius overlaps and every candidate is still accepted or rejected by the exact
distance test, zone size changes only how many candidates are examined, not
which pushers affect a query. The one exception is the 512-entry cap. Larger
zones reach it at roughly a fifth of the density, and registrations past it
are dropped, so above that density zone size does change the affecting set.
Design item 4 records that limit and the decision to keep the existing loud,
deterministic drop rather than raise the cap here.
