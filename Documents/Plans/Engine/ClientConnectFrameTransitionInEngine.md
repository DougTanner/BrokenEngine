<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T23:35:13.843Z","dependsOn":[]} -->
# Fold the Client Menu and Connect Frame Transition Into the Engine

## Context

The engine owns the change from a running game back to the main menu
(`GameBase::ChangeFrameToMainMenu`), but the opposite change, from the main
menu into a game when a connection is accepted, lives in the game's client
session, together with small connection handlers that touch only engine
state. A new game project (Coretide) would copy all of it, so the user asked
for this to be planned before that project starts.

Nothing was built or run for this Plan: every statement below is from a code
read at commit `b5c75660`. Line numbers are from that commit; locate by
symbol, because the game files named here were being edited (comments only)
by another worker in the authoring session.

The two directions today:

- To the main menu, in the engine: `GameBase::ChangeFrameToMainMenu`
  (`Engine/Source/GameBase.cpp` 1125-1144) disconnects the client runtime,
  asserts the menu flag is clear, calls `game::gpGame->StartMenuMusic()` and
  `game::Camera::StorePose(false)`, sets `GameFlags::kMainMenu`, calls
  `CreateNewFrame()` then `ResetSession()`, and calls
  `game::Camera::ApplyPose(true)`.
- Into a game, in the game: `ClientSession::OnConnectionAccepted`
  (`Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp`
  212-225), when `GameFlags::kMainMenu` is set, calls `StartGameMusic()` and
  `Camera::StorePose(true)`, clears the flag, calls `ResetSession()` then
  `CreateNewFrame()`, calls `Camera::ApplyPose(false)`, and sets `meUiState`
  to `engine::UiState::kNone`. The engine runtime calls it on every poll of
  an accepted connection (`Engine/Source/Network/Client/ClientSessionRuntime.cpp`
  468); the flag test makes it act once.

The handlers around it in the same game file use only engine members:

- `ClientSession::ConnectToServer` (194-198) clears
  `gpGame->mpcModalMessage` and calls the runtime's `Connect` with
  `engine::kiDefaultPort`.
- `ClientSession::OnConnectionRejected` (200-204) and
  `ClientSession::OnConnectionFailed` (206-210) write `mpcModalMessage` and
  set `meUiState` to `engine::UiState::kModal`.
- `ClientSession::OnConnectionLost` (227-235) calls
  `ChangeFrameToMainMenu()`, writes "Connection lost" when no message is
  set, and sets the modal state.

The engine calls these by name from `ClientSessionRuntime.cpp` 352, 452,
457, 468 and 476. `mpcModalMessage` is a `GameBase` member
(`Engine/Source/GameBase.h` 231) that engine code already writes
(`Engine/Source/Network/Client/ClientDesyncCore.cpp` 37, 77, 103, 129).

`Game::ApplyStandardMenuAction`
(`Projects/BrokenEngineSandbox/Source/Game.cpp` 195-209) forwards each
`engine::StandardMenuAction` (`GameBase.h` 191-196) to the client runtime's
`StartDiscovery`, to `ConnectToDiscoveredServer(engine::kiDefaultPort)`, or
to `ChangeFrameToMainMenu()`. The engine menu screens call it by name
(`Engine/Source/Ui/Screens/MainMenuScreen.cpp` 58, 66, 119;
`Engine/Source/Ui/Screens/PauseMenuScreen.cpp` 71).

Two ranges the originating audit listed are not transition code and are left
out: `Game.cpp` 122-148 is the map (`MapOpen`, `ToggleMap`, `MapOpacity`),
and `ClientSession.cpp` 299-310 is the smoke-trail copy inside
`ClientSession::HydrateReceivedFullState`.

Originating gap: a read-only audit of code a second game would have to copy.
It is not an unmet acceptance criterion of any change.

## Design

Recommended shape (the author's recommendation; rationale given):

- A client-only `GameBase` function beside `ChangeFrameToMainMenu` holds the
  menu-to-game change now in `OnConnectionAccepted`, statement for statement,
  and the runtime calls it where it calls `OnConnectionAccepted` today.
  Rationale: the two directions then sit together and read as each other's
  reverse. The order of `ResetSession()` and `CreateNewFrame()` differs
  between the two directions today; each keeps its own order.
- The rejected, failed and lost handling and `ConnectToServer` move to
  `engine::ClientSessionBase` (`Engine/Source/Network/Client/ClientSessionBase.h`),
  which already declares `ClientSessionRuntime` its friend (line 30) and
  already holds the session mechanics a game inherits. The game's
  `ClientSession` loses those members.
- `ApplyStandardMenuAction` moves to `GameBase`, since each of its three
  cases today forwards to engine code. A saved Feature would add a fourth
  that does not (`## Notes`).
- The seam stays the engine/game contract in `Engine/Source/AGENTS.md`
  `## Hub Conventions`: the engine keeps calling the game by name for what
  only a game has — `game::gpGame->StartGameMusic()`, `StartMenuMusic()`, and
  the `game::Camera` pose functions, as `ChangeFrameToMainMenu` does today.
  No virtual function, callback or function pointer is added, and no empty
  game hook is added for a reaction no game has.
- The executing session reads each moved function's callers first and keeps
  in the game any statement that turns out to name a game-only member.

Risk tier: Tier 2 (the author's classification). Trigger: runtime behavior of
the client session and menu flow, across `GameBase` and the engine client
session. Exposure: client only. No packet, CRC, save, `.pack`, `kiVersion` or
replay change is expected: the moved statements send nothing and change no
simulation state beyond what `ResetSession()` and `CreateNewFrame()` already
do. Affinity: the new functions are `BT_CLIENT` code; `GameBase.cpp` is
compiled by both executables, so they are guarded as
`ChangeFrameToMainMenu` guards its client lines. Threading: all of this runs
on the main thread in the client poll and UI paths, and stays there.

## Critical files

- `Engine/Source/GameBase.h` and `GameBase.cpp`
- `Engine/Source/Network/Client/ClientSessionBase.h` and
  `ClientSessionBase.cpp`
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp`
- `Engine/Source/Ui/Screens/MainMenuScreen.cpp` and `PauseMenuScreen.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.h` and
  `ClientSession.cpp`
- `Projects/BrokenEngineSandbox/Source/Game.h` and `Game.cpp`
- `Engine/Source/Ui/AGENTS.md` and `Engine/Source/Ui/Screens/AGENTS.md`

## In scope

- `ClientSession::OnConnectionAccepted`, `OnConnectionRejected`,
  `OnConnectionFailed`, `OnConnectionLost` and `ConnectToServer`: their
  bodies, declarations, and the friend access they need.
- `Game::ApplyStandardMenuAction` and its declaration.
- The new `GameBase` function and the engine call sites listed in
  `## Context`, plus the game caller of `ConnectToServer` in
  `Game::ProcessGameMenuInput`.
- Documentation sync of `Engine/Source/Network/Client/AGENTS.md`,
  `Projects/BrokenEngineSandbox/Source/Network/Client/AGENTS.md`, and
  `Engine/Source/AGENTS.md` where they describe who owns these functions;
  and of `Engine/Source/Ui/AGENTS.md` `## Shared Types` and
  `Engine/Source/Ui/Screens/AGENTS.md` `## Standard Menu Contract`, which
  record `ApplyStandardMenuAction` as a `game::Game` member that routes
  game-owned side effects.

## Out of scope

- `GameBase::ChangeFrameToMainMenu`, `ResetSession` and `CreateNewFrame`
  bodies, and `Game::ResetGameState`.
- `ClientSession::OnServerLoad`, `ProcessReceivedGamePackets`,
  `HydrateReceivedFullState`, and every `Send*Request` function.
- `Game::GetStandardMenuModel`, the menu screens' layout, and the modal
  screen.
- The music playlists, `StartMenuMusic` and `StartGameMusic`, and the
  `game::Camera` pose functions.
- The client runtime's connection state machine, discovery, and the desync
  core.
- The map functions in `Game.cpp`.

## Acceptance criteria

1. `/compile` builds the BrokenEngineSandbox client and server.
2. A search finds no definition of the five moved `ClientSession` functions
   or of `ApplyStandardMenuAction` in the game.
3. Through `/agent-harness`, with a local server: connecting from the main
   menu enters the game (no menu or modal shown, game camera pose); the
   pause menu's return-to-main-menu action returns to the menu; connecting
   again enters the game again.
4. Through `/agent-harness`: connecting with no server running shows the
   "Connection failed" modal on the main menu; stopping the server during a
   game returns the client to the main menu with the "Connection lost" modal.
5. The replay determinism recipe in
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md` shows no
   CRC mismatch.

The supplied evidence records no harness setup for criteria 3 to 5; the
executing session sets them up from the harness documentation.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- The smoke-trail copy in `ClientSession::HydrateReceivedFullState` uses only
  engine types and may be a separate extraction; it is not part of this Plan.
- The Feature `Documents/Features/Network/ConnectClientToHostedServer.md`
  `## Design` (lines 65-78) adds a `kConnectToRemoteServer` case to
  `Game::ApplyStandardMenuAction` that passes a game-owned address constant
  in `Game.cpp` to `ClientSession::ConnectToServer`. This Plan moves both
  functions, and that case is not a forward to engine code alone. A Feature
  is not a scheduler input, so no `dependsOn` edge records this: whichever
  of the two is done second places that case and its constant against the
  other's result, and returns the placement to the user if it is not plain.
