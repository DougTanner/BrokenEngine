<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:44:49.397Z","dependsOn":[]} -->
# Remove dead work from the client session base and desync core

## Context

Found by the in-function refactoring phase of a deep analysis of
`Engine/Source/Network/Client` at commit `d87ad840`. Line numbers are from
that commit; locate by symbol if they moved. Paths without a directory are
under `Engine/Source/Network/Client/`. Each item is work whose result nothing
can observe.

### A reset loop over cells that are destroyed on the next line

`ClientSessionBase::ResetSessionForServerLoad`
(`ClientSessionBase.cpp:95-98`) calls `rCell.ResetClientState()` on every
cell, and line 99 is `game::gpGame->mCells.clear()` with nothing between.
`Cell::ResetClientState` (`Engine/Source/GameBase.h:141-159`) assigns integer
members and calls `serverUpdates.clear()` and
`optionalPendingFullState.reset()`; it touches no global, renderer, texture
slot, or counter. `Cell` declares no destructor, so destruction does not read
the reset members. The sibling loops at lines 61-64 and 79-82 are live: those
functions keep the cells.

### An IsStalled return the only caller has already taken

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

### A bool result nobody reads

`ClientDesyncCore::PollDesyncTimeout` (declared `ClientDesyncCore.h:34`,
defined `ClientDesyncCore.cpp:82-107`) returns `bool`. Its only caller,
`ClientSessionBase::PollDesynchronizationState` (`ClientSessionBase.cpp:73`),
discards the result.

### Two guards with the same body

In `ClientDesyncCore::PollDebugFrameResponse`, the guard at
`ClientDesyncCore.cpp:50` (`pDebugFrame->iTick != mDesyncDebugState.iTick`)
and the guard at line 56
(`pDebugFrame->coordinate != mDesyncDebugState.coord`) have byte-identical
bodies: the same LOG line and `return`.

Unmet condition: the session base and desync core hold no check, loop, or
result that cannot affect anything
(`.agents/references/cpp-conventions.md`: no check for a state no caller can
produce).

Originating gap: that analysis phase's review of these functions. It is not
an unmet acceptance criterion of any change. It is pre-existing: the analysis
session changed no source.

## Design

Recommended (the author's recommendation):

- Delete the loop at `ClientSessionBase.cpp:95-98`. The comment above it and
  `mCells.clear()` stay.
- Delete the `IsStalled` test and return at `ClientSessionBase.cpp:26-29`.
- Make `PollDesyncTimeout` return `void`: the two `return false;` become
  `return;` and the final `return true;` is deleted.
- Optional rider, which the acceptance criteria do not need: merge the two
  guards into one `if` joined with `||`, tick test first as today.
  `Documents/C++StyleGuide.txt` rule 62 asks for separate `if`s when
  unrelated conditions guard the same single-statement exit, and lets "the
  same check over parallel values" stay in one `if`. Here the exit is two
  statements and the two tests are the halves of one response-identity match,
  so the author reads the rule as permitting the merge. That reading is
  contestable. The author recommends making the merge and dropping it without
  further work if the style review contests it.

`Projects/Coretide` is a private game project that is absent from a session
worktree. `PollDesyncTimeout` is a public member of an engine class the game
reaches through `mpDesynchronizationCore`, so the executing session searches
`Projects/Coretide` in the primary checkout for `PollDesyncTimeout` and
reports what it finds before it changes the return type.

Risk tier: Tier 2 (the author's classification). Trigger: a public member's
return type changes, and a stall test is removed on the strength of its
caller's test. No packet, CRC, save, or replay change:
`kbDesynchronizationDebugFrames` is `false`
(`Engine/Source/EngineSwitches.h`), so the stalled state is never set in the
shipped configuration. All of this runs on the main thread.

## Critical files

- `Engine/Source/Network/Client/ClientSessionBase.cpp`
- `Engine/Source/Network/Client/ClientDesyncCore.h` and
  `ClientDesyncCore.cpp`

## In scope

- `ClientSessionBase::ResetSessionForServerLoad`: the `ResetClientState`
  loop.
- `ClientSessionBase::Reconcile`: the `IsStalled` test and return.
- `ClientDesyncCore::PollDesyncTimeout`: its return type and `return`
  statements.
- `ClientDesyncCore::PollDebugFrameResponse`: the two non-matching-response
  guards, for the optional rider only.

## Out of scope

- The stall test in `GameBase::ClientUpdate`, and
  `ClientDesyncCore::IsStalled` itself, which that test still calls.
- The `ResetClientState` loops in
  `ResetCoordinateStatesForResynchronization` and `OnRuntimeDisconnected`.
- `Cell::ResetClientState`.
- The `gpClient` test in `Reconcile` and in `ClientDesyncCore::Reset`.
- The visibility of `RecoverFromDesync`.

## Invariants

- `mCells.clear()` stays ahead of `gpCamera->DiscardTrackingCaches()`.
- `GameBase::ClientUpdate` keeps its stall test ahead of `Reconcile`.
- The log text of the non-matching-response guard is unchanged.

## Acceptance criteria

1. `ResetSessionForServerLoad` holds no `ResetClientState` call, `Reconcile`
   holds no `IsStalled` call, and `PollDesyncTimeout` returns `void`.
2. `/compile` builds the BrokenEngineSandbox client and server.
3. Through `/agent-harness`, with a local server: after the client connects
   and publishes a cell, a server load through the debug load request leaves
   the client connected and publishing cells again. The supplied evidence
   records no harness setup; the executing session sets it up from the
   harness documentation.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
