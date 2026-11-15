<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:20:12.608Z","dependsOn":[]} -->
# Descriptor bindings and shared shader layout constants

## Context

`PDW` is `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp`, `MP`
is `Engine/Source/Graphics/Objects/ModelPipeline.cpp`, `SLB` is
`Engine/Data/Shaders/ShaderLayoutsBase.h`, `MF` is
`Engine/Data/Shaders/Model/Model.frag`, `M/` is
`Engine/Source/Graphics/Managers/`, `O/` is
`Engine/Source/Graphics/Objects/`, and `S/` is `Engine/Data/Shaders/`. The
model-pipeline and materials-buffer facts were verified against commit
`c675b6b2`; the inert-entry and selector facts are at commit `401e4b75`,
except that the C++ selector literal lines reflect the authoring session's
later edits to the two recording files. Line numbers move as earlier steps
land; re-find them by symbol.

### Inert sampler and bindless-array entries

Four pipeline descriptions carry a standalone sampler entry followed by a
`DescriptorFlags::kTextures` entry, eight entries in all:
`M/DynamicPipelines.cpp:204-205` (`CreateAreaLightingPipeline`, used for
area and point lighting), `:228-229` (`CreatePipelineVisibleLights`),
`:253-254` (`CreatePipelineBillboards`), and `M/PipelineManager.cpp:412-413`
(particle render). None of the eight produces a descriptor write that
reaches a set. How the writer treats them:

- Binding numbers are one number space across Set 0, 1, and 2 inside a
  shader: the exporter builds one table per shader indexed by binding
  number and asserts each number is used once
  (`DataPacker/Source/ExportJobs/ExportShader.cpp:46-53,70-80,313-319,329`).
- `PipelineDescriptorWriter::Write` gives each entry the number
  `iExplicitBinding` or else the running write count `cursor.iDescriptorCount`
  (`PDW:477`). `FilterWritesByShaderLayout` (`PDW:321-342`) then drops a
  write whose number no shader declares, and `RouteWritesBySet`
  (`PDW:344-364`) drops a write whose number reflects as Set 0. Neither drop
  logs or asserts.
- Area lighting (`S/Quads/QuadsVisibleArea.vert`, `AreaLight.frag` declare
  0, 1, 3, 4): the sampler gets number 2, undeclared, filtered; `kTextures`
  gets 3, a Set 0 number, routed away.
- Point lighting (declares 0, 1, 4, 12): numbers 2 and 3 are undeclared,
  both filtered.
- Visible lights (`VisibleLight.vert`, `.frag` declare 0 to 5): numbers 3
  and 4 are Set 0, routed away.
- Billboards (declares 0, 1, 2, 4, 12): 3 filtered, 4 routed away.
- Particle render (declares 0, 1, 2, 3, 4, 12): 4 routed away, 5 filtered.

Two consequences:

- `CreateAreaLightingPipeline`'s `eSamplerFlag` parameter
  (`M/DynamicPipelines.h:64`) reaches only `WriteStandaloneSampler`
  (`PDW:215`), whose write is always dropped. So `kSamplerRepeat` against
  `kSamplerClamp` at the two callers
  (`Engine/Source/Frame/Collections/AreaLights/AreaLightsRender.cpp:23`,
  `.../PointLights/PointLightsRender.cpp:23`) selects nothing; the shader's
  own Set 0 constant picks the sampler.
- In `CreatePipelineVisibleLights` a live entry follows the pair: the
  terrain elevation combined sampler (`M/DynamicPipelines.cpp:230`) lands on
  binding 5 only because the two dead entries take positions 3 and 4.
  Without them it would get number 3, be routed away as Set 0, and leave
  `S/Lighting/VisibleLight.frag:19` unwritten. In the other three lists the
  pair is last, so nothing shifts.

After the eight entries go, `DescriptorFlags::kTextures` (`O/Pipeline.h:17`)
and its writer branch (`PDW:503-512`), and the `WriteStandaloneSampler`
function and branch (`PDW:211-231`, `:499-502`), have no producer: a
repository search for standalone sampler and `kTextures` entries finds only
these four sites.

### Positional model descriptor bindings

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
  (`M/DynamicPipelines.cpp:114-116`, `:167-169`), so the `kModel` writes land
  on 3, 4, 5, 6, 7, 8 and the four appended ones on 9 to 12. Those match the
  literals at `MF:50-64`. Nothing asserts the count of leading descriptors.

`WriteModelDescriptor`'s first two writes, a standalone sampler and the
bindless texture array (`PDW:79-112`), exist only to advance the cursor by
two. They land on bindings 3 and 4, which are `kiGlobalBindingSamplerRepeat`
and `kiGlobalBindingBindlessTextures` (`SLB:125-126`), declared `set = 0` at
`MF:46-47` and by no shadow shader. For the main variant the pipeline has an
external Set 0, so `ShouldRegisterBinding` (`PDW:44-47`) is false and
`RouteWritesBySet` drops both writes; for the shadow variant
`FilterWritesByShaderLayout` drops both. Their only effects are the cursor
advance and one of the image infos `kiModelDescriptorImageInfos` (`PDW:27`)
reserves.

`iRegisterBinding` is initialised from `iBinding` (`PDW:478`) and never
reassigned. It is passed to `WriteStandaloneSampler` (`PDW:501`), which
section 1 deletes, and to `RegisterCombinedSamplerBindings` (`PDW:519`), and
the comment at `PDW:476` describes a distinction the code no longer makes.

`RegisterCombinedSamplerBindings` ends with
`else if (rDescriptorInfo.ppTextures != nullptr)` (`PDW:294`), while
`WriteCombinedSamplers`, which always runs first on the same descriptor
(`PDW:515`), ends the same three-way selection with a plain `else` that
dereferences `ppTextures` (`PDW:257-266`). The null test can be false only
with `iCount == 0`, which no producer sets (`DescriptorInfo::iCount`
defaults to 1).

### Per-pipeline Materials buffers

A scene with M materials gets 2M identical whole-scene Materials buffers where
one is needed:

- `ModelPipeline::Create` passes one `pipelineInfo`, with its `kModel`
  descriptor (`MP:27-28`), to every per-material `Pipeline::Create`
  (`MP:142`), for the main variant and again for the shadow variant.
- Each pipeline's `PipelineDescriptorWriter::Write` reaches
  `WriteModelDescriptor`, finds its own
  `Pipeline::mModelMaterialsStorageBuffer` (`O/Pipeline.h:183`) null
  (`PDW:118`), and creates a device-local buffer sized for every material of
  the scene (`PDW:120-124`), filled for every material (`PDW:145-155`) through
  a staged one-shot upload (`O/Buffer.cpp:245-260`).
- The material is selected by push constant at draw time (`MP:174`), so every
  buffer holds the same content.
- Only `MF:55` declares the materials binding, so the shadow variant's write
  is dropped and its M buffers are never bound.

The memory is small. The costs are 2M blocking one-shot submissions per scene
at boot and at every pipeline-tier recreation, and misplaced ownership: a
model-only member and a create-if-null sit in the generic `Pipeline` and its
writer; the material texture-index rejection (`PDW:133-144`, throwing
`std::ios_base::failure`) lives in the writer's upload lambda while the other
scene-chunk checks are in `ModelPipeline::Create` (`MP:72-108`); and
`O/Pipeline.cpp:212-214` documents that in-place recreation keeps stale
materials.

### Bare push-constant selectors

Record-once command buffers select a projection area or a rendering mode
through a push constant, and both sides write the selector as a bare number:

- C++ literals: `M/CommandBufferRecordGlobal.cpp:69`;
  `M/CommandBufferRecordMain.cpp:81`, `:85`, `:89`, `:232`, `:260`, `:264`,
  `:272`, `:276`, `:289`, `:444`, `:517`.
- Shader decodes: `S/Quads/QuadsVisibleArea.vert:47-57`,
  `S/Quads/QuadsAxisAlignedVisibleArea.vert:44-54`,
  `S/Objects/HexShield.vert:85`, `S/Model/ModelCommon.h:107-126`.

Two separate protocols are involved: the area selector in `.x` (quads, hex
shield) and the rendering mode in `.y` (models). The values agree today. A
mismatch falls silently into an `else` branch. `S/AGENTS.md`
`## Shared Contract` states "Push constants select projection/render modes
where command buffers are recorded once", and `SLB:177-185` is the precedent
for named selectors shared by both languages.

Separately, `SLB:98` defines `ELEVATION_FORMAT`, which nothing uses.

### Origin and unmet criteria

Origin: an architecture review and an in-function refactor review of
`Engine/Source/Graphics/Objects`, and the architecture phase (data shape,
client/server lenses) of an analysis of `Engine/Source/Graphics/Managers`,
each independently verified. Unmet criteria:

- `Engine/Data/Shaders/AGENTS.md` `## Shared Contract` — "Binding constants
  live in the shared layout header and must match C++ layout/writes."
- Every `descriptorInfos` entry of the four lighting, billboard, and
  particle descriptions produces a write the pipeline's own Set 1 receives,
  and no parameter selects a value nothing consumes.
- One Materials buffer per scene, owned by the object that validates the
  scene chunk.
- No bare selector literal on either side; no unused define in the shared
  layout header.

## Design

Recommended, in this order.

### 1. Remove the inert lighting descriptor entries

- Delete the eight entries.
- Remove the `eSamplerFlag` parameter of `CreateAreaLightingPipeline` and
  the argument at its two callers.
- Give the visible-lights elevation entry an explicit binding: add one Set 1
  binding constant to `SLB`, beside the existing Set 1 constants, with the
  number the shader uses today, and use it both as `iExplicitBinding` at
  `M/DynamicPipelines.cpp:230` and in `VisibleLight.frag:19`. Rationale:
  `iExplicitBinding` with a shared constant is the existing mechanism
  (`MP:49-58`, `M/WorldLightingShadowPipelines.cpp:381,408-412`).
  Renumbering elevation to the next free position is not available, because
  3 and 4 are Set 0 numbers in the same number space.
- Delete what is left without a producer: `DescriptorFlags::kTextures`, its
  writer branch, and `WriteStandaloneSampler` with its branch and the
  `bSampler` local. Rationale: the repository keeps no unreachable branch.
  The executing session repeats the producer search first; if it finds a
  producer, that item stays and is reported.

The descriptor sets the four pipelines end up with are the same as today:
only dropped writes are removed.

### 2. Give the model descriptor writes explicit binding numbers

These steps run in this order; deleting the padding first would shift every
later binding by two.

1. Add `kiModelBinding*` constants for bindings 5 to 12 beside the three in
   `SLB`, and use them in the `MF` layout qualifiers.
2. Set `iExplicitBinding` on the four appended descriptors in
   `ModelPipeline::Create`.
3. Give `WriteModelDescriptor`'s image-based-lighting and materials writes
   their constants; `PushCombinedImageSamplerWrite` takes the binding as a
   parameter instead of reading the cursor.
4. Delete the sampler and bindless-array padding writes, and lower
   `kiModelDescriptorImageInfos` and its comment to what remains.
5. Drop the `iRegisterBinding` local, pass `iBinding` to
   `RegisterCombinedSamplerBindings`, and reword the `PDW:476` comment.
6. Make `PDW:294` a plain `else`, restoring the mirror with
   `WriteCombinedSamplers`.

Step 5 has one call site, not two: section 1 already deleted
`WriteStandaloneSampler` with its parameter.

The binding numbers themselves do not change, so compiled shader reflection
and the `.pack` contents carry the same bindings.

### 3. ModelPipeline owns one Materials buffer per scene

- `ModelPipeline` gains one `Buffer` member, created in `Create` for the main
  variant before the per-material loop. The shadow variant creates none.
- The fill loop and the `ResolveTextureIndex` rejection move into
  `ModelPipeline::Create`, beside the other scene-chunk checks. The rejection
  still throws `std::ios_base::failure` for a present texture selector whose
  index is out of range, and an absent selector still leaves its index unused,
  as `Engine/Source/Graphics/Objects/AGENTS.md` `## Resource Contracts`
  states.
- The rejection stays inside the fill, so after the change it runs for the
  main variant only; today every per-material pipeline of both variants runs
  it (`PDW:118-144`). The same inputs are still rejected only because
  `DynamicPipelines::CreateModelPipelines`, the one caller of both variant
  creators, creates the main variant before the shadow variant
  (`M/DynamicPipelines.cpp:64-65`) and the main creator's catch rethrows
  (`:128`), so a scene the main variant rejects never reaches shadow
  creation. If that order does not hold in the landed tree, the executing
  session runs the rejection for both variants, outside the buffer creation.
- The `kModel` `DescriptorInfo` carries the buffer through `pBuffers`, and
  `WriteModelDescriptor` binds that instead of a pipeline member, at the
  materials binding constant from section 2. For the shadow variant the write
  is dropped as today, so the recommended form skips the materials write when
  no buffer is supplied.
- Delete `Pipeline::mModelMaterialsStorageBuffer`, the create-if-null block in
  `WriteModelDescriptor`, and the comment at the end of `Pipeline::Destroy`.

The binding number, the shader source, and the buffer contents do not change
in this section.

### 4. Name the push-constant area and rendering-mode selectors

- Add two constant groups to `SLB`, one per protocol, with the values in use
  today, and use them at the C++ sites and the four shader files. The payload
  is a `vec4`, so each C++ site converts the constant to `float`.
- The lighting area has no decoded value in the quad shaders: it is the
  "anything else" branch. Name it with a constant and leave that fallthrough
  as it is.
- Delete the `ELEVATION_FORMAT` line.

No selector value, push-constant layout, or shader branch changes.

### Validation and capture evidence

A validation-clean run does not prove bindings 5 to 12, including the
materials binding, are still written: with `UPDATE_AFTER_BIND` or
`PARTIALLY_BOUND` set on a binding, the Khronos validation layer's core
checks skip the draw-time check for a descriptor that was never written
(Vulkan-ValidationLayers `vulkan-sdk-1.4.363.0`,
`layers/core_checks/cc_descriptor.cpp:949-953`; only GPU-assisted validation
reports it). Both model pipelines set `kUpdateAfterBind`, so the acceptance
check inspects the bound descriptors in a GPU frame capture.

## Critical files

- `Engine/Data/Shaders/ShaderLayoutsBase.h`
- `Engine/Data/Shaders/Model/Model.frag`
- `Engine/Data/Shaders/Model/ModelCommon.h`
- `Engine/Data/Shaders/Lighting/VisibleLight.frag`
- `Engine/Data/Shaders/Quads/QuadsVisibleArea.vert`
- `Engine/Data/Shaders/Quads/QuadsAxisAlignedVisibleArea.vert`
- `Engine/Data/Shaders/Objects/HexShield.vert`
- `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp`
- `Engine/Source/Graphics/Objects/ModelPipeline.h`
- `Engine/Source/Graphics/Objects/ModelPipeline.cpp`
- `Engine/Source/Graphics/Objects/Pipeline.h`
- `Engine/Source/Graphics/Objects/Pipeline.cpp`
- `Engine/Source/Graphics/Managers/DynamicPipelines.cpp`
- `Engine/Source/Graphics/Managers/DynamicPipelines.h`
- `Engine/Source/Graphics/Managers/PipelineManager.cpp`
- `Engine/Source/Graphics/Managers/CommandBufferRecordGlobal.cpp`
- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp`
- `Engine/Source/Frame/Collections/AreaLights/AreaLightsRender.cpp`
- `Engine/Source/Frame/Collections/PointLights/PointLightsRender.cpp`

## In scope

Inert lighting descriptor entries:

- The eight entries and the `eSamplerFlag` parameter of
  `CreateAreaLightingPipeline` with its two caller arguments.
- One new Set 1 binding constant in `SLB` and its two uses (the
  visible-lights elevation entry and `VisibleLight.frag:19`).
- `DescriptorFlags::kTextures`, its writer branch, and
  `WriteStandaloneSampler` with its branch and the `bSampler` local.

Model descriptor bindings:

- New `kiModelBinding*` constants for bindings 5 to 12 in `SLB`, and the
  eight layout qualifiers at `MF:50-64` that use those numbers.
- The four appended descriptors without `iExplicitBinding` in
  `ModelPipeline::Create`.
- `WriteModelDescriptor`: the two padding writes, and the binding each
  remaining write uses. `PushCombinedImageSamplerWrite`'s binding source.
  `kiModelDescriptorImageInfos` and its comment.
- `iRegisterBinding` in `PipelineDescriptorWriter::Write`, its argument to
  `RegisterCombinedSamplerBindings`, and the comment above the `iBinding`
  resolution.
- The final arm of `RegisterCombinedSamplerBindings`.

Materials buffer:

- A Materials `Buffer` member on `ModelPipeline`, and in
  `ModelPipeline::Create` its creation, its fill, and the material
  texture-index rejection.
- `WriteModelDescriptor`: the materials buffer creation block and the source
  of the buffer its storage-buffer write binds.
- `Pipeline::mModelMaterialsStorageBuffer` and the comment about it at the end
  of `Pipeline::Destroy`.

Push-constant selectors:

- Two selector constant groups in `SLB` and the `ELEVATION_FORMAT` line.
- The twelve C++ literals and the four shader decodes listed in
  `## Context`.

Documentation:

- Documentation sync for the removed flag and parameter, and
  `Engine/Source/Graphics/Objects/AGENTS.md` only if a sentence becomes
  incorrect.

## Out of scope

- `RegisterStandaloneSamplerBinding` and the standalone-sampler registry.
- Binding 2, the caller-supplied model storage buffer, which the game
  collections also pass as a literal.
- The two leading uniform-buffer descriptors every model producer passes,
  which are Set 0 writes dropped the same way.
- Which descriptor set binding 8, the materials binding, belongs to, the
  Set 2 layout, allocation, routing, and `kMultiSet`. Whether Set 2 is
  reserved for per-material content is an open owner question, and this Plan
  is the same under either answer.
- `FilterWritesByShaderLayout` and `RouteWritesBySet`.
- The `ppTextures` null test in `IsPublishedTextureDescriptor`
  (`O/Pipeline.cpp:45`), which runs before the writer and has no `pTexture`
  arm.
- Moving positional Set 1 numbers other than the model pipeline's bindings 5
  to 12 and the visible-lights elevation entry to shared constants, including
  literal Set 1 bindings in other shader families.
- `CreateModelPipeline` and `ModelPipelineSpec`.
- `PbrMaterialLayout`, `MaterialShaderData`, and the scene chunk format.
- Adding a separate `kStorageBuffer` descriptor after the `kModel` entry.
- Descriptor binding number values, selector values, the push-constant block
  layout, and the decode order.
- One macro per storage-image format for the GLSL format qualifiers.

## Risk tier

Tier 2 — scoped behavior. Triggers: the client renderer's model pipeline
changes how it numbers its descriptor writes, and the CPU/GPU binding
contract in the shared layout header and shaders is re-expressed; a public
member-function signature and a public flag change and the writer loses two
branches; one subsystem's runtime behavior changes — the number and owner of
a GPU buffer — and a pack trust-boundary check moves between two functions of
that subsystem; and the push-constant selector contract shared by C++ and
GLSL is re-expressed, so shader data is rebuilt. It stays Tier 2 because the
binding numbers, set layouts, surviving writes, selector values, push-constant
layout, shader interface, and packed reflection data are unchanged (a slip
shows as a misbound texture or a wrong branch), and the material rejection
rejects the same inputs with the same exception type at the same boot stage,
which rests on the creation order `## Design` section 3 states. The layout
header is compiled into the server too, but the added constants are unused
there. No determinism/CRC, wire, serialization, `.pack` format, `kiVersion`,
replay, or threading exposure.

## Acceptance criteria

1. None of the eight inert entries, the `eSamplerFlag` parameter, or (unless
   a producer was found and reported) `kTextures` and
   `WriteStandaloneSampler` remains.
2. The elevation entry of `CreatePipelineVisibleLights` and
   `VisibleLight.frag` use one shared constant.
3. No model descriptor write takes its binding from write order after the
   `kModel` slot: the image-based-lighting, materials, lighting, shadow,
   smoke, and object-shadow bindings each name a `kiModelBinding*` constant
   on both the C++ and GLSL side.
4. A repository search for `iRegisterBinding` finds it only as the parameter
   of `RegisterCombinedSamplerBindings`.
5. A repository search for `mModelMaterialsStorageBuffer` returns nothing.
6. No bare selector literal remains at the listed selector sites, and
   `ELEVATION_FORMAT` is gone.
7. `/compile` builds the BrokenEngineSandbox client, the server, and
   ModelViewer with regenerated shader data.
8. Through `/agent-harness`, one client run with validation layers on:
   - shows area lights, point lights, visible lights (occluded by terrain as
     before), billboards, and particles with no validation error;
   - a RenderDoc capture of a frame that draws a model shows a written
     descriptor at each of bindings 5 to 12 of the main model pipeline, one
     buffer named "Materials" per model scene, and a written descriptor at
     the materials binding of each main model pipeline;
   - screenshots of a world frame with terrain, water, those lights,
     billboards, particles, a hex shield, and models (opaque and
     transparent, with shadows), and a model screenshot, match the same
     scenes before the change.

   The supplied evidence records no harness setup.

## Coordination

These Plans edit other regions of the same files. None depends on this Plan,
and this Plan depends on none of them. Whichever lands second re-finds its
lines by symbol from the landed files:

- `Documents/Plans/Engine/PipelineObjectsDeadCode.md` (`Pipeline.h`,
  `Pipeline.cpp`, `ModelPipeline::Create` in `ModelPipeline.cpp`)
- `Documents/Plans/Engine/PublishAreaToUvInverseExtents.md`
  (`S/Objects/HexShield.vert`, `S/Model/ModelCommon.h`)

## Notes

- Literal Set 1 bindings are common in the engine's shaders, so the positional
  scheme is not a model-only pattern, and no misbinding has been observed.
- The exception text the material rejection throws names
  `PipelineDescriptorWriter` today; `DynamicPipelines` logs it. Choosing the
  text after the move is a trivial choice for the executing session.
- Not traced further: no shader under `Engine/Data/Shaders` declares a
  standalone sampler outside Set 0, so the standalone-sampler registry
  (`M/TextureDescriptors.cpp:553-557,631`) may have no reachable registrant.
  This Plan does not act on it.
- The executing session confirms no private-game caller passes
  `eSamplerFlag` where that source is available, and reports it as
  unconfirmed otherwise.
- A plan reviewer asked whether a wrong GLSL storage-image format qualifier
  is reported by validation; that was not settled and is not part of this
  Plan.
- Nothing was built or run by the authoring sessions. The reflected bindings
  were read from GLSL source, not from a built pack, and material counts were
  read from source glTF files, not from exported scene headers.
