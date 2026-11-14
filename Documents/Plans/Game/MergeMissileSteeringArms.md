<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:10:04.259Z","dependsOn":[]} -->
# Merge the two Missile steering arms in Missiles Update

## Context

Line numbers are from commit `022915b6`; locate by symbol if they moved.
Nothing was built or run for this Plan: every statement is from a code read.

In `MissilesPostRender::Update`
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesUpdate.cpp`)
the `if (bRetained)` arm (lines 160-173) and its `else` arm (174-186) run the
same six steps: subtract the frame delta from `deltaRotationDelay`, take the
Z of the cross product of the current direction with a wanted direction, pick
a signed turn rate from its sign, compute the delay percentage, scale the
rate by it, and blend it into `fDeltaRotation`. They differ in two operands
only:

- the wanted direction: the normalized vector to `retained.vecPreviousPosition`
  (lines 164-166) against `vecStoredDirection` (lines 178-179);
- the rate constant: `kfDeltaRotationTowardsTarget` (line 167) against
  `kfDeltaRotationTowardsStored` (line 180).

Originating gap: an in-function refactor review of the Missiles collection,
not an unmet acceptance criterion of any change.

## Design

Recommended: keep one copy of the six steps in place. Before them, choose the
wanted direction and the rate: `vecStoredDirection` and
`kfDeltaRotationTowardsStored` by default, replaced inside `if (bRetained)`
by the normalized vector to the retained target and
`kfDeltaRotationTowardsTarget`. No new function.

Two constraints:

- The default is taken after the target-loss block (lines 149-157), because
  line 155 reassigns `vecStoredDirection`.
- Every floating-point expression keeps its operands and their order, and the
  cross product keeps the current direction as its first argument, so the
  result is bit-identical under `/fp:strict`
  (`Documents/FloatingPointDeterminism.txt`).

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesUpdate.cpp`

## In scope

- `MissilesPostRender::Update`: the `if (bRetained)` / `else` steering arms.

## Out of scope

- The target-loss block, the jitter block, and every other statement of
  `MissilesPostRender::Update`.
- The steering constants and their values.
- What `Transfer` sends as the stored direction.
- Any `kiVersion`: the computed values do not change.

## Risk tier

Tier 2 — scoped behavior (the author's classification). Trigger: the edit
rewrites statements that compute CRC'd simulation state. It is meant to
compute the same bits, so it does not change the determinism surface, but
that exposure rules out Tier 1 and the replay check below settles it. No
wire, serialization, threading, allocation, or shader exposure.

## Acceptance criteria

1. One copy of the six steering steps remains in `MissilesPostRender::Update`.
2. `/compile` builds the BrokenEngineSandbox client and server.
3. Through `/agent-harness`, a replay recorded on the build before the change,
   in which missiles fly both with and without a target, plays back on the
   changed build with no per-tick CRC mismatch. The supplied evidence records
   no harness setup, so the executing session builds the scene.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
