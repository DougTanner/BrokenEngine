<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:20:12.608Z","dependsOn":[]} -->
# Remove inert sampler and bindless-array descriptor entries

## Context

`M/` is `Engine/Source/Graphics/Managers/`, `O/` is
`Engine/Source/Graphics/Objects/`, and `S/` is `Engine/Data/Shaders/`. Lines
are those at commit `401e4b75`.

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
- `PipelineDescriptorWriter::Write` (`O/PipelineDescriptorWriter.cpp`) gives
  each entry the number `iExplicitBinding` or else the running write count
  (`:477`). `FilterWritesByShaderLayout` (`:321-342`) then drops a write
  whose number no shader declares, and `RouteWritesBySet` (`:344-364`) drops
  a write whose number reflects as Set 0. Neither drop logs or asserts.
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
  (`O/PipelineDescriptorWriter.cpp:215`), whose write is always dropped. So
  `kSamplerRepeat` against `kSamplerClamp` at the two callers
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
and its writer branch (`O/PipelineDescriptorWriter.cpp:503-512`), and the
`WriteStandaloneSampler` function and branch (`:211-231`, `:499-502`), have
no producer: a repository search for standalone sampler and `kTextures`
entries finds only these four sites.

Originating gap: the architecture phase (data shape lens) of an analysis of
`Engine/Source/Graphics/Managers` at that commit. Unmet criterion: every
`descriptorInfos` entry of these four descriptions produces a write the
pipeline's own Set 1 receives, and no parameter selects a value nothing
consumes.

## Design

Recommended:

- Delete the eight entries.
- Remove the `eSamplerFlag` parameter of `CreateAreaLightingPipeline` and
  the argument at its two callers.
- Give the visible-lights elevation entry an explicit binding: add one Set 1
  binding constant to `S/ShaderLayoutsBase.h`, beside the existing Set 1
  constants, with the number the shader uses today, and use it both as
  `iExplicitBinding` at `M/DynamicPipelines.cpp:230` and in
  `VisibleLight.frag:19`. Rationale: `iExplicitBinding` with a shared
  constant is the existing mechanism (`O/ModelPipeline.cpp:49-58`,
  `M/WorldLightingShadowPipelines.cpp:381,408-412`). Renumbering elevation
  to the next free position is not available, because 3 and 4 are Set 0
  numbers in the same number space.
- Delete what is left without a producer: `DescriptorFlags::kTextures`, its
  writer branch, and `WriteStandaloneSampler` with its branch and the
  `bSampler` local. Rationale: the repository keeps no unreachable branch.
  The executing session repeats the producer search first; if it finds a
  producer, that item stays and is reported.

The descriptor sets the four pipelines end up with are the same as today:
only dropped writes are removed.

## Critical files

- `Engine/Source/Graphics/Managers/DynamicPipelines.cpp`
- `Engine/Source/Graphics/Managers/DynamicPipelines.h`
- `Engine/Source/Graphics/Managers/PipelineManager.cpp`
- `Engine/Source/Graphics/Objects/Pipeline.h`
- `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp`
- `Engine/Source/Frame/Collections/AreaLights/AreaLightsRender.cpp`
- `Engine/Source/Frame/Collections/PointLights/PointLightsRender.cpp`
- `Engine/Data/Shaders/ShaderLayoutsBase.h`
- `Engine/Data/Shaders/Lighting/VisibleLight.frag`

## In scope

- The eight entries and the `eSamplerFlag` parameter with its two caller
  arguments.
- One new Set 1 binding constant and its two uses.
- `DescriptorFlags::kTextures`, its writer branch, and
  `WriteStandaloneSampler` with its branch.
- Documentation sync for the removed flag and parameter.

## Out of scope

- The sampler and bindless-array writes inside `WriteModelDescriptor`
  (`O/PipelineDescriptorWriter.cpp:79-112`) and
  `RegisterStandaloneSamplerBinding`.
- Moving other positional Set 1 numbers to shared constants.
- `FilterWritesByShaderLayout` and `RouteWritesBySet`.
- `CreateModelPipeline` and `ModelPipelineSpec`.

## Risk tier

Tier 2 — scoped behavior. Trigger: a public member-function signature and a
public flag change, a shader and the shared layout header are edited, and
the writer loses two branches; more than Tier 1's local work. The binding
number, set layouts, and surviving writes are unchanged, so there is no
data-layout, determinism, wire, or threading exposure.

## Acceptance criteria

1. None of the eight entries, the `eSamplerFlag` parameter, or (unless a
   producer was found and reported) `kTextures` and `WriteStandaloneSampler`
   remains.
2. The elevation entry of `CreatePipelineVisibleLights` and
   `VisibleLight.frag` use one shared constant.
3. `/compile` builds the BrokenEngineSandbox client and ModelViewer with
   regenerated shader data.
4. Through `/agent-harness`, a client run with validation layers on shows
   area lights, point lights, visible lights (occluded by terrain as
   before), billboards, and particles with no validation error, and a
   screenshot matches the same scene before the change. The supplied
   evidence records no harness setup.

## Coordination

`Documents/Plans/Engine/AssertUnwrittenPipelineBindings.md` depends on this
Plan (metadata): its check is added after the dropped writes are gone.

`Documents/Plans/Engine/NamePushConstantSelectors.md` also edits
`Engine/Data/Shaders/ShaderLayoutsBase.h`, on other lines: it adds two
selector constant groups and deletes the `ELEVATION_FORMAT` line. Neither
Plan depends on the other; whichever lands later re-derives its lines by
symbol.

## Notes

- Not traced further: no shader under `Engine/Data/Shaders` declares a
  standalone sampler outside Set 0, so the standalone-sampler registry
  (`M/TextureDescriptors.cpp:553-557,631`) may have no reachable registrant.
  This Plan does not act on it.
- The executing session confirms no private-game caller passes
  `eSamplerFlag` where that source is available, and reports it as
  unconfirmed otherwise.
