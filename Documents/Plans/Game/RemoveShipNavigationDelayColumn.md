<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:09:06.236Z","dependsOn":[]} -->
# Remove the unread pNavigationDelays ship column

## Context

Paths are under `Projects/BrokenEngineSandbox/Source/` unless they start at
the repository root; `FC/` stands for `Frame/Collections/`. Line numbers are
from the commit that added this Plan; locate each site by symbol.

Originating gap: the architecture phase of a deep analysis of `FC/Fighters`
(its directory only), verified by a second reader. It is not an unmet
acceptance criterion of any change. The false condition, from
`Engine/Source/Frame/AGENTS.md` `## Simulation and Visual State`: "A row,
column, or value no simulation decision reads is visual and does not belong
there."

`pNavigationDelays` is a PostRender column of all three ship types
(`FC/Fighters/Fighters.h:261`, `FC/Battleships/Battleships.h:253`,
`FC/Omnis/Omnis.h:318`). It is in `SharedMembers()` and `SharedCrcMembers()`,
travels in every `SpawnInfo` as `navigationDelay`, and is range-checked by
each `PostRead`. No simulation decision reads it. Every site is carry,
validation, logging, or agent tooling:

- Fighters: `Fighters.h:217` (`PostRead`), `:277`, `:301`, `:319`, `:330`;
  `Fighters.cpp:385` (transfer payload), `:533` (spawn store), `:590` and
  `:730` (load and store with no use between), `:787` (`LogDifferences`).
- Battleships: `Battleships.h:210`, `:265`, `:289`, `:307`, `:318`;
  `Battleships.cpp:264`, `:388`, `:439`, `:548`, `:603`.
- Omnis: `Omnis.h:240`, `:328`, `:352`, `:376`, `:388`; `Omnis.cpp:322`,
  `:604`, `:786`, `:838`; `OmnisNavigation.cpp:41`.
- `FC/Ships/ShipStatusChanges.cpp:77` writes it from `kUpdateShip`; the
  behavior of that record is the `pCellChangeTimers` write on the next line.
- `Agent/Commands/ServerFrameEdit.cpp:21-23` lists it as an `edit_frame` and
  `read_frame` column; `Agent/Commands/ServerSimulationHarnessRigs.cpp:282-287`
  range-checks an injected spawn payload's `navigationDelay`.

No fleet spawn sets `SpawnInfo::navigationDelay`; the only assignments are the
three transfer sites, so a row holds the 60-second default unless a harness
rig changes it. The live delay is `Fleet::navigationDelaySeconds`
(`Fleet.h`), consumed by `Network/Server/FleetNavigationController.cpp` and
shown by `Ui/Screens/HudScreen.cpp:380`. The column is a remnant: in the
initial commit `e571f6f1` the HUD read the row column; that reader now reads
the fleet.

`Documents/Plans/Game/FleetOwnedSquadronState.md` records the same fact
("`pNavigationDelays` ... has no simulation reader") and lists the column
under its `## Out of scope`, so no existing Plan removes it.

## Design

The authoring agent's record of a user decision, carried over from a duplicate
Plan that this one replaced. That Plan came from an architecture review of the
Battleships collection, was added by commit `7dae9a27`, and covered the same
column, sites, and versions. It recorded that the user decided on 2026-11-14,
when its authoring session asked: remove the unread `pNavigationDelays` column
from all three ship types, and do not wire a per-row re-arm consumer. The
session that wrote this Plan did not ask the user; it reached the same removal
as its own recommendation.

Considered and declined, by the same carried-over record: reading the column
as a missing re-arm of the per-row cell-change timer. The fleet-level timer
re-arms from its own delay (`Network/Server/FleetNavigationController.cpp:127`,
`:175`, `:182`), while the per-row timer only counts down
(`UpdateFleetAndFlagshipNavigation`, `FC/Ships/ShipBehaviours.cpp:329-335`).

The steps are the author's recommended way to carry that decision out,
following the layout-change checklist of `/add-collection-member`
(`.agents/skills/add-collection-member/`) as the propagation reference for a
removed column. The three types change together to keep the
Fighters/Battleships mirror and the shared `Ships/` code parallel.

1. In `FightersPostRender`, `BattleshipsPostRender`, and `OmnisPostRender`:
   delete the `pNavigationDelays` member, its `SharedMembers()` and
   `SharedCrcMembers()` entries, its `PostRead` test, and the navigation-delay
   clause of the `PostRead` comment; delete `SpawnInfo::navigationDelay` and
   its `Fields()` entry.
2. Delete the carry lines in each type: the transfer payload initializer, the
   spawn store, the `Update` load and store, and the `LogDifferences` line.
3. `FC/Ships/ShipStatusChanges.cpp`: delete the `pNavigationDelays` write in
   the `kUpdateShip` arm. The `pCellChangeTimers` write and
   `UpdateShipData::navigationDelaySeconds` stay; that record still resets the
   cell-change timer.
4. `Agent/Commands/ServerFrameEdit.cpp`: remove the column from the three ship
   column lists (their `static_assert` against `Members()` fails until then).
5. `Agent/Commands/ServerSimulationHarnessRigs.cpp` `ParseSpawnEntry`: delete
   the `navigationDelay` range check. The same `if constexpr` block also holds
   the `IsShipHeadingUnit` check on a ship spawn's `vecDirection`, which must
   keep running for the three ship payloads only (a missile payload also has
   `vecDirection`). Recommended gate: `requires { rData.fleetWantedCoordinate; }`,
   the ship-only member the same function already tests. Update the function
   comment.
6. Versions: bump `FightersPostRender::kiVersion`,
   `BattleshipsPostRender::kiVersion`, and `OmnisPostRender::kiVersion` (column
   layout, which `Frame::kiVersion` composes); `FrameInput::kiVersion`
   (`Frame/FrameInput.h`; the `SpawnInfo` status-change payload); and
   `engine::kiProtocolVersion` (`Engine/Source/Network/NetworkProtocol.h`; the
   payload is wire-visible), with its version history entry in
   `Documents/Architecture/Network.md`.
7. Documentation, the `AGENTS.md` files through `/update-claude-docs`:
   `FC/Ships/AGENTS.md` `## Ship Invariants` (the `pNavigationDelays` bullet
   keeps its other `PostRead` rules); and
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`
   `inject_payload` (the three ship spawn field rows, the `navigationDelay`
   default example, the `UpdateShip` sentence that points at `read_frame`
   `pNavigationDelays`, and the `navigationDelay` range failure).

Stays: `UpdateShipData::navigationDelaySeconds` and its range check in the
`kUpdateShip` harness arm, `IsNavigationDelayInRange` (fleet admission and
that arm use it), `pCellChangeTimers`, and `Fleet::navigationDelaySeconds`
with all its fleet, wire, save, and HUD code.

Invariants: no simulation result changes, since nothing read the column; the
CRC changes only by losing that column; no backward compatibility, so older
peers, saves, and replays are rejected by the bumped versions.

Risk tier: Tier 3 — invariant/integration. Triggers: data layout and CRC (a
shared CRC'd column is removed), wire/protocol (the spawn and transfer
payload), and save/replay compatibility.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/OmnisNavigation.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipStatusChanges.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp`
- `Engine/Source/Network/NetworkProtocol.h`
- `Documents/Architecture/Network.md`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`

## In scope

- `pNavigationDelays`, `SpawnInfo::navigationDelay`, and every site
  `## Design` steps 1 and 2 name, in the three ship types.
- The `pNavigationDelays` write in the `kUpdateShip` arm of
  `FC/Ships/ShipStatusChanges.cpp`.
- The three ship column lists in `Agent/Commands/ServerFrameEdit.cpp`.
- The spawn-payload delay check, its gate, and the function comment in
  `ParseSpawnEntry` (`Agent/Commands/ServerSimulationHarnessRigs.cpp`).
- `FightersPostRender::kiVersion`, `BattleshipsPostRender::kiVersion`,
  `OmnisPostRender::kiVersion`, `FrameInput::kiVersion`,
  `engine::kiProtocolVersion`, and a protocol version history entry in
  `Documents/Architecture/Network.md`.
- The documentation regions `## Design` step 7 names, and any other
  `AGENTS.md` sentence `/update-claude-docs` finds that the removal makes
  false.

## Out of scope

- `UpdateShipData::navigationDelaySeconds`, the `kUpdateShip` record and its
  harness arm, and `IsNavigationDelayInRange`.
- `pCellChangeTimers` and every navigation behavior.
- Adding a per-row cell-change timer re-arm, adding a production `kUpdateShip`
  producer, or removing `kUpdateShip`.
- The documents the investigation notes in `## Notes` name.
- `Fleet::navigationDelaySeconds`, the fleet navigation-delay request, FleetSync,
  the grid save's fleet bytes, and the HUD slider.
- Any other column, and the Interpolate structs.
- Backward compatibility with older peers, saves, or replays.

## Acceptance criteria

1. Code read: no `pNavigationDelays` identifier and no `SpawnInfo`
   `navigationDelay` member remain under `Projects/BrokenEngineSandbox/Source`
   or in `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`.
   `UpdateShipData::navigationDelaySeconds` and `IsNavigationDelayInRange`
   remain.
2. Code read: the five versions in `## Design` step 6 are each above their
   values in the commit that added this Plan (12, 9, 26, 37, and 53), and
   `ParseSpawnEntry` still rejects a ship spawn whose `vecDirection` is not a
   horizontal unit vector while accepting a missile payload as before.
3. `/compile` builds the BrokenEngineSandbox client and server, and the
   repository static checks pass.
4. Harness (`/agent-harness`): a client connects to the server and fleet and
   random-spawn ships of all three types run, with a transfer between cells,
   without a frame CRC mismatch; an `inject_payload` `UpdateShip` entry still
   resets the row's `pCellChangeTimers` (`read_frame`); and `read_frame` of a
   ship collection no longer returns `pNavigationDelays`.
5. Harness: the replay determinism acceptance in
   `.agents/skills/agent-harness/references/replay.md`
   `### Replay determinism acceptance`, over a recording with a ship transfer,
   reports no mismatch.

The originating analysis was a code read and records no harness setup, so the
executing session builds the scenes for criteria 4 and 5.

## Notes

Other Plans bump the same versions or edit neighbouring code, with no ordering
required: `Documents/Plans/Game/FleetOwnedSquadronState.md`
(`engine::kiProtocolVersion`, `FC/Ships/AGENTS.md` `## Ship Invariants`),
`Documents/Plans/Game/OmniAnimationTimeClientOnly.md` (ship collection
versions, `FrameInput::kiVersion`), and
`Documents/Plans/Game/IncludeShipGlobalIdsInCrc.md` (the same
`SharedCrcMembers()` tuples). Each bump is relative to the tree the executing
session starts from; the session that lands second bumps again on merge.

Plans from other analyses overlap the same way, with no ordering required:
`Documents/Plans/Engine/RemoveIslandPlacementsFromStaticDataWire.md`
(`engine::kiProtocolVersion` and its `Documents/Architecture/Network.md`
entry), `Documents/Plans/Game/RemoveWriteOnlyMissileTargetFlags.md` (the
`kSpawnMissile` arm of `Agent/Commands/ServerSimulationHarnessRigs.cpp` and
the `SpawnMissile` text of `commands-server.md`), and
`Documents/Plans/Game/ShareShipPreCollision.md` (`PreCollision` in
`Omnis.cpp`).

Investigation notes carried over from the duplicate Plan `## Design` names.
They are unconfirmed; the executing session checks them and returns them as
residuals, and does not fix them here:

- A search for `kUpdateShip` and `UpdateShipData` finds one producer, the
  harness rig (`Agent/Commands/ServerSimulationHarnessRigs.cpp:410`), and one
  consumer (`FC/Ships/ShipStatusChanges.cpp:72`). `Frame/AGENTS.md` lists
  `kUpdateShip` among the consumed status changes, and the `ShipOrders`
  comment in `FC/Ships/ShipTypes.h:52-53` names it on the fleet path.
- A search for `pCellChangeTimers` writes finds the spawn draw in each type's
  `Spawn`, `ShipStatusChanges.cpp:78`, and the carried countdown. None of them
  re-arms an expired timer.

More Plans from the same analysis edit files this Plan edits. No ordering is
required; whichever lands second merges. Each name is a Plan under
`Documents/Plans/Game/`:

- `Fighters.cpp`: `ReturnDestinationReadsCentredIslands.md` and
  `Documents/Plans/Engine/ConsolidatedSmallDeduplications.md`.
- `Battleships.cpp`: `ReturnDestinationReadsCentredIslands.md` and
  `Documents/Plans/Engine/ConsolidatedSmallDeduplications.md`.
- `Omnis.cpp`: `ReturnDestinationReadsCentredIslands.md`.
- `Fighters.h` and `Battleships.h`:
  `Documents/Plans/Engine/ConsolidatedSmallDeduplications.md`.

A Plan from another analysis overlaps the same way, with no ordering required:
`Documents/Plans/Game/SyncFighterBeamHalfWidthParameter.md` edits
`Fighters.cpp`, `Omnis.cpp`, and `Fighters.h`.

## Coordination

`Documents/Plans/Engine/ConsolidatedSmallDeduplications.md` (its item 14)
edits the body and comment of `IsNavigationDelayInRange`, which this Plan
keeps, and leaves the `SpawnInfo::navigationDelay` defaults this Plan deletes
untouched. Neither Plan depends on the other. That Plan keeps the predicate's
name and signature. Whichever lands second re-derives its sites from the
landed tree: here, the predicate and its callers as they then read.
