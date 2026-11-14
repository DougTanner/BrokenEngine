<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:52:53.026Z","dependsOn":[]} -->
# Merge the Two Islands Emission Loops

## Context

Found by a deep analysis of `Engine/Source/Graphics` (direct files only) and
verified independently. Pre-existing at commit `401e4b75`; line numbers are
from that commit, so locate by symbol if they moved. `G/` abbreviates
`Engine/Source/Graphics/`.

1. `Islands::UpdateActiveIslands` emits placements with two loops
   (`G/Islands.cpp:332-344` and `:350-362`) that are the same traversal of the
   active cells and differ only by the `!` on `IsMeshVisible` (`:339`,
   `:357`). With the counting loop (`:257-275`), every placement is visited
   three times per rendered frame: `IsMeshVisible` runs at `:270`, `:339`, and
   `:357`, and the per-cell `MakeRenderBasis` at `:260`, `:335`, and `:353`.
   Both run bounds are final before emission (`:289-296`). Both
   `EmitPlacement` calls pass `rTemplate.iTextureSlot` while also passing
   `rTemplate` (`:341`, `:359`).
2. `ASSERT(mIslandMeshVirtualBlock != VK_NULL_HANDLE)` in
   `Islands::AllocateMeshRanges` (`:92`) and `Islands::FreeMeshRanges`
   (`:128`) checks a state no writer can produce: the member is written once,
   by the `CHECK_VK` at `:28` in the constructor, and a failed create throws
   out of the constructor (`G/GraphicsUtils.cpp:55-61`), so no constructed
   `Islands` holds a null block. The neighbouring ASSERTs at `:129-130` differ
   and stay. This item alone has no functional effect; it is recorded here
   only because the Plan already edits the file.

Unmet criteria: one emission traversal, with visibility tested once per
placement during emission; no ASSERT on an invariant the constructor
establishes. Authority: root `AGENTS.md` `## Directives` (DRY);
`.agents/references/cpp-conventions.md` (no useless ASSERTs).

## Design

Recommended; items 1 to 3 are inside `Islands::UpdateActiveIslands`:

1. Merge the two emission loops into one that keeps two cursors per template:
   a visible placement is written at the run's base cursor and an offscreen
   one at base plus the template's mesh-visible count. The output is identical
   by construction: today a visible placement lands at base plus its rank
   among that template's visible placements in traversal order, and an
   offscreen one at base plus the visible count plus its rank among the
   offscreen ones; traversal order within each class is unchanged and
   `IsMeshVisible` is pure within the call. The indirect records (`:293-294`)
   and the stale-tail clear (`:369-376`) do not depend on emission order, and
   the bound check at `:304-309` applies unchanged.
2. The second per-template cursor needs scratch storage, so the scratch
   reservation at `:225-233` and the completeness ASSERT at `:364-367` change
   with it; keep that ASSERT, against both cursors.
3. Drop `EmitPlacement`'s `iTextureSlot` parameter and read
   `rTemplate.iTextureSlot` inside the lambda.
4. Delete the two ASSERT lines at `:92` and `:128`.

Invariant to keep (`G/AGENTS.md` `## World and Camera Contracts`): each
template's run is contiguous with mesh-visible instances first.

## Critical files

- `Engine/Source/Graphics/Islands.cpp`

## In scope

- The emission part of `Islands::UpdateActiveIslands`: the `EmitPlacement`
  lambda, the two emission loops, the per-template scratch reservation, and
  the completeness ASSERT.
- The `mIslandMeshVirtualBlock` ASSERT in `Islands::AllocateMeshRanges` and
  in `Islands::FreeMeshRanges`.

## Out of scope

- The counting loop, `IsMeshVisible`, `MakeRenderBasis`, the indirect-record
  writes, and the stale-tail clear.
- The reference counts and last-used stamps the residency step reads.
- When `UpdateActiveIslands` runs relative to the framebuffer fence wait.
- The ASSERTs at `Islands.cpp:129-130`.

## Risk tier

Tier 2 — scoped behavior. Trigger: it restructures a per-frame GPU buffer
write in one subsystem whose layout the terrain and shadow passes depend on;
the intended output is byte-identical. Client render only: no
determinism/CRC, serialization, wire, replay, or threading exposure.

## Acceptance criteria

- `UpdateActiveIslands` holds one emission loop, and `IsMeshVisible` is
  called at two sites (counting and emission).
- The BrokenEngineSandbox client and ModelViewer build.
- Through `/agent-harness`, a screenshot of an island scene with islands
  both on screen and partly off screen matches one taken before the change,
  and render-target dumps of the terrain elevation and shadow elevation
  targets match too (they read the mesh-visible prefix and the full run). The
  supplied evidence records no harness setup.

## Coordination

No Plan below depends on this one, and it depends on none. Whichever lands
second re-derives its regions by symbol from the landed file.

- `Documents/Plans/Engine/IslandsWriteAfterFramebufferFenceWait.md` edits
  the comment above the first statement of `Islands::UpdateActiveIslands` and
  the order in which its callers run it.

## Notes

- None.
