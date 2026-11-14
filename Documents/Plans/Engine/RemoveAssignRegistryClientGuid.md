<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:38:59.712Z","dependsOn":["Documents/Plans/Engine/RemoveTransferPathDeadWork.md"]} -->
# Remove the redundant owner-GUID write after a transfer

## Context

`STM` is `Engine/Source/Network/Server/ServerTransferManager.cpp`; `BES` is
`Projects/BrokenEngineSandbox/Source`. Verified against commit `4cf9d607`.

`ServerTransferManager::TrackClientTransfers` (`STM:224-247`) calls
`engine::AssignRegistryClientGuid` (`STM:241`) to write the owner's GUID onto
the destination row of each transferred owned entity. The game `Spawn` that
`game::ApplySpawnStatusChange` just ran already wrote that GUID from the same
payload: each ship `Spawn` assigns `pClientGuids[iIndex] = rInfo.clientGuid`
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

Origin: an in-function refactor review of `Engine/Source/Network/Server`,
independently verified with the caveat that the correction is wider than the
one line. Unmet criterion: the transfer path does no work that has no effect.

## Design

Recommended:

- Delete the `AssignRegistryClientGuid` call in `TrackClientTransfers`; keep
  the nonzero-GUID test and the `OnOwnedEntityChangedCell` game call it
  guards. Reword the comment above them, which says the owner "keeps its GUID
  in the destination", so it no longer implies this function writes it.
- Delete `AssignRegistryClientGuid`: declaration, its comment, and
  definition.
- In `Engine/Source/Frame/AGENTS.md`, replace the bullet that describes
  `AssignRegistryClientGuid` with what is then true of the ownership layer's
  client-GUID column, or remove it if nothing remains to state.

## Critical files

- `Engine/Source/Network/Server/ServerTransferManager.cpp`
- `Engine/Source/Frame/FrameRegistry.h`
- `Engine/Source/Frame/FrameRegistry.cpp`
- `Engine/Source/Frame/AGENTS.md`

## In scope

- `ServerTransferManager::TrackClientTransfers`: the
  `AssignRegistryClientGuid` call and the comment above the GUID test.
- `engine::AssignRegistryClientGuid` in `FrameRegistry.h` and
  `FrameRegistry.cpp`.
- The `AssignRegistryClientGuid` bullet in `Engine/Source/Frame/AGENTS.md`,
  and the `pClientGuids` member comment in `FrameRegistry.h` ("sole write
  channel") if the deletion makes it incorrect.

## Out of scope

- The game `Spawn` functions and `SpawnInfo::clientGuid`.
- `game::ServerSession::OnOwnedEntityChangedCell` and its call.
- `ServerSessionBase::RebuildOwnedEntities`.
- The rest of `ServerTransferManager`.
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
and removes the Frame registry's one write function and its documented
contract. No determinism/CRC exposure (the column is CRC-excluded and the
value written is unchanged), and no wire, serialization, version, or threading
change.

## Acceptance criteria

- `AssignRegistryClientGuid` no longer exists, and no source file and no
  `AGENTS.md` names it.
- The client and server build.
- Through `/agent-harness`, an owned ship that crosses a cell boundary is
  reported to its connected owner as `ChangedCell`, not `Died`
  (`ServerSessionBase::SendOwnedEntityState` log), and the owner's fleet
  member coordinate follows it. The supplied evidence records no harness
  setup.

## Coordination

No mandatory nondirectional coordination beyond the metadata dependency.

## Notes

- Not established: whether replay playback restores the GUID on a replayed
  transfer payload. The deletion does not depend on it: when the payload GUID
  is zero, the existing test already skips the write.
