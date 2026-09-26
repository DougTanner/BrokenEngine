<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-26T13:35:39.243Z","dependsOn":[]} -->
# Reach game-instantiated objects through their globals, never through stored or passed Base references

## Context

User direction (authoritative, verbatim): "File a followup plan to remove those
\"GameBase& mrGameBase;\" everywhere in the codebase this pattern appears, the
codebase pattern is that we use Game directly through the gpGame pointer, we
never refer to Base objects but assume they are instantiated by the Game code.
We obviously also need to strenthen a code-review and/or code-style
instructions about this."

The established pattern: `game::Game` derives from `engine::GameBase` and
publishes itself as `game::gpGame` (`Projects/BrokenEngineSandbox/Source/Game.h:209`,
set in `Game::Game` at `Game.cpp:38-40`, cleared at the end of `Game::~Game`);
`game::ProfileManager` derives from `engine::ProfileManagerBase` and publishes
`game::gpProfileManager` (`Projects/BrokenEngineSandbox/Source/Profile/ProfileManager.h:109`,
re-exported by `using game::gpProfileManager;`). Engine code already reads both
globals directly in dozens of places (`Engine/Source/GameBase.cpp`,
`Engine/Source/File/Replay.cpp` `game::gpGame->Reset()`,
`Engine/Source/Network/Client/*.cpp`, `Engine/Source/Graphics/Render/MainUniforms.cpp`,
`Engine/Source/Server/ServerDisplay.cpp` `*gpProfileManager`), and
`Engine/Source/AGENTS.md` `## Hub Conventions` allows engine code to consume game
globals. `.agents/references/cpp-conventions.md` already says "Base classes:
Include/use game versions, not Base versions — `game::gpGame` not `GameBase`
directly", but that bullet does not name stored back-references or pass-through
parameters, and `/repo-code-review` has no check for them, so the pattern was
added and extended unchallenged (the most recent extension added
`Replay::IsPlaybackActiveOrPending` and `Replay::IsRecordingActiveOrPending`,
both reading `mrGameBase`).

### Survey (baseline `7971970a`, Engine, Projects, Common, Tools, DataPacker; ThirdParty excluded)

Classes with a `Base` suffix: `GameBase`, `ProfileManagerBase` (game-instantiated
singletons with a `gp*` global), `TweaksScreenBase`, `FrameInterpolateBase`,
`FramePostRenderBase`. Every site that stores or passes a `GameBase` or
`ProfileManagerBase` reference or pointer, all in scope:

Stored back-references

- `Engine/Source/File/Replay.h:31,55` — `Replay(GameBase& rGameBase)` and
  member `GameBase& mrGameBase;`; set by `std::make_unique<Replay>(*this)` in
  `GameBase::GameBase` (`Engine/Source/GameBase.cpp:26`); read throughout
  `Engine/Source/File/Replay.cpp` (`Replay::Replay`, `PublishReplayingState`,
  `IsPlaybackActiveOrPending`, `IsRecordingActiveOrPending`, and the bodies of
  the coordinate-activation, `SaveLoadReplay`, `SyncReplayTick` paths — every
  `mrGameBase.` token).
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.h:28,41` —
  `GameSaveLoad(engine::GameBase& rGameBase)` and member
  `engine::GameBase& mrGameBase;`; set by `mGameSaveLoad(*this)` in the
  `BT_SERVER` initializer of `Game::Game` (`Game.cpp:34-36`); read throughout
  `GameSaveLoad.cpp` (every `mrGameBase` token).

Pass-through parameters (every caller passes `*game::gpGame`,
`*gpProfileManager`, `*this` from the singleton itself, or one of the stored
references above)

- `Engine/Source/File/GridSave.h:22,23,25` / `GridSave.cpp:12,50,165` —
  `WriteGridSave(GameBase&, ...)`, `ReadGridSave(GameBase&, ...)` (the
  four-argument overload), `AdoptGridSave(GameBase&, ...)`. Callers:
  `Replay.cpp` (`AdoptGridSave(mrGameBase, ...)`, `WriteGridSave(mrGameBase, ...)`)
  and `GameSaveLoad.cpp` (four calls with `mrGameBase`).
- `Engine/Source/Network/Client/ReconcileReplay.h:166` / `.cpp:383` —
  `ReconcileDispatcher::Run(GameBase& rGameBase, const ReconcileInputs&)`.
  Caller: `Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp:55`
  `mDispatcher.Run(*gpGame, inputs)`.
- `Engine/Source/Ui/Screens/{MainMenu,Modal,PauseMenu,GraphicsMenu,SoundMenu,GameSettings}Screen.h/.cpp`
  — each `Render(GameBase& rGame)`. Caller:
  `Engine/Source/Graphics/Managers/ImGuiManager.cpp:510-520`
  (`Render(*game::gpGame)` six times).
- `Engine/Source/Agent/AgentCommandsClientGeneric.h:13` / `.cpp:1157` —
  `ExecuteClientAgentCommand(..., const GameBase& rGame, ProfileManagerBase& rProfileManager)`,
  plus its file-local helpers `BuildDescribeUi(const GameBase&)` (`:671`),
  `BeginScriptAndDefer(..., const GameBase* pGame, ...)` (`:843`, the deferred
  lambda captures `pGame`), `CommandDescribeUi` (`:896`), `CommandClick`
  (`:903`), `CommandHover` (`:917`), `CommandQueryProfile(..., ProfileManagerBase&)`
  (`:1076`). Caller:
  `Projects/BrokenEngineSandbox/Source/Agent/AgentCommands.cpp:37`
  (`*gpGame, *gpProfileManager`).
- `Engine/Source/Profile/ProfileManagerBase.h:539-545` /
  `Engine/Source/Profile/ProfileScreens.cpp:14,64,130,275,312,359` —
  `FormatCpuTimersText`, `FormatCpuCountersText`, `FormatGpuTimerRows`,
  `FormatFpsHeader`, `FormatCpuScreen`, `FormatGpuScreen`, each taking
  `ProfileManagerBase& rProfileManager`. Callers:
  `Engine/Source/Profile/ProfileManagerBase.cpp` render path (`*this`, three
  calls), `ProfileScreens.cpp` internal calls, and
  `Engine/Source/Server/ServerDisplay.cpp:447,459` (`*gpProfileManager`).

Not in scope, with reason

- `FrameInterpolateBase` / `FramePostRenderBase` parameters and locals
  (`Engine/Source/Frame/**`, `Engine/Source/Graphics/EngineCamera.*`,
  `Projects/BrokenEngineSandbox/Source/Graphics/Camera.*`,
  `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:832-853`): per-coord,
  per-snapshot data objects with many instances and no global; passing them is
  correct.
- `TweaksScreenBase::Section::pfnRender(TweaksScreenBase&)`
  (`Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.h:33` and the section
  tables in `TweaksScreenBase.cpp` and the game `TweaksScreen.cpp`): the screen
  dispatches to its own sections with `*this`; no `gp*` global exists for it.
- `GameBase` / `ProfileManagerBase` member functions using their own `this`,
  and `Main.cpp`'s owning `pGame` local that instantiates `game::Game`.
- `ExecuteSharedAgentCommand` receiving the tick as `int64_t` and
  `Input::BeginPoll` receiving menu visibility as a `bool`
  (`Engine/Source/Agent/AGENTS.md`, `Engine/Source/Input/AGENTS.md:24`): plain
  values, not Base references; the shared command also runs before `Game`
  exists.
- `WriteGridSave`'s `clientGridCoord` value parameter and `ReadGridSave`'s
  `rClientGridCoord` out parameter: plain values, left unchanged.

### Documentation that contradicts the user direction

- `Engine/Source/Agent/AGENTS.md` `## Architecture`, the
  `ExecuteClientAgentCommand` bullet: "passing the live `const GameBase&` and
  `ProfileManagerBase&` those handlers report from; the engine source names no
  game global or derived game type. A deferred UI script captures the
  `GameBase` pointer by value and reads it when the script completes."
- `Engine/Source/Ui/Screens/AGENTS.md:7`: "A standard screen takes
  `Render(GameBase&)` and never names `game::Game`, `game::gpGame`, or a game
  session type."
- `Engine/Source/Graphics/Managers/AGENTS.md:99`: "Standard screens receive
  `GameBase&`".
- Code comments restating the same: `Engine/Source/Agent/AgentCommandsClientGeneric.h`
  ("rGame and rProfileManager supply the live state the handlers report; the
  game dispatcher owns the globals they come from."),
  `AgentCommandsClientGeneric.cpp` above `BeginScriptAndDefer` ("pGame is
  non-null only when ... the caller's reference parameter does not outlive this
  call."), `Engine/Source/Engine.h:101` ("because the interface names GameBase
  and its UI state types"), and `Engine/Source/Network/Client/ReconcileReplay.h:160-161`
  ("every eligible coord of a GameBase").

Authority: the user statement outranks these documents (root `AGENTS.md`
`### Diagnosis Discipline`), so this Plan rewrites them.

## Design

The author's recommendation, following the user direction:

1. `Replay`: delete the `GameBase& mrGameBase;` member and the constructor
   parameter (`Replay();`), drop the now-unused `class GameBase;` forward
   declaration in `Replay.h` (the `friend class GameBase;` declaration names the
   class itself), change `GameBase::GameBase` to `std::make_unique<Replay>()`,
   and replace every `mrGameBase.` in `Replay.cpp` with `game::gpGame->`.
   `Replay.cpp` already includes `Game.h`. `Replay` is constructed inside
   `GameBase::GameBase`, before `Game::Game` sets `gpGame`, and destroyed after
   `Game::~Game` clears it; `Replay::Replay`/`~Replay` never touch the game today,
   and must stay that way.
2. `GameSaveLoad`: delete the member, the constructor declaration and
   definition (the class becomes default-constructible), the
   `engine::GameBase` forward-declaration block in `GameSaveLoad.h`, and the
   `BT_SERVER` `: mGameSaveLoad(*this)` initializer in `Game::Game`; replace every
   `mrGameBase` use with `game::gpGame->`. `Autoload` runs in `Game::Game` after
   `gpGame = this`, so it is safe.
3. `GridSave`: drop the `GameBase&` parameter from `WriteGridSave`,
   the four-argument `ReadGridSave`, and `AdoptGridSave`; their bodies read
   `game::gpGame->`; add `#include "Game.h"` to `GridSave.cpp`; update the
   `Replay.cpp` and `GameSaveLoad.cpp` callers.
4. `ReconcileDispatcher::Run(const ReconcileInputs&)`: body reads
   `game::gpGame->`; add `#include "Game.h"` to `ReconcileReplay.cpp` when it is
   not already reachable; update `ClientReconciler.cpp:55`; reword the
   `ReconcileReplay.h` class comment to "every eligible coord of the game".
5. Standard screens: each becomes `void Render();` reading `game::gpGame->`
   where it read `rGame.`; remove the `class GameBase;` forward declarations
   from the six headers; include `Game.h` in each `.cpp`; update the six
   `ImGuiManager.cpp` calls to `Render()`.
6. Client agent commands: `ExecuteClientAgentCommand(cmd, rParams, rResult)`;
   `BuildDescribeUi()`, `CommandDescribeUi`, `CommandClick`, `CommandHover`
   drop the game parameter and read `game::gpGame`; `BeginScriptAndDefer`
   drops `pGame` (the existing `bDescribeUiAfter` already gates the UI dump)
   and its deferred lambda calls `BuildDescribeUi()` at completion, reading the
   live global; `CommandQueryProfile` drops its parameter and reads
   `gpProfileManager`. Update `AgentCommands.cpp:37`, delete the header comment
   sentence about `rGame`/`rProfileManager` and the `pGame` sentences above
   `BeginScriptAndDefer`, and reword the `Engine.h:101` comment to "declared
   after GameBase.h because the interface names its UiState and GameFlags
   types" (both types are defined in `GameBase.h`, so the include order stays).
7. Profile formatters: the six functions drop `ProfileManagerBase&` and read
   `gpProfileManager`; add `#include "Profile/ProfileManager.h"` to
   `ProfileScreens.cpp` when it is not already reachable; update the
   `ProfileManagerBase.cpp` and `ServerDisplay.cpp` callers and the five
   declarations in `ProfileManagerBase.h` (`FormatGpuTimerRows` is file-local
   to `ProfileScreens.cpp` and has no header declaration).
8. Documentation: rewrite the three AGENTS.md passages listed above to state
   the new shape — the client-generic handlers read `game::gpGame` and
   `gpProfileManager` directly and a deferred UI script reads `game::gpGame`
   when it completes; a standard screen takes `Render()`, reaches the game
   through `game::gpGame`, and never names a game session type; standard
   screens are called as `Render()`.
9. Instruction strengthening, one owning statement plus one reviewer check:
   - Owner: `.agents/references/cpp-conventions.md`, replacing the existing
     "Base classes:" bullet (it is the layer "every implementer and reviewer of
     C++ applies", and it already owns this convention) with:
     "Game-instantiated objects: reach `Game` and every other object the game
     instantiates behind an engine `*Base` class through its `gp*` global
     (`game::gpGame`, `gpProfileManager`). Never store a reference or pointer to
     it (no `GameBase& mrGameBase;`), never pass it as a parameter
     (`GameBase&`, `ProfileManagerBase&`), and never name the `*Base` type to
     reach it; the game instantiates the concrete type, and engine code may use
     game globals (`Engine/Source/AGENTS.md` `## Hub Conventions`). A class's
     own members use `this`; per-instance data bases such as
     `FrameInterpolateBase` are passed normally."
   - Reviewer check: `.agents/skills/repo-code-review/references/checks.md`
     `### Repository patterns`, a new bullet in the existing hard-flag form:
     "Flag a stored reference or pointer member, constructor argument, or
     function parameter of a game-instantiated `*Base` type (`GameBase`,
     `ProfileManagerBase`) or its game type when the object is reachable through
     its `gp*` global (the conventions reference). This is a hard flag, not a
     suggestion."
   - Not `Documents/C++StyleGuide.txt`: it is the source for `/code-style-review`,
     which applies only provably meaning-preserving fixes; removing a parameter
     changes signatures and call sites, which is correctness-review territory
     `/repo-code-review` already loads the conventions reference for.

Why one Plan: the user asked for one follow-up Plan; every site shares one root
cause (the convention was never stated as a checkable rule), one mechanical
transformation, one invariant (behavior preserved; the object reached is the
same single instance), and one verification (both builds plus the absence
grep below).

## Critical files

- `Engine/Source/File/Replay.h`, `Engine/Source/File/Replay.cpp`,
  `Engine/Source/GameBase.cpp` (`GameBase::GameBase`)
- `Engine/Source/File/GridSave.h`, `Engine/Source/File/GridSave.cpp`
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.h`,
  `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp`,
  `Projects/BrokenEngineSandbox/Source/Game.cpp` (`Game::Game`)
- `Engine/Source/Network/Client/ReconcileReplay.h`,
  `Engine/Source/Network/Client/ReconcileReplay.cpp`,
  `Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp`
- `Engine/Source/Ui/Screens/{MainMenu,Modal,PauseMenu,GraphicsMenu,SoundMenu,GameSettings}Screen.h/.cpp`,
  `Engine/Source/Graphics/Managers/ImGuiManager.cpp`
- `Engine/Source/Agent/AgentCommandsClientGeneric.h`,
  `Engine/Source/Agent/AgentCommandsClientGeneric.cpp`,
  `Engine/Source/Engine.h`,
  `Projects/BrokenEngineSandbox/Source/Agent/AgentCommands.cpp`
- `Engine/Source/Profile/ProfileManagerBase.h`,
  `Engine/Source/Profile/ProfileManagerBase.cpp`,
  `Engine/Source/Profile/ProfileScreens.cpp`,
  `Engine/Source/Server/ServerDisplay.cpp`
- `Engine/Source/Agent/AGENTS.md`, `Engine/Source/Ui/Screens/AGENTS.md`,
  `Engine/Source/Graphics/Managers/AGENTS.md`
- `.agents/references/cpp-conventions.md`,
  `.agents/skills/repo-code-review/references/checks.md`

## In scope

- `Replay`: the `mrGameBase` member, the constructor signature, the
  `class GameBase;` forward declaration in `Replay.h`, every `mrGameBase` use
  in `Replay.cpp`, and the `make_unique<Replay>` call in `GameBase::GameBase`.
- `GameSaveLoad`: the `mrGameBase` member, its constructor declaration and
  definition, the `engine::GameBase` forward declaration in `GameSaveLoad.h`,
  every `mrGameBase` use in `GameSaveLoad.cpp`, and the `mGameSaveLoad(*this)`
  initializer with its `#if defined(BT_SERVER)` bracket in `Game::Game`.
- The `GameBase&` parameter of `WriteGridSave`, the four-argument
  `ReadGridSave`, and `AdoptGridSave`, their bodies' game accesses, the
  `Game.h` include in `GridSave.cpp`, and their callers' argument lists.
- `ReconcileDispatcher::Run`'s `GameBase&` parameter, its body's game accesses,
  a `Game.h` include in `ReconcileReplay.cpp` if needed, the class comment in
  `ReconcileReplay.h`, and the `ClientReconciler.cpp` call.
- The six standard screens' `Render` signatures, bodies' `rGame` accesses,
  header forward declarations, `.cpp` `Game.h` includes, and the six
  `ImGuiManager.cpp` calls.
- `ExecuteClientAgentCommand`, `BuildDescribeUi`, `BeginScriptAndDefer` (its
  `pGame` parameter, lambda capture, and `BuildDescribeUi` call),
  `CommandDescribeUi`, `CommandClick`, `CommandHover`, `CommandQueryProfile`
  signatures and their game/profile accesses; the header comment sentence and
  the `BeginScriptAndDefer` comment sentences naming those parameters; the
  `Engine.h:101` comment; the `AgentCommands.cpp:37` call.
- The `ProfileManagerBase&` parameter of the six profile formatters, the five
  declarations in `ProfileManagerBase.h` (all but the file-local
  `FormatGpuTimerRows` in `ProfileScreens.cpp`), their bodies' accesses, a
  `Profile/ProfileManager.h` include in `ProfileScreens.cpp` if needed, and the
  callers in `ProfileManagerBase.cpp`, `ProfileScreens.cpp`, and
  `ServerDisplay.cpp`.
- The three AGENTS.md passages named in `## Context`.
- The "Base classes:" bullet in `.agents/references/cpp-conventions.md` and one
  new bullet in `.agents/skills/repo-code-review/references/checks.md`
  `### Repository patterns`.

## Out of scope

- Any behavior, determinism/CRC, wire, save or replay format, or threading
  change; any other replay or save logic in `Replay.cpp` or
  `GameSaveLoad.cpp`.
- `FrameInterpolateBase`/`FramePostRenderBase` parameters, the
  `TweaksScreenBase` section callbacks, and other items listed as not in scope
  in `## Context`.
- `WriteGridSave`'s `clientGridCoord` and `ReadGridSave`'s `rClientGridCoord`
  value parameters, and the three-argument `ReadGridSave` overload.
- Include cleanups beyond the includes this change needs.
- `Documents/C++StyleGuide.txt`, `/code-style-review`, and any other skill,
  AGENTS.md, or reference text.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: a change spanning independently owned
subsystems (File replay, game Save, client Network reconcile, standard UI
screens, client Agent commands, Profile formatters, and ChangeWorkflow
instructions). No determinism/CRC, serialization, wire, or threading surface
changes: every replaced access reaches the same single `Game` or
`ProfileManager` instance.

Preserve these invariants:

- `Replay` and `GameSaveLoad` constructors and destructors do not read
  `game::gpGame` (it is null during `GameBase::GameBase` and after
  `Game::~Game`'s body).
- The deferred UI script still dumps UI exactly when `bDescribeUiAfter` is set.
- `Engine.h` include order and affinity spans are unchanged.

## Acceptance criteria

- `git grep -n -E 'mrGameBase|(GameBase|ProfileManagerBase)\s*(const\s*)?[&*]' -- Engine Projects Common Tools DataPacker ':!**/*.md'`
  returns no matches.
- Client and server `Debug|x64` builds pass through `/compile`.
- Server quicksave/quickload and a replay record-then-playback round trip, and
  client `describe_ui`, `click` with `describeUiAfter`, and `query_profile`,
  behave as before under `/agent-harness`.
- The three AGENTS.md passages, `cpp-conventions.md`, and `checks.md` carry the
  wording decided in `## Design`.

## Notes

Origin: user direction in a `/next-plan` session after reviewing a change that
added `Replay::IsPlaybackActiveOrPending` and
`Replay::IsRecordingActiveOrPending`, both reading `mrGameBase`; no acceptance
criterion of that change is unmet. `Replay.cpp` line numbers shift under other
replay Plans, so locate sites by symbol and `mrGameBase` token. `CommandHover` is also the
admission site of `Documents/Plans/Engine/AgentHoverTimeoutCancellation.md`;
the two changes touch different lines of that function and neither depends on
the other. The `checks.md` edit is a skill package change, so the session runs
`/external-skill-creator` validation, and `/progressive-disclosure-review`
covers the instruction prose.
