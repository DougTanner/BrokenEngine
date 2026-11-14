<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:19:55.721Z","dependsOn":[]} -->
# Clean up TextureUploadManager staging, barrier, and signal code

## Context

A path without a directory is under `Engine/Source/Graphics/Managers/`. Lines
are those at commit `401e4b75`.

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

Originating gap: the architecture phase (cohesion lens, the staging buffer)
and the in-function phase (the rest) of an analysis of
`Engine/Source/Graphics/Managers` at that commit. Unmet criteria: the
transfer staging buffer is a `StagingBuffer` and no member stores its size;
`SubmitChunkUpload` holds only the two barrier variants the upload thread
can run; the staging copy region is written once; no code outside
`TextureUploadManager` touches `mFrameSignal`.

## Design

Recommended:

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

## Critical files

- `Engine/Source/Graphics/Managers/TextureUploadManager.cpp`
- `Engine/Source/Graphics/Managers/TextureUploadManager.h`
- `Engine/Source/Graphics/Managers/TextureManager.cpp`

## In scope

- The staging members, their creation (`:54-57`) and destruction
  (`:94-100`), and the six uses at `:487`, `:493`, `:497`, `:507`, `:539`,
  `:549`.
- `SubmitChunkUpload` `:562-597`.
- The loop body of `RecordStagingCopies` (`:487-555`).
- `TextureManager.cpp:640-643` and the access level of `mFrameSignal`.
- The entry loop in `DestroyTransferResources` (`:103-112`).

## Out of scope

- `WaitIdle` (`:158-189`) and the drain-then-release pairs at `:71-72` and
  `:183-184`: at those two teardown sites the call is not a frame signal,
  and `SignalFrame`'s comment does not hold for them.
- `SelectQueueFamilies` and the queue-alias rule.
- The upload budget, the chunk order, and the copy-region rule described at
  `:521-527`.

## Risk tier

Tier 2 — scoped behavior. Trigger: member declarations change in a header
`Engine.h` aggregates. The semaphore operations at the `TextureManager.cpp`
site are the same two calls through `SignalFrame`, the staging buffer is
created and destroyed at the same two points with the upload thread not
running or joined, and the live barrier branches are unchanged, so there is
no threading change; no determinism, layout, or wire exposure.

## Acceptance criteria

1. The four criteria in `## Context` hold in the source.
2. The BrokenEngineSandbox client and ModelViewer build through `/compile`.
3. Through `/agent-harness`, a client run with validation layers on uploads
   and adopts lazy textures on a device with a distinct transfer queue, and
   survives one full graphics recreation, with no validation error. The
   partial-mip branch of `RecordStagingCopies` runs only for a texture whose
   mip exceeds `kiByteBudgetPerFrame`; the session reports whether its run
   reached that branch. The supplied evidence records no harness setup.

## Coordination

None of these Plans depends on another. Whichever lands later re-derives its
lines by symbol from the landed files:

- `Documents/Plans/Engine/PostUploadDrainPermitUnderLock.md` rewrites
  `WaitIdle` `:175-184`, which this Plan leaves alone. The author recommends
  landing that Plan first: it fixes a race and this Plan is clean-up.
- `Documents/Plans/Engine/ReduceTextureManagerCpp.md` may move the
  `TextureManager.cpp` site to another file.
- `Documents/Plans/Engine/TextureManagerDuplicateAndDeadCode.md` edits the
  acquire lookup at `TextureManager.cpp:650-661`, just below that site.
