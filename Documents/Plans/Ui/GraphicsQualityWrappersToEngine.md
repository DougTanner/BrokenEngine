<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-11T00:00:05.000Z","dependsOn":[]} -->
# Move the graphics quality-level wrappers into the engine

## Context

`Projects/BrokenEngineSandbox/Source/Ui/GraphicsQualityWrappers.h` (40 lines) and `GraphicsQualityWrappers.cpp` (114 lines) hold the player-facing Low/Medium/High quality levels and the code that expands each level into engine rendering wrappers. Nothing in either file names a game concept: the declarations are `engine::Wrapper`, the header includes only `Ui/WrapperBase.h`, the implementation includes only engine headers (`Ui/GraphicsSettingsWrappersBase.h`, `Ui/LightingWrappersBase.h`, `Ui/ShadowWrappersBase.h`, `Ui/WaterWrappersBase.h`), and the level tables (`GraphicsQualityWrappers.cpp:25-60` — terrain-shadow and object-shadow render multipliers, `kLightingLevels`, `kfSmokeSimulationPixels`, `kWaterLevels`) tune engine renderer features exclusively.

`Engine/Source/Ui/` already hosts the sibling this file was modelled on, `GraphicsSettingsWrappersBase.{h,cpp}`, alongside eleven other `*WrappersBase` pairs. A second game would need the same quality levels for the same renderer.

## Design

Move both files wholesale to `Engine/Source/Ui/GraphicsQualityWrappersBase.{h,cpp}` in namespace `engine`, matching the neighbouring naming convention. The whole-file `#if defined(BT_CLIENT)` guard, the `GraphicsQualityLevel` enum, the five level wrappers, the five `Apply*Level` functions, `ApplyAllGraphicsQualityLevels`, the tables, and `LevelIndex` all move unchanged; the implementation's self-include renames to the `*Base` header, and its four engine includes resolve identically from `Engine/Source`.

The two consuming TUs update their include from `"Ui/GraphicsQualityWrappers.h"` (`ClientSettings.cpp:6`, `GraphicsMenuScreen.cpp:7`) to `"Ui/GraphicsQualityWrappersBase.h"` and requalify the five wrappers, the five `Apply*Level` calls, and `ApplyAllGraphicsQualityLevels` from unqualified-`game` to `engine::`. No other TU references these symbols, and no aggregation header changes: wrapper headers are deliberately consumed directly, never through `Engine.h` (`Engine/Source/Ui/AGENTS.md`).

## Critical files

- `Engine/Source/Ui/GraphicsQualityWrappersBase.h`, `GraphicsQualityWrappersBase.cpp` — new home
- `Projects/BrokenEngineSandbox/Source/Ui/GraphicsQualityWrappers.h`, `.cpp` — deleted
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/GraphicsMenuScreen.cpp` and `ClientSettings.cpp` — include path and requalification
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj` and `.filters` — the files are whole-file `BT_CLIENT` and belong to the client project only; entries repath to `Engine\Source\Ui\` and refilter from `Game\Ui` to `Engine\Ui`; the server project stays untouched (there is no separate engine project; the game vcxprojs compile `Engine/Source/`)

## In scope

- Moving both files verbatim into `engine` under the `*Base` names, including the enum, wrappers, tables, `LevelIndex`, and every comment
- Updating the two consumers' includes, requalifying every reference, and updating client project/filter membership (`/update-vcxproj`)
- Any `Projects/BrokenEngineSandbox/Source/Ui/AGENTS.md` or `Engine/Source/Ui/AGENTS.md` sentence that names the old owner — in particular the "Graphics Quality Levels" section, which describes contracts that now belong to the engine hub

## Out of scope

- Any change to a level's default, allowed set, or table value
- Any change to when a level applies its wrappers, or to the rule that `Graphics::Refresh` is the single poller and the apply path never calls `Changed<T>()`
- The persisted settings layout or version in `ClientSettings.cpp`
- Moving the hex shield wrappers — owned by `Documents/Plans/Ui/HexShieldWrappersToEngine.md`
- Moving the menu helpers that render these controls — owned by `Documents/Plans/Ui/MenuUtilsToEngine.md`

## Risk tier and invariants

Tier 2 — client rendering settings only, outside the CRC and the wire. Invariants documented in `Projects/BrokenEngineSandbox/Source/Ui/AGENTS.md` and preserved verbatim: a level is the persisted source of truth and its engine wrappers are derived, so the derived values are never written to the settings file; a level applies on change and once at startup for both a successful and a failed settings load; a level read back from the file is opaque input and is clamped before it indexes a table (`LevelIndex`); the `kWaterLevels` counts stay inside the `{15, 31, 63, 127, 255}` allowed set.

## Acceptance criteria

- Client compiles and links; the server build is unaffected (whole file stays `BT_CLIENT`-guarded).
- Selecting Low, Medium, and High on the Graphics screen changes the same underlying engine wrappers as before, verified from the Tweaks readouts or a harness wrapper query.
- A fresh run with no settings file renders at the default level, and a saved file restores the same level after restart.

## Scores

Effort 1 / Impact 3 / Risk 1
