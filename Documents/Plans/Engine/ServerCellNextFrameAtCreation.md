<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-18T19:36:05.896Z","dependsOn":[]} -->
# Allocate a server cell's next frame at creation and drop the lazy next-frame allocators

Line numbers cite the tree in which the server active-cell invariant change (the server `Cell::pCurrent` comment and the checked `mCells.at` lookups in `EnsureNextFrames` and `RefreshReplayActiveSet`) landed; symbols are authoritative where lines drift.

## Context

The server `Cell` (`Engine/Source/GameBase.h:72-76`) holds two frames, `pCurrent` and `pNext`. The comment on `pCurrent` (`:73`) states that every server cell has a non-null current frame from creation. `pNext` has no such rule: a new cell gets only `pCurrent`, and several sites allocate `pNext` later, each guarding against a null `pNext`.

- `GameBase::CreateCellFrame` (`Engine/Source/GameBase.cpp:973-991`) sets only `rCell.pCurrent = std::make_unique<game::Frame>()` on the server (`:982`). It is the frame step of `CreateCellAtCoordinate` (`:993-1004`) and `CreateNewFrame` (`:1006-1036`).
- `CreateNewFrame` allocates the origin cell's `pNext` itself after `CreateCellFrame` (`:1033-1035`).
- `GameBase::EnsureNextFrames` (`:1133-1145`) gives every active coordinate's cell a `pNext` when it is null. Its three callers each call it straight after `ComputeActiveSet`: `ServerSessionBase::PrepareTick` (`Engine/Source/Network/Server/ServerSessionBase.cpp:42`), `GameBase::PrepareActiveSet` (`GameBase.cpp:1082`), and the replay-stop branch of the server loop (`GameBase.cpp:464`). Declared at `GameBase.h:213`.
- `GameBase::RefreshReplayActiveSet` (`GameBase.cpp:954-971`) allocates `pNext` for each reader coordinate whose `pNext` is null (`:965-968`).
- `ServerTransferManager::CollectTransfers` creates a missing destination cell, then allocates its `pNext` and swaps the two frames (`Engine/Source/Network/Server/ServerTransferManager.cpp:77-84`, `make_unique` at `:81`, swap at `:82`).
- `Replay::ActivateReplayReader` (`Engine/Source/File/Replay.cpp:343-356`) creates or keeps the cell, moves the reader's saved start into `pCurrent`, allocates a new `pNext` (`:351`), and copies the start into it with `TransferViaStream` (`:352`).
- `ReadGridSave` stages each loaded cell with both frames (`Engine/Source/File/GridSave.cpp:137-140`); staged cells do not go through `CreateCellFrame`.

Every cell already ends up with a non-null `pNext` before anything reads it, and nothing sets `pNext` to null afterward: every writer assigns a new frame or swaps two non-null frames (`GameBase::SwapFrames`, `GameBase.cpp:1167-1175`; the `CollectTransfers` swap). After a swap, `pNext` holds the previous current frame as stale, reusable memory (the `SwapFrames` comment, `:1174`). So a `pNext` allocated at creation instead of lazily holds no state that a tick reads before overwriting.

Origin: `/over-engineering-review` design suggestion D1, raised while reviewing the server active-cell invariant change; the user chose to record it as this follow-up.

## Design

1. Proof first, before any edit. Re-prove from current source, and record the proof in the session's working notes, that allocating `pNext` in `CreateCellFrame` is safe on every server creation path, and that no reader relies on `pNext` being freshly made or on `EnsureNextFrames` timing:
   - Creation paths: `CreateCellAtCoordinate` (reached from `ServerSessionRuntime::SyncActiveCells`, `CollectTransfers`, `ActivateReplayReader`, and the `create_cell` agent command `CommandCreateCell` at `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp:510`), `CreateNewFrame`, and `ReadGridSave` staging (which keeps its own `pNext` allocation). Re-run a `git grep` of `mCells` writers over `Engine` and `Projects` to confirm that list is complete.
   - `CollectTransfers`: today the swap leaves a fresh default-constructed frame in `pCurrent` and the `CreateCellFrame`-initialized frame in `pNext`, which the arrivals then write (`ServerTransferManager.cpp:145`, `:172`, `:182`, `:235`). With `pNext` allocated by `CreateCellFrame` as a default-constructed frame, the swap leaves the same pair. Confirm.
   - `ActivateReplayReader` on an existing cell: `pNext` would hold a live cell's stale frame instead of a new one. `game::operator>>` (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:734-745`) reads into a local `Frame` and move-assigns it over the target, so `TransferViaStream` should not depend on the target's prior contents. Confirm, including any other reader of the `ActivateReplayReader` `pNext`.
   - `EnsureNextFrames` timing: confirm that no code between a cell's creation and that cell's first `EnsureNextFrames`/`RefreshReplayActiveSet` call tests or reads `pNext`, so that allocating earlier changes no branch.
   - If any item fails, stop and return the failing path to main; do not narrow the design to fit it.
2. In the server branch of `CreateCellFrame`, allocate `pNext` with `pCurrent` as a default-constructed `game::Frame` with no initialization, since every tick overwrites the next frame before reading it.
3. Rewrite the server `Cell` comment (`GameBase.h:73`) so it states the invariant for both pointers: every server cell has non-null `pCurrent` and `pNext` from creation. The swap clause no longer needs its own justification.
4. Delete `GameBase::EnsureNextFrames` (definition and declaration) and its three calls. In `PrepareTick`, keep the `ScopedSuppressAllocationTracking` that `ComputeActiveSet` still needs, and change its `Heap:` comment (`ServerSessionBase.cpp:38`) so it no longer names `EnsureNextFrames`.
5. In `RefreshReplayActiveSet`, delete the `pNext` null test and its allocation (`GameBase.cpp:965-968`) together with the `mCells.at` lookup above it (`:964`), whose only reader is that test; keep the coordinate push. Change the `Heap:` comment (`:957`) so it no longer names `make_unique<Frame>`.
6. Delete the `pNext` allocation in `CreateNewFrame` (`GameBase.cpp:1033-1035`), now made by `CreateCellFrame`.
7. In `CollectTransfers`, delete the `make_unique` of `pNext` (`ServerTransferManager.cpp:81`); keep the swap (`:82`).
8. In `ActivateReplayReader`, delete the `make_unique` of `pNext` (`Replay.cpp:351`); keep the `TransferViaStream` into the existing `pNext`.

Exposure: no CRC, wire, save, replay-format, `kiVersion`, threading, or trust-boundary change. Allocation: `CreateCellFrame`'s callers already run under `ScopedSuppressAllocationTracking` (`CreateCellAtCoordinate`, `CreateNewFrame`); confirm the replay and transfer creation paths still reach it under a suppression scope. Memory: a cell now holds its second frame from creation rather than from its first tick; every surviving cell gets it before its first swap today anyway.

Change Workflow tier: Tier 2. Highest trigger: one subsystem's runtime behavior (server cell frame lifecycle, allocation timing of `pNext`) with no determinism/CRC, wire, serialization, save/replay compatibility, threading, or trust surface changed.

## Critical files

- `Engine/Source/GameBase.h` — server `Cell` comment; `EnsureNextFrames` declaration.
- `Engine/Source/GameBase.cpp` — `CreateCellFrame`, `CreateNewFrame`, `RefreshReplayActiveSet`, `EnsureNextFrames`, `PrepareActiveSet`, the replay-stop branch of the server loop.
- `Engine/Source/Network/Server/ServerSessionBase.cpp` — `PrepareTick`.
- `Engine/Source/Network/Server/ServerTransferManager.cpp` — `CollectTransfers`.
- `Engine/Source/File/Replay.cpp` — `ActivateReplayReader`.

## In scope

- The server branch of `GameBase::CreateCellFrame`: add the `pNext` allocation.
- The server `Cell` comment in `GameBase.h` directly above `pCurrent`.
- `GameBase::EnsureNextFrames`: delete its definition and declaration, and its calls in `ServerSessionBase::PrepareTick`, `GameBase::PrepareActiveSet`, and the replay-stop branch of the server loop.
- The `pNext` null test and allocation in `GameBase::RefreshReplayActiveSet`, and that function's `Heap:` comment.
- The `pNext` allocation at the end of `GameBase::CreateNewFrame`.
- The `pNext` `make_unique` in `ServerTransferManager::CollectTransfers` (its swap stays).
- The `pNext` `make_unique` in `Replay::ActivateReplayReader` (its `TransferViaStream` stays).
- The `Heap:` comment in `ServerSessionBase::PrepareTick` that names `EnsureNextFrames`.

## Out of scope

- The client branch of `CreateCellFrame` and the client snapshot ring.
- `ReadGridSave` staging and its `pNext` allocation (`GridSave.cpp:137-140`), `AdoptGridSave`, and the grid-save format.
- `GameBase::SwapFrames` and the `CollectTransfers` swap.
- `ComputeActiveSet`, `SyncActiveCells`, and the active-set membership rules.
- `SyncReplayTick`, `PrepareReplayTransfers`, reader retirement, and the replay stream format.
- The `pCurrent` invariant and every check the server active-cell invariant change already removed or kept, except the checked `mCells.at` lookups in `EnsureNextFrames` (`GameBase.cpp:1140-1142`) and `RefreshReplayActiveSet` (`:964`), which Design steps 4-5 delete.
- `Documents/Architecture/FrameUpdatePipeline.md` (its `pCurrent`/`pNext` description stays accurate).

## Acceptance criteria

1. Design step 1's proof is recorded, and every item holds.
2. No `EnsureNextFrames` symbol remains in code under `Engine/` or `Projects/`, and no server code tests `pNext == nullptr` or allocates `pNext` outside `CreateCellFrame` and `ReadGridSave`.
3. Server and client build clean.
4. Live checks through `/agent-harness`, each with no `ASSERT`, `DEBUG_BREAK`, crash, or CRC mismatch:
   - normal play on the origin and at least one neighbor cell;
   - save, load, and reset;
   - replay determinism, including a reader activated mid-playback;
   - `inject_payload` with one spawn entry and one destroy or update entry, each scheduled for a later tick;
   - a cross-cell transfer into a cell that did not exist before the transfer.

## Notes

- The cell pin (`FramePostRenderBase::uiPinned`, kept active by `ServerSessionBase::AddOwnedEntityCoordinates`) creates a pinned cell through `create_cell` (`CommandCreateCell`, `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp`), which calls `CreateCellAtCoordinate` outside a tick, so today the cell gets `pNext` through the next `PrepareTick`'s `EnsureNextFrames`.
  That timing is context, not a mechanism the pin commands depend on: neither `create_cell` nor `edit_frame` reads `pNext`.
