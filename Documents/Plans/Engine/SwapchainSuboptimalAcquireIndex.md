<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:39:43.792Z","dependsOn":[]} -->
# Fix: Publish the framebuffer selected by a suboptimal acquire

## Context

The accepted survivor `CAI/shard-0029/003` shows that
`SwapchainManager::AcquireNextImage` publishes `miFramebufferIndex` only for
`VK_SUCCESS`.  Its `VK_SUBOPTIMAL_KHR` branch discards the returned index,
clears the acquire fence, and only raises the swapchain destroy tier
(`Engine/Source/Graphics/Managers/SwapchainManager.cpp:553-586`).  The initial
Graphics constructor acquires before the boot render loop
(`Engine/Source/Graphics/Graphics.cpp:122-126`; `Engine/Source/Main.cpp:316-323`),
which has no later deferred-swapchain skip guard.

The source report is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0029.md:129`;
the consolidated selector is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:833`.
The frozen manager rows matched baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`, and the ordinary tail path was
traced separately.  No source changes were made in this routing session.

Impact: a supported WSI result can make the first frame submit/present a
different swapchain image than the one associated with its acquire semaphore,
causing validation errors, wrong presentation, or device failure.

## Design

Author's recommendation: treat `VK_SUBOPTIMAL_KHR` as a usable acquire result
for the current frame.  Assign `miFramebufferIndex` from the returned
`uiFramebufferIndex`, retain the current acquire fence/semaphore pairing until
the next acquisition waits it, and still escalate `meDestroyType` to the
swapchain tier so the next frame recreates the surface.  Keep
`VK_ERROR_OUT_OF_DATE_KHR` as the no-current-image path and retain its fence
clear.

## Critical files

- `Engine/Source/Graphics/Managers/SwapchainManager.cpp:553-586` — acquire
  result handling.
- `Engine/Source/Graphics/Managers/SwapchainManager.h:40-55` — framebuffer index
  and acquire semaphore/fence ownership.
- `Engine/Source/Graphics/Graphics.cpp:110-127,280-290` — constructor and
  normal acquire sequencing.
- `Engine/Source/Main.cpp:316-323` and `Engine/Source/GameBase.cpp:805-827` —
  boot versus deferred render behavior.

## In scope

- Publishing the returned image index and preserving its acquire fence for
  `VK_SUBOPTIMAL_KHR`.
- Scheduling the existing deferred swapchain recreation after the current
  usable frame.
- Keeping the out-of-date/error branches, acquire-ring rotation, and present
  result handling unchanged.

## Out of scope

- Swapchain render-pass formats, image-count policy, present-mode selection,
  device-loss handling, or a new boot retry loop.
- Graphics manager teardown, semaphore allocation, or Vulkan validation-layer
  suppression.
- Simulation CRC, wire/save/replay formats, and unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Trigger: WSI image
identity and semaphore publication cross SwapchainManager, Graphics boot,
command submission, and present.

Tier rationale: the change is confined to one `VK_SUBOPTIMAL_KHR` branch of a
single function, and the Design fully specifies it — publish the returned
index, keep the fence, still escalate the destroy tier. The out-of-date and
error branches, the acquire ring, and every serialized format are untouched.

Preserve these invariants:

- Every usable acquire result, including suboptimal, publishes the image index
  returned by the WSI and retains the matching semaphore/fence.
- A pending recreation never causes a submission to use a stale framebuffer
  index or an unsignaled acquire semaphore.
- Out-of-date and fatal results still skip the current frame and follow their
  existing recovery paths.
- No simulation CRC, wire, save, replay, or `.pack` bytes change.

## Acceptance criteria

- A forced or validation-layer-observed startup `VK_SUBOPTIMAL_KHR` with a
  nonzero returned index renders/presents that same index for the current frame,
  then recreates on the next frame without a semaphore/index mismatch.
- A normal successful acquire and an out-of-date acquire retain their existing
  behavior.
- Client Debug and Release builds pass `/compile`; a harness resize/fullscreen
  startup scenario has no stale-index or present-ownership error.
- The acquire fence remains waitable exactly once on the following acquire.

## Notes

The audit catalog identifies this as `CAI-EXT-007`; the Vulkan
`vkAcquireNextImageKHR` suboptimal return contract is an external API rule to
preserve during implementation.
