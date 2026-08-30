<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:49:37.221Z","dependsOn":[]} -->
# Keep newly fired Blasters inside their owning cell

## Context

The accepted finding `CAI/shard-0046/002` identifies a fixed-tick ownership
gap.  `BlastersPostRender::Spawn` validates finite vector lanes but appends any
position without a frame-boundary check (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.cpp:119-147`).
`PlayersPostRender::SpawnBlasters` adds an alternating lateral barrel offset to
a legal near-boundary player position and calls that overload in the final
Destroy/Spawn phase (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp:328-369`).
The new row therefore misses the current tick's collision/transfer phases.  On
the next tick an inward-moving row starts outside, so `TracePointToFrameExit`
forces a time-zero boundary event and the collision cutoff suppresses the
swept collision (`Engine/Source/Frame/FrameUtils.h:67-87`; `Engine/Source/Frame/Collision.cpp:611-615`).
The endpoint can then produce a zero-delta transfer.

The source finding is recorded at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0046.md:68`
and the consolidated selector at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1116`.
Assigned source and authority hashes match frozen baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; the late-spawn ownership gap is
pre-existing, unresolved, and outside the audit work.

Impact: a projectile can cross an in-cell target without collision and then be
re-materialized as a same-cell transfer, violating the earliest-event and
cell-ownership contracts.

## Design

Author's recommendation: make the player weapon spawn boundary use the current
cell's `FrameStaticData` bounds before appending a Blaster and refuse every
out-of-cell muzzle/final position through the existing spawn-rejection
behavior.  Do not let a row enter PostRender outside its owner and rely on the
next tick to repair it.  Keep normal in-cell firing, velocity, random draws,
and collision/transfer ordering unchanged.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp:303-376` — barrel-offset producer and final-phase spawn call.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:220-236` — player spawn helper contract.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.cpp:119-147` and `Blasters.h:99-135` — shared spawn boundary.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:337-362` — fixed-tick phase ordering (read-only evidence).
- `Engine/Source/Frame/FrameUtils.h:56-65,67-87,132-139` — frame bounds and boundary predicates.

## In scope

- Cell-boundary validation for player-fired Blaster spawn positions before the
  row is appended, including the required `FrameStaticData`/spawn contract
  propagation.
- Existing refusal needed to preserve the first collision/transfer event for an
  out-of-cell muzzle.
- Valid in-cell Blaster initialization and the deterministic fixed-tick phase
  order.

## Out of scope

- Collision algorithm/tie policy, boundary tracing, endpoint transfer delta
  rules, barrel dimensions, firing timers, or enemy projectile producers.
- Clamping an illegal position into the source cell, new cross-cell spawn
  queues, or protocol/save/replay format changes.
- Generic vector validation or unrelated collection ownership.

## Risk tier and invariants

Expected Change Workflow Tier 3.  Trigger: deterministic fixed-tick collision,
cell transfer, and projectile spawn phases must agree across client/server.

Preserve these invariants:

- Every newly created Blaster is inside the frame that owns it before its first
  collision phase, or is routed with an explicit ownership decision.
- A boundary event cannot suppress a valid swept collision on an inward return
  segment or create a misleading same-cell transfer.
- Valid firing, random-draw order, shared CRC, and transfer wire fields remain
  unchanged.

## Acceptance criteria

- A legal player placed near each cell edge with the alternating barrel offset
  cannot publish an out-of-cell Blaster in the source frame.
- A refused edge shot does not miss an in-cell target on an inward segment and
  does not create a zero-delta repair transfer.
- Ordinary in-cell player and enemy firing retains current positions, timing,
  collision, transfer, and CRC behavior.
- Client and server `Debug|x64` builds clean through `/compile`; a focused
  fixed-tick edge-of-cell scenario proves the first-event outcome.

## Notes

The report distinguishes this late player-spawn boundary from normal Blaster
collision handling and from the generic registry/type-index candidates.
The later scope-boundary audit independently retained the same root cause as
`CSB/shard-0048/001` (SERIOUS_CONFIRMED, HIGH) at durable source
`80896f33661aaab99cf180a96db54600099be652`; its consolidated triage is
`Temp/CppScopeBoundaryAudit/80896f33661aaab99cf180a96db54600099be652/triage-0001.md`
(SHA-256 `ea32fd84e984dea3d14ed1993c62adf54b86b0c81fed41e3f0ff665c035a3150`).
The current source still has the same late-spawn path at
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp:303-369`
and the unbounded shared append at
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.cpp:119-147`.
