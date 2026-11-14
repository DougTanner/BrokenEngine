<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T14:22:37.824Z","dependsOn":[]} -->
# Make RemoveIndexableElement reuse DestroyElement

## Context

Found by the in-function analysis phase of a deep analysis of
`Engine/Source/Frame/Collections` (2026-11-14). Line numbers are from commit
`ae681f03` plus the change this Plan landed with; locate by symbol if they
moved.

In `Engine/Source/Frame/Collections/CollectionLifecycle.h`:

1. `RemoveIndexableElement` (`:103-119`) repeats the whole body of
   `DestroyElement` (`:186-192`): the same last-row test, the same two
   `SwapElement` calls, and the same two count decrements. It adds only the
   `lastId` capture, the map re-point of the moved row, and the map erase.
2. Its first line, `ASSERT(rInterpolate.iCount > 0)` (`:103`), restates what
   the next line enforces: `idToIndexMap.at(id)` already fails for an empty
   collection. Both callers
   (`Engine/Source/Frame/Collections/PointLights/PointLights.cpp:15`,
   `Engine/Source/Frame/FrameUtils.h:402`) pass `pIds[i]` of a live row, so the
   asserted state has no producer. `.agents/references/cpp-conventions.md`
   forbids useless ASSERTs.

Unmet criteria: one function owns the paired swap-and-pop; no assert restates
what the next line enforces.

## Design

Recommended: `RemoveIndexableElement` looks up the index, reads `lastId` from
the last row, calls `DestroyElement`, then re-points `lastId` when the removed
row was not the last and erases `id`. Delete the `ASSERT` at `:103`.

Read `lastId` before calling `DestroyElement`, which overwrites the row.

The ID map is in the collection CRC
(`Engine/Source/Frame/Collections/Collection.h:197-219`). The rewrite leaves
the same map contents: when the removed row is the last, `lastId == id`, so
any assignment is undone by the erase; and the CRC sorts keys, so the order of
map operations does not matter.

## Critical files

- `Engine/Source/Frame/Collections/CollectionLifecycle.h`

## In scope

- The body of `RemoveIndexableElement`.

## Out of scope

- `DestroyElement`, `SwapElement`, and the signature of
  `RemoveIndexableElement`.
- Both callers.

## Risk tier

Tier 2 — scoped behavior. Trigger: the function is on a determinism/CRC path
(shared collections reach it through `CompactCollections`), which Tier 1's "no
invariant exposure" excludes; it stays Tier 2 because the change does not
alter what that path computes.

## Acceptance criteria

- `RemoveIndexableElement` contains no `SwapElement` call and no count
  decrement of its own, and no `ASSERT` on `iCount`.
- `/compile` builds the BrokenEngineSandbox client and server.
- A replay determinism check through `/agent-harness`, on a scene in which
  id-indexed rows are destroyed, reports no CRC mismatch. The supplied evidence
  records no harness setup, so the executing session builds the scene.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
