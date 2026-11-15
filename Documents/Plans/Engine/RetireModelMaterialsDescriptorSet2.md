<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-15T16:24:49.571Z","dependsOn":[]} -->
# Move the model materials binding into Set 1 and retire Set 2

## Context

`O/` is `Engine/Source/Graphics/Objects/`, `PC` is `O/PipelineCreator.cpp`,
`PDW` is `O/PipelineDescriptorWriter.cpp`, `MP` is `O/ModelPipeline.cpp`, and
`MF` is `Engine/Data/Shaders/Model/Model.frag`. Line numbers were read at
commit `6004e5db`, before the prerequisite change; re-find them by symbol.

### Expected starting state

This Plan starts from the state its prerequisite change left: the model-only
write path (`WriteModelDescriptor`, `DescriptorFlags::kModel`) is gone,
`ModelPipeline::Create` appends ordinary descriptor entries that name their
binding through shared `kiModelBinding*` constants, and the main variant's
materials entry binds one ModelPipeline-owned Materials buffer uploaded once
per scene. The Set 1/Set 2 layout is unchanged by that change. The executing
session confirms this state in the landed tree before editing; if the tree
differs, it reports the difference instead of adapting this Plan.

### Set 2 carries one binding

Descriptor set 2 exists only for the main model pipelines, and only binding 8
lives in it:

- `MF:54-58` declares the Materials storage buffer at `set = 2, binding = 8`.
  No other shader under `Engine/Data/Shaders` declares `set = 2`.
- `PipelineFlags::kMultiSet` (`O/Pipeline.h:90`) is set only by the main
  model producer (`Engine/Source/Graphics/Managers/DynamicPipelines.cpp:109`);
  the shadow producer (`:160`) does not set it. `PC:640` asserts compute
  pipelines never set it.
- For a `kMultiSet` pipeline, `SplitBindingsBySet` (`PC:155-173`) routes
  set-2 bindings into a second array, `CreateMultiSetPipelineLayout`
  (`PC:190-211`) creates `Pipeline::mSet2VkDescriptorSetLayout` and a
  three-set pipeline layout, `PipelineDescriptorWriter::Write`
  (`PDW:382-385`, `:422-437`) allocates `Pipeline::mVkDescriptorSetsSet2` per
  framebuffer, and `RouteWritesBySet` (`PDW:334-353`) points set-2 writes at
  it.
- `ModelPipeline::RecordDrawIndirect` (`MP:149-184`) binds Set 0 and pipeline
  0's Set 1 once, then calls `Pipeline::RecordDrawIndirectSet2`
  (`O/Pipeline.cpp:270-290`) per material, which binds the pipeline and that
  material pipeline's Set 2.
- `Pipeline::Destroy` frees the Set 2 sets and layout
  (`O/Pipeline.cpp:174-178`, `:193-197`).

Every material pipeline of a scene therefore owns its own Set 2 layout and one
Set 2 per framebuffer, each holding the same descriptor: after the
prerequisite, all point at the one scene Materials buffer, and the material is
selected by push constant (`MP:174`). Set 2 carries nothing per material.

### Set 1 is already shared per scene

In `ModelPipeline::Create` (`MP:133-142`), pipeline 0 of a `kMultiSet` scene
creates its own Set 1 layout; pipelines 1..N receive it as
`vkExternalSet1DescriptorSetLayout`, so their Set 1 writes are dropped
(`PDW:347-350`) and they allocate no Set 1 (`PDW:378`, `:407`). Each scene is its
own `ModelPipeline`, so each scene's pipeline 0 owns that scene's Set 1
descriptor sets.

### Origin and unmet criterion

Recorded during the claim of the prerequisite Plan, where the user chose to
leave the set layout unchanged there and to retire Set 2 in this follow-up.
Unmet criterion: one descriptor-set mechanism per piece of shared model state;
today a single per-scene buffer travels through a per-material set, a second
layout, a second allocation path, and a second routing branch.
`Engine/Data/Shaders/AGENTS.md` `## Shared Contract` describes Set 2 as
"per-material", which no Set 2 content is.

## Design

Recommended:

1. In `MF`, declare the Materials buffer at `set = 1` with its existing
   binding constant, and fix the comment above it. The binding number does
   not change.
2. Remove Set 2 from layout creation: `SplitBindingsBySet` fills only Set 1;
   `CreateMultiSetPipelineLayout` creates no Set 2 layout and always builds the
   two-set layout. Update every caller of the two helpers, including the
   compute path if
   `Documents/Plans/Engine/PipelineObjectsDeadCode.md` has landed and calls
   them there.
3. Remove Set 2 from the writer: `PipelineDescriptorWriter::Write` allocates
   no Set 2 and `RouteWritesBySet` loses its `vkDestinationSet2` parameter and
   its set-2 branch.
4. Delete `Pipeline::mSet2VkDescriptorSetLayout`,
   `Pipeline::mVkDescriptorSetsSet2`, and their frees in `Pipeline::Destroy`.
5. Replace the Set 2 bind: the per-material draw binds the pipeline, pushes
   constants, and draws, with no descriptor bind, because
   `ModelPipeline::RecordDrawIndirect` already bound Sets 0 and 1. The
   author's recommendation is to keep `RecordDrawIndirectSet2` as that
   function minus its Set 2 bind and give it a name that no longer says Set 2;
   the exact name is a trivial choice.
6. Keep `PipelineFlags::kMultiSet`: it still selects the shared Set 1 and the
   bind-once draw. Reword the comments that say it means Set 2
   (`O/Pipeline.h:168`, `PC:152-153`, `PC:175-176`, `PC:191`, `PDW:422`,
   `PDW:523`). Renaming the flag is not part of this Plan.

Expected effect: binding 8 is written once per framebuffer into pipeline 0's
Set 1 per scene, and pipelines 1..N, which drop Set 1 writes, inherit it
through the shared Set 1. The prerequisite's ModelPipeline-owned buffer and
its upload are unchanged.

### Checks the executing session settles first

These were not verified when this Plan was written; each is settled from the
landed code (and, where noted, the capture) before or while editing, and a
failing check is reported instead of worked around:

- Shared Set 1 can carry a different Materials buffer per scene. The Set 1
  layout is created by the scene's pipeline 0, its sets are allocated per
  framebuffer and use update-after-bind; confirm that no Set 1 layout or set
  is shared across `ModelPipeline` instances, so each scene's sets can point at
  that scene's buffer.
- Pipelines 0..N stay layout-compatible for Sets 0 and 1 after binding 8
  moves: all share pipeline 0's Set 1 layout handle and the same push-constant
  range, so binding Sets 0 and 1 once with pipeline 0's layout remains valid
  for every material pipeline.
- The shadow variant still drops binding 8: its fragment shader
  (`ModelShadow.frag`) and the model vertex shaders (through `ModelCommon.h`)
  declare no binding 8, so
  `FilterWritesByShaderLayout` drops any binding-8 write and its layout has no
  binding 8; if the prerequisite already appends the materials entry only for
  the main variant, there is no such write at all.

### Validation and capture evidence

A validation-clean run does not prove binding 8 is written: with
`UPDATE_AFTER_BIND` or `PARTIALLY_BOUND` on a binding, the Khronos validation
layer's core checks skip the draw-time check for an unwritten descriptor, and
both model pipelines set `kUpdateAfterBind`. The acceptance check therefore
inspects bound descriptors in a GPU frame capture.

## Critical files

- `Engine/Data/Shaders/Model/Model.frag`
- `Engine/Source/Graphics/Objects/PipelineCreator.cpp`
- `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp`
- `Engine/Source/Graphics/Objects/Pipeline.h`
- `Engine/Source/Graphics/Objects/Pipeline.cpp`
- `Engine/Source/Graphics/Objects/ModelPipeline.cpp`
- `Engine/Data/Shaders/AGENTS.md`

## In scope

- `MF`: the Materials buffer's `set` qualifier and its comment.
- `PipelineCreator.cpp`: the Set 2 output of `SplitBindingsBySet`, the Set 2
  layout creation and the three-set branch of `CreateMultiSetPipelineLayout`,
  the Set 2 locals at their callers, and the comments describing Set 2.
- `PipelineDescriptorWriter.cpp`: the Set 2 resize and allocation in
  `PipelineDescriptorWriter::Write`, the `vkDestinationSet2` parameter and
  set-2 branch of `RouteWritesBySet`, and the comments describing Set 2.
- `Pipeline.h` / `Pipeline.cpp`: `mSet2VkDescriptorSetLayout`,
  `mVkDescriptorSetsSet2`, their frees in `Pipeline::Destroy`,
  `RecordDrawIndirectSet2` (its Set 2 bind and its name), and the comments
  that describe `kMultiSet` as Set 2.
- `ModelPipeline.cpp`: the call to the renamed draw function in
  `ModelPipeline::RecordDrawIndirect`.
- `Engine/Data/Shaders/AGENTS.md` `## Shared Contract`: the descriptor
  ownership sentence that names Set 2, and any other documentation sentence
  that becomes incorrect.

## Out of scope

- The `kMultiSet` flag value, name, and producers, and the shared Set 1
  hand-off in `ModelPipeline::Create`.
- The Materials buffer's ownership, creation, upload, and texture-index
  rejection, which the prerequisite owns.
- Binding numbers, including binding 8, and every other binding's set.
- `FilterWritesByShaderLayout`, `ResolveBindingSetIndex`, and the Set 0 drop
  in `RouteWritesBySet`.
- `ModelPipeline::UpdateStorageBufferDescriptors` and the shadow variant's
  per-material Set 1 ownership.
- The descriptor pool sizes and flags.
- The shader pack format and `kiVersion`.

## Risk tier

Tier 2 — scoped behavior. Triggers: the client renderer's model pipeline
layout changes from three descriptor sets to two, a shader's descriptor set
qualifier changes so shader data is rebuilt, and public `Pipeline` members and
a member function are removed or renamed. It stays inside the client graphics
subsystem: binding numbers, buffer contents, push constants, and draw order
are unchanged, and a slip shows as a misbound material or a validation error.
No determinism/CRC, wire, serialization, `.pack` format, `kiVersion`, replay,
threading, or trust-boundary exposure; the server compiles none of these
files' client paths.

## Acceptance criteria

1. A repository search for `mSet2VkDescriptorSetLayout`,
   `mVkDescriptorSetsSet2`, `RecordDrawIndirectSet2`, and `vkDestinationSet2`
   returns nothing, and no shader under `Engine/Data/Shaders` declares
   `set = 2`.
2. No pipeline layout is created with three set layouts.
3. The three checks under `## Design` are each reported with their outcome.
4. `/compile` builds the BrokenEngineSandbox client, the server, and
   ModelViewer with regenerated shader data.
5. Through `/agent-harness`, one client run with validation layers on shows no
   validation error, and:
   - a RenderDoc capture of a frame that draws at least two model scenes shows
     each main model pipeline layout with two sets, binding 8 written in each
     scene's Set 1 with that scene's "Materials" buffer, and no Set 2 bind;
   - screenshots of a world frame with models (opaque and transparent, with
     shadows) and a ModelViewer model screenshot match the same scenes before
     the change.

   The supplied evidence records no harness setup.

## Coordination

`Documents/Plans/Engine/PipelineObjectsDeadCode.md` edits other regions of
`PipelineCreator.cpp`, `Pipeline.h`, `Pipeline.cpp`, and `ModelPipeline.cpp`,
calls `SplitBindingsBySet` and `CreateMultiSetPipelineLayout` from
`PipelineCreator::CreateComputePipeline` with scratch Set 2 storage, and
deletes an ASSERT in `RecordDrawIndirectSet2`. Neither Plan depends on the
other; whichever lands second re-finds its lines by symbol and adapts to the
landed helper signatures and function name.

## Notes

- After this change `kMultiSet` means "material pipelines share pipeline 0's
  Set 1 and bind it once"; a rename to match is a possible later cleanup and
  is not recorded as a Plan.
- Nothing was built or run by the authoring session; reflected sets were read
  from GLSL source, not from a built pack.
