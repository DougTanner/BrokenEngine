<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:45:29.774Z","dependsOn":[]} -->
# Blasters light sync shares SynchronizeBlaster and drops the direction column

## Context

Line numbers are from commit `629ced2e`, except those in
`Blasters/BlastersUpdate.cpp`, which follow the removal of that file's
`Frame/TerrainUtils.h` include after that commit; locate by symbol if they
moved. Paths without a root are under
`Projects/BrokenEngineSandbox/Source/Frame/Collections/`. Nothing was built or
run for this Plan: every statement is from a code read.

### Client light sync

Three defects in the client-only Blasters light sync, all inside
`#if defined(BT_CLIENT)` except the one local load named last:

- Repeated logic. `BlastersInterpolate::ClientInitialize` ends with a block
  (`Blasters/Blasters.cpp:57-80`) that repeats the body of the file-static
  `SynchronizeBlaster` (`Blasters/BlastersUpdate.cpp:92-120`): the point-light
  test, `fSize`, the `PointLightsInterpolate::Sync` fields, `fWidth`,
  `fLength`, the `CalculateArea` arguments, and the
  `AreaLightsInterpolate::Sync` fields are the same. The real differences are
  the direction source (the stored `pVecDirections[iIndex]` at
  `Blasters.cpp:72` against `XMVector3Normalize(vecVelocity)` at
  `BlastersUpdate.cpp:111`) and where `rType` is looked up.
- Repeated normalize. `SynchronizeBlaster` takes `vecVelocity` and normalizes
  it at `:111`. Its only caller (`:146`) computed the same
  `XMVector3Normalize(vecVelocity)` at `:140` and stored it at `:143`.
- Guard scope. `BlastersInterpolate::Update` declares
  `[[maybe_unused]] int64_t iTypeIndex` at `:133` and loads it in both
  builds, but its only reads are at `:146` and `:150`, inside the `BT_CLIENT`
  block at `:145-159`. The load is not part of the load-and-store rule in
  `Collections/AGENTS.md` `## Game-Specific Rules`: the function never stores
  the column, and the comment at `:132` names it as copied by
  `AllocateAndCopy`. Root `AGENTS.md` `## Client/Server Targets` asks for the
  narrowest practical guard.

### Derived direction column

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

### Protocol version

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

### Origin

Originating gap: a findings-only deep analysis of that directory (recursive).
Its architecture phase (cohesion and generation-residue lens) found the
repeated block, and its architecture phase (client/server and data shape
lens) found the derived column; its in-function analysis phase found the
repeated normalize, the guard scope, and the `LogDifferences` reader. Each was
confirmed by an independent verification read. Pre-existing debt, not an
unmet acceptance criterion of any change. Unmet criteria: one copy of the
Blasters light sync, computing the direction once, with client-only locals
under `BT_CLIENT`; and no shared Blasters column that no simulation decision
reads.

## Design

Recommended (the author's recommendation): share one light sync, delete the
column, and derive the direction where the client needs it. Follow the
layout-change checklist of `/add-collection-member`; `Collections/AGENTS.md`
`## Game-Specific Rules` routes every layout change through it. The steps
reach the end state directly; no step leaves `ClientInitialize` reading the
stored column.

1. Change `SynchronizeBlaster`'s `vecVelocity` parameter to the direction and
   delete the normalize at `BlastersUpdate.cpp:111`.
2. In `BlastersInterpolate::Update`, move the `iTypeIndex` declaration to the
   first line of the `BT_CLIENT` block and drop `[[maybe_unused]]`; move the
   `vecDirection` computation (`BlastersUpdate.cpp:140`) into the same
   `BT_CLIENT` block, ahead of the `SynchronizeBlaster` call that is then its
   only reader, and pass `vecDirection` at `:146`. Delete the store at `:143`.
   The server build of Interpolate Update then normalizes nothing.
3. Replace `Blasters.cpp:57-80` with a `SynchronizeBlaster` call that passes
   the row's lights, position, direction, and type index, where the direction
   is the normalized `rFrame.postRender.pBlasters->pVecVelocities[iIndex]`.
   Spawn stores that velocity (`Blasters/Blasters.cpp:112`) before it calls
   `ClientInitialize` (`:116`), so the value is there, and the light no
   longer relies on a stored direction for a row that arrived by read or
   transfer.
4. Remove `pVecDirections` and its `SharedMembers()` entry
   (`Blasters/Blasters.h`), the spawn store (`Blasters/Blasters.cpp:109`), and
   the `LogDifferences` line (`Blasters/Blasters.cpp:163`).
5. Remove the `dir` field from the agent blaster row query and
   `pVecDirections` from `BlastersFrameColumns`, and update the harness
   documentation that lists them
   (`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`
   lists the blaster row as `index,local,dir,alignment`).
6. Bump `BlastersInterpolate::kiVersion` (`Blasters/Blasters.h:34`), which
   `Frame::kiVersion` composes, so older saves and replays are rejected, and
   so is a client of another build at Hello. No `engine::kiProtocolVersion`
   or `FrameInput::kiVersion` bump, for the reason `### Protocol version`
   gives.

How `ClientInitialize` reaches `SynchronizeBlaster` is the one choice this
Plan pre-stages; the executing session confirms it in its plan:

- Option A, recommended: give `SynchronizeBlaster` external linkage and
  declare it by hand in `Blasters/Blasters.cpp` under `BT_CLIENT`, mirroring
  Missiles, where `SynchronizeMissile` is defined in `Missiles/Missiles.cpp`
  and declared by hand in `Missiles/MissilesUpdate.cpp:34`. Rationale: root
  `AGENTS.md` `## Directives` keeps mirrored patterns parallel, and no header
  changes.
- Option B: declare it in `Blasters/Blasters.h` under `BT_CLIENT`. It avoids
  a hand declaration but adds a client-only free function to a header in the
  game precompiled-header closure.

## Risk tier

Tier 3. Trigger: serialization and data layout, and the per-tick CRC: a
shared CRC'd column leaves the frame. The edit also rewrites statements
inside `BlastersInterpolate::Update`, which both builds compile and which
stores CRC'd columns, and Option B adds a header declaration.

Exposure: save, replay, and full-state bytes change for every Blaster row;
the CRC value changes but stays equal on client and server; no simulation
value changes, because nothing in the simulation reads the column. Server
work drops one normalize per row per tick and one local load nothing reads.
The lights are client-only visual state outside the CRC, and each keeps the
same inputs. No threading exposure. Agent harness output and `edit_frame`
lose a column, a developer-tool surface.

## Critical files

- `Blasters/Blasters.h`
- `Blasters/Blasters.cpp`
- `Blasters/BlastersUpdate.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`

## In scope

- `SynchronizeBlaster`: its linkage, its direction parameter, and the
  normalize at `BlastersUpdate.cpp:111`.
- In `BlastersInterpolate::Update`: the `SynchronizeBlaster` call, the
  `iTypeIndex` declaration, the `vecDirection` local
  (`Blasters/BlastersUpdate.cpp:140`, its place only, moved into the
  `BT_CLIENT` block), and the store at `:143`.
- The light-sync block at the end of `BlastersInterpolate::ClientInitialize`
  (`Blasters.cpp:57-80`) and the direction it passes.
- The one declaration the chosen option adds.
- `BlastersInterpolate::pVecDirections`, its `SharedMembers()` entry,
  `BlastersInterpolate::kiVersion`, the spawn store, and the `LogDifferences`
  line.
- The blaster `dir` field of the agent row query, the `pVecDirections` entry
  of `BlastersFrameColumns`, and the harness documentation that names them.
- Documentation sync the removal requires.

## Out of scope

- The light and wind-trail creation in `ClientInitialize`
  (`Blasters.cpp:31-55`) and the wind-trail sync in Update.
- `BlastersPostRender::pVecVelocities` and every simulation statement.
- `SynchronizeMissile` and the Missiles files.
- The `pVecDirections` columns of Fighters, Omnis, Battleships, and
  Missiles, and their `dir` query fields.
- The Blasters `SpawnInfo` payload, `FrameInput::kiVersion`, and
  `engine::kiProtocolVersion` (unless plan review overrules
  `### Protocol version`).

## Acceptance criteria

1. `Blasters.cpp` holds no `PointLightsInterpolate::Sync` or
   `AreaLightsInterpolate::Sync` call, `SynchronizeBlaster` holds no
   `XMVector3Normalize`, and `iTypeIndex` is declared inside `BT_CLIENT`
   without `[[maybe_unused]]`; the diff settles this.
2. No `BlastersInterpolate::pVecDirections` remains in the tracked tree, and
   `BlastersInterpolate::kiVersion` is higher than at commit `629ced2e`.
3. `/compile` builds the BrokenEngineSandbox client and server.
4. Through `/agent-harness`, one scenario with Omnis firing blasters: a
   replay recorded on the changed build plays back with no per-tick CRC
   mismatch, and a client screenshot shows blaster area lights aligned with
   their flight direction, both for bolts spawned while the
   client watches and for bolts present when it joins. The supplied evidence
   records no harness setup, so the executing session builds the scene.
