<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:43:59.980Z","dependsOn":[]} -->
# Log the connecting-client diagnostic dump at kDebug

## Context

Found by a deep analysis of `Projects/BrokenEngineSandbox/Source/Network`
(in-function phase), verified by code read against commit `4cf9d607`.

`ServerClientManager::LogConnectingClientDiagnostic`
(`Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp:147`)
runs once per connecting client (called from `NewClients`) and logs, all at
`kInfo`:

- one header line naming the client and its GUID (`:149`);
- the fleet owner count (`:150`);
- one line per fleet owner (`:153`);
- one line per cell that holds an owned entity (`:163`);
- one line per fleet-controlled ship row in those cells (`:172`).

The body's line count grows with the world, and `kInfo` is the default
threshold. `.agents/references/cpp-conventions.md` (log levels) puts one-time
connect logging at `kDebug` and reserves `kInfo` for state transitions and
important one-shots. The header line is a state transition and matches engine
practice (the engine logs the accepted handshake at `kInfo`); the dump body is
connect-time diagnostics.

Nothing records the dump as deliberate at `kInfo`, and no document or script
reads these lines: a repository search for "Connecting Client" and
"FleetGuids:" hits only this file. The effect is log volume at the default
threshold only.

## Design

In `LogConnectingClientDiagnostic`, change the level token of the four dump
lines (the fleet owner count, the per-owner line, the per-cell line, and the
per-row line) from `kInfo` to `kDebug`. The header line stays at `kInfo`.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp`

## In scope

- The level token of the four dump `LOG` lines in
  `ServerClientManager::LogConnectingClientDiagnostic`.

## Out of scope

- The header line, the message text and arguments of every line, and the
  function's loops.
- Every other log line in the file, including the constructor's
  spawn-candidate line and the client map fleet marker logs.

## Risk tier

Tier 2 — scoped behavior. Trigger: it changes what one server diagnostic
writes at the default log threshold. No determinism/CRC, wire, serialization,
replay, or threading exposure.

## Acceptance criteria

- Code read: the four dump lines log at `kDebug` and the header line at
  `kInfo`.
- `/compile` builds the BrokenEngineSandbox server.
- The repository static checks pass.

## Notes

`Documents/Plans/Game/FleetOwnedSquadronState.md` and
`FighterLaunchTerrainAdmissionNeedsElevationGrid.md` (this folder) edit
`SpawnFleetMember` in the same file, a different function; no ordering is
needed.
