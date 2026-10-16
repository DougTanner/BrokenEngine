<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-15T23:34:04.876Z","dependsOn":[]} -->
# Remove Sandbox gameplay names from engine C++

Tier 3 (`.agents/references/risk-tiers.md`): the change moves the status-change payload codec (wire and serialization), reorders where server per-update status changes are built and where transfers read their alignment (determinism/CRC), moves the grid-save extension declarations (save), and spans engine and game subsystems. Line numbers cite baseline `f1d140ae`. Where a statement here and the code disagree, the code wins; report the contradiction instead of matching one side to the other. Paths under `Frame/`, `Network/`, `Save/`, `Agent/` and bare file names such as `Game.h` or `SpawnTransfer.cpp` mean `Projects/BrokenEngineSandbox/Source/<path>`; `Engine/` and `Common/` paths are repository-relative.

## Context

The engine must compile against a second game project. Today engine C++ names Sandbox gameplay concepts directly: it includes Sandbox collection and fleet headers, switches over Sandbox status-change types and reads their payload fields, and calls fleet-manager and Players methods. A second game has no `Players`, `Spaceships`, `Blasters`, `Missiles`, or fleets, so none of this compiles there.

The dispatching session recorded these as the user's decisions this session:

- The engine may call expected `game::` classes, globals, and members by name (for example `game::gpServerSession->mpClientManager->OnClientDisconnected`, `GameBase` calling `game::Game` methods, game timer enums, `game::StatusChange`). That hook contract is not changed.
- The engine must not name the Sandbox gameplay concepts Blaster, Spaceship, Missile, Player, `bUseMissiles`, or Fleet.
- Fleets stay game-owned. The engine gets neutral hook calls at its three fleet touch points: a per-update hook in `ServerBroadcaster` before the frame inputs are snapshotted, an "owned entity changed cell" hook in `ServerTransferManager`, and a neutrally named game save-extension header in `GridSave.h`. No fleet code moves into the engine.
- The game owns each status-change type's payload read/write. Engine `NetworkSerialization` keeps only the per-type group envelope, ascending type order, and LZ4 framing. Wire bytes and CRC stay identical. This overrides `Engine/Source/Network/AGENTS.md:41-42` and `Engine/Source/Network/Server/AGENTS.md` `## Transfers and Publication`, which call the coupling deliberate; both are rewritten here.
- No backward compatibility, and a changed format bumps its owning version. This Plan changes no format (see `## Risk triggers and invariants`), so no version moves.

Verified sites (baseline):

- Sandbox includes in engine: `Engine/Source/Network/Server/ServerTransferManager.cpp:10-13` (`Blasters.h`, `Missiles.h`, `Spaceships.h`, `ServerFleetManager.h`), `Engine/Source/Network/Server/ServerBroadcaster.cpp:11` (`ServerFleetManager.h`), `Engine/Source/Graphics/Render/MainUniforms.cpp:10` (`Players.h`), `Engine/Source/File/GridSave.h:5` (`Network/Server/ServerFleetSerialization.h`).
- Codec: `Engine/Source/Network/NetworkSerialization.cpp:12-120` holds the four per-transfer-type writers and readers, and `:140-186` and `:269-315` switch over `kSpawnPlayer`, `kTransferBlaster/Spaceship/Missile/Player`, `kDestroyPlayer`, `kUpdatePlayer`, `kUpdateFleet` and read `SpawnPlayerData`, `DestroyPlayerData`, `UpdatePlayerData`, `UpdateFleetData`, and `TransferData` fields. The envelope is `:132-133` (type byte, `uint16_t` count) and `:194-243` (group by type, ascending emission). `DefaultDataForType` is called at `:267`. `game::NetworkSessionContract` (`Network/NetworkSessionContract.h:32-39`) already wraps `CompressStatusChangeBatch`/`DecompressStatusChangeBatch` and stays as is.
- Broadcaster: `ServerBroadcaster.h:16-22,31,37` declare `PendingUpdatePlayerRequest` (`bUseMissiles`), `ProcessUpdatePlayerRequests`, and `mPendingUpdatePlayerRequests`. `ServerBroadcaster.cpp:31-63` runs, only when the update advances, the waiting-spawn prune (`mClientsWaitingForSpawn`, `LookupFleetWantedCoord`), `ProcessUpdatePlayerRequests`, `TickFleetTimers`, `ProcessFlagshipUpdates`, and `SpawnWaitingClients`, in that order, with ordering comments at `:54-62`. `:222-271` builds `kUpdatePlayer`/`UpdatePlayerData`. The game writes the queue at `Network/Server/ServerSession.cpp:114` and clears it at `:253`. The engine `ResetState` (`ServerBroadcaster.cpp:273-278`) clears it, and only `ServerSession.cpp:391` calls that.
- Transfer manager: `ServerTransferManager.cpp:82,104,137,183,325` test `kTransferPlayer`. `:104,108,194,227,240-250,267-268,331,338` read `globalPlayerId`. `:144` passes `game::gpGame->mPlayerAlignment` to `game::SpawnTransfer`. `:200,210` log `pBlasters`, `pSpaceships`, `pMissiles` counts. `:236-242` use `game::gpServerSession->mClientPlayers`. `:252,268` call `mpFleetManager->OnPlayerTransferred`. `ServerTransferManager.h:16` declares `ClientTransferInfo::globalPlayerId`.
- Alignment: `ReconcileReplayTick.cpp:133` already passes `pNext->postRender.playerAlignment`. `Game::InitializeFramePostRender` (`Game.cpp:656-663`) seeds every frame's `playerAlignment` from `mPlayerAlignment`, and `FramePostRender::Update` (`Frame/Frame.cpp:195`) carries it forward. `SpawnTransfer.cpp:39-41,48` uses the parameter for Blaster wind-trail defaults and the CRC'd Missile target flags.
- Client replay log: `Engine/Source/Network/Client/ReconcileReplayTick.cpp:48-80,122-163` counts and logs per Sandbox transfer type. `ReconcileReplayCrc.cpp:94-115` already counts by type index and prints `game::StatusChangeTypeName`.
- Server window: `Engine/Source/Server/ServerDisplay.cpp:38-41,94-106,152-155,305,596-599` reads `game::ServerCellStats::iPlayers/iSpaceships/iBlasters/iMissiles` (`Frame/ServerCellStats.h:11-20`, filled at `Frame/ServerCellStats.cpp:17-29`, summed at `:31-85`).
- Debug render: `MainUniforms.cpp:553` calls `game::PlayersInterpolate::DebugRender`.
- Server mutex: `Engine/Source/Main.cpp:878` hardcodes `"BrokenEngineSandboxServer"`. The server `game::kGameName` (`Game.h:27`) is `"Broken Engine Sandbox Server"`. Nothing else in the repository opens that mutex name.
- Names only: `Engine/Source/Graphics/CameraBase.h:138-140` and `CameraBase.cpp:69-71,109,198-212` (`mVecLastKnownPlayerPosition/Velocity`, `mfLastKnownPlayerTime`, which track the camera target).
- Engine comments and assert text naming gameplay concepts: `GridSave.cpp:86`, `Replay.cpp:277-278,556`, `GameBase.h:239`, `GameBase.cpp:818`, `AnimationData.h:41`, `CameraBase.cpp:116`, `Main.cpp:303`, `Client.h:106`, `NetworkProtocol.h:111-112`, `Server.cpp:634,643,708`, `ServerDisplay.cpp:149`, plus the comments in the files above. Also `Common/Math/Random.cpp:37`.
- Human-player sense, not the gameplay concept, which stays: `Engine/Source/Network/Client/Client.cpp:827` (chat log), `Engine/Source/Ui/Screens/ChatScreen.cpp:84` (chat UI text), `Engine/Source/Ui/GraphicsQualityWrappersBase.h:19`, `Engine/Source/Graphics/Managers/PipelineManager.cpp:181`, `TextureManager.cpp:496`, `WorldLightingShadowPipelines.cpp:144` ("player-facing").
- Allowed hooks already neutral: `Engine/Source/Agent/Commands/ReplayHarnessRigs.h:32` holds `game::ReplayTransferCaptureCounts` without naming its fields, and `ReplayHarnessRigs.cpp:286` calls `game::CountCapturedReplayTransfers`.

## Design

One landing, implemented in the stages below. Each stage should leave both builds compiling, so a stage-scoped review sees a buildable tree. The author recommends that order because each later stage depends only on earlier renames.

### Stage 1 — game owns status-change payloads

- New shared game files `Network/StatusChangeCodec.h/.cpp`, in both the client and server projects, beside the other top-level game payload code (`Network/AGENTS.md` `## Layering`):

```cpp
namespace game
{
struct StatusChange;
// Payload bytes of one item; the engine codec writes the group envelope around it.
void WriteStatusChangePayload(uint8_t*& rpCursor, const StatusChange& rChange);
// rChange.eType is set; seats DefaultDataForType(eType) and reads that type's payload.
void ReadStatusChangePayload(const uint8_t*& rpCursor, StatusChange& rChange);
}
```

- Move `NetworkSerialization.cpp:12-120` (the eight per-transfer-type static functions) and the per-type `switch` bodies at `:140-186` and `:269-315` into that `.cpp` without changing anything else, keeping every field's order and width. It uses the engine cursor primitives (`Engine/Source/Network/NetworkCursor.h`).
- Engine `SerializeGroup` keeps the type byte, the `uint16_t` count, the per-index loop, and the per-item `ASSERT` against `kiMaxStatusChangeBytesPerItem`, with `game::WriteStatusChangePayload(rpCursor, pChanges[i])` in place of the switch. `DeserializeStatusChangeBatch` keeps the envelope loop, sets `rChange = {}` and `rChange.eType`, and calls `game::ReadStatusChangePayload`. The engine call to `DefaultDataForType` moves into the game reader. The author recommends free functions over new `NetworkSessionContract` members because the codec already calls game free functions (`DefaultDataForType`, `StatusChangeTypeName`) and is not templated on the contract.
- Rewrite the `NetworkSerialization.h:5-7,16-18` comments to say the engine owns the envelope, the ordering, and the framing, and the game owns per-type payload bytes.

### Stage 2 — per-update server hook

- Move `PendingUpdatePlayerRequest` (unchanged fields) and `mPendingUpdatePlayerRequests` from `engine::ServerBroadcaster` to `game::ServerSession` (`Network/Server/ServerSession.h`), and move `ProcessUpdatePlayerRequests` (body `ServerBroadcaster.cpp:222-271`, log prefix renamed to `ServerSession::`) to a private `ServerSession` member.
- Add the public `void game::ServerSession::BuildAdvancingFrameInputs()`. Its body is `ServerBroadcaster.cpp:35-63` moved verbatim, comments included: the waiting-spawn prune, `ProcessUpdatePlayerRequests()`, `mpFleetManager->TickFleetTimers()`, `mpFleetManager->ProcessFlagshipUpdates()`, `mpClientManager->SpawnWaitingClients()`, in today's order.
- Engine `BuildFrameInputs` keeps the input reset, the broadcast-map clear, the active-coordinate seeding, and the `bAdvancing` gate, and inside the gate calls only `game::gpServerSession->BuildAdvancingFrameInputs()`. Drop the `ServerClientManager.h` and `ServerFleetManager.h` includes there if nothing else in the file needs them.
- Game sites: `ServerSession.cpp:114` and `:253` use the member directly. Next to `mpBroadcaster->ResetState()` (`:391`), add `mPendingUpdatePlayerRequests.clear();`. The engine `ResetState` drops that clear.

### Stage 3 — transfer seams and transfer logs

- `Frame/StatusChange.h`: add `inline constexpr StatusChangeType kOwnedEntityTransferType = StatusChangeType::kTransferPlayer;`. It is the transfer type whose rows carry the owned-entity identity `Frame::OwnershipLayer` binds. Every engine `kTransferPlayer` test (`ServerTransferManager.cpp:82,104,137,183,325`, `ReconcileReplayTick.cpp:138`) compares against it.
- Renames to neutral owned-entity names (game-side sites move with them):
  - `TransferData::globalPlayerId` → `globalId`: `StatusChange.h:97,141`, `Frame/Collections/Players/PlayersNavigation.cpp:241`, `SpawnTransfer.cpp:96`, `Agent/Commands/ServerSimulationHarnessRigs.cpp:666`, `Network/Client/ReconcileReplayClientState.cpp:40`, the Stage 1 codec, and the engine sites.
  - `engine::ClientTransferInfo::globalPlayerId` → `globalId`.
  - `game::SubscriptionUpdate::globalPlayerId` → `globalId`: `ServerSession.h:27`, `ServerSession.cpp:350-351`, `ServerTransferManager.cpp:247`.
  - `game::ServerSession::mClientPlayers` → `mClientOwnedEntities`: every site in `ServerSession`, `ServerClientManager.cpp`, `ServerFleetManager.cpp`, the moved `ProcessUpdatePlayerRequests`, and `ServerTransferManager.cpp:236-242`.
- Add `void game::ServerSession::OnOwnedEntityChangedCell(const engine::ClientGuid& rClientGuid, engine::GlobalId globalId, engine::GridCoord destination)`, which forwards to `mpFleetManager->OnPlayerTransferred` with the same arguments. `ServerTransferManager.cpp:252,268` call it at the same points. Remove the four Sandbox includes at `:10-13`.
- `game::SpawnTransfer` drops its `playerAlignment` parameter and reads `rFrame.postRender.playerAlignment` (`SpawnTransfer.h:10`, `SpawnTransfer.cpp:13,39-41,48`). The two callers (`ServerTransferManager.cpp:144`, `ReconcileReplayTick.cpp:133`) drop the argument. The author recommends this over a renamed `Game` accessor because the client replay already uses the frame value and CRC-matches the server, so the two sources are already shown equal (see the invariants).
- Shared log helper: the per-type count loop at `ReconcileReplayCrc.cpp:94-115` becomes `void engine::AppendStatusChangeTypeCounts(common::ScopedWorkbufferArena& rBuilder, std::span<const game::StatusChange> changes)`, declared in `NetworkSerialization.h` and defined in `NetworkSerialization.cpp` (both builds, already the engine's status-change batch home), with output identical to today's (`TransferPlayer x2, TransferBlaster`). It is used by `ReconcileReplayCrc.cpp`, by `ReconcileReplayTick.cpp`'s transfer summary (replacing the four counters and `LogTransferSummary`'s four count parameters), and by `ServerTransferManager::ApplyPreparedTransfers` (replacing the `pBlasters/pSpaceships/pMissiles` population counts). The author recommends one helper because three sites would otherwise carry the same loop. Both transfer logs keep their total count, their owned-entity count (`OwnershipLayer(...).iCount` on the server), and up to eight owned-entity global IDs taken from `kOwnedEntityTransferType` transfers, with labels renamed to `OwnedCount` and `OwnedIds`. No script parses these verbose log lines.
- Reword the transfer-manager comments and log text (`:26-30,80-81,135,139,226,264`) to "owned entity" / "non-owned transfer".

### Stage 4 — game save-extension header

- New server-only game header `Save/GameSaveState.h` (whole-file `BT_SERVER`, includes `Fleet.h`). It takes the `SaveStagedState` struct and the `WriteSaveState`, `ReadSaveState`, `AdoptSaveState`, and `ResetSaveState` declarations, unchanged, from `Network/Server/ServerFleetSerialization.h:10-21`, which then includes `Save/GameSaveState.h` so its game consumers compile unchanged. Definitions stay in `ServerFleetSerialization.cpp`.
- `Engine/Source/File/GridSave.h:5` includes `Save/GameSaveState.h`. Save bytes do not change.

### Stage 5 — server window, debug render, mutex, camera names, comments

- `Frame/ServerCellStats.h`: replace `iPlayers`, `iSpaceships`, `iBlasters`, `iMissiles` with `std::array<int64_t, std::size(kServerEntityCountNames)> entityCounts {}`, where `inline constexpr std::array<const char*, 4> kServerEntityCountNames {"Players", "Spaceships", "Blasters", "Missiles"};` and a game-internal `enum class ServerEntityCount : uint8_t { kPlayers, kSpaceships, kBlasters, kMissiles }` indexes both. `iTick`, `durationCurrentTime`, and `iExplosions` stay. `ServerCellStats.cpp` fills and sums through the enum.
- Engine `ServerDisplay.cpp`: one file-static `std::array<int64_t, std::size(game::kServerEntityCountNames)>` of totals replaces the four statics. Aggregation, the content hash (each row mixed separately, as today), the cell-label sum, and the panel rows (`"%s: %lld"`, so the drawn text stays `Players: N` and so on) loop over the rows. Reword the `:149` example comment.
- `Frame/Frame.h`: add a client-only `static void DebugRender(const FrameInterpolate& __restrict rFrameInterpolate, engine::GridCoord coordinate);` next to `Render`, defined in `Frame/Frame.cpp` as a call to `PlayersInterpolate::DebugRender`. `MainUniforms.cpp:553` calls `game::FrameInterpolate::DebugRender`, and `:10` loses the `Players.h` include.
- `Main.cpp:878`: `CreateMutex(nullptr, TRUE, game::kGameName.data())` (`kGameName` views a null-terminated literal, already passed this way at `:889`).
- `CameraBase`: `mVecLastKnownPlayerPosition/Velocity` → `mVecLastKnownTargetPosition/Velocity`, `mfLastKnownPlayerTime` → `mfLastKnownTargetTime`. The `:116,198` comments say "target".
- Reword the comments and assert text listed in `## Context` so they describe owned entities, game state, or the target without the gameplay names. Examples: the `NetworkProtocol.h:112` assert text becomes "the game owned-entity client-GUID column stride shifts", `:111` "game save owner records", and `GameBase.h:239` names "game timers advanced once per update" instead of `ServerFleetManager::TickFleetTimers`.

### Stage 6 — documentation

Rewrite only the docs this change makes false:

- `Engine/Source/Network/AGENTS.md:41`: the engine codec owns the per-type group envelope, ascending `StatusChangeType` emission with input order kept within a type, the per-item bound, and LZ4 framing; game Network owns the type set and each type's payload read/write (`game::WriteStatusChangePayload`/`ReadStatusChangePayload`). Keep the CRC-ordering sentence. `:42`: the two managers name `game::StatusChange`, whole game Frames, and the game hooks listed in `Server/AGENTS.md`, never game entity types.
- `Engine/Source/Network/Server/AGENTS.md:3,33,35-36`: owned-entity wording for transfer liveness; the hooks the managers call (`kOwnedEntityTransferType`, `Frame::OwnershipLayer`, `ServerSession::BuildAdvancingFrameInputs`, `ServerSession::OnOwnedEntityChangedCell`, `SpawnTransfer`); no gameplay type is named.
- `Engine/Source/Server/AGENTS.md:8-9`: the window consumes `game::GetServerCellStatistics` and `game::kServerEntityCountNames`.
- `Network/AGENTS.md:3,14` (game): payload read/write lives in `StatusChangeCodec`, not the engine codec.
- `Network/Server/AGENTS.md:28` (game): the once-per-advancing-update work runs in `ServerSession::BuildAdvancingFrameInputs`, which the engine `BuildFrameInputs` calls.
- `Frame/AGENTS.md:27-28` (game): `ServerCellStats` returns named population rows.

## Critical files

- `Engine/Source/Network/NetworkSerialization.h`, `.cpp`; `Projects/BrokenEngineSandbox/Source/Network/StatusChangeCodec.h`, `.cpp` (new)
- `Engine/Source/Network/Server/ServerBroadcaster.h`, `.cpp`; `Engine/Source/Network/Server/ServerTransferManager.h`, `.cpp`
- `Engine/Source/Network/Client/ReconcileReplayTick.cpp`, `ReconcileReplayCrc.cpp`
- `Engine/Source/File/GridSave.h`; `Projects/BrokenEngineSandbox/Source/Save/GameSaveState.h` (new); `Network/Server/ServerFleetSerialization.h`
- `Engine/Source/Server/ServerDisplay.cpp`; `Frame/ServerCellStats.h`, `.cpp`
- `Engine/Source/Graphics/Render/MainUniforms.cpp`; `Frame/Frame.h`, `Frame.cpp`
- `Engine/Source/Main.cpp`; `Engine/Source/Graphics/CameraBase.h`, `.cpp`
- `Frame/StatusChange.h`, `SpawnTransfer.h`, `SpawnTransfer.cpp`, `Network/Server/ServerSession.h`, `.cpp`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj(.filters)`, `BrokenEngineSandboxServer.vcxproj(.filters)` through `/update-vcxproj`

## In scope

- Stage 1: `engine::SerializeGroup`, `engine::DeserializeStatusChangeBatch`, the eight per-transfer static functions (moved), the `NetworkSerialization.h` comments; new `game::WriteStatusChangePayload`, `game::ReadStatusChangePayload`.
- Stage 2: `engine::ServerBroadcaster::BuildFrameInputs`, `::ResetState`, removal of `PendingUpdatePlayerRequest`, `ProcessUpdatePlayerRequests`, `mPendingUpdatePlayerRequests` from the engine; `game::ServerSession` gains them plus `BuildAdvancingFrameInputs`; `ServerSession.cpp:114,253,391` call sites.
- Stage 3: `game::kOwnedEntityTransferType`; the four renames with every site listed under Stage 3; `game::ServerSession::OnOwnedEntityChangedCell`; `game::SpawnTransfer` signature and its two callers; `engine::AppendStatusChangeTypeCounts` and its three call sites; `ServerTransferManager` `IsDestinationLive` comment, `CollectTransfers`, `SpawnTransfers`, `ApplyPreparedTransfers`, `TrackClientTransfers`, `ApplyReplayTransfers`; `ReconcileReplayTick.cpp` `LogTransferSummary` and the transfer loop in `ReconcileRunTickCoord`; `ReconcileReplayCrc.cpp:94-115`.
- Stage 4: `GridSave.h` include; new `Save/GameSaveState.h`; `ServerFleetSerialization.h:10-21`.
- Stage 5: `game::ServerCellStats`, `GetServerCellStatistics`, `PublishServerEntityCounts`, `kServerEntityCountNames`, `ServerEntityCount`; `ServerDisplay.cpp` statics, `ServerUpdateDisplayStatistics`, `ServerDisplayContentChanged`, the cell label at `:304-305`, the panel rows at `:596-599`; `game::FrameInterpolate::DebugRender`; `MainUniforms.cpp:10,553`; `Main.cpp:878`; the three `CameraBase` members and their uses; the comment and assert-text sites listed in `## Context` (engine and `Common/Math/Random.cpp:37`).
- Stage 6: the doc lines listed there.
- Project membership for the three new files.

## Out of scope

- Moving any fleet code, fleet state, or `ServerFleetManager`/`FleetNavigationController`/`ServerClientManager` logic into the engine, or changing it beyond the call sites listed.
- Renaming game-internal Sandbox names other than the four Stage 3 renames: `StatusChangeType` enumerators, payload struct names, `PendingUpdatePlayerRequest`, `OnPlayerTransferred`, `mPlayerAlignment`, `playerAlignment`, Players columns, fleet member fields, and game packet names all stay.
- Any wire, save, replay, or Frame layout change, and every `kiVersion`, `FrameInput::kiVersion`, and `engine::kiProtocolVersion`.
- The six human-player lines listed in `## Context`, including chat text.
- Engine `AGENTS.md` prose not listed in Stage 6 (for example `Engine/Source/Frame/Collections/HexShields/AGENTS.md:3`, `Pushers/AGENTS.md:8`, `Ui/Screens/AGENTS.md:17`).
- The allowed hooks the engine already calls neutrally, including `game::ReplayTransferCaptureCounts` and `ServerSimulationHarnessRigs.h`.
- Shared project files, tooling, `Game.cpp` shell moves, save/settings/profile ownership, collection helpers, session protocol pieces, harness command moves, and Sandbox cleanups, which the sibling Plans under `Documents/Plans/Engine/`, `ChangeWorkflow/ProjectAwareTooling.md`, and `Game/SandboxSmallCleanups.md` own.
- Adding client-side checks to the status-change decoder (`Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy`).

## Risk triggers and invariants

- Wire and serialization (Tier 3): each type's payload bytes, field order, and widths stay byte-identical. The envelope (type byte, `uint16_t` count), ascending type emission, input order within a type, the 4-byte uncompressed-size prefix, and LZ4 framing stay engine-owned and unchanged. The per-item bound assert still runs on every item. Because no byte changes, no protocol, `FrameInput`, or `Frame` version moves. If the implementation finds a byte change necessary, stop and return it as a decision instead of bumping.
- Determinism/CRC: the server per-update work keeps its exact call order and position. It runs after the frame-input reset and before the first `PrepareTickStatusChanges` drain, only on advancing updates, so the fleet-RNG draw position and the order spawn status changes enter the frame input (`ServerBroadcaster.cpp:54-62`) are unchanged.
- Determinism/CRC: `SpawnTransfer`'s alignment source changes on the server from `gpGame->mPlayerAlignment` to the destination frame's `postRender.playerAlignment`. Missile target flags are CRC'd. The two values are equal: every frame is seeded from `mPlayerAlignment` (`Game.cpp:660`, through `GameBase::CreateCellAtCoordinate`, `Engine/Source/GameBase.cpp:964`), carried forward each tick (`Frame.cpp:195`), and the client replay already spawns with the frame value and CRC-matches the server. The replay determinism check below must cover a Missile transfer.
- Transfer ownership: `OnOwnedEntityChangedCell` is called at exactly today's two points with today's arguments, after the relink and GUID assignment, before death detection (`Network/Server/AGENTS.md` `## Deterministic Tick Contracts`).
- Save: grid-save bytes unchanged; only the declaring header moves.
- Affinity: `Save/GameSaveState.h` is server-only; `StatusChangeCodec` is shared by both projects; `FrameInterpolate::DebugRender` is client-only.

## Acceptance criteria

1. From the worktree root, `rg -n "Blaster|Spaceship|Missile|Player|Fleet|Flagship|blaster|spaceship|missile|player|fleet|flagship" Engine/Source Common -g "*.h" -g "*.cpp" -g "*.inl"` prints exactly the six human-player lines listed in `## Context` and nothing else. `bUseMissiles` is covered by `Missile`.
2. `rg -n "#include \"(Frame/Collections/(Players|Spaceships|Blasters|Missiles)|Network/Server/ServerFleet|Fleet\.h)" Engine/Source` prints nothing.
3. `/compile` builds the BrokenEngineSandbox client and the BrokenEngineSandboxServer server with no new warnings, and `/update-vcxproj` validation passes for the three new files.
4. An `/agent-harness` replay determinism check on a Debug build, over a recording that includes `SpawnPlayer`, `UpdatePlayer` (weapon toggle), `UpdateFleet`, and transfers of all four types including a Missile, reports an identical CRC on every tick.
5. A live `/agent-harness` session with a connected client that crosses cells and fires missiles logs no CRC mismatch or desync. The server monitoring window panel shows the `Players`, `Spaceships`, `Blasters`, `Missiles` rows with live counts.
6. The diff changes no `kiVersion`, `FrameInput::kiVersion`, or `engine::kiProtocolVersion`, and a grid save written and then loaded through `/agent-harness` restores fleets.
7. The Stage 6 doc lines no longer state that the engine codec implements payload read/write or that the managers own game entity types.

## Coordination

- `Documents/Plans/Game/ShipTypeSplit.md` (and its descendants `Game/FlightPlaneClimbing.md`, `Game/CrossPlaneWeapons.md`) edits the engine codec switch, the `kTransferPlayer` tests, `ServerBroadcaster`'s update-request path, the transfer logs, `ServerDisplay`, and `MainUniforms`' `DebugRender` call (its decisions 14, 19, 20). Whichever lands second rebases onto the other's edits. If this Plan lands first, those edits apply to the game sides introduced here instead: `StatusChangeCodec.cpp`, `kOwnedEntityTransferType` (which then becomes the game's owned-transfer test), `ServerSession::ProcessUpdatePlayerRequests`, `kServerEntityCountNames`, and `FrameInterpolate::DebugRender`. Engine files then need no ship names.
- `Documents/Plans/Game/FrameEditCellPins.md` extends `SaveStagedState` and the save-state functions. Whichever lands second rebases onto the other's edits. Landed after this Plan, `SaveStagedState` lives in `Save/GameSaveState.h`.

## Notes

- `/update-claude-docs` needs no edits beyond Stage 6.
- Of the sibling Plans written alongside this one, `Engine/SharedSessionProtocol.md` depends on this Plan, and `Engine/AgentHarnessIntoEngine.md` depends on it through `SharedSessionProtocol`. The others (`Engine/SharedEngineProjectFiles.md`, `ChangeWorkflow/ProjectAwareTooling.md`, `Engine/GameShellIntoGameBase.md`, `Engine/EngineOwnedSaveSettingsAndSwitches.md`, `Engine/SharedCollectionHelpers.md`, `Game/SandboxSmallCleanups.md`) overlap with it only textually and can land in either order.
