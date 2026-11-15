<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:52:46.754Z","dependsOn":[]} -->
# Order the Islands Buffer Write After Its Framebuffer's Fence Wait

## Context

Found by a deep analysis of `Engine/Source/Graphics` (direct files only) and
verified independently. Pre-existing at commit `401e4b75`; line numbers are
from that commit, so locate by symbol if they moved. `G/` abbreviates
`Engine/Source/Graphics/`.

`Islands::UpdateActiveIslands` writes the per-framebuffer islands buffers
before anything in the repository proves the previous GPU read of that
instance has finished.

- The write comes first. `GameBase::Render` calls `UpdateActiveIslands()`
  (`Engine/Source/GameBase.cpp:990`), then `RenderUiPresentAcquire` (`:1008`)
  or `RenderGlobal` (`:1013`). `UpdateActiveIslands` reads
  `gpSwapchainManager->miFramebufferIndex` (`G/Islands.cpp:199`) and writes
  that instance's indirect records (`:207-215`, `:290-296`) and storage buffer
  (`:311-327`, `:374`). `Graphics::RenderGlobal` reads the same index
  (`G/Graphics.cpp:211`) and only then calls `WaitFramebuffer` (`:213`);
  `RenderUiPresentAcquire` waits at `:244`. ModelViewer has the same order
  (`Projects/ModelViewer/Source/ModelViewerApplication.cpp:929`, then `:938`).
- What the fence covers. Framebuffer i's `mVkFence` is signaled by the ImGui
  submission (`G/Managers/ImGuiManager.cpp:786`), which waits Main's finished
  semaphore, and Main waits Global's. The readers of instance i are in Global
  (shadow elevation and terrain elevation prepasses,
  `CommandBufferRecordGlobal.cpp:76, 108`) and Main (terrain indirect draws,
  `CommandBufferRecordMain.cpp:489`). So fence i signaled implies every
  prior-frame reader of instance i finished.
- No other host-side ordering precedes the write. Between the tail
  `AcquireNextImage` (`G/Graphics.cpp:308`) and `Islands.cpp:199` the main
  thread waits only the present worker (`Graphics.cpp:295`, a CPU join). The
  acquire's own fence is waited only at the start of the next
  `AcquireNextImage` (`SwapchainManager.cpp:546-567`).
- The contract comments claim an ordering the tree does not enforce:
  `G/Islands.h:20-23` ("its prior frame has presented and completed its GPU
  read") and `G/Islands.cpp:194-198` ("Re-acquiring this image index implies
  the prior frame that used it has presented, so its GPU read of this instance
  has finished").
- The tree does not rely on that implication elsewhere. `WaitFramebuffer`
  handles `VK_NOT_READY` on the re-acquired index with a timed wait
  (`G/Graphics.cpp:122-127`), and other per-framebuffer host writes order
  themselves after the fence wait (`BufferManager.cpp:445, 526, 555`).

Outside facts, as verified against the Vulkan specification:
`vkAcquireNextImageKHR` returning index i does not order against the prior
present's wait semaphores (verified), and the acquire semaphore or fence
signal covers acquisition only — the proposition that it implies the prior
read finished was refuted. So the comments' premise is false and waiting the
acquire fence would not be a sufficient correction. Whether an unsynchronized
host write to memory a pending command buffer reads is undefined behavior or
an unspecified value read is unresolved; it sets severity only.

Reachability: the read can still be pending whenever the GPU trails the CPU
by the submissions between two uses of index i (two frames at the preferred
three images, `SwapchainManager.cpp:277`; one where the surface caps the count
at two, `:278-281`), under GPU-bound load. The outcome is a pending draw
reading a torn indirect record or placement run. Not reproduced.

Unmet criterion: no host write to a per-framebuffer buffer instance while a
submission that reads it can be pending.

## Design

Two orderings satisfy the criterion; the plan review at claim time confirms
one.

- Recommended: order framebuffer i's fence wait before
  `Islands::UpdateActiveIslands` at both hosts, so the frame waits once, ahead
  of the write, and `RenderGlobal` and `RenderUiPresentAcquire` no longer wait
  the same framebuffer a second time. Rationale: it keeps
  `UpdateActiveIslands` ahead of the residency step, which reads the reference
  counts and last-used stamps it writes and states an ordering against
  eviction and restoration (`G/Islands.cpp:203-206`, `:266-268`). Its cost,
  which the executing session checks: `WaitFramebuffer` is private
  (`G/Graphics.h:167`), and moving the wait moves `kCpuTimerWaitFence`, the
  rendered-interval sample (`G/Graphics.cpp:138-146`), and the two
  `MarkFramebufferWaited` calls (`:133-136`) earlier in the frame, ahead of
  the islands write.
- Alternative: move the write after `UpdateFrameResidency`. It leaves the
  fence wait where it is but changes the residency ordering contract above,
  so it needs that contract re-derived.

Either way, correct the two contract comments to state the fence wait as the
guarantee, and keep these orderings: the fence wait precedes
`UpdateFrameResidency` and `ProfileManager::GpuRead` for the same framebuffer,
and the deferred-swapchain skip in `GameBase::Render` still returns before any
of it.

## Critical files

- `Engine/Source/GameBase.cpp`
- `Projects/ModelViewer/Source/ModelViewerApplication.cpp`
- `Engine/Source/Graphics/Graphics.cpp`
- `Engine/Source/Graphics/Graphics.h`
- `Engine/Source/Graphics/Islands.cpp`
- `Engine/Source/Graphics/Islands.h`

## In scope

- The call order of the framebuffer fence wait and
  `Islands::UpdateActiveIslands` in `GameBase::Render` and in ModelViewer's
  render function (`ModelViewerApplication.cpp:929-938`).
- `Graphics::RenderGlobal`, `Graphics::RenderUiPresentAcquire`, and the
  declaration of `Graphics::WaitFramebuffer`, as far as the chosen ordering
  requires.
- The contract comments at `Islands.h:20-23` and `Islands.cpp:194-198`.
- Documentation sync the changed order requires.

## Out of scope

- The body of `Graphics::WaitFramebuffer` and `Graphics::UpdateFrameResidency`.
- What `UpdateActiveIslands` writes, and its emission loops.
- `SwapchainManager::AcquireNextImage` and the acquire fence.
- Other per-framebuffer host writes, which already follow the fence wait.
- Swapchain image count and present mode.

## Risk tier

Tier 3 — invariant/integration. Triggers: it changes the ordering between a
host thread's writes and pending GPU submissions (the threading surface), and
it spans the engine frame loop (`GameBase`), `Graphics`, and ModelViewer.
Client render only: no determinism/CRC, serialization, wire, or replay
exposure.

## Acceptance criteria

- At both hosts, on world and world-hidden frames, framebuffer i's fence wait
  completes before `UpdateActiveIslands` first writes instance i, and each
  frame waits that fence once.
- The fence wait still precedes `UpdateFrameResidency` and `GpuRead` for the
  same framebuffer.
- The comments at `Islands.h:20-23` and `Islands.cpp:194-198` name the fence
  wait, not re-acquisition, as what orders the write.
- The BrokenEngineSandbox client and ModelViewer build.
- Through `/agent-harness`, an island scene renders as before on a world
  frame and after a world-hidden frame, the profile output still reports the
  fence wait timer, and a Debug run with the Vulkan validation layers reports
  no new message. The supplied evidence records no harness setup, and the
  torn read itself was never observed.

## Coordination

None of the Plans below depends on this one, and it depends on none.
Whichever lands second re-derives its regions by symbol from the landed
files.

- `Documents/Plans/Engine/MergeIslandsEmissionLoops.md` edits the emission
  loops of `Islands::UpdateActiveIslands`; this Plan edits only the comment
  above its first statement.
- `Documents/Plans/Engine/ZeroUnwrittenGpuTimerRows.md` relies on `GpuRead`
  running after `Graphics::WaitFramebuffer` on the same framebuffer and cites
  those calls by line (`Graphics.cpp:213-216`, `:244-247`); this Plan keeps
  that order and may move the lines.
- `Documents/Plans/Engine/ReduceGameBaseCpp.md` may move `GameBase::Render`
  out of `GameBase.cpp`.

## Notes

- `ModelViewerApplication.cpp` is also edited by
  `Documents/Plans/Game/EngineModelRenderHelpers.md` and
  `Documents/Plans/Engine/ConsolidatedSmallDeduplications.md`, in
  other functions.
