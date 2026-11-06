<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-05T13:48:43.357Z","dependsOn":[]} -->
# Prove or fix the ship hull contact assert

## Context

`ShipHullContact`
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipHitboxes.cpp:102`)
is the client-only end point of the Fighter beam, called
from `FightersCombat.cpp:125`. It moves the shooter into the target's local
frame, sets the path delta to reach the local origin, and calls
`common::ClipSegmentToPrism` with `fEnter` zero and `fExit`
`std::numeric_limits<float>::max()`, so the path is a ray through the origin and
beyond. Then `ASSERT(bHit)` at `:115` requires that ray to meet the prism built
from the target's cached box and Z range.

Observed symptom: nothing establishes that requirement in Z. The bake keeps the
local origin inside the outline, so it is inside the box, which encloses the
hull and the outline. When the origin is also inside the Z range the ray always
hits. When it is outside the Z range, a ray from a suitable shooter can miss,
and the assert fires. Only the following facts were read:

- The DataPacker bake's outline checks are vertex count, finite values, hull
  convexity, and the origin lying inside the outline
  (`DataPacker/Source/ExportJobs/ExportModel.cpp:253`,
  `DataPacker/Source/ExportJobs/AGENTS.md` `## Model Alignment`); none tests the
  origin against the Z range.
- A sidecar `offset` moves a model after centering. Among the models
  `kShipHitboxTypes` names, `Spaceship/scene.gltf.model.json` and
  `SpaceBattleshipAquamarine/scene.gltf.model.json` carry a nonzero offset.
- `ASSERT` is active in every build (`Common/ErrorUtils.h:20`).

No run has been recorded firing this assert, so its reachability is the open
question. The audit that found it (commits cfd6116e, 13e4ae19, 5c2e6a8d) did not
inspect packed outline values.

## Design

The executing session first confirms the root cause through
`/external-diagnose-bug`: whether a packed target's Z range can exclude its
origin for any model in `kShipHitboxTypes`.

Then it makes the smallest change inside the region `## In scope` names:

- If every model's prism holds its origin, the assert stands and the finding is
  that invariant, recorded in `Ships/AGENTS.md`.
- If a model's does not, `ShipHullContact` handles the miss. The author
  recommends ending the beam at the target's center position, the point the
  path already aims at, in place of the assert.

A fix outside that region, such as a bake check in DataPacker or an edited
sidecar, is surfaced for re-planning instead of widening this scope.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipHitboxes.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md`

## In scope

- Investigation: for each of the models in `kShipHitboxTypes`, both controllers
  of all three ship types, read the packed `common::ModelOutline`
  (`pf2HullVertices`, `fMinimumZ`, `fMaximumZ`) and test the local origin
  against the prism `InitializeShipHitboxes` caches, whose box comes from that
  hull. Include the shooter-at-origin case, where the delta is zero and
  `ClipSegmentToPrism` decides from the half-space tests alone.
- Fix region: `ShipHullContact`, and the `## Ship Invariants` bullet on the
  collision shape in `Ships/AGENTS.md`.

## Out of scope

- `common::ClipSegmentToPrism` (`Common/Math/ConvexHull.h`) and `ExportModel`.
- Model sidecars and packed data.
- `ASSERT(std::isfinite(fContact))` at `:128`:
  `Documents/Plans/Game/ShipCombatAndFleetCleanups.md` owns its removal.
- Beam damage, target choice, and any server code.

## Risk triggers and invariants

- Tier 2. Trigger: one subsystem's runtime behavior, a client-only visual end
  point.
- `ShipHullContact` sits under `BT_CLIENT`. Its result is client-only visuals and
  stays out of the CRC; beam damage does not read it.
- No `.pack`, `kiVersion`, wire, replay, or threading exposure inside the region.

## Acceptance criteria

- The handoff records, per model, whether the origin lies inside the prism in XY
  and in Z, read from packed data.
- The recorded symptom no longer reproduces: either that record proves
  `ASSERT(bHit)` unreachable for every listed model, or `ShipHullContact` no
  longer asserts on a miss and returns a finite contact.
- `/compile` builds the BrokenEngineSandbox client and server.
- A live `/agent-harness` run with Fighters beaming each listed target model
  raises no assert. The supplied evidence records no harness run or setup, so the
  session builds its own rig through `/agent-harness`.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
