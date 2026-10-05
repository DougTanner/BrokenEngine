<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-05T23:35:09.141Z","dependsOn":["Documents/Plans/Game/InjectPayloadCommand.md"]} -->
# `edit_frame`: direct agent-harness writes to Frame collection columns and frame scalars

Line numbers cite baseline `63669fbb61d2f1dbc0b16ae616c7ea661eb57a87`. Where a statement here and the code disagree, the code wins; report the contradiction instead of matching one side to the other.

## Dependency

This Plan depends on `Documents/Plans/Game/InjectPayloadCommand.md` (the `inject_payload` Plan). Execute it only after that Plan has landed. That Plan creates:

- the generator `.agents/scripts/Write-AgentFieldNames.ps1` (parameter `-Check` only; exit codes 0 written or up to date, 1 stale under `-Check`, 2 parse failure; deterministic UTF-8 without BOM, LF, one trailing newline; paths resolved from `$PSScriptRoot`, no input- or output-path parameter). Its grammar: inside each `StatusChangeData` variant struct's braces, found by brace depth from its `struct NAME` line, it emits every `TYPE NAME = INIT;` or `TYPE NAME {};` line whose name does not start with `operator`, and ignores every other line; it exits 2 only for a missing variant declaration or a variant alternative with no struct definition;
- the generated header `Projects/BrokenEngineSandbox/Source/Agent/Commands/AgentFieldNames.h` (server `ClInclude` under `Game\Agent\Commands`; whole-file `#if defined(BT_SERVER)` inside `#pragma once`; no `#include`; `template <auto pMember> inline constexpr std::string_view kStatusChangeFieldName {};` plus one specialization per payload member), included by `ServerSimulationFixtures.cpp`;
- the `.agents/scripts/Invoke-StaticChecks.ps1` row `agent-field-names`, which runs the generator with `-Check` through `Invoke-StaticCheckProcess`, triggers on any inventory entry whose `path` or `oldPath` is `StatusChange.h`, the generated header, or the generator, and joins the composed-script presence check;
- the line in `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` `## Contracts` (`:15`) naming that header and its regenerate command.

This Plan extends that script, header, and row; it never redesigns or renames them.

Before editing, confirm each of those exists at the landed tree under exactly those names. If any differs, stop and report the difference to main instead of adapting this Plan.

## Context

User direction: "we should allow the AgentHarness to affect arbitary structs inside the Frame directly (for testing puposes) so it can perfect set up its test cases or modify a save the same Frame it is loaded".

Today the server harness reads frames (`query_frame`, `query_players`, `query_collection`; `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp:157-209`) and writes only through deterministic tick input. Nothing sets an arbitrary column. `Documents/Investigations/AgentHarnessFineGrainedControl.md:79-86` says a mutation from the command drain "would broadcast nothing, so every connected client desyncs and every replay checksum breaks". That holds for a bare write. This Plan pairs the write with the load path's existing client resynchronization and refuses it while a replay is recording or playing, so neither failure occurs.

User decisions (binding):

1. After an edit, clients are resynchronized through the existing `game::OnStateReplaced()` (`Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:46-49`).
2. The command is refused while recording, while playing back, or while a save- or load-replay flag is pending. The harness documentation states that recording edits into replays is a possible future feature that could be built if an agent needs it.
3. Rows are addressed by index only. The editable members are every server collection column plus the frame scalars `fSpawnTimer`, `uiNextUuid`, `uiFrameIdentifier`, `enemyAlignment`, and `playerAlignment`. `randomEngine` and the `alignments` table are not editable.
4. `engine::Id<T>`-typed columns are refused. After writing, the command runs the collections' `PostRead` hooks, the `puiTypeIndices` bound, and `AdmitSpawnTimer`, and rolls back the whole batch on failure. The goal is that an edited frame is always one a save could hold. Row add and remove are out of scope.

The author also recommends, as the shape this Plan adopts: the edit applies immediately to `pCurrent` at the agent drain with no pause requirement; the command exists only on `kbDebugInput` builds; keys are verbatim C++ member names from a generated table per collection pair, client-only columns are skipped, and the generator adopts the `Test-CollectionLayout.ps1` declaration grammar; `OnStateReplaced()` is skipped when no handshaken client exists.

The author's recommendations, each with its rationale:

1. Extend the first Plan's one script, one header, and one static-check row rather than adding a second set. The `kStatusChangeFieldName` template keeps its name because it still names only payload fields.
2. Follow the first Plan's rule that the generator copies names and never maps types: the generated table holds `{name, pointer to member}`. The element type tag is the member pointer's own C++ type, read at compile time. That makes the Id refusal a compile-time trait, and an unsupported column type fails the build instead of needing a hand-kept type list in the script. The column's element type is therefore carried by its member pointer, never by generated text.
3. Write an `XMVECTOR` as `[x,y,z]` and keep the element's existing W lane. That matches the `local` and `dir` shapes `query_players` returns (`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:45`). It also rules out the W-lane `ASSERT` in `common::ValidateVector` (`Common/Math/MathUtils.h:33-48`) without per-column knowledge of which vectors are positions.
4. Use one pass with an undo log instead of a separate validate pass followed by a write pass. Each write records the element's old bytes, and any exception (parse or post-check) restores them in reverse order. That gives the same all-or-nothing result with half the traversal code.
5. Reuse `QueryFrame` for the frame lookup by making it non-static and non-const, rather than writing a second lookup that would duplicate the `coord has no loaded frame` and `coord frame not ready` rule (`AgentCommandsServerQueries.cpp:140-155`).
6. Extract the two checks `ValidateAfterRead` runs (`Engine/Source/Frame/Collections/Collection.h:73-96`) into a stream-free `ValidateCollectionValues`, and export `AdmitSpawnTimer` (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:17-23`). Without that, the command would have to duplicate both validators.
7. Do not call `ComputeActiveSet` after the resynchronization. `ServerLoad` calls it right after `OnStateReplaced()` (`GameSaveLoad.cpp:77-78`), but every server update recomputes the active set in `PrepareActiveSet` (`Engine/Source/GameBase.cpp:352`, `:944-955`), and an edit changes no cell membership.
8. Do not refuse while a fixture queue holds an entry; document it instead. `OnStateReplaced()` resets the broadcaster and the transfer manager (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:403-406`), and those resets clear both `inject_payload` queues (`Engine/Source/Network/Server/ServerBroadcaster.cpp:277-282`; `Engine/Source/Network/Server/ServerTransferManager.cpp:368-373`), exactly as `load` and `reset` already do. The documented recipe is to edit first and inject second.

## Design

### Where the frame is and when the write happens

- Server frames are one `CoordFrames` per cell with `pCurrent` and `pNext`, held in `gpGame->mCoordinateFrames` (`Engine/Source/GameBase.h:64-72`, `:244`). `RunFrameTick` builds `pNext` from `pCurrent` and stamps its `uiSharedCrc` (`Engine/Source/Frame/FrameBase.cpp:268-289`).
- The agent drain runs at the top of `ServerUpdate`, before `SaveLoadReplay` and the tick loop (`GameBase.cpp:309-316`). A paused update runs zero ticks (`:334-338`).
- So `edit_frame` writes `pCurrent` in place during the drain, whether the server is paused or not, and the next tick derives from the edited frame. After `load {"pauseAfterLoad":true}`, `pCurrent` is the loaded save (`Engine/Source/File/GridSave.cpp:137-140`), which covers the case of modifying a save in the same frame it was loaded.

### Command shape

`edit_frame {"coord":[x,y], "edits":[{"collection":"players","index":0,"writes":{"pfArmors":0.25,"pVecPositions":[10,-20,0]}}, {"collection":"frame","writes":{"fSpawnTimer":0.25}}]}`

- Top-level keys are exactly `coord` and `edits`; any other key throws. The frame comes from `QueryFrame(rParameters)`, so the cell must be loaded and its `pCurrent` present. `edits` is a non-empty array of objects.
- `collection` is one of `players`, `spaceships`, `missiles`, `blasters`, `explosions`, `pushers`, or `frame`:
  - Each collection name addresses one Interpolate/PostRender pair: game pairs `Projects/BrokenEngineSandbox/Source/Frame/Frame.h:61-65`, `:101-105`, engine pairs `Engine/Source/Frame/FrameBase.h:71`, `:77`, `:166`, `:172`. One `index` addresses both halves, because paired rows share an index (`Engine/Source/Frame/Collections/AGENTS.md:13`). The column name selects the half. Keys are exactly `collection`, `index`, `writes`. `index` is a JSON integer with `0 <= index < iCount` of the pair's PostRender half.
  - `frame` addresses the five frame scalars. Keys are exactly `collection` and `writes`; an `index` key throws.
- `writes` is a non-empty object. Each key must name a column of that pair, or a scalar for `frame`, from the generated header; any other key throws. That includes the name of a client-only column, which the header does not contain.
- JSON value form by element type, the only types the command parses. One function template over the element type, with a dependent `static_assert` for any other type:
  - `float`: a JSON number within `±std::numeric_limits<float>::max()`, so the stored `float` is finite. The transport parse already rejects a literal that overflows `double`, such as `1e999` (`Engine/Source/Agent/AgentCommandServer.cpp:232-240`; nlohmann `out_of_range.406`, `ThirdParty/tinygltf/json.hpp:11183-11189`), so every parsed number is a finite `double` and only the narrowing to `float` can overflow.
  - `int32_t`, `uint8_t`, `uint16_t`, `uint64_t`: a JSON integer (`is_number_integer()`, so `1.5` and `1e0` fail) within the type's range.
  - `XMVECTOR`: `[x,y,z]`, three numbers under the `float` rule. W is kept from the element's current value (recommendation 3).
  - `common::Flags<E>`: a JSON integer within `std::underlying_type_t<E>`, stored as `Flags<E>(static_cast<E>(value))` (`Common/Flags.h:16`). `Flags<E>::Underlying_t` is private (`Common/Flags.h:9-12`).
  - `engine::AlignmentIdentifier`: a JSON integer within `uint32_t` (`Engine/Source/Frame/Alignments.h:7-9`).
  - `engine::GlobalId`: a JSON integer within `int64_t` (`Engine/Source/Frame/Collections/CollectionId.h:10-14`).
  - `engine::ClientGuid`: `[uiHigh,uiLow]`, two JSON integers within `uint64_t` (`Engine/Source/Network/NetworkProtocol.h:96-101`).
  - `engine::GridCoord`: `[x,y]` through the existing `CoordinateFromParameter(rWrites, <column name>)` (`AgentCommandsServer.cpp:280-292`).
  - `engine::Id<T>`: always throws `'<column>' is an identity or handle column and cannot be edited`. That covers `players` `pIds` (`Players.h:259`) and `pPushers` (`:79`), `spaceships` `puiPushers` and `puiRegistryIds` (`Spaceships.h:54-55`), `missiles` `puiRegistryTargets` (`Missiles.h:125`), and `pushers` `pIds` (`Pushers.h:95`).
  - An array-of-pointer column (`explosions` `pfTrailTimes`, `Explosions.h:199`) takes a JSON array of exactly `std::extent_v` values of its element type. Value `j` writes `pfTrailTimes[j][index]`.
- Writes apply in request order, edit by edit and key by key, each first recording an undo entry that holds the target address and its old bytes. Every element type above is trivially copyable and at most 16 bytes: `static_assert` both.
- After all writes, the post-checks run:
  - For each pair touched by the batch, the new `engine::ValidateCollectionValues` on both halves.
  - When a `frame` edit wrote `fSpawnTimer`, `AdmitSpawnTimer(rFrame.interpolate.fSpawnTimer)`.
  - When a `frame` edit wrote `uiNextUuid`, the new value must be at least the value the frame held before the command and at most `0x0000'FFFF'FFFF'FFFF`, the largest counter `MakeUuid` keeps unmasked (`Engine/Source/Frame/FrameBase.h:156-160`); otherwise throw `'uiNextUuid' must stay within [current, 2^48)`. A lower counter re-mints a uuid a live row already holds (`Engine/Source/Frame/Collections/CollectionLifecycle.h:48-49`), so the next save or full-state read throws on the duplicate map key (`Engine/Source/Frame/Collections/Collection.h:217-220`) and the registry build `ASSERT`s on a duplicate id (`Engine/Source/Frame/FrameRegistry.cpp:89`, `:96`). A value at or above 2^48 aliases an earlier counter through that mask.
- On any exception from parsing or a post-check, the undo log restores every recorded element in reverse order and the exception is rethrown into the drain's failure envelope (`Engine/Source/Agent/AGENTS.md:18`). The frame is then byte-identical to before the command, and its CRC is untouched.
- On success:
  - Set `rFrame.postRender.uiSharedCrc = rFrame.Crc()`, mirroring the in-place recompute rule (`Engine/Source/Network/Server/AGENTS.md:33`; `ServerTransferManager.cpp:163-172`).
  - Count the handshaken clients in `engine::gpServer->mClients` (`bHandshakeComplete`, as `CommandStatus` does at `AgentCommandsServer.cpp:88-95`). When the count is nonzero, call `game::OnStateReplaced()`.
  - Respond `{"edited":<number of written keys>,"resynchronizedClients":<that count>}`.

### Refusals, checked before any write

- A non-`kbDebugInput` build (`Projects/BrokenEngineSandbox/Source/Pch.h:34`, `:52`, `:70`): `throw std::runtime_error("edit_frame requires kbDebugInput build")`, using the `if constexpr (!kbDebugInput)` idiom (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:51-58`).
- A recording is open (`!engine::gpReplay->mReplayWriters.empty()`, `Engine/Source/File/Replay.h:27`), playback is running (`gpGame->mbReplaying`, `Engine/Source/GameBase.h:262`), or `engine::GameFlags::kSaveReplay` or `kLoadReplay` is set (`GameBase.h:58-59`).

Queued `inject_payload` entries are not a refusal (recommendation 8).

### Generator extension

`.agents/scripts/Write-AgentFieldNames.ps1` keeps everything the first Plan specified, including its `StatusChange.h` grammar, and adds Frame tables to the same output:

- Hard-coded inputs, in this order:

  | JSON collection | Header | Interpolate struct | PostRender struct |
  |---|---|---|---|
  | `players` | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h` | `PlayersInterpolate` | `PlayersPostRender` |
  | `spaceships` | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.h` | `SpaceshipsInterpolate` | `SpaceshipsPostRender` |
  | `missiles` | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.h` | `MissilesInterpolate` | `MissilesPostRender` |
  | `blasters` | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.h` | `BlastersInterpolate` | `BlastersPostRender` |
  | `explosions` | `Engine/Source/Frame/Collections/Explosions/Explosions.h` | `engine::ExplosionsInterpolate` | `engine::ExplosionsPostRender` |
  | `pushers` | `Engine/Source/Frame/Collections/Pushers/Pushers.h` | `engine::PushersInterpolate` | `engine::PushersPostRender` |

- Struct location, as the first Plan's grammar and `Test-CollectionLayout.ps1` do (`.agents/scripts/Test-CollectionLayout.ps1:271-290`): every `//` comment is stripped first; the struct's line is the one whose code starts with `struct <unqualified name>` followed by whitespace, `:`, or the end of the line. The body opens at the first `{` at or after that line, so a base clause spanning several lines is accepted (`Engine/Source/Frame/Collections/Explosions/Explosions.h:150-152`), and ends where the brace depth returns to the struct line's depth. A struct line found zero times or more than once in its header exits 2.
- Columns: lines at the struct's own brace depth matching either `Test-CollectionLayout.ps1` declaration form, `^[A-Za-z_][\w:]*\s*\*\s*__restrict\s+(\w+)\s*[=;]` or `^[A-Za-z_][\w:]*\s*\*\s+(\w+)\s*\[[^\]]+\]\s*[=;]` (`.agents/scripts/Test-CollectionLayout.ps1:12-15`, `:298`), kept in declaration order, Interpolate struct first.
- Guards, tracked at any depth inside the body:
  - `#if defined(BT_CLIENT)` opens a client-only region, `#else` flips it to server-visible, and `#endif` closes it.
  - A column inside a client-only region is skipped.
  - Any other `#if`, `#ifdef`, `#ifndef`, or `#elif` line inside a body exits 2.
- Scalars are not parsed. The generator emits five fixed specializations and the `FrameScalarColumns` alias as constant text, in this order: `FrameInterpolate::fSpawnTimer` (`Projects/BrokenEngineSandbox/Source/Frame/Frame.h:59`), `engine::FramePostRenderBase::uiNextUuid` and `uiFrameIdentifier` (`Engine/Source/Frame/FrameBase.h:145`, `:150`), `FramePostRender::enemyAlignment` and `playerAlignment` (`Frame.h:98-99`). A renamed or removed scalar fails the build through its member pointer.
- A column name repeated within one pair exits 2. At the baseline, no pair repeats a name, and the inputs yield 79 columns.
- Output, appended after the existing `kStatusChangeFieldName` block of the one header:
  - `#include` lines for the four game collection headers (`"Frame/Collections/Blasters/Blasters.h"`, `"Frame/Collections/Missiles/Missiles.h"`, `"Frame/Collections/Players/Players.h"`, `"Frame/Collections/Spaceships/Spaceships.h"`), sorted, at the top inside the `BT_SERVER` wrap. The engine collections and `Frame.h` come through the game PCH (`Pch.h:101`, `:103`).
  - `template <auto... pMembers> struct FrameColumnList {};`
  - `template <auto pMember> inline constexpr std::string_view kFrameColumnName {};`
  - One specialization per column and per scalar, for example `template <> inline constexpr std::string_view kFrameColumnName<&PlayersPostRender::pfArmors> = "pfArmors";`, grouped under a `// <JSON collection>` line.
  - One alias per pair, `using PlayersFrameColumns = FrameColumnList<&PlayersInterpolate::pVecPositions, ...>;`, and likewise `SpaceshipsFrameColumns`, `MissilesFrameColumns`, `BlastersFrameColumns`, `ExplosionsFrameColumns` (Interpolate columns only, since `ExplosionsPostRender` has none), `PushersFrameColumns`, and `FrameScalarColumns`.
  - The first-line comment names the script and every input header.
- Determinism, encoding, and exit codes are unchanged from the first Plan. The added exit-2 cases are a missing or repeated collection struct, an unrecognized `#if` form inside a collection body, and a repeated pair column name.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.h`, `.cpp` (new): `CommandEditFrame`.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp`: dispatch.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.h`, `.cpp`: `QueryFrame` export.
- `Engine/Source/Frame/Collections/Collection.h`: `ValidateCollectionValues`.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.h`, `Frame.cpp`: `AdmitSpawnTimer` export.
- `.agents/scripts/Write-AgentFieldNames.ps1` (extended), `Projects/BrokenEngineSandbox/Source/Agent/Commands/AgentFieldNames.h` (regenerated), `.agents/scripts/Invoke-StaticChecks.ps1`.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj` and `.vcxproj.filters`.
- Documentation: `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`, `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md`, `Projects/BrokenEngineSandbox/Source/Save/AGENTS.md`, `Documents/Investigations/AgentHarnessFineGrainedControl.md`.

## In scope

- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.h` (new): `#pragma once` outside a whole-file `#if defined(BT_SERVER)` wrap, declaring `void CommandEditFrame(const nlohmann::json& rParameters, nlohmann::json& rResult);` in `namespace game`.
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp` (new, whole-file `BT_SERVER`): `CommandEditFrame` and its file-local helpers:
  - the refusals;
  - the key checks;
  - the element-type value reader template;
  - the Id refusal trait;
  - the column dispatch over the generated `FrameColumnList` aliases;
  - the mapping from each owner struct to its instance in the `Frame`;
  - the undo log and rollback;
  - the post-checks;
  - the CRC recompute;
  - the conditional `OnStateReplaced()` call;
  - the response.

  It includes `Agent/Commands/AgentFieldNames.h`, `Agent/AgentCommandsServerQueries.h`, `Save/GameSaveLoad.h`, `File/Replay.h`, `Network/Server/ServerSession.h`, and `Game.h` as needed.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp`: `#include "Agent/Commands/ServerFrameEdit.h"`, and one `edit_frame` row in `ExecuteAgentCommandServer` (`:294-366`), placed before the `ExecuteServerSimulationFixtureCommand` call.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp` `QueryFrame` (`:140-155`): drop `static` and return `Frame&`. The three query callers keep binding `const Frame&`. `AgentCommandsServerQueries.h`: declare `Frame& QueryFrame(const nlohmann::json& rParameters);` and reword the comment at `:8-11` so it says the lookup is shared with the frame-edit command.
- `Engine/Source/Frame/Collections/Collection.h`: add `template <typename STRUCT> void ValidateCollectionValues(const STRUCT& rStruct)` holding the type-index bound and the `PostRead` call moved out of `ValidateAfterRead` (`:73-96`). `ValidateAfterRead` keeps its `rStream.good()` guard and calls the new function. Behavior is unchanged.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp` `AdmitSpawnTimer` (`:17-23`): drop `static`. `Frame.h`: declare `float AdmitSpawnTimer(float fSpawnTimer);` in `namespace game`.
- Generator `.agents/scripts/Write-AgentFieldNames.ps1`: add the collection inputs, struct location, column and guard grammar, fixed scalar text, uniqueness failure, and output in `### Generator extension`. Update its header comment: the newly recognized forms, the inputs, and the added exit-2 cases.
- Generated header `AgentFieldNames.h`: regenerate it by running `pwsh -NoProfile -File .agents/scripts/Write-AgentFieldNames.ps1`. Never hand-edit it.
- `.agents/scripts/Invoke-StaticChecks.ps1`: add the six collection headers to the `agent-field-names` row's trigger set, under the same both-sides `path`/`oldPath` scan.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj` and `.vcxproj.filters`, through `/update-vcxproj`: `ServerFrameEdit.h` as a `ClInclude` and `ServerFrameEdit.cpp` as a `ClCompile`, beside `ServerSimulationFixtures` (`:384`, `:506`; filters `:477`, `:800`), under `Game\Agent\Commands`.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`:
  - One `edit_frame` row after the `query_collection` row, stating:
    - the schema;
    - the collection names and `frame`;
    - that keys are the C++ member names from `AgentFieldNames.h`;
    - the value forms, including W kept and the array form;
    - the refused identity and handle columns;
    - that `uiFrameIdentifier`, `pGlobalPlayerIds`, and `pClientGuids` are written unchecked: a value duplicating another frame's or player's identity gives odd behavior but no assert;
    - the all-or-nothing rollback;
    - the post-checks, including the `uiNextUuid` range refusal;
    - the `kbDebugInput` and replay refusals;
    - the response.
  - The row's recipe text:
    - `load {"pauseAfterLoad":true}` or `pause`, then `edit_frame`, then `pause {"paused":false}`;
    - edit before `inject_payload`, because with a handshaken client the resynchronization drops every queued `inject_payload` entry, as `load` and `reset` do.
  - The row's resynchronization text: with a handshaken client, each edit runs the load-time resynchronization, which advances the server's load generation (shared 255-per-process budget with `load` and `reset`; exceeding it hits the `ASSERT` at `Engine/Source/Network/Server/Server.cpp:68`), zeroes `status.harvestedTransferTotal`, and makes clients log `ClientSession::OnServerLoad`, drop their coordinate state, and resubscribe. So batch edits into one command.
  - The row's replay sentence: edits are never recorded into a replay, and recording an edit into the replay stream could be built if an agent needs edits inside a recorded scenario.
  - Extend the ownership sentences at `:58` and `:63` to name `Commands/ServerFrameEdit.cpp` for frame edits.
- `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md`:
  - At `:5`, name the focused server frame-edit module as the owner of `edit_frame`.
  - In `## Contracts`, add one bullet stating the direct-edit contract:
    - it writes `pCurrent` at the drain;
    - the batch is all-or-nothing and checked with the save-read validators;
    - identity and handle columns are refused;
    - the CRC is recomputed;
    - it resynchronizes through the Save entry point when a client is connected;
    - it is refused while recording, during playback, or with a pending replay flag.
  - In the bullet the first Plan added at `:15`, add that `edit_frame` keys are the Frame member names from the same `AgentFieldNames.h`, regenerated with the same command after a collection header in `### Generator extension` changes.
- `Projects/BrokenEngineSandbox/Source/Save/AGENTS.md:19`: add the agent frame edit with a connected client to the list of paths sharing the one relink and resynchronization entry point, so the sentence no longer says "all four paths".
- `Documents/Investigations/AgentHarnessFineGrainedControl.md:82-86`: qualify the sentence. A drain-time mutation outside the tick input desyncs clients and replays unless the mutation is paired with the load-time resynchronization and refused during replay, as `edit_frame` does. The paragraph's conclusion for scenario controls in general stays.

## Out of scope

- Any `StatusChangeType`, payload, wire format, `FrameInput::kiVersion`, `Frame::kiVersion`, `engine::kuiProtocolVersion`, save, or replay format change.
- Row add or remove, capacity changes, `idToIndexMap` edits, identity minting, and every `engine::Id<T>` column.
- `randomEngine`, the `alignments` table, `uiSharedCrc` as an input, `transferRequests`, `FrameInterpolateBase::iTick`, `fCurrentTime`, `fDeltaTime`, `FrameInterpolate::gameFlags`, and every `FrameStaticData` member.
- Validation beyond the decided set and the `uiNextUuid` range refusal: no position-in-cell, unit-length, alignment-known, global-id-uniqueness, client-GUID, or `uiFrameIdentifier` checks.
- Client-build code, client commands, and any new query or query field. `query_players` and `query_collection` keep their schemas.
- Recording an edit into a replay stream, or any new replay record.
- `OnStateReplaced`, `ResetClientsForLoad`, `ComputeActiveSet`, `HandleResyncRequests`, and every network, save, and replay algorithm.
- A pause requirement, a deferred or scheduled edit, and an edit response that echoes values.
- Generator features beyond `### Generator extension`: no type mapping, no JSON writer, no client-guarded tables, no further inputs, no scalar parsing, and no path parameter. The generator, header, and static-check row keep their names. The `kStatusChangeFieldName` block and the `inject_payload` command are unchanged.
- `Documents/Architecture/Network.md:19`, whose load-generation statement stays true.
- `Test-CollectionLayout.ps1`, the `validate-skill` and `markdown-links` rows, `Get-SessionChangeInventory.ps1`, and the runner's `schemaVersion`.

## Risk triggers and invariants

Tier 3 (`.agents/references/risk-tiers.md:8-9`).

- Determinism and CRC: the command writes CRC'd simulation state (`Engine/Source/Frame/FrameBase.cpp:12-26`; `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:774-778`) outside `FrameInput`, the first write path of that kind. Preserved invariants:
  - every frame a client, a save, or the next tick observes has `uiSharedCrc == Crc()`;
  - every edited frame passes the save-read collection and spawn-timer validators;
  - a frame's history stays a pure function of start state plus inputs within any recording, because edits are refused while recording or playing back.
- Cross-subsystem: game Agent (new command), engine Frame Collections (`Collection.h` validator extraction, member pointers into engine `Explosions`/`Pushers`), game Frame (`AdmitSpawnTimer` export), game Save/Network resynchronization (`GameSaveLoad.h:15` to `ServerSession.cpp:372-412`), and the shared static-check tooling (`Invoke-StaticChecks.ps1`).
- Threading: main thread only, at the existing drain point (`Engine/Source/Agent/AGENTS.md:9`, `:27`); no tick runs during the drain (`GameBase.cpp:309-316`).
- Trust: unchanged. The agent channel is an exempt developer tool (`Engine/Source/Agent/AGENTS.md:18`). The checks exist so an edited frame is always one a save could hold: the save-read collection validators, the spawn-timer admission, and the `uiNextUuid` range refusal, whose violation would make the next save or full-state read throw and the registry build assert. `uiFrameIdentifier`, `pGlobalPlayerIds`, and `pClientGuids` stay unrefused: no assert or read rejection follows from them, only odd behavior.
- Resynchronization cost: one load generation per edit command with a handshaken client (`Server.cpp:67-71`), shared with `load` and `reset`.

## Acceptance criteria

See `## Execution card` `Acceptance checks`.

## Execution card

### What does this plan do?

It adds one Debug-only server harness command, `edit_frame`, that writes chosen columns of chosen rows of any server collection, plus five frame-level values, directly into a cell's current frame. The batch is checked with the same validators a save load runs and is undone completely if any check fails. The frame checksum is then recomputed, and connected clients are resynchronized the way a load resynchronizes them. The column names are the C++ member names, generated into the existing field-name header by the same script as the `inject_payload` names, and the static check keeps that header current.

### Why this is good for the codebase

Runtime tests today can reach a state only by simulating toward it or by adding a purpose-built command. The user asked for agents to set up exact test states and to modify a save right after loading it. With this command, a harness recipe can load or pause, set exact armor, positions, timers, or flags, and resume, with no new command per scenario. Clients stay in sync because the command reuses the proven load path, and replays stay valid because edits are refused while recording or playing back.

- Goal: `edit_frame` per `## Design` `### Command shape` and `### Refusals, checked before any write`, with column tables from the extended generator per `### Generator extension`, the `agent-field-names` static-check row, and the documentation edits listed under `## In scope`.
- Out of scope: everything under `## Out of scope`:
  - format and version changes;
  - row add or remove and Id columns;
  - `randomEngine`, `alignments`, the clock, and static data;
  - extra validation;
  - client code and new queries;
  - recording edits into replays;
  - the resynchronization and network algorithms;
  - pause or scheduling;
  - generator features beyond the column and scalar tables.
- Tier trigger: Tier 3. The change writes CRC'd deterministic state outside the tick input (determinism surface) and spans independently owned subsystems: game Agent, engine Frame Collections, game Frame, game Save/Network resynchronization, and static-check tooling (`.agents/references/risk-tiers.md:8-9`; evidence under `## Risk triggers and invariants`).
- Interfaces and invariants:
  - `Frame& QueryFrame(const nlohmann::json&)` is exported from `AgentCommandsServerQueries.h` with the unchanged lookup rule.
  - `engine::ValidateCollectionValues(const STRUCT&)` runs exactly the checks `ValidateAfterRead` ran, and `ValidateAfterRead` behavior is unchanged.
  - `game::AdmitSpawnTimer(float)` is declared in `Frame.h` with unchanged behavior.
  - The generated `AgentFieldNames.h` holds `kStatusChangeFieldName` unchanged plus `FrameColumnList`, `kFrameColumnName`, and seven aliases. It is byte-identical to the script's output for the current inputs, and no key string is hand-written in `ServerFrameEdit.cpp`.
  - The command is all-or-nothing. On failure the frame bytes and `uiSharedCrc` equal the pre-command values. On success `uiSharedCrc == Crc()`, and the edited frame passes `ValidateCollectionValues` for every touched pair and `AdmitSpawnTimer`, and its `uiNextUuid` is within [pre-command value, 2^48).
  - `engine::Id<T>` columns are never written.
  - The W lane of every edited `XMVECTOR` is unchanged, and every written `float` is finite.
  - The command is refused on non-`kbDebugInput` builds, while recording, during playback, or with `kSaveReplay` or `kLoadReplay` set. Queued `inject_payload` entries do not refuse it.
  - `OnStateReplaced()` runs exactly when the edit succeeds and at least one handshaken client exists.
- Acceptance checks (via `/agent-harness`, Debug server, no client unless noted):
  The cell under test is `[0,0]`, the coordinate `client_full_state_fixture {"action":"clear"}` reports after clearing (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ClientFullStateFixture.cpp:30-34`, `:147`), which check 4 reads. "The player" is the `query_players` row at `index` 0 on that cell. Create it first with `inject_payload` `SpawnPlayer` and one unpaused tick, then `pause {"paused":true}`.

  | # | Check | Expected observation |
  |---|---|---|
  | 1 | Paused. Read `query_players` and `status.tick`. `edit_frame` the player: `pfArmors:0.25`, `pfShields:0.5`, `pVecPositions:[10,-20,<its current local z>]` | response `edited:3`, `resynchronizedClients:0`. `query_players` shows that row's `armor` 0.25, `shield` 0.5, `local` `[10,-20,z]`, and an unchanged `uuid` and `globalId`. `status.tick` and `paused:true` are unchanged |
  | 2 | Continue 1: `pause {"paused":false}`, wait until `status.tick` advances by at least 2, then pause and `query_players` | the same `uuid` row has `armor` ≤ 0.25. `get_logs` has no new `Assert` line |
  | 3 | Save modification: `save {"file":"EditBase.save"}`, `load {"file":"EditBase.save","pauseAfterLoad":true}`, `edit_frame` the player `pfShields:0.125`, `save {"file":"Edited.save"}`, `load {"file":"Edited.save","pauseAfterLoad":true}`, `query_players` | both loads return `resetToFresh:false`. After the second load, that row's `shield` is 0.125 |
  | 4 | One connected client subscribed to `[0,0]`. Pause the server, read `status.tick` = E, and note the client's `client_full_state_fixture {"action":"clear"}` `loadGeneration`. `edit_frame` the player `pfArmors:0.5` on `[0,0]`. While still paused, poll `clear` until it reports `loadGeneration` + 1 and `coordState.present:true` with `lastFullStateTick` E, so the client has adopted the edited tick. Then `pause {"paused":false}` and watch 128 ticks | response `resynchronizedClients:1`. The server log has one new `ServerSession::ResetClientsForLoad` and the client log one new `ClientSession::OnServerLoad`. The paused poll reaches the stated adoption. After unpausing, `coordState.confirmedTick` passes E, and no new `CONFIRMED DESYNC` line appears in client logs, which observes that the published CRC equals the edited frame's recomputed `Crc()` |
  | 5 | Rejections, each its own command on a paused server: unknown top-level key; unknown `collection`; `frame` with `index`; unknown column; client-only column `pfShieldRotations`; `index` equal to `total`; `index:-1`; `pfArmors:"x"`; `frame` `uiNextUuid:0` (below its current value); `frame` `uiNextUuid:281474976710656` (2^48); `puiPendingWeaponModeTicks:256`; `puiPendingWeaponModeTicks:1.5`; `pVecPositions:[1,2]`; `pIds:[0]` and `pPushers` on `players`; `pIds` on `pushers`; `frame` `fSpawnTimer:0.5` | `ok:false` each time. `query_players` for the player equals its pre-command values |
  | 6 | Rollback: `edit_frame` the player `{"pfArmors":0.1,"pfNextBlasterFireTimes":1.0}` (the PostRead range, `Players.h:198-201`). Then a two-edit batch: the player `pfArmors:0.1`, then `players` `index` equal to `total` | `ok:false` for both batches. The player's `armor` is unchanged |
  | 7 | Other pairs and scalars: `pushers` index 0 `pfRadii:5`; `frame` `fSpawnTimer:0.25`; `frame` `fSpawnTimer:0.75` | the first two succeed with `edited:1`. The third fails with the `FrameInterpolate fSpawnTimer` message |
  | 8 | Replay refusal: while paused, `replay_record {"start":true}` (pending), then `edit_frame` → expect refusal; cancel with `replay_record {"start":false}`. Unpause, start recording, poll `recording:true`, `edit_frame`; stop and poll `recording:false`. `replay_play`, poll `replaying:true`, `edit_frame`; cancel playback | each `edit_frame` returns `ok:false` |
  | 9 | Edit then record: `edit_frame` the player `pfArmors:0.3`, then record about 64 ticks, stop, and run the replay determinism check | CRC match, an `End replay ..., looping` marker, and no `LogDifferences CRC` line |
  | 10 | Profile server: `edit_frame` | `ok:false` with `edit_frame requires kbDebugInput build` |
  | 11 | On the finished tree, record the `Get-FileHash` of `AgentFieldNames.h`, run `pwsh -NoProfile -File .agents/scripts/Write-AgentFieldNames.ps1`, and hash again | exit 0 and an identical hash. The file has no BOM, only LF line endings, and one trailing newline. It holds 79 column and 5 scalar `kFrameColumnName` specializations, and none of the `BT_CLIENT` columns (for example `pfShieldRotations`, `pTrails`) |
  | 12 | `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot '<worktree root>' -Baseline <session baseline> -IncludeUntracked` | the `agent-field-names` row is `triggered:true` and `pass`. The `validate-skill` and `markdown-links` rows are still present |
  | 13 | By code reading, per `.agents/references/change-workflow.md:122` (no input-path parameter exists): stale and malformed generator input | a new `* __restrict` column in an input struct outside a `BT_CLIENT` region adds one specialization and one alias entry to the in-memory output, so `-Check` exits 1 and the row is `fail`. A repeated pair column name, an unrecognized `#if` form inside a body, or a missing collection struct exits 2, and the row is `blocked`. A change to `StatusChange.h` or any of the six collection headers triggers the row |
  | 14 | By code reading (no harness input reaches these paths): the `float` rule and the type-index bound | the transport parse rejects a literal that overflows `double` before dispatch (`Engine/Source/Agent/AgentCommandServer.cpp:232-240`), and the `float` reader rejects a finite `double` outside `±std::numeric_limits<float>::max()`, so no written `float` or `XMVECTOR` lane is non-finite. `ValidateCollectionValues` holds the `puiTypeIndices` bound moved verbatim from `ValidateAfterRead`, and the command runs it on both halves of every touched pair, so a `blasters` or `explosions` `puiTypeIndices` write at or above `ssize(sTypes)` throws and rolls back |

  Static checks also apply:
  - client Debug and server Debug and Profile builds compile, which proves every generated member pointer exists and every element type has a reader;
  - `git grep -n "QueryFrame"` shows the header declaration, the three query callers, and `ServerFrameEdit.cpp`.
- Roles (Tier 3; same assignments as `Documents/Plans/Game/InjectPayloadCommand.md` `## Execution card`):
  - `researcher` runs `/plan-alternatives`.
  - `reviewer` runs `/plan-audit` and `/plan-simplicity-review`; `/external-grill-plan` follows `/plan-audit`.
  - `implementer` runs `/implement-plan` in three slices, then `/update-vcxproj` for the new files, and `/update-affected-code`:
    - the generator extension, the regenerated header, and the static-check trigger set;
    - the C++ (`Collection.h`, `Frame.h`/`.cpp`, `AgentCommandsServerQueries`, `ServerFrameEdit`, and dispatch), started after the header exists;
    - the documentation.
  - `mechanic` runs `/code-style-review`.
  - `builder` runs `/compile` (client Debug; server Debug and Profile).
  - `reviewer` runs `/repo-code-review`, `/comment-review`, `/coherence-review` (docs and the two PowerShell scripts), and `/adversarial-review`.
  - `/update-claude-docs` and `/progressive-disclosure-review` run next.
  - The landing gate is `/finalize-changes`, with user confirmation.

## Unresolved decisions

None. Every agent-made choice is stated under `## Context` as the author's recommendation with its rationale.
