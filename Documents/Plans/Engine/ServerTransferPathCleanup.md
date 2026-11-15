<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:38:59.712Z","dependsOn":[]} -->
# Remove dead and redundant work from the server transfer path

## Context

`STM` is `Engine/Source/Network/Server/ServerTransferManager.cpp`; `BES` is
`Projects/BrokenEngineSandbox/Source`. Every line reference was verified
against commit `4cf9d607`, before any step of this Plan runs.

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

A redundant write follows: `ServerTransferManager::TrackClientTransfers`
(`STM:224-247`) calls `engine::AssignRegistryClientGuid` (`STM:241`) to write
the owner's GUID onto the destination row of each transferred owned entity.
The game `Spawn` that `game::ApplySpawnStatusChange` just ran already wrote
that GUID from the same payload: each ship `Spawn` assigns
`pClientGuids[iIndex] = rInfo.clientGuid`
(`BES/Frame/Collections/Omnis/Omnis.cpp:326`,
`BES/Frame/Collections/Fighters/Fighters.cpp:533`,
`BES/Frame/Collections/Battleships/Battleships.cpp:384`), and
`game::OwnedSpawnClientGuid` (`BES/Frame/StatusChange.cpp`), which feeds
`ClientTransferInfo::clientGuid`, returns that same `clientGuid` member. The
second write stores the value already there, after the destination CRC
recompute, into a column the CRC excludes.

`STM:241` is the only caller of `AssignRegistryClientGuid`
(`Engine/Source/Frame/FrameRegistry.h:58`,
`Engine/Source/Frame/FrameRegistry.cpp:76-90`), and
`Engine/Source/Frame/AGENTS.md` (lines 182-185) describes it as "the
registry's only write".

Origin: an in-function refactor review and an architecture review (cohesion
lens) of `Engine/Source/Network/Server`, independently verified (the GUID
finding with the caveat that the correction is wider than the one line).
Unmet criterion: the transfer path does no work whose only consumer is
compiled out and no work that has no effect, holds no check that cannot fire,
and derives the owned-transfer list once.

## Design

Recommended, each behavior-preserving, in this order:

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
6. Delete the `AssignRegistryClientGuid` call in `TrackClientTransfers`; keep
   the nonzero-GUID test and the `OnOwnedEntityChangedCell` game call it
   guards. Reword the comment above them, which says the owner "keeps its GUID
   in the destination", so it no longer implies this function writes it.
7. Delete `AssignRegistryClientGuid`: declaration, its comment, and
   definition.
8. In `Engine/Source/Frame/AGENTS.md`, replace the bullet that describes
   `AssignRegistryClientGuid` with what is then true of the ownership layer's
   client-GUID column, or remove it if nothing remains to state.

## Critical files

- `Engine/Source/Network/Server/ServerTransferManager.h`
- `Engine/Source/Network/Server/ServerTransferManager.cpp`
- `Engine/Source/Frame/FrameRegistry.h`
- `Engine/Source/Frame/FrameRegistry.cpp`
- `Engine/Source/Frame/AGENTS.md`

## In scope

`ServerTransferManager`:

- `ApplyPreparedTransfers`: the pre-CRC arena and loop, the log preparation
  and the two verbose logs, both parameters, and the new single derivation.
- `SpawnTransfers`: the liveness check and the parameter.
- `CollectTransfers`: its parameter, the `ClientTransferInfo` push, the
  destination lookup, and the `empty()` guard.
- `ApplyReplayTransfers` and `HarvestTransfers`: the arena, the derivation
  loop, and the call arguments.
- `IsDestinationLive`: the slot loop.
- `TrackClientTransfers`: its list parameter as step 3 passes it, the
  `AssignRegistryClientGuid` call, and the comment above the GUID test.
- The matching private declarations in `ServerTransferManager.h`.

Frame registry:

- `engine::AssignRegistryClientGuid` in `FrameRegistry.h` and
  `FrameRegistry.cpp`.
- The `AssignRegistryClientGuid` bullet in `Engine/Source/Frame/AGENTS.md`,
  and the `pClientGuids` member comment in `FrameRegistry.h` ("sole write
  channel") if the deletion makes it incorrect.

## Out of scope

- The `uiSharedCrc` recompute, `SortTransfersByType`, `DrainPendingSpawns`,
  and the replay capture.
- The liveness test in `CollectTransfers` and its verbose drop log.
- The visibility of `IsDestinationLive`.
- `AGENTS.md` files other than the `AssignRegistryClientGuid` bullet in
  `Engine/Source/Frame/AGENTS.md`: none describes the removed work. The
  game-name list in `Engine/Source/Network/Server/AGENTS.md` is already
  corrected and stays as it is: `game::StatusChangeTypeName` and
  `game::Frame::OwnedEntityCount` lose a use here but keep others in
  `CollectTransfers` and `IsDestinationLive`.
- The game `Spawn` functions and `SpawnInfo::clientGuid`.
- `game::ServerSession::OnOwnedEntityChangedCell` and its call.
- `ServerSessionBase::RebuildOwnedEntities`.
- `Documents/Plans/Game/DropClientGuidFromShipLogDifferences.md` and
  `Documents/Investigations/Engine/ResolveShipGlobalIdCrcExclusion.md`, which
  cite `AssignRegistryClientGuid` as evidence and are left as they are. The
  first cites it only to show that the GUID column is server-local, which
  stays true: the server's in-memory payload carries the GUID and the wire
  does not. Its citation of `Engine/Source/Frame/FrameRegistry.cpp:84` goes
  stale when this Plan lands. The second (lines 50-54) says the client-GUID
  column "has a server-only post-tick writer", naming
  `AssignRegistryClientGuid`; this Plan deletes that writer, so the statement
  goes stale too.

## Risk tier

Tier 3 — invariant/integration (the author's classification). Trigger: the
change spans two independently owned subsystems, `Network/Server` and `Frame`,
and removes the Frame registry's one write function
(`FrameRegistry.h`/`FrameRegistry.cpp`) and its documented contract. It also
restructures the code that materializes transfers into CRC'd Frames, with
behavior meant to be unchanged; a mistake would alter CRC'd state or drop the
CRC recompute.

Invariants: no determinism/CRC exposure (the GUID column is CRC-excluded and
the value written is unchanged, and the transfer restructuring keeps CRC'd
state and the `uiSharedCrc` recompute as they are); no wire, serialization,
version, or threading change; every changed `ServerTransferManager` signature
is private.

## Acceptance criteria

- `ApplyPreparedTransfers` holds no pre-CRC capture and no log preparation,
  and still recomputes `uiSharedCrc` for every destination after
  `SpawnTransfers`.
- `bFilterDestinationLiveness` and the `ClientTransferInfo` arena parameters
  no longer exist; the list is built in one place.
- `AssignRegistryClientGuid` no longer exists, and no source file and no
  `AGENTS.md` names it.
- The client and server build.
- One `/agent-harness` run (the supplied evidence records no harness setup):
  an owned ship that crosses a cell boundary is reported to its connected
  owner as `ChangedCell`, not `Died` (`ServerSessionBase::SendOwnedEntityState`
  log), and the owner's fleet member coordinate follows it; the replay
  recorded across that cell transfer plays back with no CRC mismatch.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- Not established: whether replay playback restores the GUID on a replayed
  transfer payload. The deletion does not depend on it: when the payload GUID
  is zero, the existing test already skips the write.
