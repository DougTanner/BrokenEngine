<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T12:56:48.472Z","dependsOn":[]} -->
# Remove dormant TextureInfo flag modes

## Context

The false required condition is that `TextureInfo` must expose multisampling,
depth, and host-visible texture-flag modes even though no current producer sets
them. `TextureFlags` declares `kMultisampling`, `kDepth`, and `kHostVisible`
(`Engine/Source/Graphics/Objects/Texture.h:11-18`), but exact producer searches
find only `{}` or `kRenderPass` in `TextureInfo` initializers. Their branches
remain in `Texture::Create`, `CreateRenderTarget`, destruction, and the member
render-pass helper (`Engine/Source/Graphics/Objects/Texture.cpp:171-227,296-420,422-445,497-512`).
The candidate's live-path evidence is the exact producer inventory and source
selectors above.

The user explicitly directs removal of these unproduced modes and only the
branches proven dead by exact repository searches. Preserve live texture
upload, readback, render-pass, multisample attachment, and Vulkan ownership
paths. The concern is pre-existing at session baseline
`80896f33661aaab99cf180a96db54600099be652`.

## Design

The author's recommendation is to retain `TextureFlags::kRenderPass` and the
full live `TextureInfo` image/format/layout fields, but remove only
`TextureFlags::kMultisampling`, `kDepth`, and `kHostVisible`. Simplify
`Texture::Create` to optimal tiling and the existing render-pass dedicated
allocation decision; remove the dead host-visible VMA branch. Make
`CreateRenderTarget` color-only and remove its dead per-texture depth image,
depth attachment, and `mpDepthTexture` ownership. Make the member
`RecordBeginRenderPass` derive only its live clear flag.

Keep the separate live `RenderPassFlags::kDepth` and
`RenderPassFlags::kMultisampling` enum values and static
`Texture::RecordBeginRenderPass` overload: `CommandBufferRecordMain` uses them
for the HDR render pass. Keep `TextureInfo::samples`, swapchain depth/MSAA
textures, `RecordCopyImageFrom`, `TextureCache::CopyImageToHostMemory`,
readback staging buffers, and every current upload/transition path. Do not
refactor Texture broadly or remove Buffer host-visible support.

## Critical files

- `Engine/Source/Graphics/Objects/Texture.h:8-18,42-139` — flag declarations, `TextureInfo`, and depth ownership.
- `Engine/Source/Graphics/Objects/Texture.cpp:23-44,56-75,80-115,171-235,296-445,455-512` — live mapping/create/render/copy paths and dead branches.
- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp:324-335` — live HDR depth/multisample render-pass flags.
- `Engine/Source/Graphics/Managers/SwapchainManager.cpp:353-400` — live swapchain depth and multisample `TextureInfo` consumers.
- `Engine/Source/Graphics/Managers/TextureCache.cpp:10-36,123-184,250-275` and `Engine/Source/Graphics/Screenshot.cpp:95-106,532-572` — live readback/upload paths.
- `Engine/Source/Graphics/Objects/AGENTS.md:12-21` and `Engine/Source/Graphics/AGENTS.md:7-14` — Vulkan ownership and render lifecycle authorities.

## In scope

- Removing the three unproduced `TextureFlags` modes and only their exact
  `TextureInfo`-driven host-visible, depth, and multisample branches.
- Removing the dead per-texture depth image/member and making the current
  `CreateRenderTarget`/member begin path match its only color-render-target
  producers.
- Preserving static `RenderPassFlags` depth/multisample handling, image sample
  counts, swapchain attachments, texture upload/readback, transitions,
  descriptor ownership, and Vulkan destruction ordering.
- Updating comments and exact references so the remaining Texture contract is
  explicit without broad Texture refactoring.

## Out of scope

- The separate `RenderPassFlags` enum, HDR render-pass flags, swapchain depth
  or MSAA images, `TextureInfo::samples`, Buffer host-visible modes, or Vulkan
  capability/settings behavior.
- Removing live `kRenderPass`, image layouts, mip/array handling, upload,
  readback, copy, descriptor, render-target, or ownership paths.
- Adding a new Texture mode, compatibility path, abstraction, or unit test;
  unrelated graphics plans and shader assets are excluded.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped Graphics behavior). The exact evidence
for not triggering a Tier 3 surface is the producer inventory: every current
`TextureInfo` initializer uses `{}` or `kRenderPass`, with no producer of
`TextureFlags::kMultisampling`, `kDepth`, or `kHostVisible`; the live
`CommandBufferRecordMain.cpp:324-335` depth/multisample use is the separate
`RenderPassFlags` API, and `SwapchainManager.cpp:353-400` uses
`TextureInfo::samples` rather than the removed flags. The edits stay inside
the client Graphics `Texture` wrapper and its local callers: no `.pack` or
serialization/data layout, deterministic CRC, wire/protocol, save/replay,
threading, trust-boundary, shader source, project membership, or
build/bootstrap surface changes.

Preserve these invariants:

- Every current `TextureInfo` producer still creates the same image format,
  sample count, usage, layout, and render-target attachment behavior.
- Static render-pass recording still handles live depth/multisample flags and
  clear-value counts for HDR/swapchain passes.
- Upload, transition, copy/readback, descriptor generation, and destruction
  remain valid and ordered for all current textures.
- No simulation CRC, wire/save/replay, or asset-pack contract changes.

## Acceptance criteria

- Exact repository searches prove no producer sets `TextureFlags::kMultisampling`,
  `kDepth`, or `kHostVisible`; the only remaining multisample/depth flag uses
  are the intentionally preserved `RenderPassFlags` and live swapchain paths.
- Client compilation and Vulkan static checks show all current render-target,
  upload, readback, copy, transition, and destruction paths remain reachable;
  no dead `mpDepthTexture` or TextureInfo mode reference remains.
- An existing client harness render plus screenshot/readback scenario completes
  without Vulkan validation errors, and HDR depth/MSAA attachments remain
  paired as before.
- Client Debug and Release builds pass `/compile`; no unit tests are added.

## Notes

Future workflow routing: the implementation receives `/repo-code-review` and
`/code-style-review` for `Texture.h`/`Texture.cpp`, `/update-claude-docs` after
the C++ change, and client Debug/Release `/compile` through the builder. The
Tier 2 acceptance scenario is the existing client render plus
Screenshot/TextureCache readback path. No `/glsl-review`, `/update-vcxproj`,
server build is triggered because the approved change does not touch GLSL,
project membership, shared serialization, or a correctness-dependent caller
outside this wrapper. The mandatory `/update-affected-code` propagation pass
still runs after the future C++ change, as required by the root Implement and
propagate step, and records
that no affected caller was found if the exact search remains empty.
