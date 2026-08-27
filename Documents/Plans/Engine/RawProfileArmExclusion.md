<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:52:28.200Z","dependsOn":[]} -->
# Reject a second armed raw-profile event

## Context

The retained survivor `CAI/shard-0038/002` identifies a one-slot event
occupancy gap. `ArmRawCpuTimerEventLocked` checks only event registration,
availability, and overrun at `Engine/Source/Profile/ProfileManagerBase.cpp:367-375`,
then sets `kEventArmed` and overwrites `iMinimumSampleTick` at `:377-379`.
`PublishRawCpuTimerEvent` has one payload and clears that armed state only after
one publication (`:387-413`). The server's
`inject_status_changes` handler swaps its prepared queue after the arm succeeds
(`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:931-951`),
so a second valid arm can be accepted and committed while the first is still
pending.

The direct evidence is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0038.md:68`
and consolidated selector `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:984`. Frozen/live source
identity matches baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`; no source
was changed during routing. The command contract explicitly requires an
occupied event to reject before queue mutation.

## Design

The author's recommendation is to include `RawCpuTimerStateFlags::kEventArmed`
in the locked occupancy predicate before replacing `iMinimumSampleTick`. A
second arm must return the existing command failure and leave both the first
event and its prepared status-change transaction untouched. Retain the current
available/overrun checks, one-slot publisher, exact-sequence acknowledgement,
and accepted-tick latch timing.

## Critical files

- `Engine/Source/Profile/ProfileManagerBase.cpp:367-444` — arm, publish, and
  acknowledgement state.
- `Engine/Source/Profile/ProfileManagerBase.h:66-71` — raw-event flags.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:931-951`
  — status-change transaction and arm command.
- `Engine/Source/Profile/AGENTS.md` and
  `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` — one-slot event and
  reject-before-mutation contracts.

## In scope

- Occupancy validation for a pending raw CPU timer activation event.
- Rejection ordering relative to the prepared status-change queue swap.
- Existing publication, sequence acknowledgement, overrun, and accepted-latch
  behavior.

## Out of scope

- CPU/GPU timer collection, profiling names, event payload layout, or raw sample
  math.
- Adding an event queue, changing the one-slot diagnostic contract, or changing
  simulation/replay behavior.
- Unrelated agent command validation.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Trigger: an external
agent command mutates a mutex-protected cross-thread diagnostic event and a
deterministic status-change transaction.

Preserve these invariants:

- One armed event owns the one retained payload until acknowledgement.
- A rejected second arm does not overwrite the minimum tick or commit its
  status-change queue.
- Available, overrun, exact-sequence, and accepted-tick paths retain current
  behavior.

Tier rationale: the fix is one added flag term in an existing locked occupancy
predicate, fully specified in the Design, and it only rejects a duplicate
request through the command failure path that already exists. No locking
structure, event payload, wire format, or valid-data behavior changes.

## Acceptance criteria

- Two valid arm requests before the first latch yield one success and one
  occupied failure; the first minimum tick and queue remain unchanged.
- The first event publishes one identifiable record and a subsequent arm is
  accepted only after the normal acknowledgement/availability transition.
- A rejected arm leaves no partially committed status changes.
- Client and server `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0038/002`; source selector is the shard line above and the
consolidated selector is `consolidated-index.md:984`. No source fix or build
was performed during routing.
