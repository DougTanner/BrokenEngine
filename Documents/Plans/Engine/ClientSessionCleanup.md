<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:44:49.397Z","dependsOn":[]} -->
# Client Session Cleanup and Engine-Owned Connect Frame Transition

## Context

Five findings on the client session, from code reads only; nothing was built
or run. Each part records the commit its line numbers come from; locate by
symbol where they moved. All parts share `ClientSession.h`/`.cpp`,
`ClientSessionBase.cpp`, `ClientReconciler.h`/`.cpp`, the client
`AGENTS.md`, the client build, and one local-server harness run.

Path shorthand: `Client/` means
`Projects/BrokenEngineSandbox/Source/Network/Client/`; `EngineClient/` means
`Engine/Source/Network/Client/`.

### Part A: dead work in the client session base and desync core

From the in-function refactoring phase of a deep analysis of
`Engine/Source/Network/Client` at commit `d87ad840`. Each item is work whose
result nothing can observe.

A reset loop over cells that are destroyed on the next line:
`ClientSessionBase::ResetSessionForServerLoad`
(`EngineClient/ClientSessionBase.cpp:95-98`) calls `rCell.ResetClientState()`
on every cell, and line 99 is `game::gpGame->mCells.clear()` with nothing
between. `Cell::ResetClientState` (`Engine/Source/GameBase.h:141-159`)
assigns integer members and calls `serverUpdates.clear()` and
`optionalPendingFullState.reset()`; it touches no global, renderer, texture
slot, or counter. `Cell` declares no destructor, so destruction does not read
the reset members. The sibling loops at lines 61-64 and 79-82 are live: those
functions keep the cells.

An IsStalled return the only caller has already taken:
`ClientSessionBase::Reconcile` returns at lines 26-29 when
`mpDesynchronizationCore->IsStalled()`. Its only caller is
`GameBase::ClientUpdate` (`Engine/Source/GameBase.cpp:357`), which returns on
the same condition at lines 282-285. Nothing between can set the state:

- Stalled means `mDesyncDebugState.iTick >= 0`
  (`ClientDesyncCore.h:27-30`). Its only non-reset writer is
  `ClientDesyncCore::OnDesyncDetected` (`ClientDesyncCore.cpp:26`).
- `OnDesyncDetected` is called from `ClientSessionBase.cpp:41`, inside
  `Reconcile` after the test, and from
  `Engine/Source/Agent/Commands/ClientDesyncProbe.cpp:138`.
- The probe runs only from `gpAgentCommandServer->Drain()`, which on the
  client is `Engine/Source/Main.cpp:444`, before `ProcessInput` and
  `ClientUpdate`.
- `GameBase.cpp:286-356` calls `TickRealtime`, `UpdateDesiredCoordinates`,
  `SynchronizeSubscriptions`, `PrepareActiveSet`, and profiling only.

`Documents/Architecture/GameReconciliation.md` draws the gate in
`ClientUpdate` and says it "gates physics, subscriptions, and
reconciliation"; that stays true through the `GameBase.cpp:282` test.

A bool result nobody reads: `ClientDesyncCore::PollDesyncTimeout` (declared
`ClientDesyncCore.h:34`, defined `ClientDesyncCore.cpp:82-107`) returns
`bool`. Its only caller, `ClientSessionBase::PollDesynchronizationState`
(`ClientSessionBase.cpp:73`), discards the result.

Two guards with the same body: in `ClientDesyncCore::PollDebugFrameResponse`,
the guard at `ClientDesyncCore.cpp:50`
(`pDebugFrame->iTick != mDesyncDebugState.iTick`) and the guard at line 56
(`pDebugFrame->coordinate != mDesyncDebugState.coord`) have byte-identical
bodies: the same LOG line and `return`.

Unmet condition: the session base and desync core hold no check, loop, or
result that cannot affect anything
(`.agents/references/cpp-conventions.md`: no check for a state no caller can
produce). It is pre-existing: the analysis session changed no source.

### Part B: unused declarations and includes in the client game files

From a deep analysis of `Projects/BrokenEngineSandbox/Source/Network`
(architecture phase), verified by code read against commit `4cf9d607`. Two
client headers declare names nothing uses, and two `.cpp` files include a
header they do not use:

- `Client/ClientReconciler.h:10` `struct FrameInput;` — not used in the
  header; `game::FrameInput` is already complete through the PCH (`Pch.h`
  includes `GameBase.h`, which stores it by value).
- `Client/ClientReconciler.h:12` `using engine::SnapshotIndex;` — every
  unqualified `SnapshotIndex` use in the repository is inside
  `namespace engine`.
- `Client/ClientSession.h:13` `class Client;` and `:15`
  `struct ReceivedDebugFrame;` (in its `namespace engine` block) — neither is
  named in the header; both are defined in `EngineClient/Client.h`, which
  `Engine.h` aggregates into the PCH.
- `Client/ClientSession.h:24` `enum class GamePacketType : uint8_t;` — not
  named in the header; every user includes `Network/GamePacketType.h` itself.
- `Client/ClientSession.h:33` `SubscriptionChangeReason::kPollTick` — only its
  `ToString` case (`ClientSession.cpp:31`) names it. The engine passes the
  string literal instead (`Engine/Source/GameBase.cpp:299`,
  `Engine/Source/Agent/AgentCommandsClientGeneric.cpp:1107`).
- `#include "Profile/ProfileRows.h"` at `Client/ClientReconciler.cpp:8` and
  `Client/ClientSession.cpp:12` — the header declares only `GameCpuCounters`,
  `GameCpuTimers`, and their two name tables, and no file under
  `Projects/BrokenEngineSandbox/Source/Network/` names any of them.
  `ReconcileReplayClientState.cpp` includes the same `ClientReconciler.h` and
  `Game.h` without it.

Every declaration deleted sits in a game-local header, and no file under
`Engine/Source` depends on any of them.

### Part C: the dead corrupt-packet catch arm

From the same analysis, verified against commit `4cf9d607`.
`ClientSession::ProcessReceivedGamePackets` (`Client/ClientSession.cpp:99`)
wraps the fleet-sync and map fleet marker handlers in one `try` with two catch
arms. The first arm (`:118-126`) catches `std::ios_base::failure`, logs
"dropped corrupt packet" at `kError`, and asserts; its comment (`:120-123`)
says a shared reader throwing that type means server bytes the client cannot
decode. Nothing in the `try` throws that type:

- `MessageReader` (`Engine/Source/Network/NetworkMessages.h`) only clears its
  validity flag. The one throw is `ThrowCorruptStream`, reached from
  `NetworkMessages::Read`, which neither `FleetSyncMessage::ReadPayload` nor
  `MapFleetMarkersMessage::ReadPayload`
  (`Projects/BrokenEngineSandbox/Source/Network/GameMessages.h`) calls.
- `FleetSelection::SynchronizeFleets` has no `throw`.
- An out-of-range count reaches `std::vector::resize` and lands in the second
  arm (`std::exception`, log a warning and continue).

History shows the throw was removed on purpose. Commit `bca1752f` ("Remove
client-side checks on server-to-client data", completing a Plan) deleted the
validity throws from `FleetSyncMessage::ReadPayload` and wrote the comments
above both `ReadPayload` functions. The arm stayed live then because the same
`try` still decoded player events through a shared reader; commit `fe7ca062`
moved that decode into the engine `Client::Receive`, which is when the arm
lost its last thrower.

`Client/AGENTS.md` `## Reconciliation Invariants` (third bullet) still says
the fleet-sync and map fleet marker handlers "share one boundary that splits
corrupt data from an ordinary local failure". The code cannot do that split.

Two authorities disagree here, and this Plan records which was trusted: the
leaf sentence above (restated on 2026-11-13 by commit `ae681f03`) against
`Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy` ("client code adds
no new value or structural checks on server data") and the approved change
`bca1752f`. The policy and the approved change are trusted; the leaf sentence
is the side to correct.

### Part D: the ClientReconciler confirmed-client-state member

From the same analysis (architecture phase, plus one in-function finding),
verified against commit `4cf9d607`. `ClientReconciler::mConfirmedClientState`
(`Client/ClientReconciler.h:35`) looks like state kept between calls but
keeps nothing:

- `ClientReconciler::Run` overwrites all three fields at its top
  (`ClientReconciler.cpp:73-75`) before the only reads (`:102`, and through it
  `ReconcileUpdateClientState`). The early returns at `:93` and `:98` leave
  values nothing reads, and `Reset` (`:145`) clears a value the next `Run`
  overwrites.
- `ConfirmedClientState::clientGridCoordinate` (`ClientReconciler.h:16`) has
  two writes (`ClientReconciler.cpp:73`, `:145`) and no read anywhere in the
  repository; the transfer scan uses `rWork.coord`.
- The struct is copied four times for one armor value: `Run` copies the member
  into `newConfirmedClientState` (`:102`) and back (`:137`), and
  `ReconcileUpdateClientState` copies its in-out parameter into a local
  (`ReconcileReplayClientState.cpp:16`) and back (`:79`).
- `Client/AGENTS.md` `## Ownership` (third bullet) says the reconciler owns
  "confirmed client-state tracking"; no state survives a call. The code is
  trusted and the document is the side to correct.

In the same function, `ReconcileUpdateClientState` tests
`clientState.clientGlobalIdentifier.iValue != 0` inside the innermost of three
loops (`ReconcileReplayClientState.cpp:38-41`), under an
`if (bAnyFullReplay)` wrapper (`:18`). The function never writes the
identifier (it writes only `fPreviousClientArmor`), so the test is
loop-invariant, and `IsOwnedEntityTransfer`
(`Projects/BrokenEngineSandbox/Source/Frame/StatusChange.cpp`) is a pure read,
so skipping the loops when the identifier is 0 changes nothing.
`Documents/C++StyleGuide.txt` rule 62 asks for early returns in guard
sequences.

`mfPreviousClientArmor` is client-only visual state read by
`Game::UpdateCameraShake`; `Run` and its callee run on the main thread.

### Part E: the menu-to-game frame transition lives in the game

From a read-only audit of code a second game would have to copy, at commit
`b5c75660`. The engine owns the change from a running game back to the main
menu (`GameBase::ChangeFrameToMainMenu`), but the opposite change, from the
main menu into a game when a connection is accepted, lives in the game's
client session, together with small connection handlers that touch only
engine state. A new game project (Coretide) would copy all of it, so the user
asked for this to be planned before that project starts.

The two directions today:

- To the main menu, in the engine: `GameBase::ChangeFrameToMainMenu`
  (`Engine/Source/GameBase.cpp` 1125-1144) disconnects the client runtime,
  asserts the menu flag is clear, calls `game::gpGame->StartMenuMusic()` and
  `game::Camera::StorePose(false)`, sets `GameFlags::kMainMenu`, calls
  `CreateNewFrame()` then `ResetSession()`, and calls
  `game::Camera::ApplyPose(true)`.
- Into a game, in the game: `ClientSession::OnConnectionAccepted`
  (`Client/ClientSession.cpp` 212-225), when `GameFlags::kMainMenu` is set,
  calls `StartGameMusic()` and `Camera::StorePose(true)`, clears the flag,
  calls `ResetSession()` then `CreateNewFrame()`, calls
  `Camera::ApplyPose(false)`, and sets `meUiState` to
  `engine::UiState::kNone`. The engine runtime calls it on every poll of an
  accepted connection (`EngineClient/ClientSessionRuntime.cpp` 468); the flag
  test makes it act once.

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
(`EngineClient/ClientDesyncCore.cpp` 37, 77, 103, 129).

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

None of these findings is an unmet acceptance criterion of any change.

## Design

The steps run top to bottom; each is the author's recommendation unless
marked otherwise.

### Part A: engine session base and desync core

1. Delete the loop at `ClientSessionBase.cpp:95-98`. The comment above it and
   `mCells.clear()` stay.
2. Delete the `IsStalled` test and return at `ClientSessionBase.cpp:26-29`.
3. Make `PollDesyncTimeout` return `void`: the two `return false;` become
   `return;` and the final `return true;` is deleted. `Projects/Coretide` is a
   private game project that is absent from a session worktree.
   `PollDesyncTimeout` is a public member of an engine class the game reaches
   through `mpDesynchronizationCore`, so the executing session searches
   `Projects/Coretide` in the primary checkout for `PollDesyncTimeout` and
   reports what it finds before it changes the return type.
4. Optional rider, which the acceptance criteria do not need: merge the two
   `PollDebugFrameResponse` guards into one `if` joined with `||`, tick test
   first as today. `Documents/C++StyleGuide.txt` rule 62 asks for separate
   `if`s when unrelated conditions guard the same single-statement exit, and
   lets "the same check over parallel values" stay in one `if`. Here the exit
   is two statements and the two tests are the halves of one
   response-identity match, so the author reads the rule as permitting the
   merge. That reading is contestable. Make the merge and drop it without
   further work if the style review contests it.

### Part B: unused declarations and includes

Delete exactly these lines:

5. `ClientReconciler.h`: `struct FrameInput;` and
   `using engine::SnapshotIndex;`.
6. `ClientSession.h`: `class Client;` and `struct ReceivedDebugFrame;` (keep
   `class ClientSessionRuntime;`, which the `friend` declaration needs), the
   `GamePacketType` forward declaration, and the `kPollTick` enumerator.
7. `ClientSession.cpp`: the `kPollTick` case in `ToString`, and the
   `Profile/ProfileRows.h` include.
8. `ClientReconciler.cpp`: the `Profile/ProfileRows.h` include.

`SubscriptionChangeReason` values are passed only to `ToString`, so removing
an enumerator changes no stored or sent value.

### Part C: corrupt-packet catch arm

Recommended by the authoring analysis, not chosen by the user: delete the arm
and correct the sentence. The rejected alternative is to make both
`ReadPayload` functions throw on an invalid reader so the arm becomes live; it
would re-add a check an approved change removed and that the corrupt-input
policy forbids. The one reason to keep the arm is as a parallel to the engine
`Client::Receive` catch, which stays live because engine decoders still go
through `NetworkMessages::Read`; keeping an arm for a thrower that does not
exist is speculative, so the recommendation is to delete it.

9. `ClientSession::ProcessReceivedGamePackets`: delete the
   `catch (const std::ios_base::failure&)` arm with its comment, log, and
   `ASSERT`. The `std::exception` arm and `eParsingType` stay. No reachable
   behavior changes: the deleted arm cannot be entered.
10. `Client/AGENTS.md` `## Reconciliation Invariants`, third bullet: reword
    the clause so it says the two handlers share one boundary that logs an
    ordinary local failure as a warning, and that their decoders raise no
    corrupt-data signal, under the hub's corrupt-input policy; through
    `/update-claude-docs`.

### Part D: confirmed-client-state member

Recommended shape (the author's choice among equivalent ones): keep
`ConfirmedClientState` as the callee's in-out parameter with its two read
fields, and make it a local of `Run`. Replacing the struct with two parameters
is the other shape; it changes the callee's signature for no gain.

11. `ClientReconciler.h`: delete `ConfirmedClientState::clientGridCoordinate`
    and the member `mConfirmedClientState`.
12. `ClientReconciler::Run`: where the three member writes are today (before
    the dispatch, so both values are read at the same point as now), declare
    one local `ConfirmedClientState` initialized from
    `gpGame->ClientOwnedEntityIdentifier()` and `gpGame->mfPreviousClientArmor`.
    Pass that local to `ReconcileUpdateClientState`, delete
    `newConfirmedClientState` and the copy back into the member, and assign
    `gpGame->mfPreviousClientArmor` from the local where it is assigned today.
13. `ClientReconciler::Reset`: delete the `mConfirmedClientState = {};` line.
14. `ReconcileUpdateClientState`: delete the local copy and the write-back and
    use `rInOutState` directly; replace the `if (bAnyFullReplay)` wrapper with
    an early return, add an early return when the identifier is 0 before the
    loops, and delete the per-request identifier test.
15. `Client/AGENTS.md` `## Ownership`, third bullet: remove the "confirmed
    client-state tracking" claim so the bullet names only the visual-error
    aggregation and the owned-entity transfer migration, through
    `/update-claude-docs`.

Behavior is unchanged: the same two values are read at the same point, the
same armor value is written back on the same paths, and the verbose log line
in the transfer scan is untouched.

### Part E: connect frame transition into the engine

Recommended shape (the author's recommendation; rationale given):

16. A client-only `GameBase` function beside `ChangeFrameToMainMenu` holds the
    menu-to-game change now in `OnConnectionAccepted`, statement for
    statement, and the runtime calls it where it calls `OnConnectionAccepted`
    today. Rationale: the two directions then sit together and read as each
    other's reverse. The order of `ResetSession()` and `CreateNewFrame()`
    differs between the two directions today; each keeps its own order.
17. The rejected, failed and lost handling and `ConnectToServer` move to
    `engine::ClientSessionBase` (`EngineClient/ClientSessionBase.h`), which
    already declares `ClientSessionRuntime` its friend (line 30) and already
    holds the session mechanics a game inherits. The game's `ClientSession`
    loses those members.
18. `ApplyStandardMenuAction` moves to `GameBase`, since each of its three
    cases today forwards to engine code. A saved Feature would add a fourth
    that does not (`## Notes`).
19. The seam stays the engine/game contract in `Engine/Source/AGENTS.md`
    `## Hub Conventions`: the engine keeps calling the game by name for what
    only a game has — `game::gpGame->StartGameMusic()`, `StartMenuMusic()`,
    and the `game::Camera` pose functions, as `ChangeFrameToMainMenu` does
    today. No virtual function, callback or function pointer is added, and no
    empty game hook is added for a reaction no game has.
20. The executing session reads each moved function's callers first and keeps
    in the game any statement that turns out to name a game-only member.
21. Documentation sync per `## In scope`.

## Critical files

- `Engine/Source/GameBase.h` and `GameBase.cpp`
- `Engine/Source/Network/Client/ClientSessionBase.h` and
  `ClientSessionBase.cpp`
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp`
- `Engine/Source/Network/Client/ClientDesyncCore.h` and `ClientDesyncCore.cpp`
- `Engine/Source/Ui/Screens/MainMenuScreen.cpp` and `PauseMenuScreen.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.h` and
  `ClientSession.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.h` and
  `ClientReconciler.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ReconcileReplayClientState.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Game.h` and `Game.cpp`
- `Engine/Source/Ui/AGENTS.md` and `Engine/Source/Ui/Screens/AGENTS.md`

## In scope

Engine session base and desync core:

- `ClientSessionBase::ResetSessionForServerLoad`: the `ResetClientState`
  loop.
- `ClientSessionBase::Reconcile`: the `IsStalled` test and return.
- `ClientDesyncCore::PollDesyncTimeout`: its return type and `return`
  statements.
- `ClientDesyncCore::PollDebugFrameResponse`: the two non-matching-response
  guards, for the optional rider only.

Client game Network files:

- The five declaration lines, the `kPollTick` enumerator and its `ToString`
  case, and the two include lines named in Design steps 5-8.
- The `std::ios_base::failure` catch arm of
  `ClientSession::ProcessReceivedGamePackets`.
- `ConfirmedClientState::clientGridCoordinate` and
  `ClientReconciler::mConfirmedClientState`: deleted.
- `ClientReconciler::Run`: the three member writes and their comment, the
  `newConfirmedClientState` copy, the call argument, and the two closing
  assignments.
- `ClientReconciler::Reset`: the member reset line.
- `ReconcileUpdateClientState`: the entry copy, the `bAnyFullReplay` wrapper,
  the identifier test, and the exit write-back.

Connect frame transition:

- `ClientSession::OnConnectionAccepted`, `OnConnectionRejected`,
  `OnConnectionFailed`, `OnConnectionLost` and `ConnectToServer`: their
  bodies, declarations, and the friend access they need.
- `Game::ApplyStandardMenuAction` and its declaration.
- The new `GameBase` function and the engine call sites listed in Part E of
  `## Context`, plus the game caller of `ConnectToServer` in
  `Game::ProcessGameMenuInput`.

Documentation:

- `Client/AGENTS.md` `## Reconciliation Invariants`, the fleet-sync and map
  fleet marker clause of the third bullet; and `## Ownership`, third bullet.
- Sync of `Engine/Source/Network/Client/AGENTS.md`, `Client/AGENTS.md`, and
  `Engine/Source/AGENTS.md` where they describe who owns the moved connection
  functions; and of `Engine/Source/Ui/AGENTS.md` `## Shared Types` and
  `Engine/Source/Ui/Screens/AGENTS.md` `## Standard Menu Contract`, which
  record `ApplyStandardMenuAction` as a `game::Game` member that routes
  game-owned side effects.

## Out of scope

- `GameBase::ChangeFrameToMainMenu`, `ResetSession` and `CreateNewFrame`
  bodies, and `Game::ResetGameState`.
- The stall test in `GameBase::ClientUpdate`, and
  `ClientDesyncCore::IsStalled` itself, which that test still calls.
- The `ResetClientState` loops in
  `ResetCoordinateStatesForResynchronization` and `OnRuntimeDisconnected`,
  and `Cell::ResetClientState`.
- The `gpClient` test in `Reconcile` and in `ClientDesyncCore::Reset`, and
  the visibility of `RecoverFromDesync`.
- The client runtime's connection state machine and discovery, and every part
  of the desync core other than the Part A items.
- `ClientSession::OnServerLoad`, `HydrateReceivedFullState` (including its
  smoke-trail copy), and every `Send*Request` function; in
  `ProcessReceivedGamePackets`, everything but the `std::ios_base::failure`
  arm.
- Adding any throw or validity check to `FleetSyncMessage::ReadPayload`,
  `MapFleetMarkersMessage::ReadPayload`, or `MessageReader`.
- The `std::exception` arm, and the engine `Client::Receive` catches.
- `Engine/Source/Network/AGENTS.md` and
  `Documents/Architecture/GameReconciliation.md`, which stay true.
- The `BoundedCount` arguments in `GameMessages.h` that feed the unread
  validity flag (noted by the analysis as an engine-mechanism question, not
  accepted as debt).
- Replacing `SubscriptionChangeReason` and `ToString` with string literals:
  it reaches `Ui/Screens/HudScreen.cpp` and trades compile-checked names for
  literals, a style preference the analysis did not accept as debt.
- The engine's `"kPollTick"` literals, and the two reason strings that differ
  from their enumerator names.
- Every other include and declaration in `ClientReconciler.h`/`.cpp` and
  `ClientSession.h`/`.cpp` beyond the Part B lines and the moved Part E
  members, and the whole-file guard, which already sits directly after the
  first include.
- The visual-error block in `ClientReconciler::Run` (its duplicated length
  expression was judged cosmetic) and the three position helpers.
- The order in which `ReconcileUpdateClientState` scans works when two
  transfers match (judged below the benefit bar).
- `Game::mfPreviousClientArmor`, `Game::UpdateCameraShake`, and replay
  metadata; `Documents/Plans/Game/ReplayMetaClientStateAndFighterAdmission.md`
  owns the replay-metadata side and excludes the reconciler.
- `Game::GetStandardMenuModel`, the menu screens' layout, and the modal
  screen.
- The music playlists, `StartMenuMusic` and `StartGameMusic`, and the
  `game::Camera` pose functions.
- The map functions in `Game.cpp`.

## Risk tier

Tier 2 — scoped behavior; every part is Tier 2. Triggers:

- Runtime behavior of the client session and menu flow, across `GameBase`
  and the engine client session (Part E).
- A public member's return type changes, and a stall test is removed on the
  strength of its caller's test (Part A).
- An enumerator is removed from an enum declared in a header that engine
  translation units include, and a struct field and class member are removed
  from a header, which Tier 1's "no public signature exposure" excludes
  (Parts B and D).
- The exception boundary of the client's handlers for server data is edited,
  which Tier 1's "no invariant exposure" excludes; it stays Tier 2 because it
  does not alter what the client trusts or any reachable outcome (Part C).

Exposure: client only, main thread throughout. No packet, CRC, save,
`.pack`, `kiVersion`, serialization, trust, or replay change is expected: the
Part E statements send nothing and change no simulation state beyond what
`ResetSession()` and `CreateNewFrame()` already do; Part A's stalled state is
never set in the shipped configuration (`kbDesynchronizationDebugFrames` is
`false` in `Engine/Source/EngineSwitches.h`); Part D's state is client-only
visual state. Affinity: the new Part E functions are `BT_CLIENT` code;
`GameBase.cpp` is compiled by both executables, so each is guarded whole under
`BT_CLIENT`, as `ChangeFrameToMainMenu` is.

## Invariants

- `mCells.clear()` stays ahead of `gpCamera->DiscardTrackingCaches()`.
- `GameBase::ClientUpdate` keeps its stall test ahead of `Reconcile`.
- The log text of the non-matching-response guard is unchanged.
- Each frame-transition direction keeps its own order of `ResetSession()` and
  `CreateNewFrame()`.

## Acceptance criteria

1. `/compile` builds the BrokenEngineSandbox client and server. The client
   build also compiles the engine translation units that include the Part B
   headers (`Engine/Source/GameBase.cpp`,
   `Network/Client/ClientSessionBase.cpp`, `ClientSessionRuntime.cpp`,
   `ClientDesyncCore.cpp`, `Profile/ProfileNetworkScreen.cpp`,
   `Agent/AgentCommandsClientGeneric.cpp`,
   `Agent/Commands/ClientDesyncProbe.cpp`), and it is the proof that the
   include and the declarations are unused.
2. The repository static checks pass.
3. Code read: `ResetSessionForServerLoad` holds no `ResetClientState` call,
   `Reconcile` holds no `IsStalled` call, and `PollDesyncTimeout` returns
   `void`.
4. Code read: none of the Part B deleted names or include lines remains in
   the four files.
5. Code read: `ProcessReceivedGamePackets` has one catch arm, for
   `std::exception`, and no callee in its `try` throws
   `std::ios_base::failure`.
6. Code read: `ClientReconciler` has no `mConfirmedClientState`,
   `ConfirmedClientState` has no `clientGridCoordinate`, and
   `ReconcileUpdateClientState` holds no copy of its parameter and no
   identifier test inside a loop.
7. Code read: `Client/AGENTS.md` `## Reconciliation Invariants` no longer says
   the game packet handlers split corrupt data from a local failure, and
   `## Ownership` no longer says the reconciler tracks confirmed client state.
8. A search finds no definition of the five moved `ClientSession` functions
   or of `ApplyStandardMenuAction` in the game.
9. One `/agent-harness` scenario observes:
   - With no server running, connecting from the main menu shows the
     "Connection failed" modal on the main menu.
   - With a local server, connecting from the main menu enters the game (no
     menu or modal shown, game camera pose), and the client publishes a cell.
   - A server load through the debug load request leaves the client connected
     and publishing cells again.
   - The pause menu's return-to-main-menu action returns to the menu;
     connecting again enters the game again.
   - Stopping the server during a game returns the client to the main menu
     with the "Connection lost" modal.
10. The replay determinism recipe in
    `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md` shows no
    CRC mismatch.

The supplied evidence records no harness setup for criteria 9 and 10; the
executing session sets them up from the harness documentation. No harness run
backs Part D; its code read is decisive for that change.

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
- The Part C choice between deleting the arm and keeping it as a deliberate
  parallel was made by the analysis from the evidence above. The user has not
  chosen it; it binds only once the user approves it
  (`.agents/references/authority-order.md`).
