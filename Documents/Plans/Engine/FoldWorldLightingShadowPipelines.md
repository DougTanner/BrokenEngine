<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:18:00.689Z","dependsOn":[]} -->
# Fold WorldLightingShadowPipelines into PipelineManager

## Context

A path without a directory is under `Engine/Source/Graphics/Managers/`. Lines
are those at commit `401e4b75`, except that the `PipelineManager.cpp` and
`WorldLightingShadowPipelines.cpp` lines reflect the authoring session's
later edits to those files.

`class WorldLightingShadowPipelines` owns nothing. It holds one reference and
nine pointers into its owner (`WorldLightingShadowPipelines.h:24-33`), is
built with ten arguments (`PipelineManager.h:116`), and has four methods
called only at `PipelineManager.cpp:82-85`; no other source names the class.
Besides lighting and shadow pipelines it also builds Terrain, Water,
WaterDisplacement, and WaterFoam (`WorldLightingShadowPipelines.cpp:257-370`).

Every aliased member already has its owner's member name (`mpPipelines`,
`mpSpreadPipelines`, `mpSpreadPipelineNames`, `mpCombinePipelines`,
`mpLightingTemporalPipelines`, `mpLightingHistoryCopyPipelines`,
`mppWaterNormalTextures`, `mppWaterFoamTextures`,
`mppWaterFoamBreakupTextures`), so as `PipelineManager` members the four
bodies change only `mrShaders` to `mShaders`. Adding a lighting array today
takes four sites (owner member, constructor parameter and initializer, helper
member, construction argument); folded, it takes one.

The file-size split is not what the class provides: the second `.cpp` is.
`RenderTargetTexturesLighting.cpp:17,62,415` is the existing precedent for a
second `.cpp` that defines members of a class declared elsewhere. The other
owned-by-value helpers `AGENTS.md` lists own state (`DynamicPipelines` owns
five containers), so removing this one breaks no mirrored pattern.

Originating gap: the architecture phase (cohesion lens) of an analysis of
`Engine/Source/Graphics/Managers` at that commit. Unmet criterion: fixed
lighting, shadow, terrain, and water pipeline creation reaches
`PipelineManager` state directly, with no aliasing helper.

## Design

Recommended:

- The four functions become `PipelineManager` members, declared in
  `PipelineManager.h`, and stay defined in the second `.cpp`.
- Delete `WorldLightingShadowPipelines.h`, the class's constructor and its
  ten members, the include at `PipelineManager.h:6`, the member at
  `PipelineManager.h:116`, and its initializer. `PipelineManager.cpp:82-85`
  calls the members directly.
- File name: keep `WorldLightingShadowPipelines.cpp`. Rationale: the
  smallest change, no project-file rename, and other Plans cite the file by
  that name. The precedent names such a part after its owner
  (`RenderTargetTexturesLighting.cpp`), so the kept name no longer matches a
  class; the executing session states that in its report, and a rename is
  not part of this Plan.
- Remove the deleted header from both client projects and their filters
  through `/update-vcxproj`.
- `Engine/Source/Graphics/Managers/AGENTS.md` is synchronized where it names
  the class.

No pipeline, descriptor, or creation order changes.

## Critical files

- `Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.h` (deleted)
- `Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.cpp`
- `Engine/Source/Graphics/Managers/PipelineManager.h`
- `Engine/Source/Graphics/Managers/PipelineManager.cpp`
- The project and filter files of BrokenEngineSandbox and ModelViewer

## In scope

- The class `WorldLightingShadowPipelines`, its header, and its project and
  filter entries.
- `WorldLightingShadowPipelines.cpp:3,18-30` and the `mrShaders` uses in the
  four functions.
- `PipelineManager.h:6,84-87,94,116` and `PipelineManager.cpp:82-85`.
- Documentation sync for the removed class name.

## Out of scope

- The bodies of the four functions beyond the member respelling, including
  the shadow compute description table and its `Create` loop in
  `CreatePipelineShadows`.
- `DynamicPipelines` and the other owned-by-value helpers.
- Renaming or re-splitting the `.cpp` files.
- `Documents/Features/Graphics/ScreenSpaceIndirectLightingInputs.md:219`,
  which names the class and goes stale; `Documents/Features` is manual.

## Risk tier

Tier 2 — scoped behavior. Trigger: a class is removed and declarations
change in `PipelineManager.h`, which `Engine.h` aggregates, and project
membership changes; more than Tier 1's "no public signature". No runtime
behavior, threading, determinism, layout, or wire exposure.

## Acceptance criteria

1. No `WorldLightingShadowPipelines` class or header remains, and the four
   functions are `PipelineManager` members.
2. `/update-vcxproj` validation passes for both client projects.
3. The BrokenEngineSandbox client and ModelViewer build through `/compile`.
4. Through `/agent-harness`, a client run with validation layers on creates
   every fixed pipeline and renders a world frame with lighting, shadows,
   terrain, water, and foam with no validation error. The supplied evidence
   records no harness setup.
