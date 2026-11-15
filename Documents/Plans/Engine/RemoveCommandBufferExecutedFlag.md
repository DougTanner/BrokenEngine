<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:55:13.912Z","dependsOn":[]} -->
# Remove dead submit-worker state: kExecuted and a never-waiting Wait

## Context

`CBM` is `Engine/Source/Graphics/Managers/CommandBufferManager.cpp`, `CBH` is
`Engine/Source/Graphics/Objects/CommandBuffers.h`, and `G/` is
`Engine/Source/Graphics/`. `CBM` and `CBH` lines were verified at commit
`c675b6b2`; `SwapchainManager.cpp` and `G/Graphics.cpp` lines are those at
commit `401e4b75`. The `kExecuted` lines are the same at both commits.

### kExecuted

`CommandBufferFlags::kExecuted` (`CBH:11`) is written and never read. It is
cleared in `CommandBufferManager::RecordCommandBuffer` (`CBM:59`) and set in
`SubmitGlobalToQueue` (`CBM:113`). The only other mention is the comment on
`CommandBuffers::mFlags` (`CBH:27-32`). The search covered `Engine`,
`Projects`, `Tools`, `DataPacker`, and `Common`. The one reader of `mFlags`
tests `kRecorded` (`CBM:54`).

`SubmitGlobalToQueue` runs on the `mSubmitGlobal` worker, so the set at
`CBM:113` is the only write to `mFlags` off the main thread; it writes the
byte that carries `kRecorded`. This is dead state, not a race: every access
to `mFlags` was traced as sequenced by the `PersistentWorker` Wake/Wait chain,
and no race is claimed.

Three comments describe the worker write: `CBH:27-32`, and the two that cite
"CommandBuffers.h (mFlags/mVkFence)" as an example of a plain member published
across a Wake/Wait edge (`CBM:75`, `CBM:166-167`).

`RecordCommandBuffer` runs only on a freshly constructed
`CommandBufferManager`, so the clear at `CBM:59` always clears a bit that is
already zero.

### The Present worker's mSubmitMain.Wait()

The `SwapchainManager::Present` worker lambda calls
`gpCommandBufferManager->mSubmitMain.Wait()` (`SwapchainManager.cpp:643`).
The call never waits. `Present` has one caller (`G/Graphics.cpp:280`, inside
`PresentAcquire`), reached two ways: `RenderMainPresentAcquire` after
`SubmitUiCommandBuffer` (`G/Graphics.cpp:237`), where the main thread already
ran `mSubmitMain.Wait()` (`CommandBufferManager.cpp:189`), which clears
`mbDispatched` before any rethrow
(`Common/Threading/PersistentWorker.cpp:52-58`); and `RenderUiPresentAcquire`
(`G/Graphics.cpp:255-257`), where nothing woke `mSubmitMain` this frame. The
main thread does not wake `mSubmitMain` again before `mPresent.Wait()`
(`G/Graphics.cpp:295`, or `:720` in `Destroy`). So `mbDispatched` is false at
`:643` on every path. The call still reads another worker's `mbDispatched`
from the present thread. The comment at
`Common/Threading/PersistentWorker.h:28` leaves that member unsynchronized and
limits it to the serialized `Wake`, `Wait`, and destructor calls, so the read
is safe only because the `mPresent` wake/wait edge orders it against the main
thread's calls.

### Dead store

`SwapchainManager.cpp:566` stores `VK_NULL_HANDLE` to
`mCurrentImageAvailableVkFence`, and `:569` overwrites it unconditionally with
nothing reading it between.

Origin: an architecture review of `Engine/Source/Graphics/Objects` and the
architecture phase (simulation and threading invariants lens) of an analysis
of `Engine/Source/Graphics/Managers`, plus one in-function item (the dead
store), independently verified. Unmet criteria: root `AGENTS.md`
`## Directives` YAGNI — no state without a reader, so no `CommandBufferFlags`
value exists without a reader; the present worker touches no other
`PersistentWorker`.

## Design

Recommended:

- Delete `kExecuted` (`CBH:11`) and its two writes: the clear in
  `RecordCommandBuffer` (`CBM:59`) and the set in `SubmitGlobalToQueue`
  (`CBM:113`).
- Reword the `mFlags` / `mVkFence` comment in `CommandBuffers.h`
  (`CBH:27-32`) so it covers only `mVkFence`, which is still reset on
  `mSubmitMain` in `SubmitMainToQueue` and read on the main thread in
  `Graphics`; that part stays.
- Correct the two `CommandBufferManager.cpp` comments (`CBM:75`,
  `CBM:166-167`) so they no longer name `mFlags` as a member published across
  a worker edge.
- Delete the `mSubmitMain.Wait()` call in the `Present` worker lambda
  (`SwapchainManager.cpp:643`).
- Delete the dead store at `SwapchainManager.cpp:566`. This edit is cosmetic
  alone and is included only because this Plan already edits the file.

Describe the change as dead-state removal, not as a race fix. No submission,
wait, or signal that takes effect today changes.

## Critical files

- `Engine/Source/Graphics/Objects/CommandBuffers.h`
- `Engine/Source/Graphics/Managers/CommandBufferManager.cpp`
- `Engine/Source/Graphics/Managers/SwapchainManager.cpp`

## In scope

- `CommandBuffers.h`: `CommandBufferFlags::kExecuted` and the comment above
  `CommandBuffers::mFlags`.
- `CommandBufferManager.cpp`: the `kExecuted` clear in `RecordCommandBuffer`,
  the `kExecuted` set in `SubmitGlobalToQueue`, and the `mFlags` reference in
  the comments in `SubmitGlobalToQueue` and `SubmitMainToQueue`.
- `SwapchainManager.cpp`: the `mSubmitMain.Wait()` call in the `Present`
  worker lambda, and the dead store at `:566`.

## Out of scope

- `kRecorded`, its guard, and the `CommandBufferFlags_t` type.
- `mVkFence`, the `mVkFence` part of the comment, and every other Wake/Wait
  edge, submission, and semaphore, including the `mSubmitMain` worker's
  `mSubmitGlobal.Wait()` (`CommandBufferManager.cpp:177`).
- The other members those comments name: `mbParticleSemaphoreSignaled`,
  `meDestroyType`, and `kPendingAcquireBarriers`.
- The fence debug name in `CommandBuffers.cpp`.
- The suboptimal-acquire branch (`SwapchainManager.cpp:591-599`) and fence
  destruction order.

## Risk tier

Tier 3 — invariant/integration. Trigger: threading. The change removes a call
one worker thread makes on another worker and a worker-thread write to shared
flags; both are on the submission graph's wake/wait chain, a surface Tier 2
excludes. It also removes an enumerator from an enum declared in a header and
edits comments that document a threading contract. The evidence above shows
both removals are no-ops today, so the expected effect is none: afterwards
`mFlags` is touched only on the main thread. No determinism/CRC, wire,
serialization, replay, or shader exposure.

## Acceptance criteria

1. A repository search for `kExecuted` returns nothing.
2. No comment says `CommandBuffers::mFlags` is written off the main thread.
3. The `Present` worker lambda names no other `PersistentWorker`.
4. The BrokenEngineSandbox client and ModelViewer build through `/compile`.
5. Through `/agent-harness`, a client run with validation layers on renders
   world frames and world-hidden frames (the map overlay), survives a window
   resize and one full graphics recreation, and exits cleanly, with no
   validation error, hang, or assert. The supplied evidence records no
   harness setup.

## Notes

- Nothing was built or run by the authoring sessions; the evidence is from
  reading and searching the tree.
