<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:53:46.791Z","dependsOn":[]} -->
# Give the model descriptor writes explicit binding numbers

## Context

`PDW` is `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp`, `MP`
is `Engine/Source/Graphics/Objects/ModelPipeline.cpp`, `SLB` is
`Engine/Data/Shaders/ShaderLayoutsBase.h`, and `MF` is
`Engine/Data/Shaders/Model/Model.frag`. Verified against commit `c675b6b2`.

The model pipeline's Set 1 and Set 2 bindings 5 to 12 get their numbers from
write order, not from a shared constant:

- `ModelPipeline::Create` appends seven descriptors after the `kModel` entry.
  Only the last three set `iExplicitBinding` (`MP:49`, `:54`, `:58`, using
  `kiModelBindingMeshData`, `kiModelBindingJointMatrix`,
  `kiModelBindingTerrainElevation`, `SLB:147-149`). The first four
  (`MP:32-45`) do not.
- A descriptor without an explicit binding takes `cursor.iDescriptorCount`
  (`PDW:477`), and `WriteModelDescriptor` numbers each of its writes from
  `rCursor.iDescriptorCount` (`PDW:60`, `:87`, `:104`, `:165`).
- Both model producers pass exactly three leading descriptors
  (`Engine/Source/Graphics/Managers/DynamicPipelines.cpp:114-116`,
  `:167-169`), so the `kModel` writes land on 3, 4, 5, 6, 7, 8 and the four
  appended ones on 9 to 12. Those match the literals at `MF:50-64`. Nothing
  asserts the count of leading descriptors.

`WriteModelDescriptor`'s first two writes, a standalone sampler and the
bindless texture array (`PDW:79-112`), exist only to advance the cursor by
two. They land on bindings 3 and 4, which are `kiGlobalBindingSamplerRepeat`
and `kiGlobalBindingBindlessTextures` (`SLB:125-126`), declared `set = 0` at
`MF:46-47` and by no shadow shader. For the main variant the pipeline has an
external Set 0, so `ShouldRegisterBinding` (`PDW:44-47`) is false and
`RouteWritesBySet` (`PDW:344-364`) drops both writes; for the shadow variant
`FilterWritesByShaderLayout` drops both. Their only effects are the cursor
advance and one of the image infos `kiModelDescriptorImageInfos` (`PDW:27`)
reserves.

Two smaller facts in the same code:

- `iRegisterBinding` is initialised from `iBinding` (`PDW:478`) and never
  reassigned. It is passed to `WriteStandaloneSampler` (`PDW:501`, parameter
  at `:211`, used at `:226`) and `RegisterCombinedSamplerBindings`
  (`PDW:519`), and the comment at `PDW:476` describes a distinction the code
  no longer makes.
- `RegisterCombinedSamplerBindings` ends with
  `else if (rDescriptorInfo.ppTextures != nullptr)` (`PDW:294`), while
  `WriteCombinedSamplers`, which always runs first on the same descriptor
  (`PDW:515`), ends the same three-way selection with a plain `else` that
  dereferences `ppTextures` (`PDW:257-266`). The null test can be false only
  with `iCount == 0`, which no producer sets (`DescriptorInfo::iCount`
  defaults to 1).

Origin: an architecture review and an in-function refactor review of
`Engine/Source/Graphics/Objects`, independently verified. Unmet criterion:
`Engine/Data/Shaders/AGENTS.md` `## Shared Contract` — "Binding constants live
in the shared layout header and must match C++ layout/writes."

## Design

Recommended, as one change in this order (deleting the padding first would
shift every later binding by two):

1. Add `kiModelBinding*` constants for bindings 5 to 12 beside the three in
   `SLB`, and use them in the `MF` layout qualifiers.
2. Set `iExplicitBinding` on the four appended descriptors in
   `ModelPipeline::Create`.
3. Give `WriteModelDescriptor`'s image-based-lighting and materials writes
   their constants; `PushCombinedImageSamplerWrite` takes the binding as a
   parameter instead of reading the cursor.
4. Delete the sampler and bindless-array padding writes, and lower
   `kiModelDescriptorImageInfos` and its comment to what remains.
5. Drop the `iRegisterBinding` local and the `WriteStandaloneSampler`
   parameter, pass `iBinding` at both call sites, and reword the `PDW:476`
   comment.
6. Make `PDW:294` a plain `else`, restoring the mirror with
   `WriteCombinedSamplers`.

The binding numbers themselves do not change, so compiled shader reflection
and the `.pack` contents carry the same bindings.

A validation-clean run does not prove bindings 5 to 12 are still written:
with `UPDATE_AFTER_BIND` or `PARTIALLY_BOUND` set on a binding, the Khronos
validation layer's core checks skip the draw-time check for a descriptor that
was never written (Vulkan-ValidationLayers `vulkan-sdk-1.4.363.0`,
`layers/core_checks/cc_descriptor.cpp:949-953`; only GPU-assisted validation
reports it). Both model pipelines set `kUpdateAfterBind`, so the acceptance
check inspects the bound descriptors in a GPU frame capture.

## Critical files

- `Engine/Data/Shaders/ShaderLayoutsBase.h`
- `Engine/Data/Shaders/Model/Model.frag`
- `Engine/Source/Graphics/Objects/ModelPipeline.cpp`
- `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp`

## In scope

- New `kiModelBinding*` constants for bindings 5 to 12 in `SLB`, and the
  eight layout qualifiers at `MF:50-64` that use those numbers.
- The four appended descriptors without `iExplicitBinding` in
  `ModelPipeline::Create`.
- `WriteModelDescriptor`: the two padding writes, and the binding each
  remaining write uses. `PushCombinedImageSamplerWrite`'s binding source.
  `kiModelDescriptorImageInfos` and its comment.
- `iRegisterBinding` in `PipelineDescriptorWriter::Write`, the
  `WriteStandaloneSampler` parameter, and the comment above the `iBinding`
  resolution.
- The final arm of `RegisterCombinedSamplerBindings`.

## Out of scope

- Binding 2, the caller-supplied model storage buffer, which the game
  collections also pass as a literal.
- The two leading uniform-buffer descriptors every model producer passes,
  which are Set 0 writes dropped the same way.
- The materials buffer's creation, ownership, and contents;
  `Documents/Plans/Engine/ModelPipelineOwnsMaterialsBuffer.md` owns them.
- Which set binding 8 belongs to, and the Set 2 layout, allocation, and
  routing.
- `FilterWritesByShaderLayout` and `RouteWritesBySet`.
- The `ppTextures` null test in `IsPublishedTextureDescriptor`
  (`Engine/Source/Graphics/Objects/Pipeline.cpp:45`), which runs before the
  writer and has no `pTexture` arm.
- Literal Set 1 bindings in other shader families.

## Risk tier

Tier 2 — scoped behavior (the author's classification). Trigger: the change
rewrites how one subsystem, the client renderer's model pipeline, numbers its
descriptor writes, and touches the CPU/GPU binding contract in a shared
layout header and a shader. It stays Tier 2 because the binding numbers, the
shader interface, and the packed reflection data are unchanged; a slip shows
as a misbound texture. The layout header is compiled into the server too, but
the added constants are unused there. No determinism/CRC, wire,
serialization, `kiVersion`, replay, or threading exposure.

## Acceptance criteria

- No model descriptor write takes its binding from write order after the
  `kModel` slot: the image-based-lighting, materials, lighting, shadow, smoke,
  and object-shadow bindings each name a `kiModelBinding*` constant on both
  the C++ and GLSL side.
- A repository search for `iRegisterBinding` finds it only as the parameter of
  `RegisterCombinedSamplerBindings`.
- The client, the server, and ModelViewer build.
- Through `/agent-harness`, a RenderDoc capture of a frame that draws a model
  shows a written descriptor at each of bindings 5 to 12 of the main model
  pipeline, and a model screenshot is unchanged from before the change. The
  supplied evidence records no harness setup.

## Coordination

`Documents/Plans/Engine/ModelPipelineOwnsMaterialsBuffer.md` depends on this
Plan through metadata. The Plan listed below edits other regions of the same
files. It does not depend on this Plan, and this Plan does not depend on it.
Whichever lands second re-derives its regions by symbol from the landed files:

- `Documents/Plans/Engine/RemoveExternalSet0LayoutOverride.md`
  (`ModelPipeline.cpp`)

## Notes

- Literal Set 1 bindings are common in the engine's shaders, so the positional
  scheme is not a model-only pattern, and no misbinding has been observed.
- Whether Set 2 is reserved for per-material content is an open owner
  question; this Plan is the same under either answer.
- Nothing was built or run by the authoring session. The reflected bindings
  were read from GLSL source, not from a built pack.
