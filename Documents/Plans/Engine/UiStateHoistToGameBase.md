<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-11T00:00:03.000Z","dependsOn":[]} -->
# Hoist UiState and its members to engine GameBase

## Context

`enum class UiState` (`Projects/BrokenEngineSandbox/Source/Game.h:27-38`) names only engine-owned screen concepts: `kNone`, `kGameSettings`, `kGraphicsSettings`, `kModal`, `kPause`, `kSound`, `kTweaks`. Nothing in it is specific to this game. Its state members live beside it — `meUiState` and `mModalMessage[256]` (`Game.h:154-155`) and `mbShowImGui` (`Game.h:157`), all three outside any `BT_CLIENT` guard, so both builds compile them today.

Engine-owned concerns already read them through `game::gpGame`:

- `Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp:316,319` — the graphics-settings camera override and the ImGui-visible check.
- `Projects/BrokenEngineSandbox/Source/Input/Input.cpp:88` — suppressing game input while any UI state is up.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp:933` and `Agent/AgentScene.cpp:193` — the harness UI-state query.

Every future extraction of a screen, of the input gate, or of the agent UI snapshot is blocked until the state they read is engine-owned. There are 47 `UiState` references across the tree (spellings of the type or an enumerator, plus `UiStateName`), in `Game.h`, `Game.cpp`, `Graphics/Camera.cpp`, `Input/Input.cpp`, `Agent/AgentScene.{h,cpp}`, `Agent/AgentCommandsClient.cpp`, `Network/Client/ClientSession.cpp`, and seven screens under `Ui/Screens/`. Member accesses without the `UiState` spelling — `ClientSettings.cpp:310,327` (`mbShowImGui` persistence), `Network/Client/ClientDesyncManager.cpp:33,58,80,106` (`mModalMessage` writers), `Ui/Screens/TweaksScreen/TweaksScreen.cpp:14` — reach the members through `gpGame` and keep compiling unchanged once the members are inherited.

## Design

Move the enum and the three members to `engine::GameBase` (`Engine/Source/GameBase.h`), which `game::Game` already derives from:

- `enum class UiState` goes at namespace scope beside the existing `MenuFlags`/`GameFlags` enums (`GameBase.h:23-37`), unguarded, enumerators byte-identical.
- `meUiState`, `mModalMessage`, and `mbShowImGui` become public `GameBase` members beside `mGameFlags` (`GameBase.h:184`), unguarded — matching their current unguarded declaration in `Game.h`, so the server build keeps compiling them exactly as today. `meUiState` keeps its initializer `UiState::kPause`, `mModalMessage` its 256-byte extent and zero initializer, `mbShowImGui` its `false`.

`GameBase.h` is already aggregated at `Engine.h:100`, which the game PCH force-includes, so every TU sees the new declarations without include edits. Nothing changes about who writes them; only the declaring class changes, so every reference is compile-checked and a missed site is a build error.

`game::UiState` is not kept as an alias: leaving one behind would leave two spellings of one concept. Every `UiState` spelling in game code therefore requalifies to `engine::UiState` (or an enumerator under it); `using enum UiState;` at `Game.cpp:15` becomes `using enum engine::UiState;`, which keeps `Game.cpp`'s unqualified enumerator uses compiling unchanged. `Agent/AgentScene.h` changes `UiStateName`'s parameter to `engine::UiState`, drops its `#include "Game.h"` (present solely for `UiState`, per its own comment at `AgentScene.h:18`), and updates that comment; `AgentScene.cpp` already includes `Game.h` itself for `gpGame`.

No files are created, deleted, or change project affinity, so `/update-vcxproj` is not triggered.

## Critical files

- `Engine/Source/GameBase.h` — new home for the enum and the three members
- `Projects/BrokenEngineSandbox/Source/Game.h`, `Game.cpp` — removals, requalification, the `using enum` at `Game.cpp:15`
- `Agent/AgentScene.h` — `engine::UiState` signature, `Game.h` include and comment removal
- `Graphics/Camera.cpp`, `Input/Input.cpp`, `Agent/AgentScene.cpp`, `Agent/AgentCommandsClient.cpp`, `Network/Client/ClientSession.cpp`, and `Ui/Screens/` `GameSettingsScreen.cpp`, `GraphicsMenuScreen.cpp`, `HudScreen.cpp`, `MainMenuScreen.cpp`, `ModalScreen.cpp`, `PauseMenuScreen.cpp`, `SoundMenuScreen.cpp` — requalification of `UiState` spellings (member accesses in `ClientSettings.cpp`, `Network/Client/ClientDesyncManager.cpp`, and `Ui/Screens/TweaksScreen/TweaksScreen.cpp` compile unchanged)
- `Projects/BrokenEngineSandbox/Source/Input/AGENTS.md:17` — names `UiState::kNone`; requalifies with the code

## In scope

- Moving `enum class UiState`, `meUiState`, `mModalMessage`, and `mbShowImGui` from `game::Game` to `engine::GameBase` with unchanged enumerators, extents, and initializers
- Requalifying every `UiState` reference, including the `using enum` in `Game.cpp`, the `UiStateName` mapping used by the agent queries, and `AgentScene.h`'s signature, include, and comment
- Any `Engine/Source/AGENTS.md` or game `AGENTS.md` sentence that names the old owner, including `Input/AGENTS.md:17`

## Out of scope

- Moving any screen class out of `Projects/BrokenEngineSandbox/Source/Ui/Screens/` — separate work
- Moving the input gate, camera override, or agent snapshot logic that reads these members
- Adding, removing, or renaming any `UiState` enumerator
- The menu chrome and widget helpers — owned by `Documents/Plans/Ui/MenuUtilsToEngine.md`
- `mbShowImGui` persistence format in `ClientSettings.cpp` (the settings POD keeps its current layout and version)

## Risk tier and invariants

Tier 2 — the hoist is mechanical with no behavior, layout, or persistence change, and the members are client UI state outside the CRC, but declaring the enum and the three members on public `engine::GameBase` adds to the engine's public interface, which Tier 1 excludes. It stays below Tier 3: no determinism/CRC, wire, serialization, save/replay, threading, or trust-boundary surface is touched, and the change stays inside one subsystem. Invariants: `meUiState`'s default stays `kPause` (the main menu depends on it), `mModalMessage` stays exactly 256 bytes because every writer passes `sizeof(...)` to `std::snprintf`, and the persisted `bShowImGui` field in the client settings POD is unaffected.

## Acceptance criteria

- Client and server compile with no `UiState` declaration remaining in the game layer, and a repo grep finds no `game::UiState` spelling — the qualification churn is compile-checked, so a clean build of both executables is the decisive evidence for the 47 requalified references.
- Launching the client shows the main menu; Escape toggles pause; a modal (forced connection failure) still displays its message.
- The harness `uiState` query returns the same strings as before.

## Scores

Effort 1 / Impact 3 / Risk 1
