<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-19T14:44:21.850Z","dependsOn":[]} -->
# Keep a cross-cell transfer that lands exactly on the cell edge

## Context

A transferable entity whose position after its tick is exactly on its cell's
edge is harvested from the source cell but never appears in the destination.
The entity is silently lost: no row, no explosion, no warning.

Observed on the server through `/agent-harness`. Each Fighter was spawned in
cell [0,0] at local x=390 with velocity [40,0] (1.25 m per tick, so x reaches
exactly 450.0 after 48 ticks) and `arrivalGracePeriod` 3. Every such Fighter
was lost. `harvestedTransferTotal` rose by one per Fighter, but no row with its
`globalId` ever appeared in [1,0], and [1,0] held no ship of any type for more
than 300 ticks. The result was the same with or without an `UpdateFleet`, an
`UpdateShip`, a flight-plane change, `create_cell [1,0]`, or `edit_frame`. A
Fighter that had no status change at all was lost too. Fighters that started at
x=389.7 or x=433.86, which never land exactly on 450.0, arrived normally.

Verified root cause, from code in the tree this Plan lands with (primary
`80b5f51dd` plus the flight-plane climbing change):

- Source side: `engine::IsOutOfBounds` (`Engine/Source/Frame/FrameUtils.h:161`)
  treats x == `fMaxX` as outside, so `FightersPostRender` combat marks the row
  `kTransfer`. In `FightersPostRender::Transfer`,
  `FramePostRenderBase::PrepareTransferRequest`
  (`Engine/Source/Frame/FrameBase.cpp:141`) then uses `ComputeTransferDelta`
  (`FrameUtils.h:169`, `>=`/`<=`) to compute delta +1. It subtracts one cell
  width (`kfCellWidth` = 900, `Engine/Source/Frame/GridCoord.h:44`), so the
  payload position becomes x = -450.0, exactly the destination's `fMinX`.
- Destination side: `ApplySpawnStatusChange`
  (`Projects/BrokenEngineSandbox/Source/Frame/StatusChange.cpp:149`) calls
  `FightersPostRender::Spawn(SpawnInfo)`, whose `common::InsideArea` test
  (`Common/Math/MathUtils.cpp:156`) is strict on every edge. It refuses
  -450.0 and returns false, and the caller ignores the result.
- `ServerTransferManager::HarvestTransfers` has already counted the transfer.

The cause is that a point exactly on a shared edge belongs to neither cell, and
`PrepareTransferRequest` maps the source's edge onto the destination's edge. The
same mapping applies on every axis and in both directions: x == -450.0 maps to
+450.0. It applies to every producer that calls `PrepareTransferRequest`, which
is Fighters, Battleships, Omnis, Blasters, and Missiles.

Two consequences beyond the observed one, found by code inspection and not
observed live:

- `OmnisPostRender::Spawn(SpawnInfo)` asserts instead of refusing
  (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp:237`).
  An Omni that lands exactly on an edge therefore asserts in the destination
  rather than being lost.
- `Engine/Source/Frame/AGENTS.md` `## Architecture` currently records this loss
  as a known consequence ("a transfer arrival that rebases exactly onto the
  destination edge of a refusing collection is refused and lost"). It was
  accepted when commit `a98e2fa17` moved the bounds test into every
  `Spawn(SpawnInfo)` to end an endless per-tick shuttle between two cells. That
  commit's rationale covers ending the shuttle. It does not mention losing an
  owned fleet ship to an exact-edge float coincidence, or an assert reachable
  through Omnis.

Primary commit `80b5f51dd` also documented the loss for harness users:
`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`, in the
cross-cell coast recipe, warns them to pick start positions whose per-tick
steps never land exactly on the edge, because Blasters drop such a row and
Omnis assert on it.

The behavior predates the flight-plane climbing work that surfaced it: every
bounds test, `PrepareTransferRequest`, and the Omni assert above are the same at
`45c83a786` and at `80b5f51dd`.

## Design

The author recommends fixing the single conversion point and leaving the
bounds predicates unchanged. In `FramePostRenderBase::PrepareTransferRequest`,
after the cell-width rebase, check each axis whose delta is nonzero. If the
rebased coordinate equals that axis's destination edge exactly (`fMinX`/`fMaxX`,
`fMinY`/`fMaxY` from the `rBounds` already passed in), move it one float step
inward with `std::nextafter` toward zero. The payload then always lands strictly
inside the destination.

Rationale:

- `PrepareTransferRequest` is documented as the one place a transfer position is
  converted, and every downstream consumer forwards the position unchanged. One
  local edit therefore covers all five producers, live play, and replay.
- No shuttle returns. The arrival is strictly inside, so `IsOutOfBounds` is false
  in the destination unless the entity's own motion carries it back out, and
  that is a legitimate crossing.
- `std::nextafter` and exact float comparison are exact IEEE operations under
  `/fp:strict`. `TerrainCollisionUtils.cpp` already uses `std::nextafter` in
  simulation code, so client and server compute the same value.
- `Spawn(SpawnInfo)` keeps refusing, or asserting, for every other out-of-cell
  position. The "no caller is exempt" rule stands.

Alternative the claiming session may weigh: a half-open ownership convention,
in which each cell owns its minimum edges and not its maximum edges. That means
changing `common::InsideArea`, `engine::IsOutOfBounds`, `ComputeTransferDelta`,
`TracePointToCellExit`, and the collision cell-exit cutoffs together. The
author does not recommend it. `InsideArea` is a Common predicate with client and
render consumers, and the change would shift many more CRC-affecting decisions
for the same outcome.

Exposure:

- Determinism/CRC: this changes the CRC'd position of every exact-edge arrival.
  The author recommends raising the base of `Frame::kiVersion`
  (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:67`) by one, as
  `a98e2fa17` did for its edge decision, so older saves and replays are
  rejected.
- No wire, `FrameInput`, `.pack`, threading, allocation, or shader change.
  `PrepareTransferRequest` runs inside the per-cell tick, so the edit must stay
  allocation-free and thread-local.

Change Workflow tier: Tier 3. Trigger: a determinism/CRC change with a
save/replay compatibility bump (`Frame::kiVersion`).

## Critical files

- `Engine/Source/Frame/FrameBase.cpp` — `FramePostRenderBase::PrepareTransferRequest`
- `Engine/Source/Frame/FrameBase.h` — the `PrepareTransferRequest` declaration comment
- `Engine/Source/Frame/AGENTS.md` — `## Architecture` bullet on in-cell positions
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp` — `Frame::kiVersion` base
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md` — the
  exact-edge warning in the cross-cell coast recipe

## In scope

- `FramePostRenderBase::PrepareTransferRequest`: after the rebase, move each
  transferred axis's coordinate that lands exactly on the destination edge one
  float step inward.
- The `PrepareTransferRequest` comment in `FrameBase.h`, extended to state that
  the rebased position is strictly inside the destination cell.
- `Engine/Source/Frame/AGENTS.md` `## Architecture`: replace the sentence
  recording that an exact-edge transfer arrival is refused and lost with a
  statement that `PrepareTransferRequest` always rebases strictly inside the
  destination. Keep the rest of the bullet.
- `Frame::kiVersion` base in `Frame.cpp`: raise it by one.
- `commands-server.md` cross-cell coast recipe: delete the sentence beginning
  "For any moving row, choose a start position", through the closing
  parenthesis and period after its Frame invariants link, keeping "another
  cell." before it. It warns that an exact-edge row is dropped by Blasters or
  asserts in Omnis, which the fix makes false.

## Out of scope

- `common::InsideArea`, `engine::IsOutOfBounds`, `ComputeTransferDelta`,
  `TracePointToCellExit`, and collision cutoffs. Their edge semantics stay as
  they are.
- Every collection's `Spawn(SpawnInfo)` bounds test, including the Omni assert.
- `ApplySpawnStatusChange`, `ServerTransferManager`, and the
  `harvestedTransferTotal` accounting.
- Spawns that do not come from a transfer: harness, random-wave, and queued
  spawns.

## Acceptance criteria

| Criterion | Verification |
|---|---|
| A Fighter spawned in [0,0] at local [390,0] with `vecVelocity` [40,0] and `arrivalGracePeriod` 3 appears in [1,0] with the same `globalId`, at local x strictly greater than -450, within a few ticks of `harvestedTransferTotal` rising to 1 | `/agent-harness`: `reset`, `inject_payload` `SpawnFighter`, unpause, `query_collection` fighters in [0,0] and [1,0], `status` `harvestedTransferTotal` (`cross-cell.md`) |
| The mirrored westward case arrives: a Fighter in [0,0] at local [-390,0] with `vecVelocity` [-40,0] appears in [-1,0] at local x strictly less than 450 | the same harness sequence |
| Fighters that do not land on the edge (start x=389.7) still arrive as before | the same harness sequence |
| No client desync while a connected client watches both cells during the edge arrival, and a replay recorded over it plays back with matching CRCs | `/agent-harness` client launch and replay determinism check |

## Notes

- Reproduction needs no setup beyond `reset`. In the lost case, [1,0] never
  becomes active, because the refused arrival leaves the destination with no
  owned entity.
- The Omni assert path above is inferred from code. A claiming session that
  wants live proof can try an Omni with the same edge-exact motion, but the
  recommended fix covers it either way.
