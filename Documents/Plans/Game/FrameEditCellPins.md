<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-06T17:48:39.498Z","dependsOn":["Documents/Plans/Game/FrameEditRows.md"]} -->
# `pin_cell` and `unpin_cell`: initialize and remove cells through a saved pinned-coordinate set

Line numbers cite baseline `2b4cb8d38aa57ca7ccd45983cc5121f64e85cf15`. Where a statement here and the code disagree, the code wins; report the contradiction instead of matching one side to the other.

## Context

User direction (binding): the Debug-only server harness initializes a cell at an unloaded coordinate (through `CreateCellAtCoordinate`) and removes a loaded cell, using a Debug-only pinned-coordinate set that keeps edited or initialized playerless cells in the active set; pinning also lifts `edit_frame`'s refusal of a playerless non-origin cell while a client is handshaken. Pinned coordinates are stored in the save, which is a save-format change: the owning version is bumped so older saves are rejected. Save and reload of a scenario use the existing `save` and `load` commands.

Why a pin is needed: the server's active set is recomputed every update and every tick (`Engine/Source/Network/Server/ServerSessionRuntime.cpp:330-345`, called from `Engine/Source/GameBase.cpp:352`, `:961`, and `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:56`) as active subscriptions plus the game hook's player cells (`ServerSession.cpp:290-302`) plus the origin, and `SyncActiveCells` creates a cell for every entering coordinate and erases every other (`ServerSessionRuntime.cpp:306-328`). A bare `CreateCellAtCoordinate` is therefore undone within the update, and a playerless non-origin cell a client once subscribed to is erased when the load-time resynchronization frees every subscription (`ServerSession.cpp:386-395`), which is why `edit_frame` refuses it (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp:394-399`). The same mechanism drops a reloaded save's playerless cells, although `WriteGridSave` writes every loaded frame (`Engine/Source/File/GridSave.cpp:29-45`), so without saved pins a scenario built in such a cell would not reload.

Decisions this Plan makes (author's, with rationale):

1. The pinned set is `game::ServerSession::mPinnedCoordinates`, consulted by `AddGameRequiredCoordinates`. The session already owns the hook, the fleet state the save's game half carries, and the relink policy; a pin is "keep this cell active", nothing else.
2. Two commands, `pin_cell` and `unpin_cell`, each `{"coord":[x,y]}`, in `AgentCommandsServer.cpp` beside the other simulation controls. Pinning creates the cell by running `ComputeActiveSet()` the way `ServerLoad` does (`Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:78`); unpinning removes the cell the same way when nothing else keeps the cell. Origin and player cells need no special case: pinning them changes nothing and unpinning them leaves them loaded, which the response reports.
3. Pins are saved in the game half of the grid save after the fleet data, read into `SaveStagedState`, adopted with it, and cleared by `ResetSaveState` (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:209-230`), so `reset`, a failed load, and a fresh game start unpinned, and `load` restores the pins before `ServerLoad` recomputes the active set. The engine reader keeps carrying the game half uninspected (`Engine/Source/File/AGENTS.md` `## Grid Saves`).
4. The owning version is `game::Frame::kiVersion`: the grid save header is `WriteVersionHeader<game::Frame>` (`GridSave.cpp:21`, validated at `:77` through `Engine/Source/File/FileManager.h:274-288`). Its base constant moves from 133 to 134 (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:38`). The same constant gates replay streams (`Engine/Source/File/DifferenceStream.h:79`, `:241-249`) and the client handshake (`Engine/Source/Network/Server/AGENTS.md` `## Session Invariants`), so older replays and mismatched builds are rejected too, as the repository's no-compatibility rule requires.
5. Both commands require `kbDebugInput` and share `edit_frame`'s replay refusal (recording, playback, pending transition): a pin changes the recorded cell set outside the recorded inputs, and one refusal rule is simpler than analysing what a recording would capture.
6. Non-Debug builds keep the member and the save fields, always empty, so one save format exists.
7. Pinned cells are not live transfer destinations: `ServerTransferManager::IsDestinationLive` sees players and subscriptions only (`Engine/Source/Network/Server/ServerTransferManager.cpp:31-50`), so a non-player transfer out of a pinned cell into a neighbour that holds no player or subscription is dropped with a Network log. Documented; not changed.

## Design

### Pinned set and active-set hook

`ServerSession.h`: `std::vector<engine::GridCoord> mPinnedCoordinates;` beside `mPendingSubscriptionUpdates`, with a comment that it is the Debug harness's pinned cells, saved with the fleet data. `ServerSession::AddGameRequiredCoordinates` (`ServerSession.cpp:290-302`) also appends every pinned coordinate not already in `gpGame->mActiveCoordinates`.

### Commands

In `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp`, two handlers and two dispatch rows after `reset` (`:339-342`). Both: the `if constexpr (!kbDebugInput)` refusal (`<command> requires kbDebugInput build`); the replay refusal with `edit_frame`'s condition and wording (`ServerFrameEdit.cpp:368-372`); parameters must be an object whose only key is `coord`, parsed by `CoordinateFromParameter`.

- `pin_cell`: `bool bCreated = !gpGame->mCells.contains(coordinate);` append the coordinate to `mPinnedCoordinates` unless present; `gpServerSession->mpRuntime->ComputeActiveSet();` respond `{"coord":[x,y],"created":bCreated}`. After the call the cell has `pCurrent` (`GameBase.cpp:923-948`), so `query_frame`, `read_frame`, and `edit_frame` accept it at once; `pNext` arrives through the next update's `EnsureNextFrames`.
- `unpin_cell`: the coordinate must be pinned, else `coord is not pinned`; erase it; `ComputeActiveSet();` respond `{"coord":[x,y],"loaded":gpGame->mCells.contains(coordinate)}` — `true` when a player, an active subscription, or the origin still keeps the cell.

`ComputeActiveSet` already retires departing frames through `gpReplay->RetireCoordinate` (`ServerSessionRuntime.cpp:325`); with recording refused, that is a no-op handoff.

### `edit_frame` refusal

`ServerFrameEdit.cpp:396`: the condition gains `&& !std::ranges::contains(gpServerSession->mPinnedCoordinates, coordinate)`, and the message becomes `edit_frame with a handshaken client requires a cell with a player, a pinned cell, or the origin cell`. `ServerFrameEdit.cpp` includes `Network/Server/ServerSession.h`.

### Save format

- `SaveStagedState` (`ServerFleetSerialization.h:11-16`) gains `std::vector<engine::GridCoord> pinnedCoordinates;`.
- `WriteSaveState` (`ServerFleetSerialization.cpp:209-212`): after `WriteFleetData`, write `int64_t` count then each coordinate through `GridCoord::Write` (`Engine/Source/Frame/GridCoord.h:29`), in the vector's order (pin order is the only order that exists, and the save is already deterministic per server state).
- `ReadSaveState` (`:214-217`): after `ReadFleetData`, read the count, `common::ValidateDeserializedCount(iCount, 2 * sizeof(int32_t), rFileStream, "ReadSaveState pins")`, resize, read each through `GridCoord::Read`. The count bound is the trust-boundary check the save reader owes (`Engine/Source/File/AGENTS.md` `## Grid Saves`); a duplicate or unloaded pinned coordinate is harmless, because the hook deduplicates and `SyncActiveCells` creates a missing cell.
- `AdoptSaveState` (`:219-225`): `gpServerSession->mPinnedCoordinates = std::move(rStagedState.pinnedCoordinates);`. `ResetSaveState` (`:227-230`): also `gpServerSession->mPinnedCoordinates.clear();`.
- `Frame.cpp:38`: `133` becomes `134`, with no comment change.

Load order holds without further change: `ReadGridSave` stages the game half before the frames (`GridSave.cpp:104`), `AdoptGridSave` adopts it (`:188`), `ServerLoad` then resynchronizes and calls `ComputeActiveSet` (`GameSaveLoad.cpp:77-78`), which keeps every pinned cell's saved frame. A fresh process that runs `load` gets the pins from the file.

### Documentation

- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`: entries for `pin_cell` and `unpin_cell` after `reset` (schema, responses, that a pin keeps the cell in the active set across updates, loads, and process restarts through the save, the origin and player-cell behavior, the `kbDebugInput` and replay refusals, and the transfer caveat from decision 7); in the `edit_frame` entry, the resynchronization paragraph's "keeps only cells with a player and the origin" and the refusal sentence add pinned cells, and the recipe gains "pin a cell first to build a scenario in an empty cell"; in the `save`/`load` entries, that the save carries the pinned cells and that a save from an older `Frame::kiVersion` loads as `resetToFresh:true`.
- `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` `## Contracts`: one bullet — the Debug pinned-coordinate set lives on the server session, joins the active set through the game hook, is saved with the fleet data and cleared by reset, and admits a playerless cell to `edit_frame`'s resynchronizing path.
- `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md` `## Deterministic Tick Contracts`, first bullet: the hook contributes every coordinate holding a Player and every Debug-pinned coordinate.
- `Projects/BrokenEngineSandbox/Source/Save/AGENTS.md` `## Save and Load`: the game half of the grid save is the fleet data plus the pinned coordinates; reset clears both.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.h`, `.cpp`: `mPinnedCoordinates`, `AddGameRequiredCoordinates`.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.h`, `.cpp`: `SaveStagedState`, `WriteSaveState`, `ReadSaveState`, `AdoptSaveState`, `ResetSaveState`.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`: `Frame::kiVersion` base.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp`: the two commands and dispatch rows.
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp`: the refusal.
- Documentation: `commands-server.md`, game `Agent/AGENTS.md`, game `Network/Server/AGENTS.md`, `Save/AGENTS.md`.

## In scope

- `ServerSession.h`: the `mPinnedCoordinates` member. `ServerSession.cpp` `AddGameRequiredCoordinates` (`:290-302`).
- `ServerFleetSerialization.h` `SaveStagedState`; `ServerFleetSerialization.cpp` `WriteSaveState`, `ReadSaveState`, `AdoptSaveState`, `ResetSaveState` (`:209-230`).
- `Frame.cpp:38`: the base constant.
- `AgentCommandsServer.cpp`: `CommandPinCell`, `CommandUnpinCell`, two rows in `ExecuteAgentCommandServer` (`:296-366`), and the include of `Network/Server/ServerSession.h` if not already present.
- `ServerFrameEdit.cpp:394-399`: the refusal condition and message; its include.
- The documentation edits under `### Documentation`.

## Out of scope

- `ComputeActiveSet`, `SyncActiveCells`, `CreateCellAtCoordinate`, `EnsureNextFrames`, `ResetClientsForLoad`, `OnStateReplaced`, and `ServerLoad`/`ServerReset` ordering.
- `IsDestinationLive` and every transfer rule; pinned cells stay non-live destinations.
- The engine grid header, `StagedGridSave`, `ReadGridSave`, `AdoptGridSave`, `WriteGridSave`; fleet serialization bytes other than the appended pin list; replay files.
- A `status` field or query listing pins, a bulk pin command, pin limits, origin or player-cell special cases, and clearing pins on client disconnect or session replacement beyond what session destruction already does.
- Recording a pin into a replay; pinning during recording or playback.
- `read_frame`, `edit_frame` beyond the one refusal, `inject_payload`, and client code.

## Risk triggers and invariants

Tier 3 (`.agents/references/risk-tiers.md`): save-format change with a version bump, and the active-set membership rule changes across game Network Server, game Save, game Agent, and game Frame (the version constant).

- Format: every grid save written by this build carries the pin list and the new `Frame::kiVersion`; every older save, replay, and client build is rejected by the existing version gates; no reader path accepts both layouts.
- Active set: a pinned coordinate is in `mActiveCoordinates` after every `ComputeActiveSet` while pinned, so its cell is never erased by `SyncActiveCells`; unpinning returns the cell to the pre-existing rule.
- Save and reload: a pinned cell's frame is written by `WriteGridSave` and survives `ServerLoad`'s `ComputeActiveSet`; after `reset` or a failed load no coordinate is pinned.
- `edit_frame`: a pinned playerless cell edited with a handshaken client survives the resynchronization's active-set recompute, which is the fact the lifted refusal relied on.
- Determinism, CRC, wire protocol (`engine::kiProtocolVersion`), threading, and trust: unchanged; the save count bound is the only new reader check.

## Acceptance criteria

Builds: server Debug and Profile, client Debug compile (`Frame::kiVersion` is shared).

Harness checks (`/agent-harness`, Debug server, after `reset`; `[2,3]` is a cell no player or client touches):

| # | Check | Expected observation |
|---|---|---|
| 1 | `pin_cell {"coord":[2,3]}`, then unpause for 8 ticks and pause, then `status` and `query_frame {"coord":[2,3]}` | response `created:true`; `status.activeCoords` contains `[2,3]` after the ticks; `query_frame` returns zero counts; `pin_cell` again returns `created:false` |
| 2 | `edit_frame` on `[2,3]`: one `spaceships` spawn edit, then `read_frame` `frame` on `[2,3]` | `ok:true`; `frame.values.iTick` equals `status.tick` and `uiFrameIdentifier` differs from the origin's |
| 3 | `save {"file":"Pinned.save"}`, `reset`, `status`; then `load {"file":"Pinned.save","pauseAfterLoad":true}`, `status`, `query_frame [2,3]` | after `reset`, `activeCoords` lacks `[2,3]` and `query_frame [2,3]` fails with `coord has no loaded frame`; after `load`, `resetToFresh:false`, `activeCoords` contains `[2,3]`, and the spaceship count is 1 |
| 4 | Quit the server, relaunch it, `load {"file":"Pinned.save","pauseAfterLoad":true}`, unpause 8 ticks, `status` | `activeCoords` contains `[2,3]` and `query_frame [2,3]` shows the spaceship (pins come from the file, not process memory) |
| 5 | `unpin_cell {"coord":[2,3]}`, unpause 8 ticks, `status`, `query_frame [2,3]`; then `unpin_cell [2,3]` again | first response `loaded:false`; `activeCoords` lacks `[2,3]`; `query_frame` fails with `coord has no loaded frame`; the second `unpin_cell` returns `ok:false` with `coord is not pinned` |
| 6 | `pin_cell [0,0]` then `unpin_cell [0,0]`; `inject_payload` `TransferPlayer` into `[1,0]` and one tick, then `pin_cell [1,0]` and `unpin_cell [1,0]` | origin: `created:false` then `loaded:true`; player cell: `created:false` then `loaded:true`, and `[1,0]` stays in `activeCoords` |
| 7 | One connected client subscribed to `[0,0]`, paused: `pin_cell [2,3]`, `edit_frame` on `[2,3]` `frame` `fSpawnTimer: 0.25`, then unpause 64 ticks. Then give the client an assigned player and move the client's grid cell to `[0,1]` (`Projects/BrokenEngineSandbox/Documents/AgentHarness/cross-cell.md` `### Cross-cell subscription verification` steps 2 and 4), pause once `status.activeCoords` contains `[0,1]` and `query_frame [0,1]` shows zero players, send the same edit on `[0,1]`, and restore with that recipe's step 10 | `ok:true` with `resynchronizedClients:1` (the refusal is lifted), `[2,3]` stays in `activeCoords` across the resynchronization, no `CONFIRMED DESYNC` line; the `[0,1]` edit (loaded only by the client's subscription, unpinned, playerless) is refused with the new message |
| 8 | While recording (`replay_record {"start":true}`, poll `recording:true`), during playback, and with a pending `replay_record`: `pin_cell [5,5]` and `unpin_cell [5,5]` | `ok:false` each |
| 9 | Profile server: `pin_cell` | `ok:false` with `pin_cell requires kbDebugInput build` |
| 10 | Unknown key or malformed `coord`: `pin_cell {"coord":[1.0,2]}`, `pin_cell {"coord":[1,2],"x":1}` | `ok:false` each, no pin added (`unpin_cell [1,2]` then fails with `coord is not pinned`) |

By code reading: `WriteVersionHeader<game::Frame>` and `ReadAndValidateVersionHeader<game::Frame>` compare `Frame::kiVersion`, so a save written at 133-based versions fails the header check and `load` returns `resetToFresh:true`; `ReadSaveState` bounds the count before resizing; `ResetSaveState` clears pins on both the `reset` and the failed-load path (`GridSave.cpp:57-62`).

## Execution card

- Goal: the pinned set and hook, the two commands, the lifted refusal, the saved pin list with the version bump, and the documentation edits under `## In scope`.
- Tier 3; roles as the Change Workflow assigns: `implementer` runs `/implement-plan` in two slices (session, save, and version first; commands, refusal, and documentation second), then `/update-affected-code`; `builder` runs `/compile` (server Debug and Profile, client Debug); `reviewer` runs `/plan-audit` and `/plan-simplicity-review` before implementation, `/external-grill-plan` after `/plan-audit`, and `/repo-code-review`, `/comment-review`, `/coherence-review`, and `/adversarial-review` after; `/update-claude-docs` and `/progressive-disclosure-review` follow; `/finalize-changes` lands with user confirmation.
