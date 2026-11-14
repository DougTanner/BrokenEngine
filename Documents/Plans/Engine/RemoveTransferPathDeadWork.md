<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:38:42.597Z","dependsOn":[]} -->
# Remove dead work from the server transfer path

## Context

`STM` is `Engine/Source/Network/Server/ServerTransferManager.cpp`; `BES` is
`Projects/BrokenEngineSandbox/Source`. Verified against commit `4cf9d607`.

Three pieces of `ServerTransferManager` do work that has no effect:

1. Log-only work that is compiled out. `ApplyPreparedTransfers` (`STM:154-222`)
   fills a `preCrcsArena` with each destination's pre-transfer CRC, then per
   destination per tick runs two `common::ToHex` calls, an owned-id
   `std::format_to_n` loop, and a subscription `std::ranges::any_of`
   (`STM:163-167`, `STM:176`, `STM:179-217`). Their only consumers are two
   `LOG(kNetwork, kVerbose, ...)` calls (`STM:205`, `STM:215`). `LOG` is
   `if constexpr` on the category's compile floor (`Common/Log/Log.h`), the
   Network floor is `kDebug` (`keLogLevelNetwork`,
   `Engine/Source/EngineSwitches.h`), and `kVerbose` is below `kDebug`
   (`Common/Log/LogTypes.h`), so both calls are discarded at compile time
   while the preparation outside the macro still runs.
2. A check that cannot fire. `SpawnTransfers` (`STM:141-147`) repeats the
   liveness predicate `CollectTransfers` (`STM:64`) applied in the same
   `HarvestTransfers` call. Between the two run only `DrainPendingSpawns`,
   `SortTransfersByType`, and `Replay::CaptureAcceptedTransfers` (which takes
   a const Frame); none changes `pCurrent` or a subscription. If it did fire
   it would log, break, and still apply the transfer. Replay passes `false`
   (`STM:338`). The check rescans ship rows through
   `game::Frame::OwnedEntityCount`.
3. One list derived twice. `CollectTransfers` (`STM:81-89`) and
   `ApplyReplayTransfers` (`STM:313-336`) each build the `ClientTransferInfo`
   list with the same filter (`game::IsOwnedEntityTransfer` and a nonzero
   `game::OwnedSpawnGlobalId`). A drained spawn fails
   `game::IsOwnedEntityTransfer`, which requires `game::IsTransferType`
   (`BES/Frame/StatusChange.cpp`), so walking `mTransfers` gives the same
   entries; only their order differs, and the consumer writes one member per
   unique global ID (`ServerFleetManager::OnOwnedEntityTransferred`,
   `BES/Network/Server/ServerFleetManager.cpp`).

Two smaller sites in the same functions:

- `IsDestinationLive` (`STM:27-36`) hand-writes the scan
  `ClientConnection::FindSlotForCoordinate`
  (`Engine/Source/Network/Server/Server.h`) already does: same predicate, and
  neither reads the epoch.
- `CollectTransfers` never reads `it` after `STM:71`, so `STM:76` is a dead
  store plus a lookup, and `STM:45-48` is an `empty()` guard in front of a
  range-for over the same container.

Origin: an in-function refactor review and an architecture review (cohesion
lens) of `Engine/Source/Network/Server`, independently verified. Unmet
criterion: the transfer path does no work whose only consumer is compiled out,
holds no check that cannot fire, and derives the owned-transfer list once.

## Design

Recommended, each behavior-preserving:

1. In `ApplyPreparedTransfers`, delete `preCrcsArena`, its fill loop, and the
   log preparation and both verbose `LOG` calls. Keep the `SpawnTransfers`
   call and the loop that recomputes `postRender.uiSharedCrc` for each
   destination: `Engine/Source/Network/Server/AGENTS.md`
   `## Transfers and Publication` requires that recompute. Deletion is
   recommended over gating the work on the log floor because no sibling code
   gates on the floor.
2. Delete the liveness check in `SpawnTransfers` and the
   `bFilterDestinationLiveness` parameter from `SpawnTransfers` and
   `ApplyPreparedTransfers` and from both callers.
3. Derive the `ClientTransferInfo` list once, in `ApplyPreparedTransfers`,
   from `mTransfers`, and pass it to `TrackClientTransfers`. Remove the arena
   parameters from `CollectTransfers` and `ApplyPreparedTransfers`, the
   derivation in `CollectTransfers`, the derivation in `ApplyReplayTransfers`,
   and the arena in `HarvestTransfers`.
4. In `IsDestinationLive`, replace the hand-written slot loop with
   `std::ranges::any_of` over `mClients` calling `FindSlotForCoordinate`; keep
   the owned-entity test above it.
5. In `CollectTransfers`, test the destination with `contains`, delete the
   re-lookup into `it`, and delete the `empty()` guard.

## Critical files

- `Engine/Source/Network/Server/ServerTransferManager.h`
- `Engine/Source/Network/Server/ServerTransferManager.cpp`

## In scope

- `ServerTransferManager::ApplyPreparedTransfers`: the pre-CRC arena and loop,
  the log preparation and the two verbose logs, both parameters, and the new
  single derivation.
- `ServerTransferManager::SpawnTransfers`: the liveness check and the
  parameter.
- `ServerTransferManager::CollectTransfers`: its parameter, the
  `ClientTransferInfo` push, the destination lookup, and the `empty()` guard.
- `ServerTransferManager::ApplyReplayTransfers` and `HarvestTransfers`: the
  arena, the derivation loop, and the call arguments.
- `ServerTransferManager::IsDestinationLive`: the slot loop.
- The matching private declarations in `ServerTransferManager.h`.

## Out of scope

- The `uiSharedCrc` recompute, `SortTransfersByType`, `DrainPendingSpawns`,
  and the replay capture.
- The body of `TrackClientTransfers`;
  `Documents/Plans/Engine/RemoveAssignRegistryClientGuid.md` owns it.
- The liveness test in `CollectTransfers` and its verbose drop log.
- The visibility of `IsDestinationLive`.
- `AGENTS.md` files: none describes the removed work. The game-name list in
  `Engine/Source/Network/Server/AGENTS.md` is already corrected and stays as
  it is: `game::StatusChangeTypeName` and `game::Frame::OwnedEntityCount` lose
  a use here but keep others in `CollectTransfers` and `IsDestinationLive`.

## Risk tier

Tier 2 — scoped behavior (the author's classification). Trigger: it
restructures the code that materializes transfers into CRC'd Frames, inside one
subsystem, with behavior meant to be unchanged; a mistake would alter CRC'd
state or drop the CRC recompute. No wire, serialization, version, or threading
change; every changed signature is private.

## Acceptance criteria

- `ApplyPreparedTransfers` holds no pre-CRC capture and no log preparation,
  and still recomputes `uiSharedCrc` for every destination after
  `SpawnTransfers`.
- `bFilterDestinationLiveness` and the `ClientTransferInfo` arena parameters
  no longer exist; the list is built in one place.
- The server builds.
- A replay recorded across cell transfers plays back with no CRC mismatch
  through `/agent-harness` (the supplied evidence records no harness setup).

## Coordination

No directional dependencies or mandatory nondirectional coordination.
