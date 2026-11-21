<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T23:51:26.414Z","dependsOn":[]} -->
# Remove the Pipeline Copy of the Global Descriptor Set Layout

## Context

`Pipeline::mExternalVkDescriptorSetLayout`
(`Engine/Source/Graphics/Objects/Pipeline.h:152-153`, with its comment) is a
copy of `gpTextureManager->mTextureDescriptors.mGlobalVkDescriptorSetLayout`:

- Sole writer: `Pipeline::Create`
  (`Engine/Source/Graphics/Objects/Pipeline.cpp:90`) assigns the global layout.
- Sole reader: `CreateMultiSetPipelineLayout`
  (`Engine/Source/Graphics/Objects/PipelineCreator.cpp:157`), reached only from
  `CreateGraphicsPipeline` and `CreateComputePipeline`
  (`PipelineCreator.cpp:181`, `:607`), which only `Pipeline::Create` calls
  (`Pipeline.cpp:110`, `:114`), synchronously after the assignment.

A repository-wide search, ModelViewer included, finds no other use. The global
layout is created once per `TextureDescriptors`
(`Engine/Source/Graphics/Managers/TextureDescriptors.cpp:68`) and destroyed only
with it, so the copy always equals the live value at its one read.
`PipelineCreator.cpp` already reads `gpTextureManager` (`:113`, `:414`).

## Design

Read `gpTextureManager->mTextureDescriptors.mGlobalVkDescriptorSetLayout`
directly in `CreateMultiSetPipelineLayout`, and delete the member, its comment,
and its assignment.

## Critical files

- `Engine/Source/Graphics/Objects/Pipeline.h`
- `Engine/Source/Graphics/Objects/Pipeline.cpp`
- `Engine/Source/Graphics/Objects/PipelineCreator.cpp`

## In scope

- `Pipeline::mExternalVkDescriptorSetLayout` and its comment: delete.
- `Pipeline::Create`: delete the assignment.
- `CreateMultiSetPipelineLayout`: read the global layout from
  `gpTextureManager`.

## Out of scope

- `Pipeline::mExternalSet1VkDescriptorSetLayout`, also a copy (of
  `mInfo.vkExternalSet1DescriptorSetLayout`) but read in three places.
- `Pipeline::Destroy`.

## Acceptance criteria

- Clean `/compile` builds of the BrokenEngineSandbox client and of ModelViewer.

## Notes

- Change Workflow tier: Tier 1. Trigger: local behavior-preserving change that
  removes an internal data member whose only reader is updated.
- No shader, descriptor layout, or threading change: the same handle reaches
  `vkCreatePipelineLayout`.
