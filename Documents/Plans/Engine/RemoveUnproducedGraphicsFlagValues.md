<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:55:07.658Z","dependsOn":[]} -->
# Remove graphics flag and layout values that nothing produces

## Context

`Objects/` is `Engine/Source/Graphics/Objects/` and `Managers/` is
`Engine/Source/Graphics/Managers/`. Verified against commit `c675b6b2`. Each
"no producer" search covered `Engine`, `Projects`, `Tools`, `DataPacker`, and
`Common`.

Four enum values have no producer, and one sampler slot is reached only
through one of them:

- `DescriptorFlags::kSamplerClampLinear` and `kSamplerRepeatLinear`
  (`Objects/Pipeline.h:27-28`). Their only other uses are two literals of the
  `kSamplerAny` mask (`Objects/Pipeline.h:32`) and two rows of the
  flag-to-slot table in `TextureManager::GetSampler`
  (`Managers/TextureManager.cpp:436-437`). No `DescriptorInfo` sets either.
  The sibling `kSamplerMirroredRepeatLinear` is live
  (`Managers/PipelineManager.cpp:274`, `:293`, `:357`).
- `TextureManager::kSamplerSlotRepeatLinear` (`Managers/TextureManager.h:185`)
  is read only through the `kSamplerRepeatLinear` table row, so the sampler
  `CreateSamplers` builds for it (`Managers/TextureManager.cpp:413-417`) is
  never bound. `kSamplerSlotLinearClamp` stays live: `GetSampler` returns it
  for unflagged descriptors (`Managers/TextureManager.cpp:460`) and the
  lighting blur uses it (`:793`, `:795`).
- `TextureLayout::kGeneral` (`Objects/Texture.h:33`) and its row in
  `kpLayoutMappings` (`Objects/Texture.cpp:30-31`) have no user.
- `PipelineFlags::kCullFront` (`Objects/Pipeline.h:78`) is read only by the
  cull-mode ternary in `ConfigureRasterization`
  (`Objects/PipelineCreator.cpp:388`). It is the twin of the live `kCullBack`.

Origin: an architecture review and an in-function refactor review of
`Engine/Source/Graphics/Objects`, independently verified. Unmet criterion:
root `AGENTS.md` `## Directives` "no speculative ... configuration". The
`kCullFront` removal was judged too small to stand alone and is recorded here
because this Plan is already a dead-flag removal in the same header.

## Design

Recommended:

- Delete `kSamplerClampLinear` and `kSamplerRepeatLinear`, remove exactly
  their two literals from `kSamplerAny`, and delete their two rows from the
  `GetSampler` table.
- Delete `kSamplerSlotRepeatLinear` and the sampler creation that fills it.
  The create-info state those three address-mode assignments set is used by
  nothing after them.
- Delete `TextureLayout::kGeneral` together with its `kpLayoutMappings` row.
  The table is positional and the `static_assert` after it checks only the
  total count, so the value and its row go in the same edit.
- Delete `kCullFront`; the cull mode becomes back culling when `kCullBack` is
  set and none otherwise.

Leave every remaining enum value's bits unchanged.

## Critical files

- `Engine/Source/Graphics/Objects/Pipeline.h`
- `Engine/Source/Graphics/Objects/PipelineCreator.cpp`
- `Engine/Source/Graphics/Objects/Texture.h`
- `Engine/Source/Graphics/Objects/Texture.cpp`
- `Engine/Source/Graphics/Managers/TextureManager.h`
- `Engine/Source/Graphics/Managers/TextureManager.cpp`
- `Engine/Source/Graphics/Managers/AGENTS.md`

## In scope

- `DescriptorFlags::kSamplerClampLinear`, `kSamplerRepeatLinear`, and the
  `kSamplerAny` mask.
- `PipelineFlags::kCullFront` and the cull-mode expression in
  `ConfigureRasterization`.
- `TextureManager::SamplerSlot::kSamplerSlotRepeatLinear`, the `RepeatLinear`
  sampler creation at the end of `TextureManager::CreateSamplers`, and the two
  table rows in `TextureManager::GetSampler`.
- `TextureLayout::kGeneral` and its `kpLayoutMappings` row.
- `Engine/Source/Graphics/Managers/AGENTS.md` `### TextureManager`: the
  sentence listing the samplers that omit anisotropy says "explicit offscreen
  linear variants". With both flags gone `kSamplerMirroredRepeatLinear` is the
  only explicit linear variant, and `kSamplerSlotLinearClamp` is reached only
  unflagged, so the plural becomes singular.

## Out of scope

- `kSamplerMirroredRepeatLinear`, `kSamplerSlotLinearClamp`, and every other
  sampler flag, slot, and sampler.
- `DescriptorFlags::kUniformBuffer` and `kPerCommandBufferUniformBuffers`:
  whether they stay as documented routing vocabulary is an open owner
  question.
- `VK_IMAGE_LAYOUT_GENERAL` uses that do not go through
  `TextureLayout::kGeneral`.
- Renumbering any remaining flag value.

## Risk tier

Tier 2 — scoped behavior. Trigger: enumerators are removed from enums declared
in headers, which Tier 1's "no public signature exposure" excludes, and one
never-bound `VkSampler` is no longer created. No pipeline, descriptor, or
image changes. Client-only; no determinism/CRC, wire, serialization, `.pack`,
replay, threading, or shader exposure.

## Acceptance criteria

- A repository search for `kSamplerClampLinear`, `kSamplerRepeatLinear`,
  `kSamplerSlotRepeatLinear`, `kCullFront`, and the whole word `kGeneral`
  returns nothing.
- `Engine/Source/Graphics/Managers/AGENTS.md` `### TextureManager` names one
  explicit offscreen linear variant, not several, among the samplers that omit
  anisotropy.
- The client and ModelViewer build.

## Coordination

These Plans edit other regions of the same files. None depends on this Plan,
and it depends on none. Whichever lands second re-derives its regions by
symbol from the landed files:

- `Documents/Plans/Engine/RemovePipelinePushConstantBytes.md` (`Pipeline.h`,
  `PipelineCreator.cpp`)
- `Documents/Plans/Engine/SharePipelineDescriptorSetBinder.md` (`Pipeline.h`)
- `Documents/Plans/Engine/RemoveExternalSet0LayoutOverride.md` (`Pipeline.h`)
- `Documents/Plans/Engine/ModelPipelineOwnsMaterialsBuffer.md` (`Pipeline.h`)
- `Documents/Plans/Engine/ComputeLayoutReusesSetHelpers.md`
  (`PipelineCreator.cpp`)
- `Documents/Plans/Engine/RemoveTextureUpdateData.md` (`Texture.h`,
  `Texture.cpp`)

## Notes

- The private game project in the primary checkout holds only documents and no
  source, so it names none of these values. If a game source tree exists
  elsewhere, it must still build.
- The Linear sampler family is not a required parallel of the non-Linear one:
  there is no linear border variant.
- Nothing was built or run by the authoring session; the evidence is from
  reading and searching the tree.
