<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-15T22:07:43.255Z","dependsOn":[]} -->
# Release SwapchainManager's Vulkan handles when its constructor throws

## Context

`SM` is `Engine/Source/Graphics/Managers/SwapchainManager.cpp`. Line numbers
are those of the commit that added the constructor `ASSERT` described below
(the commit completing `SwapchainTeardownAndCameraAspect`); locate code by
symbol if they have shifted.

`SwapchainManager::SwapchainManager()` (`SM:10-28`) calls `CreateRenderPass`,
`CreateSwapchain`, `CreateFramebuffers`, and `CreateSynchronizationObjects`,
each of which writes raw Vulkan handles into members:

- `CreateRenderPass` — `mHdrVkRenderPass` (`SM:152`), `mVkRenderPass`
  (`SM:210`).
- `CreateSwapchain` — `mVkSwapchainKHR` (`SM:322`).
- `CreateFramebuffers` — `mHdrVkFramebuffer` (`SM:409`), and per swapchain
  image `Framebuffer::vkPresentImageView` (`SM:442`) and
  `Framebuffer::vkPresentFramebuffer` (`SM:461`).
- `CreateSynchronizationObjects` — `mImageAvailableFences` (`SM:478`) and
  `mImageAvailableSemaphores` (`SM:492`).

Only `SwapchainManager::~SwapchainManager()` (`SM:497-523`) releases them. A
constructor that throws runs no destructor of its own, so every handle created
before the throw leaks. The `Texture` members (`mDepthTexture`,
`mMultisamplingTexture`, `mHdrTexture`) are unaffected: `Texture::~Texture`
(`Engine/Source/Graphics/Objects/Texture.cpp:110`) releases them on unwind.

The throw is reachable: every creation call above runs under `CHECK_VK`, and
`CheckVkFailed` (`Engine/Source/Graphics/GraphicsUtils.cpp:55-59`) throws
`std::system_error` on `VK_ERROR_DEVICE_LOST`; `mHdrTexture.Create` in
`CreateFramebuffers` (`SM:378-392`) submits its layout transition, so a device
loss can surface there after the swapchain exists. The throw leaves
`Graphics::Create`'s `std::make_unique<SwapchainManager>()`
(`Engine/Source/Graphics/Graphics.cpp:415-418`) with `mpSwapchainManager`
null. Device-loss recovery (`Engine/Source/Main.cpp:483-494`,
`Projects/ModelViewer/Source/Main.cpp:331`) catches the error, resets
`Graphics`, and `Graphics::Destroy`'s `kSurface` tier resets the device and
instance (`Graphics.cpp` `Destroy`, `mpDeviceManager.reset()`) while the leaked
swapchain, render passes, framebuffers, image views, fences, and semaphores
still exist.

The completing session bounded the swapchain part of this with a stopgap: the
constructor wraps `CreateFramebuffers` and `CreateSynchronizationObjects` in
`try` / `catch (...)` whose handler runs `ASSERT(mVkSwapchainKHR ==
VK_NULL_HANDLE)` and rethrows (`SM:17-27`), making a failure after the
swapchain exists fatal instead of leaking; the `### SwapchainManager` section
of `Engine/Source/Graphics/Managers/AGENTS.md` documents that the window is
deliberately fatal and skips device-loss recovery. The user accepted the
stopgap ("If this rare edge case does repro we can fix it later") and chose
this follow-up for the root-cause fix: an RAII owner releasing the handles when
the constructor throws, which makes the `try` / `catch` and its `ASSERT`
unnecessary and restores device-loss recovery in that window. The originating
finding is the over-engineering-review design suggestion against `SM:16` that
names `SM:20-27` as made unnecessary; the leak itself was a pre-existing
`/repo-code-review` residual.

## Design

Root cause: the constructor acquires raw Vulkan handles whose only release is
the destructor. Every handle listed in `## Context` shares that cause, so this
Plan covers all of them, not only the swapchain; the render passes leak even
when `CreateSwapchain` throws, which a swapchain-only owner would leave
unfixed.

Recommended mechanism (the author's recommendation; rationale follows):

1. Move the body of `~SwapchainManager()` unchanged into a new private member
   function, `void DestroyVulkanObjects();`, declared in the `private:` section
   of `SwapchainManager.h`; `~SwapchainManager()` calls it.
2. At the start of the constructor body, after `scopedBootTimer` and before
   `CreateRenderPass()`, hold a function-scope `std::unique_ptr` owner of
   `this` whose deleter is a captureless lambda calling
   `DestroyVulkanObjects()`; after `CreateSynchronizationObjects()` returns,
   call `release()` on it so a successful construction leaves release to the
   destructor.
3. Delete the `try` / `catch (...)` block, its comment, and the `ASSERT`
   (`SM:17-27`); the four `Create*` calls run in sequence inside the owner's
   scope.
4. Delete the `### SwapchainManager` sentence in
   `Engine/Source/Graphics/Managers/AGENTS.md` that says a constructor failure
   after swapchain creation is deliberately fatal and skips device-loss
   recovery.

Rationale:

- `Documents/C++StyleGuide.txt` rule 5 requires a `std::unique_ptr` custom
  deleter to be a captureless lambda type kept to `.cpp` files and function
  scope. A `std::unique_ptr` member per handle, the example the user gave,
  would put that deleter type in `SwapchainManager.h`; the function-scope owner
  of step 2 satisfies rule 5 and keeps the user's RAII requirement.
- One owner covers every handle, and the destructor and the throw path share
  one teardown, so the two cannot drift.
- `DestroyVulkanObjects` already tolerates a partially constructed object: each
  `vkDestroy*` call it makes accepts `VK_NULL_HANDLE`, every handle member
  defaults to `VK_NULL_HANDLE`, and the vectors hold `VK_NULL_HANDLE` in slots
  not yet created (`std::vector::resize` value-initializes; `Framebuffer`
  members default to `VK_NULL_HANDLE`).
- On unwind the owner releases the raw handles first, then the `Texture`
  member destructors run, all while `gpDeviceManager` is still alive; the
  device reset happens later in `Graphics::Destroy`.

If the executing session prefers per-handle owner members instead, that
contradicts rule 5's function-scope restriction and returns to the user for a
decision rather than being chosen silently.

## Critical files

- `Engine/Source/Graphics/Managers/SwapchainManager.cpp`
- `Engine/Source/Graphics/Managers/SwapchainManager.h`
- `Engine/Source/Graphics/Managers/AGENTS.md`

## In scope

- `SwapchainManager::SwapchainManager()`: add the function-scope owner, remove
  the `try` / `catch (...)` block and its `ASSERT`.
- `SwapchainManager::~SwapchainManager()`: body moved to
  `DestroyVulkanObjects`, which it calls.
- New private `SwapchainManager::DestroyVulkanObjects()` declaration and
  definition.
- The one `### SwapchainManager` sentence in
  `Engine/Source/Graphics/Managers/AGENTS.md` about the fatal constructor
  window.

## Out of scope

- `CreateRenderPass`, `CreateSwapchain`, `CreateFramebuffers`,
  `CreateSynchronizationObjects` bodies, and the teardown order and calls
  inside the moved destructor body.
- `Texture` and its RAII, `Framebuffer`'s copy/move declarations, and the
  handle member types (they stay raw Vulkan handles).
- `Graphics::Create`, `Graphics::Destroy`, `Graphics::Graphics`'s own
  `try` / `catch`, and device-loss recovery in both `Main.cpp` files.
- Constructor-throw leaks in any other manager.

## Risk and invariants

Change Workflow Tier 2. Trigger: scoped runtime behavior of one subsystem
(Graphics `SwapchainManager` construction failure path). No determinism/CRC,
serialization, `.pack`/`kiVersion`, replay, wire, threading, allocation-policy,
shader, or affinity exposure; the file stays client-only under `BT_CLIENT`.

Invariants: a successful construction and `~SwapchainManager()` release
exactly the same handles as today; the owner never runs after a successful
construction.

## Acceptance criteria

- The constructor contains no `try` / `catch` and no `ASSERT` on
  `mVkSwapchainKHR`; a function-scope owner releases every handle listed in
  `## Context` when any `Create*` call throws, and is released after
  `CreateSynchronizationObjects()` returns.
- `~SwapchainManager()` and the throw path run the same
  `DestroyVulkanObjects()`.
- `Engine/Source/Graphics/Managers/AGENTS.md` no longer claims the window is
  fatal.
- The client builds, and an `/agent-harness` launch with a swapchain-tier
  recreation (window resize) runs without Vulkan validation errors from
  `SwapchainManager`. The throwing window itself has no recorded reproduction
  setup; the supplied evidence records none, so the throw path is settled by
  diff review against the `## Context` handle list.

## Notes

- The `Framebuffer` move-constructor comment in `SwapchainManager.h`
  ("SwapchainManager frees them") stays true and needs no edit.
