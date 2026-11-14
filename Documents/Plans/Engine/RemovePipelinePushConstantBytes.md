<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:52:56.989Z","dependsOn":[]} -->
# Remove the unproduced push-constant size override

## Context

Paths are relative to `Engine/Source/Graphics/Objects/`. Verified against
commit `c675b6b2`.

`PipelineInfo::iPushConstantBytes` (`Pipeline.h:115-117`) lets a pipeline
declare a push-constant range smaller than `shaders::PushConstantsLayout`. No
file assigns it. Its only references are the declaration, the reader
`ResolvePushConstantBytes` (`PipelineCreator.cpp:16-21`, called at `:78`,
`:226`, `:691`), the comment above `RecordPushConstants`
(`Pipeline.cpp:15-16`), and six `ASSERT(mInfo.iPushConstantBytes == 0)` lines
(`Pipeline.cpp:223`, `:239`, `:254`, `:277`, `:299`, `:328`). The search
covered `Engine`, `Projects`, `Tools`, `DataPacker`, and `Common`.

The member comment names LightCombine as the pipeline that would use the
override. `Engine/Data/Shaders/Lighting/LightCombine.comp` has no
`push_constant` block, and its pipeline
(`Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.cpp:83`) does
not set `kPushConstants`. Every `push_constant` block under
`Engine/Data/Shaders` declares `PushConstantsLayout`
(`Engine/Data/Shaders/ShaderLayoutsBase.h:210`). So the three size sites
always pass `sizeof(shaders::PushConstantsLayout)` and the six ASSERTs can
never fire.

Separately, the comment at `PipelineCreator.cpp:594` in
`PipelineCreator::CreateGraphicsPipeline` says the shader modules, the layout,
and the render pass are resolved after `CreateDescriptorSetLayouts`. Only the
layout (`:598`) depends on that call: the modules (`:595-596`) read
`mVkShaderModule`, which the call does not write, and the render pass (`:601`)
reads `mInfo` and the swapchain manager.

Origin: an architecture review and an in-function refactor review of
`Engine/Source/Graphics/Objects`, each finding independently verified. Unmet
criteria: root `AGENTS.md` `## Directives` "no speculative ... configuration",
and `Documents/C++StyleGuide.txt` rule 64 (every statement a comment makes
must be true).

## Design

Recommended:

- Delete `PipelineInfo::iPushConstantBytes` and its comment.
- Delete `ResolvePushConstantBytes` and its comment; at its three call sites
  use `sizeof(shaders::PushConstantsLayout)` directly.
- Delete the six `ASSERT(mInfo.iPushConstantBytes == 0)` lines and trim the
  comment above `RecordPushConstants` so it no longer names the override.
- Reword the `PipelineCreator.cpp:594` comment so it claims the ordering only
  for the pipeline layout.

The six `if (mInfo.flags & kPushConstants)` blocks keep their shape. Folding
the flag test into `RecordPushConstants` was reviewed and judged cosmetic: it
would make a function named "record" silently do nothing.

## Critical files

- `Engine/Source/Graphics/Objects/Pipeline.h`
- `Engine/Source/Graphics/Objects/Pipeline.cpp`
- `Engine/Source/Graphics/Objects/PipelineCreator.cpp`

## In scope

- `PipelineInfo::iPushConstantBytes` and its comment.
- `ResolvePushConstantBytes` and the `vkPushConstantRange.size` assignment in
  `CreateSingleSetPipelineLayout`, `CreateDescriptorSetLayouts`, and
  `PipelineCreator::CreateComputePipeline`.
- The `iPushConstantBytes` ASSERT in `Pipeline::RecordDraw`,
  `RecordBindPipelineAndDescriptors`, `RecordDrawIndirect`,
  `RecordDrawIndirectSet2`, `RecordCompute`, and `RecordComputeIndirectFrom`,
  and the comment above `RecordPushConstants`.
- The comment above the shader-module assignments in
  `PipelineCreator::CreateGraphicsPipeline`.

## Out of scope

- The `kPushConstants` flag, its producers, and the six guard blocks' shape.
- Moving the depth-test, depth-write, or shader-module assignments in
  `CreateGraphicsPipeline` into their initializers.
- Where the push-constant range is built and how the layout helpers share it;
  `Documents/Plans/Engine/ComputeLayoutReusesSetHelpers.md` owns that.
- Any shader or `PushConstantsLayout` change.

## Risk tier

Tier 2 — scoped behavior. Trigger: a field is removed from `PipelineInfo`, a
struct declared in a header, which Tier 1's "no public signature exposure"
excludes. No runtime behavior changes: the values passed to Vulkan are the
same. Client-only; no determinism/CRC, wire, serialization, `.pack`, replay,
threading, or shader exposure.

## Acceptance criteria

- A repository search for `iPushConstantBytes` and `ResolvePushConstantBytes`
  returns nothing.
- No comment in `CreateGraphicsPipeline` says the shader modules or the render
  pass depend on `CreateDescriptorSetLayouts`.
- The client and ModelViewer build.

## Coordination

`Documents/Plans/Engine/ComputeLayoutReusesSetHelpers.md` depends on this
Plan through metadata. These Plans edit other regions of the same files. None
depends on this Plan, and it depends on none of them. Whichever lands second
re-derives its regions by symbol from the landed files:

- `Documents/Plans/Engine/SharePipelineDescriptorSetBinder.md` (`Pipeline.h`,
  `Pipeline.cpp`)
- `Documents/Plans/Engine/RemoveExternalSet0LayoutOverride.md` (`Pipeline.h`,
  `Pipeline.cpp`)
- `Documents/Plans/Engine/ModelPipelineOwnsMaterialsBuffer.md` (`Pipeline.h`,
  `Pipeline.cpp`)
- `Documents/Plans/Engine/RemoveUnproducedGraphicsFlagValues.md`
  (`Pipeline.h`, `PipelineCreator.cpp`)

## Notes

- The private game project in the primary checkout holds only documents and no
  source, so it names none of these symbols.
- Nothing was built or run by the authoring session; the evidence is from
  reading and searching the tree.
