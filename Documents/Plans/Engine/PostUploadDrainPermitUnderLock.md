<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:21:37.419Z","dependsOn":[]} -->
# Post the WaitIdle probe permit inside the drain-request lock

## Context

A path without a directory is under `Engine/Source/Graphics/Managers/`;
`G/` is `Engine/Source/Graphics/`. Lines are those at commit `401e4b75`.

`TextureUploadManager::WaitIdle` (`TextureUploadManager.cpp:158-189`) is the
teardown drain handshake. `AGENTS.md` `### TextureUploadManager` documents
its guarantee: "the upload thread acknowledges only from a
no-submit-in-flight point". `WaitIdle` sets the drain request under
`mWorkMutex` (`:176-179`), unlocks, and only then posts its probe permit on
`mFrameSignal` (`:183-184`). The request is therefore visible before the
permit exists, and this interleaving is reachable:

1. The upload thread returns from `mFrameSignal.acquire()` (`:223`, count
   now 0) and has not yet locked `mWorkMutex` (`:236`).
2. The main thread runs `:176-179`: sets `mbDrained = false` and
   `mbDrainRequested = true`, unlocks.
3. The upload thread locks (`:236`), sees the request (`:241`), clears it,
   sets `mbDrained`, notifies, and its `try_acquire` (`:249`) finds count 0;
   it parks in `acquire()` (`:223`).
4. The main thread's `try_acquire` (`:183`) fails and `release()` (`:184`)
   posts count 1.
5. The main thread locks (`:186`); the predicate at `:187` is already true;
   `WaitIdle` returns.
6. The upload thread wakes on that permit, finds `mbDrainRequested` false
   (cleared at `:243` and again at `:188`), and runs a full iteration:
   `DequeueNextUpload`, `CreateTransferImage` (`:311`), `vkQueueSubmit` on
   `mTransferVkQueue` (`:614`), `vkWaitForFences` (`:619`).

The actors exist under supported use. The main thread reaches `WaitIdle`
from `Graphics::Destroy` (`G/Graphics.cpp:742`), called by
`Graphics::Create` (`G/Graphics.cpp:372`) at the frame tail
(`G/Graphics.cpp:300`) for any destroy tier, for example a window resize.
The permit in step 1 comes from `SignalFrame` (`G/Graphics.cpp:283`,
`Engine/Source/GameBase.cpp:985`). The window is wider than one preemption:
an iteration holds `mWorkMutex` across `vkWaitForFences` (`:274`, `:619`),
so the main thread can block at `:176` while a `SignalFrame` permit is
already pending; when the iteration ends, the upload thread consumes that
permit and both threads contend for the mutex, which is steps 1 and 2. The
harmful submit needs a non-empty upload queue and a distinct transfer queue
(`HandleUploadEarlyOut`, `:408`, otherwise ends the iteration without a
submit).

After `WaitIdle` returns, the main thread calls `vkDeviceWaitIdle`
(`G/Graphics.cpp:749`) with that submit possibly concurrent. Verified
against the Vulkan 1.2 specification: `vkDeviceWaitIdle` requires host
access to every `VkQueue` created from the device to be externally
synchronized, so a concurrent `vkQueueSubmit` on the transfer queue from
another thread is invalid usage.

Originating gap: the architecture phase (simulation and threading
invariants lens) of an analysis of `Engine/Source/Graphics/Managers` at that
commit. Unmet criterion: after `WaitIdle` returns, the upload thread
performs no iteration until the next frame signal.

## Design

Root cause, verified by the walk above: the probe permit is posted after the
drain request becomes visible.

Recommended: move the drain-then-release pair (`:183-184`) inside the
critical section that ends at `:179`, so the permit exists before the
request can be observed, and update the comment at `:181-182`. Checked
against the three upload-thread positions:

- Parked in `acquire()`: it wakes on the probe, acknowledges, and its
  `try_acquire` finds 0.
- Between `:223` and `:236`: the main thread's release makes the count 1,
  and the acknowledge-time `try_acquire` swallows it.
- Mid-iteration: the main thread blocks at `:176` until the iteration ends,
  then one of the first two cases applies.

`release()` under the mutex does not block. The change adds no state and no
lock.

## Critical files

- `Engine/Source/Graphics/Managers/TextureUploadManager.cpp`

## In scope

- `TextureUploadManager::WaitIdle` `:175-184`: the position of the
  `try_acquire` and `release` pair, and the comment above it.

## Out of scope

- `UploadThread`, `SignalFrame`, and `DestroyTransferResources`, including
  the drain-then-release pair at `:71-72`.
- The early returns at the top of `WaitIdle` and the wait at `:186-188`.
- `Graphics::Destroy` and its `vkDeviceWaitIdle` call.

## Risk tier

Tier 3 — invariant/integration. Trigger: threading. The change moves a
semaphore post relative to a mutex in a cross-thread teardown handshake
between the main thread and the upload thread. No determinism, layout, or
wire exposure.

## Acceptance criteria

1. In `WaitIdle`, the probe permit is posted while `mWorkMutex` is held, in
   the same critical section that sets `mbDrainRequested`.
2. The BrokenEngineSandbox client and ModelViewer build through `/compile`.
3. Through `/agent-harness`, a client run with validation layers on, on a
   device with a distinct transfer queue, repeats window resizes and one
   full graphics recreation while lazy textures are still uploading, with
   no hang in `WaitIdle`, no threading or other validation error, and
   uploads resuming afterwards. The supplied evidence records no harness
   setup, and the race was established by code reading, not reproduced, so
   this run shows no regression; criterion 1 and review of the three
   positions in `## Design` settle the fix.

## Coordination

`Documents/Plans/Engine/TextureUploadCleanup.md` edits other functions of
`TextureUploadManager.cpp` and leaves `WaitIdle` alone. That Plan depends on
this one and re-derives its lines by symbol.
