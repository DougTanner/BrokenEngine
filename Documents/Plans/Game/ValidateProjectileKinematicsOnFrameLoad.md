<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-27T02:27:55.380Z","dependsOn":[]} -->
# Validate Projectile Kinematics at the Frame Save-Load Boundary

## Context

`operator>>` on `game::Frame`
(`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:829-847`) validates
loaded grid-save state through `ValidateShipKinematics` (`Frame.cpp:33-68`),
`ValidatePusherOwners` (`Frame.cpp:72-124`), `ValidateFlyingMissiles`
(`Frame.cpp:127-150`), and `ValidateNoTransferRows` (`Frame.cpp:154-170`).
However, it does not validate:

- blaster positions (`pBlasters->pVecPositions`) or velocities
  (`pBlasters->pVecVelocities`);
- missile horizontal XY positions (`pMissiles->pVecPositions`), falling
  missile Z positions, missile XY velocities (`pMissiles->pVecVelocities`), or
  falling missile Z velocities.

`Engine/Source/Frame/AGENTS.md` requires every entity's position to lie inside
its own cell's hexagon (`engine::IsInsideCell`), and the repository trust
policy (`AGENTS.md`, `Engine/Source/File/AGENTS.md`) requires the server to
fully validate grid saves at the read boundary, rejecting bad values via
`std::ios_base::failure`. `operator>>` is also the shared build-local
save/replay read boundary (`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`
lines 44 and 61). Because blaster and missile positions and velocities are not
validated on load, a corrupt save or replay frame can admit out-of-cell or
non-finite projectile coordinates or huge velocities that then reach
`BlastersInterpolate::Update` (`BlastersUpdate.cpp:129-137`),
`MissilesUpdate.cpp`, `Collision::AddLayer`
(`Engine/Source/Frame/Collision.cpp:145-167`), and `TracePointAgainstTerrain`
(`Engine/Source/Frame/TerrainCollisionUtils.cpp:10-42`).

This follow-up records a root-cause design suggestion from over-engineering
review that the user accepted as a separate follow-up Plan.

## Design

Recommended approach:

1. **Validate blaster and missile kinematics in `Frame.cpp` (`operator>>`,
   `:829-847`) and at the `inject_payload` producer boundary:** Add projectile
   admission checks alongside `ValidateShipKinematics` and
   `ValidateFlyingMissiles` (either extending `ValidateFlyingMissiles` into a
   unified `ValidateProjectileKinematics` helper or adding a sibling helper
   called from `operator>>`). Because `operator>>` is the shared build-local
   save/replay read boundary, this change tightens both grid-save and replay
   frame admission identically while leaving `Frame::kiVersion` and the
   save/replay binary formats unchanged. Define one shared two-sided
   per-component velocity ceiling `kfMaximumProjectileVelocityComponent` in
   `Projects/BrokenEngineSandbox/Source/Frame/Frame.h` (beside
   `kfShipSpawnInterval`), derived from the owning cell and tick-rate constants
   as `engine::kfCellApothem * static_cast<float>(engine::kiTickRate)` (at most
   one cell apothem per fixed tick along each axis, which strictly encloses
   every legitimate blaster speed `kfOmniBlastersSpeed` and missile velocity
   under `kfBattleshipMissileInitialVelocity`,
   `kfBattleshipMissileAcceleration`, `kfVelocityDecay`, and `kfMissileGravity`,
   while guaranteeing finite single-tick integration
   `XMVectorMultiplyAdd(dt, vecVelocity, vecPosition)` without leaping past an
   adjacent cell):
   - For every blaster row in `loadedFrame`, reject (throw
     `std::ios_base::failure`) if any X, Y, Z, or W lane of `pVecPositions[i]`
     or `pVecVelocities[i]` is non-finite, if the W lane violates the
     repository vector contract (`XMVectorGetW(pVecPositions[i]) != 1.0f` or
     `XMVectorGetW(pVecVelocities[i]) != 0.0f`, matching
     `common::ValidateVector<IS_POSITION>` in `Common/Math/MathUtils.h`), if
     `!engine::IsInsideCell(pVecPositions[i])`, or if any X, Y, or Z velocity
     component fails the two-sided bound
     `std::abs(fComponent) <= kfMaximumProjectileVelocityComponent` (rejecting
     both large positive and large negative components).
   - For every missile row in `loadedFrame`, apply the same four-lane finite
     and canonical-W (`position.w == 1.0f`, `velocity.w == 0.0f`),
     `engine::IsInsideCell(pVecPositions[i])`, and two-sided
     `std::abs(fComponent) <= kfMaximumProjectileVelocityComponent` checks
     while preserving the existing `ValidateFlyingMissiles` rule that
     non-falling, non-dying missiles have zero vertical velocity
     (`XMVectorGetZ(pVecVelocities[i]) == 0.0f`).
   - Enforce the same two-sided per-component velocity bound
     `std::abs(fComponent) <= kfMaximumProjectileVelocityComponent` on
     `SpawnBlaster` and `SpawnMissile` entries in `inject_payload`
     (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp:317-326`,
     alongside the `SpawnOmni` speed check at `:299-302`), so immediate
     `inject_payload` + `save` (as well as `edit_frame` + `save`, which commits
     through `operator>>`) and constant-velocity `SpawnBlaster` saves across
     ticks satisfy the save-reader contract and round-trip cleanly. Because
     `MissilesPostRender::Update` (`MissilesUpdate.cpp:107-115, 182`) integrates
     `pfAccelerations` along `pVecDirections`, preserves 3D speed while
     steering, and applies `kfMissileGravity` while falling, stepping ticks
     after injecting or editing extreme missile propulsion or direction values
     (for example, a large `fAcceleration`) can still accelerate a missile past
     `kfMaximumProjectileVelocityComponent`, which `operator>>` then rejects on
     reload; normal gameplay missiles and harness missiles whose post-tick
     velocities stay within the ceiling round-trip cleanly.
   - Keep `Frame::ServerRead` (`Frame.cpp:813-820`) untouched: server-to-client
     snapshots are trusted under `Engine/Source/Network/AGENTS.md`.
2. **Evaluate downstream guards in `TracePointAgainstTerrain`
   (`Engine/Source/Frame/TerrainCollisionUtils.cpp:10-42`):** Once `operator>>`
   rejects invalid saved or replayed projectile kinematics, audit every
   remaining producer of `TracePointAgainstTerrain` inputs (including
   `Collision::AddLayer`, `BlastersUpdate.cpp`, `MissilesUpdate.cpp`, and
   `edit_frame` harness paths) to determine whether the non-finite input check
   and off-grid start sea-floor branch in `TerrainCollisionUtils.cpp:16-41` can
   be simplified or removed without reintroducing float-to-`int64_t` UB or
   breaking any valid caller. Keep any guard whose redundancy is not proven.
3. **Update Frame and AgentHarness documentation:**
   - Update `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` to record the
     new projectile build-local save/replay validation invariant beside the
     existing save/replay read-boundary rules.
   - Update
     `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`
     (`:102-106`, where the `edit_frame` contract currently states that
     projectile positions and velocities other than flying-missile height and Z
     velocity are unchecked by the save reader, the staged-commit check list at
     `:115-120`, and the `inject_payload` validation list at `:276-291` for the
     `SpawnBlaster` and `SpawnMissile` velocity ceiling), pointing each passage
     to the owning Frame invariant in
     `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`.

Rationale: validating at the shared build-local save/replay trust boundary and
enforcing the same velocity bound at `inject_payload` removes the root cause of
invalid persisted projectile coordinates while keeping immediate harness saves,
constant-velocity `SpawnBlaster` saves, and normal-propulsion gameplay saves
round-trippable, instead of relying solely on downstream defensive handling
across collision and terrain queries.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Frame.h` — shared
  `kfMaximumProjectileVelocityComponent` constant (beside `kfShipSpawnInterval`)
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp` — `operator>>`
  (`:829-847`) and `ValidateFlyingMissiles` (`:127-150`)
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp`
  — `inject_payload` `SpawnBlaster` and `SpawnMissile` velocity admission
  (`:317-326`)
- `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` — save/replay
  read-boundary invariants
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md` —
  `edit_frame` save-reader contract (`:102-106`), staged-commit check list
  (`:115-120`), and `inject_payload` validation list (`:276-291`)
- `Engine/Source/Frame/TerrainCollisionUtils.cpp` — `TracePointAgainstTerrain`
  (`:10-42`), evaluated for redundant downstream guards

## In scope

- Defining shared `kfMaximumProjectileVelocityComponent`
  (`engine::kfCellApothem * static_cast<float>(engine::kiTickRate)`) in
  `Projects/BrokenEngineSandbox/Source/Frame/Frame.h` and adding blaster and
  missile position (all four lanes X, Y, Z, W finite, `XMVectorGetW == 1.0f`,
  `engine::IsInsideCell`) and velocity (all four lanes X, Y, Z, W finite,
  `XMVectorGetW == 0.0f`, two-sided
  `std::abs(fComponent) <= kfMaximumProjectileVelocityComponent` on X, Y, and
  Z) validation to the shared build-local save/replay read boundary `operator>>`
  in `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp` (`:127-150`,
  `:829-847`), preserving existing flying-missile vertical-velocity checks.
- Enforcing the same two-sided per-component velocity bound
  `std::abs(fComponent) <= kfMaximumProjectileVelocityComponent` on
  `SpawnBlaster` and `SpawnMissile` in `inject_payload`
  (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp:317-326`)
  so immediate `inject_payload` saves and constant-velocity `SpawnBlaster` saves
  round-trip through the save reader (leaving `SpawnMissile` propulsion inputs
  such as `fAcceleration` uncapped).
- Evaluating and, where proven redundant against all reachable callers,
  simplifying or removing downstream guards in `TracePointAgainstTerrain`
  (`Engine/Source/Frame/TerrainCollisionUtils.cpp:10-42`).
- Synchronizing `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` and
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`
  (`:102-106`, `:115-120`, and `:276-291`) with the added save/replay and
  `inject_payload` projectile admission rules.

## Out of scope

- Changing `Frame::ServerRead` (`Frame.cpp:813-820`) or client-side network
  trust policy.
- Changing `Frame::kiVersion`, save/replay binary formats, or wire protocol
  (only read-time frame admission tightens).
- Changing `ValidateShipKinematics` (`Frame.cpp:33-68`),
  `ValidatePusherOwners` (`Frame.cpp:72-124`), or `ValidateNoTransferRows`
  (`Frame.cpp:154-170`) unless an explicit contradiction is surfaced for
  re-planning.

## Acceptance criteria

1. Code inspection of `Projects/BrokenEngineSandbox/Source/Frame/Frame.h`,
   `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`, and
   `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp`
   confirms the shared build-local save/replay reader `operator>>` rejects any
   saved or replayed blaster or missile with any non-finite X, Y, Z, or W lane
   in position or velocity, a non-canonical W lane (`position.w != 1.0f` or
   `velocity.w != 0.0f`), an out-of-cell position (`!engine::IsInsideCell`), or
   any X, Y, or Z velocity component violating the two-sided bound
   `std::abs(fComponent) <= kfMaximumProjectileVelocityComponent`
   (`engine::kfCellApothem * static_cast<float>(engine::kiTickRate)`), as well
   as any saved or replayed flying (neither falling nor dying) missile with
   nonzero vertical velocity (`XMVectorGetZ(pVecVelocities[i]) != 0.0f`, even
   when finite and within that bound), by throwing `std::ios_base::failure`;
   that `inject_payload` enforces the same
   `kfMaximumProjectileVelocityComponent` bound on `SpawnBlaster` and
   `SpawnMissile` so immediate `inject_payload` + `save` and `edit_frame` +
   `save` frames, constant-velocity `SpawnBlaster` saves across ticks, and
   normal-propulsion missile saves whose post-tick velocities remain within
   `kfMaximumProjectileVelocityComponent` round-trip cleanly (while post-tick
   saves from extreme injected or edited missile propulsion or direction values
   that accelerate past the ceiling are rejected by `operator>>`); and that
   valid in-cell blasters and missiles (including falling and dying missiles)
   load unchanged.
2. Code inspection of `Engine/Source/Frame/TerrainCollisionUtils.cpp:10-42`
   confirms that any simplified or removed downstream guard has a proof of
   unreachability from all remaining callers (including `Collision::AddLayer`,
   `BlastersUpdate.cpp`, `MissilesUpdate.cpp`, and `edit_frame`) without UB;
   any guard whose redundancy is not proven (whether because a reachable caller
   exists or because the audit is inconclusive) is retained unchanged.
3. Code inspection confirms
   `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` and
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`
   (`:102-106`, `:115-120`, and `:276-291`, pointing to
   `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`) document the updated
   projectile save/replay and `inject_payload` admission rules.
4. `/compile` builds `BrokenEngineSandbox` (`Debug|x64`, `Release|x64`) and
   `BrokenEngineSandboxServer` (`Debug|x64`, `Release|x64`) cleanly, plus
   `ModelViewer` (`Debug|x64`) if
   `Engine/Source/Frame/TerrainCollisionUtils.cpp` is modified.

## Notes

- **Change Workflow tier:** Tier 3 — modifies the shared build-local
  save/replay admission boundary in
  `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp` and spans two
  independently owned subsystems (`Projects/BrokenEngineSandbox/Source/Frame`
  and `Engine/Source/Frame/TerrainCollisionUtils.cpp`).
- **Exposure:** Shared client/server compilation (`BT_CLIENT` and `BT_SERVER`,
  plus `ModelViewer` if `TerrainCollisionUtils.cpp` changes); no change to
  `Frame::kiVersion`, save/replay binary layout, wire protocol, threading,
  allocations, or shaders. Rejecting corrupt saves or replay frames is a
  boundary-rejection path not exercised by ordinary saves and is verified by
  code inspection plus multi-target build (plus `/agent-harness` save/load
  smoke check if the executing plan's acceptance criteria request one).
