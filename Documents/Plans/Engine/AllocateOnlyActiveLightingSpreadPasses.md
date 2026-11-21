<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-21T21:04:10.701Z","dependsOn":["Documents/Plans/Engine/MergeRenderTargetTexturesLighting.md","Documents/Plans/Engine/PrepareLightingSpreadTaps.md","Documents/Plans/Engine/SkipEmptyLightingSpread.md"]} -->
# Allocate lighting spread resources only for active passes

## Context

`RenderTargetTexturesLighting.cpp:189-232,305-330` creates six Float16
textures and a framebuffer for every `kiMaxSpreadPasses` slot.
`WorldLightingShadowPipelines.cpp:27-58` creates the corresponding pipelines
and fills three capacity-sized combine sampler arrays. The recorder only
uses the active prefix. At 64 active passes, the remaining 64 slots cost
roughly 30 MB of raw 4K Medium texture payload. This estimate excludes
allocation overhead and measures bytes rather than GPU time.

## Design

Use the active integer pass count at lighting resource creation. Create
the accumulation and snapshot images, framebuffer and spread pipeline only
for each active slot. Retain the fixed-capacity CPU arrays when helpful
for indexing and the current full descriptor arrays in `LightCombine.comp`.
Fill each inactive R/G/B sampler slot with the live pass-0 snapshot of the
matching color. The active count is at least one, and combine cannot read
indices outside that prefix. Use the same active count for recording,
indirect writes, pipeline teardown, shader consumption, debug registration
and `Spread`/`SpreadOnly` dump validation. Keep every active image dumpable.

Use the existing pass-count change recreation route and its fence/lifetime
contract. Create and publish images, descriptors and commands from a single
new count, and only destroy old target and pipeline resources after their
commands are retired. Preserve active resolutions and history reset on
recreation, including the existing one-pass resolution. Avoid separate
incremental allocation and descriptor-indexing feature dependencies.

## Critical files

- `Engine/Source/Graphics/Managers/RenderTargetTextures.h` and the
  `RenderTargetTextures.cpp` definitions of `CreateLightingTextures`,
  `DestroyLightingTextures` and `RegisterDebugTextures`. The current split
  definitions and evidence are in `RenderTargetTexturesLighting.cpp`.
- `Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.cpp`,
  `PipelineManager.h`, `PipelineManager.cpp`, and `CommandBufferRecordMain.cpp`.
- `Engine/Source/Graphics/Render/LightingUniforms.cpp`,
  `Engine/Source/Graphics/Graphics.cpp:603-605`, `Screenshot.cpp:316-322`
  and the existing lighting debug range publication.
- `Engine/Data/Shaders/Lighting/LightCombine.comp:13-15,78-89`.

## In scope

- Lighting spread resource creation and retirement, spread pipelines, full
  combine descriptor tail, and the active-count readers for recording,
  indirect publication, debug selection and render-target dumps.

## Out of scope

- Altering format, active pass resolutions, tap math, lighting layer count,
  history or refresh policy, descriptor array size, and unit tests.

## Risk triggers and invariants

Tier 3 for Vulkan descriptor and in-flight resource lifetime changes.
Command buffers and descriptor sets stay valid across grow, shrink, swapchain
recreation and world reset. Inactive descriptors refer to a live read-only
image. No command or debug/dump selection uses an uncreated slot. The oldest
applicable frame retains its resources until safe retirement.

## Acceptance criteria

1. Compare output against the immediate predecessor at one, default and
   maximum passes. With the game rendering, switch between these counts
   repeatedly in both directions, resize, and change the resolution
   multipliers. Record validation-layer output and verify stable rendering,
   history reset/reseed, valid debug views, and rejection of inactive dump
   indices without touching absent images.
2. Inspect allocated image/framebuffer/pipeline counts and report actual
   texture allocation bytes or payload validated from dimensions and format.
   Verify the full descriptor tail is valid and the maximum-count case
   retains every pass. Record GPU Lighting Spread and Combine profiles on
   the fixed-scene comparison without assuming the memory saving lowers time.
3. `/compile` builds the BrokenEngineSandbox client and ModelViewer. Use
   `/agent-harness` for runtime churn, debug images, and target comparisons.
   No live capture sequence was supplied with the allocation count.

## Notes

Use the current lighting layer topology at execution. The light target
source merge is a separate mechanical change and owns the definition path.
