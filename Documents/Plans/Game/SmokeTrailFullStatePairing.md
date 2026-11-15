<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-15T16:40:24.681Z","dependsOn":[]} -->
# Smoke Trail Full-State Continuity Pairs Trails by Row Index

## Context

When the client adopts a server full state,
`engine::ClientSessionRuntime::AdoptFullState`
(`Engine/Source/Network/Client/ClientSessionRuntime.cpp` near 518) calls
`game::ClientSession::HydrateReceivedFullState` with the cell's ring tail
frame. That function
(`Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp`
292-311) first runs the four `ClientInitializeAll` hydrators, which re-create
client-owned visuals in the received frame, then copies
`SmokeTrailsInterpolate::pVecSmoothedPositions` from the ring tail into the
received frame with one `std::memcpy` over
`min(old iCount, new iCount)` rows.

The copy pairs rows by index, not by trail identity. In the received frame the
smoke rows are created fresh by `MissilesInterpolate::ClientInitializeAll`
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp`
near 163), which calls `SmokeTrailsPostRender::Add` once per live missile in
missile row order. The ring tail's smoke rows are in whatever order trails were
added and removed over time, and they can also hold explosion trails
(`Engine/Source/Frame/Collections/Explosions/ExplosionsSpawn.cpp` near 140).
Whenever the two orders differ, a smoke trail takes another trail's smoothed
tail position and its ribbon visibly jumps for one reconciliation.

The documented invariant this breaks is in
`Engine/Source/Frame/Collections/SmokeTrails/AGENTS.md` `## Invariants`:
full-state reception patches smoothed tail positions from the previous ring
frame "so reconciliation does not visibly reset trails". A mispaired copy does
not reset the trail, but it moves it to the wrong place, which is the visible
discontinuity the invariant exists to prevent.

Evidence status: the index pairing is read directly from the code above. The
visible jump has not been observed at runtime; no harness run recorded it, and
the supplied evidence records no reproducing setup. When the user deferred
this item, they asked for it to be checked at runtime first and fixed only if
it is real.

This is pre-existing code outside the change that found it; that change left
smoke trails on their copy-continuity policy unchanged.

## Design

The author recommends two steps.

1. Confirm at runtime. Through `/agent-harness`, run a scene with several
   missiles in flight in one cell and force the client to adopt a server full
   state while they fly (for example a resync, or a subscription change that
   re-subscribes the cell). Capture screenshots or render-state queries just
   before and just after adoption, and log, for each received smoke row, the
   ring-tail row it was copied from and the owner of each. The pairing is real
   when a received row is copied from a ring-tail row of a different owner, or
   from an explosion trail. If that never happens across repeated adoptions
   with missiles and explosions present, record the evidence and route the
   Plan to rejection instead of changing code.
2. If it is real, pair by identity inside `HydrateReceivedFullState`: for each
   owner that holds a smoke trail in both frames, copy the ring tail's smoothed
   position for that owner's old trail id into the row of its new trail id,
   resolving each id through the `kIdToIndex` mapping
   `SmokeTrailsInterpolate` already has. A received trail with no matching
   owner in the ring tail keeps W == 0 so its first update snaps to the owner
   position (the W rule in the SmokeTrails `AGENTS.md`). The executing session
   chooses which owner identity matches a missile across the two frames,
   using whatever stable identity the missile collection already carries;
   if none exists, it stops and returns that choice for re-planning rather
   than adding an identity column.

Rejected alternative, kept for the record: delete the copy so every smoke trail
restarts on full-state adoption. It is the smallest change and removes the
mispairing, but every trail visibly resets, and the user chose to keep smoke
continuity.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp`
  (`ClientSession::HydrateReceivedFullState`)
- `Engine/Source/Frame/Collections/SmokeTrails/SmokeTrails.h`
  (`SmokeTrailsInterpolate`, read only)
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.h`
  and `Missiles.cpp` (`puiSmokeTrails`, `ClientInitializeAll`, read only)
- `Engine/Source/Frame/Collections/SmokeTrails/AGENTS.md` (only if the
  invariant wording must change)

## In scope

- The runtime check in Design step 1.
- The smoke-trail copy block inside `ClientSession::HydrateReceivedFullState`
  and any file-local helper it needs in `ClientSession.cpp`.
- The SmokeTrails `AGENTS.md` invariant bullet, only if the fix changes what it
  states.

## Out of scope

- The `ClientInitializeAll` hydrators, their call order, and the Missiles,
  Explosions, and SmokeTrails collections' layouts, members, and update code.
- Wind trails and every other client-only collection's reconciliation
  handling.
- Adding any collection member, identity column, or persisted field; a fix
  that needs one is returned for re-planning.
- `ClientSessionRuntime::AdoptFullState` and the rest of the full-state
  adoption path.

## Risk tier

Tier 2: client-only visual behavior in one subsystem. Smoke trails are
`BT_CLIENT` interpolate state outside the CRC, so there is no determinism,
CRC, wire, save, or replay exposure; the hydration runs on the main thread at
publication, so no threading change.

## Acceptance criteria

- Step 1's evidence is recorded: either a run showing a received smoke row
  copied from a different owner's ring-tail row, or repeated adoptions with
  missiles and explosions present showing none.
- If the pairing was real: after the fix, the same setup shows every received
  smoke row that has a matching owner in the ring tail copied from that
  owner's trail, no row copied from another owner or an explosion trail, and
  no visible smoke-trail jump across adoption in before/after screenshots.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
`Documents/Plans/Engine/ClientSessionCleanup.md` edits other functions in
`ClientSession.cpp` and explicitly leaves this smoke copy out of its scope.

## Notes

- Unverified by the author: whether the received frame holds any explosion
  trail rows at hydration. Only the missile hydrator was read creating smoke
  rows there; step 1's logging settles it.
