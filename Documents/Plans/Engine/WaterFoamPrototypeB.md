<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-23T20:51:07.451Z","dependsOn":[]} -->
# Prototype B — compression with persistent fading coverage

Executable scheduler Plan; runtime execution approval pending. Research baseline `5f205af6a24a17ebf732f7005f03a48e0e6f91e0`. Execute in a dedicated preserved prototype worktree and branch, separate from A. Saving this Plan authorizes no runtime edits, builds, captures, or worktree setup now.

## Context

Use the current Jacobian compression to emit retained surface coverage, with a four-second half-life starting hypothesis. This is the recommended solution experiment for the fading requirement; A is an instantaneous control. B also adds coverage filtering, so the comparison measures both changes together rather than isolating persistence alone.

The [shared overview](../../Investigations/Graphics/WaterFoamPrototypes.md) is part of this complete design bundle: its source/shading, retained-coverage, GPU-order, altitude, and comparison sections define the exact math, resource formats, coordinate equations, ordering, and acceptance cohorts. Include those contracts in execution review.

## Design

Add WaterFoam.comp after displacement compute and before scene drawing. Read the four neighboring native J samples, mask each native land sample, apply the shared bounded compression source to each, then interpolate sources. Identity J=1 emits zero. Do not threshold an averaged J. This source and the cheap A source therefore differ in filtering as well as persistence.

Use the mapping, exact elapsed-time integration, two-image resource design, reset behavior, and fixed GPU chain owned by the [shared overview](../../Investigations/Graphics/WaterFoamPrototypes.md#retained-coverage-design-for-b). The [GPU contract](../../Investigations/Graphics/WaterFoamPrototypes.md#gpu-order-and-lifetime-for-b) requires format capability validation and device-drain recreation.

Stationary parameter-space history, the extended-band comparison preset, and bounded native-grid sources with measured limitations are Plan-authored design choices, not choices explicitly made by the user. The four-second initial half-life remains an experimental hypothesis.

## Critical files

- `Engine/Data/Shaders/Water/WaterDisplacement.comp:142-145` — existing J producer, unchanged by B.
- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp:45-53,314-325` — new fixed pass and image transitions.
- `Engine/Source/Graphics/Managers/RenderTargetTextures.cpp:30` and `.h:79` — water resource lifecycle.
- `Engine/Source/Graphics/Render/Render.h:59-87` — basis/area reuse.
- `Engine/Source/Graphics/Render/WaterUniforms.cpp:78-82` — monotonic elapsed-time precedent.

## In scope

- New Engine/Data/Shaders/Water/WaterFoam.comp covering source interpolation, parameter-space history reprojection, exact decay/emission integration, full allocation write, and reset/Low/disabled zero path.
- Engine/Data/Shaders/Water/Water.frag coverage sampler, UV mapping from existing f2InInitialPosition using current visible-area bounds, and shared prelighting/specular composition. Engine/Data/Shaders/Water/Water.vert is inspected only: existing UV is displaced, and initial position already provides the needed varying.
- Engine/Data/Shaders/ShaderGlobalLayout.h water foam controls, area pair, dt/reset; Engine/Data/Shaders/ShaderLayoutsBase.h explicit binding/format constants; Engine/Source/Graphics/Render/WaterUniforms.cpp single-owner publication; Engine/Source/Graphics/Render/Render.h external recreate flag and presentation continuity snapshot ownership.
- Engine/Source/Graphics/Managers/RenderTargetTextures.h and Engine/Source/Graphics/Managers/RenderTargetTextures.cpp current and combined history/presentation foam targets and corresponding create/destroy functions; Engine/Source/Graphics/Managers/TextureManager.cpp recreation integration; existing drain path preserved.
- Engine/Source/Graphics/Managers/PipelineManager.h pipeline enum; Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.cpp descriptors and pipeline creation; Engine/Source/Graphics/Managers/CommandBufferRecordMain.h and Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp pass declaration/order, image copy/mips/barriers; Engine/Source/Graphics/Managers/InstanceManager.cpp narrowly required R16 capability validation.
- Engine/Source/Ui/WaterWrappersBase.h and Engine/Source/Ui/WaterWrappersBase.cpp and Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenWater.cpp source strength/threshold/lifetime controls and existing altitude fade maxima only.
- Existing profile instrumentation around the pass using Engine/Source/Profile/ProfileManager.h and Engine/Source/Profile/ProfileRows.h if a named new timer requires their parallel rows; do not introduce a profiling subsystem.
- Conditional membership updates only in Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj and BrokenEngineSandbox.vcxproj.filters, or BrokenEngineSandboxServer.vcxproj and BrokenEngineSandboxServer.vcxproj.filters in that same directory, if /update-vcxproj identifies a required shader entry. Generated shader IDs/pack use the existing /compile pipeline.
- Owning shader/render/manager AGENTS updates only where the new history contract requires them; evidence artifacts from the shared comparison matrix.
- `Documents/Investigations/Graphics/WaterFoamPrototypeBResults.md` result report and normal Plan-completion bookkeeping; experimental source stays on its preserved prototype branch/worktree.

## Out of scope

Changing geometric wave generation or normal W meaning; source resolution independent of existing active wave grid; FFT/fluid solver; fluid-current advection; crest-speed transport; physical breaking claims; shoreline/wake emission; foam normals/bubbles; distance fade; simulation/CRC/protocol/save/replay formats; Agent command/schema expansion without a returned scope decision; production defaults/winner; new unit tests.

## Invariants and risk

Tier3 runtime because shared uniform data layout changes (`Engine/Data/Shaders/AGENTS.md:7-13`); persistent GPU resource
visibility/lifetime must also hold across frames and recreation (`Engine/Source/Graphics/Objects/AGENTS.md:5-9`). Render
is client-only/outside CRC (`Engine/Source/Graphics/Render/AGENTS.md:3-5`). Keep immutable recorded Global/Main buffers
(`Engine/Source/Graphics/Managers/AGENTS.md:9`), Global-before-Main publication, quality source bypass, and precision
owner. Document authorship only is Tier1.

## Acceptance and verification

| Criterion | Evidence required |
|---|---|
| Persistent new behavior | Controlled nonzero emission creates bounded F; after r=0 history persists and halves over4s; absent in unedited tree which has no water history (RenderTargetTextures.h:79-83) |
| True elapsed fade | 30/60/120fps cohorts and render gap at source off; tolerance2% relative away from zero after accounting for R16/filter/readback; no frame-count decay |
| Camera/cell/LOD mapping | Shared reprojection cohort preserves same-world overlap, no edge streak, new region zero, intentional multi-cell reset |
| Resource correctness | RenderDoc or Vulkan validation checks full writes, first use, mip barriers, repeated-frame layouts, recreate/resize/quality/toggle; no reads of uninitialized/stale wave or history data |
| Honest high-altitude/filter outcome | Default fade and extended preset cohorts; native detail/coverage resolution sweep records coverage and temporal aliasing rather than asserting it solved |
| Build and matched cost | /compile client Debug/Release x64 and server Release x64, Local shader/data pack; matched no-foam captures before the first edit; repeated GPU cohorts with confidence caveats |
| Preserved runnable experiment | Dedicated B branch/commit/worktree identity, executable build and captures recorded in the result report; no automatic production replacement or experimental source landing on primary |

## Result and completion

Create `Documents/Investigations/Graphics/WaterFoamPrototypeBResults.md` recording the actual common comparison baseline, prototype branch/commit/worktree identity, exact build commands/results, executable and asset locations, preset manifests, captures, repeated GPU measurements, native-grid/filter limitations, and unresolved outcomes. Preserve the runnable prototype branch and worktree. A is independently executable; compare against its report when available, without requiring A to complete B.

Plan-completion landing includes only the result report and Plan/dependency bookkeeping. Keep experimental runtime source and its contract documentation on the preserved prototype branch/worktree; neither candidate automatically lands on primary as a winner. Future production selection requires separate approval.

All runtime criteria use /agent-harness; graphics inspection/captures use its supported RenderDoc route. No runtime checks have been performed for this design. Review exact scope through plan alternatives/simplicity/audit and Tier3 grill before implementation; then affected-code/docs propagation and required code/shader/comment/style/coherence/adversarial review plus independent acceptance verification. Quality/cost outcomes remain experiment evidence, not pass claims of production suitability.
