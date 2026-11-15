<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-15T20:26:16.349Z","dependsOn":[]} -->
# Skip descriptor writing for model material pipelines that share Set 1

## Context

`O/` is `Engine/Source/Graphics/Objects/` and `PDW` is
`O/PipelineDescriptorWriter.cpp`. Line numbers are approximate and move as
edits land, so re-find each region by symbol. Origin: an
`/over-engineering-review` design suggestion raised after the model
per-material descriptor set was retired; the gap is root `AGENTS.md`
`## Directives` minimum sufficient change and YAGNI (code whose only effect is to
be discarded).

A multi-set model (`PipelineFlags::kMultiSet`, set only by the model pipeline
in `Engine/Source/Graphics/Managers/DynamicPipelines.cpp:109`) creates one
`Pipeline` per material in `ModelPipeline::Create` (`O/ModelPipeline.cpp:133-167`).
Pipeline 0 owns Set 1; pipelines 1..N receive pipeline 0's layout through
`PipelineInfo::vkExternalSet1DescriptorSetLayout` (`O/ModelPipeline.cpp:156-161`),
which `Pipeline::Create` copies to `mExternalSet1VkDescriptorSetLayout`
(`O/Pipeline.cpp:91`). `ModelPipeline::RecordDrawIndirect` binds pipeline 0's
Set 1 once for every material (`O/ModelPipeline.cpp:176-182`), and
`ModelPipeline::UpdateStorageBufferDescriptors` updates only pipeline 0's sets
(`O/ModelPipeline.cpp:244-248`).

Such an inner pipeline owns no descriptor set, yet `Pipeline::Create` still
calls `PipelineDescriptorWriter::Write` (`O/Pipeline.cpp:117`), which for it:

- skips the `mVkDescriptorSets` resize and the per-framebuffer allocation
  behind `bHasExternalSet1` guards (`PDW:210`, `PDW:235`), leaving the
  destination set `VK_NULL_HANDLE`;
- still builds every buffer and image write for each framebuffer, filters them
  by shader layout, then has `RouteWritesBySet` drop all of them, Set 0 because
  it is global and Set 1 because of the `!bHasExternalSet1` test (`PDW:181`),
  before calling `vkUpdateDescriptorSets` with a count of zero (`PDW:316`);
- on framebuffer 0 registers every combined-sampler binding with the
  texture-descriptor registry (`PDW:298-301`, `RegisterCombinedSamplerBindings`)
  and appends its texture CRCs to `Pipeline::mTextureCrcs`. Every model
  combined-sampler binding is in Set 1 (`Engine/Data/Shaders/Model/Model.frag:50-64`,
  `Engine/Data/Shaders/Model/ModelCommon.h:84`), so `ShouldRegisterBinding`
  admits all of them.

Those registrations write nothing: `TextureDescriptors::WriteBindingToSet`
returns early for a pipeline with no sets
(`Engine/Source/Graphics/Managers/TextureDescriptors.cpp:225-229`, whose
comment says pipeline 0's own registration writes the shared set), and
`TextureDescriptors::RewriteSamplerDescriptors` loops over an empty
`mVkDescriptorSets` (`TextureDescriptors.cpp:599`). Pipeline 0 is created from
the same `descriptorInfos` and so registers the same bindings and CRCs, and
`ModelPipeline::WriteIndirectBuffer` calls `Pipeline::WriteIndirectBuffer` on
every material pipeline including pipeline 0 (`O/ModelPipeline.cpp:236-239`),
which requests pipeline 0's `mTextureCrcs` (`O/Pipeline.cpp:291-296`). Not yet
checked: whether `TextureDescriptors::VerifyAllDescriptorGenerations` or another
registry reader depends on the inner pipelines' registration entries.

The cost is creation-time only, not per frame.

## Design

Recommended:

1. Confirm by reading every reader of `mTextureBindings` and
   `mBindlessArrayConsumers` in `TextureDescriptors.cpp`, and every reader of
   `Pipeline::mTextureCrcs`, that no behavior depends on an inner pipeline's
   registrations or CRCs. If one does, stop and surface it for re-planning
   instead of widening scope.
2. In `Pipeline::Create`, call `PipelineDescriptorWriter::Write` only when
   `mExternalSet1VkDescriptorSetLayout` is `VK_NULL_HANDLE`. The texture
   request that follows it stays as it is: an inner pipeline is indirect, so
   that request never applies to it.
3. In `PipelineDescriptorWriter::Write`, delete `bHasExternalSet1` and the two
   guards it drives: the `mVkDescriptorSets` resize and the descriptor-set
   allocation become unconditional. Add
   `ASSERT(rPipeline.mExternalSet1VkDescriptorSetLayout == VK_NULL_HANDLE)` at
   the top so a new caller cannot reach it with a shared Set 1.
4. Remove the `bHasExternalSet1` parameter of `RouteWritesBySet`; it keeps
   Set 1 writes and drops Set 0 writes. Update the routing comment above its
   call so it no longer mentions an external Set 1.
5. In `TextureDescriptors::WriteBindingToSet`, delete the early return for a
   pipeline with no descriptor sets and its comment, since no registered
   pipeline lacks sets any more.
6. Update the `Pipeline::Destroy` comment above the descriptor-set free
   (`O/Pipeline.cpp:138`) only if the edit makes it incorrect; the
   `!mVkDescriptorSets.empty()` test stays, because inner pipelines still own
   no sets.

Risk tier: Tier 2 — scoped behavior. Trigger: one subsystem's runtime
behavior (Graphics pipeline creation and texture-descriptor registration)
changes, because inner material pipelines stop registering with the registry.
Invariants: the descriptor sets allocated, the values written to them, and
every bind are unchanged; the CRCs requested for a model are unchanged.
Client-only; no determinism/CRC, wire, serialization, `.pack`, replay,
threading, or shader exposure.

## Critical files

- `Engine/Source/Graphics/Objects/Pipeline.cpp`
- `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp`
- `Engine/Source/Graphics/Managers/TextureDescriptors.cpp`

## In scope

- `Pipeline::Create`: the `PipelineDescriptorWriter::Write` call.
- `Pipeline::Destroy`: the comment above the descriptor-set free, only if it
  becomes incorrect.
- `PipelineDescriptorWriter::Write`: `bHasExternalSet1`, the two guards it
  drives, the routing comment, the `RouteWritesBySet` call, and the new ASSERT.
- `RouteWritesBySet`: its `bHasExternalSet1` parameter and test.
- `TextureDescriptors::WriteBindingToSet`: the empty-set early return and its
  comment.
- The read-only check of `TextureDescriptors.cpp` registry readers and
  `Pipeline::mTextureCrcs` readers in Design step 1.

## Out of scope

- `PipelineInfo::vkExternalSet1DescriptorSetLayout`,
  `Pipeline::mExternalSet1VkDescriptorSetLayout`, and the Set 1 layout
  hand-off in `ModelPipeline::Create`.
- `PipelineCreator`'s use of the external Set 1 layout when building the
  pipeline layout.
- `ModelPipeline::RecordDrawIndirect`, `UpdateStorageBufferDescriptors`,
  `WriteIndirectBuffer`, and `RequestTextures`.
- The `!mVkDescriptorSets.empty()` test in `Pipeline::Destroy`.
- `Pipeline::WriteIndirectBuffer` and its texture request.
- Every other registry method and the bindless consumer path.
- The external Set 0 layout.

## Acceptance criteria

- A repository search for `bHasExternalSet1` returns nothing.
- `Pipeline::Create` does not call `PipelineDescriptorWriter::Write` for a
  pipeline with an external Set 1, and `Write` asserts it never receives one.
- `TextureDescriptors::WriteBindingToSet` has no early return for an empty
  `mVkDescriptorSets`.
- The client and ModelViewer build.
- Through `/agent-harness`, a world frame with models visible renders with the
  validation layer reporting no error, and the models are textured rather
  than on the white placeholder. The supplied evidence records no harness
  setup.
- The handoff states the result of the Design step 1 check.

## Notes

- Nothing was built or run by the authoring session; the evidence comes from
  reading and searching the tree.
