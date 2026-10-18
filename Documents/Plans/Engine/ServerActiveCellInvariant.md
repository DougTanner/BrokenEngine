<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-18T17:43:45.970Z","dependsOn":[]} -->
# State the server active-cell invariant at `SyncActiveCells` and drop the re-checks it makes impossible

Line numbers cite the tree in which `Documents/Plans/Game/FrameEditRows.md` (the unified post-tick spawn queue) landed; symbols are authoritative where lines drift.

## Context

On the server, every coordinate in `mActiveCoordinates` has a cell in `mCells` with a non-null `pCurrent` whenever a tick drains queued work or dispatches frames. No code states that, so three sites re-check it:

- `ServerTransferManager::DrainPendingSpawns` (`Engine/Source/Network/Server/ServerTransferManager.cpp:112-113`) looks the cell up and skips the coordinate when `cellIt == mCells.end() || cellIt->second.pCurrent == nullptr`. It never reads `cellIt` otherwise.
- `DrainPendingAgentStatusChanges` (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp:768`, `:775-784`) has the same lookup and the same two skip branches. It never reads `cellIt` otherwise either.
- `GameBase::BuildAndDispatchFrameTicks` (`Engine/Source/GameBase.cpp:534-539`) already calls `mCells.at(rCoordinate)` for every active coordinate. It still logs `BuildDispatch NullFrame` and skips the coordinate when `rCell.pCurrent == nullptr || rCell.pNext == nullptr`.

This is an over-engineering review finding from the FrameEditRows change. The validator proved the `DrainPendingSpawns` checks impossible but did not remove them. Removing them only there would break the root `AGENTS.md` "Mirrored patterns stay parallel" directive, because the harness drain, which that change did not touch, has the same checks.

### Proof: normal play (both drains, and the dispatch)

- `ServerSessionBase::PrepareTick` (`Engine/Source/Network/Server/ServerSessionBase.cpp:28-50`) returns early when `mbReplaying` is true. Otherwise it runs `ServerSessionRuntime::ComputeActiveSet`, then `EnsureNextFrames`, then `ServerBroadcaster::PrepareTickStatusChanges` (`:49`). The harness drain's only caller is `PrepareTickStatusChanges` (`ServerBroadcaster.cpp:44`), so that drain runs only in normal play and only after the same tick's `ComputeActiveSet`.
- `ComputeActiveSet` (`ServerSessionRuntime.cpp:334-349`) rebuilds the set from subscriptions, owned-entity coordinates, and the origin. It then calls `SyncActiveCells` (`:310-331`). `SyncActiveCells` runs `CreateCellAtCoordinate` for every active coordinate that has no cell, and on the server that sets `rCell.pCurrent = std::make_unique<game::Frame>()` (`GameBase::CreateCellFrame`, `GameBase.cpp:978-1009`). It erases only cells outside the active set.
- `DrainPendingSpawns`'s only caller is `HarvestTransfers` (`ServerTransferManager.cpp:283`). `GameBase::FinalizeFrameTick` calls `HarvestTransfers` only when `!mbReplaying` (`GameBase.cpp:578-581`), after the same iteration's `PrepareTick`. `CollectTransfers` runs just before the drain and only adds cells (`ServerTransferManager.cpp:77-84`).
- Every server writer of `pCurrent` stores a non-null frame, or else moves the frame out and erases the cell:
  - `CreateCellFrame` (`GameBase.cpp:987`);
  - `ReadGridSave` (`GridSave.cpp:137-140`), which is followed by `ComputeActiveSet`;
  - `ActivateReplayReader` (`Replay.cpp:350`; `pSavedStart` is `make_unique` at `Replay.cpp:434`);
  - the `pCurrent`/`pNext` swaps in `SwapFrames` (`GameBase.cpp:1176`), `CollectTransfers` (`ServerTransferManager.cpp:83`) and `PrepareReplayTransfers` (`:320`), each of which follows a guaranteed non-null `pNext` for active cells (`EnsureNextFrames`, `GameBase.cpp:1138-1149`, or the `make_unique` just before the swap);
  - `SyncActiveCells`'s own retirement, which moves `pCurrent` into `RetireCoordinate` and then erases the cell (`ServerSessionRuntime.cpp:329-330`).
- The other `mActiveCoordinates` writers do not run on the normal-play server path between `PrepareTick` and the drains or the dispatch. Two are replay-only: `RefreshReplayActiveSet` (`GameBase.cpp:959-973`) and the reader retirement in `SyncReplayTick` (`Replay.cpp:821-823`). The rest are client-only. When `SyncReplayTick` returns `kStopBeforeDispatch` (`GameBase.cpp:445-466`), the loop breaks before dispatch and before `FinalizeFrameTick`.

### Proof: replay playback (the dispatch only)

- During playback the dispatch list is `RefreshReplayActiveSet`'s reader coordinates, minus readers that `SyncReplayTick` retires. A retired reader's cell and active entry are both erased (`Replay.cpp:821-823`).
- Each reader's cell got a non-null `pCurrent` from `ActivateReplayReader` (`Replay.cpp:343-356`).
- `SyncReplayTick` already dereferences `*mCells.at(rCoordinate).pCurrent` for every live reader on every tick, before dispatch (`Replay.cpp:889`).

## Design

The author recommends stating the invariant once, as a comment at its owner, and deleting the re-checks:

1. At `ServerSessionRuntime::SyncActiveCells`, add a short comment: after this function runs, every active coordinate has a cell with a non-null `pCurrent`, so later server code in the tick looks cells up with `mCells.at` and does not re-check. Put it on the function in `ServerSessionRuntime.cpp`, not in an `AGENTS.md` file, because it is a local invariant of that function.
2. In `DrainPendingSpawns`, delete the `cellIt` lookup and its two conditions. Keep the `std::ranges::contains(mActiveCoordinates, rCoordinate)` and `std::ranges::none_of(rPendingSpawns, IsReleased)` conditions. Neither drain needs a cell lookup afterward, because neither one reads the cell.
3. In `DrainPendingAgentStatusChanges`, delete the `cellIt` lookup and its two skip blocks. Keep the `bActive` and `none_of` skips, as in step 2.
4. In `BuildAndDispatchFrameTicks`, drop the `rCell.pCurrent == nullptr` half of the condition and the `pCurrent` field from the log line. Keep the `pNext` half and its warning. This change makes no claim about `pNext`.

Rejected alternative: an `ASSERT` loop over the active set in `SyncActiveCells`. It costs a per-tick scan to restate something the code just above it guarantees by construction. The comment is enough.

## Critical files

- `Engine/Source/Network/Server/ServerSessionRuntime.cpp`
- `Engine/Source/Network/Server/ServerTransferManager.cpp`
- `Engine/Source/GameBase.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp`

## In scope

- `ServerSessionRuntime::SyncActiveCells`: one comment stating the invariant.
- `ServerTransferManager::DrainPendingSpawns`: remove the `cellIt` lookup and the `cellIt == end()` and `pCurrent == nullptr` conditions.
- `DrainPendingAgentStatusChanges`: remove the `cellIt` lookup and the two skip blocks for a missing cell and a null `pCurrent`.
- `GameBase::BuildAndDispatchFrameTicks`: remove the `pCurrent == nullptr` operand and the `pCurrent` field of the `BuildDispatch NullFrame` log line.

## Out of scope

- The `pNext` check and warning in `BuildAndDispatchFrameTicks`, `EnsureNextFrames`, and `SwapFrames`.
- `ComputeActiveSet`'s membership rules, `CreateCellAtCoordinate`, `CollectTransfers`, `PrepareReplayTransfers`, `RefreshReplayActiveSet`, `SyncReplayTick`, and `ActivateReplayReader`.
- Any other `mCells.find` or `contains` site, including client code and `ServerTransferManager::IsDestinationLive`.
- The `ServerBroadcaster::PrepareTickStatusChanges` comment, which stays accurate under the invariant.
- Any `AGENTS.md` text, any new `ASSERT`, and narrowing the post-tick batch type.

## Risk triggers and invariants

- Change Workflow Tier 2. Trigger: scoped server-runtime behavior. The change makes an existing server invariant explicit and removes branches proven unreachable on the server tick path. It does not change what the tick computes, carries, or trusts, so no determinism/CRC, wire, serialization, save/replay, threading, or trust-boundary surface changes.
- Invariant: before the drains and the dispatch, every active coordinate has a cell with a non-null `pCurrent`. The Context section proves it for normal play and for replay playback.
- Determinism: on reachable states, the drains release the same entries in the same order, and the dispatch builds the same `mActiveFrameReferences`.

## Acceptance criteria

- No `mCells.find` or `pCurrent == nullptr` check remains in either drain. `BuildAndDispatchFrameTicks` checks only `pNext`.
- Client and server build cleanly through `/compile`.
- Run once through `/agent-harness`. Normal play must show no `BuildDispatch NullFrame` warning. A replay determinism check must pass. An `inject_payload` scheduled for a later tick must apply on that tick. A cross-cell transfer must arrive.

## Notes

- Proposal source: the over-engineering review's design suggestion, which the user chose to save as a follow-up. The narrower post-tick batch type was declined and is not part of this Plan.
- `Documents/Plans/Game/FrameEditCellPins.md` adds pinned coordinates to `ComputeActiveSet` and leaves `SyncActiveCells` unchanged. Pinned coordinates still pass through `SyncActiveCells`, so that Plan keeps this invariant without changes.
