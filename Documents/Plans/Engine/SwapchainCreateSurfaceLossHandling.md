<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-15T23:45:46.277Z","dependsOn":[]} -->
# Handle Surface Loss During Swapchain Creation

## Context

`CHECK_VK` (`Engine/Source/Graphics/GraphicsUtils.h`) calls `CheckVkFailed`
(`Engine/Source/Graphics/GraphicsUtils.cpp`) for any non-success result.
`CheckVkFailed` returns without throwing for `VK_ERROR_OUT_OF_DATE_KHR` and
`VK_SUBOPTIMAL_KHR` (escalating `gpGraphics->meDestroyType` to at least
`DestroyType::kSwapchain`) and for `VK_ERROR_SURFACE_LOST_KHR` (setting it to
`DestroyType::kSurface`). Execution therefore continues past the failed call.

That non-throwing path is deliberate for the acquire and present paths
(`SwapchainManager::AcquireNextImage`, `SwapchainManager::PresentToQueue`) and
for `Graphics::SurfaceExtentZeroArea`, which re-checks `meDestroyType` after its
capabilities query. It is not handled in `SwapchainManager::CreateSwapchain`
(`Engine/Source/Graphics/Managers/SwapchainManager.cpp`), where
`vkGetPhysicalDeviceSurfaceCapabilitiesKHR`,
`vkGetPhysicalDeviceSurfacePresentModesKHR`, and `vkCreateSwapchainKHR` all
list `VK_ERROR_SURFACE_LOST_KHR` among their documented results. Per the Vulkan
1.2 specification (Fundamentals, Return Codes), the output parameters of a
command that returns a runtime error are undefined. On surface loss:

- the capabilities and present-mode outputs are undefined, yet
  `CreateSwapchain` reads them for the extent, image count, transform,
  composite alpha, and present-mode selection;
- the `vkCreateSwapchainKHR` output handle is undefined, yet `CreateSwapchain`
  assigns it to `mVkSwapchainKHR` and names it with `VkName`;
- `SwapchainManager::CreateFramebuffers` then passes that handle to
  `vkGetSwapchainImagesKHR`, and `SwapchainManager::DestroyVulkanObjects`
  passes it to `vkDestroySwapchainKHR` when it is not `VK_NULL_HANDLE`.

Recovery itself already exists: with `meDestroyType` at `kSurface`,
`GameBase::HandleDeferredSwapchain` skips rendering and calls
`Graphics::Create`, which tears down at the surface tier and rebuilds. The
defect is the undefined values the swapchain construction consumes before that
recovery runs. The behavior is pre-existing: at baseline `1f9b41ad`
`CreateSwapchain` passed `&mVkSwapchainKHR` straight to `vkCreateSwapchainKHR`
inside `CHECK_VK`; the later change to create into a local and assign after
`CHECK_VK` does not cover the non-throwing return.

The intended handling of surface loss during creation (throw, skip the
remaining swapchain construction and let the pending `kSurface` recreation
run, or something else) is not yet decided; this Plan carries an
investigation portion for it.

## Design

1. Investigate first, by `/external-diagnose-bug`: trace what
   `SwapchainManager` construction and the rest of `Graphics::Create` do with
   the undefined values after a surface-loss result in `CreateSwapchain`, both
   on first construction (`Graphics::Graphics`, whose catch tears down at the
   surface tier) and on a recreation from `GameBase::HandleDeferredSwapchain`.
   Confirm which consumers read an undefined output before the pending
   `kSurface` recreation runs.
2. Decide the handling and record it in the session's plan. The author
   recommends keeping surface loss recoverable, because the acquire, present,
   and zero-extent paths already treat it as a deferred surface-tier
   recreation: in `CreateSwapchain`, inspect the surface-query and
   `vkCreateSwapchainKHR` results so that on `VK_ERROR_SURFACE_LOST_KHR`
   `mVkSwapchainKHR` stays `VK_NULL_HANDLE` and no undefined output is read,
   and have the swapchain-image-dependent construction skip its work while
   `meDestroyType` is `kSurface`, leaving the existing recreation to rebuild.
   Throwing instead would turn a recoverable surface loss on recreation into a
   fatal error, which the author considers worse.
3. Make the smallest fix inside the `## In scope` region. A fix needing changes
   outside it (for example to `CheckVkFailed`'s escalation contract or to
   `Graphics::Create`'s ordering) is surfaced for re-planning instead of
   widening scope.

Risk trigger: Tier 2 — scoped runtime behavior of one subsystem (the client
Vulkan renderer's swapchain recreation path). No determinism/CRC, wire,
serialization, `.pack`/`kiVersion`, replay, threading, or trust-boundary
exposure; client-only (`BT_CLIENT`).

## Critical files

- `Engine/Source/Graphics/Managers/SwapchainManager.cpp`
- `Engine/Source/Graphics/Managers/SwapchainManager.h`
- `Engine/Source/Graphics/GraphicsUtils.cpp` (read only, unless re-planned)
- `Engine/Source/Graphics/Graphics.cpp` (read only, unless re-planned)
- `Engine/Source/Graphics/AGENTS.md`, `Engine/Source/Graphics/Managers/AGENTS.md`
  (documentation of the surface-loss contract, if the handling changes it)

## In scope

- Root-cause investigation of the surface-loss path through
  `SwapchainManager::CreateSwapchain` and its downstream consumers, per
  `## Design` step 1.
- `SwapchainManager::CreateSwapchain`: the handling of
  `VK_ERROR_SURFACE_LOST_KHR` from `vkGetPhysicalDeviceSurfaceCapabilitiesKHR`,
  `vkGetPhysicalDeviceSurfacePresentModesKHR`, and `vkCreateSwapchainKHR`, and
  the assignment of `mVkSwapchainKHR`.
- The `SwapchainManager` construction steps that consume the swapchain handle
  or the surface capabilities (`CreateFramebuffers` and the constructor
  sequence that calls it), only as needed so none reads an undefined output.
- The Graphics and Managers `AGENTS.md` lines describing surface-loss handling,
  only if the handling they describe changes.

## Out of scope

- Changing `CheckVkFailed`'s or `CHECK_VK`'s escalation behavior for any result.
- The acquire and present paths (`AcquireNextImage`, `PresentToQueue`) and
  `Graphics::SurfaceExtentZeroArea`, which already handle the non-throwing
  return.
- The `InstanceManager` surface queries (`vkGetPhysicalDeviceSurfaceSupportKHR`,
  `vkGetPhysicalDeviceSurfaceFormatsKHR`) and any other `CHECK_VK` site outside
  `SwapchainManager`; if the investigation shows one of them reachable with
  surface loss, surface it as a separate follow-up.
- Zero-area extent handling and device-loss recovery.

## Acceptance criteria

- On `VK_ERROR_SURFACE_LOST_KHR` from any of the three in-scope calls, no
  output of that call is read, `mVkSwapchainKHR` holds no undefined handle, and
  neither `vkGetSwapchainImagesKHR`, `VkName`, nor `vkDestroySwapchainKHR`
  receives one; shown by code read of the final diff.
- The recorded symptom no longer reproduces: tracing a surface-loss result from
  `CreateSwapchain` through the next `GameBase::HandleDeferredSwapchain`
  reaches a surface-tier `Graphics::Create` rebuild without touching an
  undefined value. The supplied evidence records no live reproduction setup;
  surface loss is not injectable through the harness today, so verification is
  by code read.
- Client and server build cleanly.

## Notes

- `vkGetSwapchainImagesKHR` does not list `VK_ERROR_SURFACE_LOST_KHR` among its
  results; it is affected only through the undefined handle it receives.
