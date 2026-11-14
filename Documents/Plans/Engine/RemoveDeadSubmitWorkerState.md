<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:20:40.022Z","dependsOn":[]} -->
# Remove a dead cross-thread wait and an unread command-buffer flag

## Context

A path without a directory is under `Engine/Source/Graphics/Managers/`;
`G/` is `Engine/Source/Graphics/`. Lines are those at commit `401e4b75`.

- The `SwapchainManager::Present` worker lambda calls
  `gpCommandBufferManager->mSubmitMain.Wait()` (`SwapchainManager.cpp:643`).
  The call never waits. `Present` has one caller (`G/Graphics.cpp:280`,
  inside `PresentAcquire`), reached two ways: `RenderMainPresentAcquire`
  after `SubmitUiCommandBuffer` (`G/Graphics.cpp:237`), where the main
  thread already ran `mSubmitMain.Wait()` (`CommandBufferManager.cpp:189`),
  which clears `mbDispatched` before any rethrow
  (`Common/Threading/PersistentWorker.cpp:52-58`); and
  `RenderUiPresentAcquire` (`G/Graphics.cpp:255-257`), where nothing woke
  `mSubmitMain` this frame. The main thread does not wake `mSubmitMain`
  again before `mPresent.Wait()` (`G/Graphics.cpp:295`, or `:720` in
  `Destroy`). So `mbDispatched` is false at `:643` on every path. The call
  still reads another worker's `mbDispatched` from the present thread. The
  comment at `Common/Threading/PersistentWorker.h:28`, as the authoring
  session's later edit worded it, leaves that member unsynchronized and
  limits it to the serialized `Wake`, `Wait`, and destructor calls, so the
  read is safe only because the `mPresent` wake/wait edge orders it against
  the main thread's calls.
- `CommandBufferFlags::kExecuted` is written and never read. A
  whole-repository search finds it at four places: the declaration
  (`G/Objects/CommandBuffers.h:11`), a comment (`:27`), the clear
  (`CommandBufferManager.cpp:59`), and a set on the `mSubmitGlobal` worker
  (`:113`). The only `mFlags` test is `kRecorded`
  (`CommandBufferManager.cpp:54`). The write at `:113` is a worker-thread
  write to the byte that carries `kRecorded`.
- `SwapchainManager.cpp:566` stores `VK_NULL_HANDLE` to
  `mCurrentImageAvailableVkFence`, and `:569` overwrites it unconditionally
  with nothing reading it between.

Originating gap: the architecture phase (simulation and threading
invariants lens) of an analysis of `Engine/Source/Graphics/Managers` at that
commit, plus one in-function item (the dead store). Unmet criteria: the
present worker touches no other `PersistentWorker`; no `CommandBufferFlags`
value exists without a reader.

## Design

Recommended:

- Delete `SwapchainManager.cpp:643`.
- Delete `kExecuted` (`CommandBuffers.h:11`), its clear
  (`CommandBufferManager.cpp:59`), and its set (`:113`). In the comment at
  `CommandBuffers.h:27-32`, drop only the `kExecuted` clause: the sentence
  also covers `mVkFence`, which is reset on a worker and read on the main
  thread, and that part stays.
- Delete the dead store at `SwapchainManager.cpp:566`. This edit is cosmetic
  alone and is included only because this Plan already edits the file.

No submission, wait, or signal that takes effect today changes.

## Critical files

- `Engine/Source/Graphics/Managers/SwapchainManager.cpp`
- `Engine/Source/Graphics/Managers/CommandBufferManager.cpp`
- `Engine/Source/Graphics/Objects/CommandBuffers.h`

## In scope

- The `mSubmitMain.Wait()` call in the `Present` worker lambda.
- `CommandBufferFlags::kExecuted`, its two writes, and its comment clause.
- The dead store at `SwapchainManager.cpp:566`.

## Out of scope

- The `mSubmitMain` worker's `mSubmitGlobal.Wait()`
  (`CommandBufferManager.cpp:177`) and every other wake or wait.
- `kRecorded` and the `mVkFence` part of the comment.
- The suboptimal-acquire branch (`SwapchainManager.cpp:591-599`) and fence
  destruction order.

## Risk tier

Tier 3 — invariant/integration. Trigger: threading. The change removes a
call one worker thread makes on another worker and a worker-thread write to
shared flags; both are on the submission graph's wake/wait chain, a surface
Tier 2 excludes. The evidence above shows both are no-ops today, so the
expected effect is none.

## Acceptance criteria

1. The `Present` worker lambda names no other `PersistentWorker`, and
   `kExecuted` does not appear in the repository.
2. The BrokenEngineSandbox client and ModelViewer build through `/compile`.
3. Through `/agent-harness`, a client run with validation layers on renders
   world frames and world-hidden frames (the map overlay), survives a window
   resize and one full graphics recreation, and exits cleanly, with no
   validation error, hang, or assert. The supplied evidence records no
   harness setup.

## Coordination

The Plan below does not depend on this one, and this one depends on none:

- `Documents/Plans/Engine/TextureManagerDuplicateAndDeadCode.md` edits the
  acquire lookup at `CommandBufferManager.cpp:78-80`, near `:59` and `:113`.
  Whichever lands later re-derives its lines by symbol.
