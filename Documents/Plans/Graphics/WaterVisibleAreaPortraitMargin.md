<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-12T02:20:29.683Z","dependsOn":[]} -->
# Give the water mesh its off-screen margin at every aspect ratio

## Context

`CameraBase::CalculateMatricesAndVisibleArea`
(`Engine/Source/Graphics/CameraBase.cpp:91-104`) expands the render visible area
by `gVisibleAreaExtraTop` / `gVisibleAreaExtraBottom`
(`Engine/Source/Ui/WrapperBase.cpp:12-13`, defaults `0.046` and `0.16`) only when
`gpGraphics->mFramebufferExtent2D.width > height`. The portrait and square branch
assigns `f4LargeVisibleArea` unchanged, so the terrain and water mesh gets no
margin outside the frustum.

The water mesh is built over that visible area and its vertices are then moved by
the pre-computed Gerstner displacement — `Engine/Data/Shaders/Water/Water.vert:60-63`
adds `f3Displacement.xy` from `WaterDisplacement.comp` (the horizontal steepness
terms at `:104` and `:132`). Boundary vertices can move inward, so the mesh no
longer covers the frame edge.

Nothing repaints what the mesh vacates. The HDR colour attachment uses
`VK_ATTACHMENT_LOAD_OP_DONT_CARE` whenever `kbFramebufferClearColor` is false
(`Engine/Source/Graphics/Managers/SwapchainManager.cpp:41-45` and `:71`), and
`kbFramebufferClearColor` is `kbVulkanWireframe`, false by default
(`Projects/BrokenEngineSandbox/Source/Pch.h:20`). `HdrResolve.frag:40` samples
`hdrSampler` unconditionally for every pixel, so an uncovered edge pixel resolves
undefined attachment contents. The landscape branch's margin is what hides this
in the common case; the artifact is latent and was re-exposed, not introduced, by
restoring the `kbFramebufferClearColor` default.

## Design

Apply the existing margin expansion at every aspect ratio: drop the
width-versus-height branch in `CalculateMatricesAndVisibleArea` and run the four
existing expansion statements unconditionally, keeping `fVisibleHeight` and the
same `gVisibleAreaExtraTop` / `gVisibleAreaExtraBottom` factors so landscape
output is bit-identical to today. Scaling both axes by the visible height is
deliberate: it is what the landscape branch already does, and it keeps one
margin constant per edge rather than introducing an aspect-dependent second set
of tuning values.

Do not re-enable the unconditional colour clear: `kbFramebufferClearColor` stays
tied to `kbVulkanWireframe`, because a full-frame clear costs bandwidth every
frame to hide a coverage bug.

## Critical files

- `Engine/Source/Graphics/CameraBase.cpp` — the aspect-ratio branch at `:91-104`
  inside `CalculateMatricesAndVisibleArea`

## In scope

- The `width > height` branch and its `else` in
  `CalculateMatricesAndVisibleArea`, and the comment that explains why the margin
  exists

## Out of scope

- `kbFramebufferClearColor`, the swapchain attachment `loadOp` values, and
  `HdrResolve.frag`
- `gVisibleAreaExtraTop` / `gVisibleAreaExtraBottom` values and their UI wrappers
- The Gerstner displacement shaders and the water mesh construction
- The visible-area snap, LOD, and hysteresis logic that follows the branch
- Any change to `GlobalLayout` or another CPU/GPU layout

## Coordination

`Documents/Plans/Graphics/CameraBaseDeadCodeRemoval.md` edits the same function's
surroundings in `Engine/Source/Graphics/CameraBase.cpp` (its `f4RawAreaIn` line
sits just below this branch). Neither Plan may change the other's lines, and
whichever lands second re-verifies its own line references before editing.

## Risk tier and invariants

Expected Change Workflow Tier 2 — one subsystem's client render behavior.
`CameraBase.cpp` is client-only render state outside deterministic PostRender and
the CRC, so no determinism, replay, wire, or serialization surface is exposed.
Invariant: landscape framebuffers must keep producing the exact same
`f4RenderVisibleArea` as before.

## Acceptance criteria

- With a portrait or square framebuffer, the water mesh covers the full frame
  with waves at maximum amplitude and `kbFramebufferClearColor` false — no
  undefined or garbage pixels at any frame edge
- With a landscape framebuffer, the rendered visible area is unchanged
- Client compiles (the file is client-only)
