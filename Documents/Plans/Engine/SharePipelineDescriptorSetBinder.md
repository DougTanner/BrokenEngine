<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:53:17.998Z","dependsOn":[]} -->
# Share one descriptor-set binder between graphics and compute

## Context

Paths are relative to `Engine/Source/Graphics/Objects/`. Verified against
commit `c675b6b2`.

`BindComputeDescriptorSets` is declared in `Pipeline.h:190-191` and defined in
`Pipeline.cpp:65-77`. Its only callers are in the same file
(`Pipeline.cpp:305`, `:334`); the search covered `Engine`, `Projects`,
`Tools`, `DataPacker`, and `Common`. The file-static
`BindGraphicsDescriptorSets` (`Pipeline.cpp:51-63`, callers `:229`, `:245`,
`:259`) has the same body. The only difference is the bind-point constant:
`VK_PIPELINE_BIND_POINT_GRAPHICS` at `:57` and `:61` against
`VK_PIPELINE_BIND_POINT_COMPUTE` at `:71` and `:75`.

So the header exports a function nothing outside the file uses, and the rule
"global Set 0 at the framebuffer index, per-pipeline Set 1 at the
`mbPerCommandBuffer` index" is written twice.

Origin: an architecture review and an in-function refactor review of
`Engine/Source/Graphics/Objects`, independently verified. Unmet criteria: root
`AGENTS.md` `## Directives` DRY for current duplication, and no export without
a user.

## Design

Recommended: keep one file-static binder in `Pipeline.cpp` that takes a
`VkPipelineBindPoint`, pass the constant at the five call sites, and delete
the `BindComputeDescriptorSets` declaration and its comment from `Pipeline.h`.
A bind-point parameter keeps the graphics and compute paths parallel. The
comment that states the Set 0 / Set 1 indexing rule stays once, on the
remaining binder.

## Critical files

- `Engine/Source/Graphics/Objects/Pipeline.h`
- `Engine/Source/Graphics/Objects/Pipeline.cpp`

## In scope

- `BindGraphicsDescriptorSets` and `BindComputeDescriptorSets` in
  `Pipeline.cpp`, merged into one file-static function.
- The binder call in `Pipeline::RecordDraw`,
  `RecordBindPipelineAndDescriptors`, `RecordDrawIndirect`, `RecordCompute`,
  and `RecordComputeIndirectFrom`.
- The `BindComputeDescriptorSets` declaration and its comment in `Pipeline.h`.

## Out of scope

- The inline Set 0 + Set 1 bind in `ModelPipeline::RecordDrawIndirect`
  (`ModelPipeline.cpp:158-159`) and the Set 2 bind in
  `Pipeline::RecordDrawIndirectSet2`.
- The descriptor-set index each caller passes, including the unconditional
  framebuffer index in `Pipeline::RecordDrawIndirect`.
- The `kPushConstants` blocks in the same functions.

## Risk tier

Tier 2 — scoped behavior. Trigger: a function declared in a header is
removed, which Tier 1's "no public signature exposure" excludes. No runtime
behavior changes: every call binds the same sets at the same bind point.
Client-only; no determinism/CRC, wire, serialization, replay, threading, or
shader exposure.

## Acceptance criteria

- `BindComputeDescriptorSets` appears in no header, and `Pipeline.cpp` holds
  one descriptor-set binder.
- The client and ModelViewer build.

## Coordination

These Plans edit other regions of the same files. None depends on this Plan,
and it depends on none. Whichever lands second re-derives its regions by
symbol from the landed files:

- `Documents/Plans/Engine/RemovePipelinePushConstantBytes.md`
- `Documents/Plans/Engine/RemoveExternalSet0LayoutOverride.md`
- `Documents/Plans/Engine/ModelPipelineOwnsMaterialsBuffer.md`
- `Documents/Plans/Engine/RemoveUnproducedGraphicsFlagValues.md`
  (`Pipeline.h` only)

## Notes

- Nothing was built or run by the authoring session; the evidence is from
  reading and searching the tree.
