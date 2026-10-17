<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-15T23:39:11.135Z","dependsOn":["Documents/Plans/Game/FrameEditCellPins.md","Documents/Plans/Game/FrameEditRows.md"]} -->
# Move the game-agnostic agent commands into `Engine/Source/Agent` and the engine-generic harness documentation into the agent-harness skill

Line numbers cite baseline `f1d140ae26d0a8684264c8a1fa4f55c1bec1f601`. The prerequisite Plans edit several of these files first, so locate each region by the symbol named beside it. Where a statement here and the code disagree, the code wins; report the contradiction instead of matching one side to the other.

## Context

The user's direction, recorded by the dispatch that commissioned this Plan: a second game project will be created beside `Projects/BrokenEngineSandbox`. Engine code may call expected `game::` classes, globals, and members by name, but must not name Sandbox gameplay (`Blaster`, `Spaceship`, `Missile`, `Omni`, `bUseMissiles`, `Fleet`). Fleets stay game-owned. Agent commands are an exempt developer channel (`Engine/Source/Agent/AGENTS.md` `## Architecture`), so no new validation is needed.

The game Agent (`Projects/BrokenEngineSandbox/Source/Agent/`) holds commands and helpers that read only engine state, so a second game would have to copy them:

- `audio_streaming_harness_rig` (`Commands/AgentCommandsAudioStreaming.cpp:1-398`) uses only `engine::AudioStreamingHarnessRig`, `gpAgentCommandServer`, and `gpAudioManager`.
- `desync_probe` (`Commands/ClientDesyncProbe.cpp:16-153`) uses engine `Client`, `ClientDesyncCore`, `ReconcileDesyncInfo`, `GameBase` cells and `SnapshotIndex`. Its only game-owned reach is `gpClientSession->mpDesynchronizationCore` (`Network/Client/ClientSession.h:68`), which `Documents/Plans/Engine/SharedSessionProtocol.md` moves into engine ownership.
- Server `status` core fields, `pause`, and `query_profile`
  (`AgentCommandsServer.cpp:82-106,113-122,179-227`) read only `GameBase`,
  `engine::gpReplay`, `engine::gpServer`, and `ProfileManager` members, except
  the status fields that read `game::gpServerSession`
  (`pendingFlagshipUpdateCount` `:107`, `harvestedTransferTotal` `:108`, and
  `pendingAgentStatusChangeCount` `:110` once `FrameEditRows.md` removes
  `pendingTransferHarnessRigCount` `:109`) and the `NavQuery` raw-row block at
  `:200-210`. `CoordinateFromParameter` (`:229-242`) is a wrapper over
  `engine::AgentGridCoordinateValue`.
- `replay_record`, `replay_play`, `replay_transfer_capture`, `replay_drop_retained_end_frame`, and `replay_inject_persistence_failure` (`Commands/ServerSimulationHarnessRigs.cpp:51-255`) touch only `engine::gpReplay`, `GameFlags`, `mbReplaying`, and `engine::ReplayHarnessRigs`, except the four count names at `:136-139` (the slot clear at `:86` goes with the replay-transfer queue `FrameEditRows.md` removes).
- `describe_scene` (`AgentScene.cpp:185-375`): the camera, UI state, tick, client cell, `subscribedCoords`, `cellStates` (`:114-155`), islands (`:272-289`), and the visible-unit cull (`:157-183`) are engine state; units, fleets, flag names, and counts are game. The shared `.agents/skills/agent-harness/scripts/Wait-IslandSceneReady.ps1:347-389` requires `clientGridCoord` and `islands` from it.
- Duplicates of engine code: `game::ClientGridCoordinateValue`
  (`AgentCommandsClient.cpp:18-44`) repeats `engine::AgentGridCoordinateValue`
  (`Engine/Source/Agent/AgentCommandsShared.cpp:194-217`);
  `game::RequireHarnessRigClient` (`AgentCommandsClient.cpp:46-58`) repeats the
  accepted-client check of `send_raw_packet`
  (`Engine/Source/Agent/AgentCommandsClientGeneric.cpp:1178-1181`) —
  `engine::gpClient` is the `Client` singleton (`Client.cpp:17`) that
  `ClientSessionRuntime::mpClient` owns (`ClientSessionRuntime.cpp:121`);
  `FiniteFloatFromValue` (`Commands/ServerSimulationHarnessRigs.cpp:268-280`)
  repeats `FloatFromValue` (`Commands/ServerFrameEdit.cpp:135-148`);
  `describe_scene`'s `uiState`/`tweaksVisible`/`gameFlags`
  (`AgentScene.cpp:249-251`) repeat `BuildDescribeUi`
  (`AgentCommandsClientGeneric.cpp:676-678`).
- `edit_frame`/`read_frame` value and column machinery
  (`Commands/ServerFrameEdit.cpp:19-33,39-80,110-351,473-590`) works on engine
  `FrameColumnList` (`Engine/Source/Frame/FrameUtils.h`), engine value types,
  the engine base `Values()` (`Engine/Source/Frame/FrameBase.h:173-175,258`),
  and the engine explosions and pushers collections. The four game column lists
  (`:35-38`), their specializations (`:82-108`), `FrameCollections`
  (`:354-358`), and the command bodies name game collections or game policy.

Documentation: the Sandbox client command reference documents 18 engine-owned
commands
(`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-client.md:7,11-28,30-31`),
and `Engine/Source/Agent/AGENTS.md:13` routes there. `server-window.md:1-499`
documents the engine-owned server window (`Engine/Source/Server/AGENTS.md:1-7`),
while `.agents/skills/agent-harness/references/command-reference.md:9` calls it
project-owned — the engine leaf and the code location were trusted.
`launch.md:5-247` and `replay.md:5-18,78-97` are generic apart from project
values. `Engine/Source/Agent/Commands/ReplayHarnessRigs.h:5` includes the game
header `Agent/Commands/ServerSimulationHarnessRigs.h`, and
`Engine/Source/Agent/AGENTS.md:5,13,15` link Sandbox files as the contract, so
no document says what a game must provide.
`Documents/Plans/ChangeWorkflow/ProjectAwareTooling.md` `## Out of scope` hands
the agent-harness routing links (`SKILL.md:27,42`,
`references/worker.md:13,502,563-567`, `references/renderdoc.md:17`,
`references/command-reference.md:3,7,9`) to this Plan.

## Design

Mirror the client split: the engine owns every handler that names no game type beyond `GameBase` members and the agreed game hooks; the game dispatcher keeps its order (engine shared, then engine side-generic, then game side handlers). Every moved handler keeps its body, JSON fields, and error texts, except where a stage below states otherwise. Each stage is independently buildable; the recommended order is 1 to 5.

### Stage 1: shared helpers

1. `engine::CoordinateFromParameter(const nlohmann::json& rParameters, std::string_view key = "coord")`
   moves verbatim from `AgentCommandsServer.cpp:229-242` into
   `AgentCommandsShared.h/.cpp` (build-agnostic; it uses only
   `AgentGridCoordinateValue`). Remove the game declaration
   (`AgentCommandsServerQueries.h:13`); game callers
   (`AgentCommandsServerQueries.cpp:142`,
   `Commands/ServerFrameEdit.cpp:220,433`,
   `Commands/ServerSimulationHarnessRigs.cpp:338,410`) call
   `engine::CoordinateFromParameter`. The existing name is kept to keep the
   call-site churn to a namespace qualifier.
2. `engine::AgentFloatValue(const nlohmann::json& rValue, std::string_view name)`
   in `AgentCommandsShared.h/.cpp` takes `FloatFromValue`'s body. Delete
   `FiniteFloatFromValue`; its call sites
   (`ServerSimulationHarnessRigs.cpp:330,334,352`) use `AgentFloatValue`.
   Recommended over keeping `FiniteFloatFromValue` because `FloatFromValue`
   range-checks the double before narrowing instead of testing the narrowed
   result. `inject_payload`'s bad-number text becomes
   `'<name>' must be a finite number within the float range`.
3. `engine::Client& engine::RequireAcceptedClient(std::string_view command)` in `AgentCommandsClientGeneric.h/.cpp` (client-only), throwing `<command> requires an accepted connected client` when `engine::gpClient` is null or lacks `kConnectionAccepted`. `send_raw_packet` uses it (its text is unchanged). Delete `game::RequireHarnessRigClient` (`AgentCommands.h:25-26`); `set_client_grid_coord` (`AgentCommandsClient.cpp:82`) calls the engine helper.
4. Delete `game::ClientGridCoordinateValue` (`AgentCommands.h:21-23`); `set_client_grid_coord` (`:79`) calls `engine::AgentGridCoordinateValue(..., "set_client_grid_coord 'coord'")`, which produces the same two messages.

### Stage 2: engine server-generic handler

New `Engine/Source/Agent/AgentCommandsServerGeneric.h/.cpp`, whole-file
`BT_SERVER`, mirroring `AgentCommandsClientGeneric`:
`bool engine::ExecuteServerAgentCommand(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult)`.
The header is aggregated in `Engine.h` in a `BT_SERVER` block beside the
client-generic include (`Engine.h:92-95`) and declares the game hooks it calls
in a `namespace game` block (the form `Engine/Source/GameBase.h:13-20` uses for
game types), so a game that omits one fails to link with the hook's name.

- `status`: `:84-106` verbatim, then `game::AppendAgentServerStatus(nlohmann::json& rResult)`; the game defines it with the three fields at `:107`, `:108`, and `:110` (`FrameEditRows.md` removed `pendingTransferHarnessRigCount` and made `pendingAgentStatusChangeCount` read `ServerSpawnQueue::PendingCount`).
- `pause`: `:113-122` verbatim.
- `query_profile`: `:179-227`, with the `if constexpr (kbProfiling)` block at
  `:200-210` replaced by
  `game::AppendAgentCpuTimerFields(int64_t iTimer, nlohmann::json& rTimer)`,
  called for every timer inside the timer lock; the game defines it with that
  block. The engine names `gpProfileManager`, as the client handler
  already does (`AgentCommandsClientGeneric.cpp:1126`).
  `EngineOwnedSaveSettingsAndSwitches.md` leaves the CPU timer enum game-owned,
  which is why the `NavQuery` fields need the hook.
- The five replay commands: `ServerSimulationHarnessRigs.cpp:51-255` verbatim,
  with one change: the `replay_transfer_capture` count lines call
  `game::AppendReplayTransferCaptureCounts(const game::ReplayTransferCaptureCounts& rCounts, nlohmann::json& rEvent)`,
  which writes the game's count fields (`omniCount`, `spaceshipCount`,
  `blasterCount`, `missileCount`); `ReplayTransferCaptureCounts` is the game
  type `ReplayHarnessRigs::TransferCaptureEvent::transferCounts` already holds
  (`ReplayHarnessRigs.h:32`), forward-declared in the header.
  `FrameEditRows.md` removed the harness state slot, `Bind`, and the
  replay-transfer queue, so nothing else game-side is carried over.
- `ExecuteServerSimulationHarnessRigCommand` handles only `inject_payload`.
- `game::ExecuteAgentCommand` (`AgentCommands.cpp:30-34`) tries `engine::ExecuteServerAgentCommand` before `ExecuteAgentCommandServer`, as its client branch does (`:20-24`). `ExecuteAgentCommandServer` keeps `timescale`, `save`, `load`, `reset`, `edit_frame`, `read_frame`, `inject_payload`, and the three frame queries.

### Stage 3: engine client commands

- `audio_streaming_harness_rig`: `Commands/AgentCommandsAudioStreaming.h/.cpp` move to `Engine/Source/Agent/Commands/` under the same names, `namespace engine`, body unchanged; `ExecuteClientAgentCommand` dispatches it; the game dispatch row (`AgentCommandsClient.cpp:118-122`) goes.
- `desync_probe`: `Commands/ClientDesyncProbe.h/.cpp` move to
  `Engine/Source/Agent/Commands/`, `namespace engine`. The client comes from
  `RequireAcceptedClient`, the desync core from the engine owner
  `SharedSessionProtocol.md` establishes, and cells and frames from
  `game::gpGame` (including `Game.h` as `AgentCommandsClientGeneric.cpp:12`
  does). Its current-frame lookup (`ClientDesyncProbe.cpp:99-108`) uses
  `engine::Cell::TailFrame()`, which
  `Documents/Plans/Engine/GameShellIntoGameBase.md` adds and leaves this site to
  this Plan; the `nullptr` result keeps the one
  `desync_probe requires a current client frame` text.
  `ExecuteClientAgentCommand` dispatches it; the game row (`:128-132`) goes.
- `describe_scene`: new `Engine/Source/Agent/Commands/DescribeScene.h/.cpp`,
  whole-file `BT_CLIENT`, outside the `Engine.h` aggregation, included by
  `AgentCommandsClientGeneric.cpp` and the game `AgentScene.cpp`. The engine
  `CommandDescribeScene` validates `includeUnits` and `maxUnits`
  (`AgentScene.cpp:187-194,199-204`); writes `camera` (`:240-248`), the UI
  fields through a new `engine::AppendUiStateFields(nlohmann::json&)` that
  `BuildDescribeUi` also uses, `tick`, `clientGridCoord`, and per cell
  `subscribedCoords`, `cellStates`, and `islands` (`:114-155,267-289`, with
  `Vector2ToJson`; `BuildCellState`'s `tailTick` (`:149-153`) reads
  `engine::Cell::TailFrame()`, `-1` when it is `nullptr`); then calls
  `game::DescribeSceneUnits(const nlohmann::json& rParameters, engine::AgentSceneUnits& rUnits, nlohmann::json& rResult)`,
  declared in `DescribeScene.h`; then writes `units` and `truncated` from
  `rUnits`. `engine::AgentSceneUnits` carries `bIncludeUnits`, `iMaximumUnits`,
  the camera visible area, the `units` array, the running count, and the
  truncation flag. The template
  `engine::AppendVisibleUnits(AgentSceneUnits&, std::string_view type, const XMVECTOR* pVecPositions, int64_t iRowCount, GridCoord coordinate, BUILD_ROW&&)`
  is `:157-183`, making the `RenderBasis` (`:301`) itself. The game
  `AgentScene.cpp` keeps `OmniFlagNames`, `SpaceshipFlagNames`, `BuildFleets`,
  the `unitTypes` parse (`:195-198,206-237`), and its own loop over cells with a
  snapshot for `counts` and unit rows (`:291-372`), and writes `fleets` and
  `counts`. The game dispatch row (`:123-127`) goes. The engine dispatches and
  the game appends (rather than the game calling an engine builder) so every
  game's `describe_scene` carries the fields `Wait-IslandSceneReady.ps1` reads.
  `nlohmann::json` objects are key-ordered, so field order is unchanged; only
  which message wins when `maxUnits` and `unitTypes` are both invalid can
  differ.

### Stage 4: frame-edit machinery

Runs after `Documents/Plans/Game/FrameEditCellPins.md` has changed
`ServerFrameEdit.cpp`. New
`Engine/Source/Agent/Commands/FrameEdit.h`, whole-file `BT_SERVER`, outside the
`Engine.h` aggregation, included only by the game `ServerFrameEdit.cpp`. It
names `game::FrameInterpolate`/`FramePostRender` (for `FrameValueColumns`), a
game-type ownership `Engine/Source/Agent/AGENTS.md` records as the Engine hub
(`Engine/Source/AGENTS.md` `## Hub Conventions`) requires. It takes, in
`namespace engine` with `static` dropped: `kuiUuidCounterLimit`, `kbIsIdentity`,
`kbIsFlags`, `MemberOwner`, `ExplosionsFrameColumns`, `PushersFrameColumns`,
`ConcatenateColumns`, `FrameValueColumns`, `kRefusedFrameValues` and its two
checks, the primary `FrameCollection` template and its explosions and pushers
specializations, `ColumnReference`, `MatchesServerMembers`, `AreDistinct`,
`HasDistinctNames`, the two `static_assert`s, `IntegerFromValue`, `ReadElement`
(calling `AgentFloatValue` and `CoordinateFromParameter`), `WriteMember`,
`WriteKeys`, `WritesFromEdit`, `WriteRow`, `FrameCollectionAt`, `WriteElement`,
`ReadMember`, `ReadKeys`, and `ReadRow`, plus any helper the prerequisite Plans
added that names no game collection, game `Spawn`, `gpGame`, or
`OnStateReplaced`. `static_assert` texts that say "in this file" name the game
`FrameCollection` specialization instead. The game keeps the four column lists,
their specializations (now of `engine::FrameCollection`), `FrameCollections`,
`ApplyEdit`, `CommandEditFrame`, and `CommandReadFrame`. The command bodies stay
game-side because the resynchronization refusal reads `pOmnis` (`:433`), the
commit calls the game `OnStateReplaced` (`:466`); moving them would need one
hook per policy line.

### Stage 5: documentation

- `.agents/skills/agent-harness/references/command-reference.md` becomes the one
  schema reference for every engine-owned command. Keep `## Shared commands`.
  Add `## Client commands`: the `commands-client.md` entries for
  `send_raw_packet` (`:7`), `desync_probe` (`:8`), `screenshot` through `mouse`
  (`:11-28`), `query_profile` (`:30`), `presentation_continuity_probe` (`:31`),
  and `describe_scene`'s engine parameters and fields with the `basisCoord` and
  `cellStates` sentences (`:29`), noting that the project adds `unitTypes`, unit
  row fields, `fleets`, and `counts`; then `### Client command behavior`
  (`:33-41`) without its game-unit sentences. Add `## Server commands`: `status`
  core fields with the note that the project adds fields, `pause`
  (`commands-server.md:8`), the five replay commands (`:13-17`, with
  `replay_transfer_capture`'s count fields named as project-supplied), the
  generic part of `query_profile` (`:31`), and the `coord` rule (`:55`). Add
  `## Durable caveats`: the `query_profile` cohort caveat
  (`commands-client.md:45`, without the `NavQuery` sentence, which stays in
  `commands-server.md`) and a rewritten source-location caveat. Rewrite `:3,5,7`
  to route to the selected project's hub and to the moved entries, and `:9` to
  say the server window is engine-owned, linking `server-window.md` beside it.
- `server-window.md` moves to `.agents/skills/agent-harness/references/server-window.md`, unchanged apart from its title and back-link.
- `launch.md` `## Launch` (`:5-209`, including the additional client and island
  readiness) and `## Normal launch` (`:211-247`) move to
  `.agents/skills/agent-harness/references/launch.md`. Each project value —
  output directory, executable names, server process-name pattern
  (`BrokenEngineSandboxServer*`, `:9,12,218`), and the role game and app-data
  child names (`Broken Engine Sandbox Server`, `Broken Engine Sandbox`,
  `:40,83,111,148`) — becomes a placeholder defined in a new `## Launch values`
  table in the project hub
  `Projects/BrokenEngineSandbox/Documents/AgentHarness.md`. That table sits
  outside the machine-readable block at `:25-30`, which
  `Invoke-HarnessClaim.ps1` parses as exactly one assignment set. Any sentence
  naming game-only behavior stays in the project `launch.md`, which keeps its
  `## Durable caveats` (`:249-251`).
- `replay.md` `### Replay determinism acceptance` (`:5-18`) and `#### Replay manifest v5 integrity and generation matrix` (`:78-97`) move to `.agents/skills/agent-harness/references/replay.md`. Their references to project recipes route through the selected project's hub; the project `replay.md` keeps the rest and links the moved sections.
- Repoint links to moved content: `.agents/skills/agent-harness/SKILL.md:27,42`;
  `references/worker.md:13,502,563-567` (verification and cross-cell stay
  project routes through `Projects/<Project>/Documents/AgentHarness.md`);
  `references/renderdoc.md:15,17`;
  `.agents/skills/verify-acceptance/references/worker.md:42` and
  `.agents/skills/next-plan/SKILL.md:278` (the `query_profile` caveat); the
  project hub `AgentHarness.md:7` and its routing and focused-reference lists;
  links inside the project `AgentHarness/*.md` files to moved sections; and
  every other tracked reference to a moved file or section that `git grep` finds
  at implementation time, live Plans included.
- `Engine/Source/Agent/AGENTS.md`: new `## Game contract` listing every
  game-provided name the files under `Engine/Source/Agent` consume — hook
  functions with their declaring header, game globals and types, and
  project-relative game header paths such as `ReplayHarnessRigs.h:5`'s — taken
  from `git grep -n -E "game::|#include" -- Engine/Source/Agent` at
  implementation time, plus the `FrameEdit.h` ownership record.
  `## Architecture` gains `ExecuteServerAgentCommand` and the moved commands;
  `:5,13,15` stop linking Sandbox files as the contract. `Projects/AGENTS.md`
  gains one routing bullet: a game project provides the hooks in that section.
  `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` (`:5,10,11,14`) and
  `Projects/BrokenEngineSandbox/Source/AGENTS.md` (the Agent bullet of
  `## Ownership`) describe the new split; moved entries and file-location
  caveats leave `commands-client.md` and `commands-server.md` (`:46`, `:61`,
  `:65`).

### Risk tier

Tier 3 (`.agents/references/risk-tiers.md`). Trigger: the change spans independently owned subsystems — engine Agent, game Agent, and the agent-harness skill, whose readiness script consumes `describe_scene`. No determinism/CRC, wire, serialization, replay-format, threading, or trust surface changes: every moved handler runs at the same `AgentCommandServer::Drain` point on the main thread, `edit_frame` commits through the same `TransferViaStream`, and no success response changes its fields.

Invariants:

- Moved handlers keep running under `Drain`'s allocation suppression; `desync_probe` keeps its `ScopedSuppressAllocationTracking`.
- New and moved files keep whole-file `BT_CLIENT`/`BT_SERVER` guards and matching project membership: `AgentCommandsServerGeneric.*` and `FrameEdit.h` in the server project only; `DescribeScene.*`, `ClientDesyncProbe.*`, and `AgentCommandsAudioStreaming.*` in the client project only.
- Engine and game source roots share the include path, so no new engine file may share a project-relative path with a remaining game file.
- No new or moved engine file names `Blaster`, `Spaceship`, `Missile`, `Omni`, `bUseMissiles`, or `Fleet`.

## Critical files

- `Engine/Source/Agent/AgentCommandsShared.h/.cpp`, `AgentCommandsClientGeneric.h/.cpp`, new `AgentCommandsServerGeneric.h/.cpp`, `Engine/Source/Engine.h`
- `Engine/Source/Agent/Commands/`: moved `AgentCommandsAudioStreaming.h/.cpp` and `ClientDesyncProbe.h/.cpp`; new `DescribeScene.h/.cpp` and `FrameEdit.h`
- `Projects/BrokenEngineSandbox/Source/Agent/`: `AgentCommands.h/.cpp`, `AgentCommandsClient.cpp`, `AgentCommandsServer.cpp`, `AgentCommandsServerQueries.h/.cpp`, `AgentScene.h/.cpp`, `Commands/ServerSimulationHarnessRigs.cpp`, `Commands/ServerFrameEdit.cpp`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/*.vcxproj(.filters)`
- `.agents/skills/agent-harness/references/command-reference.md`; new `launch.md`, `replay.md`, `server-window.md` beside it; `SKILL.md`, `references/worker.md`, `references/renderdoc.md`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` and `AgentHarness/{commands-client,commands-server,launch,replay}.md`; `AgentHarness/server-window.md` (deleted)
- `Engine/Source/Agent/AGENTS.md`, `Projects/AGENTS.md`, `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md`, `Projects/BrokenEngineSandbox/Source/AGENTS.md`

## In scope

- `AgentCommandsShared`: `CoordinateFromParameter`, `AgentFloatValue`. `AgentCommandsClientGeneric`: `RequireAcceptedClient`, `AppendUiStateFields`, `BuildDescribeUi`'s UI lines, `CommandSendRawPacket`'s accepted-client check, and three `ExecuteClientAgentCommand` rows.
- `AgentCommandsServerGeneric.*` with the hook declarations; its `Engine.h` include.
- `Engine/Source/Agent/Commands/`: the two moved modules (namespace, includes, `RequireAcceptedClient`, desync core access, the `TailFrame` lookup), `DescribeScene.*`, `FrameEdit.h`.
- Game: `AgentCommands.h:3-10,21-26` (the `engine::Client` forward declaration
  and the two deleted helpers); `AgentCommands.cpp:30-34`;
  `AgentCommandsClient.cpp:3-4,18-58,79,82,118-132`;
  `AgentCommandsServer.cpp:82-122,179-242` and the matching
  `ExecuteAgentCommandServer` rows, plus definitions of
  `AppendAgentServerStatus` and `AppendAgentCpuTimerFields`;
  `AgentCommandsServerQueries.h:13` and the `CoordinateFromParameter` call
  sites; `AgentScene.h/.cpp` (`CommandDescribeScene` becomes
  `DescribeSceneUnits`); `ServerSimulationHarnessRigs.cpp:51-280` and
  `ExecuteServerSimulationHarnessRigCommand`, plus the definition of
  `AppendReplayTransferCaptureCounts`; `ServerFrameEdit.cpp` regions moved by
  Stage 4 and their includes. The game deletes its moved
  `Commands/AgentCommandsAudioStreaming.*` and `Commands/ClientDesyncProbe.*`.
- Project membership for every added, moved, and removed file (`/update-vcxproj`).
- The Stage 5 documents and links.

## Out of scope

- `set_client_grid_coord`, `release_client_grid_coord`, `inject_payload` with
  its `Fields()`-table parser (its queue is the engine `ServerSpawnQueue`),
  `query_frame`/`query_omnis`/`query_collection` and their extractors (including
  `Vector3ToJson`, a direction writer, not a position), `timescale`, and the
  `save`/`load`/`reset` bodies with `BareFilenameParameter` (no engine
  consumer). `Documents/Plans/Engine/EngineOwnedSaveSettingsAndSwitches.md`
  renames the save calls in `AgentCommandsServer.cpp`.
- The game dispatcher `game::ExecuteAgentCommand` beyond the one server row, and its declaration.
- `ReplayHarnessRigs.*`, `game::ReplayTransferCaptureCounts`, and `CountCapturedReplayTransfers`, which `Documents/Plans/Engine/RemoveSandboxGameplayFromEngine.md` keeps as neutral hooks; `ClientDesyncCore` ownership (owned by `SharedSessionProtocol.md`).
- `Invoke-HarnessClaim.ps1`, its hub parser, and any agent-harness script; the script and skill changes `Documents/Plans/ChangeWorkflow/ProjectAwareTooling.md` owns.
- `verification.md`, `cross-cell.md`, the game entries of `commands-server.md`/`commands-client.md`, and the project durable caveats.
- Any change to a command's success fields, any new command, parameter, or validation, and any compatibility shim for old names or paths.

## Acceptance criteria

| Criterion | Evidence |
|---|---|
| Builds | `/compile` builds the `BrokenEngineSandbox` client and server in Debug and Release x64 with no new warnings (Release compiles the `!kbDebugInput` branches of the moved replay and frame-edit handlers); `/update-vcxproj` validation passes |
| No gameplay names in engine files | `git grep -n -E "Blaster|Spaceship|Missile|Omni|bUseMissiles|Fleet"` over `AgentCommandsServerGeneric.*`, `Commands/DescribeScene.*`, `Commands/ClientDesyncProbe.*`, `Commands/AgentCommandsAudioStreaming.*`, and `Commands/FrameEdit.h` returns nothing |
| Duplicates gone | `git grep -n -E "ClientGridCoordinateValue|RequireHarnessRigClient|FiniteFloatFromValue"` returns nothing outside `Documents/` |
| Same server fields | Debug server via `/agent-harness`, run once on a build of the claim's base commit and once on the changed build with the same sequence: `status`; `pause {"paused":true}`; `query_profile {}`; `pause {"paused":false}`; `replay_record {"start":true}` and poll `status.recording:true`; `replay_transfer_capture`; `replay_record {"start":false}`; `replay_drop_retained_end_frame {"coord":[0,0]}` with recording stopped; `replay_inject_persistence_failure {"stage":"bogus"}`; `read_frame {"coord":[0,0],"collection":"frame"}` and `{"coord":[0,0],"collection":"omnis","index":0}`; one `edit_frame` write of `pfArmors` on that row. Each success response's recursive `result` key set and value types match the baseline run; each failure text matches |
| Same client fields | Same paired runs on the Debug client: `describe_scene {}` and `{"includeUnits":false,"maxUnits":0}`; `desync_probe {}`; `audio_streaming_harness_rig {"action":"inspect"}`; `set_client_grid_coord {"coord":[1.5,0]}` and `{"coord":[4294967296,0]}`. Key sets, value types, and failure texts match |
| Island readiness | `Wait-IslandSceneReady.ps1` exits `0` with artifact `Code` `ready` on the changed build |
| Replay determinism | The moved `references/replay.md` `### Replay determinism acceptance` passes on the changed Debug build |
| Documentation | `git grep -n "BrokenEngineSandbox/Documents/AgentHarness" -- .agents Engine` returns nothing; every command name dispatched by `ExecuteSharedAgentCommand`, `ExecuteClientAgentCommand`, and `ExecuteServerAgentCommand` has exactly one schema entry, in `command-reference.md`, and none in the project `commands-*.md`; `command-reference.md` calls the server window engine-owned; `Engine/Source/Agent/AGENTS.md` `## Game contract` lists every `game::` name the `git grep` in Stage 5 finds |

## Notes

- `dependsOn`: `SharedSessionProtocol.md` gives `desync_probe` an engine-owned desync core; `GameShellIntoGameBase.md` adds `engine::Cell::TailFrame()` and leaves the two agent ring-tail sites to this Plan; `FrameEditRows.md` rewrites `ServerSimulationHarnessRigs.cpp` and the `status` fields Stage 2 moves, and `FrameEditCellPins.md` rewrites `ServerFrameEdit.cpp` and adds server commands beside them, so Stage 4 splits their final shape.
- `Documents/Plans/Engine/SharedEngineProjectFiles.md` may change where engine files are listed; `/update-vcxproj` puts this Plan's files into whatever layout is current at claim time.
