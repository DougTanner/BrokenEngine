<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:11:30.857Z","dependsOn":[]} -->
# Remove the unread ship navigation delay column

## Context

Paths are under `Projects/BrokenEngineSandbox/Source/Frame/Collections/` unless
they start elsewhere. Verified against commit `022915b6`.

`pNavigationDelays` is a `SharedMembers()` and `SharedCrcMembers()` column of
all three ship PostRender collections (`Battleships/Battleships.h:251`,
`Fighters/Fighters.h:259`, `Omnis/Omnis.h:313`), so it is saved, replayed,
sent, and CRC'd. No simulation decision reads it. Every use is one of:

- written at spawn from `SpawnInfo::navigationDelay`
  (`Battleships/Battleships.cpp:387`, `Fighters/Fighters.cpp:536`,
  `Omnis/Omnis.cpp:322`) and by `kUpdateShip`
  (`Ships/ShipStatusChanges.cpp:77`);
- carried from the previous row to the current row unchanged
  (`Battleships.cpp:438`, `:549`; `Fighters.cpp:593`, `:735`; `Omnis.cpp:604`,
  `:786`);
- copied into the Transfer `SpawnInfo` literal (`Battleships.cpp:263`,
  `Fighters.cpp:388`, `Omnis/OmnisNavigation.cpp:42`);
- range-checked in `PostRead` (`Battleships.h:208-211`, `Fighters.h:215-218`,
  `Omnis.h:235-238`);
- compared in `LogDifferences` (`Battleships.cpp:606`, `Fighters.cpp:794`,
  `Omnis.cpp:838`);
- exposed to the harness `edit_frame` and `read_frame` column lists
  (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp:21-23`).

The per-row timer beside it, `pCellChangeTimers`, only counts down and never
reads the delay: `UpdateFleetAndFlagshipNavigation` subtracts the tick time
while the row roams and switches to mode 6 at zero
(`Ships/ShipBehaviours.cpp:329-335`). No production code sets
`SpawnInfo::navigationDelay` (default 60 seconds, `Battleships.h:305`,
`Fighters.h:317`, `Omnis.h:371`); only the three Transfer literals and the
harness `inject_payload` spawn fields do.

Rule broken: `Engine/Source/Frame/AGENTS.md` `## Simulation and Visual State`,
first bullet — a column no simulation decision reads does not belong in
`SharedMembers()`. `Documents/Plans/Game/FleetOwnedSquadronState.md` records
the same finding in its resolved inventory and leaves the column out of its
scope.

Originating gap: an architecture review of the Battleships collection; unmet
criterion "every simulation column has a simulation reader".

## Design

Decided by the user on 2026-11-14, when the authoring session asked: remove the
unread `pNavigationDelays` column from all three ship types, and do not wire a
per-row re-arm consumer. The rule above supports removal, and nothing reads the
value.

Considered and declined by the user: reading the column as a missing re-arm of
the per-row cell-change timer. The fleet-level timer does re-arm from its own
delay
(`Projects/BrokenEngineSandbox/Source/Network/Server/FleetNavigationController.cpp:127`,
`:175`, `:182`) while the per-row timer never does.

Removal, applied to all three ship types together:

1. Delete the `pNavigationDelays` declaration, its `SharedMembers()` and
   `SharedCrcMembers()` entries, and its `PostRead` range check.
2. Delete `SpawnInfo::navigationDelay` and its `Fields()` entry, the Transfer
   literal entry, the `Spawn` write, the `Update` load and store, and the
   `LogDifferences` line.
3. `Ships/ShipStatusChanges.cpp:77`: delete the `pNavigationDelays` write.
   Keep line 78 and `UpdateShipData::navigationDelaySeconds`, which resets
   `pCellChangeTimers`.
4. `ServerFrameEdit.cpp:21-23`: remove the column from the three column lists
   (the lists' `static_assert` against `Members()` enforces it).
5. `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp:281-291`:
   the `requires { rData.navigationDelay; }` test gates both the delay range
   check and the ship heading check. Delete the delay check and key the block
   on `rData.fleetWantedCoordinate`, the ship-only `SpawnInfo` member the same
   function already tests at line 275, so the heading check still runs for the
   three ship spawns only. Drop the navigation delay from the function's
   comment (lines 252-253).
6. Keep `IsNavigationDelayInRange` (`Ships/ShipTypes.h:184`; it moved there
   from `Ships/ShipBehaviours.h` after `022915b6`): the `UpdateShip` harness
   check (`ServerSimulationHarnessRigs.cpp:416`) and the fleet-level delay
   admissions still call it.
7. Documents: `Ships/AGENTS.md` `## Ship Invariants`, the bullet that starts
   "Every ship type's `pNavigationDelays`" (lines 92-95 name the column and
   the `PostRead` rejection); and
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`:
   `navigationDelay` in the three ship spawn rows (235-237), the default
   sentence (248), the `read_frame` `pNavigationDelays` sentence (275-278), and
   the `[0,60]` failure sentence (289), keeping `navigationDelaySeconds`.
8. Versions: bump `BattleshipsPostRender::kiVersion` (`Battleships.h:169`),
   `FightersPostRender::kiVersion` (`Fighters.h:177`),
   `OmnisPostRender::kiVersion` (`Omnis.h:208`), `FrameInput::kiVersion`
   (`Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h:10`), and
   `engine::kiProtocolVersion` (`Engine/Source/Network/NetworkProtocol.h:83`).

Apply `/add-collection-member`'s layout checklist in reverse for the removed
column.

Investigation notes, unconfirmed, for the executing session to check and report
rather than fix here:

- A search for `kUpdateShip` and `UpdateShipData` finds one producer, the
  harness rig (`ServerSimulationHarnessRigs.cpp:409`), and one consumer
  (`ShipStatusChanges.cpp:72`).
  `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` lists `kUpdateShip`
  among the consumed status changes, and the `ShipOrders` comment in
  `Ships/ShipTypes.h:52-53` names it on the fleet path.
- A search for `pCellChangeTimers` writes finds the spawn draw
  (`Battleships.cpp:388` and its Fighters and Omnis parallels),
  `ShipStatusChanges.cpp:78`, and the carried countdown. An expired timer is
  not re-armed by any of them.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/OmnisNavigation.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipStatusChanges.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`
- `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h` (version)
- `Engine/Source/Network/NetworkProtocol.h` (version)

## In scope

- `pNavigationDelays` in `BattleshipsPostRender`, `FightersPostRender`, and
  `OmnisPostRender`: declaration, `SharedMembers()` and `SharedCrcMembers()`
  entries, `PostRead` check, `Spawn` write, `Update` load and store, and
  `LogDifferences` line.
- `SpawnInfo::navigationDelay` and its `Fields()` entry in the three types, and
  the three Transfer `SpawnInfo` literals.
- The `pNavigationDelays` write in the `kUpdateShip` branch of
  `ShipStatusChanges.cpp`.
- The three ship column lists in `ServerFrameEdit.cpp`, and the
  `navigationDelay` gate, range check, and comment mention in the spawn-entry
  parser of `ServerSimulationHarnessRigs.cpp`.
- The `Ships/AGENTS.md` bullet and the `commands-server.md` sentences named in
  Design step 7.
- The three PostRender `kiVersion`s, `FrameInput::kiVersion`, and
  `engine::kiProtocolVersion`.

## Out of scope

- `pCellChangeTimers`, `UpdateShipData::navigationDelaySeconds`, and
  `IsNavigationDelayInRange`.
- `Fleet::navigationDelaySeconds`, the fleet-level timer, and every fleet
  request, save, and HUD site that carries the fleet delay.
- Adding a per-row timer re-arm, or a production `kUpdateShip` producer.
- Removing `kUpdateShip`, or editing the documents the investigation notes
  name.
- Backward compatibility with older saves, replays, or peers.

## Risk tier

Tier 3 — invariant/integration. Triggers: serialization and data layout (a
`SharedMembers()` column leaves the snapshot, save, and replay layout), CRC (it
leaves `SharedCrcMembers()`), and wire/protocol (a `SpawnInfo` `Fields()` entry
leaves the spawn and transfer status-change payload). The version bumps in
Design step 8 reject older saves, replays, and peers.

## Acceptance criteria

- No `pNavigationDelays` or `SpawnInfo::navigationDelay` identifier remains in
  source code (`.h` and `.cpp` files) or in the two documents Design step 7
  names, and `UpdateShipData::navigationDelaySeconds` still resets
  `pCellChangeTimers`.
- The five versions are each above their values at `022915b6` (9, 12, 26, 37,
  and 53).
- The client and server build, and the collection-layout check
  `/add-collection-member` documents is clean.
- A live client and server run with fleet and random-spawn ships of all three
  types, including a cross-cell transfer, reports no CRC mismatch, and a replay
  determinism check passes. The supplied evidence records no harness setup; the
  executing session builds one through `/agent-harness`.

## Notes

The executing session returns the investigation notes as residuals.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
