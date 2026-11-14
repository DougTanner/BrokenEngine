<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:53:56.985Z","dependsOn":["Documents/Plans/Engine/ExplicitModelDescriptorBindings.md"]} -->
# ModelPipeline owns one Materials buffer per scene

## Context

`PDW` is `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp`, `MP`
is `Engine/Source/Graphics/Objects/ModelPipeline.cpp`, and `Objects/` is
`Engine/Source/Graphics/Objects/`. Verified against commit `c675b6b2`; the
line numbers are from that commit and move when the dependency lands.

A scene with M materials gets 2M identical whole-scene Materials buffers where
one is needed:

- `ModelPipeline::Create` passes one `pipelineInfo`, with its `kModel`
  descriptor (`MP:27-28`), to every per-material `Pipeline::Create`
  (`MP:142`), for the main variant and again for the shadow variant.
- Each pipeline's `PipelineDescriptorWriter::Write` reaches
  `WriteModelDescriptor`, finds its own
  `Pipeline::mModelMaterialsStorageBuffer` (`Objects/Pipeline.h:183`) null
  (`PDW:118`), and creates a device-local buffer sized for every material of
  the scene (`PDW:120-124`), filled for every material (`PDW:145-155`) through
  a staged one-shot upload (`Objects/Buffer.cpp:245-260`).
- The material is selected by push constant at draw time (`MP:174`), so every
  buffer holds the same content.
- Only `Engine/Data/Shaders/Model/Model.frag:55` declares the materials
  binding, so the shadow variant's write is dropped and its M buffers are
  never bound.

The memory is small. The costs are 2M blocking one-shot submissions per scene
at boot and at every pipeline-tier recreation, and misplaced ownership: a
model-only member and a create-if-null sit in the generic `Pipeline` and its
writer; the material texture-index rejection (`PDW:133-144`, throwing
`std::ios_base::failure`) lives in the writer's upload lambda while the other
scene-chunk checks are in `ModelPipeline::Create` (`MP:72-108`); and
`Objects/Pipeline.cpp:212-214` documents that in-place recreation keeps stale
materials.

Origin: an architecture review of `Engine/Source/Graphics/Objects`,
independently verified. Unmet criterion: one Materials buffer per scene, owned
by the object that validates the scene chunk.

## Design

Recommended:

- `ModelPipeline` gains one `Buffer` member, created in `Create` for the main
  variant before the per-material loop. The shadow variant creates none.
- The fill loop and the `ResolveTextureIndex` rejection move into
  `ModelPipeline::Create`, beside the other scene-chunk checks. The rejection
  still throws `std::ios_base::failure` for a present texture selector whose
  index is out of range, and an absent selector still leaves its index unused,
  as `Objects/AGENTS.md` `## Resource Contracts` states.
- The rejection stays inside the fill, so after the change it runs for the
  main variant only; today every per-material pipeline of both variants runs
  it (`PDW:118-144`). The same inputs are still rejected only because
  `DynamicPipelines::CreateModelPipelines`, the one caller of both variant
  creators, creates the main variant before the shadow variant
  (`Engine/Source/Graphics/Managers/DynamicPipelines.cpp:64-65`) and the main
  creator's catch rethrows (`:128`), so a scene the main variant rejects never
  reaches shadow creation. If that order does not hold in the landed tree, the
  executing session runs the rejection for both variants, outside the buffer
  creation.
- The `kModel` `DescriptorInfo` carries the buffer through `pBuffers`, and
  `WriteModelDescriptor` binds that instead of a pipeline member. For the
  shadow variant the write is dropped as today, so the recommended form skips
  the materials write when no buffer is supplied.
- Delete `Pipeline::mModelMaterialsStorageBuffer`, the create-if-null block in
  `WriteModelDescriptor`, and the comment at the end of `Pipeline::Destroy`.

The binding number, the shader source, and the buffer contents do not change.

A validation-clean run does not prove the materials binding is still written,
because both model pipelines set `kUpdateAfterBind` and the validation layer's
core checks skip the never-written-descriptor check for such bindings. The
acceptance check reads a GPU frame capture.

## Critical files

- `Engine/Source/Graphics/Objects/ModelPipeline.h`
- `Engine/Source/Graphics/Objects/ModelPipeline.cpp`
- `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp`
- `Engine/Source/Graphics/Objects/Pipeline.h`
- `Engine/Source/Graphics/Objects/Pipeline.cpp`

## In scope

- A Materials `Buffer` member on `ModelPipeline`, and in
  `ModelPipeline::Create` its creation, its fill, and the material
  texture-index rejection.
- `WriteModelDescriptor`: the materials buffer creation block and the source
  of the buffer its storage-buffer write binds.
- `Pipeline::mModelMaterialsStorageBuffer` and the comment about it at the end
  of `Pipeline::Destroy`.
- `Engine/Source/Graphics/Objects/AGENTS.md`, only if a sentence becomes
  incorrect.

## Out of scope

- Which descriptor set the materials binding belongs to, the Set 2 layout,
  allocation, routing, and `kMultiSet`. Whether Set 2 is reserved for
  per-material content is an open owner question, and this Plan is the same
  under either answer.
- The binding numbers and `Model.frag`.
- `PbrMaterialLayout`, `MaterialShaderData`, and the scene chunk format.
- The image-based-lighting writes in `WriteModelDescriptor`.
- Adding a separate `kStorageBuffer` descriptor after the `kModel` entry.

## Risk tier

Tier 2 — scoped behavior (the author's classification). Trigger: one
subsystem's runtime behavior changes — the number and owner of a GPU buffer —
and a pack trust-boundary check moves between two functions of that
subsystem. It stays Tier 2 because the check rejects the same inputs with the
same exception type at the same boot stage, which rests on the creation order
`## Design` states (main variant before shadow), and the format is unchanged.
No determinism/CRC, wire, serialization, `.pack`, `kiVersion`, replay,
threading, or shader exposure.

## Acceptance criteria

- A repository search for `mModelMaterialsStorageBuffer` returns nothing.
- The client and ModelViewer build.
- Through `/agent-harness`, a RenderDoc capture of a frame that draws a model
  shows one buffer named "Materials" per model scene and a written descriptor
  at the materials binding of each main model pipeline, and a model screenshot
  is unchanged from before the change. The supplied evidence records no
  harness setup.

## Coordination

`Documents/Plans/Engine/ExplicitModelDescriptorBindings.md` is the metadata
dependency: it gives the materials write its binding constant in the function
this Plan edits. These Plans edit other regions of the same files. None
depends on this Plan, and it depends on none of them. Whichever lands second
re-derives its regions by symbol from the landed files:

- `Documents/Plans/Engine/RemovePipelinePushConstantBytes.md` (`Pipeline.h`,
  `Pipeline.cpp`)
- `Documents/Plans/Engine/SharePipelineDescriptorSetBinder.md` (`Pipeline.h`,
  `Pipeline.cpp`)
- `Documents/Plans/Engine/RemoveExternalSet0LayoutOverride.md` (`Pipeline.h`,
  `Pipeline.cpp`, `ModelPipeline.cpp`)
- `Documents/Plans/Engine/RemoveUnproducedGraphicsFlagValues.md`
  (`Pipeline.h`)

## Notes

- The exception text the rejection throws names `PipelineDescriptorWriter`
  today; `DynamicPipelines` logs it. Choosing the text after the move is a
  trivial choice for the executing session.
- Material counts were read from source glTF files, not from exported scene
  headers, and nothing was built or run by the authoring session.
