<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:52:39.290Z","dependsOn":[]} -->
# Swapchain Teardown and Camera Aspect in the Post-Destroy Deferral State

## Context

Found by a deep analysis of `Engine/Source/Graphics` (direct files only) and
verified independently by reading. Pre-existing at commit `401e4b75`; line
numbers are from that commit, so locate by symbol if they moved. `G/`
abbreviates `Engine/Source/Graphics/`.

Both defects arise from one supported state, the post-Destroy zero-area
deferral:

- `Graphics::Create` runs `Destroy()` (`G/Graphics.cpp:372`); at the swapchain
  tier that resets `mpSwapchainManager` (`:806`), and `common::Singleton`'s
  destructor nulls the global (`Common/Singleton.h:26-29`).
- The post-Destroy zero-area re-check then returns with
  `mbSwapchainRecreateDeferred` set and no swapchain manager
  (`G/Graphics.cpp:379-391`). It is taken only when the tier is swapchain, the
  pre-Destroy gate (`:348-360`) saw a nonzero extent, and the re-query after
  `Destroy` sees zero. The comment at `:374-378` states this case (minimize
  during `Destroy`'s device wait) is supported and that the manager is absent.

Reachability rests on one unsettled outside fact. On Win32, `currentExtent`
may be 0 x 0 for a minimized window (verified). Whether it can change from
nonzero to zero between two calls made on the window's owning thread with no
message pump between them is unresolved; the tree asserts it only in the
comment at `G/Graphics.cpp:374-378`, which already treats
minimize-during-Destroy as supported. `.agents/references/cpp-conventions.md`
discourages protection against ultra-rare events; against that, the code
supports this deferral state on purpose. Neither defect was reproduced at
runtime; every path below was traced by reading only.

### Defect 1: camera reads a torn-down swapchain manager

`CameraBase::CalculateMatricesAndVisibleArea` reads
`gpSwapchainManager->mfAspectRatio` (`G/CameraBase.cpp:512`). On the next loop
iteration after the deferral, `Engine/Source/Main.cpp:477` calls
`GameBase::Render`, which runs `UpdateRenderInterpolation`
(`Engine/Source/GameBase.cpp:985`) -> `UpdateCamera` (`:879-881`, needs
non-empty active coordinates and a renderable camera) -> `CameraBase::Update`
(`G/CameraBase.cpp:226`) -> line 512, all before `HandleDeferredSwapchain`
(`GameBase.cpp:987`), the only skip. No early return in `CameraBase::Update`
precedes it. Every later minimized frame repeats it, because the retry defers
at the pre-Destroy gate with the manager still absent.

Observed behavior from the code: a null dereference on the main thread instead
of the documented skip-and-retry (`G/AGENTS.md`
`## Frame and Resource Lifecycle`: a zero-area swapchain extent defers
swapchain-tier recreation while the simulation keeps ticking). Surface loss
and device loss bypass both gates and do not leave a null manager for a later
frame. ModelViewer is not exposed: it checks the deferral and returns
(`Projects/ModelViewer/Source/ModelViewerApplication.cpp:882-891`) before
`pCamera->Update()` (`:926`). The tail acquire is already guarded against this
state (`G/Graphics.cpp:302-309`); the camera read is not.

Still open (the investigation portion): the readers that run before the skip
in `GameBase::Render` other than the camera — `game::FrameInterpolate::Update`
at `GameBase.cpp:844` and the client tick — were not checked against the
post-Destroy state, where the command-buffer and pipeline managers are also
null (`G/Graphics.cpp:762-770`).

Unmet criterion: a post-Destroy deferral reaches the skip in
`GameBase::Render` without dereferencing a torn-down manager.

### Defect 2: a pending old swapchain outlives its device

`Graphics::mOldVkSwapchainKHR` (`G/Graphics.h:132`) can be left holding a
live swapchain that nothing ever destroys:

- `Graphics::Destroy` below the `kSurface` tier moves the live handle out of
  the swapchain manager into the member and resets the manager
  (`G/Graphics.cpp:802-806`); `ReleaseHandleForRecreation` nulls the manager's
  copy (`G/Managers/SwapchainManager.cpp:524-529`), so the manager's
  destructor skips its destroy (`:515-518`).
- `Graphics::Create` normally hands the member to the new manager and clears
  it (`G/Graphics.cpp:406-407`); the new manager passes it as `oldSwapchain`
  and destroys it (`SwapchainManager.cpp:313, 318-321`).
- The post-Destroy zero-area deferral returns first (`G/Graphics.cpp:379-391`),
  leaving the handle pending with `mpSwapchainManager` null.
- No other code reads or writes the member (`Graphics.h:132`,
  `Graphics.cpp:406, 407, 804` only).

Three paths then reach a `kSurface` `Destroy` that skips `:802-805` because
the manager is null and resets the device and instance (`:821-822`) with the
swapchain still alive:

1. The destructor (`:114-115`): quit while minimized after the deferral.
2. Device-loss recovery, which destroys `Graphics`
   (`Engine/Source/Main.cpp:486`, `Projects/ModelViewer/Source/Main.cpp:341`).
3. A surface loss raised by the `CHECK_VK` at `:326` during a retry
   (`Engine/Source/GameBase.cpp:910`): the same `Graphics` object continues to
   `:406` and passes the stale handle, created on the destroyed device, to the
   new `SwapchainManager` as `oldSwapchain`.

Paths 1 and 2 leak a swapchain at device destruction (path 1 at process exit
only); path 3 also uses a handle from a destroyed device.

Outside facts, as verified against the Vulkan specification: `vkDestroyDevice`
and `vkDestroySurfaceKHR` require the swapchains destroyed first (verified);
`VkSwapchainCreateInfoKHR::oldSwapchain` must be a valid, non-retired handle
(verified; whether the text also says "same device" is unresolved, and does
not change the defect); `vkDestroySwapchainKHR` after `vkDeviceWaitIdle` is
valid (verified).

Unmet criterion: every swapchain handle `Graphics` holds is destroyed before
its device.

## Design

1. Investigate first, through `/external-diagnose-bug`: confirm the root
   cause of defect 1 by checking every graphics-manager dereference that runs
   between the top of `GameBase::Render` (and the client tick) and
   `HandleDeferredSwapchain` in the post-Destroy state, where
   `gpSwapchainManager`, `gpCommandBufferManager`, and `gpPipelineManager` are
   null. Record each reader found and whether it dereferences one of them.
2. Camera fix, the smallest that removes the manager-lifetime dependency:
   compute the aspect ratio at `G/CameraBase.cpp:512` from
   `gpGraphics->mFramebufferVkExtent2D`, the same quotient
   `G/Managers/SwapchainManager.cpp:274` computes over the same member, so the
   settled value is unchanged. The function already reads that member (`:555`,
   `:613`). At the deferral it holds the last nonzero extent: it is
   initialised from the wanted extent (`G/Graphics.h:125`), copied only when
   both dimensions are nonzero (`G/Graphics.cpp:584-589`), and otherwise
   written only by `CreateSwapchain`. `gpGraphics` and `gpBufferManager` stay
   alive at the swapchain tier, so the rest of the function completes.
3. A reader step 1 finds outside `CameraBase.cpp:512` is surfaced for
   re-planning, not fixed here. The alternative of running the
   deferred-swapchain check before the camera update in `GameBase::Render`
   was not evaluated and belongs to that re-planning if other readers exist.
4. Swapchain teardown fix: in the `kSurface` branch of `Graphics::Destroy`,
   before `mpDeviceManager.reset()`, destroy a non-null `mOldVkSwapchainKHR`
   with `vkDestroySwapchainKHR` and set it to `VK_NULL_HANDLE`, mirroring
   `SwapchainManager.cpp:515-518`. The branch runs after the device drain at
   `G/Graphics.cpp:749`. This closes all three paths at one site: on path 3
   the member is null again before `:406`.

## Critical files

- `Engine/Source/Graphics/CameraBase.cpp`
- `Engine/Source/Graphics/Graphics.cpp`
- `Engine/Source/GameBase.cpp` (read only, for the investigation)

## In scope

- The root-cause investigation of step 1: readers ahead of
  `HandleDeferredSwapchain` in `GameBase::Render` and the client tick.
- The aspect-ratio read in `CameraBase::CalculateMatricesAndVisibleArea`
  (`CameraBase.cpp:512`), which bounds the camera fix.
- The `kSurface` branch of `Graphics::Destroy`, before
  `mpDeviceManager.reset()`.

## Out of scope

- `SwapchainManager::mfAspectRatio` and its other readers.
- `SwapchainManager`, including `ReleaseHandleForRecreation` and its
  destructor.
- `Graphics::Create`, both zero-area deferral gates, and the handover at
  `G/Graphics.cpp:406-407`.
- Any other part or teardown ordering of `Graphics::Destroy`.
- The order of calls in `GameBase::Render`.
- A fix for any other reader the investigation finds.
- The surface-tier zero-extent case `G/Graphics.cpp:346-347` documents as
  unhandled.

## Risk tier

Tier 2 — scoped behavior. Trigger: one subsystem's runtime behavior (the
client camera during a deferred swapchain recreate, and graphics teardown
ordering on the main thread). Client only: camera matrices are client render
state outside the CRC; no determinism/CRC, serialization, wire, replay, or
threading exposure.

## Acceptance criteria

- `CameraBase::Update` and the functions it calls dereference no
  `gpSwapchainManager` member.
- With a swapchain present and settled (no recreate pending), the aspect
  ratio the camera uses equals `SwapchainManager::mfAspectRatio`.
- The investigation result of step 1 is reported: each reader checked and
  whether it is safe in the post-Destroy state.
- The recorded camera symptom no longer reproduces: by code inspection, the
  path from `GameBase::Render` to `HandleDeferredSwapchain` through the camera
  update dereferences no torn-down manager.
- By code inspection, no path resets `mpDeviceManager` while
  `mOldVkSwapchainKHR` is non-null, and `Graphics.cpp:406` cannot pass a
  handle created on a destroyed device.
- The BrokenEngineSandbox client and ModelViewer build, and an ordinary
  launch, resize, and quit of the client in a Debug build with the Vulkan
  validation layers reports no new validation message.
- Where the deferral can be forced (minimize landing during a swapchain-tier
  `Destroy`), one run observes both fixes: the client keeps rendering frames
  through the deferral without a null dereference, and on quit the validation
  layers report no device destroyed with a live swapchain. The supplied
  evidence records no harness setup that forces it; report it as not observed
  if it cannot be forced.

## Notes

- The private game's source was not available to the authoring session; its
  `game::FrameInterpolate::Update` was not read. The executing session checks
  it where that source is available and reports it as unconfirmed otherwise.
