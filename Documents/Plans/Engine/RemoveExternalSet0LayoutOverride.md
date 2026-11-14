<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:53:24.949Z","dependsOn":[]} -->
# Remove the external Set 0 layout override from PipelineInfo

## Context

Paths are relative to `Engine/Source/Graphics/Objects/`. Verified against
commit `c675b6b2`.

`PipelineInfo::vkExternalDescriptorSetLayout` (`Pipeline.h:105-106`) has one
writer and one reader. The writer, `ModelPipeline::Create`
(`ModelPipeline.cpp:133`), assigns
`gpTextureManager->mTextureDescriptors.mGlobalVkDescriptorSetLayout`. The
reader, `Pipeline::Create` (`Pipeline.cpp:117`), uses the field when it is
non-null and otherwise that same global layout. Both arms yield the same
handle, so the field can carry only one effective value. Every `PipelineInfo`
producer uses designated initializers, so removing the member breaks no
positional initializer.

In the same function, the index-start check at `ModelPipeline.cpp:103` reads
`(i > 0 && iIndexStart < iPreviousIndexStart) || ...`. `iPreviousIndexStart`
starts at 0 (`:99`) and `iIndexStart` is widened from an unsigned
`uint32_t` (`:96`, `:102`), so at `i == 0` the comparison is already false
and the `i > 0` conjunct changes no result. It makes the check read as if the
first entry were exempt.

Origin: an architecture review and an in-function refactor review of
`Engine/Source/Graphics/Objects`, independently verified. Unmet criterion:
root `AGENTS.md` `## Directives` "no speculative ... configuration".

## Design

Recommended:

- Delete `PipelineInfo::vkExternalDescriptorSetLayout` and its comment, delete
  the assignment in `ModelPipeline::Create`, and have `Pipeline::Create`
  assign `mGlobalVkDescriptorSetLayout` to `mExternalVkDescriptorSetLayout`
  unconditionally.
- Delete `i > 0 &&` and the inner parentheses it needed from the index-start
  check. The `std::ios_base::failure` throw stays whole: it is the pack trust
  boundary `AGENTS.md` `## Resource Contracts` names, and every input is
  accepted or rejected as before.

## Critical files

- `Engine/Source/Graphics/Objects/Pipeline.h`
- `Engine/Source/Graphics/Objects/Pipeline.cpp`
- `Engine/Source/Graphics/Objects/ModelPipeline.cpp`

## In scope

- `PipelineInfo::vkExternalDescriptorSetLayout` and its comment.
- The `mExternalVkDescriptorSetLayout` assignment in `Pipeline::Create`.
- In `ModelPipeline::Create`: the `vkExternalDescriptorSetLayout` assignment,
  and the `i > 0` conjunct of the index-start check.

## Out of scope

- The member `Pipeline::mExternalVkDescriptorSetLayout`:
  `PipelineCreator::CreateComputePipeline` clears it for standalone compute
  pipelines.
- `PipelineInfo::vkExternalSet1DescriptorSetLayout` and the shared Set 1
  hand-off in `ModelPipeline::Create`.
- The rest of the index-start check, its throw, and the other scene-header
  checks.

## Risk tier

Tier 2 — scoped behavior. Trigger: a field is removed from `PipelineInfo`, a
struct declared in a header, which Tier 1's "no public signature exposure"
excludes, and one edit is inside a pack trust-boundary check. It stays Tier 2
because the layout handle used and the set of rejected inputs are unchanged.
Client-only; no determinism/CRC, wire, serialization, `.pack`, replay,
threading, or shader exposure.

## Acceptance criteria

- A repository search for `vkExternalDescriptorSetLayout` returns nothing.
- The index-start check in `ModelPipeline::Create` has no `i > 0` conjunct and
  still throws `std::ios_base::failure`.
- The client and ModelViewer build.

## Coordination

These Plans edit other regions of the same files. None depends on this Plan,
and it depends on none. Whichever lands second re-derives its regions by
symbol from the landed files:

- `Documents/Plans/Engine/RemovePipelinePushConstantBytes.md` (`Pipeline.h`,
  `Pipeline.cpp`)
- `Documents/Plans/Engine/SharePipelineDescriptorSetBinder.md` (`Pipeline.h`,
  `Pipeline.cpp`)
- `Documents/Plans/Engine/RemoveUnproducedGraphicsFlagValues.md`
  (`Pipeline.h`)
- `Documents/Plans/Engine/ExplicitModelDescriptorBindings.md`
  (`ModelPipeline.cpp`)
- `Documents/Plans/Engine/ModelPipelineOwnsMaterialsBuffer.md` (`Pipeline.h`,
  `Pipeline.cpp`, `ModelPipeline.cpp`)

## Notes

- `mGlobalVkDescriptorSetLayout` has no other reader outside
  `Engine/Source/Graphics/Managers/TextureDescriptors.cpp`.
- Nothing was built or run by the authoring session; the evidence is from
  reading and searching the tree.
