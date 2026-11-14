<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T14:22:46.798Z","dependsOn":[]} -->
# Give the collection buffer publish sequence one owner

## Context

Found by the in-function analysis phase of a deep analysis of
`Engine/Source/Frame/Collections` (2026-11-14). Line numbers are from commit
`ae681f03`; locate by symbol if they moved.

In `Engine/Source/Frame/Collections/CollectionMemory.h`, `AllocateAndAssign`
(`:136-144`) and `Allocate` (`:211-219`) end with the same sequence, with only
the struct name changed: set `iCapacity`, set `iPhysicalLayoutCapacity`, move
the new buffer into `pData`, then walk the member tuple with `AssignAligned`.
They differ before it — reuse test (`:113`), byte ceiling (`:122`), and
failure path (`:124-125`, `:131-132` against `:208`).

The sequence is the one `Engine/Source/Frame/Collections/AGENTS.md`
`## Core Contract` constrains: "Allocation failure must not publish a capacity
or layout that was not installed". Two copies mean two places that must keep
that order.

Unmet criterion: one function publishes capacity, physical capacity, and
buffer and walks `AssignAligned`.

## Design

Recommended: one helper in `CollectionMemory.h` holding the shared sequence,
called from both sites. It takes the member tuple as a forwarding reference.
The `ASSERT` at `:221` stays in `Allocate`.

The change is mechanical: the same three fields are set in the same order.
This is the allocation core for every collection, and `iCapacity` is in the
collection CRC (`Engine/Source/Frame/Collections/Collection.h:261`).

## Critical files

- `Engine/Source/Frame/Collections/CollectionMemory.h`

## In scope

- The publish-and-assign tail of `AllocateAndAssign` and of `Allocate`, and
  the one new helper.

## Out of scope

- `GrowCapacityWithCopy` (`:320-341`): it copies before replacing `pData`, so
  its sequence is not the same.
- The reuse test, byte ceiling, and failure paths of both functions.

## Risk tier

Tier 2 — scoped behavior. Trigger: the code is the allocation core on a
determinism/CRC path (`iCapacity` is CRC-checked), which Tier 1's "no
invariant exposure" excludes; it stays Tier 2 because the change does not
alter what that path computes.

## Acceptance criteria

- The publish-and-assign sequence appears once in `CollectionMemory.h`.
- `/compile` builds the BrokenEngineSandbox client and server.
- A replay determinism check through `/agent-harness` reports no CRC mismatch.
  The supplied evidence records no harness setup, so the executing session
  builds the scene.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
