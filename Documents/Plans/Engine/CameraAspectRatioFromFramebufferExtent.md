<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:52:31.647Z","dependsOn":[]} -->
# Camera Aspect Ratio Must Not Read a Torn-Down Swapchain Manager

## Context

Found by a deep analysis of `Engine/Source/Graphics` (direct files only) and
verified independently by reading. Pre-existing at commit `401e4b75`; line
numbers are from that commit, so locate by symbol if they moved. `G/`
abbreviates `Engine/Source/Graphics/`.

`CameraBase::CalculateMatricesAndVisibleArea` reads
`gpSwapchainManager->mfAspectRatio` (`G/CameraBase.cpp:513`). One supported
state leaves `gpSwapchainManager` null across a frame:

- `Graphics::Create` runs `Destroy()` (`G/Graphics.cpp:372`); at the swapchain
  tier that resets `mpSwapchainManager` (`:806`), and `common::Singleton`'s
  destructor nulls the global (`Common/Singleton.h:26-29`).
- The post-Destroy zero-area re-check then returns with
  `mbSwapchainRecreateDeferred` set and no swapchain manager
  (`G/Graphics.cpp:379-391`). It is taken only when the tier is swapchain, the
  pre-Destroy gate (`:348-360`) saw a nonzero extent, and the re-query after
  `Destroy` sees zero. The comment at `:374-378` states this case (minimize
  during `Destroy`'s device wait) is supported and that the manager is absent.
- On the next loop iteration `Engine/Source/Main.cpp:477` calls
  `GameBase::Render`, which runs `UpdateRenderInterpolation`
  (`Engine/Source/GameBase.cpp:978`) -> `UpdateCamera` (`:872-874`, needs
  non-empty active coordinates and a renderable camera) ->
  `CameraBase::Update` (`G/CameraBase.cpp:226`) -> line 513, all before
  `HandleDeferredSwapchain` (`GameBase.cpp:980`), the only skip. No early
  return in `CameraBase::Update` precedes it. Every later minimized frame
  repeats it, because the retry defers at the pre-Destroy gate with the
  manager still absent.

Observed behavior from the code: a null dereference on the main thread instead
of the documented skip-and-retry (`G/AGENTS.md`
`## Frame and Resource Lifecycle`: a zero-area swapchain extent defers
swapchain-tier recreation while the simulation keeps ticking). Surface loss
and device loss bypass both gates and do not leave a null manager for a later
frame. ModelViewer is not exposed: it checks the deferral and returns
(`Projects/ModelViewer/Source/ModelViewerApplication.cpp:882-891`) before
`pCamera->Update()` (`:926`). The tail acquire is already guarded against this
state (`G/Graphics.cpp:302-309`); the camera read is not.

Not reproduced, and reachability rests on one unsettled outside fact. On
Win32, `currentExtent` may be 0 x 0 for a minimized window (verified). Whether
it can change from nonzero to zero between two calls made on the window's
owning thread with no message pump between them is unresolved; the tree
asserts it only in the comment at `G/Graphics.cpp:374-378`, which already
treats minimize-during-Destroy as supported.

Still open (the investigation portion): the readers that run before the skip
in `GameBase::Render` other than the camera — `game::FrameInterpolate::Update`
at `GameBase.cpp:837` and the client tick — were not checked against the
post-Destroy state, where the command-buffer and pipeline managers are also
null (`G/Graphics.cpp:762-770`).

Unmet criterion: a post-Destroy deferral reaches the skip in
`GameBase::Render` without dereferencing a torn-down manager.

## Design

1. Investigate first, through `/external-diagnose-bug`: confirm the root
   cause by checking every graphics-manager dereference that runs between the
   top of `GameBase::Render` (and the client tick) and
   `HandleDeferredSwapchain` in the post-Destroy state, where
   `gpSwapchainManager`, `gpCommandBufferManager`, and `gpPipelineManager` are
   null. Record each reader found and whether it dereferences one of them.
2. Recommended fix for the camera, the smallest that removes the
   manager-lifetime dependency: compute the aspect ratio at
   `G/CameraBase.cpp:513` from `gpGraphics->mFramebufferVkExtent2D`, the same
   quotient `G/Managers/SwapchainManager.cpp:274` computes over the same
   member, so the settled value is unchanged. The function already reads that
   member (`:556`, `:616`). At the deferral it holds the last nonzero extent:
   it is initialised from the wanted extent (`G/Graphics.h:125`), copied only
   when both dimensions are nonzero (`G/Graphics.cpp:584-589`), and otherwise
   written only by `CreateSwapchain`. `gpGraphics` and `gpBufferManager` stay
   alive at the swapchain tier, so the rest of the function completes.
3. A reader step 1 finds outside `CameraBase.cpp:513` is surfaced for
   re-planning, not fixed here. The alternative of running the
   deferred-swapchain check before the camera update in `GameBase::Render`
   was not evaluated and belongs to that re-planning if other readers exist.

## Critical files

- `Engine/Source/Graphics/CameraBase.cpp`
- `Engine/Source/GameBase.cpp` (read only, for the investigation)

## In scope

- The root-cause investigation of step 1: readers ahead of
  `HandleDeferredSwapchain` in `GameBase::Render` and the client tick.
- The aspect-ratio read in `CameraBase::CalculateMatricesAndVisibleArea`
  (`CameraBase.cpp:513`), which bounds the fix.

## Out of scope

- `SwapchainManager::mfAspectRatio` and its other readers.
- `Graphics::Create`, `Graphics::Destroy`, and both deferral gates.
- The order of calls in `GameBase::Render`.
- A fix for any other reader the investigation finds.
- The surface-tier zero-extent case `G/Graphics.cpp:346-347` documents as
  unhandled.

## Risk tier

Tier 2 — scoped behavior. Trigger: one subsystem's runtime behavior (the
client camera during a deferred swapchain recreate). Camera matrices are
client render state outside the CRC: no determinism, serialization, wire,
replay, or threading exposure.

## Acceptance criteria

- `CameraBase::Update` and the functions it calls dereference no
  `gpSwapchainManager` member.
- With a swapchain present and settled (no recreate pending), the aspect
  ratio the camera uses equals `SwapchainManager::mfAspectRatio`.
- The investigation result of step 1 is reported: each reader checked and
  whether it is safe in the post-Destroy state.
- The recorded symptom no longer reproduces: by code inspection, the path from
  `GameBase::Render` to `HandleDeferredSwapchain` through the camera update
  dereferences no torn-down manager. The symptom was never reproduced live and
  the supplied evidence records no harness setup; the state needs a minimize
  landing during a swapchain-tier `Destroy`.
- The BrokenEngineSandbox client and ModelViewer build.

## Coordination

No Plan below depends on this one, and it depends on none. Whichever lands
second re-derives its regions by symbol from the landed files.

- `Documents/Plans/Engine/DestroyPendingOldSwapchainAtSurfaceTier.md` fixes
  another defect of the same post-Destroy deferral state and needs the same
  minimize-during-Destroy setup to observe.
- `Documents/Plans/Engine/ReuseComputedInverseAndEyeNormal.md` edits other
  lines of `CameraBase::CalculateMatricesAndVisibleArea`.

## Notes

- The private game's source was not available to the authoring session; its
  `game::FrameInterpolate::Update` was not read. The executing session checks
  it where that source is available and reports it as unconfirmed otherwise.
