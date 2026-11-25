<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-24T21:08:52.582Z","dependsOn":["Documents/Plans/Engine/RemoveFlightPlanes.md"]} -->
# Graceful Fighter Raise-Above-Terrain

## Context

Live Fighters avoid clipping into island terrain by snapping upward in
`RaiseAboveTerrain`
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp`),
which samples the cell elevation at the Fighter's center XY, adds the lowest
flight plane height, and immediately overwrites Z whenever the Fighter is below
that floor. `FightersInterpolate::Update` (`Fighters.cpp`) steps Z back down
linearly at constant `kfFighterClimbRate` via `StepHeightTowards` with no
ease-out, computes client-only `pfClimbTilts` only from that linear step, and
then applies `RaiseAboveTerrain`; `FightersPostRender::Update` (`Fighters.cpp`)
applies `RaiseAboveTerrain` again after pusher X correction.

As a result, Fighters fly flat until terrain plus clearance exceeds their
current Z, snap upward abruptly each tick over rising slopes, and descend at a
constant rate that stops abruptly at layer height.

The user requested: "The 'raise above terrain' code should gracefully raise the
fighter with acceleration applied before they need to be raised (as they get
close to layer height) and forcefully raised only as a last resort (they should
come down gracefully too, with ease-out at the end)".

Originating gap: user-requested follow-up kept out of scope of the single-height
ship refactor.

## Design

Replace the snap-only climb and constant-rate descent for live Fighters with a
smooth vertical motion model while keeping `RaiseAboveTerrain` as the hard
last-resort safety floor:

- **Proactive terrain lookahead (author's recommendation)**: sample cell
  elevation ahead of each live Fighter along its horizontal motion/heading in
  `FightersInterpolate::Update` (using `engine::gpIslandTerrain` cell elevation
  sampling) to detect rising terrain approaching layer height before the
  Fighter's center reaches the slope.
- **Vertical acceleration and ease-out (author's recommendation)**: add a shared
  scalar vertical speed column `float* __restrict pfVerticalSpeeds = nullptr;`
  to `FightersInterpolate` (following `/add-collection-member`, mirroring
  `pfDeltaRotations` in `FightersInterpolate`, and keeping
  `FightersPostRender::pVecVelocities` Z at `0.0f` as required by
  `ApplyHeadingMovement` in `ShipBehaviours.cpp`). In
  `FightersInterpolate::Update`, accelerate upward when the lookahead clearance
  height exceeds current Z; when clear to descend toward
  `EffectiveShipFlightHeight`, accelerate downward and decelerate with ease-out
  as the Fighter nears layer height, landing exactly on
  `EffectiveShipFlightHeight` with `pfVerticalSpeeds` zeroed within the final
  step so determinism and exact-height invariants hold.
- **Last-resort hard floor**: keep `RaiseAboveTerrain` in
  `FightersInterpolate::Update` (after vertical integration) and in
  `FightersPostRender::Update` (after pusher X correction) as the last-resort
  clamp when steep terrain or pusher correction still exceeds current Z,
  clearing any downward `pfVerticalSpeeds` on contact.
- **Client-only pitch tilt (author's recommendation)**: derive `pfClimbTilts`
  in `FightersInterpolate::Update` from `pfVerticalSpeeds` (clamped to
  `kfFighterClimbTilt`) so both proactive climbs and ease-out descents pitch
  the model smoothly without sudden tilt spikes from last-resort clamps.
- **Cross-cell transfer, serialization, and determinism**: carry
  `pfVerticalSpeeds` across cell boundaries in `FightersPostRender::SpawnInfo`
  (`float fVerticalSpeed = 0.0f;` in `SpawnInfo::Fields()`, mirroring
  `fDeltaRotation` so a Fighter climbing or descending across a cell edge keeps
  smooth vertical motion; genuine spawns default `fVerticalSpeed` to `0.0f`).
  Keep all vertical-motion math under `/fp:strict` and fixed-tick determinism.
  Add `pfVerticalSpeeds` to `FightersInterpolate::SharedMembers()` (which is
  also `FightersInterpolate`'s CRC set, since it declares no narrower
  `SharedCrcMembers()`), `FightersInterpolate::LogDifferences`, and
  `FightersInterpolate::PostRead` (rejecting non-finite or out-of-range vertical
  speeds), validate `SpawnInfo::fVerticalSpeed` with the same range test in
  `IsAdoptableStatusChange` (`StatusChange.cpp`), and bump
  `FightersInterpolate::kiVersion` (`Fighters.h`), `FrameInput::kiVersion`
  (`FrameInput.h`), and `engine::kiProtocolVersion` (`NetworkProtocol.h`).

## Critical files

- `Engine/Source/Network/NetworkProtocol.h`
- `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h`
- `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md`

## In scope

- Proactive terrain lookahead, vertical acceleration on climb, and graceful
  ease-out descent toward `EffectiveShipFlightHeight` for live Fighters in
  `FightersInterpolate::Update`, `FightersPostRender::Update`, and
  `ShipBehaviours.h/.cpp`.
- Retaining `RaiseAboveTerrain` as the last-resort hard floor after vertical
  integration and after pusher X correction, zeroing downward
  `pfVerticalSpeeds` on contact.
- Adding `pfVerticalSpeeds` to `FightersInterpolate` (`SharedMembers()`,
  `PostRead`, `LogDifferences`, `kiVersion` bump) and `fVerticalSpeed` to
  `FightersPostRender::SpawnInfo` (`Fields()`, `Transfer`, `Spawn`,
  `IsAdoptableStatusChange` in `StatusChange.cpp`), plus bumping
  `FrameInput::kiVersion` (`FrameInput.h`) and `engine::kiProtocolVersion`
  (`NetworkProtocol.h`).
- Updating client-only `pfClimbTilts` derivation in
  `FightersInterpolate::Update`.
- Updating `Fighters/AGENTS.md` and `Ships/AGENTS.md` to document the new
  vertical-motion, transfer, and tilt invariants.

## Out of scope

- Auto-rising above Battleships or other ship navigation blockers (covered by
  `Documents/Plans/Game/FighterAutoRiseAboveBlockers.md`).
- Changing Omni, Battleship, or Missile height or terrain-contact behavior.
- Changing dying-Fighter terrain bounce or the `pVecVelocities` Z == 0.0f
  invariant.
- Changing island elevation generation or navigation contour baking.

## Acceptance criteria

- `BrokenEngineSandbox` (client) and `BrokenEngineSandboxServer` build cleanly.
- Live `/agent-harness` verification (no prior observing run recorded; setup:
  spawn a Fighter flying toward and across an island slope, stepping ticks and
  querying its position Z over the approach, crest, and exit):
  - Fighter Z begins rising smoothly with increasing vertical step size before
    terrain elevation plus layer height reaches its current Z.
  - Steep terrain or pusher X correction never leaves a live Fighter below
    terrain elevation plus layer height (last-resort clamp still holds).
  - After clearing the island, Fighter Z descends smoothly with decreasing
    vertical step size (ease-out) near the end and lands exactly on
    `EffectiveShipFlightHeight`.
- Client/server replay determinism and CRC parity hold across the climb, crest,
  and ease-out descent.

## Notes

- Risk tier: Tier 3 — changes deterministic simulation state and CRC-checked
  Fighter height trajectories, and adds a shared SOA collection column with
  transfer, serialization, and version-bump exposure.
