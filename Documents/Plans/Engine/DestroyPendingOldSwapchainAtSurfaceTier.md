<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:52:39.290Z","dependsOn":[]} -->
# Destroy a Pending Old Swapchain Before Its Device

## Context

Found by a deep analysis of `Engine/Source/Graphics` (direct files only) and
verified independently by reading. Pre-existing at commit `401e4b75`; line
numbers are from that commit, so locate by symbol if they moved. `G/`
abbreviates `Engine/Source/Graphics/`.

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
   (`Engine/Source/GameBase.cpp:903`): the same `Graphics` object continues to
   `:406` and passes the stale handle, created on the destroyed device, to the
   new `SwapchainManager` as `oldSwapchain`.

Paths 1 and 2 leak a swapchain at device destruction; path 3 also uses a
handle from a destroyed device.

Outside facts, as verified against the Vulkan specification: `vkDestroyDevice`
and `vkDestroySurfaceKHR` require the swapchains destroyed first (verified);
`VkSwapchainCreateInfoKHR::oldSwapchain` must be a valid, non-retired handle
(verified; whether the text also says "same device" is unresolved, and does
not change the defect); `vkDestroySwapchainKHR` after `vkDeviceWaitIdle` is
valid (verified).

Caveats recorded by the verification: every path first needs the narrow
minimize race during one `Destroy` (window extent nonzero at the pre-Destroy
gate, zero at the re-check), and path 1 leaks at process exit only;
`.agents/references/cpp-conventions.md` discourages protection against
ultra-rare events. Against that, the code supports this deferral state on
purpose (comment at `G/Graphics.cpp:374-378`). Not reproduced at runtime; all
three paths were traced by reading only.

Unmet criterion: every swapchain handle `Graphics` holds is destroyed before
its device.

## Design

Recommended: in the `kSurface` branch of `Graphics::Destroy`, before
`mpDeviceManager.reset()`, destroy a non-null `mOldVkSwapchainKHR` with
`vkDestroySwapchainKHR` and set it to `VK_NULL_HANDLE`, mirroring
`SwapchainManager.cpp:515-518`. The branch runs after the device drain at
`G/Graphics.cpp:749`. This closes all three paths at one site: on path 3 the
member is null again before `:406`.

## Critical files

- `Engine/Source/Graphics/Graphics.cpp`

## In scope

- The `kSurface` branch of `Graphics::Destroy`, before
  `mpDeviceManager.reset()`.

## Out of scope

- Both zero-area deferral gates in `Graphics::Create` and the handover at
  `:406-407`.
- `SwapchainManager`, including `ReleaseHandleForRecreation` and its
  destructor.
- Any other teardown ordering in `Graphics::Destroy`.

## Risk tier

Tier 2 — scoped behavior. Trigger: one subsystem's runtime behavior (graphics
teardown ordering on the main thread). Client only: no determinism/CRC,
serialization, wire, replay, or threading exposure.

## Acceptance criteria

- By code inspection, no path resets `mpDeviceManager` while
  `mOldVkSwapchainKHR` is non-null, and `Graphics.cpp:406` cannot pass a
  handle created on a destroyed device.
- The BrokenEngineSandbox client and ModelViewer build, and an ordinary
  launch, resize, and quit of the client in a Debug build with the Vulkan
  validation layers reports no new validation message.
- Where the deferral can be forced (minimize during a swapchain-tier
  recreate, then quit), the validation layers report no device destroyed with
  a live swapchain. The supplied evidence records no harness setup that forces
  it; report it as not observed if it cannot be forced.

## Coordination

No Plan below depends on this one, and it depends on none. Whichever lands
second re-derives its regions by symbol from the landed files.

- `Documents/Plans/Engine/CameraAspectRatioFromFramebufferExtent.md` fixes
  another defect of the same post-Destroy deferral state and needs the same
  minimize-during-Destroy setup to observe.

## Notes

- None.
