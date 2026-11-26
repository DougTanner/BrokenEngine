<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-25T23:51:51.345Z","dependsOn":[]} -->
# Collapse Pusher Spatial-Hash Zones to One XY Grid and Decouple Audio and Nav

## Context

Earlier flight-plane removal stages moved every ship onto one flight height,
world lighting onto one layer, smoke onto one field, and object shadows onto a
single-channel uniform blur. Three simulation and audio subsystems still carry
per-flight-plane plumbing:

- `Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp` (`:3-18`,
  `:31-34`, `:60-194`) allocates one 50x50 XY spatial-hash zone grid per
  flight-plane layer (`kiPusherLayers = std::ssize(game::kFlightPlanes)`),
  registers and queries pushers across `OccupiedPlanes()` loops, and deduplicates
  peers across layers.
- `Engine/Source/Audio/StaticVoices.cpp` (`:572-580`) sets the fade listener's
  Z coordinate (at the camera look-at XY) to
  `gpFlightPlanes->Highest().fHeightMeters * 1.1f` (`77.0f` in
  BrokenEngineSandbox) and references the highest flight plane in its
  fade-band comment (`:588-593`).
- `Engine/Source/Frame/NavCellData.cpp` (`:281`, `:348`, `:356`) still names its
  per-graph navigation builder `BuildPlaneNavigationData`.

Originating gap: user-approved stage split deferring the pusher-zone collapse,
audio listener decoupling, and navigation-builder rename into an independently
buildable and verifiable stage ahead of deleting `engine::FlightPlanes` and
`FlightPlaneTable.h`.

## Design

- **Pushers — 3D pushers on a single 50x50 XY zone grid** (user-specified):
  pushers remain 3D — 3D positions, squared 3D distance radius test, 3D force
  falloff within the source sphere, and signed coincident-position X correction
  — and only the spatial-hash zone buckets collapse from per-flight-plane 50x50
  XY grids to a single 50x50 XY grid. Drop `#include "Frame/FlightPlaneTable.h"`,
  `kiPusherLayers`, the layer dimension of `spppiPushersPerZone` and
  `sPusherZones`, the layer argument of `PusherZone`, the `OccupiedPlanes` loops
  in `SetupZones` and `ApplyPush`, and the cross-layer duplicate search. Each
  row registers in ascending row order into every XY zone its radius overlaps
  (retaining the first 512 rows per zone and logging at most one overflow
  warning per `SetupZones`), and `ApplyPush` reads the single XY zone holding
  its position so every peer is visited once in ascending row order. Update the
  `ApplyPush` declaration comment in
  `Engine/Source/Frame/Collections/Pushers/Pushers.h:58-60`.
- **Frame version bump**: because a Fighter raised between the old flight planes
  previously summed peers from two layers in layer order, peer summation order
  can differ from before. Bump `Frame::kiVersion`'s base literal from `190i64`
  to `191i64` in `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:165` so
  older saves and replays are rejected cleanly (no backward compatibility).
- **Audio listener height**: in `Engine/Source/Audio/StaticVoices.cpp:572-580`,
  replace
  `gpFlightPlanes->Highest().fHeightMeters * kfListenerHeightPerHighestPlane`
  with a fixed `kfListenerHeight = 77.0f` as the fade listener's Z coordinate
  at the camera look-at XY (`XMVectorSetZ`, not an offset from look-at Z;
  author's recommendation: preserves BrokenEngineSandbox's current 77 m
  listener Z and manual fade distances, keeping pan/Doppler on the look-at)
  and update the fade-band comment (`:588-593`).
- **Navigation builder rename**: rename `BuildPlaneNavigationData` to
  `BuildGraphNavigationData` and update its log text in
  `Engine/Source/Frame/NavCellData.cpp:281,348,356` (author's recommendation:
  the function builds per-graph navigation data, not per-plane data).
- **Documentation**: update the zone-grid prose in
  `Engine/Source/Frame/Collections/Pushers/AGENTS.md:12-32`, the listener height
  note in `Engine/Source/Audio/AGENTS.md:53`, and the pusher sentence about an
  occupied flight-plane layer in
  `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md`;
  delete `Documents/Investigations/Engine/CollisionPerFlightPlaneZoneGrid.md`;
  and update `Documents/Investigations/Engine/SameCollectionCollision.md:24,40`.

## Critical files

- `Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp`
- `Engine/Source/Frame/Collections/Pushers/Pushers.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`
- `Engine/Source/Audio/StaticVoices.cpp`
- `Engine/Source/Frame/NavCellData.cpp`

## In scope

- `Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp` (`:3-18`,
  `:31-34`, `:60-119`, `:121-194`): collapse the spatial-hash zone buckets to a
  single 50x50 XY grid while keeping pushers 3D (3D positions, squared 3D
  distance radius test, 3D force falloff, and signed coincident X correction).
- `Engine/Source/Frame/Collections/Pushers/Pushers.h` (`:58-60`): update the
  `ApplyPush` declaration comment.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp` (`:165`): bump
  `Frame::kiVersion` base literal from `190i64` to `191i64`.
- `Engine/Source/Audio/StaticVoices.cpp` (`:572-580`, `:588-593`): set the
  fade listener's Z coordinate to fixed `kfListenerHeight = 77.0f` at the
  camera look-at XY (`XMVectorSetZ`) and update the fade-band comment.
- `Engine/Source/Frame/NavCellData.cpp` (`:281`, `:348`, `:356`): rename
  `BuildPlaneNavigationData` to `BuildGraphNavigationData` and update its log
  text.
- Docs: `Engine/Source/Frame/Collections/Pushers/AGENTS.md` (`:12-32`),
  `Engine/Source/Audio/AGENTS.md` (`:53`),
  `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md`
  (pusher sentence about an occupied flight-plane layer), delete
  `Documents/Investigations/Engine/CollisionPerFlightPlaneZoneGrid.md`, and
  update `Documents/Investigations/Engine/SameCollectionCollision.md`
  (`:24,40`).

## Out of scope

- Deleting `Engine/Source/Frame/FlightPlanes.{h,cpp}` or either project's
  `Frame/FlightPlaneTable.h`, introducing `game::kfBaseHeightMeters` in
  `Frame/Frame.h`, renaming `FlightPlaneProjection.cpp`, updating remaining
  `gpFlightPlanes`/`kFlightPlanes` readers, or reconciling `.vcxproj`/filters
  (owned by the follow-up base-height Plan).
- Changing pusher 3D distance math, falloff curve, coincident X thresholds, or
  the 512 per-zone cap.
- Changing ship behaviour, targeting, navigation graph contents, lighting,
  smoke, or object shadows.
- Historical version sentences in `Documents/Architecture/Network.md`.
- Unit tests.

## Risk triggers and invariants

- Change Workflow Tier 3: modifies deterministic simulation and CRC-checked
  pusher-zone traversal (`PushersUpdate.cpp`) and bumps `Frame::kiVersion`
  (`Frame.cpp:165`), and spans independently owned subsystems (Frame/Pushers,
  Frame/Navigation, Audio, and BrokenEngineSandbox).
- Pushers keep their 3D contract: 3D positions, squared 3D distance radius test,
  full 3D force falloff within the source sphere, ascending row-order
  registration, first 512 per zone retained, at most one overflow warning per
  `SetupZones`, single visitation per peer, and signed coincident X correction
  (`kfPusherCoincidentDistance`, `kfPusherCoincidentCorrection`).
- `Frame::kiVersion` composes the bumped base literal `191i64`, so older saves
  and replays are rejected cleanly.
- Simulation code stays `/fp:strict` and never reads render state.

## Acceptance criteria

1. `builder /compile` builds `BrokenEngineSandbox` (client),
   `BrokenEngineSandboxServer`, and `ModelViewer` cleanly in Debug and Release
   (`x64`).
2. Static grep across `Engine/`, `Projects/BrokenEngineSandbox/`, and
   `Projects/ModelViewer/` finds zero matches for `kiPusherLayers`,
   `BuildPlaneNavigationData`, and `OccupiedPlanes` callers outside
   `FlightPlanes.{h,cpp}`; code read confirms
   `Engine/Source/Audio/StaticVoices.cpp` sets the fade listener's Z coordinate
   to fixed `77.0f` at the camera look-at XY (not an offset from look-at Z),
   the listed `AGENTS.md` and `SameCollectionCollision.md` files are updated,
   and `CollisionPerFlightPlaneZoneGrid.md` is deleted.
3. Code read confirms `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:165`
   uses base literal `191i64` in `Frame::kiVersion`.
4. `runner /agent-harness`: replay determinism check (`replay_record`, then
   `replay_play`) over fleet and random-wave combat reports no CRC mismatch;
   code read of `PushersUpdate.cpp` `SetupZones` and `ApplyPush` verifies
   ascending row-order registration, first 512 per zone retained, one overflow
   warning per `SetupZones`, single visitation per peer, 3D positions / squared
   3D distance radius test / 3D force falloff within the source sphere, and
   signed coincident X correction.
