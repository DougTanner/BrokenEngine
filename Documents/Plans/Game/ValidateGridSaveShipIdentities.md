<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T18:25:55.502Z","dependsOn":[]} -->
# Validate ship global IDs across a grid save before adoption

## Context

Root `AGENTS.md` (trust policy) says the server fully validates every grid save under the bad-value
rule in `.agents/references/cpp-conventions.md`; `Engine/Source/File/AGENTS.md` `## Grid Saves`
makes `ReadGridSave` the boundary that bounds, cross-checks, and rejects bad adopted values. A grid
save's fleet records and its ship rows are each checked on their own, but nothing checks them
against each other or checks row global IDs across cells. A `/next-plan` run on
`ShipCombatAndFleetCleanups` proved this gap pre-existing (baseline `8fa794fa` behaves the same) and
outside that change.

Line numbers below were taken from the tree when this Plan was written; locate each site by the
named symbol if lines have moved. Paths without a directory are under
`Projects/BrokenEngineSandbox/Source/`.

Root cause, read from source:

- `engine::ReadGridSave` (`Engine/Source/File/GridSave.cpp:69-160`) reads the counter
  `iNextGlobalId`, then the game half through `game::ReadSaveState`, then every cell's frame, and
  checks only the counter's sign, duplicate coordinates, the frame clocks, and the client grid
  coordinate.
- `ReadFleetData` (`Network/Server/ServerFleetSerialization.cpp:165-225`) checks fleet records only:
  a nonzero owner GUID (line 189) and member global IDs positive, below `iNextGlobalId`, and unique
  across every owner (line 208). `ReadFleet` (same file, lines 127-132) requires exactly one
  Battleship member, named by the flagship.
- The per-frame row checks in each ship type's PostRender `PostRead`
  (`Frame/Collections/Fighters/Fighters.h:178-197`, `Frame/Collections/Omnis/Omnis.h:218-241`,
  `Frame/Collections/Battleships/Battleships.h:175-194`) reject a nonzero `pClientGuids` or
  `pGlobalIds` only on a row that is not `ShipController::kFleet`. Nothing checks that a nonzero row
  global ID is unique across rows and cells, lies below `iNextGlobalId`, or, when it names a fleet
  member, sits on a row of that member's ship type carrying that member's owner GUID.

Effect, read from source:

- `FindShipByGlobalId` (`Frame/Collections/Ships/ShipQueries.cpp:8-27`) returns the first row whose
  `pGlobalIds` matches, walking Fighters, then Omnis, then Battleships.
- `ServerFleetManager::ProcessDestroyFleetMemberRequests`
  (`Network/Server/ServerFleetManager.cpp:144-157`) skips a Battleship member by its recorded type,
  then destroys whatever row `FindShipByGlobalId` returns in the member's cell; a duplicate ID can
  make that another owner's row or a Battleship.
- `ServerFleetManager::DetectFleetMemberDeaths` (same file, lines 378-379) keeps a member live while
  any row in its cell carries its ID, so a stray row keeps a dead member live.
- `ServerFleetManager::RebuildFleetMembersForLoad` (same file, lines 323-361) revives the member
  whose ID a row carries under that row's owner GUID without comparing ship types.
- `ServerClientManager::SpawnFleetMember` (`Network/Server/ServerClientManager.cpp:241-253`) indexes
  the Battleships columns with the row `FindShipByGlobalId` returns for the flagship ID without
  checking its type. A saved `kFleet` Fighter or Omni row carrying the flagship's global ID in the
  flagship's cell makes a Fighter launch read a wrong Battleship row or past the end of the
  Battleships columns.
- A row ID at or above `iNextGlobalId` collides with a later mint.

Values the simulation produces: every nonzero row global ID is minted from `Game::miNextGlobalId`
(`ServerFleetManager.cpp:53,115`, the harness rig in
`Agent/Commands/ServerSimulationHarnessRigs.cpp:519-543`) and carried unchanged by spawn and
transfer; a fleet spawn carries its owner's GUID (`Network/Server/AGENTS.md`
`## Deterministic Tick Contracts`). Rows whose IDs name no fleet member are legitimate: a destroyed
member's dying row and a harness-injected fleet ship, which has a minted ID and an empty client GUID
(`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`, `inject_payload`). Only a
corrupt or tampered save, or an `edit_frame` write under the agent-channel exemption, produces a
value the check below rejects.

## Design

1. The author recommends a fifth game entry point beside the four in `Save/GameSaveState.h`:
   `void ValidateSaveState(const SaveStagedState& rStagedState, const std::unordered_map<engine::GridCoord, engine::Cell>& rCells, int64_t iNextGlobalId);`,
   defined in `Network/Server/ServerFleetSerialization.cpp` beside `ReadSaveState`. Rationale: the
   cells are read after the game half, so only a check run once both are staged can compare them,
   and the engine already calls the game half by name (`Engine/Source/AGENTS.md`
   `## Hub Conventions`).
2. `ValidateSaveState` walks every staged cell's frame with `ForEachShipType` and, for every row
   with a nonzero `pGlobalIds`, throws `std::ios_base::failure` when the ID is not below
   `iNextGlobalId`, when an earlier row in any cell carried the same ID, or when the ID names a
   staged fleet member and the row's ship type differs from the member's `eType` or its
   `pClientGuids` differs from the GUID of the owner whose fleet holds that member. Build the
   member lookup and the seen-ID set as `ReadFleetData` builds its sets.
3. `engine::ReadGridSave` calls it inside its existing `try`, after the client-grid-coordinate
   check, so a failure takes the existing abort path: the load returns false and nothing is adopted.
4. Update the docs the change makes incorrect: the entry-point count in `Save/AGENTS.md`
   `## Save and Load` and `Network/Server/AGENTS.md` `## State Ownership`, and the `## Grid Saves`
   paragraph of `Engine/Source/File/AGENTS.md` if it no longer describes what the reader checks.
5. No `kiVersion` bump: the stored bytes and their meaning are unchanged, and no value the
   simulation writes is newly rejected.

## Critical files

- `Engine/Source/File/GridSave.cpp`
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveState.h`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp`
- `Projects/BrokenEngineSandbox/Source/Save/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md`
- `Engine/Source/File/AGENTS.md`

## In scope

- The new `game::ValidateSaveState` declaration and definition.
- One call in the `try` block of `engine::ReadGridSave(const FileFlags_t&, const std::filesystem::path&, StagedGridSave&)`.
- The doc sentences named in Design step 4.

## Out of scope

- Changing `FindShipByGlobalId`, `ProcessDestroyFleetMemberRequests`, `DetectFleetMemberDeaths`,
  `RebuildFleetMembersForLoad`, or `SpawnFleetMember`: the load check removes the inputs that
  mislead them.
- Rejecting a row ID that names no fleet member, or a `kFleet` row with a zero ID or GUID.
- Per-frame `PostRead` checks, `edit_frame` and `inject_payload` admission, and replay
  `FrameInput` admission: the agent channel and replays are exempt developer tools.
- The grid save format, the fleet record format, and any `kiVersion`.

## Risk and invariants

- Tier trigger: Tier 3 — the change spans independently owned subsystems: the engine grid reader
  (`Engine/Source/File`) names one more game entry point, which every game project must provide.
  The format and trust are unchanged, and accepted saves behave exactly as before, so CRC,
  determinism, wire, and save layout are unchanged.
- Rejection follows the bad-value rule: throw through the existing `ReadGridSave` failure path,
  never repair or drop a row.
- Replay adoption reads its grid through the same `ReadGridSave`; that grid is written from live
  state, so it passes.
- Server-only (`BT_SERVER`). Allocation happens only at load, as `ReadFleetData`'s sets already do;
  no threading, shader, or wire exposure.

## Acceptance criteria

1. BrokenEngineSandbox client and server Debug|x64 build with no new warnings.
2. Live, through `/agent-harness`: with a client connected (its fleet and flagship exist) and the
   server paused, an `edit_frame` that writes a `kFleet` Fighter or Omni row's `pGlobalIds` to the
   flagship's global ID commits; `save` to a named file, then `load` of it, returns
   `resetToFresh:true`, and the server log shows `ReadGrid ... aborted: corrupt data`. The supplied
   evidence records no reproducing setup; the expected route is `pause`, an `inject_payload`
   `SpawnFighter` with `eController` `"Fleet"` in the flagship's cell, `edit_frame`, `save`, `load`.
3. A save whose fleet Fighter or Omni member row carries another `pClientGuids`, or whose row ID is
   at or above `status.nextGlobalId`, is rejected the same way when the harness can write it;
   otherwise settled by reading the check.
4. A save written by a running server with a connected client's fleet loads with
   `resetToFresh:false`, and its fleet members rebuild live.

## Notes

- Ordinary debt follow-up. No Plan covered this root cause when this Plan was written.
- `Documents/Plans/Game/RejectBattleshipHeightAboveFlightPlane.md` records a separate read gap found
  in the same run; the two change different regions and may land in either order.
