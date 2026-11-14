<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:55:13.912Z","dependsOn":[]} -->
# Remove the write-only CommandBufferFlags::kExecuted

## Context

`CBM` is `Engine/Source/Graphics/Managers/CommandBufferManager.cpp` and `CBH`
is `Engine/Source/Graphics/Objects/CommandBuffers.h`. Verified against commit
`c675b6b2`.

`CommandBufferFlags::kExecuted` (`CBH:11`) is written and never read. It is
cleared in `CommandBufferManager::RecordCommandBuffer` (`CBM:59`) and set in
`SubmitGlobalToQueue` (`CBM:113`). The only other mention is the comment on
`CommandBuffers::mFlags` (`CBH:27-32`). The search covered `Engine`,
`Projects`, `Tools`, `DataPacker`, and `Common`. The one reader of `mFlags`
tests `kRecorded` (`CBM:54`).

`SubmitGlobalToQueue` runs on the `mSubmitGlobal` worker, so the set at
`CBM:113` is the only write to `mFlags` off the main thread. This is dead
state, not a race: every access to `mFlags` was traced as sequenced by the
`PersistentWorker` Wake/Wait chain, and no race is claimed.

Three comments describe the worker write: `CBH:27-32`, and the two that cite
"CommandBuffers.h (mFlags/mVkFence)" as an example of a plain member published
across a Wake/Wait edge (`CBM:75`, `CBM:166-167`).

Origin: an architecture review of `Engine/Source/Graphics/Objects`,
independently verified. Unmet criterion: root `AGENTS.md` `## Directives`
YAGNI — no state without a reader.

## Design

Recommended:

- Delete `kExecuted` and its two writes.
- Reword the `mFlags` / `mVkFence` comment in `CommandBuffers.h` so it covers
  only `mVkFence`, which is still reset on `mSubmitMain` in
  `SubmitMainToQueue` and read on the main thread in `Graphics`.
- Correct the two `CommandBufferManager.cpp` comments so they no longer name
  `mFlags` as a member published across a worker edge.

Describe the change as dead-state removal, not as a race fix.

## Critical files

- `Engine/Source/Graphics/Objects/CommandBuffers.h`
- `Engine/Source/Graphics/Managers/CommandBufferManager.cpp`

## In scope

- `CommandBufferFlags::kExecuted`.
- The comment above `CommandBuffers::mFlags`.
- In `CommandBufferManager.cpp`: the `kExecuted` clear in
  `RecordCommandBuffer`, the `kExecuted` set in `SubmitGlobalToQueue`, and the
  `mFlags` reference in the comments in `SubmitGlobalToQueue` and
  `SubmitMainToQueue`.

## Out of scope

- `kRecorded`, its guard, and the `CommandBufferFlags_t` type.
- `mVkFence` and every Wake/Wait edge, submission, and semaphore.
- The other members those comments name: `mbParticleSemaphoreSignaled`,
  `meDestroyType`, and `kPendingAcquireBarriers`.
- The fence debug name in `CommandBuffers.cpp`.

## Risk tier

Tier 2 — scoped behavior (the author's classification). Trigger: an
enumerator is removed from an enum declared in a header, and the change
deletes a worker-thread write and edits comments that document a threading
contract, which Tier 1's "no public signature or invariant exposure"
excludes. It stays Tier 2 because no synchronization, ordering, Wake/Wait
edge, or value any code reads changes; afterwards `mFlags` is touched only on
the main thread. No determinism/CRC, wire, serialization, replay, or shader
exposure.

## Acceptance criteria

- A repository search for `kExecuted` returns nothing.
- No comment says `CommandBuffers::mFlags` is written off the main thread.
- The client and ModelViewer build.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- `RecordCommandBuffer` runs only on a freshly constructed
  `CommandBufferManager`, so the clear at `CBM:59` always clears a bit that is
  already zero.
- Nothing was built or run by the authoring session; the evidence is from
  reading and searching the tree.
