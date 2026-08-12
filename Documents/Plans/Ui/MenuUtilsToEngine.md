<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-11T00:00:04.000Z","dependsOn":[]} -->
# Move menu chrome, widget helpers, and the slide-panel helper into the engine

## Context

`Projects/BrokenEngineSandbox/Source/Ui/Screens/MenuUtils.{h,cpp}` (109 + 294 lines) is the shared ImGui menu toolkit, and none of it names a game concept. `ScopedMenuScale`, `ScopedMenuFont`, `AppendUtf8`, `WrapperToggle`/`WrapperSlider`/`WrapperPlusMinus`, `RadioRow`, `MenuButtonsWidth`, `MenuHeading`, `DrawFullScreenDim`, `DrawPanelAccents`, and `MenuButton` all operate on ImGui and `engine::Wrapper`. The chrome table `kMenuChromes` (`MenuUtils.cpp:152-181`) is indexed by `engine::UiTheme` and carries a `static_assert` against `engine::UiTheme::kCount`. `Documents/UserInterfaceDesign.txt` — the layout contract these helpers implement — is already engine-level documentation.

Same shape in the HUD: the hover-driven slide-out panel machinery is generic geometry over ImGui mouse position and `engine::UiScale()` — `SlidePanelState` (`Ui/Screens/HudScreen.h:22-26`), `ComputeMouseOpennessTarget` (`HudScreen.cpp:23-36`), `UpdateSlideAndGetEdgeX` (`HudScreen.cpp:38-62`), and their two tuning constants `kfActivationDistancePixels` and `kfSlideRate` (`HudScreen.cpp:14-15`). They are private statics of `HudScreen` only because that is the one screen using them today. The third constant beside them, `kfForceOpenGracePeriodSeconds` (`HudScreen.cpp:16`), belongs to the HUD's force-open logic (`HudScreen.cpp:141`), which stays game-owned.

A second game rebuilding its menus would copy all of this verbatim.

## Design

Move `MenuUtils.{h,cpp}` wholesale to `Engine/Source/Ui/MenuUtils.{h,cpp}` in namespace `engine`, preserving the existing partial `#if defined(BT_CLIENT)` spans (`MenuUtils.h:72` onward, `MenuUtils.cpp:129` onward) so the server build keeps compiling the non-render helpers exactly as today. The keep-name (non-`*Base`) spelling is deliberate: the `*WrappersBase` convention marks engine wrapper-storage pairs, and this file is a widget/chrome toolkit, not wrapper storage. The engine implementation keeps its existing `#include "Ui/GraphicsSettingsWrappersBase.h"` (for `engine::UiTheme`/`GetUiTheme`), which resolves identically from `Engine/Source`.

Three HUD layout constants stay game-owned because they describe this game's HUD panel, not engine chrome: `kfHudEdgeMarginFraction`, `kfHudPanelTopFraction`, and `kfHudPanelMaxHeightFraction` (`MenuUtils.h:25-27`). Their only consumer is `HudScreen.cpp`, so they move into its anonymous namespace, per the house rule that screen-specific dimensions stay named constants in their screen `.cpp` (`MenuUtils.h:17-18` comment). Every other constant in the layout block — including the single-screen anchors like `kfMainMenuCenterFractionX` — moves verbatim: the block is the shared-constants home `Documents/UserInterfaceDesign.txt` documents, and its values are the layout contract's defaults, like the `kMenuChromes` colours.

The slide-panel helpers move to the same engine header as free functions plus an `engine::SlidePanelState` struct, inside the `BT_CLIENT` span: `ComputeMouseOpennessTarget` and `UpdateSlideAndGetEdgeX` are already `static` and take everything they need as parameters, so dropping the `HudScreen::` qualifier is the whole change. `kfActivationDistancePixels` and `kfSlideRate` move into the engine implementation's anonymous namespace; `kfForceOpenGracePeriodSeconds` stays in `HudScreen.cpp`'s. `HudScreen` keeps its `mFleetSlide`/`mFocusedPlayerSlide` members, retyped to the engine struct, and its `PanelWidth()` and force-open state.

Nine TUs include `MenuUtils.h` today (`MenuUtils.cpp` plus the eight screens listed below); each switches to `#include "Ui/MenuUtils.h"` and requalifies calls and constants to `engine::` (Wrapper-taking helpers would also resolve via ADL, but explicit qualification matches house style). The engine TweaksScreen's `WrapperSlider(label, iSection)` member (`TweaksScreenBase.h:95`) is class-scoped and unaffected by the new `engine::WrapperSlider` free function; no other `engine`/`common` symbol collides with a moved name.

## Critical files

- `Engine/Source/Ui/MenuUtils.h`, `Engine/Source/Ui/MenuUtils.cpp` — new home
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/MenuUtils.h`, `MenuUtils.cpp` — deleted
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.h`, `HudScreen.cpp` — helper removal, member retype, relocated HUD fractions and `kfForceOpenGracePeriodSeconds`
- The eight including screens — `DeathMenuScreen.cpp`, `GameSettingsScreen.cpp`, `GraphicsMenuScreen.cpp`, `HudScreen.cpp`, `MainMenuScreen.cpp`, `ModalScreen.cpp`, `PauseMenuScreen.cpp`, `SoundMenuScreen.cpp` — include path and qualification
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/` `BrokenEngineSandbox.vcxproj`, `BrokenEngineSandboxServer.vcxproj`, and both `.filters` — `MenuUtils` is a shared file in both projects; its entries repath to `Engine\Source\Ui\` and refilter from `Game\Ui\Screens` to `Engine\Ui` (there is no separate engine project; both game vcxprojs compile `Engine/Source/`)
- `Documents/UserInterfaceDesign.txt` — cites `Projects/BrokenEngineSandbox/Source/Ui/Screens/MenuUtils.{h,cpp}` paths and line numbers throughout (lines 10, 30, 72-74, 101, 115, 184, 205, 214-216, 232-234) and the moved `HudScreen.cpp` helpers (lines 102, 183, 319); those references follow the move

## In scope

- Moving all of `MenuUtils.h` and `MenuUtils.cpp` to `engine`, except the three HUD fractions named in Design, which relocate into `HudScreen.cpp`'s anonymous namespace
- Moving `SlidePanelState`, `ComputeMouseOpennessTarget`, `UpdateSlideAndGetEdgeX`, `kfActivationDistancePixels`, and `kfSlideRate` out of `HudScreen`
- Updating include paths, namespace qualification, and project/filter membership in both vcxprojs for the moved files
- Updating `Documents/UserInterfaceDesign.txt` path/line references to the moved files
- Any `Ui/Screens/AGENTS.md` or `Engine/Source/Ui/AGENTS.md` sentence that names the old owner

## Out of scope

- Moving any screen class (`HudScreen`, `MainMenuScreen`, `GraphicsMenuScreen`, `GameSettingsScreen`, `ModalScreen`, the Tweaks screens) — separate work
- Any change to a chrome colour, scale factor, pixel constant, or layout formula
- Renaming any control or header label — those are the harness automation API per `Ui/Screens/AGENTS.md`
- Moving the graphics quality wrappers — owned by `Documents/Plans/Ui/GraphicsQualityWrappersToEngine.md`
- Moving `UiState` — owned by `Documents/Plans/Engine/UiStateHoistToGameBase.md`
- Retiring any helper for RmlUi. `Documents/Features/Engine/RmlUiPlayerFacingUi.md` may later replace some of these for player-facing screens; that does not change where they belong today, and this plan neither anticipates nor blocks it.

## Risk tier and invariants

Tier 2 — client UI presentation only, outside the CRC and the wire. Invariants: the `static_assert(std::size(kMenuChromes) == static_cast<size_t>(engine::UiTheme::kCount))` at `MenuUtils.cpp:182` survives the move verbatim (both operands are already `engine::` types, so it compiles unchanged in the new namespace); `AppendUtf8`'s workbuffer handle keeps its move-only, expression-scoped lifetime contract; `ScopedMenuFont` push/pop stays balanced within the owning ImGui window (`Ui/Screens/AGENTS.md`); the existing `BT_CLIENT` span boundaries are preserved so the server build, which compiles `MenuUtils.cpp` today, still does.

## Acceptance criteria

- Client and server compile; the server build still links the shared menu utilities it uses today, and the `kMenuChromes` size `static_assert` is present in the moved `Engine/Source/Ui/MenuUtils.cpp`.
- Screenshots of the main menu, pause menu, graphics settings, and a modal are visually unchanged, including panel accents and full-screen dim.
- Moving the mouse toward and away from a HUD side panel slides it open and closed exactly as before, and the panel is fully offscreen at rest.
- A theme switch across all three `engine::UiTheme` values still recolours chrome.

## Scores

Effort 2 / Impact 4 / Risk 1
