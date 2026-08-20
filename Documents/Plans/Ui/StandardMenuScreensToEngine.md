<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T17:52:05.622Z","dependsOn":["Documents/Plans/Graphics/CameraOwnershipToEngine.md","Documents/Plans/Ui/EngineGameLocalizationSplit.md"]} -->
# Move the standard ImGui menu screens into the engine

## Context

Implementation baseline: `9428bde972a63560667458798e368fd74d0d5982` (`Update world-grid ownership investigation`).

`Engine/Source/Graphics/Managers/ImGuiManager.h:80-86` currently stores seven
screen objects as `game::` types, and
`Engine/Source/Graphics/Managers/ImGuiManager.cpp:180-186` constructs them in
the same order that `Prepare()` renders them at `:512-523`. Six of those
screens are functional generic menus: Main Menu, Pause, Graphics, Sound, Game
Settings, and Modal. `Projects/BrokenEngineSandbox/Source/Ui/Screens/DeathMenuScreen.cpp:12`
is an empty placeholder and has no behavior to preserve; it is deleted rather
than moved. HUD and Tweaks remain game-owned.

The functional screens currently reach directly into game networking,
settings persistence, frame transitions, UI state, and the game camera. For
example, Main Menu directly starts discovery, launches the server, and connects
with `NetworkSessionContract::kiCoordSlots`
(`Projects/BrokenEngineSandbox/Source/Ui/Screens/MainMenuScreen.cpp:84-129`),
Graphics seeds the sun override from `gpCamera->RawSunAngle()` at `:151`, and
Pause calls `Game::ChangeFrame()` at `PauseMenuScreen.cpp:70-74`.

This plan depends on the two completed prerequisite Plans below. Their landed
results must be consumed before implementation, not recreated here:

- `Documents/Plans/Ui/EngineGameLocalizationSplit.md` — engine
  `LocalizationBase.h`, `Language`, `StandardString`, `TranslatedString`,
  `geLanguage`, and `kLanguageOptions`.
- `Documents/Plans/Graphics/CameraOwnershipToEngine.md` — engine
  `EngineCamera.h`, `engine::gpCamera`, and `RawSunAngle()`.

The engine will own six closed concrete screen classes. A small virtual game
contract supplies only stable menu identity and the live network/automatic
connection state the engine cannot read directly; game-owned side effects stay
behind one synchronous action method. No factory, registration mechanism,
abstract screen seam, game screen list, or replacement hook is added.

## Design

### Shared engine state and the GameBase contract

`engine::GameBase::InMainMenu() const` becomes the shared direct query of
`mGameFlags` and is not client-guarded:

```cpp
bool InMainMenu() const
{
	return mGameFlags & GameFlags::kMainMenu;
}
```

Remove the duplicate `Game::InMainMenu()` declaration/definition. Existing
server and client callers continue to use the inherited engine query, and
`Game::ChangeFrame(GameFlags_t)` remains unguarded so the server build keeps
compiling its existing frame-transition implementation.

Only the standard-menu types and virtuals below are client-only. Place them in
the `BT_CLIENT` portion of `GameBase.h`; no server declaration may depend on a
client-only menu type.

```cpp
enum class StandardMenuFeature : uint8_t
{
	kLocalServer      = 0x01,
	kRemoteServer     = 0x02,
	kGraphicsSettings = 0x04,
	kSoundSettings    = 0x08,
	kGameSettings     = 0x10,
};
using StandardMenuFeature_t = common::Flags<StandardMenuFeature>;

enum class StandardMenuState : uint8_t
{
	kClientPresent          = 0x01,
	kDiscoveryScannerPresent = 0x02,
	kServerDiscovered       = 0x04,
	kDiscoveryScanTimedOut  = 0x08,
	kConnectionAccepted     = 0x10,
	kAutoConnect            = 0x20,
	kAutoRunServer           = 0x40,
};
using StandardMenuState_t = common::Flags<StandardMenuState>;

struct StandardMenuModel
{
	std::string_view title; // Must refer to stable game-owned storage.
	StandardMenuFeature_t features;
	StandardMenuState_t state;
};

enum class StandardMenuAction
{
	kStartDiscovery,
	kLaunchServer,
	kConnectToDiscoveredServer,
	kChangeFrameToMainMenu,
	kSaveGraphicsSettings,
	kResetGraphicsSettings,
	kResetSoundSettings,
	kResetGameSettings,
	kSaveGameSettings,
};
```

Add these client-only virtuals to `GameBase`:

```cpp
virtual StandardMenuModel GetStandardMenuModel() const = 0;
virtual void ApplyStandardMenuAction(StandardMenuAction eAction) = 0;
```

Each screen has the explicit neutral render seam:

```cpp
void Render(GameBase& rGame);
```

The screen reads `rGame.meUiState`, `rGame.mModalMessage`,
`rGame.mGameFlags`, and `rGame.InMainMenu()` directly. It calls
`rGame.GetStandardMenuModel()` for the stable title/features and live state,
and calls `rGame.ApplyStandardMenuAction()` only for game-owned operations.
It never names `game::Game`, `game::gpGame`, or a game session type.

`Game` implements the model with stable title `"BROKEN ENGINE"`, all five
features enabled, and a state mask populated from the current client runtime:

| State bit | Current source value |
| --- | --- |
| `kClientPresent` | `mpClient != nullptr` |
| `kDiscoveryScannerPresent` | `mpDiscoveryScanner != nullptr` |
| `kServerDiscovered` | `ClientSessionStateFlags::kServerDiscovered` |
| `kDiscoveryScanTimedOut` | `ClientSessionStateFlags::kDiscoveryScanTimedOut` |
| `kConnectionAccepted` | `ClientStateFlags::kConnectionAccepted` |
| `kAutoConnect` | current `kbAutoConnect` build constant |
| `kAutoRunServer` | current `kbAutoRunServer` build constant |

`ApplyStandardMenuAction()` has exactly one case for each action:

| Action | Sandbox implementation |
| --- | --- |
| `kStartDiscovery` | `ClientSessionRuntime::StartDiscovery()` |
| `kLaunchServer` | Existing executable path construction and `common::LaunchExecutable()` |
| `kConnectToDiscoveredServer` | `ConnectToDiscoveredServer(engine::kuiDefaultPort, NetworkSessionContract::kiCoordSlots)` |
| `kChangeFrameToMainMenu` | Existing `ChangeFrame(GameFlags::kMainMenu)` |
| `kSaveGraphicsSettings` | `SaveGraphicsSettings()` |
| `kResetGraphicsSettings` | `ResetGraphicsSettings()` |
| `kResetSoundSettings` | `ResetSoundSettings()` |
| `kResetGameSettings` | `ResetGameSettings()` |
| `kSaveGameSettings` | `SaveGameSettings()` |

The action method is synchronous. A screen rereads the model after a
synchronous action only when later code in that same render pass makes another
network/model-dependent decision. D7 language labels are read directly from
engine `geLanguage`/`TranslatedString`; language changes do not require a
model refresh.

### Screen ownership and behavior

Move these six pairs from
`Projects/BrokenEngineSandbox/Source/Ui/Screens/` to
`Engine/Source/Ui/Screens/`, changing the namespace to `engine`:

- `MainMenuScreen.{h,cpp}`
- `PauseMenuScreen.{h,cpp}`
- `GraphicsMenuScreen.{h,cpp}`
- `SoundMenuScreen.{h,cpp}`
- `GameSettingsScreen.{h,cpp}`
- `ModalScreen.{h,cpp}`

Delete `DeathMenuScreen.{h,cpp}` from the game tree. Do not create an engine
Death screen.

Preserve the existing windows, labels, layout, hover state, persistence timing,
and transitions. The game-specific direct calls become these mappings:

- Main Menu:
  - When `kLocalServer` is enabled, preserve discovery, timeout, automatic
    launch, automatic connection, and Local Server click behavior through the
    action API.
  - When `kLocalServer` is absent, do not start discovery, launch a server,
    auto-connect, or show either the scanning placeholder or Local Server.
  - When `kRemoteServer` is absent, hide the disabled Remote Server placeholder;
    when present, retain its current disabled behavior.
  - Graphics directly sets `UiState::kGraphicsSettings` and
    `engine::gSunAngleOverride` from `engine::gpCamera->RawSunAngle()`.
  - Audio and Game Settings directly set `UiState::kSound` and
    `UiState::kGameSettings`.
  - Quit directly sets `GameFlags::kQuit`.
  - Preserve `AutoConnectState` and the automatic server-launch static.
- Pause Menu:
  - Resume directly sets `UiState::kNone`.
  - Settings entries are shown only when their feature bit is present and
    directly set the corresponding `UiState`.
  - Main Menu invokes `kChangeFrameToMainMenu`, then directly restores
    `UiState::kPause`.
  - Quit directly sets `GameFlags::kQuit`.
- Modal:
  - Continue reading `mModalMessage` directly.
  - OK directly sets `UiState::kPause` and clears the message.
- Graphics:
  - Keep every engine wrapper control and quality apply operation unchanged,
    including three-decimal Water Detail, four-decimal Minimum Ambient, and
    whole-number Lighting Update Cadence display formats.
  - Defaults invokes `kResetGraphicsSettings` and stays in Graphics.
  - Back invokes `kSaveGraphicsSettings`, then returns to pause.
- Sound:
  - Keep the `SoundMenu` window, `UiState::kSound`, internal Sound naming,
    `SoundSettings.bin`, and this exact visible/automation label list:
    `AUDIO`, `Master Volume`, `Music Volume`, `Sound Volume`,
    `Mute in background`, localized Defaults, and `Back`.
  - `Mute in background` is checked by default; when checked, background audio
    is completely suspended. Reset/save/load preserve the setting, and agents
    still boot silent.
  - Defaults invokes `kResetSoundSettings`; Back returns to pause directly.
- Game Settings:
  - Consume D7 `engine::kLanguageOptions`, `engine::geLanguage`, and
    `engine::TranslatedString` while preserving the six labels and row layout.
  - Defaults invokes `kResetGameSettings`.
  - Back invokes `kSaveGameSettings`, then returns to pause.

Feature-off settings screens return before drawing, so both their menu entry and
screen are hidden. All five features remain enabled in the sandbox model.

### ImGuiManager and startup lifetime

Change `ImGuiManager`'s seven standard screen members, includes, and
construction to the six `engine` screen types. Remove all Death declarations,
construction, and render calls. Keep `HudScreen` and `TweaksScreen` as game
types.

Preserve the relative render order:

1. Main Menu
2. Modal
3. HUD
4. Pause
5. Graphics
6. Sound
7. Game Settings
8. Tweaks

`ImGuiManager` is constructed by Graphics before `Main.cpp` constructs
`game::Game` (`Engine/Source/Main.cpp:261-271`). The six screen constructors
must remain implicit/inert: no constructor body, game dereference, filesystem
operation, or heap allocation. `Render(GameBase&)` is not called until after
the game exists.

The moved Main Menu server-launch action must retain the existing
`ScopedSuppressAllocationTracking` scope around `GetModuleFileNameW`,
`std::filesystem::path`, `std::format`, and `common::LaunchExecutable()` work.

## Critical files

Screen moves/deletion:

- `Engine/Source/Ui/Screens/MainMenuScreen.{h,cpp}` — new engine owner.
- `Engine/Source/Ui/Screens/PauseMenuScreen.{h,cpp}` — new engine owner.
- `Engine/Source/Ui/Screens/GraphicsMenuScreen.{h,cpp}` — new engine owner.
- `Engine/Source/Ui/Screens/SoundMenuScreen.{h,cpp}` — new engine owner.
- `Engine/Source/Ui/Screens/GameSettingsScreen.{h,cpp}` — new engine owner and D7 option consumer.
- `Engine/Source/Ui/Screens/ModalScreen.{h,cpp}` — new engine owner.
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/DeathMenuScreen.{h,cpp}` — delete.

Contract and game implementation:

- `Engine/Source/GameBase.h` — shared `InMainMenu`; client-only standard-menu types and virtuals.
- `Projects/BrokenEngineSandbox/Source/Game.h` — remove duplicate `InMainMenu`; add client overrides.
- `Projects/BrokenEngineSandbox/Source/Game.cpp` — model/action implementations; preserve unguarded `ChangeFrame`.
- `Projects/BrokenEngineSandbox/Source/ClientSettings.h` — existing persistence declarations consumed through the action method; no format change.
- `Projects/BrokenEngineSandbox/Source/ClientSettings.cpp` — existing persistence implementation; no format change.

Manager and membership:

- `Engine/Source/Graphics/Managers/ImGuiManager.h`
- `Engine/Source/Graphics/Managers/ImGuiManager.cpp`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj.filters`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj.filters`

Prerequisite symbols to verify after their actual landings:

- D7: `Engine/Source/Ui/LocalizationBase.h`, `engine::Language`,
  `engine::StandardString`, `engine::TranslatedString`,
  `engine::geLanguage`, and `engine::kLanguageOptions`.
- D8: `Engine/Source/Graphics/EngineCamera.h`, `engine::gpCamera`, and
  `engine::Camera::RawSunAngle()`.

Documentation:

- `Engine/Source/Ui/AGENTS.md`
- `Engine/Source/Ui/Screens/AGENTS.md`
- `Engine/Source/Graphics/Managers/ImGuiManager.AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Ui/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/AGENTS.md`
- `Documents/UserInterfaceDesign.txt`
- `Documents/Features/Engine/RmlUiPlayerFacingUi.md` — remove stale Death/current-path claims only; do not implement RmlUi.

## In scope

- Add the exact client-only standard-menu feature/state/action/model contract to `GameBase.h`.
- Add the unguarded shared `GameBase::InMainMenu() const` and remove the duplicate game method.
- Add the six `Game` client overrides and action dispatch while preserving the existing game networking, server launch, settings, and frame-transition code.
- Move the six functional screen pairs into `Engine/Source/Ui/Screens/` and change each render signature to `Render(GameBase&)`.
- Delete the empty Death screen and all corresponding manager/project/filter references.
- Preserve the six screen windows, labels, layout, action ordering, persistence, networking, and automatic connection statics.
- Update `ImGuiManager` to own/invoke the six engine screens and retain HUD/Tweaks as game screens.
- Update client-only project/filter membership and remove all standard screen membership from the server project.
- Verify D7/D8 prerequisite symbols and record their actual landed commits during coordination preparation.
- Update affected ownership/layout documentation and run the required affected-code, project-membership, style, and documentation checks.

## Out of scope

- Implementing D7 localization or D8 camera ownership.
- Moving or rewriting HUD or Tweaks.
- RmlUi, factories, registration, abstract screen interfaces, replacement hooks, or a game-supplied screen list.
- Changes to MenuUtils, ImGui theme/layout formulas, input processing, agent command schemas, or network protocol behavior.
- Changes to settings POD layouts, versions, filenames, or serialized bytes.
- Changes to simulation, CRC, replay, wire state, frame layout, or server behavior.
- Retaining compatibility aliases for the deleted Death screen or old game screen classes.
- Unit tests or a new test framework.

## Risk tier and invariants

Tier 3 — the change crosses the engine/game ownership boundary, introduces a
client-only public contract, changes project affinity, and routes network,
filesystem, settings, and frame-transition behavior through a new synchronous
action boundary.

Invariants:

1. Feature masks remain exactly `0x01`, `0x02`, `0x04`, `0x08`, and `0x10` in the requested order.
2. State masks remain exactly `0x01` through `0x40` in the requested order.
3. `GameBase::InMainMenu() const` is shared/unguarded; only standard-menu types and virtuals are client-only.
4. `Game::ChangeFrame(GameFlags_t)` remains compilable in both targets and retains its existing ordering.
5. `StandardMenuModel::title` points to stable storage and is not backed by the workbuffer.
6. The sandbox model enables all five features.
7. Missing Local Server suppresses discovery, server launch, auto-connect, scanning, and the local entry.
8. Missing Remote Server hides its placeholder; missing settings features hide both entry and screen.
9. The six screen constructors are inert and allocation-free; screen construction occurs safely before `game::Game` exists.
10. Standard screens do not directly access game globals or game session types; all game-owned actions use the `GameBase` contract.
11. `NetworkSessionContract::kiCoordSlots` remains in the game action implementation.
12. The server-launch action preserves `ScopedSuppressAllocationTracking` over all filesystem/format/launch work.
13. Main Menu, Pause, Graphics, Sound, Game Settings, and Modal window names,
    labels, layout, and relative render order remain unchanged; Sound includes
    the exact `Mute in background` checkbox, checked by default and fully
    suspending background audio when checked.
14. Existing auto-connect and automatic server-launch statics retain their lifetime and transition behavior.
15. D7 language labels and D8 `RawSunAngle()` are consumed through the prerequisite engine symbols.
16. No standard screen source, member, or project entry remains for Death.
17. No standard screen source is compiled by the server project.
18. No settings file, network packet, deterministic frame state, CRC, replay, or simulation data changes.
19. No new main-loop heap allocation is introduced by the moved UI or action path.

## Coordination

The Plan metadata must carry these exact directional dependencies:

- `Documents/Plans/Ui/EngineGameLocalizationSplit.md`
- `Documents/Plans/Graphics/CameraOwnershipToEngine.md`

Before implementation, the preparation record must capture the actual landed
commit for each dependency. Future commit IDs must not be guessed. For D7,
verify with `rg` that `LocalizationBase.h`, `engine::Language`,
`engine::StandardString`, `engine::TranslatedString`, `engine::geLanguage`,
and `engine::kLanguageOptions` exist in the landed tree. For D8, verify
`EngineCamera.h`, `engine::gpCamera`, and `RawSunAngle()` exist and that the
camera prerequisite's corrected result is present. Record the commit IDs,
targeted `git show` evidence, and the `rg` results before touching D5 files.

Implementation proceeds as one screen/contract slice followed by
`/update-affected-code`. The implementer must reread the landed D7/D8 symbols,
move only the six functional screens, delete Death, and return all affected
callers. A builder compiles client and server targets. `/update-vcxproj`
validates client-only membership and Death deletion. `/code-style-review` and
`/update-claude-docs` run after the C++ move. Tier-3 review includes correctness,
scope, simplicity, and adversarial review as routed by the Change Workflow.

## Acceptance criteria

### Contract, masks, and build affinity

- Inspect `GameBase.h` and compile both targets.
- Expect `InMainMenu() const` to be unguarded/shared, the feature/state masks to
  match the exact values above, and all model/state/action types and virtuals to
  be client-only.
- Expect `Game::ChangeFrame(GameFlags_t)` to remain available to the server
  target without naming a client-only type.
- Independently verify the D7/D8 prerequisite symbols and actual landed commits
  before implementation.

### Screen ownership and ImGui manager

- Static changed-region/C++ review proves exactly six screen pairs move to the
  engine, each exposes `Render(GameBase&)`, and Death has no remaining file,
  member, construction, render, or project/filter entry.
- Inspect `ImGuiManager` construction and `Prepare()` order. Expect Main,
  Modal, HUD, Pause, Graphics, Sound, Game Settings, Tweaks, with all relative
  functional-screen ordering preserved.
- Static inspection proves the six constructors are implicit/inert, perform no
  game dereference, filesystem operation, or allocation.
- Launch the client with the normal harness startup path. It must reach its
  first UI query without a null-game crash even though Graphics/ImGuiManager
  construction precedes `game::Game` construction in `Main.cpp`.

### Static action and UI mapping

Use an independent source check over the changed regions. Expect one action
dispatch case for each action and no direct game-only call from an engine screen.

| Screen/window | Expected labels and transitions |
| --- | --- |
| `MainMenu` | `BROKEN ENGINE`, `SCANNING...`, `LOCAL SERVER`, `REMOTE SERVER`, `GRAPHICS`, `AUDIO`, `GAME SETTINGS`, `QUIT`; local discovery/launch/connect actions; settings state transitions; direct quit and D8 sun seed. |
| `PauseMenu` | `PAUSED`, `RESUME`, `GRAPHICS`, `AUDIO`, `GAME SETTINGS`, `MAIN MENU`, `QUIT`; direct UI transitions; synchronous Main Menu frame action. |
| `GraphicsMenu` | `GRAPHICS`, localized Defaults, `Back`, `Time of Day`, `Minimum Ambient`, `Fullscreen`, `Presentation Mode`, `Immediate`, `Mailbox`, `FIFO`, `Multisampling`, `2x`, `4x`, `8x`, `16x`, `Sample Shading`, `Min Sample Shading`, `Anisotropy`, `Max Anisotropy`, `Mip Lod Bias`, `Water Detail`, `Terrain Shadows`, `Object Shadows`, `Lighting`, `Lighting Detail`, `Lighting Update Cadence`, `Smoke`, `Smoke Detail`, `Smoke Area`, `Wind`; dependent controls remain visible and disabled when their parent is off, Defaults calls `ResetGraphicsSettings`, and Back calls `SaveGraphicsSettings`. |
| `SoundMenu` | Exact labels: `AUDIO`, `Master Volume`, `Music Volume`, `Sound Volume`, `Mute in background`, localized Defaults, `Back`; `Mute in background` is checked by default and checked means complete background audio suspension; reset/save/load preserve the setting; agents still boot silent; Defaults calls `ResetSoundSettings`. |
| `GameSettingsMenu` | localized Game Settings heading, `ENGLISH`, `中文`, `ESPANOL`, `PORTUGUES`, `FRANCAIS`, `DEUTSCH`, `Font Size`, `Opaque UI`, `UI Opacity`, `Theme`, `Naval Steel`, `Dark Amber`, `Midnight Mauve`, localized Defaults, `Back`; Defaults calls `ResetGameSettings`, Back calls `SaveGameSettings`. |
| `ModalDialog` | existing dynamic message and `OK`; OK returns to pause and clears the modal buffer. |

The static action check must prove that `NetworkSessionContract::kiCoordSlots`,
`common::LaunchExecutable`, settings persistence calls, and `ChangeFrame` occur
only in the intended `Game` action implementation. It must also prove the
server-launch case retains `ScopedSuppressAllocationTracking` around
`GetModuleFileNameW`, `std::filesystem::path`, `std::format`, and launch work.

### Feature/state behavior

- With all sandbox feature bits enabled, `describe_ui` observes the existing
  windows and labels.
- Static source review proves Local Server absence suppresses discovery,
  auto-launch, auto-connect, scanning, and the Local Server entry.
- Static source review proves Remote Server absence hides its placeholder and
  each settings feature absence hides both its menu entry and screen.
- After synchronous networking actions, later same-frame network decisions use
  a fresh model read. D7 language labels read engine localization directly and
  do not depend on model refresh.

### Harness flow and prohibited logs

Using a fresh app-data root and the project AgentHarness recipe:

1. Launch the server and client, wait for both harness ports, restore the client
   window, and query `describe_ui`.
2. Verify Main Menu labels and click Local Server; confirm discovery/connection
   progresses without new errors.
3. Open Graphics, verify the settings window and `Time of Day` behavior from
   both the main menu and in-game pause entry, exercise Defaults without leaving
   Graphics, click Back, and confirm return to pause.
4. Open Sound and Game Settings from the menu flow; exercise Defaults, Back,
   and one language option, confirming the expected windows/labels and settings
   transitions.
5. Enter the in-game pause menu, exercise Resume, Graphics, Sound, Game
   Settings, Main Menu, and Quit-state behavior as applicable.
6. Exercise the existing modal path and confirm message display, OK, pause
   return, and message clear.
7. Compare newly appended client/server logs and reject any new
   `LogDifferences CRC Client`, `CONFIRMED DESYNC`, checksum-mismatch,
   `SaveLoadReplay aborted`, or replay-reader error lines.

The UI query/action observations are independent of the static source mapping;
the changed-region C++ review independently proves constructor inertness,
allocation behavior, and action ownership.

### Project, documentation, and scope checks

- `/update-vcxproj` proves all six moved screen pairs are client-only under
  `Engine\\Ui\\Screens`, the old game entries are absent, Death is absent from
  both projects, and HUD/Tweaks membership is unchanged.
- Client Debug and Release plus server Debug compile and link through the
  repository compile workflow.
- Documentation review finds no stale current ownership claim for the six
  screens or deleted Death screen; D7/D8 prerequisite documents and historical
  investigation packets are not rewritten.
- No unit tests are added.

## Execution card

### Objective and tier

- Objective: move six functional standard ImGui screens into engine ownership,
  delete the empty Death placeholder, and preserve current sandbox behavior.
- Tier: Tier 3.
- Baseline: `9428bde972a63560667458798e368fd74d0d5982`.
- Pre-existing ownership snapshot: clean at preparation; no unrelated paths are
  authorized by this body.

### Roles and checks

- Implementer: six-screen move, GameBase contract, Game action bridge, deletion,
  propagation, and documentation.
- Fresh reviewers: plan audit, simplicity, C++ correctness, scope, adversarial,
  and final verification as required by the Change Workflow.
- Mechanic: `/update-vcxproj` and `/code-style-review`.
- Implementer: `/update-affected-code` and `/update-claude-docs`.
- Builder: client Debug/Release and server Debug compile receipts.
- Harness implementer: startup-before-Game proof and menu/action flow.

### Required evidence

- Actual D7/D8 landed commit IDs plus prerequisite-symbol `rg` proof.
- Changed-region C++ review proving six inert constructors, no constructor
  allocations/dereferences, and preserved launch suppression scope.
- Independent source mapping for masks, actions, windows, labels, transitions,
  settings calls, network calls, and deleted Death references.
- Client/server project membership validation and compile receipts.
- Harness `describe_ui`/action results and prohibited-log scan.

### Prohibitions

- No Death replacement or compatibility stub.
- No factory, registration, abstract screen interface, or RmlUi integration.
- No direct game global/session access from moved screens.
- No settings-format, network-protocol, CRC, replay, simulation, or unit-test
  changes.

## Scores

Effort 4 / Impact 5 / Risk 3
