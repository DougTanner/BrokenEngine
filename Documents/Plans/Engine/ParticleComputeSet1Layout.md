<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-15T20:28:46.094Z","dependsOn":[]} -->
# Move the particle compute shaders to Set 1 and remove standalone compute layouts

## Context

`Engine/Data/Shaders/Particles/ParticlesSpawn.comp` and
`Engine/Data/Shaders/Particles/ParticlesUpdate.comp` are the only compute
shaders whose bindings all sit in set 0: `ParticlesSpawn.comp:9-29` declares
`globalUniform` at `binding = 0` and four storage buffers at bindings 1-4,
and `ParticlesUpdate.comp:8-18` declares `globalUniform` at `binding = 0`, a
storage buffer at binding 1, and `elevationTextureSampler` at binding 2, none
with a `set =` qualifier. Every other tracked `.comp` shader under
`Engine/Data/Shaders/` declares per-pipeline bindings with `set = 1` and, when
it reads the global uniform, uses
`layout (set = 0, binding = kiGlobalBindingGlobalUniform)`
(`Shadow/Shadow.comp:8-14`, `Water/WaterFoam.comp:8-22`). No tracked project
outside `Engine/` has a compute shader.

Because the particle shaders have no Set 1 binding,
`PipelineCreator::CreateComputePipeline`
(`Engine/Source/Graphics/Objects/PipelineCreator.cpp:654-664`) takes its
`else` arm for the four particle compute pipelines that
`PipelineManager.cpp:386-427` creates (`LongParticlesUpdate`,
`SquareParticlesUpdate`, `LongParticlesSpawn`, `SquareParticlesSpawn`). That
arm sets `Pipeline::mExternalVkDescriptorSetLayout` to `VK_NULL_HANDLE` and
calls `CreateSingleSetPipelineLayout` (`PipelineCreator.cpp:54-75`), so the
shader's set-0 bindings form the pipeline's own single set. The null layout
then drives three special cases:

- the file-static `BindDescriptorSets` in
  `Engine/Source/Graphics/Objects/Pipeline.cpp:45-57` binds one set instead of
  the global Set 0 plus Set 1;
- `BindingIsInSet0` in
  `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp:11-20` returns
  `false` for every binding;
- `PipelineDescriptorWriter::Write` (`PipelineDescriptorWriter.cpp:217` and
  `:346-349`) skips `RouteWritesBySet`, so every write goes to the single set.

`Pipeline::Create` (`Pipeline.cpp:97`) otherwise always assigns the global
Set 0 layout, and the compute `else` arm is the only place that clears it.
`CreateSingleSetPipelineLayout` has no other caller.
`Engine/Source/Graphics/Managers/AGENTS.md` `## Shared Contracts` records the
exception: legacy standalone compute pipelines keep their own layouts and do
not consume global Set 0.

Originating gap: a design suggestion from the over-engineering review of the
session that completed the pipeline-objects dead-code cleanup; the user chose
to record it as a follow-up. That cleanup kept the `else` arm and
`CreateSingleSetPipelineLayout` out of its scope because the documented
contract still kept standalone compute pipelines.

## Design

Recommended, in order:

1. Shaders. In both particle shaders, declare `globalUniform` as
   `layout (set = 0, binding = kiGlobalBindingGlobalUniform)`, as
   `Shadow/Shadow.comp` does, and add `set = 1` to every other binding,
   keeping each binding number. `kiGlobalBindingGlobalUniform` is 0
   (`Engine/Data/Shaders/ShaderLayoutsBase.h:121`), so descriptor index 0
   still maps to binding 0. The recommendation reads the global uniform from
   global Set 0 rather than redeclaring it in Set 1, because that is the
   pattern every other compute shader follows, and the existing
   `{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers}` entry at index 0
   of each particle pipeline's `descriptorInfos` is then filtered out of the
   per-pipeline writes the same way the shadow pipelines' entry is
   (`WorldLightingShadowPipelines.cpp:177`). The `descriptorInfos` lists in
   `PipelineManager.cpp` do not change.
2. `PipelineCreator::CreateComputePipeline`: call
   `CreateMultiSetPipelineLayout` unconditionally, delete the `iSet1Count > 0`
   test and the `else` arm with its comment, and update the comment above the
   split (`PipelineCreator.cpp:647`) so it no longer says "when Set 1 bindings
   exist". Delete `CreateSingleSetPipelineLayout`, which is then uncalled.
3. `Pipeline.cpp` `BindDescriptorSets`: keep only the two-set bind, delete the
   one-set arm, and drop the `vkExternalLayout` parameter it no longer reads,
   updating every caller in `Pipeline.cpp`.
4. `PipelineDescriptorWriter.cpp`: delete the null-layout early return in
   `BindingIsInSet0`; in `PipelineDescriptorWriter::Write`, delete
   `bHasExternalSet0` and call `RouteWritesBySet` unconditionally; fix the
   Set 1 allocation comment (`PipelineDescriptorWriter.cpp:248`) that still
   mentions a single set for compute pipelines.
5. Documentation: in `Engine/Source/Graphics/Managers/AGENTS.md`
   `## Shared Contracts`, replace "Most pipelines consume global Set 0, but
   legacy standalone compute pipelines retain their own layouts and do not."
   with a statement that every pipeline consumes global Set 0, and rewrap only
   that paragraph. Run `/update-claude-docs` for any other affected statement.

After the change no code path sets `mExternalVkDescriptorSetLayout` to null.
The member itself stays: `CreateMultiSetPipelineLayout` reads it
(`PipelineCreator.cpp:197`).

## Critical files

- `Engine/Data/Shaders/Particles/ParticlesSpawn.comp`
- `Engine/Data/Shaders/Particles/ParticlesUpdate.comp`
- `Engine/Source/Graphics/Objects/PipelineCreator.cpp`
- `Engine/Source/Graphics/Objects/Pipeline.cpp`
- `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp`
- `Engine/Source/Graphics/Managers/AGENTS.md`

## In scope

- The binding declarations of `ParticlesSpawn.comp` and
  `ParticlesUpdate.comp`.
- In `PipelineCreator.cpp`: the `iSet1Count > 0` condition, the standalone
  `else` arm and its comment in `PipelineCreator::CreateComputePipeline`, the
  comment above its `SplitBindingsBySet` call, and the whole
  `CreateSingleSetPipelineLayout` function.
- In `Pipeline.cpp`: the one-set arm and the `vkExternalLayout` parameter of
  the file-static `BindDescriptorSets`, and that argument at each of its call
  sites.
- In `PipelineDescriptorWriter.cpp`: the null-layout test in
  `BindingIsInSet0`, `bHasExternalSet0` and its guard in
  `PipelineDescriptorWriter::Write`, and the Set 1 allocation comment in
  `Write`.
- The standalone-compute sentence in `Engine/Source/Graphics/Managers/AGENTS.md`
  `## Shared Contracts`, plus any statement `/update-claude-docs` finds made
  false by this change.

## Out of scope

- The particle pipelines' `descriptorInfos`, flags, and dispatch recording in
  `PipelineManager.cpp` and `CommandBufferRecordGlobal.cpp`.
- Any change to particle shader logic, `ShaderLayouts.h`, or
  `ShaderLayoutsBase.h`.
- The member `Pipeline::mExternalVkDescriptorSetLayout` and its assignment in
  `Pipeline::Create`.
- `SplitBindingsBySet`, `CreateMultiSetPipelineLayout`, `RouteWritesBySet`,
  `Pipeline::ResolveBindingSetIndex`, and the Set 2 paths.
- Any other compute or graphics shader or pipeline.
- Adding an assertion that a compute pipeline has Set 1 bindings.

## Risk tier

Tier 2 — scoped behavior. Trigger: a shader change that alters the pipeline
layout and descriptor binding of the four particle compute pipelines (one
global Set 0 plus a per-pipeline Set 1 instead of one standalone set), which
is one subsystem's runtime behavior in the client renderer.

Exposure: shader and `.pack` content (the two shaders are repacked by the
normal DataPacker build; no format or `kiVersion` changes); client-only
graphics. No determinism/CRC, wire, serialization, save, replay, threading,
allocation, or trust-boundary exposure. Invariants: the particles read the
same global uniform, storage buffers, and elevation texture as before, and
every compute pipeline binds global Set 0 at the framebuffer index and its
Set 1 at its descriptor-set index.

## Acceptance criteria

1. `ParticlesSpawn.comp` and `ParticlesUpdate.comp` declare no per-pipeline
   binding in set 0; `CreateSingleSetPipelineLayout` no longer exists, and no
   code in `Engine/Source/Graphics/` tests `mExternalVkDescriptorSetLayout`
   against `VK_NULL_HANDLE` or assigns it `VK_NULL_HANDLE`.
2. The client builds through `/compile`, and the shaders repack without error.
3. Live run through `/agent-harness`: launch the client and server, then
   `query_profile` on the client reports the Particles Spawn GPU timer above
   0, and `get_logs` with `{"pattern":"DebugUtilsCallback"}` returns nothing.
   The supplied evidence records no other harness setup.
4. `Engine/Source/Graphics/Managers/AGENTS.md` `## Shared Contracts` no longer
   describes standalone compute pipelines.

## Notes

- Line numbers are from the tree at the end of the session that recorded this
  Plan; re-find each site by symbol.
