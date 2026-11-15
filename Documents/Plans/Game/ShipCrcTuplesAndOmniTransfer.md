<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:09:06.236Z","dependsOn":[]} -->
# Ship CRC tuples, unread ship columns, and Omni transfer payload

## Context

Paths are under `Projects/BrokenEngineSandbox/Source/` unless they start at
the repository root; `FC/` stands for `Frame/Collections/`. Line numbers are
from the commits that recorded this work (the shield-member evidence from
commit `629ced2e`); locate each site by symbol.

Three changes to the same ship collection tuples, the Omni `SpawnInfo` and
Transfer literal, the `edit_frame` column lists, and the same three version
constants, run together so one build set, one version bump each, and one
replay determinism run with ship transfers cover them.

### Ship global IDs are left out of the shared CRC

Originating gap: the architecture phase of a deep analysis of `FC/Fighters`
(its directory only), verified by a second reader. It is not an unmet
acceptance criterion of any change. The false condition: every column a
simulation decision reads is in the CRC. When the analysis ran, the exclusion
also had no documented reason; it has one now, described below.

Each ship type's PostRender `SharedCrcMembers()` leaves out `pGlobalIds` and
`pClientGuids` under one comment (`FC/Fighters/Fighters.h:298-301`,
`FC/Battleships/Battleships.h:286-289`, `FC/Omnis/Omnis.h:349-352`): "CRC-only
subset: omits pClientGuids, which is server-local, and pGlobalIds, which the
tick reads but Spawn sets identically on client and server". Commit `7dae9a27`
wrote that wording; before it the comment called both columns "server-side
bookkeeping". The current comment states the reason for the `pGlobalIds`
exclusion this Plan removes. Its two facts about `pGlobalIds` are true, and
they are the case for the CRC, not against it: a column the tick reads, and
that is meant to be equal on both sides, is what the CRC exists to verify.

- It is a simulation input. Readers: `FC/Fighters/Fighters.cpp:628`
  (`ChooseFighterFleetTactic`); `FC/Ships/FleetCoordination.cpp:145` (escort
  slot), `:184`, `:188` (shared-target leader and kept-target holder), `:225`
  (`FleetBoard` takes the flagship ID from Battleships `pGlobalIds`);
  `FC/Ships/ShipBehaviours.cpp:344` (path-recompute cadence
  `(iTick + globalId) % kiNavigationRecomputeInterval`, all three types),
  `:243`, `:252` (Omni Patrol Together leader); `FC/Ships/ShipQueries.cpp:20`
  (`FindShipByGlobalId`).
- The client reads it too (`Ui/Screens/HudScreen.cpp:71`).

Client values equal the server's by construction, so nothing legitimate makes
the two sides differ: the column is in `SharedMembers()` (`Fighters.h:277`,
read by `ServerRead`) and `PersistentMembers()` (`:295`); `globalId` is in
`SpawnInfo::Fields()` (`:330`), the list
`Engine/Source/Network/StatusChangeFields.h` serializes; the only simulation
write is `Spawn` (`Fighters.cpp:528`); and `RegistryOwnershipLayer` holds the
column `const` (`Engine/Source/Frame/FrameRegistry.h:52`).

Authority: `Engine/Source/Frame/AGENTS.md` `## Simulation and Visual State`
says simulation state is "CRC'd through `SharedCrcMembers()` where a
collection declares one", and its `## Frame Registry` names only the
client-GUID column as CRC-excluded. `FC/Omnis/AGENTS.md` `## State Boundaries`
states that the shared CRC excludes "server-assigned global identity, which
simulation reads" (commit `d87ad840` added the last three words) but gives no
reason; no other `AGENTS.md` mentions the exclusion.
`git log -S"server-side bookkeeping"` reaches the initial commit `e571f6f1`,
so the exclusion itself is inherited, not a recorded decision; only the
comment's reason is new.

Effect today: a wrong global ID (a wire, spawn-payload, or agent `edit_frame`
bug) is not caught by the CRC and shows up later as a position or target
mismatch. No such bug was observed.

`pClientGuids` is different and correctly excluded: the server rewrites it
after the tick through `AssignRegistryClientGuid`, and client rows created
from a spawn or transfer status change hold zero.

`Documents/Investigations/Engine/ResolveShipGlobalIdCrcExclusion.md` is the
options record that the decision in `## Design` resolves; this work is its
option 2.

### The unread pNavigationDelays ship column

Originating gap: the same `FC/Fighters` architecture analysis, verified by a
second reader; not an unmet acceptance criterion of any change. The false
condition, from `Engine/Source/Frame/AGENTS.md`
`## Simulation and Visual State`: "A row, column, or value no simulation
decision reads is visual and does not belong there."

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
under its `## Out of scope`, so no other Plan removes it.

### The Omni animation time is simulation state

`OmnisInterpolate::pAnimationTimes` is listed in `SharedMembers()`
(`Omnis.h:155`), so it is saved, replayed, and sent in full state, and it is
carried in the transfer payload as `OmnisPostRender::SpawnInfo::animationTime`
(`Omnis.h:365`, its `Fields()` entry at `:383`; the source Transfer literal at
`OmnisNavigation.cpp:38`; the destination store at `Omnis.cpp:274`).
`SharedCrcMembers()` already excludes it as "client-only animation time"
(`Omnis.h:180-183`).

`Engine/Source/Frame/AGENTS.md` `## Simulation and Visual State` says a column
no simulation decision reads is visual and does not belong in simulation
state. Its one exception is a client-consumed value that must cross cells with
its entity, which travels in the transfer payload and so is simulation state.

Verified from code, the exception does not apply:

- No simulation decision reads the column. Its only readers are Interpolate
  `Update`, which carries it (`Omnis.cpp:351`, `:382`), the client-only advance
  (`Omnis.cpp:407-413`), the Transfer literal, and the client render
  (`OmnisRender.cpp:225`).
- Only the client advances it: the server's Interpolate `Update` stores the
  previous value unchanged, so on the server every row keeps its spawn value,
  the `SpawnInfo` default of zero.
- Only the server consumes `transferRequests`
  (`Engine/Source/Network/Server/ServerTransferManager.cpp`), so the value that
  crosses a cell transfer is the server's never-advanced value, not the
  client's animation clock. The transfer therefore does not preserve what the
  client shows; a full state likewise overwrites the client's advanced value
  with the server's.
- Fighters and Battleships keep their `pAnimationTimes` in `ClientMembers()`
  under `BT_CLIENT` (`Fighters.h:118`, `:133`; `Battleships.h:116`, `:125`),
  and Fighters' `Spawn` zeroes it on every spawn, transfers included
  (`Fighters.cpp:498`).

So the code holds a visual column in simulation state, against the documented
rule and against the Fighters and Battleships pattern. Originating gap: a
user-requested investigation of client-only state in the game Frame code; the
user approved this change as a follow-up Plan.

### Never-set Omni shield members

`OmnisPostRender::SpawnInfo` declares `fShieldRotation` (`Omnis.h:371`,
default `0.0f`) and `fShieldShrink` (`:372`, default `1.0f`). Nothing assigns
either: a search of `Projects/BrokenEngineSandbox` finds no write, and the
Transfer literal (`OmnisNavigation.cpp:24-49`) names neither. Neither is in
`Fields()` (`Omnis.h:388`), so neither travels in a wire status-change
payload. Their only reads are `Omnis.cpp:287-288`, under `BT_CLIENT`, which
store them into the client-only columns `pfShieldRotations` and
`pfShieldShrinks`. So every spawn and every transfer arrival stores the two
defaults, and the members are configuration no caller can use.

Both members are still bytes in a replay. They are declared in both builds,
outside any `BT_CLIENT` guard, and the replay difference stream writes every
`SpawnInfo` as a raw image
(`Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:198`):
`WriteFrameInput` (`Engine/Source/Frame/FrameBase.cpp:352-361`) passes each
payload to `common::Write`, which writes `sizeof(T)` bytes
(`Common/Serialization.h:99-103`). Removing them changes the Omni spawn and
transfer payload image a server build writes, which
`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` `## Invariants` guards
only by a `FrameInput::kiVersion` bump: the difference stream checks no
payload size. Their removal alone changes no wire payload, since `Fields()`
lists neither.

Originating gap: the architecture phase (client/server and data shape lens)
of a findings-only deep analysis of
`Projects/BrokenEngineSandbox/Source/Frame/Collections/` (recursive),
confirmed by an independent verification read. Unmet criterion: `SpawnInfo`
holds no member that nothing sets.

## Design

Decisions, as the authoring agents recorded them:

- Global IDs: the user chose to put `pGlobalIds` into the shared CRC of all
  three ship types, with the version bump. Keeping the column out and
  correcting only the comments and documents was the alternative, and it was
  not chosen. Contradiction, named so it is not resolved silently: commit
  `7dae9a27`, a parallel change, carried out the comment half of that
  alternative, so the current comment records the opposite position (the
  column stays out because both sides set it identically). Under the authority
  order in root `AGENTS.md` `### Diagnosis Discipline` the recorded user choice
  outranks a code comment, so this Plan trusts the choice and rewrites the
  comment. Why the column belongs there: it is a simulation input with no
  legitimate client/server difference, which is what the engine rule puts in
  the CRC. "Equal by construction" describes every CRC'd column and so is no
  reason to leave one out.
- Navigation delay: a record carried over from a duplicate Plan (from an
  architecture review of the Battleships collection, added by commit
  `7dae9a27`, covering the same column, sites, and versions) says the user
  decided on 2026-11-14: remove the unread `pNavigationDelays` column from all
  three ship types, and do not wire a per-row re-arm consumer. The session
  that wrote the removal reached the same conclusion as its own
  recommendation. Considered and declined by the same record: reading the
  column as a missing re-arm of the per-row cell-change timer. The fleet-level
  timer re-arms from its own delay
  (`Network/Server/FleetNavigationController.cpp:127`, `:175`, `:182`), while
  the per-row timer only counts down (`UpdateFleetAndFlagshipNavigation`,
  `FC/Ships/ShipBehaviours.cpp:329-335`).
- Omni animation time: move the column to client-only state, matching
  Fighters and Battleships and the documented rule, rather than making the
  server advance a visual clock or documenting a new exception.

The steps are the authors' recommended way to carry those decisions out. The
three ship types change together to keep the Fighters/Battleships mirror and
the shared `Ships/` code parallel. Apply the layout-change checklist of
`/add-collection-member` (`.agents/skills/add-collection-member/`) in reverse
as the propagation reference for each removed shared column.

1. Ship PostRender tuples, in `FightersPostRender`, `BattleshipsPostRender`,
   and `OmnisPostRender`:
   - Add `rSelf.pGlobalIds` to the `SharedCrcMembers()` tuple at its
     `SharedMembers()` position (before `pFlagshipGlobalIds`).
   - Delete the `pNavigationDelays` member, its `SharedMembers()` and
     `SharedCrcMembers()` entries, its `PostRead` test, and the
     navigation-delay clause of the `PostRead` comment; delete
     `SpawnInfo::navigationDelay` and its `Fields()` entry.
   - In the comment above each `SharedCrcMembers()` tuple, delete the
     `pGlobalIds` clause ("and pGlobalIds, which the tick reads but Spawn sets
     identically on client and server"), which the column's inclusion makes
     false. The `pClientGuids` clause and its reason ("server-local") stay, so
     the comment names `pClientGuids` as the one omitted column.
2. Navigation-delay carry lines in each ship type: delete the transfer payload
   initializer, the spawn store, the `Update` load and store, and the
   `LogDifferences` line.
3. `FC/Ships/ShipStatusChanges.cpp`: delete the `pNavigationDelays` write in
   the `kUpdateShip` arm. The `pCellChangeTimers` write and
   `UpdateShipData::navigationDelaySeconds` stay; that record still resets the
   cell-change timer.
4. Omni animation time, in `OmnisInterpolate`:
   - Declare `pAnimationTimes` inside the `BT_CLIENT` block and move its tuple
     entry from `SharedMembers()` to `ClientMembers()`. Update the
     `SharedCrcMembers()` comment, which no longer needs to name animation
     time.
   - Move the load and store in `OmnisInterpolate::Update` under `BT_CLIENT`,
     keeping the client advance.
5. Omni `SpawnInfo`, Transfer literal, and `Spawn`:
   - Remove `SpawnInfo::animationTime`, its `Fields()` entry, and the Transfer
     literal entry in `OmnisNavigation.cpp` (the `navigationDelay` entry goes
     in step 2). In `OmnisPostRender::Spawn`, set `pAnimationTimes` to zero
     under `BT_CLIENT`, as Fighters does, and drop "and animation time" from
     the `bTransfer` comment.
   - Delete `SpawnInfo::fShieldRotation` and `fShieldShrink`. In
     `OmnisPostRender::Spawn`, the two `BT_CLIENT` stores into
     `pfShieldRotations` and `pfShieldShrinks` store `0.0f` and `1.0f`, the
     values the defaults supply today. Reword the `bTransfer` comment and the
     `Fields()` comment so neither names the removed members.
     `Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md`
     `## Game-Specific Rules` stays true: client-only shield animation state
     still starts from canonical client defaults on a transfer arrival.
6. `Agent/Commands/ServerFrameEdit.cpp`: remove `pNavigationDelays` from the
   three ship column lists and the Omni animation time column from the Omnis
   list (the server build's `static_assert` against `Members()` fails until
   then).
7. `Agent/Commands/ServerSimulationHarnessRigs.cpp` `ParseSpawnEntry`: delete
   the `navigationDelay` range check. The same `if constexpr` block also holds
   the `IsShipHeadingUnit` check on a ship spawn's `vecDirection`, which must
   keep running for the three ship payloads only (a missile payload also has
   `vecDirection`). Recommended gate:
   `requires { rData.fleetWantedCoordinate; }`, the ship-only member the same
   function already tests. Update the function comment.
8. Versions, one bump each:
   - `FightersPostRender::kiVersion`, `BattleshipsPostRender::kiVersion`,
     `OmnisPostRender::kiVersion` (the removed column), and
     `OmnisInterpolate::kiVersion` (the removed shared animation column); these
     are column layout, which `Frame::kiVersion` composes.
   - The base term of `Frame::kiVersion` (`Frame/Frame.cpp`), for the CRC
     semantics change of adding `pGlobalIds`, under `Frame/AGENTS.md`
     `## Invariants`.
   - `FrameInput::kiVersion` (`Frame/FrameInput.h`): the `SpawnInfo`
     status-change payload and its raw replay image (`navigationDelay`,
     `animationTime`, and the two shield members).
   - `engine::kiProtocolVersion` (`Engine/Source/Network/NetworkProtocol.h`;
     the spawn and transfer payload is wire-visible, under
     `Projects/BrokenEngineSandbox/Source/Network/AGENTS.md`
     `## Status-Change Wire Format`), with its version history entry in
     `Documents/Architecture/Network.md`.
9. Documentation, the `AGENTS.md` files through `/update-claude-docs`:
   - `FC/Omnis/AGENTS.md` `## State Boundaries`, first bullet: stop listing
     global identity as CRC-excluded, its "which simulation reads" clause
     included, and name the client GUID instead; stop naming client animation
     time among the CRC-excluded simulation state.
   - `FC/Ships/AGENTS.md` `## Ship Invariants`: the `pNavigationDelays`
     bullet goes; its other `PostRead` rules stay.
   - `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`
     `inject_payload`: the three ship spawn `navigationDelay` field rows, the
     `navigationDelay` default example, the `UpdateShip` sentence that points
     at `read_frame` `pNavigationDelays`, the `navigationDelay` range failure,
     and `animationTime` in the `SpawnOmni` row.

Stays: `UpdateShipData::navigationDelaySeconds` and its range check in the
`kUpdateShip` harness arm, `IsNavigationDelayInRange` (fleet admission and
that arm use it), `pCellChangeTimers`, and `Fleet::navigationDelaySeconds`
with all its fleet, wire, save, and HUD code.

Behavior change: an Omni's animation clock becomes purely client-local. It
restarts at zero on every spawn and transfer arrival, which the server's
never-advanced transfer value already causes today. After a full state it
takes the same handling as Fighters' and Battleships' client-only animation
time, instead of the server's value. No simulation result changes.

Invariants:

- The three ship types change together; `SharedCrcMembers()` stays a subset
  of `SharedMembers()`; `pClientGuids` stays out of the CRC; `LogDifferences`
  already compares `pGlobalIds` and that line is unchanged.
- No simulation result changes: nothing read `pNavigationDelays`, the server
  never advanced the Omni animation time, and the shield members are read only
  by two client-only stores. The shared CRC changes only by gaining
  `pGlobalIds` and losing `pNavigationDelays`; the Omni animation time was
  never in it.
- No backward compatibility: older peers, saves, and replays are rejected by
  the bumped versions.

Risk tier: Tier 3 — invariant/integration. Triggers: determinism/CRC (the set
of CRC'd columns changes), data layout (shared columns are removed), wire and
protocol (the spawn and transfer payload), and save/replay compatibility (the
version bumps and the raw `SpawnInfo` replay image).

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/OmnisNavigation.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipStatusChanges.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp`
- `Engine/Source/Network/NetworkProtocol.h`
- `Documents/Architecture/Network.md`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`

## In scope

- Ship PostRender structs (`FightersPostRender`, `BattleshipsPostRender`,
  `OmnisPostRender`): the `SharedCrcMembers()` tuple and the comment above it;
  `pNavigationDelays`, `SpawnInfo::navigationDelay`, and every site
  `## Design` steps 1 and 2 name.
- The `pNavigationDelays` write in the `kUpdateShip` arm of
  `FC/Ships/ShipStatusChanges.cpp`.
- `OmnisInterpolate::pAnimationTimes`: declaration, guard, and its
  `SharedMembers()`/`ClientMembers()` entries; the `OmnisInterpolate`
  `SharedCrcMembers()` comment; the column's load and store in
  `OmnisInterpolate::Update`.
- `OmnisPostRender::SpawnInfo::animationTime`, `fShieldRotation`, and
  `fShieldShrink`; the `animationTime` `Fields()` entry and the `Fields()`
  comment that names the shield members; the `bTransfer` comment;
  `OmnisPostRender::Spawn`'s animation-time initialization and its two shield
  stores; the Omnis Transfer `SpawnInfo` literal in `OmnisNavigation.cpp`.
- The ship column lists in `Agent/Commands/ServerFrameEdit.cpp`.
- The spawn-payload delay check, its gate, and the function comment in
  `ParseSpawnEntry` (`Agent/Commands/ServerSimulationHarnessRigs.cpp`).
- Versions: `FightersPostRender::kiVersion`, `BattleshipsPostRender::kiVersion`,
  `OmnisPostRender::kiVersion`, `OmnisInterpolate::kiVersion`, the base term of
  `Frame::kiVersion`, `FrameInput::kiVersion`, `engine::kiProtocolVersion`,
  and a protocol version history entry in `Documents/Architecture/Network.md`.
- The documentation regions `## Design` step 9 names, and any other
  `AGENTS.md` sentence `/update-claude-docs` finds that the change makes false.

## Out of scope

- `pClientGuids`: its CRC exclusion, its `SharedMembers()` membership, and
  whether sending every owner's client GUID to subscribed clients is wanted.
- `pGlobalIds` beyond its `SharedCrcMembers()` entry: its `SharedMembers()`,
  `PersistentMembers()`, and `SpawnInfo` membership, and every reader.
- The Interpolate `SharedCrcMembers()` tuples of every type (only the
  `OmnisInterpolate` comment above it changes).
- `LogDifferences` in every type, except deleting each `pNavigationDelays`
  line.
- `UpdateShipData::navigationDelaySeconds`, the `kUpdateShip` record and its
  harness arm, and `IsNavigationDelayInRange`.
- `pCellChangeTimers` and every navigation behavior.
- Adding a per-row cell-change timer re-arm, adding a production `kUpdateShip`
  producer, or removing `kUpdateShip`.
- The documents the investigation notes in `## Notes` name.
- `Fleet::navigationDelaySeconds`, the fleet navigation-delay request,
  FleetSync, the grid save's fleet bytes, and the HUD slider.
- The `pfShieldRotations` and `pfShieldShrinks` columns and their Update
  (`Omnis.cpp:400-405`).
- Every other `SpawnInfo` member, and every `Fields()` entry except
  `navigationDelay` and the Omni `animationTime`; the `.flags` member of the
  Omni Transfer literal and the `spawnFlags` block of `OmnisPostRender::Spawn`.
- `OmnisPostRender::pDestroyedExplosionTimes`,
  `OmnisInterpolate::pfRotationAccelerationXs` and `pfRotationAccelerationYs`,
  any other column, and any other client-only column or `BT_CLIENT` guard
  change.
- Fighters and Battleships `pAnimationTimes`, already client-only.
- A separate client Frame object, per-collection client headers, client-only
  source files, or extracting inline effect blocks into functions.
- Any edit to `Engine/Source/Frame/AGENTS.md`; its rule already describes the
  intended state.
- Backward compatibility with older peers, saves, or replays.

## Acceptance criteria

1. Code read: no ship `SharedCrcMembers()` comment names `pGlobalIds` as
   omitted, and each still names `pClientGuids` as the omitted column with its
   reason; each of the three PostRender `SharedCrcMembers()` tuples holds
   `pGlobalIds`.
2. Code read: no `pNavigationDelays` identifier and no `SpawnInfo`
   `navigationDelay` member remain under `Projects/BrokenEngineSandbox/Source`
   or in `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`.
   `UpdateShipData::navigationDelaySeconds` and `IsNavigationDelayInRange`
   remain.
3. Code read: `OmnisInterpolate::SharedMembers()` and
   `OmnisPostRender::SpawnInfo::Fields()` no longer name the animation time,
   and the column is declared and used only under `BT_CLIENT`. No
   `fShieldRotation` or `fShieldShrink` member remains in `SpawnInfo`, and the
   Omni `Fields()` loses only its `animationTime` and `navigationDelay`
   entries.
4. Code read: versions are each above their values in the tree the session
   starts from — `FightersPostRender::kiVersion`,
   `BattleshipsPostRender::kiVersion`, `OmnisPostRender::kiVersion`,
   `FrameInput::kiVersion`, and `engine::kiProtocolVersion` (12, 9, 26, 37,
   and 53 when recorded), `OmnisInterpolate::kiVersion`, and the
   `Frame::kiVersion` base (188 when recorded).
5. Code read: `ParseSpawnEntry` still rejects a ship spawn whose
   `vecDirection` is not a horizontal unit vector while accepting a missile
   payload as before.
6. `FC/Omnis/AGENTS.md` `## State Boundaries` no longer lists global identity
   or client animation time as CRC-excluded.
7. `/compile` builds the BrokenEngineSandbox client and server, the
   repository static checks pass, and the collection-layout auditor run
   `/add-collection-member` documents is clean.
8. Harness (`/agent-harness`), one scenario: a connected client and server
   with fleet and random-spawn ships of all three types, run long enough for
   Fighters to launch and for ships, Omnis included, to transfer between
   cells, report no frame CRC mismatch; the client shows Omnis animating; an
   `inject_payload` `UpdateShip` entry still resets the row's
   `pCellChangeTimers` (`read_frame`); and `read_frame` of a ship collection
   no longer returns `pNavigationDelays`.
9. Harness: the replay determinism acceptance in
   `.agents/skills/agent-harness/references/replay.md`
   `### Replay determinism acceptance`, over a recording made on the changed
   build of that scenario (with ship and Omni transfers), reports no
   mismatch.

The originating analyses were code reads and record no harness setup, so the
executing session builds the scene for criteria 8 and 9.

## Notes

Investigation notes about the navigation delay, carried over from the
duplicate Plan `## Design` names. They are unconfirmed; the executing session
checks them and returns them as residuals, and does not fix them here:

- A search for `kUpdateShip` and `UpdateShipData` finds one producer, the
  harness rig (`Agent/Commands/ServerSimulationHarnessRigs.cpp:410`), and one
  consumer (`FC/Ships/ShipStatusChanges.cpp:72`). `Frame/AGENTS.md` lists
  `kUpdateShip` among the consumed status changes, and the `ShipOrders`
  comment in `FC/Ships/ShipTypes.h:52-53` names it on the fleet path.
- A search for `pCellChangeTimers` writes finds the spawn draw in each type's
  `Spawn`, `ShipStatusChanges.cpp:78`, and the carried countdown. None of them
  re-arms an expired timer.

## Coordination

No directional dependencies. Overlaps with no ordering required; whichever
lands second merges, re-deriving its sites and version bumps from the landed
tree:

- `Documents/Plans/Game/ClearOmniFireBlasterOnTransferArrival.md` edits the
  same Omni Transfer `SpawnInfo` literal in `OmnisNavigation.cpp` (its
  `.flags` member) or the `spawnFlags` block of `OmnisPostRender::Spawn`. It
  stays a separate Plan because it changes simulation results, so a replay
  mismatch could not be attributed between it and this change.
- `Documents/Plans/Game/FleetOwnedSquadronState.md`:
  `engine::kiProtocolVersion` and `FC/Ships/AGENTS.md` `## Ship Invariants`.
- `Documents/Plans/Engine/RemoveIslandPlacementsFromStaticDataWire.md`:
  `engine::kiProtocolVersion` and its `Documents/Architecture/Network.md`
  entry.
- `Documents/Plans/Game/MissileTransferCleanup.md`: the `kSpawnMissile` arm of
  `Agent/Commands/ServerSimulationHarnessRigs.cpp` and the `SpawnMissile` text
  of `commands-server.md`.
- `Documents/Plans/Game/ShipCombatHelpers.md`: `PreCollision` in `Omnis.cpp`,
  and `Fighters.cpp`, `Omnis.cpp`, and `Fighters.h`.
