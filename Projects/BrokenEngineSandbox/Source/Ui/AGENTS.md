# Game UI - Localization and Settings Consumption

The game-side localization facade over the engine-owned strings, plus game wrapper storage. Engine wrapper semantics are defined by the Engine UI hub (`../../../../Engine/Source/Ui/AGENTS.md`); the standard player-facing menus are engine-owned (`../../../../Engine/Source/Ui/Screens/AGENTS.md`) and the HUD lives in Screens (`Screens/AGENTS.md`).

## Localization

- The engine owns the standard localization strings and the language vocabulary (`../../../../Engine/Source/Ui/AGENTS.md`). `Ui/Localization.h` is a facade: it includes the engine header and re-exports `Language`, `LanguageOption`, `StandardString`, `TranslatedString`, `InitializeLocalization`, `geLanguage`, `kLanguageCount`, and `kLanguageOptions` into `game`, so game code keeps calling `TranslatedString(kString...)` unqualified. The `Game` constructor calls that initializer on both builds.
- The facade owns no table. A game-owned string type, table, and `TranslatedString` overload are deliberately deferred until a first game-specific string exists; adding them belongs with that string, not before it.
- Initialization uppercases the table in place. Language selection is client runtime state, chosen on the engine-owned Game Settings screen and persisted by the engine with the UI font scale, opaque-UI toggle, UI opacity, and theme in `GameSettings.bin` (`../../../../Engine/Source/Ui/AGENTS.md`); a file whose stored index falls outside the Language enum falls back to English.
- Engine UI owns UTF-32-to-UTF-8 conversion and its workbuffer-backed result handle. Screens perform conversion at the consuming expression and do not retain the result past its workbuffer lifetime.

## Wrapper Affinity

- Wrappers read by shared Frame code compile into both client and server projects even when only the client UI changes them.
- Rendering-only settings and their consumers remain whole-file `BT_CLIENT`-guarded and client-project-only.
- A new Frame dependency on a game wrapper must either stay on a client-only path or make that wrapper available to the server build.
- Game wrapper storage that mirrors engine Tweaks sliders keeps its declaration order matching the `../../../../Engine/Source/Ui/Screens/TweaksScreen/AGENTS.md` slider registration order. Wrapper storage with no Tweaks counterpart keeps a stable local order of its own.

## Graphics Quality Consumption

- Game, sound, and graphics settings persistence and the menu screens that call their save and reset entry points are all engine-owned (`../../../../Engine/Source/Ui/AGENTS.md`); the loads run once at client startup in `Engine/Source/Main.cpp`.
- Game wrapper storage mirroring engine levels stays consistent with them. Water is read directly by per-frame rendering; the other selections invoke their corresponding engine apply functions.
