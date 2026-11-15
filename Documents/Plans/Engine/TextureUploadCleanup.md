<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:19:55.721Z","dependsOn":["Documents/Plans/Engine/PostUploadDrainPermitUnderLock.md"]} -->
# Clean up TextureUploadManager and TextureManager duplicate and dead code

## Context

A path without a directory is under `Engine/Source/Graphics/Managers/`. Lines
are those at commit `401e4b75`; searches covered the whole repository.

TextureUploadManager:

- The transfer staging buffer re-implements `StagingBuffer`.
  `TextureUploadManager.cpp:54-57` makes the same `Buffer::CreateBuffer` call
  as `StagingBuffer::StagingBuffer`
  (`Engine/Source/Graphics/Objects/Buffer.cpp:105-108`), and `:94-100`
  repeats `~StagingBuffer` (`Buffer.cpp:110-113`). `miStagingSize` has two
  writers (`:56` sets `kiByteBudgetPerFrame`, `:99` sets 0) and two readers
  (`:487`, `:493`), both on the upload thread, which is joined (`:73-76`)
  before the write at `:99`: every read sees the constant.
  `.agents/references/cpp-conventions.md` (derived members) applies.
- `SubmitChunkUpload` (`:562-597`) holds an unreachable barrier variant. Its
  only caller (`:346`) runs only after `HandleUploadEarlyOut` returned false
  (`:268`), that is with the transfer and graphics queue handles different
  (`:408`). `mTransferVkQueue` is written only at `DeviceManager.cpp:289`
  (equal families: aliases the graphics queue) and `:294` (its own queue),
  so equal families never reach `:562`: `bSeparateTransferFamily` is always
  true there and the `else` branch at `:587-596` cannot run. The four
  assignments at `:568-571` and `:578-581` are identical.
- `RecordStagingCopies` writes the staging copy twice (`:497-507` and
  `:539-549`): the same `memcpy`, the same seven `VkBufferImageCopy` field
  writes, and the same `vkCmdCopyBufferToImage`, differing only in the
  height and the byte count.
- `TextureManager.cpp:642-643` spells out the body of
  `TextureUploadManager::SignalFrame` (`TextureUploadManager.cpp:195-196`)
  on `gpTextureUploadManager->mFrameSignal`. That site is the only reason
  `mFrameSignal` is public (`TextureUploadManager.h:48`).
- `DestroyTransferResources` binds each map entry `const` (`:103`) and looks
  the same entry up again with `.at(rCrc)` to get it mutable (`:107`).

TextureManager:

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

Originating gap: the architecture phase (cohesion lens) and the in-function
phase of an analysis of `Engine/Source/Graphics/Managers` at that commit.
Unmet criteria:

1. The transfer staging buffer is a `StagingBuffer` and no member stores its
   size.
2. `SubmitChunkUpload` holds only the two barrier variants the upload thread
   can run.
3. The staging copy region is written once.
4. No code outside `TextureUploadManager` touches `mFrameSignal`.
5. One site owns the free, descriptor update, `kReady` store, counter
   decrement, and first blur.
6. The detail minimum and clamp are written once.
7. No unused sizing entry point.
8. The three consumers obtain the acquire command buffer from one
   `TextureManager` function.
9. The prefiltered cubemap mip count has one owner.

## Design

Recommended:

TextureUploadManager:

- Replace the four staging members (`TextureUploadManager.h:101-104`) with
  one `std::optional<StagingBuffer>` created and reset at the same two
  points, and use `kiByteBudgetPerFrame` at `:487` and `:493`. `Engine.h`
  includes `Graphics/Objects/Buffer.h` before `TextureUploadManager.h`, so
  no new include is needed.
- Delete `bSeparateTransferFamily` and its `else` branch, and write the
  shared assignments once. The two `VK_QUEUE_FAMILY_IGNORED` stores at
  `:572-573` may stay.
- In `RecordStagingCopies`, compute the height and byte count in each branch
  and write the copy once; cursor updates stay in their branches.
- `TextureManager.cpp:642-643` calls `SignalFrame()`, and `mFrameSignal`
  becomes private.
- In `DestroyTransferResources`, bind the entry mutable and drop the second
  lookup. This edit is cosmetic alone and is included only because the
  staging change already edits that function.

TextureManager:

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

`Documents/Plans/Engine/PostUploadDrainPermitUnderLock.md` lands first and
rewrites `WaitIdle` in `TextureUploadManager.cpp`; re-derive this Plan's
`TextureUploadManager.cpp` lines by symbol from the landed file.

## Critical files

- `Engine/Source/Graphics/Managers/TextureUploadManager.cpp`
- `Engine/Source/Graphics/Managers/TextureUploadManager.h`
- `Engine/Source/Graphics/Managers/TextureManager.cpp`
- `Engine/Source/Graphics/Managers/TextureManager.h`
- `Engine/Source/Graphics/Managers/TextureCache.h`
- `Engine/Source/Graphics/Managers/CommandBufferManager.cpp`
- `Engine/Source/Graphics/Render/LightingUniforms.cpp`

## In scope

`TextureUploadManager.cpp` / `.h`:

- The staging members, their creation (`:54-57`) and destruction
  (`:94-100`), and the six uses at `:487`, `:493`, `:497`, `:507`, `:539`,
  `:549`.
- `SubmitChunkUpload` `:562-597`.
- The loop body of `RecordStagingCopies` (`:487-555`).
- The access level of `mFrameSignal`.
- The entry loop in `DestroyTransferResources` (`:103-112`).

`TextureManager.cpp` / `.h`, `TextureCache.h`, `CommandBufferManager.cpp`,
`LightingUniforms.cpp`:

- The frame-signal site `TextureManager.cpp:640-643`.
- The two adoption-completion blocks and one new private member function.
- `DetailTextureSize`, `WaterDetailTextureSize`, and one new file-local
  function.
- `DetailTextureAspectRatio`.
- The three acquire command buffer lookups and one new accessor.
- `miPhysicallyBasedRenderingCubeMipmapCount`, its store, and its reader.

## Out of scope

- `WaitIdle` (`:158-189`, whose `:175-184`
  `Documents/Plans/Engine/PostUploadDrainPermitUnderLock.md` rewrites) and the
  drain-then-release pairs at `:71-72` and `:183-184`. At those two teardown
  sites the call is not a frame signal, and `SignalFrame`'s comment does not
  hold for them.
- `SelectQueueFamilies` and the queue-alias rule.
- The upload budget, the chunk order, and the copy-region rule described at
  `TextureUploadManager.cpp:521-527`.
- Shadow sizing (`TextureManager.cpp:46-50`,
  `RenderTargetTextures.cpp:112-116`): the width cap is a documented variant
  (`Engine/Source/Graphics/AGENTS.md` `## World and Camera Contracts`).
- The elevation adoption path (`TextureManager.cpp:489-494`).
- A separate function that clears the acquire publication, and making the
  acquire vector and index private: the clear in
  `Graphics::UpdateFrameResidency` is the documented owner
  (`Engine/Source/Graphics/AGENTS.md` `## Frame and Resource Lifecycle`).
- Any file split of `TextureManager.cpp`.

## Risk tier

Tier 2 — scoped behavior. Triggers: member declarations change in headers
`Engine.h` aggregates (a static removed, a member removed, an accessor added,
`mFrameSignal` made private), which Tier 1's "no public signature" excludes.

Invariants: the semaphore operations at the `TextureManager.cpp` frame-signal
site are the same two calls through `SignalFrame`; the staging buffer is
created and destroyed at the same two points with the upload thread not
running or joined; the live barrier branches are unchanged; adoption order,
the pending-adoption counter, and the points where the acquire buffer is read
are unchanged. There is no threading change and no determinism, layout, or
wire exposure.

## Acceptance criteria

1. The nine criteria in `## Context` hold in the source.
2. The BrokenEngineSandbox client and ModelViewer build through `/compile`.
3. Through `/agent-harness`, one client run with validation layers on, on a
   device with a distinct transfer queue, uploads and adopts lazy textures
   (the world leaves its white placeholders), renders lit models, returns the
   pending-adoption count to zero, and survives one full graphics recreation,
   with no validation error. The partial-mip branch of `RecordStagingCopies`
   runs only for a texture whose mip exceeds `kiByteBudgetPerFrame`; the
   session reports whether its run reached that branch. The supplied evidence
   records no harness setup.

## Coordination

Whichever lands later re-derives its lines by symbol from the landed files:

- `Documents/Plans/Engine/RemoveCommandBufferExecutedFlag.md` edits
  `CommandBufferManager.cpp:59` and `:113`, near the acquire lookup at
  `:78-80`.

## Notes

- The executing session confirms no private-game use of
  `DetailTextureAspectRatio` or the mip-count member where that source is
  available, and reports it as unconfirmed otherwise.
