# Game UI - Localization and Settings Consumption

The game-side localization facade over the engine-owned strings, plus game
wrapper storage. Engine wrapper semantics are defined by the Engine UI hub
(`../../../../Engine/Source/Ui/AGENTS.md`); the standard player-facing menus are
engine-owned (`../../../../Engine/Source/Ui/Screens/AGENTS.md`) and the HUD
lives in Screens (`Screens/AGENTS.md`).

## Localization

- The engine owns the standard localization strings and the language vocabulary
  (`../../../../Engine/Source/Ui/AGENTS.md`). `Ui/Localization.h` is a facade:
  it includes the engine header and re-exports `Language`, `LanguageOption`,
  `StandardString`, `TranslatedString`, `InitializeLocalization`, `geLanguage`,
  `kiLanguageCount`, and `kpLanguageOptions` into `game`, so game code names
  them without the `engine::` qualifier. The `Game` constructor calls that
  initializer on both builds.
- The facade owns no table. A game-owned string type, table, and
  `TranslatedString` overload are deliberately deferred until a first
  game-specific string exists; adding them belongs with that string, not before
  it.
- Initialization uppercases the table in place. Language selection is client
  runtime state, chosen on the engine-owned Game Settings screen and persisted
  by the engine with the UI font scale, opaque-UI toggle, UI opacity, and theme
  in `GameSettings.bin` (`../../../../Engine/Source/Ui/AGENTS.md`); a file whose
  stored index falls outside the Language enum is rejected whole and every
  setting it holds stays at its default.
- Engine UI owns UTF-32-to-UTF-8 conversion and its workbuffer-backed result
  handle. Screens perform conversion at the consuming expression and do not
  retain the result past its workbuffer lifetime.

## Wrapper Affinity

- Game wrapper headers are whole-file `BT_CLIENT`-guarded and
  client-project-only; only `Ui/Localization.h` stays shared with the server
  project.
- Shared Frame code must guard any wrapper include or reference under
  `#if defined(BT_CLIENT)` and never read a wrapper value on simulation paths
  (`../../../../Engine/Source/Frame/AGENTS.md`
  `## Simulation and Visual State`).
- Game wrapper storage that mirrors engine Tweaks sliders keeps its declaration
  order matching the
  `../../../../Engine/Source/Ui/Screens/TweaksScreen/AGENTS.md` slider order.
  Wrapper storage with no Tweaks counterpart keeps a stable local order of its
  own.

## Graphics Quality Consumption

- Game, sound, and graphics settings persistence and the menu screens that call
  their save and reset entry points are all engine-owned
  (`../../../../Engine/Source/Ui/AGENTS.md`); the loads run once at client
  startup in `Engine/Source/Main.cpp`.
- Game wrapper storage mirroring engine levels stays consistent with them. Water
  is read directly by per-frame rendering; the other selections invoke their
  corresponding engine apply functions.
