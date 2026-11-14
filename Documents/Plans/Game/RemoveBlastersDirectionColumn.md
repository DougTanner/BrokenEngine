<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:45:17.288Z","dependsOn":["Documents/Plans/Game/BlastersClientInitializeSharesSynchronizeBlaster.md"]} -->
# Remove the derived BlastersInterpolate direction column

## Context

Line numbers are from commit `629ced2e`, except those in
`Blasters/BlastersUpdate.cpp`, which follow the removal of that file's
`Frame/TerrainUtils.h` include after that commit; locate by symbol if they
moved. Paths without a root are under
`Projects/BrokenEngineSandbox/Source/Frame/Collections/`. Nothing was built or
run for this Plan: every statement is from a code read.

`BlastersInterpolate::pVecDirections` (`Blasters/Blasters.h:49`) is a shared
column: it is in `SharedMembers()` (`:55`), and the collection declares no
`SharedCrcMembers()`, so it is saved, replayed, sent in full state, and
CRC'd. It is fully derived from `BlastersPostRender::pVecVelocities`:

- Spawn stores `XMVector3Normalize(rSpawnInformation.vecVelocity)`
  (`Blasters/Blasters.cpp:109`).
- Every Interpolate Update stores `XMVector3Normalize(vecVelocity)` of the
  previous PostRender velocity (`Blasters/BlastersUpdate.cpp:140`, `:143`).

Its readers:

- client `BlastersInterpolate::ClientInitialize` (`Blasters/Blasters.cpp:72`),
  for the area light;
- `BlastersInterpolate::LogDifferences` (`Blasters/Blasters.cpp:163`);
- the agent blaster row query, field `dir`
  (`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp:129`);
- the `edit_frame` column list `BlastersFrameColumns`
  (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp:25`).

No simulation decision reads it. The per-tick light sync normalizes the
velocity again instead of reading it (`Blasters/BlastersUpdate.cpp:111`). The
transfer payload does not carry it (`SpawnInfo::Fields()`,
`Blasters/Blasters.h:112`), so the cross-cell exception in
`Engine/Source/Frame/AGENTS.md` `## Simulation and Visual State` does not
apply. That section's first bullet says a column no simulation decision
reads is visual and does not belong in the shared state.

The changed full-state layout needs no `engine::kiProtocolVersion` bump. A
client and server whose Blasters layouts differ never exchange a full state:
the client sends `game::Frame::kiVersion` in its Hello
(`Engine/Source/Network/Client/Client.cpp:1051`), `Server::ClientHello`
rejects a Hello whose value differs before it accepts the client
(`Engine/Source/Network/Server/Server.cpp:630`), and that version composes
`BlastersInterpolate::kiVersion` through `GameServerInterpolateTypes`
(`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:116`). The full state a
client reads goes through `Frame::ServerRead` (`Client.cpp:395`), which walks
the `SharedMembers()` lists that version covers. The protocol rule
(`Engine/Source/Network/AGENTS.md:28`) names two cases, a game `StatusChange`
payload layout change and an engine packet layout, type, or payload-contract
change. A collection column is neither: the `SpawnInfo` payload does not
change, so `FrameInput::kiVersion` is untouched too, and no engine packet
layout changes. `Documents/Architecture/Network.md:502-505` names
`Frame::kiVersion` as the Hello gate for deterministic-Frame compatibility
and keeps the protocol gate for `StatusChange` payload layouts. The sources
disagree, though: `Documents/Architecture/Network.md:487-490` records protocol
versions 25 and 24 as full-state frame changes (25 also changed
`ExplosionsInterpolate::pStartTimes`), and
`Engine/Source/Network/AGENTS.md:37-39` says `Frame::kiVersion` is not a
substitute for the protocol-version bump. This Plan trusts the rule's two
named cases (`Engine/Source/Network/AGENTS.md:28-36`) and the Hello gate
(`Server.cpp:630`), which rejects a client whose `Frame::kiVersion` differs
before any full state is exchanged. The executing session's plan review may
overrule this; a bump would then add
`Engine/Source/Network/NetworkProtocol.h` and a
`Documents/Architecture/Network.md` history entry to the change.

Originating gap: the architecture phase (client/server and data shape lens)
of a findings-only deep analysis of that directory (recursive), confirmed by
an independent verification read; the in-function phase added the
`LogDifferences` reader. Pre-existing debt, not an unmet acceptance criterion
of any change. Unmet criterion: no shared Blasters column that no simulation
decision reads.

## Design

Recommended (the author's recommendation): delete the column and derive the
direction where the client needs it. Follow the layout-change checklist of
`/add-collection-member`; `Collections/AGENTS.md` `## Game-Specific Rules`
routes every layout change through it.

The steps are stated against the code as the Plan this one depends on
(`## Coordination`) leaves it: `SynchronizeBlaster` takes a direction and
normalizes nothing, `BlastersInterpolate::Update` passes it the `vecDirection`
local, and `ClientInitialize` calls it with the stored column.

1. Remove `pVecDirections` and its `SharedMembers()` entry
   (`Blasters/Blasters.h`).
2. Remove both stores (`Blasters/Blasters.cpp:109`,
   `Blasters/BlastersUpdate.cpp:143`) and the `LogDifferences` line
   (`Blasters/Blasters.cpp:163`).
3. Move the `vecDirection` computation in `BlastersInterpolate::Update`
   (`Blasters/BlastersUpdate.cpp:140`) into the `BT_CLIENT` block, ahead of
   the `SynchronizeBlaster` call that is then its only reader. The server
   build of Interpolate Update then normalizes nothing.
4. In `ClientInitialize`, pass `SynchronizeBlaster` the normalized
   `rFrame.postRender.pBlasters->pVecVelocities[iIndex]` in place of the
   stored column. Spawn stores that velocity (`Blasters/Blasters.cpp:112`)
   before it calls `ClientInitialize` (`:116`), so the value is there.
5. Remove the `dir` field from the agent blaster row query and
   `pVecDirections` from `BlastersFrameColumns`, and update the harness
   documentation that lists them
   (`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`
   lists the blaster row as `index,local,dir,alignment`).
6. Bump `BlastersInterpolate::kiVersion` (`Blasters/Blasters.h:34`), which
   `Frame::kiVersion` composes, so older saves and replays are rejected, and
   so is a client of another build at Hello. No `engine::kiProtocolVersion`
   or `FrameInput::kiVersion` bump, for the reason `## Context` gives.

Risk tier: Tier 3 (the author's classification). Trigger: serialization and
data layout, and the per-tick CRC: a shared CRC'd column leaves the frame.
Exposure: save, replay, and full-state bytes change for every Blaster row;
the CRC value changes but stays equal on client and server; no simulation
value changes, because nothing in the simulation reads the column. Server
work drops one normalize per row per tick. Agent harness output and
`edit_frame` lose a column, a developer-tool surface.

## Critical files

- `Blasters/Blasters.h`
- `Blasters/Blasters.cpp`
- `Blasters/BlastersUpdate.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`

## In scope

- `BlastersInterpolate::pVecDirections`, its `SharedMembers()` entry, and
  `BlastersInterpolate::kiVersion`.
- The two stores, the `LogDifferences` line, and the direction argument of
  the `SynchronizeBlaster` call in `BlastersInterpolate::ClientInitialize`.
- The `vecDirection` local in `BlastersInterpolate::Update`
  (`Blasters/BlastersUpdate.cpp:140`): its place only, moved into the
  `BT_CLIENT` block.
- The blaster `dir` field of the agent row query, the `pVecDirections` entry
  of `BlastersFrameColumns`, and the harness documentation that names them.
- Documentation sync the removal requires.

## Out of scope

- `BlastersPostRender::pVecVelocities` and every simulation statement.
- The `pVecDirections` columns of Fighters, Omnis, Battleships, and
  Missiles, and their `dir` query fields.
- `SynchronizeBlaster` itself and the merge of `ClientInitialize` with it,
  which the Plan named under `## Coordination` owns.
- The Blasters `SpawnInfo` payload, `FrameInput::kiVersion`, and
  `engine::kiProtocolVersion`.

## Acceptance criteria

1. No `BlastersInterpolate::pVecDirections` remains in the tracked tree, and
   `BlastersInterpolate::kiVersion` is higher than at commit `629ced2e`.
2. `/compile` builds the BrokenEngineSandbox client and server.
3. Through `/agent-harness`, a replay recorded on the changed build, with
   Omnis firing blasters, plays back with no per-tick CRC mismatch, and a
   client screenshot shows blaster area lights aligned with their flight
   direction, including a bolt present when a client joins. The supplied
   evidence records no harness setup, so the executing session builds the
   scene.

## Coordination

This Plan depends on
`Documents/Plans/Game/BlastersClientInitializeSharesSynchronizeBlaster.md`,
which changes the same statements first: the light sync at the end of
`ClientInitialize` (`Blasters/Blasters.cpp:57-80`) and the normalize and
call in Interpolate Update (`Blasters/BlastersUpdate.cpp:140-146`). That
Plan leaves `ClientInitialize` passing the stored column to
`SynchronizeBlaster` and Update computing `vecDirection` once, outside
`BT_CLIENT`, for the store and the call. This Plan then changes that
argument to the normalized velocity, deletes the store, and moves the
computation into the `BT_CLIENT` block.
