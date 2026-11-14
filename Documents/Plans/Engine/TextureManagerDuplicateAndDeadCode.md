<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:19:37.540Z","dependsOn":[]} -->
# Remove duplicate and dead code in TextureManager

## Context

A path without a directory is under `Engine/Source/Graphics/Managers/`. Lines
are those at commit `401e4b75`; searches covered the whole repository.

- Adoption completion is written twice: `TextureManager.cpp:542-552` (the
  same-queue fallback in `ProcessPendingTextures`) and `:575-585`
  (`AdoptUploadedChunk`) run the same five steps in the same order:
  `FreeLazyChunkBuffer`, `UpdateDescriptorsForTexture`, the `kReady` store
  with `memory_order_release`, `miPendingAdoptions.fetch_sub`, then
  `BlurLightingTexture` plus `kBlurRecorded` when the CRC is a registered
  lighting texture. `AGENTS.md` `### TextureUploadManager` requires adoption
  completion to stay synchronized with the pending-adoption counter, and
  `Engine/Source/File/AGENTS.md` states the free-before-`kReady` order; two
  copies now hold both rules. The elevation adoption at `:489-494` is a
  third, shorter path and is not part of the duplicate.
- `DetailTextureSize` (`TextureManager.cpp:24-38`) and
  `WaterDetailTextureSize` (`:54-68`) are identical except `FullDetail()`
  against `WaterFullDetail()`, including the 128 by 64 minimum and the device
  clamp.
- `DetailTextureAspectRatio` (`TextureManager.h:29`,
  `TextureManager.cpp:70-74`) has no caller: the name occurs only at those
  two places.
- Three sites combine the acquire flag, the command buffer vector, and the
  index to find the pending acquire command buffer:
  `CommandBufferManager.cpp:78-80`, `:201-211`, and
  `TextureManager.cpp:650-661`, over the members at
  `TextureManager.h:216-217`.
- `TextureCache::miPhysicallyBasedRenderingCubeMipmapCount`
  (`TextureCache.h:33`) is a stored copy of
  `mTextureMap.at(kPrefilteredCrc).mInfo.iMipLevels`, written once by
  `TextureManager.cpp:228` and read once by
  `Engine/Source/Graphics/Render/LightingUniforms.cpp:332`.
  `TextureCache.cpp` never names it, it has no comment, and the copy is
  valid only after `InitializeBootTextures` while its source is valid from
  the constructor (`TextureManager.cpp:178`).
  `.agents/references/cpp-conventions.md` (derived members) requires a
  stored copy to carry a comment stating its hot-path reason.

Originating gap: the architecture phase (cohesion lens) of an analysis of
`Engine/Source/Graphics/Managers` at that commit. Unmet criteria: one site
owns the free, descriptor update, `kReady` store, counter decrement, and
first blur; the detail minimum and clamp are written once; no unused sizing
entry point; the three consumers obtain the acquire command buffer from one
`TextureManager` function; the prefiltered cubemap mip count has one owner.

## Design

Recommended:

- One private `TextureManager` function taking the CRC and the `LazyChunk&`
  holds the five completion steps; both sites call it.
- One file-local function taking the base extent holds the body of the two
  detail-size functions.
- Delete `DetailTextureAspectRatio`.
- One `TextureManager` accessor returns the pending acquire command buffer
  or `VK_NULL_HANDLE`; the three sites call it at the points where they read
  the members today. The reads stay where they are because of the
  `mSubmitGlobal` wake edge (`CommandBufferManager.cpp:71-75`).
- Delete the mip-count member and its store, and read
  `mTextureMap.at(kPrefilteredCrc).mInfo.iMipLevels` at
  `LightingUniforms.cpp:332`. Rationale: no stored copy and no
  "valid only after boot textures" window, at one map lookup per rendered
  frame. The alternative, one commented copy on `TextureManager`, also meets
  the criterion but keeps a stored copy, so the author does not recommend
  it.

## Critical files

- `Engine/Source/Graphics/Managers/TextureManager.cpp`
- `Engine/Source/Graphics/Managers/TextureManager.h`
- `Engine/Source/Graphics/Managers/TextureCache.h`
- `Engine/Source/Graphics/Managers/CommandBufferManager.cpp`
- `Engine/Source/Graphics/Render/LightingUniforms.cpp`

## In scope

- The two adoption-completion blocks and one new private member function.
- `DetailTextureSize`, `WaterDetailTextureSize`, and one new file-local
  function.
- `DetailTextureAspectRatio`.
- The three acquire command buffer lookups and one new accessor.
- `miPhysicallyBasedRenderingCubeMipmapCount`, its store, and its reader.

## Out of scope

- Shadow sizing (`TextureManager.cpp:46-50`,
  `RenderTargetTextures.cpp:112-116`): the width cap is a documented variant
  (`Engine/Source/Graphics/AGENTS.md` `## World and Camera Contracts`).
- The elevation adoption path (`:489-494`).
- A separate function that clears the acquire publication, and making the
  acquire vector and index private: the clear in
  `Graphics::UpdateFrameResidency` is the documented owner
  (`Engine/Source/Graphics/AGENTS.md` `## Frame and Resource Lifecycle`).
- Any file split of `TextureManager.cpp`.

## Risk tier

Tier 2 — scoped behavior. Trigger: public declarations change in headers
`Engine.h` aggregates (a static removed, a member removed, an accessor
added), which Tier 1's "no public signature" excludes. Adoption order, the
counter, and the points where the acquire buffer is read are unchanged, so
there is no threading change; no determinism, layout, or wire exposure.

## Acceptance criteria

1. The five criteria in `## Context` hold in the source.
2. The BrokenEngineSandbox client and ModelViewer build through `/compile`.
3. Through `/agent-harness`, a client run with validation layers on adopts
   lazy textures (the world leaves its white placeholders), renders lit
   models, and shows no validation error; the pending-adoption count returns
   to zero. The supplied evidence records no harness setup.

## Coordination

None of these Plans depends on another. Whichever lands later re-derives its
lines by symbol from the landed files:

- `Documents/Plans/Engine/ReduceTextureManagerCpp.md` may move these
  functions to another file. The author recommends landing this Plan first,
  so the reduction does not carry the duplicate and dead code along.
- `Documents/Plans/Engine/TextureUploadManagerCleanup.md` edits the
  frame-signal site at `TextureManager.cpp:640-643`, just above the acquire
  lookup at `:650-661`.
- `Documents/Plans/Engine/RemoveDeadSubmitWorkerState.md` edits
  `CommandBufferManager.cpp:59` and `:113`, near the acquire lookup at
  `:78-80`.

## Notes

- The executing session confirms no private-game use of
  `DetailTextureAspectRatio` or the mip-count member where that source is
  available, and reports it as unconfirmed otherwise.
