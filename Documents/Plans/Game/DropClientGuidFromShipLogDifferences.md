<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:11:39.510Z","dependsOn":[]} -->
# Stop comparing pClientGuids in the ship LogDifferences

## Context

Paths are under `Projects/BrokenEngineSandbox/Source/Frame/Collections/` unless
they start elsewhere. Verified against commit `022915b6`.

`LogDifferences` of the three ship PostRender collections compares both halves
of `pClientGuids` (`Battleships/Battleships.cpp:600-601`,
`Fighters/Fighters.cpp:788-789`, `Omnis/Omnis.cpp:841-842`). The column is
server-local:

- `SpawnInfo::clientGuid` is not a `Fields()` entry
  (`Battleships/Battleships.h:315-316`, "clientGuid is server-local, so it is
  not listed"), so a client row created by a spawn or transfer status change
  stores the default value (`Battleships.cpp:383`).
- Only the server sets a real value: the owned spawn
  (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp:204`)
  and `AssignRegistryClientGuid` (`Engine/Source/Frame/FrameRegistry.cpp:84`,
  called from `Engine/Source/Network/Server/ServerTransferManager.cpp:241`).
- A client row adopted from a full state holds the server value, because the
  column is in `SharedMembers()`.

So a client frame and the server frame can differ in this column with no
simulation divergence, and the column is outside `SharedCrcMembers()`.
`LogDifferences` runs only after a CRC mismatch, from
`Engine/Source/Network/Client/ClientDesyncCore.cpp:63` (client against the
server debug frame; `kbDesynchronizationDebugFrames`,
`Engine/Source/EngineSwitches.h:4`, currently false) and
`Engine/Source/File/DifferenceStream.h:472` (server replay, recorded against
re-simulated; `kbReplayFullFrames`). Both callers ignore the return value. The
effect is false rows in a desync dump.

Originating gap: a refactor review of the Battleships collection; unmet
criterion "a desync dump lists only columns whose difference is a real
divergence".

## Design

Recommended: delete the two `pClientGuids` comparisons in each of the three
`LogDifferences` bodies. Keep the `pGlobalIds` comparison
(`Battleships.cpp:599` and its parallels): that column is equal on both sides
by construction, so a difference there is real.

Not established: whether replay playback restores `pClientGuids` on the
re-simulated frame. The deletion does not depend on it.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp`

## In scope

- `BattleshipsPostRender::LogDifferences`, `FightersPostRender::LogDifferences`,
  and `OmnisPostRender::LogDifferences`: the `pClientGuids.uiHigh` and
  `pClientGuids.uiLow` lines only.

## Out of scope

- The `pClientGuids` column, its tuples, `PostRead` check, and writers.
- Every other `LogDifferences` line, and both callers.
- Documents: none describes these comparisons.

## Risk tier

Tier 2 — scoped behavior. Trigger: it changes the output of one diagnostic, the
desync difference log. No determinism/CRC, wire, serialization, version, or
threading exposure: `LogDifferences` writes no frame state and its return value
is unused.

## Acceptance criteria

- No `pClientGuids` comparison remains in the three `LogDifferences` bodies,
  and the `pGlobalIds` comparison remains in each.
- The client and server build.

## Notes

Neither caller is reachable through an existing harness command without a CRC
mismatch, so the change is verified by code reading and the build.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
