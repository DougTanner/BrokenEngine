<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:19:21.521Z","dependsOn":[]} -->
# Clean up render-target texture creation

## Context

A path without a directory is under `Engine/Source/Graphics/Managers/`;
`Objects/` is `Engine/Source/Graphics/Objects/`. Lines are those at commit
`401e4b75`.

- Repeated `TextureInfo` initializers in `RenderTargetTextures.cpp`:
  `WindTwo` (`:314-331`) equals `windTextureInfo` (`:294-311`) except
  `.name`; `ShadowBlur` (`:152-166`), `ShadowBlurIntermediate` (`:167-181`),
  and `ShadowHistory` (`:182-196`) are identical except `.name`, and
  `Shadow` (`:137-151`) differs from them in `.name` and `.eTextureLayout`;
  `WaterDisplacementNormal` (`:57-71`) equals `WaterDisplacement`
  (`:42-56`) except `.name`; `ObjectShadowsBlurIntermediate` (`:414-428`)
  equals `ObjectShadowsBlur` (`:399-413`) except `.name`. The same file
  already reuses one `TextureInfo` for such pairs (`CreateWaterFoamTextures`
  `:77-96`, `CreateSmokeTextures` `:242-270`), and the comment at `:260-261`
  says reuse is what keeps a pair compatible.
- Dangling names: in `RenderTargetTexturesLighting.cpp`, `spreadName`
  (`:196`) and `spreadOnlyName` (`:212`) are `std::string` locals of the
  inner loop body. `TextureInfo::name` is a `std::string_view`, and
  `Texture::Create` stores the info (`mInfo = rInfo` in `CreateImage`). The
  strings die at `:228`, so `mInfo.name` of every spread and spread-only
  texture dangles. The file's own comment at `:11` and
  `RenderTargetTextures.cpp:20` state the rule, and the combine, history,
  and smoke names follow it. Nothing reads the dangling name today: every
  repository read of a texture's name for these textures happens inside
  `Create`, while the string is alive. This is a latent hazard, not observed
  misbehavior.
- Three identical zero clears: `RenderTargetTextures.cpp:277-289`,
  `:343-351`, and `RenderTargetTexturesLighting.cpp:384-396` each declare a
  zero `VkClearColorValue` and the same single-mip subresource range, then
  loop transition to transfer destination, `vkCmdClearColorImage`,
  transition back. `Texture` has no clear member (`Objects/Texture.h:99-111`).
- `RenderTargetTexturesLighting.cpp:67` checks at run time a plane count
  that is a compile-time value: `FlightPlanes::mPlanes` is bound once to
  `game::kFlightPlanes`, a `std::array` in both programs. A game table with
  3 planes passes the `static_assert` at `RenderTargetTextures.cpp:18` and
  fails only at boot. `RenderTargetTextures.cpp:240` asserts a bound that
  `:15`, `:18`, and `Engine/Source/Frame/FlightPlanes.cpp:13` already
  enforce.

Originating gap: the architecture phase (cohesion lens, the zero clears)
and the in-function phase (the rest) of an analysis of
`Engine/Source/Graphics/Managers` at that commit. Unmet criteria: each pair
or triple of identical targets is created from one `TextureInfo`; every
`TextureInfo::name` a texture keeps outlives the texture; the three image
clears call one `Texture` member.

## Design

Recommended:

- Create each identical pair or triple from one `TextureInfo`, reassigning
  `.name` (and `.eTextureLayout` for `Shadow`) between `Create` calls, as
  the file already does elsewhere. No helper.
- Give the spread and spread-only names static storage at file scope,
  assigned before `Create`, as the file's other names have. This allocates
  no more than the `std::format` calls already do.
- Add one `Texture` member that takes only the command buffer and records
  the transition, zero clear, and transition back; the three sites call it.
  The one-shot command buffer each site uses stays where it is.
- Turn `RenderTargetTexturesLighting.cpp:67` into a namespace-scope
  `static_assert` on `game::kFlightPlanes`, with the
  `Frame/FlightPlaneTable.h` include, and delete
  `RenderTargetTextures.cpp:240`. These two edits are cosmetic alone and are
  included only because this Plan already edits both files; the
  `static_assert` conversion is the part with value. Naming
  `game::kFlightPlanes` in engine code is allowed
  (`Engine/Source/AGENTS.md` `## Hub Conventions`), and
  `RenderTargetTextures.cpp:18` already does it.

## Critical files

- `Engine/Source/Graphics/Managers/RenderTargetTextures.cpp`
- `Engine/Source/Graphics/Managers/RenderTargetTexturesLighting.cpp`
- `Engine/Source/Graphics/Objects/Texture.h`
- `Engine/Source/Graphics/Objects/Texture.cpp`

## In scope

- Four functions in `RenderTargetTextures.cpp` (`:37-72`, `:137-196`,
  `:292-331`, `:398-428`): the repeated initializers.
- `RenderTargetTexturesLighting.cpp:188-228` and file-scope name storage.
- One new `Texture` member and the three clear sites.
- The two plane-count ASSERTs.

## Out of scope

- `ShadowElevation` and `ObjectShadows`: their infos differ.
- A shared occupancy zero-fill function for smoke and wind: the two copies
  differ in array shape, size member, and enclosing one-shot, and wind
  occupancy being zeroed with the wind textures is a recorded design
  (`AGENTS.md` `### BufferManager`).
- Shadow texture sizing in `CreateShadowTextures`.
- The smoke gradient formula (`RenderTargetTextures.cpp:221-235`).
- Formats, extents, usages, and creation order of every target.

## Risk tier

Tier 2 — scoped behavior. Trigger: it adds a public member to `Texture`, a
header `Engine.h` aggregates, which Tier 1's "no public signature" excludes.
The created images and recorded clears are the same; no threading,
determinism, layout, or wire exposure.

## Acceptance criteria

1. The three criteria in `## Context` hold in the source, and a plane count
   the lighting layers cannot serve fails the build.
2. The BrokenEngineSandbox client and ModelViewer build through `/compile`.
3. Through `/agent-harness`, a client run with validation layers on boots,
   renders a world frame with lighting, shadows, wind, smoke, and water, and
   survives one swapchain recreation with no validation error. The supplied
   evidence records no harness setup.

## Coordination

None of these Plans depends on another. Whichever lands later re-derives its
lines by symbol from the landed files:

- `Documents/Plans/Engine/ClampSmokeGradientDistance.md` edits the gradient
  loop at `RenderTargetTextures.cpp:231`, a few lines above the plane-count
  ASSERT at `:240`.
- `Documents/Plans/Engine/ShareWholeBufferBarrierBuilder.md` rewrites the
  wind occupancy barrier at `RenderTargetTextures.cpp:356-367`, next to the
  clear at `:343-351`.

## Notes

- Whether `vkSetDebugUtilsObjectNameEXT` and the tools behind it copy the
  name they are given was asked and not settled; the dangling-name item is
  recorded as latent on repository evidence only.
