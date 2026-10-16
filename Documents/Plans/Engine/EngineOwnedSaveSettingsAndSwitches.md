<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-15T23:33:13.608Z","dependsOn":[]} -->
# Move engine-generic save/load/autosave, the engine-read Pch.h switches, and the engine-only Tweaks settings and screens into the engine

## Context

A second game project will be created beside `Projects/BrokenEngineSandbox`. Today that project would have to copy several pieces of Sandbox code that are engine mechanics, not Sandbox gameplay. Constraints relayed by the dispatching session: engine code may call expected `game::` classes, globals, and members by name; engine code must not name Sandbox gameplay (`Blaster`, `Spaceship`, `Missile`, `Player`, `bUseMissiles`, `Fleet`); fleets stay game-owned; no backward compatibility; the server validates every grid save.

Verified at baseline `f1d140ae`:

- Save/load/autosave. `game::GameSaveLoad` (`Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.h:17-30`, `.cpp:51-153`) holds `ServerSave`, `ServerLoad` (the clock-preserving sequence around `game::gpGame->Reset()`), `ServerReset`, `Autosave`, `TickAutosave` (the hourly `kAutosaveInterval` timer, skipped while replaying, recording, or loading a replay), and `Autoload` (`ServerAutosave.save`). Every body reads only engine state (`engine::WriteGridSave`/`ReadGridSave`, `GameBase` clock and id members, `engine::gpReplay`, `LatchRawCpuTimers`) plus game hooks the engine already calls elsewhere (`game::ResetSaveState` from `GridSave.cpp:61`, `game::OnStateReplaced` from `Replay.cpp:283`). The engine already reaches into the game member to drive it: `game::gpGame->mGameSaveLoad.TickAutosave()` (`Engine/Source/GameBase.cpp:474`) and `pGame->mGameSaveLoad.Autosave()` (`Engine/Source/Main.cpp:519`). Game callers: `Game.cpp:43` (`Autoload`), `Agent/AgentCommandsServer.cpp:139,156,160,175`, `Network/Server/ServerSession.cpp:168,177,183,190`.
- Pch.h switches. `Projects/BrokenEngineSandbox/Source/Pch.h:5-95` declares 40 compile-time switches and log levels. A `git grep -w` per name shows all but `kbInvincibility` (`PlayersCombat.cpp`) and `kbAutoConnect` (`Game.cpp`) are read by `Engine/` or `Common/`; some are also read by game code. `keNetworkSimulation` (`Pch.h:106`) is read only by `Engine/Source/Network/Client/Client.cpp` and `ClientSessionRuntime.cpp`. `Common/Log/Log.h:191-197` requires the `keLogLevel*` constants before `Common.h`; `DataPacker/Source/Pch.h:6-21` supplies its own values for the tools build.
- Tweaks settings. `TweaksSettings.bin` (`Projects/BrokenEngineSandbox/Source/ClientSettings.cpp:13-60`, version 14, 304 bytes) holds only engine state: `GameBase::mbShowImGui` (`Engine/Source/GameBase.h:228`), `engine::gSunAngleOverride`, and the engine-owned `TweakSectionState` through `TweaksScreenBase::SaveState`/`LoadState`. Only engine code calls the game entry points: `Engine/Source/Main.cpp:301,526` and `Engine/Source/Graphics/Graphics.cpp:442,713`. The engine's own settings files follow one pattern per file (`Engine/Source/Ui/GameSettings.cpp`, `SoundSettings.cpp`, `GraphicsSettings.cpp`).
- Tweaks screen class. `game::TweaksScreen` (`Ui/Screens/TweaksScreen/TweaksScreen.h:10-15`, `.cpp:10-21`) adds only a `mbShowImGui` gate before `TweaksScreenBase::Render`. It exists only so `ImGuiManager` can construct it (`Engine/Source/Graphics/Managers/ImGuiManager.cpp:166`, `.h:8,63`).
- Hex Shield section. `Ui/Screens/TweaksScreen/TweaksScreenHexShield.cpp` registers and draws only `engine::gHexShield*` wrappers (`Engine/Source/Ui/HexShieldWrappersBase.h:10-22`). The game's `gHexShieldLightingIntensity` is drawn by the game Lighting extension tab (`TweaksScreenLightingEffectsLighting.cpp:40`), not by this section. Sections register engine first (`TweaksScreenBase.cpp:41-53`, the last being `Wind`), then game (`Main.cpp:288-289`; `HexShield`, then `Particles`).

## Design

Recommended stage order; each stage builds on its own, and one landing covers all three.

### Stage 1: engine-owned compile-time switches

1. New `Engine/Source/EngineSwitches.h`: `#pragma once`, `#include "Log/LogTypes.h"` (it depends on no switch), then every switch line from `Pch.h:5-95` except `kbInvincibility` and `kbAutoConnect`. Values, comments, and the `BT_SERVER`/`BT_DEBUG`/`BT_PROFILE`/`BT_RELEASE` blocks are copied unchanged. Recommended because the engine and Common read these switches, so a second game would otherwise have to copy them, and because one engine definition keeps the client-must-match-server switches (`kbDesynchronizationDebugFrames`) the same in every game.
2. `keNetworkSimulation` moves to `Engine/Source/Network/NetworkSimulation.h`, directly after the `NetworkSimulationLevel` enum. It needs that enum, which `EngineSwitches.h` cannot see before `Engine.h`, and only engine code reads it.
3. The game `Pch.h` keeps `#include "ExternalHeaders.h"`, `kbInvincibility`, and `kbAutoConnect` (in their per-configuration blocks), then `#include "EngineSwitches.h"`, then its include sequence `Common.h` … `Network/GameMessages.h` (`Pch.h:97-104`) with its comment, unchanged.

### Stage 2: engine-owned Tweaks settings and screen

1. New `Engine/Source/Ui/TweaksSettings.h/.cpp` (whole-file `BT_CLIENT`, client project only), shaped like `GameSettings.h/.cpp`: `engine::SaveTweaksSettings()` and `engine::LoadTweaksSettings()`. The `TweaksSettings` struct, path, static asserts, `kbDebugInput` early returns, and load validation are copied unchanged from `ClientSettings.cpp:13-60`. The bytes on disk do not change, so `kiVersion` stays 14 and saved files keep loading.
2. Callers switch to the engine functions: `Main.cpp:301,526`, `Graphics.cpp:442,713` (and the `Graphics.cpp:709` comment that names `SaveTweaksSettings`). `ClientSettings.h/.cpp` keep only `ClientState`; remove the includes only the Tweaks code used.
3. Delete the `game::TweaksScreen` class (`TweaksScreen.h:10-15`, `TweaksScreen.cpp:10-21`). Add its gate at the top of the `if constexpr (kbDebugInput)` block in `TweaksScreenBase::Render`: `if (!game::gpGame->mbShowImGui) return;`, with the same expression `ImGuiManager.cpp:484` uses. `ImGuiManager` holds and constructs `std::unique_ptr<TweaksScreenBase>`: replace the `game::TweaksScreen` forward declaration with `engine::TweaksScreenBase`, and include `TweaksScreenBase.h` in place of the game header in `ImGuiManager.cpp`. Remove `virtual ~TweaksScreenBase()`, which no longer has a derived class. The game `TweaksScreen.h` keeps `kTweakExtensionHooks`, `giTweakSectionParticles`, the extension render declarations, and `RegisterGameTweakSections`.
4. Move `TweaksScreenHexShield.cpp` to `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenHexShield.cpp` in `namespace engine`, with the registrar keys unchanged. `giTweakSectionHexShield` and `RenderHexShieldSection` move to `TweaksScreenBase.h` beside the other engine sections. Register it last in `RegisterEngineTweakSections`, with `displayName` `"Hex Shield"` and `stableKey` `"HexShield"`. `RegisterGameTweakSections` keeps only `Particles`. The registration order stays `…Wind, HexShield, Particles`, so the layout CRC over stable keys is unchanged and saved layouts still apply.

### Stage 3: engine-owned server save/load/autosave

1. New `Engine/Source/File/ServerSaveLoad.h/.cpp` (whole-file `BT_SERVER`, server project only): `engine::ServerSaveLoad` with the six methods, `kAutosaveInterval`, and `mAutosaveTimer` moved unchanged from `GameSaveLoad.h:17-30` and `.cpp:51-153`. The header names only engine and Common types. That keeps `GameBase.h` free of a new game header, which `GridSave.h` could not do because it includes `ServerFleetSerialization.h`. The `.cpp` includes the game headers it calls, the way `GridSave.cpp` and `Replay.cpp` do. Reword the `ServerReset` comment at `GameSaveLoad.cpp:91-92` so it names the game save state, not fleets.
2. Ownership: `GameBase` gains `ServerSaveLoad mServerSaveLoad;` in its `BT_SERVER` block, with `File/ServerSaveLoad.h` included under `BT_SERVER` in `GameBase.h`. Recommended over keeping the member on `game::Game` because the engine then drives autosave through its own member instead of a game member name every game must copy.
3. Callers: `GameBase.cpp:474` calls `mServerSaveLoad.TickAutosave()`, `Main.cpp:519` calls `pGame->mServerSaveLoad.Autosave()`, `Game.cpp:43` calls `mServerSaveLoad.Autoload()`, and the game `AgentCommandsServer.cpp` and `ServerSession.cpp` sites call `gpGame->mServerSaveLoad`. Remove `Game::mGameSaveLoad` and its include.
4. `Save/GameSaveLoad.h/.cpp` keep only the game replay hooks and `OnStateReplaced` (`GameSaveLoad.h:11-15`, `.cpp:20-49`). The file name stays, so `Replay.h:5` and `ServerFrameEdit.cpp:13` still compile.

### Risk tier

Tier 3 (`.agents/references/risk-tiers.md`). Trigger: the change spans independently owned subsystems: engine File, GameBase, Graphics/Managers, Ui and TweaksScreen, and Network simulation, plus game Save, Ui, Network/Server, and Agent. It also changes the force-included PCH of both targets. Serialization exposure: the grid save and `TweaksSettings.bin` are written and read by moved code, but their bytes, versions, and validation do not change. `ReadGridSave` and `GridSave.cpp` are untouched, so the server's grid-save validation is the same. If the implementation does change either layout, bump `game::Frame::kiVersion` or `TweaksSettings::kiVersion`. No simulation, CRC, wire, replay, or threading behaviour changes; every switch value is the same.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Pch.h`; new `Engine/Source/EngineSwitches.h`; `Engine/Source/Network/NetworkSimulation.h`
- `Projects/BrokenEngineSandbox/Source/ClientSettings.h/.cpp`; new `Engine/Source/Ui/TweaksSettings.h/.cpp`; `Engine/Source/Main.cpp`; `Engine/Source/Graphics/Graphics.cpp`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.h/.cpp`; `Engine/Source/Graphics/Managers/ImGuiManager.h/.cpp`; `Projects/BrokenEngineSandbox/Source/Ui/Screens/TweaksScreen/TweaksScreen.h/.cpp`; `TweaksScreenHexShield.cpp` (game to engine)
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.h/.cpp`; new `Engine/Source/File/ServerSaveLoad.h/.cpp`; `Engine/Source/GameBase.h/.cpp`; `Projects/BrokenEngineSandbox/Source/Game.h/.cpp`; `Agent/AgentCommandsServer.cpp`; `Network/Server/ServerSession.cpp`
- Both `.vcxproj`/`.vcxproj.filters` (or the shared engine project items, if they exist by then), through `/update-vcxproj`

## In scope

- `Pch.h:5-95,106`: move every switch except `kbInvincibility` and `kbAutoConnect` into the new `EngineSwitches.h`; move `keNetworkSimulation` into `NetworkSimulation.h`; add the `EngineSwitches.h` include to `Pch.h`.
- `ClientSettings.cpp:13-60` and `ClientSettings.h:8-9`: move the Tweaks settings into the new `Engine/Source/Ui/TweaksSettings.h/.cpp`; update the callers at `Main.cpp:301,526` and `Graphics.cpp:442,709-713`.
- `game::TweaksScreen` class removal; the `mbShowImGui` gate in `TweaksScreenBase::Render`; removal of `TweaksScreenBase`'s virtual destructor; `ImGuiManager.h:8,63` and `ImGuiManager.cpp:17,166`.
- The Hex Shield section: file move, `giTweakSectionHexShield`/`RenderHexShieldSection` declarations, its `RegisterEngineTweakSections` entry, and its removal from `RegisterGameTweakSections`.
- `GameSaveLoad` class move to `engine::ServerSaveLoad`, with the `ServerReset` comment reworded; `GameBase::mServerSaveLoad`; the caller updates listed in Stage 3; `Game::mGameSaveLoad` removal.
- Project membership for every added, moved, and removed file.
- `AGENTS.md` updates found stale by `/update-claude-docs`, at least: `Engine/Source/AGENTS.md` (`EngineSwitches.h` ownership); `Engine/Source/File/AGENTS.md` (`ServerSaveLoad` and the save/load rules from game `Save/AGENTS.md` `## Save and Load`); `Projects/BrokenEngineSandbox/Source/Save/AGENTS.md`, `Source/AGENTS.md` (`GameSaveLoad`, `TweaksSettings.bin`, `Pch.h` bullets), and `Source/Profile/AGENTS.md` (the load/reset discard pointer); `Engine/Source/Ui/AGENTS.md` `## Settings Persistence`; `Engine/Source/Ui/Screens/AGENTS.md`; `Engine/Source/Ui/Screens/TweaksScreen/AGENTS.md` (disk I/O and settings-struct ownership); `Engine/Source/Graphics/Managers/AGENTS.md` (the TweaksScreen exception).

## Out of scope

- Engine CPU profile timers (`Profile/ProfileManager.h:22-48`, the raw NavQuery registration and `BootStart` in `ProfileManager.cpp:12-19`). The overlay hierarchy is positional: names are indented and listed in enum order. Moving the engine-used timers into `EngineCpuTimers` would reorder the rows and separate game sub-timers from their parents, and the fix for that has not been decided.
- The Particles section and `Ui/ParticleWrappers.*`: their content is game-named instances, and a shared multiplier struct plus subtab helper needs undecided choices about static key storage and the 14 `ExplosionType::p*Scale` pointers.
- `ClientState.bin` camera zoom persistence and `ReplayMeta::clientGridCoordinate`: they stay game-owned.
- The `Pch.h:97-104` include sequence, `DataPacker/Source/Pch.h`, and `Common/` sources.
- Grid save and replay format, `GridSave.cpp`, `Replay.*`, `ServerFleetSerialization.*`, the replay metadata hooks, and the `ServerQuicksave.save`/`ServerAutosave.save` names.
- `Game::Reset`, `CreateNewFrame`, and `ComputeActiveSet` themselves; the debug-control packet handling in `ServerSession.cpp` beyond the member rename.
- The `TweaksScreenBase` class name and the `Save/GameSaveLoad.*` file names; the game extension hooks and every other game Tweaks tab or wrapper (`Lighting`, `Smoke`, `Sound`, `WindDeposits`); `HudScreen`, `Localization.h`, `ShaderLayouts.h`.
- Any version bump, migration, or compatibility path.

## Acceptance criteria

| Criterion | Evidence |
|---|---|
| Builds | `/compile` builds the `BrokenEngineSandbox` client and server in Debug, Profile, and Release x64 with no new warnings; `/update-vcxproj` validation passes |
| Switches moved unchanged | Game `Pch.h` declares only `kbInvincibility` and `kbAutoConnect`; a diff shows every other switch line copied byte for byte into `EngineSwitches.h`, and `keNetworkSimulation` into `NetworkSimulation.h` |
| Old entry points gone | `git grep -n -E "mGameSaveLoad|GameSaveLoad::|game::(Save|Load)TweaksSettings|game::TweaksScreen\b|class TweaksScreen\b"` returns nothing outside `Documents/` |
| No Sandbox gameplay in the new engine files | `git grep -n -E "Blaster|Spaceship|Missile|Player|bUseMissiles|Fleet"` over `EngineSwitches.h`, `File/ServerSaveLoad.*`, `Ui/TweaksSettings.*`, and the engine `TweaksScreenHexShield.cpp` returns nothing |
| Save/load round trip | Debug server via `/agent-harness`, Temp `--app-data-directory`: after at least 100 ticks, `pause`, `save {"file":"RoundTrip.save"}`, then record `status.tick`, `status.nextGlobalId`, and `query_collection` spaceships for one active coord. Unpause, advance, `load {"file":"RoundTrip.save","pauseAfterLoad":true}`: `resetToFresh` is `false`, and the three recorded values match. `reset` then succeeds and `status.tick` advances after unpause |
| Autosave and autoload | Same server: `quit` writes `ServerAutosave.save` in the app-data directory with no `Final autosave failed` log line. Relaunching with that directory starts at `status.tick` at or above the tick before `quit`, not a fresh game |
| Tweaks settings persist | Debug client via `/agent-harness`, Temp `--app-data-directory`: press F3, open the Hex Shield section from the toggle bar, and `describe_ui` lists its `Grow` slider. After `quit` and a relaunch with the same directory, `describe_scene` reports `tweaksVisible: true`, and `describe_ui` lists the Hex Shield sliders with no extra input |
| Layout bytes unchanged | Code reading: `TweaksSettings::kiVersion` is 14 with `sizeof == 304`, and the stable keys still register in the order `Pbr … Wind, HexShield, Particles` |

## Coordination

- `Documents/Plans/Engine/GameShellIntoGameBase.md` moves `Game::Reset`, cell creation, and the active set into `GameBase`. `ServerLoad`/`ServerReset` call all three. The two Plans can land in either order; the second updates the moved bodies in `Engine/Source/File/ServerSaveLoad.cpp` (or in `GameSaveLoad.cpp`, if that Plan lands first) to the other's current names.
- `Documents/Plans/Engine/SharedSessionProtocol.md` moves the debug-control save/load/reset packet handling that calls these entry points. Its callers use `GameBase::mServerSaveLoad` once this Plan has landed.
- `Documents/Plans/Engine/SharedEngineProjectFiles.md`: whichever lands second puts this Plan's new and moved engine files into the project layout the other left.
- `Documents/Plans/Engine/HardenServerForUnattendedHosting.md` adds `FinalAutosave()` around the `Main.cpp:519` autosave call. Whichever lands second calls `game::gpGame->mServerSaveLoad.Autosave()` there.
