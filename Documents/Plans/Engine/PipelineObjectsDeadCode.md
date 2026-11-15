<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:55:07.658Z","dependsOn":[]} -->
# Remove dead code and duplication from the graphics pipeline objects

## Context

`Objects/` is `Engine/Source/Graphics/Objects/`, `Managers/` is
`Engine/Source/Graphics/Managers/`, and `PC` is
`Objects/PipelineCreator.cpp`. Verified against commit `c675b6b2`; line
numbers are from that commit and move as earlier Design steps land, so each
step re-finds its region by symbol. Each "no producer", "no caller", or "only
references" search covered `Engine`, `Projects`, `Tools`, `DataPacker`, and
`Common`.

Origin: an architecture review and an in-function refactor review of
`Engine/Source/Graphics/Objects`, each finding independently verified. Unmet
criteria: root `AGENTS.md` `## Directives` "no speculative ...
configuration", DRY (reuse existing mechanisms; no export without a user),
minimum sufficient change and YAGNI (no code without a caller, no parameter
with one value), and `Documents/C++StyleGuide.txt` rule 64 (every statement a
comment makes must be true).

### Push-constant size override

`PipelineInfo::iPushConstantBytes` (`Objects/Pipeline.h:115-117`) lets a
pipeline declare a push-constant range smaller than
`shaders::PushConstantsLayout`. No file assigns it. Its only references are
the declaration, the reader `ResolvePushConstantBytes` (`PC:16-21`, called at
`PC:78`, `PC:226`, `PC:691`), the comment above `RecordPushConstants`
(`Objects/Pipeline.cpp:15-16`), and six `ASSERT(mInfo.iPushConstantBytes == 0)`
lines (`Objects/Pipeline.cpp:223`, `:239`, `:254`, `:277`, `:299`, `:328`).

The member comment names LightCombine as the pipeline that would use the
override. `Engine/Data/Shaders/Lighting/LightCombine.comp` has no
`push_constant` block, and its pipeline
(`Managers/WorldLightingShadowPipelines.cpp:83`) does not set
`kPushConstants`. Every `push_constant` block under `Engine/Data/Shaders`
declares `PushConstantsLayout` (`Engine/Data/Shaders/ShaderLayoutsBase.h:210`).
So the three size sites always pass `sizeof(shaders::PushConstantsLayout)` and
the six ASSERTs can never fire.

Separately, the comment at `PC:594` in
`PipelineCreator::CreateGraphicsPipeline` says the shader modules, the layout,
and the render pass are resolved after `CreateDescriptorSetLayouts`. Only the
layout (`PC:598`) depends on that call: the modules (`PC:595-596`) read
`mVkShaderModule`, which the call does not write, and the render pass
(`PC:601`) reads `mInfo` and the swapchain manager.

### Compute layout duplicates the set helpers

`PipelineCreator::CreateComputePipeline` builds its global-Set-0 plus
per-pipeline-Set-1 layout inline, repeating two helpers the graphics path
already calls:

- `PC:661-671` is the Set 1 half of `SplitBindingsBySet` (`PC:155-173`), with
  the same `Pipeline::ResolveBindingSetIndex` filter.
- `PC:675-681` matches the Set 1 layout creation in
  `CreateMultiSetPipelineLayout` (`PC:182-188`) statement for statement.
- `PC:683-697` matches that helper's pipeline-layout assembly (`PC:203-215`)
  with a fixed two-set array and the compute stage flag.

The helper's preconditions hold for compute: `PC:640` asserts the pipeline is
not `kMultiSet`, so the helper takes its two-set path and creates no Set 2,
and no compute producer sets `vkExternalSet1DescriptorSetLayout` (its only
writer is `ModelPipeline::Create`), so the helper creates Set 1 itself.

### Duplicated descriptor-set binder

`BindComputeDescriptorSets` is declared in `Objects/Pipeline.h:190-191` and
defined in `Objects/Pipeline.cpp:65-77`. Its only callers are in the same file
(`Objects/Pipeline.cpp:305`, `:334`). The file-static
`BindGraphicsDescriptorSets` (`Objects/Pipeline.cpp:51-63`, callers `:229`,
`:245`, `:259`) has the same body. The only difference is the bind-point
constant: `VK_PIPELINE_BIND_POINT_GRAPHICS` at `:57` and `:61` against
`VK_PIPELINE_BIND_POINT_COMPUTE` at `:71` and `:75`. So the header exports a
function nothing outside the file uses, and the rule "global Set 0 at the
framebuffer index, per-pipeline Set 1 at the `mbPerCommandBuffer` index" is
written twice.

### External Set 0 layout override

`PipelineInfo::vkExternalDescriptorSetLayout` (`Objects/Pipeline.h:105-106`)
has one writer and one reader. The writer, `ModelPipeline::Create`
(`Objects/ModelPipeline.cpp:133`), assigns
`gpTextureManager->mTextureDescriptors.mGlobalVkDescriptorSetLayout`. The
reader, `Pipeline::Create` (`Objects/Pipeline.cpp:117`), uses the field when
it is non-null and otherwise that same global layout. Both arms yield the same
handle, so the field can carry only one effective value. Every `PipelineInfo`
producer uses designated initializers, so removing the member breaks no
positional initializer. `mGlobalVkDescriptorSetLayout` has no other reader
outside `Managers/TextureDescriptors.cpp`.

In the same function, the index-start check at `Objects/ModelPipeline.cpp:103`
reads `(i > 0 && iIndexStart < iPreviousIndexStart) || ...`.
`iPreviousIndexStart` starts at 0 (`:99`) and `iIndexStart` is widened from an
unsigned `uint32_t` (`:96`, `:102`), so at `i == 0` the comparison is already
false and the `i > 0` conjunct changes no result. It makes the check read as
if the first entry were exempt.

### Unproduced flag and layout values

Four enum values have no producer, and one sampler slot is reached only
through one of them:

- `DescriptorFlags::kSamplerClampLinear` and `kSamplerRepeatLinear`
  (`Objects/Pipeline.h:27-28`). Their only other uses are two rows of the
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
  cull-mode ternary in `ConfigureRasterization` (`PC:388`). It is the twin of
  the live `kCullBack`.

### Uncalled Texture::UpdateData

`Texture::UpdateData` (`Objects/Texture.h:106`, `Objects/Texture.cpp:232-236`)
has no caller; the search finds only its declaration and definition. It is
also the only source of a second value pair for the `eOldLayout` and
`eFinalLayout` parameters of `Texture::UploadImageData` and
`Texture::RecordImageData` (`Objects/Texture.h:118-119`): it passes
`kShaderReadOnly, kShaderReadOnly` (`Objects/Texture.cpp:235`). The two live
callers pass `kUndefined, mInfo.eTextureLayout` (`Objects/Texture.cpp:161` in
`Create`, `:184` in `RecordCreate`), and `UploadImageData` only forwards
(`Objects/Texture.cpp:245`). With `UpdateData` gone, both parameters carry one
value pair.

## Design

Recommended, top to bottom. Leave every remaining enum value's bits
unchanged.

1. Push-constant size override:
   - Delete `PipelineInfo::iPushConstantBytes` and its comment.
   - Delete `ResolvePushConstantBytes` and its comment; at its three call
     sites use `sizeof(shaders::PushConstantsLayout)` directly.
   - Delete the six `ASSERT(mInfo.iPushConstantBytes == 0)` lines and trim the
     comment above `RecordPushConstants` so it no longer names the override.
   - Reword the comment above the shader-module assignments in
     `CreateGraphicsPipeline` so it claims the ordering only for the pipeline
     layout.

   The six `if (mInfo.flags & kPushConstants)` blocks keep their shape.
   Folding the flag test into `RecordPushConstants` was reviewed and judged
   cosmetic: it would make a function named "record" silently do nothing.
2. Compute layout: in `CreateComputePipeline`, build the compute-stage
   `VkPushConstantRange`, call `SplitBindingsBySet` on the filtered bindings,
   and under the existing condition (`mExternalVkDescriptorSetLayout` non-null
   and a Set 1 count above zero) call `CreateMultiSetPipelineLayout`. The
   `else` arm stays as it is: it clears `mExternalVkDescriptorSetLayout` and
   calls `CreateSingleSetPipelineLayout`, because
   `Engine/Source/Graphics/Managers/AGENTS.md` `## Shared Contracts` keeps
   standalone compute pipelines. `SplitBindingsBySet` also fills a Set 2
   array; compute passes scratch storage for it and an empty span onward.

   One fact is not established and is reported, not acted on: whether the
   graphics single-set branch of `CreateDescriptorSetLayouts` (`PC:238-241`)
   is reachable. It runs only when the global Set 0 layout is null at
   `Pipeline::Create`; `Managers/TextureDescriptors.cpp` creates that layout
   and `Engine/Source/Graphics/AGENTS.md` `## Frame and Resource Lifecycle`
   says the texture manager is constructed before the pipeline manager. The
   executing session reads the construction order and states in its handoff
   whether the branch is reachable. The edit is the same either way; an
   unreachable branch is a residual for a separate change, not a removal
   here.
3. Descriptor-set binder: keep one file-static binder in `Pipeline.cpp` that
   takes a `VkPipelineBindPoint`, pass the constant at the five call sites,
   and delete the `BindComputeDescriptorSets` declaration and its comment from
   `Pipeline.h`. A bind-point parameter keeps the graphics and compute paths
   parallel. The comment that states the Set 0 / Set 1 indexing rule stays
   once, on the remaining binder.
4. External Set 0 layout override:
   - Delete `PipelineInfo::vkExternalDescriptorSetLayout` and its comment,
     delete the assignment in `ModelPipeline::Create`, and have
     `Pipeline::Create` assign `mGlobalVkDescriptorSetLayout` to
     `mExternalVkDescriptorSetLayout` unconditionally.
   - Delete `i > 0 &&` and the inner parentheses it needed from the
     index-start check. The `std::ios_base::failure` throw stays whole: it is
     the pack trust boundary `Engine/Source/Graphics/Objects/AGENTS.md`
     `## Resource Contracts` names, and every input is accepted or rejected as
     before.
5. Unproduced flag and layout values:
   - Delete `kSamplerClampLinear` and `kSamplerRepeatLinear`, and delete their
     two rows from the `GetSampler` table.
   - Delete `kSamplerSlotRepeatLinear` and the sampler creation that fills it.
     The create-info state those three address-mode assignments set is used by
     nothing after them.
   - Delete `TextureLayout::kGeneral` together with its `kpLayoutMappings`
     row. The table is positional and the `static_assert` after it checks only
     the total count, so the value and its row go in the same edit.
   - Delete `kCullFront`; the cull mode becomes back culling when `kCullBack`
     is set and none otherwise.
   - In `Engine/Source/Graphics/Managers/AGENTS.md` `### TextureManager`, the
     sentence listing the samplers that omit anisotropy says "explicit
     offscreen linear variants". With both flags gone
     `kSamplerMirroredRepeatLinear` is the only explicit linear variant, and
     `kSamplerSlotLinearClamp` is reached only unflagged, so the plural
     becomes singular.
6. `Texture::UpdateData`, as one edit (removing the parameters alone would
   break `UpdateData`):
   - Delete `Texture::UpdateData`.
   - Remove `eOldLayout` and `eFinalLayout` from `UploadImageData` and
     `RecordImageData`. `RecordImageData` transitions from `kUndefined` and
     ends at `mInfo.eTextureLayout`.
   - Keep the "final layout is not `kUndefined`" test before the last
     transition, now on `mInfo.eTextureLayout`: several producers leave
     `eTextureLayout` at `kUndefined` (`Managers/SwapchainManager.cpp`,
     `TextureManager.cpp`, `ImGuiManager.cpp`).

## Critical files

- `Engine/Source/Graphics/Objects/Pipeline.h`
- `Engine/Source/Graphics/Objects/Pipeline.cpp`
- `Engine/Source/Graphics/Objects/PipelineCreator.cpp`
- `Engine/Source/Graphics/Objects/ModelPipeline.cpp`
- `Engine/Source/Graphics/Objects/Texture.h`
- `Engine/Source/Graphics/Objects/Texture.cpp`
- `Engine/Source/Graphics/Managers/TextureManager.h`
- `Engine/Source/Graphics/Managers/TextureManager.cpp`
- `Engine/Source/Graphics/Managers/AGENTS.md`

## In scope

- `Pipeline.h`: `PipelineInfo::iPushConstantBytes`,
  `PipelineInfo::vkExternalDescriptorSetLayout`, and their comments;
  `DescriptorFlags::kSamplerClampLinear` and `kSamplerRepeatLinear`;
  `PipelineFlags::kCullFront`; the
  `BindComputeDescriptorSets` declaration and its comment.
- `Pipeline.cpp`: `BindGraphicsDescriptorSets` and
  `BindComputeDescriptorSets`, merged into one file-static function; the
  binder call and the `iPushConstantBytes` ASSERT in `Pipeline::RecordDraw`,
  `RecordBindPipelineAndDescriptors`, `RecordDrawIndirect`, `RecordCompute`,
  and `RecordComputeIndirectFrom`; the `iPushConstantBytes` ASSERT in
  `RecordDrawIndirectSet2`; the comment above `RecordPushConstants`; the
  `mExternalVkDescriptorSetLayout` assignment in `Pipeline::Create`.
- `PipelineCreator.cpp`:
  - `ResolvePushConstantBytes` and the `vkPushConstantRange.size` assignment
    in `CreateSingleSetPipelineLayout`, `CreateDescriptorSetLayouts`, and
    `PipelineCreator::CreateComputePipeline`.
  - The comment above the shader-module assignments in
    `PipelineCreator::CreateGraphicsPipeline`.
  - `PipelineCreator::CreateComputePipeline`: the Set 1 partition loop, the
    inline Set 1 layout creation, and the inline two-set pipeline-layout
    assembly, replaced by calls to `SplitBindingsBySet` and
    `CreateMultiSetPipelineLayout`; the comment above the partition loop, if
    the edit makes it incorrect.
  - The read-only reachability check of the single-set branch in
    `CreateDescriptorSetLayouts`.
  - The cull-mode expression in `ConfigureRasterization`.
- `ModelPipeline.cpp`, in `ModelPipeline::Create`: the
  `vkExternalDescriptorSetLayout` assignment and the `i > 0` conjunct of the
  index-start check.
- `Texture.h` / `Texture.cpp`: `TextureLayout::kGeneral` and its
  `kpLayoutMappings` row; `Texture::UpdateData`, declaration and definition;
  the `eOldLayout` and `eFinalLayout` parameters of
  `Texture::UploadImageData` and `Texture::RecordImageData`, their uses inside
  `RecordImageData`, and the arguments at the call sites in `Texture::Create`,
  `Texture::RecordCreate`, and `UploadImageData`.
- `TextureManager.h` / `TextureManager.cpp`:
  `TextureManager::SamplerSlot::kSamplerSlotRepeatLinear`, the `RepeatLinear`
  sampler creation at the end of `TextureManager::CreateSamplers`, and the two
  table rows in `TextureManager::GetSampler`.
- `Engine/Source/Graphics/Managers/AGENTS.md` `### TextureManager`: the
  sentence naming the explicit offscreen linear variants.

## Out of scope

- The `kPushConstants` flag, its producers, and the six guard blocks' shape.
- Moving the depth-test, depth-write, or shader-module assignments in
  `CreateGraphicsPipeline` into their initializers.
- Any shader or `PushConstantsLayout` change.
- The standalone-compute `else` arm and the `iSet1Count > 0` condition in
  `CreateComputePipeline`.
- The signatures and bodies of `SplitBindingsBySet`,
  `CreateMultiSetPipelineLayout`, and `CreateSingleSetPipelineLayout`,
  including giving the single-set helper a `VkPushConstantRange` parameter or
  moving `setLayoutCount` into it.
- Removing the graphics single-set branch.
- The indirect-buffer setup and the empty-binding filter at the top of
  `CreateComputePipeline`.
- The inline Set 0 + Set 1 bind in `ModelPipeline::RecordDrawIndirect`
  (`Objects/ModelPipeline.cpp:158-159`) and the Set 2 bind in
  `Pipeline::RecordDrawIndirectSet2`.
- The descriptor-set index each binder caller passes, including the
  unconditional framebuffer index in `Pipeline::RecordDrawIndirect`.
- The member `Pipeline::mExternalVkDescriptorSetLayout`:
  `PipelineCreator::CreateComputePipeline` clears it for standalone compute
  pipelines.
- `PipelineInfo::vkExternalSet1DescriptorSetLayout` and the shared Set 1
  hand-off in `ModelPipeline::Create`.
- The rest of the index-start check, its throw, and the other scene-header
  checks.
- `kSamplerMirroredRepeatLinear`, `kSamplerSlotLinearClamp`, and every other
  sampler flag, slot, and sampler.
- `DescriptorFlags::kUniformBuffer` and `kPerCommandBufferUniformBuffers`:
  whether they stay as documented routing vocabulary is an open owner
  question.
- `VK_IMAGE_LAYOUT_GENERAL` uses that do not go through
  `TextureLayout::kGeneral`.
- Renumbering any remaining flag value.
- `Texture::TransitionImageLayout` and its own `eOldLayout` parameter.
- The layout transition `Texture::Create` performs when no data function is
  supplied.
- `CreateImageView`.

## Risk tier

Tier 2 — scoped behavior. Triggers: fields are removed from `PipelineInfo`,
enumerators from header-declared enums, a header-declared function
(`BindComputeDescriptorSets`), and a public `Texture` method, all of which
Tier 1's "no public signature exposure" excludes; layout creation for every
non-standalone compute pipeline moves to shared helpers, one subsystem's
runtime path; one never-bound `VkSampler` is no longer created; and one edit
is inside a pack trust-boundary check.

Invariants: the values passed to Vulkan are the same, the compute create-info
contents are unchanged, every call binds the same sets at the same bind
point, every reachable upload records the same transitions, the layout handle
used is the same, and the set of rejected pack inputs is unchanged. No
pipeline, descriptor, or image changes. Client-only; no determinism/CRC, wire,
serialization, `.pack`, replay, threading, or shader exposure.

## Acceptance criteria

- A repository search for `iPushConstantBytes`, `ResolvePushConstantBytes`,
  `vkExternalDescriptorSetLayout`, `kSamplerClampLinear`,
  `kSamplerRepeatLinear`, `kSamplerSlotRepeatLinear`, `kCullFront`, and the
  whole word `kGeneral` returns nothing.
- No comment in `CreateGraphicsPipeline` says the shader modules or the render
  pass depend on `CreateDescriptorSetLayouts`.
- `CreateComputePipeline` holds no inline `vkCreateDescriptorSetLayout` or
  `vkCreatePipelineLayout` call.
- `BindComputeDescriptorSets` appears in no header, and `Pipeline.cpp` holds
  one descriptor-set binder.
- The index-start check in `ModelPipeline::Create` has no `i > 0` conjunct and
  still throws `std::ios_base::failure`.
- `Engine/Source/Graphics/Managers/AGENTS.md` `### TextureManager` names one
  explicit offscreen linear variant, not several, among the samplers that omit
  anisotropy.
- A repository search for `UpdateData` finds no `Texture` member. In
  `Texture.h` and `Texture.cpp`, `eOldLayout` appears only as
  `TransitionImageLayout`'s own parameter, and `eFinalLayout` does not appear.
- The client and ModelViewer build.
- Through `/agent-harness`, a world frame renders with the validation layer
  reporting no error; shadow, lighting, smoke, and particle compute all run in
  it. The supplied evidence records no harness setup.
- The handoff states whether the graphics single-set branch of
  `CreateDescriptorSetLayouts` is reachable.

## Coordination

`Documents/Plans/Engine/RetireModelMaterialsDescriptorSet2.md` edits other
regions of `PipelineCreator.cpp`, `Pipeline.h`, `Pipeline.cpp`, and
`ModelPipeline.cpp`. Neither Plan depends on the other; whichever lands second
re-finds its lines by symbol and adapts to the landed helper signatures and
function name.

## Notes

- The private game project in the primary checkout holds only documents and no
  source, so it names none of these symbols and has no caller. If a game
  source tree exists elsewhere, it must still build.
- The Linear sampler family is not a required parallel of the non-Linear one:
  there is no linear border variant.
- Nothing was built or run by the authoring session; the evidence and the
  equivalence claims come from reading and searching the tree.
