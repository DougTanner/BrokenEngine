<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-17T18:15:12.657Z","dependsOn":[]} -->
# Run the server's teardown message drain only while the game object exists

## Context

This Plan is a behavior-preserving cleanup of the server's teardown order. It
changes no UI. The server runs on a machine nobody watches, and the server code
is going to be split from the server UI later, so this Plan does not change what
the monitoring window shows or how it responds. It only removes the workarounds
that exist because a message can be dispatched after the game object is gone.
Doing it before or together with that split leaves the split with one teardown
pump instead of two.

Verified root cause (`Engine/Source/Main.cpp`, server build):

- `MainThread` declares the `destroyWindow` scope guard (`:240-265`) before
  `pGame` (`:351`). Guards are destroyed in reverse order, so the guard's
  server-only `ProcessMessages()` drain (`:254`) runs after `pGame` is
  destroyed. On a normal exit it also runs after the post-loop drain
  (`PostQuitMessage(0); ProcessMessages();`, `:534-535`), which runs while the
  game object exists.
- `game::Game` derives from `GameBase` and then `Singleton<Game>`
  (`Projects/BrokenEngineSandbox/Source/Game.h:55`). `~Singleton` clears
  `game::gpGame` (`Common/Singleton.h:26-29`), so the drain at `:254`
  dispatches into `WindowProcedure` with no game object.
- Three workarounds exist only because of that late drain:
  1. `ValidateRect` (`:246-253`), so no `WM_PAINT` reaches `PaintServerDisplay`,
     which reads game state.
  2. `gbQuit = true` in the guard (`:243-244`) and the `WM_LBUTTONDOWN` test
     `if (gbQuit) return 0;` (`:736-741`), so a click cannot invalidate the
     window during the late drain.
  3. The `game::gpGame != nullptr` test in the `WM_ENDSESSION` case
     (`:755-759`), whose comment cites only the teardown drain. That drain is
     the only point in the server build that retrieves messages while
     `game::gpGame` is null. The server's other `ProcessMessages()` calls
     (`:374`, `:396`, `:535`) all run while the game object exists, and the
     server shows no `MessageBox` or other dialog, so nothing else can
     dispatch a sent message before `pGame` is constructed or after it is
     destroyed.

The client already works this way. Commit `8f9589d5` ("Keep client window
messages inside manager lifetime") removed the client pump from the same guard
and kept only the post-loop pump, which runs while every owner exists (`:256-257`).

Originating step: Change Workflow Review and resolve correctness for
`Documents/Plans/Engine/HardenServerForUnattendedHosting.md`,
`/over-engineering-review` finder design suggestion D1. This is a design
suggestion, not an acceptance failure, and the user chose to record it as a
follow-up. It depends on that Plan because that Plan adds the `WM_ENDSESSION`
case and its `gpGame` null test.

## Design

The author recommends deleting the server block of the `destroyWindow` guard
(`:242-255`) entirely rather than adding a second pump. Rationale:

- The post-loop pump at `:534-535` is already the final drain that runs while
  the game object exists, so moving the guard's drain into the post-loop block
  would just repeat it. This mirrors the client path from `8f9589d5`.
- Declaring the whole `destroyWindow` guard after `pGame` would not work. On the
  client, `Graphics` (`:295`) is declared before `pGame` and needs the window to
  outlive it. Splitting out a separate server-only drain guard after `pGame`
  would also run on exception unwinding, with the game object still alive. A
  `WM_PAINT` or `WM_ENDSESSION` would then reach a game whose main loop threw,
  where today the null test skips the save. That would change behavior.
- After the deletion, nothing on the server pumps messages between the post-loop
  pump and `DestroyWindow`. `hideWindow`'s `ShowWindow(SW_HIDE)` and
  `DestroyWindow` send only synchronous messages, which `WindowProcedure`
  handles without game state (`WM_DESTROY` at `:848-854`). The server registers
  no timers, and the only message it posts to itself, the `WM_QUIT` from
  `PostQuitMessage` at `:534`, is drained by the post-loop pump at `:535`.
  `DestroyWindow` throws away a paint region or input left in the queue, and the
  server never handled those after the game object was gone anyway, so the
  result looks the same.

Then:

- `destroyWindow`: remove the `#if defined(BT_SERVER)` block (`:242-255`). Rewrite
  the following comment (`:256-257`) so it states the rule for both builds: no
  pump here, because the game object is already destroyed and the post-loop pump
  is the last one.
- `WM_LBUTTONDOWN` (`:734-746`): remove the `gbQuit` test and its comment. A
  click during the post-loop pump now repaints with the game object still
  alive. That is the one visible difference, a repaint of an unwatched window.
- `WM_ENDSESSION` (`:748-762`): remove the `game::gpGame != nullptr` test and
  its comment, and call `game::gpGame->mServerSaveLoad.FinalAutosave()`
  directly. The server window exists before `pGame` is constructed (`:234`
  against `:351`), but the server retrieves no messages in that time: its first
  `ProcessMessages()` call (`:374`) runs after `pGame` exists, and the server
  shows no dialog whose modal loop could dispatch a sent message. With the
  teardown drain gone, every server message retrieval runs while the game
  object exists. Before deleting the test, the fix session repeats that check:
  every `ProcessMessages()` call, `MessageBox`, and other dialog in the files
  built into `BrokenEngineSandboxServer`.
- `Engine/Source/Server/AGENTS.md` `## Ownership and Cadence`, teardown bullet
  (`:34`): remove the second
  sentence (the `Main.cpp` validation before its own teardown pump). Keep the
  first sentence, which describes the post-loop pump.

Change Workflow tier: Tier 2. The trigger is scoped behavior: one subsystem's
runtime behavior, the order in which the server shuts down and dispatches window
messages. It touches no determinism/CRC, wire, serialization, `.pack`, replay,
threading, or trust-boundary surface. Exposure: the change is server-only
(`BT_SERVER`), changes no client code, and changes no project membership.

## Critical files

- `Engine/Source/Main.cpp`
- `Engine/Source/Server/AGENTS.md`

## In scope

- `Engine/Source/Main.cpp` `MainThread`: the server block of the
  `destroyWindow` scope guard (`gbQuit = true`, the `ValidateRect` block, the
  `ProcessMessages()` call) and the comment that follows it.
- `Engine/Source/Main.cpp` `WindowProcedure`, server cases: the
  `WM_LBUTTONDOWN` `gbQuit` test and its comment; the `WM_ENDSESSION`
  `game::gpGame != nullptr` test and its comment.
- `Engine/Source/Server/AGENTS.md` `## Ownership and Cadence`: the teardown
  bullet (`:34`).

## Out of scope

- The post-loop block (`:520-536`): its `FinalAutosave()`, `PostQuitMessage`,
  and `ProcessMessages()` stay as they are.
- The rest of the `WM_ENDSESSION` case (the `wParam` test, its log line, and
  `gbQuit = true`), and `FinalAutosave` / `sbFinalAutosaveDone` behavior.
- Moving `CreateWindow`, `ShowWindow`, or the `destroyWindow` / `hideWindow`
  declarations, and any server-code / server-UI split.
- The accepted `hideWindow` exception-unwind gap (`hideWindow` is declared after
  `pGame`, `:355`).
- Client teardown, `PaintServerDisplay`, `HandleServerClick`, and every other
  `WindowProcedure` case.

## Acceptance criteria

- `MainThread` has no `ProcessMessages()` call that runs after `pGame` is
  destroyed, and the `destroyWindow` guard contains no `ValidateRect` and no
  `BT_SERVER` block.
- In the server build, every `ProcessMessages()` call runs while
  `game::gpGame` is set, no `MessageBox` or other dialog remains, and the
  `WM_ENDSESSION` case has no `game::gpGame != nullptr` test.
- The server builds, and an agent `quit` exits normally with exit code `0` and
  one final autosave in the log (`/agent-harness`).
- Sending `WM_ENDSESSION` with `wParam` 1 to a running server still returns only
  after `ServerAutosave.save` exists under
  `<app-data-directory>\Broken Engine Sandbox Server\`, because the session may
  end as soon as the message returns. The process then exits on its own with
  exit code `0`.
- `/update-claude-docs` reports no stale teardown wording.

## Notes

- Schedule this before or with the server-code / server-UI split. No Plan for
  that split exists yet, so this Plan has no `## Coordination` entry for it.
- The `gpGame` null test was added only for the teardown drain on exception
  unwinding, and the server shows no dialog that could pump messages before
  `pGame` exists, so removing the drain leaves the test with nothing to guard.
