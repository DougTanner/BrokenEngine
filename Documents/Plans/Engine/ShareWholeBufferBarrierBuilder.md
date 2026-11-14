<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:15:00.401Z","dependsOn":[]} -->
# Build whole-buffer barriers through one function

## Context

A path without a directory is under `Engine/Source/Graphics/Managers/`. Lines
are those at commit `401e4b75`, except that the `CommandBufferRecordGlobal.cpp`
and `BufferManager.cpp` lines reflect the authoring session's later edits to
those files.

Fifteen `VkBufferMemoryBarrier` initializers are written field by field:
thirteen in `CommandBufferRecordGlobal.cpp` (`:118-139` two, `:152-195` four,
`:205-226` two, `:249-260` one, `:270-291` two, `:307-318` one, `:333-343`
one), one at `BufferManager.cpp:604-615`, and one at
`RenderTargetTextures.cpp:356-367`. Every one sets the same nine fields and
varies only `srcAccessMask`, `dstAccessMask`, and `buffer`.

`Buffer::RecordBarriers`
(`Engine/Source/Graphics/Objects/Buffer.cpp:115-160`) cannot serve them: it
fixes the source access and the source stage, offers three destinations, and
issues its own `vkCmdPipelineBarrier`, while these sites group barriers with
different source accesses under one call with call-site stage masks. So this
is uncovered duplication, not a bypassed helper.

Originating gap: the architecture phase (cohesion lens) and the in-function
phase of an analysis of `Engine/Source/Graphics/Managers` at that commit,
which reported the same blocks. Unmet criterion: no whole-buffer barrier in
the three files is written field by field.

## Design

Recommended: one static function beside `Buffer::RecordBarriers` that returns
one whole-buffer `VkBufferMemoryBarrier` from (source access, destination
access, buffer), and the fifteen sites call it. Rationale: three files use
it, so it belongs with the buffer helpers; three parameters replace eleven
lines per use.

Stage masks, the grouping of barriers under each `vkCmdPipelineBarrier` call,
and the order of calls stay at the call sites, so the recorded commands are
unchanged.

## Critical files

- `Engine/Source/Graphics/Objects/Buffer.h`
- `Engine/Source/Graphics/Objects/Buffer.cpp`
- `Engine/Source/Graphics/Managers/CommandBufferRecordGlobal.cpp`
- `Engine/Source/Graphics/Managers/BufferManager.cpp`
- `Engine/Source/Graphics/Managers/RenderTargetTextures.cpp`

## In scope

- One new static function in `Buffer`.
- The fifteen initializers listed in `## Context`.

## Out of scope

- `Buffer::RecordBarriers` and `BarrierInfo`: both stay as they are.
- Image barriers and barriers that cover a sub-range.
- Stage masks, barrier grouping, and call order at every site.
- A shared occupancy zero-fill function for smoke and wind.

## Risk tier

Tier 2 — scoped behavior. Trigger: it adds a public static function to
`Buffer`, a header `Engine.h` aggregates, which Tier 1's "no public
signature" excludes. No threading, determinism, layout, or wire exposure; the
recorded barriers are field-for-field the same.

## Acceptance criteria

1. None of the fifteen sites sets the barrier fields one by one, and
   `RecordBarriers` and `BarrierInfo` are unchanged.
2. The BrokenEngineSandbox client and ModelViewer build through `/compile`.
3. Through `/agent-harness`, a client run with validation layers on renders a
   world frame with smoke and wind active with no synchronization or other
   validation error. The supplied evidence records no harness setup.

## Coordination

None of these Plans depends on another. Whichever lands later re-derives its
lines by symbol from the landed files:

- `Documents/Plans/Engine/BufferManagerDeadStateAndDuplicates.md` turns
  `miSmokeActiveTileBufferSize` (`BufferManager.cpp:622-623`) into a local,
  next to the barrier at `:604-615`.
- `Documents/Plans/Engine/RenderTargetTextureCreationCleanup.md` replaces the
  image clear at `RenderTargetTextures.cpp:343-351`, next to the barrier at
  `:356-367`.
